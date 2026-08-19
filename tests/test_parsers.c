/* test_parsers.c - unit tests for flowm's pure parsing routines
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "../src/util.h"
#include "../src/log.h"
#include "../src/wm.h"
#include "../src/config.h"
#include "../src/rules.h"

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
test_str_helpers(void)
{
    char buf[16];
    char trimme[] = "   hello world \t ";

    CHECK(xstrlcpy(buf, "abc", sizeof(buf)) == 3);
    CHECK(strcmp(buf, "abc") == 0);
    CHECK(xstrlcpy(buf, "0123456789abcdefXYZ", sizeof(buf)) == 19);
    CHECK(strlen(buf) == 15);

    CHECK(strcmp(str_trim(trimme), "hello world") == 0);

    CHECK(str_ieq("MoD4", "mod4"));
    CHECK(!str_ieq("mod4", "mod41"));
    CHECK(str_has_prefix("workspace_name_3", "workspace_name_"));
    CHECK(!str_has_prefix("work", "workspace"));
}

static void
test_parse_long(void)
{
    long v = -1;

    CHECK(parse_long("42", 0, 100, &v) && v == 42);
    CHECK(parse_long("-7", -10, 10, &v) && v == -7);
    CHECK(parse_long("10  ", 0, 100, &v) && v == 10);
    CHECK(!parse_long("10x", 0, 100, &v));
    CHECK(!parse_long("101", 0, 100, &v));
    CHECK(!parse_long("", 0, 100, &v));
    CHECK(!parse_long(NULL, 0, 100, &v));
}

static void
test_parse_hex_color(void)
{
    unsigned long c = 0;

    CHECK(parse_hex_color("#ffffff", &c) && c == 0xffffff);
    CHECK(parse_hex_color("#4C7899", &c) && c == 0x4c7899);
    CHECK(parse_hex_color("#000000", &c) && c == 0x000000);
    CHECK(!parse_hex_color("ffffff", &c));
    CHECK(!parse_hex_color("#fff", &c));
    CHECK(!parse_hex_color("#gggggg", &c));
    CHECK(!parse_hex_color("#1234567", &c));
}

static void
test_parse_chord(void)
{
    unsigned int mods = 0;
    unsigned long sym = 0;

    CHECK(config_parse_chord("Mod4+Return", &mods, &sym));
    CHECK(mods == Mod4Mask);
    CHECK(sym == XK_Return);

    CHECK(config_parse_chord("Mod4+Shift+q", &mods, &sym));
    CHECK(mods == (Mod4Mask | ShiftMask));
    CHECK(sym == XK_q);

    CHECK(config_parse_chord("ctrl+alt+t", &mods, &sym));
    CHECK(mods == (ControlMask | Mod1Mask));
    CHECK(sym == XK_t);

    CHECK(!config_parse_chord("Mod4+a+b", &mods, &sym));
    CHECK(!config_parse_chord("Mod4+Shift", &mods, &sym));
    CHECK(!config_parse_chord("Mod4+NotAKeyXYZ", &mods, &sym));
}

static void
test_parse_action(void)
{
    Binding b;

    CHECK(config_parse_action("exec xterm -fg white", &b));
    CHECK(b.action == ACT_EXEC);
    CHECK(strcmp(b.arg_str, "xterm -fg white") == 0);
    free(b.arg_str);

    CHECK(config_parse_action("workspace 3", &b));
    CHECK(b.action == ACT_WORKSPACE && b.arg_int == 2);

    CHECK(config_parse_action("send_to_workspace 1", &b));
    CHECK(b.action == ACT_SEND_TO_WORKSPACE && b.arg_int == 0);

    CHECK(config_parse_action("move -1 0", &b));
    CHECK(b.action == ACT_MOVE && b.arg_int == -1 && b.arg_int2 == 0);

    CHECK(config_parse_action("resize 0 1", &b));
    CHECK(b.action == ACT_RESIZE && b.arg_int == 0 && b.arg_int2 == 1);

    CHECK(config_parse_action("fullscreen", &b));
    CHECK(b.action == ACT_FULLSCREEN);

    CHECK(!config_parse_action("workspace 0", &b));     /* 1-based   */
    CHECK(!config_parse_action("workspace 99", &b));    /* too big   */
    CHECK(!config_parse_action("exec", &b));            /* no cmd    */
    CHECK(!config_parse_action("frobnicate", &b));      /* unknown   */
}

static void
test_rule_patterns(void)
{
    CHECK(rules_pattern_matches("Firefox", "firefox"));
    CHECK(rules_pattern_matches("Gimp*", "gimp-2.10"));
    CHECK(rules_pattern_matches("*", "anything"));
    CHECK(!rules_pattern_matches("Gimp", "gimp-2.10"));
    CHECK(!rules_pattern_matches("Fire", "firefox"));
    CHECK(!rules_pattern_matches("", "firefox"));
}

static void
test_rule_lines(void)
{
    RuleSet rs;

    memset(&rs, 0, sizeof(rs));

    CHECK(rules_parse_line(&rs, "Firefox workspace=2,maximize"));
    CHECK(rs.count == 1);
    CHECK(rs.rules[0].set_workspace && rs.rules[0].workspace == 1);
    CHECK(rs.rules[0].maximize);
    CHECK(!rs.rules[0].fullscreen);

    CHECK(rules_parse_line(&rs, "mpv center,size=960x540"));
    CHECK(rs.count == 2);
    CHECK(rs.rules[1].center);
    CHECK(rs.rules[1].set_size);
    CHECK(rs.rules[1].width == 960 && rs.rules[1].height == 540);

    CHECK(rules_parse_line(&rs, "xterm position=10:20"));
    CHECK(rs.rules[2].set_position);
    CHECK(rs.rules[2].x == 10 && rs.rules[2].y == 20);

    CHECK(!rules_parse_line(&rs, "NoOptionsHere"));
    CHECK(!rules_parse_line(&rs, "app bogus=1"));
    CHECK(!rules_parse_line(&rs, "app size=abc"));
    CHECK(rs.count == 3);
}

static void
test_config_defaults_roundtrip(void)
{
    Config cfg;

    config_defaults(&cfg);
    CHECK(cfg.border_width >= 0);
    CHECK(cfg.workspace_count >= 1 &&
          cfg.workspace_count <= FLOWM_MAX_WORKSPACES);
    CHECK(cfg.binding_count > 0);
    CHECK(cfg.rules.count == 0);
    CHECK(strcmp(cfg.workspace_names[0], "1") == 0);
    config_free(&cfg);
    CHECK(cfg.binding_count == 0);
}


int
main(void)
{
    log_init(NULL, LOG_ERROR, true);

    test_str_helpers();
    test_parse_long();
    test_parse_hex_color();
    test_parse_chord();
    test_parse_action();
    test_rule_patterns();
    test_rule_lines();
    test_config_defaults_roundtrip();

    if (tests_failed == 0)
        printf("OK: %d checks passed\n", tests_run);
    else
        printf("FAILED: %d of %d checks\n", tests_failed, tests_run);

    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
