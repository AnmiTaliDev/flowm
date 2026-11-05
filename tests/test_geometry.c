/* SPDX-License-Identifier: MIT */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#include "../src/geometry.h"

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        tests_run++;                                                   \
        if (!(cond)) {                                                 \
            tests_failed++;                                            \
            fprintf(stderr, "FAIL %s:%d: %s\n",                        \
                    __FILE__, __LINE__, #cond);                        \
        }                                                              \
    } while (0)

static void
test_snap_names(void)
{
    ClientSnap s = SNAP_NONE;

    CHECK(geometry_snap_from_name("snap_left", &s) && s == SNAP_LEFT);
    CHECK(geometry_snap_from_name("tile_left", &s) && s == SNAP_LEFT);
    CHECK(geometry_snap_from_name("SNAP_RIGHT", &s) && s == SNAP_RIGHT);
    CHECK(geometry_snap_from_name("tile_right", &s) && s == SNAP_RIGHT);
    CHECK(geometry_snap_from_name("snap_top", &s) && s == SNAP_TOP);
    CHECK(geometry_snap_from_name("snap_bottom", &s) && s == SNAP_BOTTOM);
    CHECK(geometry_snap_from_name("snap_top_left", &s) &&
          s == SNAP_TOP_LEFT);
    CHECK(geometry_snap_from_name("snap_top_right", &s) &&
          s == SNAP_TOP_RIGHT);
    CHECK(geometry_snap_from_name("snap_bottom_left", &s) &&
          s == SNAP_BOTTOM_LEFT);
    CHECK(geometry_snap_from_name("snap_bottom_right", &s) &&
          s == SNAP_BOTTOM_RIGHT);
    CHECK(!geometry_snap_from_name("snap", &s));
    CHECK(!geometry_snap_from_name("", &s));
}

static void
test_snap_rect_halves(void)
{
    int x, y, w, h;

    geometry_snap_rect(SNAP_LEFT, 0, 20, 1000, 580, 22, 2,
                       &x, &y, &w, &h);
    CHECK(x == 2 && y == 44 && w == 496 && h == 554);

    geometry_snap_rect(SNAP_RIGHT, 0, 20, 1000, 580, 22, 2,
                       &x, &y, &w, &h);
    CHECK(x == 502 && y == 44 && w == 496 && h == 554);

    geometry_snap_rect(SNAP_TOP, 0, 20, 1000, 580, 22, 2,
                       &x, &y, &w, &h);
    CHECK(x == 2 && y == 44 && w == 996 && h == 264);

    geometry_snap_rect(SNAP_BOTTOM, 0, 20, 1000, 580, 22, 2,
                       &x, &y, &w, &h);
    CHECK(x == 2 && y == 334 && w == 996 && h == 264);
}

static void
test_snap_rect_corners(void)
{
    int x, y, w, h;

    geometry_snap_rect(SNAP_TOP_LEFT, 0, 20, 1000, 580, 22, 2,
                       &x, &y, &w, &h);
    CHECK(x == 2 && y == 44 && w == 496 && h == 264);

    geometry_snap_rect(SNAP_TOP_RIGHT, 0, 20, 1000, 580, 22, 2,
                       &x, &y, &w, &h);
    CHECK(x == 502 && y == 44 && w == 496 && h == 264);

    geometry_snap_rect(SNAP_BOTTOM_LEFT, 0, 20, 1000, 580, 22, 2,
                       &x, &y, &w, &h);
    CHECK(x == 2 && y == 334 && w == 496 && h == 264);

    geometry_snap_rect(SNAP_BOTTOM_RIGHT, 0, 20, 1000, 580, 22, 2,
                       &x, &y, &w, &h);
    CHECK(x == 502 && y == 334 && w == 496 && h == 264);
}

static void
test_snap_rect_odd_sizes(void)
{
    int x, y, w, h;

    geometry_snap_rect(SNAP_RIGHT, 0, 0, 1001, 601, 0, 0,
                       &x, &y, &w, &h);
    CHECK(x == 500 && w == 501 && y == 0 && h == 601);

    geometry_snap_rect(SNAP_LEFT, 0, 0, 3, 3, 22, 4, &x, &y, &w, &h);
    CHECK(w >= 1 && h >= 1);
}

static void
test_snap_rect_offset_area(void)
{
    int x, y, w, h;

    geometry_snap_rect(SNAP_TOP_LEFT, 30, 50, 800, 400, 20, 1,
                       &x, &y, &w, &h);
    CHECK(x == 31 && y == 71 && w == 398 && h == 178);
}

static void
test_snap_edges(void)
{
    int x, y;

    x = 5; y = 50;
    geometry_snap_edges(12, 0, 20, 1000, 580, 22, 2, 300, 200, &x, &y);
    CHECK(x == 2);
    CHECK(y == 44);

    x = 500; y = 300;
    geometry_snap_edges(12, 0, 20, 1000, 580, 22, 2, 300, 200, &x, &y);
    CHECK(x == 500 && y == 300);

    x = 690; y = 390;
    geometry_snap_edges(12, 0, 20, 1000, 580, 22, 2, 300, 200, &x, &y);
    CHECK(x == 698);
    CHECK(y == 398);

    x = 5; y = 60;
    geometry_snap_edges(0, 0, 20, 1000, 580, 22, 2, 300, 200, &x, &y);
    CHECK(x == 5 && y == 60);
}

int
main(void)
{
    test_snap_names();
    test_snap_rect_halves();
    test_snap_rect_corners();
    test_snap_rect_odd_sizes();
    test_snap_rect_offset_area();
    test_snap_edges();

    if (tests_failed == 0)
        printf("OK: %d checks passed\n", tests_run);
    else
        printf("FAILED: %d of %d checks\n", tests_failed, tests_run);

    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
