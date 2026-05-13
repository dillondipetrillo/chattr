#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>
#include "auth_hook.h"
#include "logger.h"

struct auth_result unix_socket_auth_hook(struct auth_request request)
{
    struct auth_result result = {0, 0};
    static char auth_socket_path[256] = {0};
    static int initialized = 0;
    static int warned = 0;

    if (!initialized) {
        const char *path = getenv("ENGINE_AUTH_SOCKET");
        if (path) strncpy(auth_socket_path, path, 255);
        initialized = 1;
    }

    if (auth_socket_path[0] == '\0') {
        if (!warned) {
            log_info("[AUTH] ENGINE_AUTH_SOCKET not set. Falling back to "
                "default (accept all).");
            warned = 1;
        }
        return default_auth_hook(request);
    }

    int auth_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (auth_socket_fd == -1) {
        int saved_errno = errno;
        log_error("[AUTH] unix socket() failed: %s", strerror(saved_errno));
        return result;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000; // 500ms

    if (setsockopt(auth_socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const void *)&tv,
        sizeof(tv)) < 0)
    {
        int saved_errno = errno;
        log_error("[AUTH] Warning: Could not set RCVTIMEO: %s. "
            "Engine may hang on slow auth.", strerror(saved_errno));
        close(auth_socket_fd);
        return result;
    }
    if (setsockopt(auth_socket_fd, SOL_SOCKET, SO_SNDTIMEO, (const void *)&tv,
        sizeof(tv)) < 0)
    {
        int saved_errno = errno;
        log_error("[AUTH] Warning: Could not set SNDTIMEO: %s. "
            "Engine may hang on slow auth.", strerror(saved_errno));
        close(auth_socket_fd);
        return result;
    }

    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, auth_socket_path,
        sizeof(address.sun_path) - 1);

    if (connect(auth_socket_fd, (struct sockaddr*)&address,
        sizeof(address)) == -1)
    {
        int saved_errno = errno;
        log_error("[AUTH] Connection to %s failed: %s", auth_socket_path,
            strerror(saved_errno));
        close(auth_socket_fd);
        return result;
    }

    struct auth_request_msg msg;
    memset(&msg, 0, sizeof(msg));

    size_t copy_len = (request.token_len > 254) ? 254 : request.token_len;
    msg.token_len = htons((uint16_t)copy_len);
    memcpy(msg.token, request.token, copy_len);

    if (send(auth_socket_fd, &msg, sizeof(msg), 0) != sizeof(msg)) {
        int saved_errno = errno;
        log_error("[AUTH] Failed to send full auth request: %s",
            strerror(saved_errno));
        close(auth_socket_fd);
        return result;
    }

    struct auth_response_msg resp;
    if (recv(auth_socket_fd, &resp, sizeof(resp), MSG_WAITALL) != sizeof(resp))
    {
        int saved_errno = errno;
        log_error("[AUTH] Failed to receive full auth response" 
            "(timeout or closed): %s", strerror(saved_errno));
        close(auth_socket_fd);
        return result;
    }

    close(auth_socket_fd);
    result.valid = (int)resp.valid;
    result.user_id = ntohl(resp.user_id);

    return result;
}