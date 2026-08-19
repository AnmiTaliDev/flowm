/* log.h - leveled logging for flowm
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_LOG_H
#define FLOWM_LOG_H

#include <stdbool.h>

typedef enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4
} LogLevel;

bool log_init(const char *path, LogLevel level, bool also_stderr);

void log_shutdown(void);

void log_set_level(LogLevel level);

LogLevel log_get_level(void);

bool log_level_from_name(const char *name, LogLevel *out);

void log_emit(LogLevel level, const char *file, int line,
              const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

#define log_debug(...) log_emit(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_emit(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_emit(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_emit(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

void log_fatal(const char *fmt, ...)
    __attribute__((format(printf, 1, 2), noreturn));

#endif
