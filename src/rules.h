/* rules.h - per-application window rules
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_RULES_H
#define FLOWM_RULES_H

#include <stdbool.h>
#include <X11/Xlib.h>

#include "util.h"

#define FLOWM_MAX_RULES 64

typedef struct Rule {
    char match[64];

    bool set_workspace;
    int  workspace;

    bool center;
    bool maximize;
    bool fullscreen;
    bool sticky;
    bool snap_left;
    bool snap_right;

    bool set_size;
    int  width, height;

    bool set_position;
    int  x, y;
} Rule;

typedef struct RuleSet {
    Rule rules[FLOWM_MAX_RULES];
    int  count;
} RuleSet;

bool rules_parse_line(RuleSet *rs, const char *value);

const Rule *rules_match(const RuleSet *rs, Display *dpy, Window win);

bool rules_pattern_matches(const char *pattern, const char *name);

#endif
