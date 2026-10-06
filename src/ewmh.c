/* ewmh.c - ICCCM/EWMH property plumbing
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "ewmh.h"
#include "wm.h"
#include "log.h"
#include "util.h"

static Atom atoms[ATOM_COUNT];

static const char *atom_names[ATOM_COUNT] = {
    [ATOM_WM_PROTOCOLS]          = "WM_PROTOCOLS",
    [ATOM_WM_DELETE_WINDOW]      = "WM_DELETE_WINDOW",
    [ATOM_WM_TAKE_FOCUS]         = "WM_TAKE_FOCUS",
    [ATOM_WM_STATE]              = "WM_STATE",

    [ATOM_NET_SUPPORTED]         = "_NET_SUPPORTED",
    [ATOM_NET_SUPPORTING_WM_CHECK] = "_NET_SUPPORTING_WM_CHECK",
    [ATOM_NET_WM_NAME]           = "_NET_WM_NAME",
    [ATOM_NET_CLIENT_LIST]       = "_NET_CLIENT_LIST",
    [ATOM_NET_ACTIVE_WINDOW]     = "_NET_ACTIVE_WINDOW",
    [ATOM_NET_NUMBER_OF_DESKTOPS] = "_NET_NUMBER_OF_DESKTOPS",
    [ATOM_NET_CURRENT_DESKTOP]   = "_NET_CURRENT_DESKTOP",
    [ATOM_NET_DESKTOP_NAMES]     = "_NET_DESKTOP_NAMES",
    [ATOM_NET_WM_DESKTOP]        = "_NET_WM_DESKTOP",
    [ATOM_NET_WM_STATE]          = "_NET_WM_STATE",
    [ATOM_NET_WM_STATE_FULLSCREEN] = "_NET_WM_STATE_FULLSCREEN",
    [ATOM_NET_WM_STATE_DEMANDS_ATTENTION] =
        "_NET_WM_STATE_DEMANDS_ATTENTION",
    [ATOM_NET_WM_STATE_STICKY]   = "_NET_WM_STATE_STICKY",
    [ATOM_NET_WM_WINDOW_TYPE]    = "_NET_WM_WINDOW_TYPE",
    [ATOM_NET_WM_WINDOW_TYPE_DOCK] = "_NET_WM_WINDOW_TYPE_DOCK",
    [ATOM_NET_WM_WINDOW_TYPE_DESKTOP] = "_NET_WM_WINDOW_TYPE_DESKTOP",
    [ATOM_NET_WM_WINDOW_TYPE_DIALOG]  = "_NET_WM_WINDOW_TYPE_DIALOG",
    [ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION] =
        "_NET_WM_WINDOW_TYPE_NOTIFICATION",

    [ATOM_UTF8_STRING]           = "UTF8_STRING",

    [ATOM_FLOWM_CMD]             = "_FLOWM_CMD",
};

void
ewmh_init_atoms(WM *wm)
{
    char *names[ATOM_COUNT];
    int i;

    for (i = 0; i < ATOM_COUNT; i++)
        names[i] = (char *)atom_names[i];

    if (!XInternAtoms(wm->dpy, names, ATOM_COUNT, False, atoms))
        log_fatal("XInternAtoms failed");

    log_debug("ewmh: interned %d atoms", ATOM_COUNT);
}

Atom
ewmh_atom(AtomId id)
{
    return atoms[id];
}

void
ewmh_setup(WM *wm)
{
    Atom supported[16];
    int n = 0;
    const char wm_name[] = "flowm";

    wm->check_win = XCreateSimpleWindow(wm->dpy, wm->root,
                                        -1, -1, 1, 1, 0, 0, 0);

    XChangeProperty(wm->dpy, wm->check_win,
                    atoms[ATOM_NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32,
                    PropModeReplace,
                    (unsigned char *)&wm->check_win, 1);
    XChangeProperty(wm->dpy, wm->check_win, atoms[ATOM_NET_WM_NAME],
                    atoms[ATOM_UTF8_STRING], 8, PropModeReplace,
                    (const unsigned char *)wm_name, sizeof(wm_name) - 1);

    XChangeProperty(wm->dpy, wm->root,
                    atoms[ATOM_NET_SUPPORTING_WM_CHECK], XA_WINDOW, 32,
                    PropModeReplace,
                    (unsigned char *)&wm->check_win, 1);

    supported[n++] = atoms[ATOM_NET_SUPPORTED];
    supported[n++] = atoms[ATOM_NET_SUPPORTING_WM_CHECK];
    supported[n++] = atoms[ATOM_NET_WM_NAME];
    supported[n++] = atoms[ATOM_NET_CLIENT_LIST];
    supported[n++] = atoms[ATOM_NET_ACTIVE_WINDOW];
    supported[n++] = atoms[ATOM_NET_NUMBER_OF_DESKTOPS];
    supported[n++] = atoms[ATOM_NET_CURRENT_DESKTOP];
    supported[n++] = atoms[ATOM_NET_DESKTOP_NAMES];
    supported[n++] = atoms[ATOM_NET_WM_DESKTOP];
    supported[n++] = atoms[ATOM_NET_WM_STATE];
    supported[n++] = atoms[ATOM_NET_WM_STATE_FULLSCREEN];
    supported[n++] = atoms[ATOM_NET_WM_STATE_DEMANDS_ATTENTION];
    supported[n++] = atoms[ATOM_NET_WM_STATE_STICKY];
    supported[n++] = atoms[ATOM_NET_WM_WINDOW_TYPE];

    XChangeProperty(wm->dpy, wm->root, atoms[ATOM_NET_SUPPORTED],
                    XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)supported, n);

    ewmh_update_desktops(wm);
    ewmh_update_client_list(wm);
    ewmh_update_active_window(wm);

    log_info("ewmh: advertising %d supported hints", n);
}

void
ewmh_teardown(WM *wm)
{
    XDeleteProperty(wm->dpy, wm->root, atoms[ATOM_NET_SUPPORTED]);
    XDeleteProperty(wm->dpy, wm->root, atoms[ATOM_NET_CLIENT_LIST]);
    XDeleteProperty(wm->dpy, wm->root, atoms[ATOM_NET_ACTIVE_WINDOW]);
    XDeleteProperty(wm->dpy, wm->root,
                    atoms[ATOM_NET_SUPPORTING_WM_CHECK]);
    if (wm->check_win != None) {
        XDestroyWindow(wm->dpy, wm->check_win);
        wm->check_win = None;
    }
}

void
ewmh_update_client_list(WM *wm)
{
    Window list[256];
    int n = 0;
    Client *c;

    for (c = wm->clients; c != NULL && n < (int)LENGTH(list); c = c->next)
        list[n++] = c->win;

    XChangeProperty(wm->dpy, wm->root, atoms[ATOM_NET_CLIENT_LIST],
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)list, n);
}

void
ewmh_update_active_window(WM *wm)
{
    Window active = wm->focused != NULL ? wm->focused->win : None;

    XChangeProperty(wm->dpy, wm->root, atoms[ATOM_NET_ACTIVE_WINDOW],
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&active, 1);
}

void
ewmh_update_desktops(WM *wm)
{
    long count = wm->config.workspace_count;
    long current = wm->current_ws;
    char names[FLOWM_MAX_WORKSPACES * 32];
    int off = 0, i;

    XChangeProperty(wm->dpy, wm->root,
                    atoms[ATOM_NET_NUMBER_OF_DESKTOPS], XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)&count, 1);
    XChangeProperty(wm->dpy, wm->root, atoms[ATOM_NET_CURRENT_DESKTOP],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&current, 1);

    for (i = 0; i < wm->config.workspace_count; i++) {
        int len = (int)strlen(wm->workspaces[i].name) + 1;
        if (off + len > (int)sizeof(names))
            break;
        memcpy(names + off, wm->workspaces[i].name, (size_t)len);
        off += len;
    }
    XChangeProperty(wm->dpy, wm->root, atoms[ATOM_NET_DESKTOP_NAMES],
                    atoms[ATOM_UTF8_STRING], 8, PropModeReplace,
                    (unsigned char *)names, off);
}

void
ewmh_set_client_desktop(WM *wm, Client *c)
{
    long ws = c->is_sticky ? 0xFFFFFFFFL : (long)c->workspace;

    XChangeProperty(wm->dpy, c->win, atoms[ATOM_NET_WM_DESKTOP],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&ws, 1);
}

static void
update_net_wm_state(WM *wm, Client *c)
{
    Atom states[2];
    int count = 0;

    if (c->is_fullscreen)
        states[count++] = atoms[ATOM_NET_WM_STATE_FULLSCREEN];
    if (c->is_sticky)
        states[count++] = atoms[ATOM_NET_WM_STATE_STICKY];

    if (count > 0) {
        XChangeProperty(wm->dpy, c->win, atoms[ATOM_NET_WM_STATE],
                        XA_ATOM, 32, PropModeReplace,
                        (unsigned char *)states, count);
    } else {
        XChangeProperty(wm->dpy, c->win, atoms[ATOM_NET_WM_STATE],
                        XA_ATOM, 32, PropModeReplace, NULL, 0);
    }
}

void
ewmh_set_fullscreen_state(WM *wm, Client *c, bool fullscreen)
{
    (void)fullscreen;
    update_net_wm_state(wm, c);
}

void
ewmh_set_sticky_state(WM *wm, Client *c, bool sticky)
{
    (void)sticky;
    update_net_wm_state(wm, c);
}

void
ewmh_set_wm_state(WM *wm, Client *c, long state)
{
    long data[2] = { state, None };

    XChangeProperty(wm->dpy, c->win, atoms[ATOM_WM_STATE],
                    atoms[ATOM_WM_STATE], 32, PropModeReplace,
                    (unsigned char *)data, 2);
}

static bool
fetch_text_property(WM *wm, Window win, Atom prop,
                    char *buf, size_t bufsize)
{
    XTextProperty tp;
    bool ok = false;

    if (XGetTextProperty(wm->dpy, win, &tp, prop) == 0 ||
        tp.nitems == 0 || tp.value == NULL)
        return false;

    if (tp.encoding == atoms[ATOM_UTF8_STRING] ||
        tp.encoding == XA_STRING) {
        xstrlcpy(buf, (char *)tp.value, bufsize);
        ok = true;
    } else {
        char **list = NULL;
        int count = 0;
        if (XTextPropertyToStringList(&tp, &list, &count) != 0 &&
            count > 0 && list != NULL) {
            xstrlcpy(buf, list[0], bufsize);
            ok = true;
        }
        if (list != NULL)
            XFreeStringList(list);
    }

    XFree(tp.value);
    return ok;
}

bool
ewmh_fetch_title(WM *wm, Window win, char *buf, size_t bufsize)
{
    if (fetch_text_property(wm, win, atoms[ATOM_NET_WM_NAME],
                            buf, bufsize))
        return true;
    if (fetch_text_property(wm, win, XA_WM_NAME, buf, bufsize))
        return true;

    xstrlcpy(buf, "(untitled)", bufsize);
    return false;
}

void
ewmh_update_protocols(WM *wm, Client *c)
{
    Atom *protocols = NULL;
    int count = 0, i;

    c->deletes_window = false;
    c->takes_wm_focus = false;

    if (XGetWMProtocols(wm->dpy, c->win, &protocols, &count) == 0)
        return;

    for (i = 0; i < count; i++) {
        if (protocols[i] == atoms[ATOM_WM_DELETE_WINDOW])
            c->deletes_window = true;
        else if (protocols[i] == atoms[ATOM_WM_TAKE_FOCUS])
            c->takes_wm_focus = true;
    }
    if (protocols != NULL)
        XFree(protocols);
}

bool
ewmh_is_unmanaged_type(WM *wm, Window win, bool *is_dialog)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *data = NULL;
    bool unmanaged = false;
    unsigned long i;

    if (is_dialog != NULL)
        *is_dialog = false;

    if (XGetWindowProperty(wm->dpy, win,
                           atoms[ATOM_NET_WM_WINDOW_TYPE], 0, 8, False,
                           XA_ATOM, &actual_type, &actual_format,
                           &nitems, &bytes_after, &data) != Success ||
        data == NULL)
        return false;

    for (i = 0; i < nitems; i++) {
        Atom t = ((Atom *)(void *)data)[i];

        if (t == atoms[ATOM_NET_WM_WINDOW_TYPE_DOCK] ||
            t == atoms[ATOM_NET_WM_WINDOW_TYPE_DESKTOP] ||
            t == atoms[ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION])
            unmanaged = true;
        if (t == atoms[ATOM_NET_WM_WINDOW_TYPE_DIALOG] &&
            is_dialog != NULL)
            *is_dialog = true;
    }

    XFree(data);
    return unmanaged;
}

bool
ewmh_send_protocol(WM *wm, Window win, Atom protocol, Time t)
{
    XEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.type                 = ClientMessage;
    ev.xclient.window       = win;
    ev.xclient.message_type = atoms[ATOM_WM_PROTOCOLS];
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = (long)protocol;
    ev.xclient.data.l[1]    = (long)t;

    return XSendEvent(wm->dpy, win, False, NoEventMask, &ev) != 0;
}
