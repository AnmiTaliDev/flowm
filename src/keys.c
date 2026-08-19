/* keys.c - key grabbing and action dispatch
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include "keys.h"
#include "client.h"
#include "workspace.h"
#include "log.h"
#include "util.h"

static unsigned int numlock_mask = 0;

static void
detect_numlock_mask(WM *wm)
{
    XModifierKeymap *modmap;
    KeyCode numlock_code;
    int row, col;

    numlock_mask = 0;
    numlock_code = XKeysymToKeycode(wm->dpy, XK_Num_Lock);
    if (numlock_code == 0)
        return;

    modmap = XGetModifierMapping(wm->dpy);
    if (modmap == NULL)
        return;

    for (row = 0; row < 8; row++) {
        for (col = 0; col < modmap->max_keypermod; col++) {
            KeyCode code =
                modmap->modifiermap[row * modmap->max_keypermod + col];
            if (code == numlock_code)
                numlock_mask = 1U << row;
        }
    }
    XFreeModifiermap(modmap);

    log_debug("keys: numlock mask = 0x%x", numlock_mask);
}

static unsigned int
lock_combos(int i)
{
    static const unsigned int base[4] = { 0, LockMask, 0, 0 };
    unsigned int combos[4];

    combos[0] = base[0];
    combos[1] = base[1];
    combos[2] = numlock_mask;
    combos[3] = numlock_mask | LockMask;
    return combos[i];
}

void
keys_grab(WM *wm)
{
    int i, j;

    detect_numlock_mask(wm);
    XUngrabKey(wm->dpy, AnyKey, AnyModifier, wm->root);

    for (i = 0; i < wm->config.binding_count; i++) {
        const Binding *b = &wm->config.bindings[i];
        KeyCode code = XKeysymToKeycode(wm->dpy, b->keysym);

        if (code == 0) {
            log_warn("keys: keysym 0x%lx has no keycode, "
                     "binding skipped", (unsigned long)b->keysym);
            continue;
        }

        for (j = 0; j < 4; j++) {
            unsigned int extra = lock_combos(j);
            if (j >= 2 && numlock_mask == 0)
                continue;
            XGrabKey(wm->dpy, code, b->modifiers | extra, wm->root,
                     True, GrabModeAsync, GrabModeAsync);
        }
    }

    log_info("keys: grabbed %d bindings", wm->config.binding_count);
}

void
keys_ungrab(WM *wm)
{
    XUngrabKey(wm->dpy, AnyKey, AnyModifier, wm->root);
}

void
keys_run_action(WM *wm, const Binding *b)
{
    Client *c = wm->focused;

    switch (b->action) {
    case ACT_EXEC:
        spawn_shell(b->arg_str);
        break;

    case ACT_CLOSE:
        if (c != NULL)
            client_close(wm, c);
        break;

    case ACT_KILL:
        if (c != NULL)
            client_kill(wm, c);
        break;

    case ACT_QUIT:
        log_info("action: quit requested");
        wm_quit(wm);
        break;

    case ACT_WORKSPACE:
        workspace_switch(wm, b->arg_int);
        break;

    case ACT_SEND_TO_WORKSPACE:
        if (c != NULL)
            client_send_to_workspace(wm, c, b->arg_int);
        break;

    case ACT_WORKSPACE_NEXT:
        workspace_next(wm);
        break;

    case ACT_WORKSPACE_PREV:
        workspace_prev(wm);
        break;

    case ACT_FULLSCREEN:
        if (c != NULL)
            client_set_fullscreen(wm, c, !c->is_fullscreen);
        break;

    case ACT_MAXIMIZE:
        if (c != NULL)
            client_toggle_maximize(wm, c);
        break;

    case ACT_FOCUS_NEXT:
        client_focus_cycle(wm, true);
        break;

    case ACT_FOCUS_PREV:
        client_focus_cycle(wm, false);
        break;

    case ACT_MOVE:
        if (c != NULL && !c->is_fullscreen)
            client_move_resize(wm, c,
                               c->x + b->arg_int * wm->config.move_step,
                               c->y + b->arg_int2 * wm->config.move_step,
                               c->w, c->h, false);
        break;

    case ACT_RESIZE:
        if (c != NULL && !c->is_fullscreen && !c->is_fixed)
            client_move_resize(wm, c, c->x, c->y,
                               c->w + b->arg_int * wm->config.resize_step,
                               c->h + b->arg_int2 * wm->config.resize_step,
                               true);
        break;

    case ACT_CENTER:
        if (c != NULL && !c->is_fullscreen)
            client_center(wm, c);
        break;

    case ACT_RAISE:
        if (c != NULL)
            client_raise(wm, c);
        break;

    case ACT_LOWER:
        if (c != NULL)
            client_lower(wm, c);
        break;

    case ACT_NONE:
    default:
        break;
    }
}

void
keys_handle_keypress(WM *wm, XKeyEvent *ev)
{
    KeySym sym;
    unsigned int clean_mods;
    int i;

    sym = XkbKeycodeToKeysym(wm->dpy, (KeyCode)ev->keycode, 0, 0);
    clean_mods = ev->state & ~(LockMask | numlock_mask);
    clean_mods &= (ShiftMask | ControlMask | Mod1Mask | Mod2Mask |
                   Mod3Mask | Mod4Mask | Mod5Mask);

    for (i = 0; i < wm->config.binding_count; i++) {
        const Binding *b = &wm->config.bindings[i];

        if (b->keysym == sym && b->modifiers == clean_mods) {
            log_debug("keys: chord matched, action=%d", (int)b->action);
            keys_run_action(wm, b);
            return;
        }
    }
}
