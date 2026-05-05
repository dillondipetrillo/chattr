#ifndef AUTH_HOOK
#define AUTH_HOOK

#include <stdint.h>
#include <stdio.h>

// The engine fills this and passes it to the hook
struct auth_request {
    const char *token;  // the session token bytes
    size_t token_len;   // how many bytes
    int connection_id;  // which connection is authenticating
};

//The hook fills this and returns it to the engine
struct auth_result {
    int valid;          // 1= authenticated, 0 = rejected
    uint32_t user_id;   // application's user identifier
};

typedef struct auth_result (*auth_hook_fn)(struct auth_request request);

struct auth_result default_auth_hook(struct auth_request request);

#endif