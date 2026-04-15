#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>

ssize_t handle_recv(const int c, char *buffer, const size_t size);
ssize_t handle_send(const int c, const char *bytes, const size_t size);

#endif