/* Tests for the saved-settings code of crop_rec (tests/run.sh pulls the real table and the real
 * functions out of modules/crop_rec/crop_rec.c, and the real range check from src/settings-check.h) */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "film-formats.h"
#include "settings-check.h"

#define COUNT(x) ((int)(sizeof(x) / sizeof((x)[0])))
#define COERCE(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

/* the saved variables, same names and defaults as in crop_rec.c */
static int fps_over = 0, tapdisp = 1, crop_preset_fps_reduce = 1, crop_preset_index = 3, shutter_range = 0,
           fix_dual_iso_flicker = 1, bit_depth_analog = 1, brighten_lv_method = 0, crop_preset_ar_menu = 4,
           crop_preset_1x1_res_menu = 3, crop_preset_1x3_res_menu = 1, crop_preset_3x3_res_menu = 2,
           crop_preset_fps_menu = 0, SET_button = 1, Half_Shutter = 2, INFO_button = 0, Shutter_zoom = 0, Shutter_rec = 0,
           Arrows_U_D = 3, more_hacks = 1, Arrows_L_R = 2, crop_settings_ver = 0, slim_film_fmt = 0, slim_film_frames = 0;

/* ---- real code from crop_rec.c ---- */
#include "crop_rec_snippets.h"

static void reset_defaults(void)
{
    fps_over = 0; tapdisp = 1; crop_preset_fps_reduce = 1; crop_preset_index = 3; shutter_range = 0;
    fix_dual_iso_flicker = 1; bit_depth_analog = 1; brighten_lv_method = 0; crop_preset_ar_menu = 4;
    crop_preset_1x1_res_menu = 3; crop_preset_1x3_res_menu = 1; crop_preset_3x3_res_menu = 2;
    crop_preset_fps_menu = 0; SET_button = 1; Half_Shutter = 2; INFO_button = 0; Shutter_zoom = 0; Shutter_rec = 0;
    Arrows_U_D = 3; more_hacks = 1; Arrows_L_R = 2; crop_settings_ver = 0; slim_film_fmt = 0; slim_film_frames = 0;
}

int main(void)
{
    /* --- settings_check itself --- */
    {
        int a = 5, b = 50, c = -3;
        struct setting_range t[] = { SETTING(a, 0, 10, 7), SETTING(b, 0, 10, 7), SETTING(c, 0, 10, 7) };
        int n = settings_check(t, 3);
        CHECK(n == 2 && a == 5 && b == 7 && c == 7, "settings_check: n=%d a=%d b=%d c=%d", n, a, b, c);
        a = 0; b = 10;
        CHECK(settings_check(t, 2) == 0, "settings_check: both ends of the range are valid");
        int big = INT_MAX, small = INT_MIN;
        struct setting_range t2[] = { SETTING(big, 0, 10, 1), SETTING(small, 0, 10, 2) };
        settings_check(t2, 2);
        CHECK(big == 1 && small == 2, "settings_check: extreme values are reset");
    }

    /* --- table sanity: every default (and every fix value) is inside its own range --- */
    for (int i = 0; i < COUNT(crop_settings); i++)
    {
        CHECK(crop_settings[i].min <= crop_settings[i].max, "table row %d: min > max", i);
        CHECK(crop_settings[i].fix >= crop_settings[i].min && crop_settings[i].fix <= crop_settings[i].max, "table row %d: reset value outside the range", i);
    }
    reset_defaults();
    {
        int saved_ver = crop_settings_ver;
        CHECK(settings_check(crop_settings, COUNT(crop_settings)) == 0, "the default values must all be inside their ranges");
        CHECK(saved_ver == crop_settings_ver, "range check must not touch the version");
    }

    /* --- fresh install: nothing changes, version becomes current --- */
    reset_defaults();
    crop_settings_load();
    CHECK(crop_settings_ver == CROP_SETTINGS_VERSION, "version after first load: %d", crop_settings_ver);
    CHECK(INFO_button == 0 && Arrows_U_D == 3 && SET_button == 1 && crop_preset_index == 3 && fps_over == 0, "fresh defaults must stay as they are");

    /* --- config from the oldest builds (version 0): INFO and arrows are converted --- */
    static const struct { int old_info, new_info; const char * what; } info_map[] = {
        { 0, 0, "OFF" }, { 1, 0, "Aperture -> OFF" }, { 2, 4, "False Color" }, { 3, 0, "Dual ISO -> OFF" }, { 4, 5, "Framing" },
    };
    for (int i = 0; i < COUNT(info_map); i++)
    {
        reset_defaults();
        INFO_button = info_map[i].old_info;
        crop_settings_load();
        CHECK(INFO_button == info_map[i].new_info, "v0 INFO %s: %d -> %d, expected %d", info_map[i].what, info_map[i].old_info, INFO_button, info_map[i].new_info);
    }
    reset_defaults(); Arrows_U_D = 1; Arrows_L_R = 1;
    crop_settings_load();
    CHECK(Arrows_U_D == 3 && Arrows_L_R == 3, "v0 arrows ISO (1) become 3: %d %d", Arrows_U_D, Arrows_L_R);
    reset_defaults(); Arrows_U_D = 2;
    crop_settings_load();
    CHECK(Arrows_U_D == 2, "v0 arrows Aperture (2) stays 2");

    /* --- version 2 config (Dual ISO still in the list): 1 -> OFF, 2..6 -> 1..5 --- */
    for (int v = 0; v <= 6; v++)
    {
        reset_defaults(); crop_settings_ver = 2; INFO_button = v;
        crop_settings_load();
        int expect = (v <= 1) ? 0 : v - 1;
        CHECK(INFO_button == expect, "v2 INFO %d -> %d, expected %d", v, INFO_button, expect);
    }

    /* --- current config: INFO untouched, and loading twice changes nothing more --- */
    for (int v = 0; v <= 6; v++)
    {
        reset_defaults(); crop_settings_ver = 3; INFO_button = v; Shutter_rec = 1;
        crop_settings_load();
        CHECK(INFO_button == v, "v3 INFO %d must stay, got %d", v, INFO_button);
        CHECK(Shutter_rec == 1, "v3 Shutter record must stay on");
        crop_settings_load();
        CHECK(INFO_button == v && crop_settings_ver == 3, "second load must change nothing");
    }
    reset_defaults(); INFO_button = 2;               /* old FC, converted once... */
    crop_settings_load();
    int once = INFO_button;
    crop_settings_load();                            /* ...and not again */
    CHECK(INFO_button == once, "converted INFO value must not be converted twice");

    /* --- a config written by a NEWER build is not converted and the version is not lowered --- */
    reset_defaults(); crop_settings_ver = CROP_SETTINGS_VERSION + 4; INFO_button = 1;
    crop_settings_load();
    CHECK(crop_settings_ver == CROP_SETTINGS_VERSION + 4 && INFO_button == 1, "newer config: version %d INFO %d", crop_settings_ver, INFO_button);

    /* --- damaged / hand-edited values are reset --- */
    reset_defaults(); crop_settings_ver = 3;
    crop_preset_index = 9; crop_preset_1x1_res_menu = 99; crop_preset_3x3_res_menu = -4; crop_preset_ar_menu = 5;
    crop_preset_fps_menu = 12; bit_depth_analog = -2; fps_over = 2000000000; SET_button = 0; Arrows_U_D = 7;
    INFO_button = 40; slim_film_fmt = 7; slim_film_frames = -1; Shutter_zoom = 3; shutter_range = 2; tapdisp = 6;
    crop_settings_load();
    CHECK(crop_preset_index == 1, "crop_preset_index reset to 1, got %d", crop_preset_index);
    CHECK(crop_preset_1x1_res_menu == 3 && crop_preset_3x3_res_menu == 2 && crop_preset_ar_menu == 4 && crop_preset_fps_menu == 0, "preset menus reset to defaults");
    CHECK(bit_depth_analog == 1 && fps_over == 0 && SET_button == 1 && Arrows_U_D == 3 && INFO_button == 0, "other values reset");
    CHECK(slim_film_fmt == 0 && slim_film_frames == 0 && Shutter_zoom == 0 && shutter_range == 0 && tapdisp == 1, "film format / misc reset");

    /* --- legitimate values from every menu survive --- */
    reset_defaults(); crop_settings_ver = 3;
    crop_preset_index = 2; crop_preset_1x1_res_menu = 7; crop_preset_1x3_res_menu = 3; crop_preset_3x3_res_menu = 0;
    crop_preset_ar_menu = 0; crop_preset_fps_menu = 3; bit_depth_analog = 3; fps_over = -100000; SET_button = 2;
    Arrows_U_D = 0; INFO_button = 6; slim_film_fmt = 5; slim_film_frames = 4095; Shutter_zoom = 2; tapdisp = 5;
    crop_settings_load();
    CHECK(crop_preset_index == 2 && crop_preset_1x1_res_menu == 7 && crop_preset_1x3_res_menu == 3 && crop_preset_3x3_res_menu == 0 &&
          crop_preset_ar_menu == 0 && crop_preset_fps_menu == 3 && bit_depth_analog == 3 && fps_over == -100000 && SET_button == 2 &&
          Arrows_U_D == 0 && INFO_button == 6 && slim_film_fmt == 5 && slim_film_frames == 4095 && Shutter_zoom == 2 && tapdisp == 5,
          "values at the edge of their ranges must be kept");

    /* --- Frame choice per Film Format (2 bits each, saved in one number) --- */
    reset_defaults();
    for (int f = 0; f < FILM_FORMAT_COUNT; f++)
        for (int k = 0; k < film_formats[f].count; k++)
        {
            slim_film_frames = 0;
            slim_film_frame_set(f, k);
            CHECK(slim_film_frame_get(f) == k, "frame %d of format %d reads back as %d", k, f, slim_film_frame_get(f));
            for (int o = 0; o < FILM_FORMAT_COUNT; o++)
                if (o != f)
                    CHECK(slim_film_frame_get(o) == 0, "setting format %d changed format %d", f, o);
        }
    slim_film_frames = 0;
    for (int f = 0; f < FILM_FORMAT_COUNT; f++)
        slim_film_frame_set(f, film_formats[f].count - 1);
    for (int f = 0; f < FILM_FORMAT_COUNT; f++)
        CHECK(slim_film_frame_get(f) == film_formats[f].count - 1, "all formats at their last frame: format %d reads %d", f, slim_film_frame_get(f));
    slim_film_frames = 0xFFF;                        /* every field = 3: must clamp to the last valid frame */
    for (int f = 0; f < FILM_FORMAT_COUNT; f++)
        CHECK(slim_film_frame_get(f) == film_formats[f].count - 1, "format %d: stored 3 must clamp to %d, got %d", f, film_formats[f].count - 1, slim_film_frame_get(f));
    CHECK(slim_film_frames <= (1 << (2 * FILM_FORMAT_COUNT)) - 1, "frames field fits its range");

    printf("settings_tests: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}
