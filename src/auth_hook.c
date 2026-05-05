// TODO: Replace this in production with real validation
// Default implementation - accepts everything
#include "auth_hook.h"

struct auth_result default_auth_hook(struct auth_request request)
{
    (void)request;
    struct auth_result result;
    result.valid = 1;
    result.user_id = 1;
    return result;
}