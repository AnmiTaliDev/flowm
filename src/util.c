/* util.c - implementation of the helper routines declared in util.h
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include "util.h"
#include "log.h"

static void
oom_abort(size_t size)
{
    fprintf(stderr, "flowm: fatal: out of memory allocating %zu bytes\n",
            size);
    abort();
}

void *
xmalloc(size_t size)
{
    void *p;

    if (size == 0)
        size = 1;
    p = malloc(size);
    if (p == NULL)
        oom_abort(size);
    return p;
}

void *
xcalloc(size_t nmemb, size_t size)
{
    void *p;

    if (nmemb == 0 || size == 0) {
        nmemb = 1;
        size = 1;
    }
    p = calloc(nmemb, size);
    if (p == NULL)
        oom_abort(nmemb * size);
    return p;
}

void *
xrealloc(void *ptr, size_t size)
{
    void *p;

    if (size == 0)
        size = 1;
    p = realloc(ptr, size);
    if (p == NULL)
        oom_abort(size);
    return p;
}

char *
xstrdup(const char *s)
{
    size_t len;
    char *copy;

    if (s == NULL)
        s = "";
    len = strlen(s) + 1;
    copy = xmalloc(len);
    memcpy(copy, s, len);
    return copy;
}

size_t
xstrlcpy(char *dst, const char *src, size_t dstsize)
{
    size_t srclen = strlen(src);

    if (dstsize > 0) {
        size_t copylen = srclen >= dstsize ? dstsize - 1 : srclen;
        memcpy(dst, src, copylen);
        dst[copylen] = '\0';
    }
    return srclen;
}

char *
str_trim(char *s)
{
    char *end;

    while (*s != '\0' && isspace((unsigned char)*s))
        s++;
    if (*s == '\0')
        return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        *end-- = '\0';
    return s;
}

bool
str_ieq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

bool
str_has_prefix(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool
parse_long(const char *s, long lo, long hi, long *out)
{
    char *end = NULL;
    long value;

    if (s == NULL || *s == '\0')
        return false;

    errno = 0;
    value = strtol(s, &end, 10);
    if (errno != 0)
        return false;

    while (end != NULL && *end != '\0') {
        if (!isspace((unsigned char)*end))
            return false;
        end++;
    }

    if (value < lo || value > hi)
        return false;

    *out = value;
    return true;
}

static int
hex_digit(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool
parse_hex_color(const char *s, unsigned long *out)
{
    unsigned long value = 0;
    int i;

    if (s == NULL || s[0] != '#' || strlen(s) != 7)
        return false;

    for (i = 1; i <= 6; i++) {
        int d = hex_digit((unsigned char)s[i]);
        if (d < 0)
            return false;
        value = (value << 4) | (unsigned long)d;
    }

    *out = value;
    return true;
}

void
spawn_shell(const char *cmdline)
{
    pid_t pid;

    if (cmdline == NULL || *cmdline == '\0')
        return;

    pid = fork();
    if (pid < 0) {
        log_error("spawn: fork failed: %s", strerror(errno));
        return;
    }

    if (pid == 0) {
        pid_t pid2;

        if (setsid() < 0)
            _exit(127);

        pid2 = fork();
        if (pid2 < 0)
            _exit(127);
        if (pid2 > 0)
            _exit(0);

        execl("/bin/sh", "sh", "-c", cmdline, (char *)NULL);
        _exit(127);
    }

    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
        ;
    log_debug("spawn: launched '%s'", cmdline);
}

unsigned long
now_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (unsigned long)ts.tv_sec * 1000UL
         + (unsigned long)ts.tv_nsec / 1000000UL;
}
