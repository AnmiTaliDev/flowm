/* events.c - X event handlers
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "events.h"
#include "client.h"
#include "ewmh.h"
#include "keys.h"
#include "bar.h"
#include "workspace.h"
#include "log.h"
#include "util.h"

#define DRAG_MIN_SIZE 32

static void
handle_map_request(WM *wm, XMapRequestEvent *ev)
{
    Client *c = client_from_window(wm, ev->window);

    if (c != NULL) {
        client_show(wm, c);
        return;
    }
    client_manage(wm, ev->window, false);
}

static void
handle_unmap_notify(WM *wm, XUnmapEvent *ev)
{
    Client *c = client_from_window(wm, ev->window);

    if (c == NULL)
        return;

    if (c->ignore_unmaps > 0) {
        c->ignore_unmaps--;
        log_debug("unmap: 0x%lx ignored (%d left)",
                  ev->window, c->ignore_unmaps);
        return;
    }

    log_debug("unmap: 0x%lx withdrawing", ev->window);
    client_unmanage(wm, c, false);
}

static void
handle_destroy_notify(WM *wm, XDestroyWindowEvent *ev)
{
    Client *c = client_from_window(wm, ev->window);

    if (c != NULL)
        client_unmanage(wm, c, true);
}

static void
handle_configure_request(WM *wm, XConfigureRequestEvent *ev)
{
    Client *c = client_from_window(wm, ev->window);

    if (c == NULL) {
        XWindowChanges wc;

        wc.x            = ev->x;
        wc.y            = ev->y;
        wc.width        = ev->width;
        wc.height       = ev->height;
        wc.border_width = ev->border_width;
        wc.sibling      = ev->above;
        wc.stack_mode   = ev->detail;
        XConfigureWindow(wm->dpy, ev->window,
                         (unsigned)ev->value_mask, &wc);
        return;
    }

    if (c->is_fullscreen) {
        (void)0;
    } else {
        int x = c->x, y = c->y, w = c->w, h = c->h;

        if (ev->value_mask & CWX)
            x = ev->x;
        if (ev->value_mask & CWY)
            y = ev->y;
        if (ev->value_mask & CWWidth)
            w = ev->width;
        if (ev->value_mask & CWHeight)
            h = ev->height;

        client_move_resize(wm, c, x, y, w, h, false);
    }

    if (ev->value_mask & CWStackMode) {
        if (ev->detail == Above)
            client_raise(wm, c);
        else if (ev->detail == Below)
            client_lower(wm, c);
    }

    {
        XConfigureEvent ce;

        memset(&ce, 0, sizeof(ce));
        ce.type   = ConfigureNotify;
        ce.event  = c->win;
        ce.window = c->win;
        ce.x      = c->x;
        ce.y      = c->y;
        ce.width  = c->w;
        ce.height = c->h;
        XSendEvent(wm->dpy, c->win, False, StructureNotifyMask,
                   (XEvent *)&ce);
    }
}

static void
begin_drag(WM *wm, Client *c, DragMode mode, int root_x, int root_y)
{
    Cursor cur = mode == DRAG_MOVE ? wm->cursor_move
                                   : wm->cursor_resize;

    if (c->is_fullscreen)
        return;
    if (mode == DRAG_RESIZE && c->is_fixed)
        return;

    if (XGrabPointer(wm->dpy, wm->root, False,
                     ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, None, cur,
                     CurrentTime) != GrabSuccess) {
        log_warn("drag: pointer grab failed");
        return;
    }

    wm->drag.mode         = mode;
    wm->drag.client       = c;
    wm->drag.start_root_x = root_x;
    wm->drag.start_root_y = root_y;
    wm->drag.start_x      = c->x;
    wm->drag.start_y      = c->y;
    wm->drag.start_w      = c->w;
    wm->drag.start_h      = c->h;

    if (mode == DRAG_MOVE)
        c->is_maximized = false;

    log_debug("drag: begin %s on 0x%lx",
              mode == DRAG_MOVE ? "move" : "resize", c->win);
}

static void
update_drag(WM *wm, int root_x, int root_y)
{
    Drag *d = &wm->drag;
    int dx = root_x - d->start_root_x;
    int dy = root_y - d->start_root_y;

    if (d->mode == DRAG_NONE || d->client == NULL)
        return;

    if (d->mode == DRAG_MOVE) {
        int nx = d->start_x + dx;
        int ny = d->start_y + dy;

        client_snap_position(wm, d->client, &nx, &ny);
        client_move_resize(wm, d->client, nx, ny,
                           d->client->w, d->client->h, false);
    } else {
        int nw = MAX(DRAG_MIN_SIZE, d->start_w + dx);
        int nh = MAX(DRAG_MIN_SIZE, d->start_h + dy);

        client_move_resize(wm, d->client,
                           d->client->x, d->client->y, nw, nh, true);
    }
}

static void
end_drag(WM *wm)
{
    if (wm->drag.mode == DRAG_NONE)
        return;

    XUngrabPointer(wm->dpy, CurrentTime);
    log_debug("drag: end");
    wm->drag.mode   = DRAG_NONE;
    wm->drag.client = NULL;
}

static void
handle_button_press(WM *wm, XButtonEvent *ev)
{
    Client *c;
    unsigned int mod = wm->config.bindings[0].modifiers;

    mod = Mod4Mask;

    if (bar_owns_window(wm, ev->window)) {
        bar_handle_click(wm, ev->x, ev->y);
        return;
    }

    c = client_from_frame(wm, ev->window);
    if (c != NULL) {
        int th = client_titlebar_height(wm, c);

        client_focus(wm, c);
        client_raise(wm, c);

        if (ev->y < th) {
            if (ev->x >= c->w - client_close_button_width(wm)) {
                client_close(wm, c);
                return;
            }
            if (ev->button == Button1)
                begin_drag(wm, c, DRAG_MOVE, ev->x_root, ev->y_root);
            else if (ev->button == Button3)
                begin_drag(wm, c, DRAG_RESIZE, ev->x_root, ev->y_root);
        }
        return;
    }

    c = client_from_window(wm, ev->window);
    if (c != NULL) {
        client_focus(wm, c);
        client_raise(wm, c);

        if ((ev->state & mod) && ev->button == Button1) {
            XAllowEvents(wm->dpy, AsyncPointer, ev->time);
            begin_drag(wm, c, DRAG_MOVE, ev->x_root, ev->y_root);
        } else if ((ev->state & mod) && ev->button == Button3) {
            XAllowEvents(wm->dpy, AsyncPointer, ev->time);
            begin_drag(wm, c, DRAG_RESIZE, ev->x_root, ev->y_root);
        } else {
            XAllowEvents(wm->dpy, ReplayPointer, ev->time);
        }
        XSync(wm->dpy, False);
    }
}

static void
handle_motion(WM *wm, XMotionEvent *ev)
{
    XEvent skip;

    while (XCheckTypedEvent(wm->dpy, MotionNotify, &skip))
        *ev = skip.xmotion;

    update_drag(wm, ev->x_root, ev->y_root);
}

static void
handle_button_release(WM *wm, XButtonEvent *ev)
{
    UNUSED(ev);
    end_drag(wm);
}

static void
handle_enter_notify(WM *wm, XCrossingEvent *ev)
{
    Client *c;

    if (!wm->config.focus_follows_mouse)
        return;
    if (ev->mode != NotifyNormal || ev->detail == NotifyInferior)
        return;

    c = client_from_frame(wm, ev->window);
    if (c == NULL)
        c = client_from_window(wm, ev->window);
    if (c != NULL && c != wm->focused)
        client_focus(wm, c);
}

static void
handle_property_notify(WM *wm, XPropertyEvent *ev)
{
    Client *c = client_from_window(wm, ev->window);

    if (c == NULL)
        return;

    if (ev->atom == XA_WM_NAME ||
        ev->atom == ewmh_atom(ATOM_NET_WM_NAME)) {
        client_update_title(wm, c);
    } else if (ev->atom == XA_WM_NORMAL_HINTS) {
        client_update_size_hints(wm, c);
    } else if (ev->atom == XA_WM_HINTS) {
        client_update_wm_hints(wm, c);
    } else if (ev->atom == ewmh_atom(ATOM_WM_PROTOCOLS)) {
        ewmh_update_protocols(wm, c);
    }
}

static void
handle_expose(WM *wm, XExposeEvent *ev)
{
    Client *c;

    if (ev->count != 0)
        return;

    if (bar_owns_window(wm, ev->window)) {
        bar_draw(wm);
        return;
    }
    c = client_from_frame(wm, ev->window);
    if (c != NULL)
        client_draw_frame(wm, c);
}

static void
handle_client_message(WM *wm, XClientMessageEvent *ev)
{
    Client *c = client_from_window(wm, ev->window);

    if (ev->message_type == ewmh_atom(ATOM_NET_WM_STATE)) {
        Atom a1 = (Atom)ev->data.l[1];
        Atom a2 = (Atom)ev->data.l[2];

        if (c == NULL)
            return;
        if (a1 == ewmh_atom(ATOM_NET_WM_STATE_FULLSCREEN) ||
            a2 == ewmh_atom(ATOM_NET_WM_STATE_FULLSCREEN)) {
            long action = ev->data.l[0];
            bool want = action == NET_WM_STATE_ADD ||
                        (action == NET_WM_STATE_TOGGLE &&
                         !c->is_fullscreen);
            client_set_fullscreen(wm, c, want);
        }
        return;
    }

    if (ev->message_type == ewmh_atom(ATOM_NET_ACTIVE_WINDOW)) {
        if (c == NULL)
            return;
        if (c->workspace != wm->current_ws)
            workspace_switch(wm, c->workspace);
        client_raise(wm, c);
        client_focus(wm, c);
        return;
    }

    if (ev->message_type == ewmh_atom(ATOM_NET_CURRENT_DESKTOP)) {
        workspace_switch(wm, (int)ev->data.l[0]);
        return;
    }
}

static void
handle_mapping_notify(WM *wm, XMappingEvent *ev)
{
    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard ||
        ev->request == MappingModifier) {
        log_info("mapping changed: regrabbing keys");
        keys_grab(wm);
    }
}

static void
handle_focus_in(WM *wm, XFocusChangeEvent *ev)
{
    if (wm->focused != NULL && ev->window != wm->focused->win &&
        client_from_window(wm, ev->window) != NULL) {
        XSetInputFocus(wm->dpy, wm->focused->win, RevertToPointerRoot,
                       CurrentTime);
    }
}

void
events_dispatch(WM *wm, XEvent *ev)
{
    switch (ev->type) {
    case MapRequest:
        handle_map_request(wm, &ev->xmaprequest);
        break;
    case UnmapNotify:
        handle_unmap_notify(wm, &ev->xunmap);
        break;
    case DestroyNotify:
        handle_destroy_notify(wm, &ev->xdestroywindow);
        break;
    case ConfigureRequest:
        handle_configure_request(wm, &ev->xconfigurerequest);
        break;
    case ButtonPress:
        handle_button_press(wm, &ev->xbutton);
        break;
    case ButtonRelease:
        handle_button_release(wm, &ev->xbutton);
        break;
    case MotionNotify:
        handle_motion(wm, &ev->xmotion);
        break;
    case EnterNotify:
        handle_enter_notify(wm, &ev->xcrossing);
        break;
    case KeyPress:
        keys_handle_keypress(wm, &ev->xkey);
        break;
    case PropertyNotify:
        handle_property_notify(wm, &ev->xproperty);
        break;
    case Expose:
        handle_expose(wm, &ev->xexpose);
        break;
    case ClientMessage:
        handle_client_message(wm, &ev->xclient);
        break;
    case MappingNotify:
        handle_mapping_notify(wm, &ev->xmapping);
        break;
    case FocusIn:
        handle_focus_in(wm, &ev->xfocus);
        break;
    default:
        break;
    }
}
