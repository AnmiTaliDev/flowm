/* panelctl.c - live control of flowm's panels via _FLOWM_CMD
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

static void
send_command(Display *dpy, Window root, const char *text)
{
    Atom cmd = XInternAtom(dpy, "_FLOWM_CMD", False);

    XChangeProperty(dpy, root, cmd, XA_STRING, 8, PropModeReplace,
                    (const unsigned char *)text, (int)strlen(text));
    XFlush(dpy);
}

static void
set_options(Display *dpy, Window root, const char *label,
           const char *const *pairs, size_t npairs)
{
    char buf[4096];
    size_t off = 0, i;

    buf[0] = '\0';
    for (i = 0; i < npairs; i++)
        off += (size_t)snprintf(buf + off, sizeof(buf) - off,
                                "%s\n", pairs[i]);

    printf("-> %s\n", label);
    for (i = 0; i < npairs; i++)
        printf("   %s\n", pairs[i]);

    send_command(dpy, root, buf);
}

static int
run_demo(Display *dpy, Window root)
{
    static const char *const bar_only[] = {
        "bar_bg=#202030",
        "bar_fg=#c0c0ff",
        "bar_ws_active=#7040c0",
    };
    static const char *const decor_only[] = {
        "border_focused=#e0a030",
        "border_unfocused=#402c10",
        "titlebar_focused=#805010",
        "titlebar_unfocused=#2a2015",
        "title_text=#ffe8c0",
    };
    static const char *const both[] = {
        "bar_bg=#0f1a12",
        "bar_fg=#a0ffb0",
        "bar_ws_active=#30a050",
        "border_focused=#30a050",
        "border_unfocused=#183820",
        "titlebar_focused=#1f6030",
        "titlebar_unfocused=#132518",
        "title_text=#d0ffd8",
    };
    static const char *const restore[] = {
        "bar_bg=#1c1c1c",
        "bar_fg=#d0d0d0",
        "bar_ws_active=#4c7899",
        "border_focused=#4c7899",
        "border_unfocused=#333333",
        "titlebar_focused=#4c7899",
        "titlebar_unfocused=#2a2a2a",
        "title_text=#ffffff",
    };

    set_options(dpy, root, "step 1/3: bar panel only",
               bar_only, sizeof(bar_only) / sizeof(bar_only[0]));
    sleep(2);

    set_options(dpy, root, "step 2/3: window decorations only",
               decor_only, sizeof(decor_only) / sizeof(decor_only[0]));
    sleep(2);

    set_options(dpy, root, "step 3/3: bar + decorations together",
               both, sizeof(both) / sizeof(both[0]));
    sleep(2);

    set_options(dpy, root, "restoring defaults",
               restore, sizeof(restore) / sizeof(restore[0]));

    return EXIT_SUCCESS;
}

int
main(int argc, char **argv)
{
    Display *dpy;
    Window root;
    int status;

    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        fprintf(stderr, "panelctl: cannot open display '%s'\n",
                getenv("DISPLAY") != NULL ? getenv("DISPLAY") : "");
        return EXIT_FAILURE;
    }
    root = DefaultRootWindow(dpy);

    if (argc > 1) {
        char buf[4096];
        size_t off = 0;
        int i;

        buf[0] = '\0';
        for (i = 1; i < argc; i++)
            off += (size_t)snprintf(buf + off, sizeof(buf) - off,
                                    "%s\n", argv[i]);
        send_command(dpy, root, buf);
        printf("sent %d option(s) to flowm\n", argc - 1);
        status = EXIT_SUCCESS;
    } else {
        status = run_demo(dpy, root);
    }

    XCloseDisplay(dpy);
    return status;
}
