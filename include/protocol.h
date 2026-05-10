#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <time.h>

#define MAX_BUFF_SIZE 4096
#define MAX_CLIENTS 1024
#define MAX_NAME 32
#define MAX_PAYLOAD 1024
#define MAX_SCOPES 16

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
    STATUS_ERR_AUTH_FAILED = 400,
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
    int is_authenticated;
    int scope_count;
    int socket_fd;
    uint32_t client_id;
    uint32_t scopes[MAX_SCOPES];
    uint32_t user_id;
    char recv_bug[MAX_BUFF_SIZE];
    char *send_buf;
    char session_token[256];
    size_t recv_len;
    size_t send_len;
    size_t send_offset;
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