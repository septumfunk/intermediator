#include "hashtable.h"
#include "stringext.h"
#include "crypto.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

pair_t *pair_create(void *key, uint32_t key_size, void *value, uint32_t value_size) {
    pair_t *p = calloc(1, sizeof(pair_t));

    p->key = calloc(1, key_size);
    memcpy(p->key, key, key_size);
    p->value = calloc(1, value_size);
    memcpy(p->value, value, value_size);
    p->size = value_size;

    return p;
}

void pair_delete(pair_t *this) {
    if (this->next) this->next->previous = this->previous;
    if (this->previous) this->previous->next = this->next;
    free(this->key);
    free(this->value);
    free(this);
}

pair_t *pair_push(pair_t *list, pair_t *pair) {
    if (list == nullptr)
        return pair;
    while (true) {
        if (list->previous == nullptr) {
            list->previous = pair;
            pair->next = list;
            break;
        }
        list = list->previous;
    }
    return pair;
}

hashtable_t hashtable_string(void) {
    return (hashtable_t) {
        .key_size = HASHTABLE_STRING,
        .bucket_count = HASHTABLE_DEFAULT_SIZE,
        .pair_count = 0,
        .buckets = calloc(HASHTABLE_DEFAULT_SIZE, sizeof(bucket_t)),
        .mutex = mutex_new(),
    };
}

hashtable_t hashtable_arbitrary(uint32_t size) {
    return (hashtable_t) {
        .key_size = size,
        .bucket_count = HASHTABLE_DEFAULT_SIZE,
        .pair_count = 0,
        .buckets = calloc(HASHTABLE_DEFAULT_SIZE, sizeof(bucket_t)),
        .mutex = mutex_new(),
    };
}

void hashtable_delete(hashtable_t *this) {
    mutex_lock(this->mutex);
    for (bucket_t *buck = this->buckets; buck < this->buckets + this->bucket_count; ++buck) {
        while (buck->pair != nullptr) {
            pair_t *next = buck->pair->next;
            pair_delete(buck->pair);
            buck->pair = next;
        }
    }
    mutex_release(this->mutex);

    mutex_delete(this);
}

void hashtable_reset(hashtable_t *this) {
    hashtable_delete(this);
    *this = this->key_size == HASHTABLE_STRING ? hashtable_string() : hashtable_arbitrary(this->key_size);
}

void *hashtable_insert(hashtable_t *this, void *key, void *value, uint32_t size) {
    if (hashtable_get(this, key))
        return nullptr;

    mutex_lock(this->mutex);
    uint32_t hash = hashtable_hash(this, key) & (this->bucket_count - 1);
    pair_t *pair = pair_create(key, this->key_size == HASHTABLE_STRING ? strlen(key) + 1 : this->key_size, value, size);
    this->buckets[hash].pair = pair_push(this->buckets[hash].pair, pair);
    this->pair_count++;
    if (hashtable_calculate_load(this, this->bucket_count) > HASHTABLE_LOAD_CAP) {
        mutex_release(this->mutex);
        hashtable_rehash(this, this->bucket_count * 2);
        mutex_lock(this->mutex);
    }
    mutex_release(this->mutex);

    return this->buckets[hash].pair->value;
}

void *hashtable_get(hashtable_t *this, void *key) {
    mutex_lock(this->mutex);
    uint32_t hash = hashtable_hash(this, key) & (this->bucket_count - 1);
    for (pair_t *pair = this->buckets[hash].pair; pair != nullptr; pair = pair->next) {
        if (this->key_size == -1 ? bstrcmp(key, pair->key) : memcmp(key, pair->key, this->key_size) == 0) {
            mutex_release(this->mutex);
            return pair->value;
        }
    }

    mutex_release(this->mutex);
    return nullptr;
}

pair_t **hashtable_pairs(hashtable_t *this, uint32_t *count) {
    mutex_lock(this->mutex);
    *count = 0;
    pair_t **pairs = nullptr;
    for (bucket_t *buck = this->buckets; buck < this->buckets + this->bucket_count; ++buck) {
        for (pair_t *pair = buck->pair; pair != nullptr; pair = pair->next) {
            pairs = realloc(pairs, (++*count) * sizeof(pair_t *));
            pairs[*count - 1] = pair;
        }
    }
    mutex_release(this->mutex);

    return pairs;
}

void hashtable_remove(hashtable_t *this, void *key) {
    mutex_lock(this->mutex);
    uint32_t hash = hashtable_hash(this, key) & (this->bucket_count - 1);
    for (pair_t *pair = this->buckets[hash].pair; pair != nullptr; pair = pair->next) {
        if (this->key_size == -1 ? bstrcmp(key, pair->key) : memcmp(key, pair->key, this->key_size)) {
            if (pair == this->buckets[hash].pair)
                this->buckets[hash].pair = pair->next;
            pair_delete(pair);
            this->pair_count--;

            if (hashtable_calculate_load(this, this->bucket_count / 2) <= HASHTABLE_LOAD_CAP && this->bucket_count > HASHTABLE_DEFAULT_SIZE) {
                mutex_release(this->mutex);
                hashtable_rehash(this, this->bucket_count * 2);
                mutex_lock(this->mutex);
            }
            break;
        }
    }
    mutex_release(this->mutex);
}

double hashtable_calculate_load(hashtable_t *this, uint64_t count) {
    return (float)this->pair_count / count;
}

void hashtable_rehash(hashtable_t *this, uint64_t count) {
    mutex_lock(this->mutex);
    bucket_t *old_buckets = this->buckets;
    this->buckets = calloc(count, sizeof(bucket_t));

    // Rehash process
    for (bucket_t *bucket = old_buckets; bucket < old_buckets + this->bucket_count; ++bucket) {
        for (pair_t *pair = bucket->pair; pair != nullptr; pair = pair->next) {
            uint32_t hash = hashtable_hash(this, pair->key) & (count - 1);
            this->buckets[hash].pair = pair_push(this->buckets[hash].pair, pair);
        }
    }

    // Update count and clean
    this->bucket_count = count;
    free(old_buckets);
    mutex_release(this->mutex);
}

uint32_t hashtable_hash(hashtable_t *this, void *key) {
    return this->key_size == HASHTABLE_STRING ? jhash_str(key) : jhash(key, this->key_size);
}
