/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "ipc.h"
#include "ewmh.h"
#include "log.h"
#include "util.h"

#define IPC_CMD_MAX   4096
#define IPC_REPLY_MAX 8192

static void
send_reply(WM *wm, const char *text)
{
    XChangeProperty(wm->dpy, wm->root, ewmh_atom(ATOM_FLOWM_REPLY),
                    XA_STRING, 8, PropModeReplace,
                    (const unsigned char *)text, (int)strlen(text));
    XFlush(wm->dpy);
}

static void
reply_workspace(WM *wm)
{
    char buf[128];

    snprintf(buf, sizeof(buf), "%d\t%s\n", wm->current_ws + 1,
             wm->workspaces[wm->current_ws].name);
    send_reply(wm, buf);
}

static void
reply_focused(WM *wm)
{
    char buf[FLOWM_TITLE_MAX + 32];

    if (wm->focused == NULL)
        buf[0] = '\0';
    else
        snprintf(buf, sizeof(buf), "0x%lx\t%s\n",
                 wm->focused->win, wm->focused->title);
    send_reply(wm, buf);
}

static void
reply_windows(WM *wm)
{
    char buf[IPC_REPLY_MAX];
    size_t off = 0;
    const Client *c;

    buf[0] = '\0';
    for (c = wm->clients; c != NULL && off < sizeof(buf); c = c->next) {
        int n;

        if (c->is_sticky)
            n = snprintf(buf + off, sizeof(buf) - off,
                         "0x%lx\tall\t%s\n", c->win, c->title);
        else if (c->workspace == WORKSPACE_HIDDEN)
            n = snprintf(buf + off, sizeof(buf) - off,
                         "0x%lx\thidden\t%s\n", c->win, c->title);
        else
            n = snprintf(buf + off, sizeof(buf) - off,
                         "0x%lx\t%d\t%s\n", c->win,
                         c->workspace + 1, c->title);
        if (n < 0 || (size_t)n >= sizeof(buf) - off)
            break;
        off += (size_t)n;
    }
    send_reply(wm, buf);
}

static void
handle_query(WM *wm, const char *what)
{
    if (str_ieq(what, "workspace"))
        reply_workspace(wm);
    else if (str_ieq(what, "focused"))
        reply_focused(wm);
    else if (str_ieq(what, "windows"))
        reply_windows(wm);
    else {
        log_warn("ipc: unknown query '%s'", what);
        send_reply(wm, "");
    }
}

void
ipc_handle_property(WM *wm)
{
    unsigned char *data = NULL;
    Atom type;
    int format;
    unsigned long nitems, after;
    char buf[IPC_CMD_MAX];
    char *text;

    if (XGetWindowProperty(wm->dpy, wm->root, ewmh_atom(ATOM_FLOWM_CMD),
                           0, IPC_CMD_MAX / 4, True, AnyPropertyType,
                           &type, &format, &nitems, &after,
                           &data) != Success || data == NULL)
        return;

    if (nitems == 0) {
        XFree(data);
        return;
    }

    xstrlcpy(buf, (const char *)data, sizeof(buf));
    XFree(data);
    text = str_trim(buf);

    if (str_ieq(text, "reload"))
        wm_reload_config(wm);
    else if (str_has_prefix(text, "get "))
        handle_query(wm, str_trim(text + strlen("get ")));
    else
        wm_apply_config_string(wm, text);
}
