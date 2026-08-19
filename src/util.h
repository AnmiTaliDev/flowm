/* util.h - miscellaneous helper routines for flowm
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_UTIL_H
#define FLOWM_UTIL_H

#include <stddef.h>
#include <stdbool.h>

#define FLOWM_TITLE_MAX 256

#define FLOWM_LINE_MAX 1024

#define LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define UNUSED(x) ((void)(x))

void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

size_t xstrlcpy(char *dst, const char *src, size_t dstsize);

char *str_trim(char *s);

bool str_ieq(const char *a, const char *b);

bool str_has_prefix(const char *s, const char *prefix);

bool parse_long(const char *s, long lo, long hi, long *out);

bool parse_hex_color(const char *s, unsigned long *out);

void spawn_shell(const char *cmdline);

unsigned long now_ms(void);

#endif
