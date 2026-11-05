/* test_parsers.c - unit tests for flowm's pure parsing routines
 *
 * Copyright (c) 2026 flowm contributors
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

    CHECK(config_parse_action("sticky", &b));
    CHECK(b.action == ACT_STICKY);

    CHECK(config_parse_action("snap_left", &b));
    CHECK(b.action == ACT_SNAP && b.arg_int == SNAP_LEFT);

    CHECK(config_parse_action("snap_right", &b));
    CHECK(b.action == ACT_SNAP && b.arg_int == SNAP_RIGHT);

    CHECK(config_parse_action("tile_left", &b));
    CHECK(b.action == ACT_SNAP && b.arg_int == SNAP_LEFT);

    CHECK(config_parse_action("tile_right", &b));
    CHECK(b.action == ACT_SNAP && b.arg_int == SNAP_RIGHT);

    CHECK(config_parse_action("snap_top", &b));
    CHECK(b.action == ACT_SNAP && b.arg_int == SNAP_TOP);

    CHECK(config_parse_action("snap_bottom_right", &b));
    CHECK(b.action == ACT_SNAP && b.arg_int == SNAP_BOTTOM_RIGHT);

    CHECK(config_parse_action("focus_last", &b));
    CHECK(b.action == ACT_FOCUS_LAST);

    CHECK(config_parse_action("scratchpad", &b));
    CHECK(b.action == ACT_SCRATCHPAD);

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

    CHECK(rules_parse_line(&rs, "mpv sticky"));
    CHECK(rs.count == 3);
    CHECK(rs.rules[2].sticky);

    CHECK(rules_parse_line(&rs, "feh snap_left"));
    CHECK(rs.count == 4);
    CHECK(rs.rules[3].snap == SNAP_LEFT);

    CHECK(rules_parse_line(&rs, "xterm position=10:20"));
    CHECK(rs.rules[4].set_position);
    CHECK(rs.rules[4].x == 10 && rs.rules[4].y == 20);

    CHECK(rules_parse_line(&rs, "dock snap_bottom_left,no_titlebar"));
    CHECK(rs.rules[5].snap == SNAP_BOTTOM_LEFT);
    CHECK(rs.rules[5].no_titlebar);

    CHECK(rules_parse_line(&rs, "chat workspace=3,follow"));
    CHECK(rs.rules[6].follow && rs.rules[6].workspace == 2);

    CHECK(rules_parse_line(&rs, "popup ignore"));
    CHECK(rs.rules[7].ignore);

    CHECK(rules_parse_line(&rs, "\"title:Save As*\" center"));
    CHECK(rs.rules[8].match_title);
    CHECK(strcmp(rs.rules[8].match, "Save As*") == 0);
    CHECK(!rs.rules[7].match_title);

    CHECK(!rules_parse_line(&rs, "NoOptionsHere"));
    CHECK(!rules_parse_line(&rs, "app bogus=1"));
    CHECK(!rules_parse_line(&rs, "app size=abc"));
    CHECK(!rules_parse_line(&rs, "title: center"));
    CHECK(!rules_parse_line(&rs, "\"title:Save As* center"));
    CHECK(!rules_parse_line(&rs, "\"title:Save As*\""));
    CHECK(rs.count == 9);
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

static const Binding *
find_binding(const Config *cfg, unsigned int mods, unsigned long sym)
{
    int i;

    for (i = 0; i < cfg->binding_count; i++) {
        const Binding *b = &cfg->bindings[i];

        if (b->modifiers == mods && b->keysym == sym)
            return b;
    }
    return NULL;
}

static void
write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");

    if (fp == NULL)
        return;
    fputs(text, fp);
    fclose(fp);
}

static void
test_config_files(void)
{
    char dir[256];
    char main_path[300], inc_path[300], loop_path[300];
    const char *tmp = getenv("TMPDIR");
    Config cfg;
    const Binding *b;

    snprintf(dir, sizeof(dir), "%s/flowm-test-XXXXXX",
             tmp != NULL && *tmp != '\0' ? tmp : "/tmp");
    if (mkdtemp(dir) == NULL) {
        CHECK(false);
        return;
    }
    snprintf(main_path, sizeof(main_path), "%s/main.conf", dir);
    snprintf(inc_path, sizeof(inc_path), "%s/extra.conf", dir);
    snprintf(loop_path, sizeof(loop_path), "%s/loop.conf", dir);

    write_file(inc_path,
               "border_width = 5\n"
               "bind = Mod4+x snap_top_left\n");
    write_file(main_path,
               "include = extra.conf\n");

    config_defaults(&cfg);
    CHECK(config_load(&cfg, main_path));
    CHECK(cfg.parse_errors == 0);
    CHECK(cfg.border_width == 5);

    b = find_binding(&cfg, Mod4Mask, XK_x);
    CHECK(b != NULL && b->action == ACT_SNAP &&
          b->arg_int == SNAP_TOP_LEFT);
    config_free(&cfg);

    write_file(main_path,
               "include = missing.conf\n"
               "bogus = 1\n");
    config_defaults(&cfg);
    CHECK(config_load(&cfg, main_path));
    CHECK(cfg.parse_errors == 2);
    config_free(&cfg);

    write_file(loop_path, "include = loop.conf\n");
    config_defaults(&cfg);
    CHECK(config_load(&cfg, loop_path));
    CHECK(cfg.parse_errors >= 1);
    config_free(&cfg);

    config_defaults(&cfg);
    CHECK(!config_load(&cfg, "/nonexistent/flowm.conf"));
    config_free(&cfg);

    unlink(main_path);
    unlink(inc_path);
    unlink(loop_path);
    rmdir(dir);
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
    test_config_files();

    if (tests_failed == 0)
        printf("OK: %d checks passed\n", tests_run);
    else
        printf("FAILED: %d of %d checks\n", tests_failed, tests_run);

    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
