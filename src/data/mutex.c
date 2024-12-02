#include "mutex.h"
#include <stdbool.h>

mutex_t mutex_new(void) {
    return CreateMutex(nullptr, false, nullptr);
}

void mutex_lock(mutex_t self) {
    WaitForSingleObject(self, INFINITE);
}

void mutex_release(mutex_t self) {
    ReleaseMutex(self);
}

void mutex_delete(mutex_t self) {
    CloseHandle(self);
}