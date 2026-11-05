/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "struts.h"
#include "ewmh.h"
#include "log.h"
#include "util.h"

static Strut *
find_strut(const WM *wm, Window win)
{
    int i;

    for (i = 0; i < wm->strut_count; i++)
        if (wm->struts[i].win == win)
            return (Strut *)&wm->struts[i];
    return NULL;
}

static bool
read_strut_property(WM *wm, Window win, AtomId id, unsigned long need,
                    Strut *out)
{
    Atom type;
    int format;
    unsigned long nitems, after;
    unsigned char *data = NULL;
    const long *v;

    if (XGetWindowProperty(wm->dpy, win, ewmh_atom(id), 0, 12, False,
                           XA_CARDINAL, &type, &format, &nitems, &after,
                           &data) != Success || data == NULL)
        return false;

    if (type != XA_CARDINAL || format != 32 || nitems < need) {
        XFree(data);
        return false;
    }

    v = (const long *)(void *)data;
    out->left   = (int)v[0];
    out->right  = (int)v[1];
    out->top    = (int)v[2];
    out->bottom = (int)v[3];
    XFree(data);
    return true;
}

static void
read_struts(WM *wm, Strut *s)
{
    s->left = s->right = s->top = s->bottom = 0;

    if (!read_strut_property(wm, s->win, ATOM_NET_WM_STRUT_PARTIAL, 12, s))
        read_strut_property(wm, s->win, ATOM_NET_WM_STRUT, 4, s);
}

void
struts_add(WM *wm, Window win)
{
    Strut *s = find_strut(wm, win);

    if (s == NULL) {
        if (wm->strut_count >= FLOWM_MAX_STRUTS) {
            log_warn("struts: table full (%d), 0x%lx not tracked",
                     FLOWM_MAX_STRUTS, win);
            return;
        }
        s = &wm->struts[wm->strut_count++];
        s->win = win;
    }

    XSelectInput(wm->dpy, win, PropertyChangeMask);
    read_struts(wm, s);
    log_debug("struts: 0x%lx l=%d r=%d t=%d b=%d",
              win, s->left, s->right, s->top, s->bottom);
}

void
struts_remove(WM *wm, Window win)
{
    Strut *s = find_strut(wm, win);

    if (s == NULL)
        return;
    *s = wm->struts[--wm->strut_count];
}

void
struts_refresh(WM *wm, Window win)
{
    Strut *s = find_strut(wm, win);

    if (s == NULL)
        return;
    read_struts(wm, s);
    log_debug("struts: 0x%lx updated l=%d r=%d t=%d b=%d",
              win, s->left, s->right, s->top, s->bottom);
}

bool
struts_tracked(const WM *wm, Window win)
{
    return find_strut(wm, win) != NULL;
}

void
struts_reserved(const WM *wm, int *left, int *right, int *top, int *bottom)
{
    int i;

    *left = *right = *top = *bottom = 0;
    for (i = 0; i < wm->strut_count; i++) {
        const Strut *s = &wm->struts[i];

        *left   = MAX(*left, s->left);
        *right  = MAX(*right, s->right);
        *top    = MAX(*top, s->top);
        *bottom = MAX(*bottom, s->bottom);
    }
}
