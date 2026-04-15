#include "utils.h"

ssize_t handle_send(const int c, const char *bytes, const size_t size)
{
    size_t total_sent = 0;
    ssize_t bytes_sent;
    while (total_sent < size) {
        bytes_sent = send(c, bytes + total_sent, size - total_sent, 0);
        if (bytes_sent <= 0)
            return bytes_sent;
        total_sent += bytes_sent;
    }
    return total_sent;
}

ssize_t handle_recv(const int c, char *buffer, const size_t size)
{
    size_t total = 0;
    while (total < size) {
        ssize_t n = recv(c, buffer + total, size - total, 0);
        if (n <= 0)
            return n;
        total += n;
    }
    return total;
}