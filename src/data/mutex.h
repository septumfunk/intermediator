#pragma once
#include "../util/win32.h"

typedef HANDLE mutex_t;

mutex_t mutex_new(void);
void mutex_lock(mutex_t self);
void mutex_release(mutex_t self);
void mutex_delete(mutex_t self);