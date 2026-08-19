/* ewmh.h - ICCCM and EWMH protocol support for flowm
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FLOWM_EWMH_H
#define FLOWM_EWMH_H

#include <stdbool.h>
#include <X11/Xlib.h>

struct WM;
struct Client;

typedef enum AtomId {
    ATOM_WM_PROTOCOLS = 0,
    ATOM_WM_DELETE_WINDOW,
    ATOM_WM_TAKE_FOCUS,
    ATOM_WM_STATE,

    ATOM_NET_SUPPORTED,
    ATOM_NET_SUPPORTING_WM_CHECK,
    ATOM_NET_WM_NAME,
    ATOM_NET_CLIENT_LIST,
    ATOM_NET_ACTIVE_WINDOW,
    ATOM_NET_NUMBER_OF_DESKTOPS,
    ATOM_NET_CURRENT_DESKTOP,
    ATOM_NET_DESKTOP_NAMES,
    ATOM_NET_WM_DESKTOP,
    ATOM_NET_WM_STATE,
    ATOM_NET_WM_STATE_FULLSCREEN,
    ATOM_NET_WM_STATE_DEMANDS_ATTENTION,
    ATOM_NET_WM_WINDOW_TYPE,
    ATOM_NET_WM_WINDOW_TYPE_DOCK,
    ATOM_NET_WM_WINDOW_TYPE_DESKTOP,
    ATOM_NET_WM_WINDOW_TYPE_DIALOG,
    ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION,

    ATOM_UTF8_STRING,

    ATOM_FLOWM_CMD,

    ATOM_COUNT
} AtomId;

#define NET_WM_STATE_REMOVE 0
#define NET_WM_STATE_ADD    1
#define NET_WM_STATE_TOGGLE 2

void ewmh_init_atoms(struct WM *wm);

Atom ewmh_atom(AtomId id);

void ewmh_setup(struct WM *wm);

void ewmh_teardown(struct WM *wm);

void ewmh_update_client_list(struct WM *wm);

void ewmh_update_active_window(struct WM *wm);

void ewmh_update_desktops(struct WM *wm);

void ewmh_set_client_desktop(struct WM *wm, struct Client *c);

void ewmh_set_fullscreen_state(struct WM *wm, struct Client *c,
                               bool fullscreen);

void ewmh_set_wm_state(struct WM *wm, struct Client *c, long state);

bool ewmh_fetch_title(struct WM *wm, Window win, char *buf, size_t bufsize);

void ewmh_update_protocols(struct WM *wm, struct Client *c);

bool ewmh_is_unmanaged_type(struct WM *wm, Window win, bool *is_dialog);

bool ewmh_send_protocol(struct WM *wm, Window win, Atom protocol, Time t);

#endif
