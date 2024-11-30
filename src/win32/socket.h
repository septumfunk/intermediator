#pragma once
#include "win32.h"

/// Converts a sockaddr_in into a string representation and returns it.
char *address_string(struct sockaddr_in address);