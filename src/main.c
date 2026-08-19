/* main.c - entry point and command-line handling for flowm
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wm.h"
#include "config.h"
#include "log.h"

#define FLOWM_VERSION "1.0.0"

static void
print_usage(FILE *out)
{
    fprintf(out,
        "usage: flowm [options]\n"
        "\n"
        "options:\n"
        "  -c FILE   read configuration from FILE instead of\n"
        "            $XDG_CONFIG_HOME/flowm/flowm.conf\n"
        "  -d        enable debug logging to stderr\n"
        "  -v        print version and exit\n"
        "  -h        show this help and exit\n"
        "\n"
        "flowm is a floating, reparenting window manager for X11.\n"
        "See flowm(1) and the shipped flowm.conf for configuration.\n");
}

static void
print_version(void)
{
    printf("flowm %s\n", FLOWM_VERSION);
}

int
main(int argc, char **argv)
{
    const char *config_path = NULL;
    bool debug = false;
    int opt;
    WM wm;

    while ((opt = getopt(argc, argv, "c:dvh")) != -1) {
        switch (opt) {
        case 'c':
            config_path = optarg;
            break;
        case 'd':
            debug = true;
            break;
        case 'v':
            print_version();
            return EXIT_SUCCESS;
        case 'h':
            print_usage(stdout);
            return EXIT_SUCCESS;
        default:
            print_usage(stderr);
            return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "flowm: unexpected argument '%s'\n",
                argv[optind]);
        print_usage(stderr);
        return EXIT_FAILURE;
    }

    log_init(NULL, debug ? LOG_DEBUG : LOG_INFO, true);

    if (!wm_init(&wm, config_path)) {
        log_shutdown();
        return EXIT_FAILURE;
    }
    if (debug)
        log_set_level(LOG_DEBUG);

    wm_scan_existing(&wm);
    wm_run(&wm);
    wm_shutdown(&wm);

    log_info("bye");
    log_shutdown();
    return EXIT_SUCCESS;
}
