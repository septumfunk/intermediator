#include "networking/server.h"
#include <stdlib.h>
#include <time.h>

int main(void) {
    srand(time(NULL));
    server_start();
}