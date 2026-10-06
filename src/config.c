/* config.c - configuration file parser and built-in defaults
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
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include "config.h"
#include "rules.h"
#include "log.h"
#include "util.h"

#define DEFMOD Mod4Mask

static void
add_binding(Config *cfg, unsigned int mods, KeySym sym,
            ActionType action, int a1, int a2, const char *str)
{
    Binding *b;

    if (cfg->binding_count >= FLOWM_MAX_BINDINGS) {
        log_warn("config: binding table full (%d), ignoring binding",
                 FLOWM_MAX_BINDINGS);
        return;
    }
    b = &cfg->bindings[cfg->binding_count++];
    b->modifiers = mods;
    b->keysym    = sym;
    b->action    = action;
    b->arg_int   = a1;
    b->arg_int2  = a2;
    b->arg_str   = str != NULL ? xstrdup(str) : NULL;
}

void
config_defaults(Config *cfg)
{
    int i;

    memset(cfg, 0, sizeof(*cfg));

    cfg->border_width       = 2;
    cfg->titlebar_height    = 22;
    cfg->workspace_count    = 4;
    cfg->snap_distance      = 12;
    cfg->move_step          = 24;
    cfg->resize_step        = 24;
    cfg->focus_follows_mouse = false;
    cfg->show_bar           = true;

    cfg->color_border_focused    = 0x4c7899;
    cfg->color_border_unfocused  = 0x333333;
    cfg->color_border_urgent     = 0xb03030;
    cfg->color_titlebar_focused  = 0x4c7899;
    cfg->color_titlebar_unfocused = 0x2a2a2a;
    cfg->color_title_text        = 0xffffff;
    cfg->color_bar_bg            = 0x1c1c1c;
    cfg->color_bar_fg            = 0xd0d0d0;
    cfg->color_bar_ws_active     = 0x4c7899;

    xstrlcpy(cfg->font_name, "fixed", sizeof(cfg->font_name));
    cfg->log_path[0] = '\0';
    cfg->log_level   = LOG_INFO;

    for (i = 0; i < FLOWM_MAX_WORKSPACES; i++)
        snprintf(cfg->workspace_names[i],
                 sizeof(cfg->workspace_names[i]), "%d", i + 1);

    add_binding(cfg, DEFMOD,             XK_Return, ACT_EXEC, 0, 0, "xterm");
    add_binding(cfg, DEFMOD,             XK_d,      ACT_EXEC, 0, 0, "dmenu_run");
    add_binding(cfg, DEFMOD,             XK_q,      ACT_CLOSE, 0, 0, NULL);
    add_binding(cfg, DEFMOD | ShiftMask, XK_q,      ACT_KILL,  0, 0, NULL);
    add_binding(cfg, DEFMOD | ShiftMask, XK_e,      ACT_QUIT,  0, 0, NULL);

    add_binding(cfg, DEFMOD, XK_f, ACT_FULLSCREEN, 0, 0, NULL);
    add_binding(cfg, DEFMOD, XK_m, ACT_MAXIMIZE,   0, 0, NULL);
    add_binding(cfg, DEFMOD, XK_c, ACT_CENTER,     0, 0, NULL);
    add_binding(cfg, DEFMOD, XK_s, ACT_STICKY,     0, 0, NULL);

    add_binding(cfg, DEFMOD,             XK_Tab, ACT_FOCUS_NEXT, 0, 0, NULL);
    add_binding(cfg, DEFMOD | ShiftMask, XK_Tab, ACT_FOCUS_PREV, 0, 0, NULL);

    add_binding(cfg, DEFMOD, XK_r, ACT_RAISE, 0, 0, NULL);
    add_binding(cfg, DEFMOD, XK_l, ACT_LOWER, 0, 0, NULL);

    add_binding(cfg, DEFMOD, XK_Left,  ACT_MOVE, -1,  0, NULL);
    add_binding(cfg, DEFMOD, XK_Right, ACT_MOVE,  1,  0, NULL);
    add_binding(cfg, DEFMOD, XK_Up,    ACT_MOVE,  0, -1, NULL);
    add_binding(cfg, DEFMOD, XK_Down,  ACT_MOVE,  0,  1, NULL);
    add_binding(cfg, DEFMOD | ShiftMask, XK_Left,  ACT_RESIZE, -1,  0, NULL);
    add_binding(cfg, DEFMOD | ShiftMask, XK_Right, ACT_RESIZE,  1,  0, NULL);
    add_binding(cfg, DEFMOD | ControlMask, XK_Left,  ACT_SNAP_LEFT,  0, 0, NULL);
    add_binding(cfg, DEFMOD | ControlMask, XK_Right, ACT_SNAP_RIGHT, 0, 0, NULL);
    add_binding(cfg, DEFMOD | ShiftMask, XK_Up,    ACT_RESIZE,  0, -1, NULL);
    add_binding(cfg, DEFMOD | ShiftMask, XK_Down,  ACT_RESIZE,  0,  1, NULL);

    for (i = 0; i < 9; i++) {
        add_binding(cfg, DEFMOD, XK_1 + i,
                    ACT_WORKSPACE, i, 0, NULL);
        add_binding(cfg, DEFMOD | ShiftMask, XK_1 + i,
                    ACT_SEND_TO_WORKSPACE, i, 0, NULL);
    }
    add_binding(cfg, DEFMOD, XK_period, ACT_WORKSPACE_NEXT, 0, 0, NULL);
    add_binding(cfg, DEFMOD, XK_comma,  ACT_WORKSPACE_PREV, 0, 0, NULL);
}

void
config_free(Config *cfg)
{
    int i;

    for (i = 0; i < cfg->binding_count; i++) {
        free(cfg->bindings[i].arg_str);
        cfg->bindings[i].arg_str = NULL;
    }
    cfg->binding_count = 0;
    cfg->rules.count = 0;
}

typedef struct ModName {
    const char  *name;
    unsigned int mask;
} ModName;

static const ModName mod_names[] = {
    { "shift",   ShiftMask   },
    { "control", ControlMask },
    { "ctrl",    ControlMask },
    { "mod1",    Mod1Mask    },
    { "alt",     Mod1Mask    },
    { "mod2",    Mod2Mask    },
    { "mod3",    Mod3Mask    },
    { "mod4",    Mod4Mask    },
    { "super",   Mod4Mask    },
    { "win",     Mod4Mask    },
    { "mod5",    Mod5Mask    },
};

bool
config_parse_chord(const char *chord, unsigned int *mods_out,
                   unsigned long *keysym_out)
{
    char buf[128];
    char *token, *save = NULL;
    unsigned int mods = 0;
    KeySym sym = NoSymbol;

    if (chord == NULL || mods_out == NULL || keysym_out == NULL)
        return false;

    xstrlcpy(buf, chord, sizeof(buf));

    for (token = strtok_r(buf, "+", &save);
         token != NULL;
         token = strtok_r(NULL, "+", &save)) {
        bool is_mod = false;
        size_t i;

        token = str_trim(token);
        if (*token == '\0')
            continue;

        for (i = 0; i < LENGTH(mod_names); i++) {
            if (str_ieq(token, mod_names[i].name)) {
                mods |= mod_names[i].mask;
                is_mod = true;
                break;
            }
        }
        if (is_mod)
            continue;

        if (sym != NoSymbol)
            return false;

        sym = XStringToKeysym(token);
        if (sym == NoSymbol)
            return false;
    }

    if (sym == NoSymbol)
        return false;

    *mods_out   = mods;
    *keysym_out = (unsigned long)sym;
    return true;
}

bool
config_parse_action(const char *action_str, Binding *b)
{
    char buf[256];
    char *verb, *rest;

    if (action_str == NULL || b == NULL)
        return false;

    memset(b, 0, sizeof(*b));
    xstrlcpy(buf, action_str, sizeof(buf));

    verb = str_trim(buf);
    rest = strchr(verb, ' ');
    if (rest != NULL) {
        *rest++ = '\0';
        rest = str_trim(rest);
    }

    if (str_ieq(verb, "exec")) {
        if (rest == NULL || *rest == '\0')
            return false;
        b->action  = ACT_EXEC;
        b->arg_str = xstrdup(rest);
        return true;
    }
    if (str_ieq(verb, "close"))       { b->action = ACT_CLOSE;      return true; }
    if (str_ieq(verb, "kill"))        { b->action = ACT_KILL;       return true; }
    if (str_ieq(verb, "quit"))        { b->action = ACT_QUIT;       return true; }
    if (str_ieq(verb, "fullscreen"))  { b->action = ACT_FULLSCREEN; return true; }
    if (str_ieq(verb, "maximize"))    { b->action = ACT_MAXIMIZE;   return true; }
    if (str_ieq(verb, "center"))      { b->action = ACT_CENTER;     return true; }
    if (str_ieq(verb, "sticky"))      { b->action = ACT_STICKY;     return true; }
    if (str_ieq(verb, "snap_left") || str_ieq(verb, "tile_left"))   { b->action = ACT_SNAP_LEFT;  return true; }
    if (str_ieq(verb, "snap_right") || str_ieq(verb, "tile_right")) { b->action = ACT_SNAP_RIGHT; return true; }
    if (str_ieq(verb, "raise"))       { b->action = ACT_RAISE;      return true; }
    if (str_ieq(verb, "lower"))       { b->action = ACT_LOWER;      return true; }
    if (str_ieq(verb, "focus_next"))  { b->action = ACT_FOCUS_NEXT; return true; }
    if (str_ieq(verb, "focus_prev"))  { b->action = ACT_FOCUS_PREV; return true; }
    if (str_ieq(verb, "workspace_next")) { b->action = ACT_WORKSPACE_NEXT; return true; }
    if (str_ieq(verb, "workspace_prev")) { b->action = ACT_WORKSPACE_PREV; return true; }

    if (str_ieq(verb, "workspace") || str_ieq(verb, "send_to_workspace")) {
        long n;
        if (!parse_long(rest != NULL ? rest : "", 1,
                        FLOWM_MAX_WORKSPACES, &n))
            return false;
        b->action  = str_ieq(verb, "workspace")
                   ? ACT_WORKSPACE : ACT_SEND_TO_WORKSPACE;
        b->arg_int = (int)n - 1;
        return true;
    }

    if (str_ieq(verb, "move") || str_ieq(verb, "resize")) {
        char *second;
        long a, c;

        if (rest == NULL)
            return false;
        second = strchr(rest, ' ');
        if (second == NULL)
            return false;
        *second++ = '\0';
        if (!parse_long(str_trim(rest), -100, 100, &a) ||
            !parse_long(str_trim(second), -100, 100, &c))
            return false;
        b->action   = str_ieq(verb, "move") ? ACT_MOVE : ACT_RESIZE;
        b->arg_int  = (int)a;
        b->arg_int2 = (int)c;
        return true;
    }

    return false;
}

static bool
set_color_option(const char *value, unsigned long *slot)
{
    unsigned long rgb;

    if (!parse_hex_color(value, &rgb))
        return false;
    *slot = rgb;
    return true;
}

static bool
set_bool_option(const char *value, bool *slot)
{
    if (str_ieq(value, "true") || str_ieq(value, "yes") ||
        str_ieq(value, "on")   || strcmp(value, "1") == 0) {
        *slot = true;
        return true;
    }
    if (str_ieq(value, "false") || str_ieq(value, "no") ||
        str_ieq(value, "off")   || strcmp(value, "0") == 0) {
        *slot = false;
        return true;
    }
    return false;
}

static bool
set_int_option(const char *value, long lo, long hi, int *slot)
{
    long n;

    if (!parse_long(value, lo, hi, &n))
        return false;
    *slot = (int)n;
    return true;
}

bool
config_apply_option(Config *cfg, const char *key, const char *value)
{
    if (str_ieq(key, "border_width"))
        return set_int_option(value, 0, 64, &cfg->border_width);
    if (str_ieq(key, "titlebar_height"))
        return set_int_option(value, 0, 128, &cfg->titlebar_height);
    if (str_ieq(key, "workspaces"))
        return set_int_option(value, 1, FLOWM_MAX_WORKSPACES,
                              &cfg->workspace_count);
    if (str_ieq(key, "snap_distance"))
        return set_int_option(value, 0, 256, &cfg->snap_distance);
    if (str_ieq(key, "move_step"))
        return set_int_option(value, 1, 512, &cfg->move_step);
    if (str_ieq(key, "resize_step"))
        return set_int_option(value, 1, 512, &cfg->resize_step);

    if (str_ieq(key, "focus_follows_mouse"))
        return set_bool_option(value, &cfg->focus_follows_mouse);
    if (str_ieq(key, "show_bar"))
        return set_bool_option(value, &cfg->show_bar);

    if (str_ieq(key, "border_focused"))
        return set_color_option(value, &cfg->color_border_focused);
    if (str_ieq(key, "border_unfocused"))
        return set_color_option(value, &cfg->color_border_unfocused);
    if (str_ieq(key, "border_urgent"))
        return set_color_option(value, &cfg->color_border_urgent);
    if (str_ieq(key, "titlebar_focused"))
        return set_color_option(value, &cfg->color_titlebar_focused);
    if (str_ieq(key, "titlebar_unfocused"))
        return set_color_option(value, &cfg->color_titlebar_unfocused);
    if (str_ieq(key, "title_text"))
        return set_color_option(value, &cfg->color_title_text);
    if (str_ieq(key, "bar_bg"))
        return set_color_option(value, &cfg->color_bar_bg);
    if (str_ieq(key, "bar_fg"))
        return set_color_option(value, &cfg->color_bar_fg);
    if (str_ieq(key, "bar_ws_active"))
        return set_color_option(value, &cfg->color_bar_ws_active);

    if (str_ieq(key, "font")) {
        xstrlcpy(cfg->font_name, value, sizeof(cfg->font_name));
        return true;
    }
    if (str_ieq(key, "log_file")) {
        xstrlcpy(cfg->log_path, value, sizeof(cfg->log_path));
        return true;
    }
    if (str_ieq(key, "log_level"))
        return log_level_from_name(value, &cfg->log_level);

    if (str_has_prefix(key, "workspace_name_")) {
        long idx;
        if (!parse_long(key + strlen("workspace_name_"), 1,
                        FLOWM_MAX_WORKSPACES, &idx))
            return false;
        xstrlcpy(cfg->workspace_names[idx - 1], value,
                 sizeof(cfg->workspace_names[0]));
        return true;
    }

    return false;
}

static bool
apply_bind(Config *cfg, const char *value)
{
    char buf[FLOWM_LINE_MAX];
    char *chord, *action_spec;
    unsigned int mods;
    unsigned long sym;
    Binding parsed;
    int i;

    xstrlcpy(buf, value, sizeof(buf));
    chord = str_trim(buf);
    action_spec = strchr(chord, ' ');
    if (action_spec == NULL)
        return false;
    *action_spec++ = '\0';
    action_spec = str_trim(action_spec);

    if (!config_parse_chord(chord, &mods, &sym))
        return false;
    if (!config_parse_action(action_spec, &parsed))
        return false;

    for (i = 0; i < cfg->binding_count; i++) {
        Binding *b = &cfg->bindings[i];
        if (b->modifiers == mods && b->keysym == (KeySym)sym) {
            free(b->arg_str);
            b->action   = parsed.action;
            b->arg_int  = parsed.arg_int;
            b->arg_int2 = parsed.arg_int2;
            b->arg_str  = parsed.arg_str;
            return true;
        }
    }

    if (cfg->binding_count >= FLOWM_MAX_BINDINGS) {
        free(parsed.arg_str);
        log_warn("config: binding table full, dropping '%s'", value);
        return false;
    }

    cfg->bindings[cfg->binding_count] = parsed;
    cfg->bindings[cfg->binding_count].modifiers = mods;
    cfg->bindings[cfg->binding_count].keysym = (KeySym)sym;
    cfg->binding_count++;
    return true;
}

static void
strip_inline_comment(char *s)
{
    char *p;

    for (p = s; *p != '\0'; p++) {
        if (*p == ';' && p > s && isspace((unsigned char)p[-1])) {
            *p = '\0';
            return;
        }
    }
}

static FILE *
open_config_file(const char *explicit_path, char *used, size_t used_size)
{
    const char *xdg, *home;
    FILE *fp;

    if (explicit_path != NULL) {
        xstrlcpy(used, explicit_path, used_size);
        return fopen(explicit_path, "r");
    }

    xdg = getenv("XDG_CONFIG_HOME");
    if (xdg != NULL && *xdg != '\0') {
        snprintf(used, used_size, "%s/flowm/flowm.conf", xdg);
        fp = fopen(used, "r");
        if (fp != NULL)
            return fp;
    }

    home = getenv("HOME");
    if (home != NULL && *home != '\0') {
        snprintf(used, used_size, "%s/.config/flowm/flowm.conf", home);
        fp = fopen(used, "r");
        if (fp != NULL)
            return fp;
    }

    used[0] = '\0';
    return NULL;
}

bool
config_load(Config *cfg, const char *explicit_path)
{
    char path[512];
    char line[FLOWM_LINE_MAX];
    FILE *fp;
    int lineno = 0;
    int errors = 0;

    fp = open_config_file(explicit_path, path, sizeof(path));
    if (fp == NULL) {
        if (explicit_path != NULL) {
            log_error("config: cannot open '%s'", explicit_path);
            return false;
        }
        log_info("config: no config file found, using defaults");
        return true;
    }

    log_info("config: loading %s", path);

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *text, *eq, *key, *value;

        lineno++;
        text = str_trim(line);

        if (*text == '\0' || *text == '#' || *text == ';')
            continue;

        strip_inline_comment(text);
        text = str_trim(text);
        if (*text == '\0')
            continue;

        eq = strchr(text, '=');
        if (eq == NULL) {
            log_warn("config:%s:%d: expected 'key = value': %s",
                     path, lineno, text);
            errors++;
            continue;
        }
        *eq = '\0';
        key   = str_trim(text);
        value = str_trim(eq + 1);

        if (*key == '\0' || *value == '\0') {
            log_warn("config:%s:%d: empty key or value", path, lineno);
            errors++;
            continue;
        }

        if (str_ieq(key, "rule")) {
            if (!rules_parse_line(&cfg->rules, value)) {
                log_warn("config:%s:%d: bad rule '%s'",
                         path, lineno, value);
                errors++;
            }
            continue;
        }

        if (str_ieq(key, "bind")) {
            if (!apply_bind(cfg, value)) {
                log_warn("config:%s:%d: bad binding '%s'",
                         path, lineno, value);
                errors++;
            }
            continue;
        }

        if (!config_apply_option(cfg, key, value)) {
            log_warn("config:%s:%d: unknown key or bad value: %s = %s",
                     path, lineno, key, value);
            errors++;
        }
    }

    fclose(fp);

    if (errors > 0)
        log_warn("config: %d error(s) in %s, offending lines ignored",
                 errors, path);
    else
        log_info("config: loaded successfully (%d bindings)",
                 cfg->binding_count);
    return true;
}
