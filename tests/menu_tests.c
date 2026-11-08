/* Tests for the Film Format / Standard menu logic of crop_rec.  tests/run.sh pulls the real
 * functions out of modules/crop_rec/crop_rec.c, so this runs the code that is built into the camera. */
#include <stdio.h>
#include <string.h>
#include "film-formats.h"

#define COERCE(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

/* the menu state, same names as in crop_rec.c */
static int slim_mode_ui = 2, slim_1x1_ar = 2, slim_unified_preset = 0, crop_preset_ar_menu = 4;
static int slim_film_fmt = 0, slim_film_frames = 0, crop_preset_fps_menu = 0;
static int apply_calls = 0;
/* backend state read by slim_crop_fps_mask() */
enum { CROP_PRESET_OFF, CROP_PRESET_1X1, CROP_PRESET_1X3, CROP_PRESET_3X3 };
static int g_preset = CROP_PRESET_3X3, crop_preset_1x1_res_menu = 3, crop_preset_1x3_res_menu = 1;
#define CROP_PRESET_MENU g_preset

#include "crop_rec_menu_snippets.h"

/* stand-in for the real mode switch (which writes the sensor settings): not under test here */
static void slim_crop_apply_mode(void) { apply_calls++; }
/* real slim_film_apply is extracted after this point */
#include "crop_rec_apply_snippet.h"

static void set_state(int fmt, int frame)
{
    slim_film_fmt = fmt;
    slim_film_frames = 0;
    slim_film_frame_set(fmt, frame);
}

int main(void)
{
    /* 1. every format and frame: apply -> menu state -> sync gives back the same format and frame */
    for (int f = 0; f < FILM_FORMAT_COUNT; f++)
        for (int k = 0; k < film_formats[f].count; k++)
        {
            set_state(f, k);
            apply_calls = 0;
            slim_film_apply(f);
            CHECK(apply_calls == 1, "format %d frame %d: apply must switch the mode once", f, k);
            int ro = slim_film_menu_readout();
            CHECK(ro == film_frames[film_frame_index(f, k)].readout, "format %d frame %d: menu readout %d, table says %d",
                  f, k, ro, film_frames[film_frame_index(f, k)].readout);
            /* lose the saved choice, as after a reboot with a different group: sync keeps what it can */
            int fmt = slim_film_sync();
            CHECK(fmt == f && slim_film_frame_get(f) == k, "format %d frame %d: sync gives format %d frame %d", f, k, fmt, slim_film_frame_get(f));
            CHECK(slim_video_standard() == film_is_video(f), "format %d: standard flag wrong", f);
        }

    /* 2. a saved frame that belongs to another readout is repaired by sync */
    set_state(9, 0);                       /* 1/3": frame 0 is 16:9 (2.5K) */
    slim_mode_ui = 0; slim_1x1_ar = 3; slim_unified_preset = 0;     /* but the camera is on 1280p */
    CHECK(slim_film_sync() == 4, "1/3\" cannot use 1280p: the first format that can (S8)");
    set_state(8, 1);                       /* 1/2": both frames are on 1280p, so the saved frame is kept */
    CHECK(slim_film_sync() == 8 && slim_film_frame_get(8) == 1, "1/2\" 4:3 stays on 1280p");

    /* 3. leftover legacy modes are not film formats */
    slim_mode_ui = 0; slim_1x1_ar = 1; slim_unified_preset = 1;      /* 2.8K Higher */
    CHECK(slim_film_sync() == 2 && slim_film_frame_get(2) == 3, "2.8K Higher is S16 2.35:1");
    slim_mode_ui = 0; slim_1x1_ar = 1; slim_unified_preset = 0;      /* 3K Highest */
    CHECK(slim_film_sync() == -1, "3K is not a film format any more");
    slim_mode_ui = 2; crop_preset_ar_menu = 0;                       /* 3x3 16:9 high fps */
    CHECK(slim_film_sync() == -1, "3x3 high fps is not a film format");
    slim_mode_ui = 1;
    CHECK(slim_film_sync() == -1, "1x3 16:9 is not a film format");
    slim_mode_ui = 1; crop_preset_ar_menu = 3; slim_unified_preset = 1;
    CHECK(slim_film_sync() == -1, "1x3 2.35:1 Higher is not a CROP MOOD format (Highest only)");
    slim_unified_preset = 0;
    slim_film_fmt = 12;
    CHECK(slim_film_sync() == 12, "1x3 2.35:1 Highest keeps CROP MOOD 1x3 when that was picked");
    slim_film_fmt = 0;
    CHECK(slim_film_sync() == 2, "1x3 2.35:1 Highest from A35 -> S16 Actual (the first 1x3 frame)");
    crop_preset_ar_menu = 0;
    slim_mode_ui = 3;
    CHECK(slim_film_sync() == -1, "Full-Res LV is not a film format");

    /* 3b. frame rates: bit0 = 23.976, bit1 = 25, bit2 = 30 (29.97 in VIDEO), bit3 = 18.
     *     FILM never offers 30 or 29.97; VIDEO never offers 18; VIDEO offers 29.97 only on the
     *     2.5K and 1280p frames (1440p / 1620p do not run it) */
    for (int f = 0; f < FILM_FORMAT_COUNT; f++)
        for (int k = 0; k < film_formats[f].count; k++)
        {
            set_state(f, k);
            slim_film_apply(f);
            g_preset = (slim_mode_ui == 2) ? CROP_PRESET_3X3 : (slim_mode_ui == 1) ? CROP_PRESET_1X3 : CROP_PRESET_1X1;
            crop_preset_1x3_res_menu = slim_unified_preset;
            int mask = slim_crop_fps_mask();
            int ro = film_frames[film_frame_index(f, k)].readout;
            CHECK(mask & 1, "format %d frame %d: 23.976 must always be offered", f, k);
            /* 36 fps (bit 4): every A35 frame (3x3 readout), nothing else.
             * 48 fps (bit 5): only the A35 frames that fit in the 976-row readout. */
            int a35 = (ro == FILM_RO_3X3);
            int fits976 = a35 && film_frames[film_frame_index(f, k)].h <= 958;
            CHECK(!!(mask & 0x10) == a35, "format %d frame %d: 36 fps offered=%d, expected %d (mask %x)", f, k, !!(mask & 0x10), a35, mask);
            CHECK(!!(mask & 0x20) == fits976, "format %d frame %d: 48 fps offered=%d, expected %d (mask %x)", f, k, !!(mask & 0x20), fits976, mask);
            if (!film_is_video(f))
                CHECK(!(mask & 4), "FILM format %d frame %d must not offer 30 / 29.97 (mask %x)", f, k, mask);
            else
            {
                CHECK(!(mask & 8), "VIDEO format %d frame %d must not offer 18 fps (mask %x)", f, k, mask);
                /* 29.97 is offered on the 2.5K frames and on 1280p; 1440p does not run it */
                int ok30 = (ro == FILM_RO_25K || ro == FILM_RO_1280);
                CHECK(!!(mask & 4) == ok30, "VIDEO format %d frame %d: 29.97 offered=%d, expected %d (mask %x)", f, k, !!(mask & 4), ok30, mask);
            }
        }

    /* 3c. Readout Speed (tenths of ms): recorded height x line time, line time from the readout.
     *     Independent expectations worked out by hand: rows * clocks / 3200, rounded. */
    {
        static const struct { int w, h, fps, x10; } exp[] = {
            { 1696,  954, 0, 157 }, { 1696,  916, 0, 151 }, { 1696,  722, 0, 119 },   /* A35, 528 clocks */
            { 1376, 1152, 0, 190 }, { 1536, 1152, 0, 190 },                          /* anamorphic */
            { 1696,  954, 1, 191 },                                                  /* 25 fps: 640 clocks */
            { 1696,  954, 4, 157 },                                                  /* 36 fps: 528 clocks */
            { 1696,  954, 5, 155 }, { 1696,  722, 5, 117 },                          /* 48 fps: 520 clocks */
            { 2832, 1204, 0, 303 },                                                  /* S16 2.35:1, 2.8K 806 clocks */
            { 2384, 1340, 0, 307 },                                                  /* 16mm, 1440p 732 clocks */
            { 1344,  930, 0, 164 },                                                  /* S8, 1280p 566 clocks */
            { 2032, 1536, 0, 304 },                                                  /* 2/3" 4:3, 1620p 634 clocks */
            {  968, 1720, 0, 280 }, {  792, 1736, 0, 282 },                          /* S16 / 16mm Actual, 1x3 520 clocks */
            { 1024, 2046, 0, 332 }, { 1024, 1728, 0, 281 },                          /* 1" Actual / 16:9, 1x3 */
            { 1104,  620, 0, 139 },                                                  /* 1/3", 2.5K 716 clocks */
            { 1736, 1152, 0, 190 }, { 1736,  976, 0, 161 },                          /* CROP MOOD 3x3, 528 clocks */
            { 1736,  736, 5, 120 },                                                  /* CROP MOOD 3x3 2.35:1 at 48 fps */
            { 1736, 2214, 0, 360 },                                                  /* CROP MOOD 1x3, 520 clocks */
        };
        for (unsigned n = 0; n < sizeof(exp) / sizeof(exp[0]); n++)
        {
            int found = 0;
            for (int f = 0; f < FILM_FORMAT_COUNT && !found; f++)
                for (int k = 0; k < film_formats[f].count && !found; k++)
                {
                    int i = film_frame_index(f, k);
                    if (film_frames[i].w != exp[n].w || film_frames[i].h != exp[n].h)
                        continue;
                    found = 1;
                    set_state(f, k);
                    slim_film_apply(f);
                    crop_preset_fps_menu = exp[n].fps;
                    CHECK(slim_readout_x10() == exp[n].x10, "%dx%d at fps index %d: readout %d, expected %d",
                          exp[n].w, exp[n].h, exp[n].fps, slim_readout_x10(), exp[n].x10);
                }
            CHECK(found, "readout test: no frame %dx%d", exp[n].w, exp[n].h);
        }
        crop_preset_fps_menu = 0;
        /* every format and frame has a readout time; none is zero or absurd */
        for (int f = 0; f < FILM_FORMAT_COUNT; f++)
            for (int k = 0; k < film_formats[f].count; k++)
            {
                set_state(f, k);
                slim_film_apply(f);
                CHECK(slim_readout_x10() >= 80 && slim_readout_x10() <= 400, "format %d frame %d: readout %d out of range", f, k, slim_readout_x10());
            }
    }

    /* 4. the default install is FILM */
    slim_film_fmt = 0; slim_film_frames = 0;
    CHECK(!slim_video_standard(), "default standard is FILM");

    printf("menu_tests: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}
