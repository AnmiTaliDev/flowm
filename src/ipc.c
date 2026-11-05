/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>

#include "ipc.h"
#include "ewmh.h"

void
ipc_handle_property(WM *wm)
{
    unsigned char *data = NULL;
    Atom type;
    int format;
    unsigned long nitems, after;

    if (XGetWindowProperty(wm->dpy, wm->root, ewmh_atom(ATOM_FLOWM_CMD),
                           0, 4096 / 4, True, AnyPropertyType,
                           &type, &format, &nitems, &after,
                           &data) != Success || data == NULL)
        return;

    if (nitems > 0)
        wm_apply_config_string(wm, (const char *)data);

    XFree(data);
}
