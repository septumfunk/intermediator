#include "net/server.h"
#include <stdlib.h>
#include <time.h>

int main(void) {
    srand(time(nullptr));
    server_start();
}