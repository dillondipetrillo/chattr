#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_NAME 32
#define MAX_PAYLOAD 1024

#include <stdint.h>
#include <time.h>

enum packet_type {
    TYPE_SYS_IDENTIFY = 1,
    TYPE_SYS_JOIN = 2,
    TYPE_SYS_PING = 3,
    TYPE_APP_REALTIME = 10,
    TYPE_APP_STANDARD = 11,
    TYPE_APP_BACKGROUND = 12
};

struct client_info {
    int socket_fd;
    uint32_t client_id;
    uint32_t scope_id;
    int is_identified;
    char username[MAX_NAME];
};

struct __attribute__((packed)) packet_header {
    uint8_t type;
    uint32_t payload_len;
    uint32_t scope_id;
    uint32_t sender_id;
    uint64_t expires_at;
};

#endif