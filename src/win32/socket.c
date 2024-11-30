#include "socket.h"
#include "../util/stringext.h"

char *address_string(struct sockaddr_in address) {
    return format("%s:%d", inet_ntoa(address.sin_addr), ntohs(address.sin_port));
}