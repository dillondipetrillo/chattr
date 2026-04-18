#ifndef LOGGER_H
#define LOGGER_H

int logger_init(const char *filepath);
void logger_close(void);

void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif
