/* log.c - implementation of the flowm logger
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "util.h"

static FILE     *log_file    = NULL;
static LogLevel  min_level   = LOG_INFO;
static bool      mirror_err  = true;

static const char *level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

bool
log_init(const char *path, LogLevel level, bool also_stderr)
{
    min_level  = level;
    mirror_err = also_stderr;

    if (path == NULL || *path == '\0')
        return true;

    log_file = fopen(path, "a");
    if (log_file == NULL) {
        fprintf(stderr, "flowm: cannot open log file '%s', "
                        "logging to stderr only\n", path);
        return false;
    }

    setvbuf(log_file, NULL, _IOLBF, 0);
    return true;
}

void
log_shutdown(void)
{
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

void
log_set_level(LogLevel level)
{
    min_level = level;
}

LogLevel
log_get_level(void)
{
    return min_level;
}

bool
log_level_from_name(const char *name, LogLevel *out)
{
    size_t i;

    if (name == NULL || out == NULL)
        return false;

    for (i = 0; i < LENGTH(level_names); i++) {
        if (str_ieq(name, level_names[i])) {
            *out = (LogLevel)i;
            return true;
        }
    }
    return false;
}

static void
format_timestamp(char *buf, size_t bufsize)
{
    struct timespec ts;
    struct tm tm;
    size_t len;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm);
    len = strftime(buf, bufsize, "%Y-%m-%d %H:%M:%S", &tm);
    if (len > 0 && len + 5 < bufsize)
        snprintf(buf + len, bufsize - len, ".%03ld",
                 ts.tv_nsec / 1000000L);
}

static void
vemit(FILE *out, LogLevel level, const char *file, int line,
      const char *fmt, va_list ap)
{
    char stamp[64];

    format_timestamp(stamp, sizeof(stamp));
    fprintf(out, "%s [%-5s] %s:%d: ", stamp, level_names[level],
            file, line);
    vfprintf(out, fmt, ap);
    fputc('\n', out);
}

void
log_emit(LogLevel level, const char *file, int line, const char *fmt, ...)
{
    va_list ap;

    if (level < min_level)
        return;

    {
        const char *slash = strrchr(file, '/');
        if (slash != NULL)
            file = slash + 1;
    }

    if (log_file != NULL) {
        va_start(ap, fmt);
        vemit(log_file, level, file, line, fmt, ap);
        va_end(ap);
    }
    if (mirror_err || log_file == NULL) {
        va_start(ap, fmt);
        vemit(stderr, level, file, line, fmt, ap);
        va_end(ap);
    }
}

void
log_fatal(const char *fmt, ...)
{
    va_list ap;

    if (log_file != NULL) {
        va_start(ap, fmt);
        vemit(log_file, LOG_FATAL, "flowm", 0, fmt, ap);
        va_end(ap);
    }
    va_start(ap, fmt);
    vemit(stderr, LOG_FATAL, "flowm", 0, fmt, ap);
    va_end(ap);

    log_shutdown();
    exit(EXIT_FAILURE);
}
