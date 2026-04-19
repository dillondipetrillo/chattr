#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "logger.h"

#define ERROR "ERROR"
#define INFO "INFO"

static FILE *log_file = NULL;

int logger_init(const char *filepath)
{
    log_file = fopen(filepath, "a");
    if (log_file == NULL) {
        perror("logger_init: fopen");
        return -1;
    }
    printf("=== Server started ===\n");
    return 0;
}

void logger_close(void)
{
    if (log_file != NULL) {
        printf("=== Server stopped ===\n");
        fclose(log_file);
        log_file = NULL;
    }
}

static void log_write(const char *level, const char *fmt, va_list args)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    FILE *out = strcmp(level, ERROR) == 0 ? stderr : stdout;
    va_list args_copy;
    va_copy(args_copy, args);

    printf("[%s] %-5s ", timestamp, level);
    vfprintf(out, fmt, args);
    printf("\n");

    if (log_file != NULL) {
        fprintf(log_file, "[%s] %-5s ", timestamp, level);
        vfprintf(log_file, fmt, args_copy);
        fprintf(log_file, "\n");
        fflush(log_file);
    }

    va_end(args_copy);
}

void log_info(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(INFO, fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(ERROR, fmt, args);
    va_end(args);
}