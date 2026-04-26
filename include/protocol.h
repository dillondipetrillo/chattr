#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_CLIENTS 1024
#define MAX_NAME 32
#define MAX_PAYLOAD 1024
#define MAX_SCOPES 16
#define PORT 8080

#include <stdint.h>
#include <time.h>

enum packet_type {
    TYPE_SYS_IDENTIFY = 1,
    TYPE_SYS_JOIN = 2,
    TYPE_SYS_PING = 3,
    TYPE_SYS_ACK = 4,
    TYPE_SYS_ERROR = 5,
    TYPE_SYS_LEAVE = 6,
    TYPE_APP_REALTIME = 10,
    TYPE_APP_STANDARD = 11,
    TYPE_APP_BACKGROUND = 12
};

enum status_code {
    STATUS_OK = 100,
    STATUS_ERR_UNIDENTIFIED = 401,
    STATUS_ERR_ALREADY_ID = 402,
    STATUS_ERR_ROOM_FULL = 403,
    STATUS_ERR_NOT_IN_ROOM = 404,
    STATUS_ERR_ALREADY_IN_ROOM = 405,
    STATUS_ERR_SCOPES_FULL = 406,
    STATUS_ERR_EXPIRED = 410,
    STATUS_ERR_PAYLOAD_SIZE = 413
};

struct client_info {
    int socket_fd;
    uint32_t client_id;
    uint32_t scopes[MAX_SCOPES];
    int scope_count;
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

struct __attribute__((packed)) response_payload {
    uint32_t status_code;
};

#endif