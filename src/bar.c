/* bar.c - built-in status bar rendering
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <X11/Xlib.h>

#include "bar.h"
#include "workspace.h"
#include "log.h"
#include "util.h"

#define WS_LABEL_PAD 8

#define TITLE_GAP 12

static int
text_width(const Bar *bar, const char *s)
{
    if (bar->font == NULL)
        return (int)strlen(s) * 7;
    return XTextWidth(bar->font, s, (int)strlen(s));
}

static int
label_width(const Bar *bar, const char *s)
{
    return text_width(bar, s) + 2 * WS_LABEL_PAD;
}

void
bar_init(WM *wm)
{
    XSetWindowAttributes attrs;
    Bar *bar = &wm->bar;

    memset(bar, 0, sizeof(*bar));

    if (!wm->config.show_bar) {
        log_info("bar: disabled by configuration");
        return;
    }

    bar->font = XLoadQueryFont(wm->dpy, wm->config.font_name);
    if (bar->font == NULL) {
        log_warn("bar: font '%s' not found, trying 'fixed'",
                 wm->config.font_name);
        bar->font = XLoadQueryFont(wm->dpy, "fixed");
    }

    bar->height = bar->font != NULL
                ? bar->font->ascent + bar->font->descent + 8
                : 24;

    attrs.override_redirect = True;
    attrs.background_pixel  = wm->px_bar_bg;
    attrs.event_mask        = ExposureMask | ButtonPressMask;

    bar->window = XCreateWindow(wm->dpy, wm->root,
                                0, 0,
                                (unsigned)wm->screen_w,
                                (unsigned)bar->height,
                                0, CopyFromParent, InputOutput,
                                CopyFromParent,
                                CWOverrideRedirect | CWBackPixel |
                                CWEventMask,
                                &attrs);

    bar->gc = XCreateGC(wm->dpy, bar->window, 0, NULL);
    if (bar->font != NULL)
        XSetFont(wm->dpy, bar->gc, bar->font->fid);

    XMapRaised(wm->dpy, bar->window);
    bar->mapped = true;

    log_info("bar: created, height=%d", bar->height);
    bar_draw(wm);
}

void
bar_shutdown(WM *wm)
{
    Bar *bar = &wm->bar;

    if (bar->gc != NULL) {
        XFreeGC(wm->dpy, bar->gc);
        bar->gc = NULL;
    }
    if (bar->font != NULL) {
        XFreeFont(wm->dpy, bar->font);
        bar->font = NULL;
    }
    if (bar->window != None) {
        XDestroyWindow(wm->dpy, bar->window);
        bar->window = None;
    }
    bar->mapped = false;
}

bool
bar_owns_window(const WM *wm, Window win)
{
    return wm->bar.mapped && win == wm->bar.window;
}

static int
draw_ws_label(WM *wm, int index, int x)
{
    Bar *bar = &wm->bar;
    char label[48];
    int w, baseline;
    bool active = index == wm->current_ws;
    bool urgent = workspace_has_urgent(wm, index);
    int occupied = workspace_client_count(wm, index);

    if (!active && occupied == 0 && !urgent)
        return x;

    snprintf(label, sizeof(label), "%s", wm->workspaces[index].name);
    w = label_width(bar, label);

    if (active) {
        XSetForeground(wm->dpy, bar->gc, wm->px_bar_ws_active);
        XFillRectangle(wm->dpy, bar->window, bar->gc,
                       x, 0, (unsigned)w, (unsigned)bar->height);
    } else if (urgent) {
        XSetForeground(wm->dpy, bar->gc, wm->px_border_urgent);
        XFillRectangle(wm->dpy, bar->window, bar->gc,
                       x, 0, (unsigned)w, (unsigned)bar->height);
    }

    baseline = bar->font != NULL
             ? (bar->height + bar->font->ascent - bar->font->descent) / 2
             : bar->height - 6;

    XSetForeground(wm->dpy, bar->gc,
                   active || urgent ? wm->px_title_text : wm->px_bar_fg);
    XDrawString(wm->dpy, bar->window, bar->gc,
                x + WS_LABEL_PAD, baseline,
                label, (int)strlen(label));

    return x + w;
}

void
bar_draw(WM *wm)
{
    Bar *bar = &wm->bar;
    int x = 0, i, baseline, clock_w;
    char clock_buf[64];
    time_t now;
    struct tm tm;

    if (!bar->mapped)
        return;

    XSetForeground(wm->dpy, bar->gc, wm->px_bar_bg);
    XFillRectangle(wm->dpy, bar->window, bar->gc, 0, 0,
                   (unsigned)wm->screen_w, (unsigned)bar->height);

    baseline = bar->font != NULL
             ? (bar->height + bar->font->ascent - bar->font->descent) / 2
             : bar->height - 6;

    for (i = 0; i < wm->config.workspace_count; i++)
        x = draw_ws_label(wm, i, x);

    now = time(NULL);
    localtime_r(&now, &tm);
    strftime(clock_buf, sizeof(clock_buf), "%a %d %b  %H:%M", &tm);
    clock_w = text_width(bar, clock_buf);
    XSetForeground(wm->dpy, bar->gc, wm->px_bar_fg);
    XDrawString(wm->dpy, bar->window, bar->gc,
                wm->screen_w - clock_w - WS_LABEL_PAD, baseline,
                clock_buf, (int)strlen(clock_buf));

    if (wm->focused != NULL) {
        char title[FLOWM_TITLE_MAX];
        int avail = wm->screen_w - clock_w - 2 * WS_LABEL_PAD
                  - x - 2 * TITLE_GAP;
        int len;

        xstrlcpy(title, wm->focused->title, sizeof(title));
        len = (int)strlen(title);
        if (bar->font != NULL) {
            while (len > 0 &&
                   XTextWidth(bar->font, title, len) > avail)
                len--;
        }
        if (len > 0) {
            XSetForeground(wm->dpy, bar->gc, wm->px_bar_fg);
            XDrawString(wm->dpy, bar->window, bar->gc,
                        x + TITLE_GAP, baseline, title, len);
        }
    }
}

void
bar_handle_click(WM *wm, int click_x, int click_y)
{
    Bar *bar = &wm->bar;
    int x = 0, i;

    UNUSED(click_y);

    if (!bar->mapped)
        return;

    for (i = 0; i < wm->config.workspace_count; i++) {
        bool active = i == wm->current_ws;
        bool urgent = workspace_has_urgent(wm, i);
        int occupied = workspace_client_count(wm, i);
        int w;

        if (!active && occupied == 0 && !urgent)
            continue;
        w = label_width(bar, wm->workspaces[i].name);
        if (click_x >= x && click_x < x + w) {
            workspace_switch(wm, i);
            return;
        }
        x += w;
    }
}

void
bar_tick(WM *wm)
{
    bar_draw(wm);
}
