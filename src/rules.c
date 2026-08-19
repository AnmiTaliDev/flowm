/* rules.c - per-application window rules
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "rules.h"
#include "wm.h"
#include "log.h"
#include "util.h"

bool
rules_pattern_matches(const char *pattern, const char *name)
{
    size_t plen;

    if (pattern == NULL || name == NULL)
        return false;

    plen = strlen(pattern);
    if (plen == 0)
        return false;

    if (pattern[plen - 1] == '*') {
        size_t i;

        for (i = 0; i + 1 < plen; i++) {
            if (name[i] == '\0')
                return false;
            if (tolower((unsigned char)pattern[i]) !=
                tolower((unsigned char)name[i]))
                return false;
        }
        return true;
    }

    return str_ieq(pattern, name);
}

static bool
parse_size_value(const char *s, int *w, int *h)
{
    char buf[64];
    char *x;
    long a, b;

    xstrlcpy(buf, s, sizeof(buf));
    x = strchr(buf, 'x');
    if (x == NULL)
        return false;
    *x++ = '\0';
    if (!parse_long(buf, 1, 32768, &a) || !parse_long(x, 1, 32768, &b))
        return false;
    *w = (int)a;
    *h = (int)b;
    return true;
}

static bool
parse_position_value(const char *s, int *px, int *py)
{
    char buf[64];
    char *comma;
    long a, b;

    xstrlcpy(buf, s, sizeof(buf));
    comma = strchr(buf, ':');
    if (comma == NULL)
        return false;
    *comma++ = '\0';
    if (!parse_long(buf, -32768, 32768, &a) ||
        !parse_long(comma, -32768, 32768, &b))
        return false;
    *px = (int)a;
    *py = (int)b;
    return true;
}

static bool
apply_rule_option(Rule *r, const char *opt)
{
    const char *eq = strchr(opt, '=');

    if (eq == NULL) {
        if (str_ieq(opt, "center"))     { r->center = true;     return true; }
        if (str_ieq(opt, "maximize"))   { r->maximize = true;   return true; }
        if (str_ieq(opt, "fullscreen")) { r->fullscreen = true; return true; }
        return false;
    }

    if (str_has_prefix(opt, "workspace=")) {
        long n;
        if (!parse_long(eq + 1, 1, FLOWM_MAX_WORKSPACES, &n))
            return false;
        r->set_workspace = true;
        r->workspace = (int)n - 1;
        return true;
    }
    if (str_has_prefix(opt, "size=")) {
        if (!parse_size_value(eq + 1, &r->width, &r->height))
            return false;
        r->set_size = true;
        return true;
    }
    if (str_has_prefix(opt, "position=")) {
        if (!parse_position_value(eq + 1, &r->x, &r->y))
            return false;
        r->set_position = true;
        return true;
    }

    return false;
}

bool
rules_parse_line(RuleSet *rs, const char *value)
{
    char buf[FLOWM_LINE_MAX];
    char *match, *opts, *token, *save = NULL;
    Rule r;

    if (rs == NULL || value == NULL)
        return false;
    if (rs->count >= FLOWM_MAX_RULES) {
        log_warn("rules: table full (%d), rule dropped",
                 FLOWM_MAX_RULES);
        return false;
    }

    xstrlcpy(buf, value, sizeof(buf));
    match = str_trim(buf);
    opts = strchr(match, ' ');
    if (opts == NULL)
        return false;
    *opts++ = '\0';
    opts = str_trim(opts);

    memset(&r, 0, sizeof(r));
    xstrlcpy(r.match, match, sizeof(r.match));

    for (token = strtok_r(opts, ",", &save);
         token != NULL;
         token = strtok_r(NULL, ",", &save)) {
        token = str_trim(token);
        if (*token == '\0')
            continue;
        if (!apply_rule_option(&r, token)) {
            log_warn("rules: bad option '%s' in rule '%s'",
                     token, match);
            return false;
        }
    }

    rs->rules[rs->count++] = r;
    log_debug("rules: added rule for '%s'", r.match);
    return true;
}

const Rule *
rules_match(const RuleSet *rs, Display *dpy, Window win)
{
    XClassHint hint;
    const Rule *found = NULL;
    int i;

    if (rs == NULL || rs->count == 0)
        return NULL;

    memset(&hint, 0, sizeof(hint));
    if (XGetClassHint(dpy, win, &hint) == 0)
        return NULL;

    for (i = 0; i < rs->count && found == NULL; i++) {
        const Rule *r = &rs->rules[i];

        if ((hint.res_name != NULL &&
             rules_pattern_matches(r->match, hint.res_name)) ||
            (hint.res_class != NULL &&
             rules_pattern_matches(r->match, hint.res_class)))
            found = r;
    }

    if (found != NULL)
        log_debug("rules: window 0x%lx (%s/%s) matched '%s'",
                  win,
                  hint.res_name != NULL ? hint.res_name : "?",
                  hint.res_class != NULL ? hint.res_class : "?",
                  found->match);

    if (hint.res_name != NULL)
        XFree(hint.res_name);
    if (hint.res_class != NULL)
        XFree(hint.res_class);

    return found;
}
