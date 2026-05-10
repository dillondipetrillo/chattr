#ifndef AUTH_HOOK
#define AUTH_HOOK

#include <stdint.h>
#include <stdio.h>

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

typedef struct auth_result (*auth_hook_fn)(struct auth_request request);

// Default implementation - accepts any token, assigns user_id = conn_fd
// Use this during development and testing only
struct auth_result default_auth_hook(struct auth_request request);

#endif