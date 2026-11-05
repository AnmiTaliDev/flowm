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
test_snap_rect_halves(void)
{
    int x, y, w, h;

    geometry_snap_rect(SNAP_LEFT, 0, 20, 1000, 580, 22, 2,
                       &x, &y, &w, &h);
    CHECK(x == 2 && y == 44 && w == 496 && h == 554);

    geometry_snap_rect(SNAP_RIGHT, 0, 20, 1000, 580, 22, 2,
                       &x, &y, &w, &h);
    CHECK(x == 502 && y == 44 && w == 496 && h == 554);
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

int
main(void)
{
    test_snap_rect_halves();
    test_snap_rect_odd_sizes();

    if (tests_failed == 0)
        printf("OK: %d checks passed\n", tests_run);
    else
        printf("FAILED: %d of %d checks\n", tests_failed, tests_run);

    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
