/* wm.c - window manager lifecycle: connection, error handling, loop
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/cursorfont.h>

#include "wm.h"
#include "client.h"
#include "config.h"
#include "events.h"
#include "ewmh.h"
#include "keys.h"
#include "bar.h"
#include "workspace.h"
#include "log.h"
#include "util.h"

static WM *error_handler_wm = NULL;
static bool other_wm_detected = false;

static int
on_wm_detected(Display *dpy, XErrorEvent *ev)
{
    UNUSED(dpy);
    if (ev->error_code == BadAccess)
        other_wm_detected = true;
    return 0;
}

static int
on_x_error(Display *dpy, XErrorEvent *ev)
{
    char text[256];

    if (ev->error_code == BadWindow ||
        (ev->request_code == X_SetInputFocus &&
         ev->error_code == BadMatch) ||
        (ev->request_code == X_ConfigureWindow &&
         ev->error_code == BadMatch) ||
        (ev->request_code == X_GrabButton &&
         ev->error_code == BadAccess) ||
        (ev->request_code == X_CopyArea &&
         ev->error_code == BadDrawable) ||
        (ev->request_code == X_PolyFillRectangle &&
         ev->error_code == BadDrawable) ||
        (ev->request_code == X_PolySegment &&
         ev->error_code == BadDrawable)) {
        log_debug("x11: benign error req=%d err=%d res=0x%lx",
                  ev->request_code, ev->error_code, ev->resourceid);
        return 0;
    }

    XGetErrorText(dpy, ev->error_code, text, sizeof(text));
    log_error("x11: error: %s (request=%d, resource=0x%lx)",
              text, ev->request_code, ev->resourceid);
    return 0;
}

static int
on_x_io_error(Display *dpy)
{
    UNUSED(dpy);
    log_fatal("x11: connection to display lost");
    return 0;
}

static volatile sig_atomic_t got_signal = 0;

static void
signal_handler(int signum)
{
    got_signal = signum;
}

static void
install_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa, NULL);
}

unsigned long
wm_alloc_pixel(WM *wm, unsigned long rgb)
{
    XColor color;
    Colormap cmap = DefaultColormap(wm->dpy, wm->screen);

    color.red   = (unsigned short)(((rgb >> 16) & 0xff) * 257);
    color.green = (unsigned short)(((rgb >> 8) & 0xff) * 257);
    color.blue  = (unsigned short)((rgb & 0xff) * 257);
    color.flags = DoRed | DoGreen | DoBlue;

    if (XAllocColor(wm->dpy, cmap, &color) == 0) {
        log_warn("color: cannot allocate #%06lx, using black", rgb);
        return BlackPixel(wm->dpy, wm->screen);
    }
    return color.pixel;
}

static void
alloc_all_pixels(WM *wm)
{
    const Config *cfg = &wm->config;

    wm->px_border_focused    = wm_alloc_pixel(wm, cfg->color_border_focused);
    wm->px_border_unfocused  = wm_alloc_pixel(wm, cfg->color_border_unfocused);
    wm->px_border_urgent     = wm_alloc_pixel(wm, cfg->color_border_urgent);
    wm->px_titlebar_focused  = wm_alloc_pixel(wm, cfg->color_titlebar_focused);
    wm->px_titlebar_unfocused = wm_alloc_pixel(wm, cfg->color_titlebar_unfocused);
    wm->px_title_text        = wm_alloc_pixel(wm, cfg->color_title_text);
    wm->px_bar_bg            = wm_alloc_pixel(wm, cfg->color_bar_bg);
    wm->px_bar_fg            = wm_alloc_pixel(wm, cfg->color_bar_fg);
    wm->px_bar_ws_active     = wm_alloc_pixel(wm, cfg->color_bar_ws_active);
}

void
wm_usable_area(const WM *wm, int *x, int *y, int *w, int *h)
{
    int bar_h = wm->bar.mapped ? wm->bar.height : 0;

    *x = 0;
    *y = bar_h;
    *w = wm->screen_w;
    *h = wm->screen_h - bar_h;
}

bool
wm_init(WM *wm, const char *config_path)
{
    XSetWindowAttributes root_attrs;

    memset(wm, 0, sizeof(*wm));

    config_defaults(&wm->config);
    if (!config_load(&wm->config, config_path))
        return false;

    if (wm->config.log_path[0] != '\0')
        log_init(wm->config.log_path, wm->config.log_level, true);
    log_set_level(wm->config.log_level);

    wm->dpy = XOpenDisplay(NULL);
    if (wm->dpy == NULL) {
        log_error("cannot open display '%s'",
                  getenv("DISPLAY") != NULL ? getenv("DISPLAY") : "");
        return false;
    }

    wm->screen   = DefaultScreen(wm->dpy);
    wm->root     = RootWindow(wm->dpy, wm->screen);
    wm->screen_w = DisplayWidth(wm->dpy, wm->screen);
    wm->screen_h = DisplayHeight(wm->dpy, wm->screen);

    log_info("flowm starting on %s, root=0x%lx, %dx%d",
             DisplayString(wm->dpy), wm->root,
             wm->screen_w, wm->screen_h);

    other_wm_detected = false;
    XSetErrorHandler(on_wm_detected);
    root_attrs.event_mask = SubstructureRedirectMask |
                            SubstructureNotifyMask |
                            ButtonPressMask |
                            PropertyChangeMask;
    XChangeWindowAttributes(wm->dpy, wm->root, CWEventMask, &root_attrs);
    XSync(wm->dpy, False);

    if (other_wm_detected) {
        log_error("another window manager is already running");
        XCloseDisplay(wm->dpy);
        return false;
    }

    error_handler_wm = wm;
    XSetErrorHandler(on_x_error);
    XSetIOErrorHandler(on_x_io_error);

    install_signal_handlers();

    alloc_all_pixels(wm);

    wm->cursor_normal = XCreateFontCursor(wm->dpy, XC_left_ptr);
    wm->cursor_move   = XCreateFontCursor(wm->dpy, XC_fleur);
    wm->cursor_resize = XCreateFontCursor(wm->dpy,
                                          XC_bottom_right_corner);
    XDefineCursor(wm->dpy, wm->root, wm->cursor_normal);

    wm->frame_font = XLoadQueryFont(wm->dpy, wm->config.font_name);
    if (wm->frame_font == NULL)
        wm->frame_font = XLoadQueryFont(wm->dpy, "fixed");
    wm->frame_gc = XCreateGC(wm->dpy, wm->root, 0, NULL);
    if (wm->frame_font != NULL)
        XSetFont(wm->dpy, wm->frame_gc, wm->frame_font->fid);

    ewmh_init_atoms(wm);
    workspace_init(wm);
    bar_init(wm);
    ewmh_setup(wm);
    keys_grab(wm);

    wm->running = 1;
    return true;
}

void
wm_scan_existing(WM *wm)
{
    Window root_ret, parent_ret;
    Window *children = NULL;
    unsigned int count = 0, i;
    int adopted = 0;

    XGrabServer(wm->dpy);

    if (XQueryTree(wm->dpy, wm->root, &root_ret, &parent_ret,
                   &children, &count) != 0) {
        for (i = 0; i < count; i++) {
            XWindowAttributes attrs;

            if (XGetWindowAttributes(wm->dpy, children[i], &attrs) == 0)
                continue;
            if (attrs.override_redirect ||
                attrs.map_state != IsViewable)
                continue;
            if (bar_owns_window(wm, children[i]) ||
                children[i] == wm->check_win)
                continue;
            if (client_manage(wm, children[i], true) != NULL)
                adopted++;
        }
        if (children != NULL)
            XFree(children);
    }

    XUngrabServer(wm->dpy);

    if (adopted > 0)
        log_info("scan: adopted %d existing window(s)", adopted);
}

void
wm_run(WM *wm)
{
    int xfd = ConnectionNumber(wm->dpy);

    log_info("entering event loop");

    while (wm->running) {
        fd_set fds;
        struct timeval tv;
        int ready;

        if (got_signal != 0) {
            log_info("received signal %d, shutting down",
                     (int)got_signal);
            break;
        }

        while (XPending(wm->dpy) > 0) {
            XEvent ev;
            XNextEvent(wm->dpy, &ev);
            events_dispatch(wm, &ev);
            if (!wm->running)
                return;
        }

        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        ready = select(xfd + 1, &fds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            log_error("select: %s", strerror(errno));
            break;
        }
        if (ready == 0)
            bar_tick(wm);
    }
}

void
wm_quit(WM *wm)
{
    wm->running = 0;
}

void
wm_shutdown(WM *wm)
{
    log_info("shutting down");

    while (wm->clients != NULL)
        client_unmanage(wm, wm->clients, false);

    keys_ungrab(wm);
    bar_shutdown(wm);
    ewmh_teardown(wm);

    if (wm->frame_gc != NULL)
        XFreeGC(wm->dpy, wm->frame_gc);
    if (wm->frame_font != NULL)
        XFreeFont(wm->dpy, wm->frame_font);

    XFreeCursor(wm->dpy, wm->cursor_normal);
    XFreeCursor(wm->dpy, wm->cursor_move);
    XFreeCursor(wm->dpy, wm->cursor_resize);

    XSetInputFocus(wm->dpy, PointerRoot, RevertToPointerRoot,
                   CurrentTime);
    XSync(wm->dpy, False);
    XCloseDisplay(wm->dpy);

    config_free(&wm->config);
    error_handler_wm = NULL;
}
