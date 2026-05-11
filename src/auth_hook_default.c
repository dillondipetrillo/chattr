// Replace this in production with real validation - communicates over Unix
// socket defined in ENGINE_AUTH_SOCKET.
// Default implementation - accepts everything
#include "auth_hook.h"
#include "logger.h"

struct auth_result default_auth_hook(struct auth_request request)
{
    struct auth_result result;
    result.valid = 1;
    result.user_id = request.conn_fd;

    // Log that default hook was called so it is obvious in development
    log_info("=== Using default auth hook ===");
    log_info("[DEFAULT AUTH HOOK] ACCEPTS ALL TOKENS — NOT FOR PRODUCTION");
    log_info("Auth result    | valid: %d, user_id: %u",
        result.valid, result.user_id);
    log_info("============================");
    
    return result;
}