/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>

#include "ipc.h"
#include "ewmh.h"
#include "util.h"

#define IPC_CMD_MAX 4096

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
    else
        wm_apply_config_string(wm, text);
}
