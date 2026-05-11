#ifndef AUTH_HOOK
#define AUTH_HOOK

#include <stdint.h>
#include <stdio.h>

struct __attribute__((packed)) auth_request_msg {
    uint16_t token_len;
    char token[254];
};

struct __attribute__((packed)) auth_response_msg {
    uint8_t valid;
    uint32_t user_id;
    uint8_t padding[3];
};

// The engine fills this and passes it to the hook
struct auth_request {
    const char *token;  // the session token bytes from client
    size_t token_len;   // how many bytes / length of token
    int conn_fd;  // which connection is authenticating
};

//The hook fills this and returns it to the engine
struct auth_result {
    int valid;          // 1= authenticated, 0 = rejected
    uint32_t user_id;   // opaque user id, engine stores but doesn't read
};

// Function pointer type, engine calls this, never knowing the implementation
typedef struct auth_result (*auth_hook_fn)(struct auth_request request);

// Default implementation - accepts any token, assigns user_id = conn_fd
// Use this during development and testing only
// Development hook
struct auth_result default_auth_hook(struct auth_request request);

// Production hook, validates via Unix socket at ENGINE_AUTH_SOCKET
// Falls back to default_auth_hook if ENGINE_AUTH_SOCKET is not set
struct auth_result unix_socket_auth_hook(struct auth_request request);

#endif