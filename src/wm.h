/* wm.h - central data structures and lifecycle of the flowm window manager
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_WM_H
#define FLOWM_WM_H

#include <stdbool.h>
#include <X11/Xlib.h>

#include <signal.h>

#include "util.h"
#include "log.h"
#include "rules.h"

#define FLOWM_MAX_WORKSPACES 16
#define FLOWM_MAX_BINDINGS   128

struct WM;

typedef struct Client {
    Window win;
    Window frame;

    char title[FLOWM_TITLE_MAX];

    int x, y;
    int w, h;

    int sx, sy, sw, sh;

    int  base_w, base_h;
    int  min_w, min_h;
    int  max_w, max_h;
    int  inc_w, inc_h;
    float min_aspect, max_aspect;
    bool  is_fixed;

    int  workspace;

    bool is_fullscreen;
    bool is_maximized;
    bool is_mapped;
    bool never_focus;
    bool takes_wm_focus;
    bool deletes_window;
    bool is_urgent;

    int ignore_unmaps;

    struct Client *next;
} Client;

typedef enum ActionType {
    ACT_NONE = 0,
    ACT_EXEC,
    ACT_CLOSE,
    ACT_KILL,
    ACT_QUIT,
    ACT_WORKSPACE,
    ACT_SEND_TO_WORKSPACE,
    ACT_WORKSPACE_NEXT,
    ACT_WORKSPACE_PREV,
    ACT_FULLSCREEN,
    ACT_MAXIMIZE,
    ACT_FOCUS_NEXT,
    ACT_FOCUS_PREV,
    ACT_MOVE,
    ACT_RESIZE,
    ACT_CENTER,
    ACT_RAISE,
    ACT_LOWER
} ActionType;

typedef struct Binding {
    unsigned int modifiers;
    KeySym       keysym;
    ActionType   action;
    int          arg_int;
    int          arg_int2;
    char        *arg_str;
} Binding;

typedef struct Config {
    int border_width;
    int titlebar_height;
    int workspace_count;
    int snap_distance;
    int move_step;
    int resize_step;
    bool focus_follows_mouse;
    bool show_bar;

    unsigned long color_border_focused;
    unsigned long color_border_unfocused;
    unsigned long color_border_urgent;
    unsigned long color_titlebar_focused;
    unsigned long color_titlebar_unfocused;
    unsigned long color_title_text;
    unsigned long color_bar_bg;
    unsigned long color_bar_fg;
    unsigned long color_bar_ws_active;

    char font_name[128];
    char log_path[512];
    LogLevel log_level;

    Binding bindings[FLOWM_MAX_BINDINGS];
    int     binding_count;

    RuleSet rules;

    char workspace_names[FLOWM_MAX_WORKSPACES][32];
} Config;

typedef struct Workspace {
    char    name[32];
    Client *focused;
} Workspace;

typedef enum DragMode {
    DRAG_NONE = 0,
    DRAG_MOVE,
    DRAG_RESIZE
} DragMode;

typedef struct Drag {
    DragMode mode;
    Client  *client;
    int      start_root_x, start_root_y;
    int      start_x, start_y;
    int      start_w, start_h;
} Drag;

typedef struct Bar {
    Window window;
    int    height;
    GC     gc;
    XFontStruct *font;
    bool   mapped;
} Bar;

typedef struct WM {
    Display *dpy;
    int      screen;
    Window   root;
    int      screen_w, screen_h;

    Config    config;
    char      config_path[512];
    Workspace workspaces[FLOWM_MAX_WORKSPACES];
    int       current_ws;

    Client *clients;
    Client *focused;

    Drag drag;
    Bar  bar;

    Cursor cursor_normal;
    Cursor cursor_move;
    Cursor cursor_resize;

    Window check_win;

    unsigned long px_border_focused;
    unsigned long px_border_unfocused;
    unsigned long px_border_urgent;
    unsigned long px_titlebar_focused;
    unsigned long px_titlebar_unfocused;
    unsigned long px_title_text;
    unsigned long px_bar_bg;
    unsigned long px_bar_fg;
    unsigned long px_bar_ws_active;

    GC frame_gc;
    XFontStruct *frame_font;

    volatile sig_atomic_t running;
} WM;

bool wm_init(WM *wm, const char *config_path);

void wm_scan_existing(WM *wm);

void wm_run(WM *wm);

void wm_shutdown(WM *wm);

void wm_quit(WM *wm);

void wm_reload_config(WM *wm);

void wm_apply_visual_config(WM *wm);

void wm_apply_config_string(WM *wm, const char *text);

void wm_usable_area(const WM *wm, int *x, int *y, int *w, int *h);

unsigned long wm_alloc_pixel(WM *wm, unsigned long rgb);

#endif
