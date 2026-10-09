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
static int slim_film_fmt = 0, slim_film_frames = 0;
static int apply_calls = 0;

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
    set_state(7, 0);                       /* 1/2": frame 0 is 16:9 (2.5K) */
    slim_mode_ui = 0; slim_1x1_ar = 3; slim_unified_preset = 0;     /* but the camera is on 1280p */
    CHECK(slim_film_sync() == 7 && slim_film_frame_get(7) == 1, "1/2\" on 1280p must show its 4:3 frame");

    /* 3. leftover legacy modes are not film formats */
    slim_mode_ui = 0; slim_1x1_ar = 1; slim_unified_preset = 1;      /* 2.8K Higher */
    CHECK(slim_film_sync() == -1, "2.8K is not a film format");
    slim_mode_ui = 2; crop_preset_ar_menu = 0;                       /* 3x3 16:9 high fps */
    CHECK(slim_film_sync() == -1, "3x3 high fps is not a film format");
    slim_mode_ui = 1;
    CHECK(slim_film_sync() == -1, "1x3 is not a film format");
    slim_mode_ui = 3;
    CHECK(slim_film_sync() == -1, "Full-Res LV is not a film format");

    /* 4. the default install is FILM */
    slim_film_fmt = 0; slim_film_frames = 0;
    CHECK(!slim_video_standard(), "default standard is FILM");

    printf("menu_tests: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}
