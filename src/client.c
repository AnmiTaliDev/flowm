/* client.c - managed client lifecycle, frames, focus and geometry
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "client.h"
#include "rules.h"
#include "ewmh.h"
#include "bar.h"
#include "log.h"
#include "util.h"

#define CLIENT_EVENT_MASK (PropertyChangeMask | StructureNotifyMask | \
                           FocusChangeMask)

#define FRAME_EVENT_MASK (SubstructureRedirectMask |                  \
                          SubstructureNotifyMask | ExposureMask |     \
                          ButtonPressMask | ButtonReleaseMask |       \
                          PointerMotionMask | EnterWindowMask)

int
client_titlebar_height(const WM *wm, const Client *c)
{
    if (c != NULL && c->is_fullscreen)
        return 0;
    return wm->config.titlebar_height;
}

int
client_close_button_width(const WM *wm)
{
    return wm->config.titlebar_height;
}

Client *
client_from_window(WM *wm, Window win)
{
    Client *c;

    for (c = wm->clients; c != NULL; c = c->next)
        if (c->win == win)
            return c;
    return NULL;
}

Client *
client_from_frame(WM *wm, Window frame)
{
    Client *c;

    for (c = wm->clients; c != NULL; c = c->next)
        if (c->frame == frame)
            return c;
    return NULL;
}

static void
frame_geometry(const WM *wm, const Client *c,
               int *fx, int *fy, int *fw, int *fh)
{
    int th = client_titlebar_height(wm, c);
    int bw = c->is_fullscreen ? 0 : wm->config.border_width;

    UNUSED(bw);

    *fx = c->x;
    *fy = c->y - th;
    *fw = c->w;
    *fh = c->h + th;
}

void
client_update_size_hints(WM *wm, Client *c)
{
    XSizeHints hints;
    long supplied = 0;

    c->base_w = c->base_h = 0;
    c->min_w = c->min_h = 1;
    c->max_w = c->max_h = 0;
    c->inc_w = c->inc_h = 0;
    c->min_aspect = 0.0f;
    c->max_aspect = 0.0f;
    c->is_fixed = false;

    if (XGetWMNormalHints(wm->dpy, c->win, &hints, &supplied) == 0)
        return;

    if (hints.flags & PBaseSize) {
        c->base_w = hints.base_width;
        c->base_h = hints.base_height;
    } else if (hints.flags & PMinSize) {
        c->base_w = hints.min_width;
        c->base_h = hints.min_height;
    }

    if (hints.flags & PMinSize) {
        c->min_w = MAX(1, hints.min_width);
        c->min_h = MAX(1, hints.min_height);
    } else if (hints.flags & PBaseSize) {
        c->min_w = MAX(1, hints.base_width);
        c->min_h = MAX(1, hints.base_height);
    }

    if (hints.flags & PMaxSize) {
        c->max_w = hints.max_width;
        c->max_h = hints.max_height;
    }

    if (hints.flags & PResizeInc) {
        c->inc_w = hints.width_inc;
        c->inc_h = hints.height_inc;
    }

    if (hints.flags & PAspect) {
        if (hints.min_aspect.y > 0)
            c->min_aspect = (float)hints.min_aspect.x
                          / (float)hints.min_aspect.y;
        if (hints.max_aspect.y > 0)
            c->max_aspect = (float)hints.max_aspect.x
                          / (float)hints.max_aspect.y;
    }

    c->is_fixed = c->max_w > 0 && c->max_h > 0 &&
                  c->max_w == c->min_w && c->max_h == c->min_h;
}

void
client_apply_size_hints(const Client *c, int *w, int *h)
{
    int width = *w, height = *h;

    width  = MAX(width, c->min_w);
    height = MAX(height, c->min_h);
    if (c->max_w > 0)
        width = MIN(width, c->max_w);
    if (c->max_h > 0)
        height = MIN(height, c->max_h);

    if (c->min_aspect > 0.0f || c->max_aspect > 0.0f) {
        int aw = width - c->base_w;
        int ah = height - c->base_h;

        if (aw > 0 && ah > 0) {
            float aspect = (float)aw / (float)ah;
            if (c->max_aspect > 0.0f && aspect > c->max_aspect)
                aw = (int)((float)ah * c->max_aspect + 0.5f);
            else if (c->min_aspect > 0.0f && aspect < c->min_aspect)
                ah = (int)((float)aw / c->min_aspect + 0.5f);
            width  = aw + c->base_w;
            height = ah + c->base_h;
        }
    }

    if (c->inc_w > 0)
        width -= (width - c->base_w) % c->inc_w;
    if (c->inc_h > 0)
        height -= (height - c->base_h) % c->inc_h;

    *w = MAX(1, width);
    *h = MAX(1, height);
}

void
client_update_wm_hints(WM *wm, Client *c)
{
    XWMHints *hints = XGetWMHints(wm->dpy, c->win);
    bool urgent = false;

    c->never_focus = false;

    if (hints != NULL) {
        if ((hints->flags & InputHint) && hints->input == False)
            c->never_focus = true;
        if (hints->flags & XUrgencyHint)
            urgent = true;
        XFree(hints);
    }

    if (c->takes_wm_focus)
        c->never_focus = false;

    client_set_urgent(wm, c, urgent);
}

void
client_set_urgent(WM *wm, Client *c, bool urgent)
{
    if (urgent == c->is_urgent)
        return;

    c->is_urgent = urgent;
    client_update_decor_colors(wm, c);
    bar_draw(wm);
}

void
client_update_title(WM *wm, Client *c)
{
    ewmh_fetch_title(wm, c->win, c->title, sizeof(c->title));
    client_draw_frame(wm, c);
    if (c == wm->focused)
        bar_draw(wm);
}

void
client_update_decor_colors(WM *wm, Client *c)
{
    unsigned long border;

    if (c->is_urgent)
        border = wm->px_border_urgent;
    else if (c == wm->focused)
        border = wm->px_border_focused;
    else
        border = wm->px_border_unfocused;

    XSetWindowBorder(wm->dpy, c->frame, border);
    client_draw_frame(wm, c);
}

void
client_draw_frame(WM *wm, Client *c)
{
    int th = client_titlebar_height(wm, c);
    unsigned long bg, fg;
    int btn_w, text_x, text_y, baseline;
    char label[FLOWM_TITLE_MAX];

    if (th <= 0)
        return;

    bg = c == wm->focused ? wm->px_titlebar_focused
                          : wm->px_titlebar_unfocused;
    fg = wm->px_title_text;

    XSetForeground(wm->dpy, wm->frame_gc, bg);
    XFillRectangle(wm->dpy, c->frame, wm->frame_gc,
                   0, 0, (unsigned)c->w, (unsigned)th);

    btn_w = client_close_button_width(wm);
    text_x = 6;
    baseline = wm->frame_font != NULL
             ? (th + wm->frame_font->ascent - wm->frame_font->descent) / 2
             : th - 6;
    text_y = baseline;

    xstrlcpy(label, c->title, sizeof(label));

    if (wm->frame_font != NULL) {
        int avail = c->w - btn_w - text_x - 4;
        int len = (int)strlen(label);

        while (len > 0 &&
               XTextWidth(wm->frame_font, label, len) > avail)
            len--;
        if (len < (int)strlen(label) && len >= 1)
            label[len > 1 ? len - 1 : len] = '\0';
        else
            label[len] = '\0';

        XSetForeground(wm->dpy, wm->frame_gc, fg);
        XDrawString(wm->dpy, c->frame, wm->frame_gc, text_x, text_y,
                    label, (int)strlen(label));
    }

    {
        int pad = th / 3;
        int x0 = c->w - btn_w + pad;
        int y0 = pad;
        int x1 = c->w - pad;
        int y1 = th - pad;

        XSetForeground(wm->dpy, wm->frame_gc, fg);
        XDrawLine(wm->dpy, c->frame, wm->frame_gc, x0, y0, x1, y1);
        XDrawLine(wm->dpy, c->frame, wm->frame_gc, x0, y1, x1, y0);
    }
}

static void
send_synthetic_configure(WM *wm, Client *c)
{
    XConfigureEvent ce;

    memset(&ce, 0, sizeof(ce));
    ce.type              = ConfigureNotify;
    ce.display           = wm->dpy;
    ce.event             = c->win;
    ce.window            = c->win;
    ce.x                 = c->x;
    ce.y                 = c->y;
    ce.width             = c->w;
    ce.height            = c->h;
    ce.border_width      = 0;
    ce.above             = None;
    ce.override_redirect = False;

    XSendEvent(wm->dpy, c->win, False, StructureNotifyMask,
               (XEvent *)&ce);
}

void
client_move_resize(WM *wm, Client *c, int x, int y, int w, int h,
                   bool honor_hints)
{
    int fx, fy, fw, fh;
    int th = client_titlebar_height(wm, c);
    bool moved, resized;

    if (honor_hints)
        client_apply_size_hints(c, &w, &h);

    w = MAX(1, w);
    h = MAX(1, h);

    moved   = x != c->x || y != c->y;
    resized = w != c->w || h != c->h;

    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;

    frame_geometry(wm, c, &fx, &fy, &fw, &fh);
    XMoveResizeWindow(wm->dpy, c->frame, fx, fy,
                      (unsigned)fw, (unsigned)fh);
    XMoveResizeWindow(wm->dpy, c->win, 0, th,
                      (unsigned)w, (unsigned)h);

    if (moved && !resized)
        send_synthetic_configure(wm, c);
    if (resized)
        client_draw_frame(wm, c);
}

void
client_snap_position(const WM *wm, const Client *c, int *x, int *y)
{
    int snap = wm->config.snap_distance;
    int ux, uy, uw, uh;
    int th = client_titlebar_height(wm, c);
    int bw = wm->config.border_width;
    int fw = c->w + 2 * bw;
    int fh = c->h + th + 2 * bw;

    if (snap <= 0)
        return;

    wm_usable_area(wm, &ux, &uy, &uw, &uh);

    if (abs(*x - bw - ux) < snap)
        *x = ux + bw;
    if (abs((*x - bw) + fw - (ux + uw)) < snap)
        *x = ux + uw - fw + bw;
    if (abs((*y - th - bw) - uy) < snap)
        *y = uy + th + bw;
    if (abs((*y - th - bw) + fh - (uy + uh)) < snap)
        *y = uy + uh - fh + th + bw;
}

void
client_center(WM *wm, Client *c)
{
    int ux, uy, uw, uh;
    int th = client_titlebar_height(wm, c);

    wm_usable_area(wm, &ux, &uy, &uw, &uh);
    client_move_resize(wm, c,
                       ux + (uw - c->w) / 2,
                       uy + th + (uh - th - c->h) / 2,
                       c->w, c->h, false);
}

void
client_set_fullscreen(WM *wm, Client *c, bool fullscreen)
{
    if (fullscreen == c->is_fullscreen)
        return;

    if (fullscreen) {
        c->sx = c->x;
        c->sy = c->y;
        c->sw = c->w;
        c->sh = c->h;

        c->is_fullscreen = true;
        XSetWindowBorderWidth(wm->dpy, c->frame, 0);
        client_move_resize(wm, c, 0, 0,
                           wm->screen_w, wm->screen_h, false);
        client_raise(wm, c);
    } else {
        c->is_fullscreen = false;
        XSetWindowBorderWidth(wm->dpy, c->frame,
                              (unsigned)wm->config.border_width);
        client_move_resize(wm, c, c->sx, c->sy, c->sw, c->sh, true);
    }

    ewmh_set_fullscreen_state(wm, c, fullscreen);
    client_update_decor_colors(wm, c);
    log_debug("client 0x%lx fullscreen=%d", c->win, (int)fullscreen);
}

void
client_toggle_maximize(WM *wm, Client *c)
{
    if (c->is_fullscreen)
        return;

    if (!c->is_maximized) {
        int ux, uy, uw, uh;
        int th = client_titlebar_height(wm, c);
        int bw = wm->config.border_width;

        c->sx = c->x;
        c->sy = c->y;
        c->sw = c->w;
        c->sh = c->h;

        wm_usable_area(wm, &ux, &uy, &uw, &uh);
        c->is_maximized = true;
        client_move_resize(wm, c,
                           ux + bw, uy + th + bw,
                           uw - 2 * bw, uh - th - 2 * bw, false);
    } else {
        c->is_maximized = false;
        client_move_resize(wm, c, c->sx, c->sy, c->sw, c->sh, true);
    }
}

void
client_set_sticky(WM *wm, Client *c, bool sticky)
{
    if (sticky == c->is_sticky)
        return;

    c->is_sticky = sticky;
    if (sticky) {
        client_show(wm, c);
    } else {
        c->workspace = wm->current_ws;
    }

    ewmh_set_client_desktop(wm, c);
    ewmh_set_sticky_state(wm, c, sticky);
    bar_draw(wm);
    log_debug("client 0x%lx sticky=%d", c->win, (int)sticky);
}

void
client_focus(WM *wm, Client *c)
{
    Client *previous = wm->focused;

    if (c != NULL && ((c->workspace != wm->current_ws && !c->is_sticky) || c->never_focus))
        return;

    wm->focused = c;

    if (previous != NULL && previous != c)
        client_update_decor_colors(wm, previous);

    if (c != NULL) {
        XSetInputFocus(wm->dpy, c->win, RevertToPointerRoot,
                       CurrentTime);
        if (c->takes_wm_focus)
            ewmh_send_protocol(wm, c->win,
                               ewmh_atom(ATOM_WM_TAKE_FOCUS),
                               CurrentTime);
        wm->workspaces[wm->current_ws].focused = c;
        client_set_urgent(wm, c, false);
        client_update_decor_colors(wm, c);
    } else {
        XSetInputFocus(wm->dpy, wm->root, RevertToPointerRoot,
                       CurrentTime);
    }

    ewmh_update_active_window(wm);
    bar_draw(wm);
}

static Client *
next_on_workspace(WM *wm, Client *from)
{
    Client *c = from != NULL ? from->next : wm->clients;
    int guard = 0;

    while (guard++ < 2) {
        for (; c != NULL; c = c->next)
            if ((c->workspace == wm->current_ws || c->is_sticky) && !c->never_focus)
                return c;
        c = wm->clients;
    }
    return NULL;
}

static Client *
prev_on_workspace(WM *wm, Client *from)
{
    Client *c, *best = NULL;

    for (c = wm->clients; c != NULL && c != from; c = c->next)
        if ((c->workspace == wm->current_ws || c->is_sticky) && !c->never_focus)
            best = c;
    if (best != NULL)
        return best;

    for (c = wm->clients; c != NULL; c = c->next)
        if ((c->workspace == wm->current_ws || c->is_sticky) && !c->never_focus)
            best = c;
    return best;
}

void
client_focus_cycle(WM *wm, bool forward)
{
    Client *target;

    target = forward ? next_on_workspace(wm, wm->focused)
                     : prev_on_workspace(wm, wm->focused);
    if (target == NULL || target == wm->focused)
        return;

    client_raise(wm, target);
    client_focus(wm, target);
}

void
client_raise(WM *wm, Client *c)
{
    XRaiseWindow(wm->dpy, c->frame);
}

void
client_lower(WM *wm, Client *c)
{
    XLowerWindow(wm->dpy, c->frame);
}

void
client_show(WM *wm, Client *c)
{
    XMapWindow(wm->dpy, c->frame);
    ewmh_set_wm_state(wm, c, NormalState);
}

void
client_hide(WM *wm, Client *c)
{
    c->ignore_unmaps++;
    XUnmapWindow(wm->dpy, c->frame);
    ewmh_set_wm_state(wm, c, IconicState);
}

void
client_send_to_workspace(WM *wm, Client *c, int ws)
{
    if (ws < 0 || ws >= wm->config.workspace_count)
        return;
    if (ws == c->workspace && !c->is_sticky)
        return;

    if (c->is_sticky) {
        c->is_sticky = false;
        ewmh_set_sticky_state(wm, c, false);
    }

    c->workspace = ws;
    ewmh_set_client_desktop(wm, c);
    if (ws != wm->current_ws)
        client_hide(wm, c);

    if (wm->focused == c && ws != wm->current_ws)
        client_focus(wm, next_on_workspace(wm, NULL));

    if (wm->workspaces[ws].focused == NULL)
        wm->workspaces[ws].focused = c;
    bar_draw(wm);
    log_debug("client 0x%lx sent to workspace %d", c->win, ws + 1);
}

void
client_close(WM *wm, Client *c)
{
    if (c->deletes_window) {
        log_debug("client 0x%lx: sending WM_DELETE_WINDOW", c->win);
        ewmh_send_protocol(wm, c->win,
                           ewmh_atom(ATOM_WM_DELETE_WINDOW),
                           CurrentTime);
    } else {
        client_kill(wm, c);
    }
}

void
client_kill(WM *wm, Client *c)
{
    log_info("client 0x%lx: XKillClient", c->win);
    XGrabServer(wm->dpy);
    XKillClient(wm->dpy, c->win);
    XUngrabServer(wm->dpy);
}

static void
place_client(WM *wm, Client *c, const XWindowAttributes *attrs)
{
    XSizeHints hints;
    long supplied = 0;
    int ux, uy, uw, uh;
    int th = client_titlebar_height(wm, c);

    wm_usable_area(wm, &ux, &uy, &uw, &uh);

    if (XGetWMNormalHints(wm->dpy, c->win, &hints, &supplied) != 0 &&
        (hints.flags & (USPosition | PPosition)) &&
        (attrs->x != 0 || attrs->y != 0)) {
        c->x = attrs->x;
        c->y = attrs->y;
    } else if (wm->focused != NULL &&
               (wm->focused->workspace == wm->current_ws ||
                wm->focused->is_sticky)) {
        c->x = wm->focused->x + 32;
        c->y = wm->focused->y + 32;
    } else {
        c->x = ux + (uw - c->w) / 2;
        c->y = uy + th + (uh - th - c->h) / 2;
    }

    c->x = CLAMP(c->x, ux - c->w + 64, ux + uw - 64);
    c->y = CLAMP(c->y, uy + th, uy + uh - 16);
}

Client *
client_manage(WM *wm, Window win, bool existing)
{
    XWindowAttributes attrs;
    XSetWindowAttributes frame_attrs;
    Client *c;
    const Rule *rule;
    bool is_dialog = false;
    int th, fx, fy, fw, fh;

    if (XGetWindowAttributes(wm->dpy, win, &attrs) == 0) {
        log_debug("manage: window 0x%lx vanished", win);
        return NULL;
    }
    if (attrs.override_redirect) {
        log_debug("manage: 0x%lx is override-redirect, skipping", win);
        return NULL;
    }
    if (ewmh_is_unmanaged_type(wm, win, &is_dialog)) {
        log_info("manage: 0x%lx is a dock/desktop type, mapping raw",
                 win);
        XMapWindow(wm->dpy, win);
        return NULL;
    }
    if (client_from_window(wm, win) != NULL) {
        log_warn("manage: 0x%lx already managed", win);
        return NULL;
    }

    c = xcalloc(1, sizeof(*c));
    c->win       = win;
    c->workspace = wm->current_ws;
    c->w         = MAX(1, attrs.width);
    c->h         = MAX(1, attrs.height);
    c->x         = attrs.x;
    c->y         = attrs.y;

    ewmh_update_protocols(wm, c);
    client_update_size_hints(wm, c);
    client_update_wm_hints(wm, c);
    ewmh_fetch_title(wm, c->win, c->title, sizeof(c->title));

    if (!existing)
        place_client(wm, c, &attrs);

    rule = !existing
         ? rules_match(&wm->config.rules, wm->dpy, c->win)
         : NULL;
    if (rule != NULL) {
        if (rule->set_size) {
            c->w = rule->width;
            c->h = rule->height;
            client_apply_size_hints(c, &c->w, &c->h);
        }
        if (rule->set_position) {
            c->x = rule->x;
            c->y = rule->y;
        }
        if (rule->set_workspace &&
            rule->workspace < wm->config.workspace_count)
            c->workspace = rule->workspace;
        if (rule->center) {
            int ux, uy, uw, uh;

            wm_usable_area(wm, &ux, &uy, &uw, &uh);
            c->x = ux + (uw - c->w) / 2;
            c->y = uy + wm->config.titlebar_height
                 + (uh - wm->config.titlebar_height - c->h) / 2;
        }
    }

    th = client_titlebar_height(wm, c);

    frame_attrs.event_mask        = FRAME_EVENT_MASK;
    frame_attrs.background_pixel  = wm->px_titlebar_unfocused;
    frame_attrs.border_pixel      = wm->px_border_unfocused;
    frame_attrs.override_redirect = True;

    fx = c->x;
    fy = c->y - th;
    fw = c->w;
    fh = c->h + th;

    c->frame = XCreateWindow(wm->dpy, wm->root, fx, fy,
                             (unsigned)fw, (unsigned)fh,
                             (unsigned)wm->config.border_width,
                             CopyFromParent, InputOutput,
                             CopyFromParent,
                             CWEventMask | CWBackPixel | CWBorderPixel |
                             CWOverrideRedirect,
                             &frame_attrs);

    XAddToSaveSet(wm->dpy, win);
    XSelectInput(wm->dpy, win, CLIENT_EVENT_MASK);

    if (attrs.map_state == IsViewable)
        c->ignore_unmaps++;
    XReparentWindow(wm->dpy, win, c->frame, 0, th);
    XSetWindowBorderWidth(wm->dpy, win, 0);

    XGrabButton(wm->dpy, Button1, AnyModifier, win, False,
                ButtonPressMask, GrabModeSync, GrabModeSync,
                None, None);

    c->next     = wm->clients;
    wm->clients = c;

    XMapWindow(wm->dpy, win);
    XMapWindow(wm->dpy, c->frame);
    c->is_mapped = true;

    ewmh_set_wm_state(wm, c, NormalState);
    ewmh_set_client_desktop(wm, c);
    ewmh_update_client_list(wm);

    client_move_resize(wm, c, c->x, c->y, c->w, c->h, true);
    client_raise(wm, c);

    if (rule != NULL) {
        if (rule->sticky)
            client_set_sticky(wm, c, true);
        if (rule->maximize)
            client_toggle_maximize(wm, c);
        if (rule->fullscreen)
            client_set_fullscreen(wm, c, true);
    }

    if (c->workspace != wm->current_ws && !c->is_sticky) {
        client_hide(wm, c);
        ewmh_set_client_desktop(wm, c);
    } else {
        client_focus(wm, c);
    }
    client_draw_frame(wm, c);

    log_info("manage: 0x%lx \"%s\" %dx%d+%d+%d ws=%d dialog=%d",
             win, c->title, c->w, c->h, c->x, c->y,
             c->workspace + 1, (int)is_dialog);
    return c;
}

void
client_unmanage(WM *wm, Client *c, bool destroyed)
{
    Client **link;
    int i;

    log_info("unmanage: 0x%lx \"%s\" destroyed=%d",
             c->win, c->title, (int)destroyed);

    for (link = &wm->clients; *link != NULL; link = &(*link)->next) {
        if (*link == c) {
            *link = c->next;
            break;
        }
    }
    for (i = 0; i < wm->config.workspace_count; i++) {
        if (wm->workspaces[i].focused == c)
            wm->workspaces[i].focused = NULL;
    }

    if (!destroyed) {
        XGrabServer(wm->dpy);
        XSelectInput(wm->dpy, c->win, NoEventMask);
        XUngrabButton(wm->dpy, Button1, AnyModifier, c->win);
        XReparentWindow(wm->dpy, c->win, wm->root, c->x, c->y);
        XRemoveFromSaveSet(wm->dpy, c->win);
        ewmh_set_wm_state(wm, c, WithdrawnState);
        XUngrabServer(wm->dpy);
    }

    XDestroyWindow(wm->dpy, c->frame);

    if (wm->focused == c) {
        wm->focused = NULL;
        client_focus(wm, next_on_workspace(wm, NULL));
    }

    ewmh_update_client_list(wm);
    bar_draw(wm);
    free(c);
}
