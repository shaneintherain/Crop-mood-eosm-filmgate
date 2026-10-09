/* Tests for src/film-formats.h (the Film Format table) - run on a computer, see tests/run.sh */
#include <stdio.h>
#include <string.h>
#include "film-formats.h"

#define COUNT(x) ((int)(sizeof(x) / sizeof((x)[0])))
static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

/* sensor readouts named in the 'mode' column: "...  WxH ..." */
static int readout_of(const char * mode, int * w, int * h)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", mode);
    for (char * t = strtok(buf, " "); t; t = strtok(NULL, " "))
        if (sscanf(t, "%dx%d", w, h) == 2 && *w >= 640)
            return 1;
    return 0;
}

int main(void)
{
    /* table shape */
    CHECK(COUNT(film_frames) == FILM_FRAME_COUNT, "film_frames[] has %d entries, FILM_FRAME_COUNT says %d", COUNT(film_frames), FILM_FRAME_COUNT);
    CHECK(COUNT(film_formats) == FILM_FORMAT_COUNT, "film_formats[] has %d entries, FILM_FORMAT_COUNT says %d", COUNT(film_formats), FILM_FORMAT_COUNT);
    CHECK(film_frames[0].w == 0 && film_frames[0].h == 0 && !strcmp(film_frames[0].name, "OFF"), "entry 0 must be OFF");

    /* each Film Format owns a run of frames; together they cover entries 1..N-1 exactly once */
    int next = 1;
    for (int f = 0; f < FILM_FORMAT_COUNT; f++)
    {
        CHECK(film_formats[f].first == next, "format %d (%s) starts at %d, expected %d", f, film_formats[f].name, film_formats[f].first, next);
        CHECK(film_formats[f].count >= 1 && film_formats[f].count <= 4, "format %d has %d frames (the saved setting stores 2 bits per format: 1..4)", f, film_formats[f].count);
        CHECK(film_formats[f].name[0] && film_formats[f].label[0], "format %d needs a name and a label", f);
        next += film_formats[f].count;
    }
    CHECK(next == FILM_FRAME_COUNT, "formats cover %d entries, table has %d", next, FILM_FRAME_COUNT);

    /* every frame: sensible size, aligned, fits the readout named in 'mode', all texts filled in */
    for (int i = 1; i < FILM_FRAME_COUNT; i++)
    {
        const struct film_frame * fr = &film_frames[i];
        int rw = 0, rh = 0;
        CHECK(fr->w > 0 && fr->h > 0, "frame %d (%s): empty size", i, fr->name);
        CHECK(fr->w % 16 == 0, "frame %d (%s): width %d is not a multiple of 16", i, fr->name, fr->w);
        CHECK(fr->h % 2 == 0, "frame %d (%s): height %d is odd", i, fr->name, fr->h);
        CHECK(fr->name[0] && fr->gate[0] && fr->mode[0] && fr->frame[0], "frame %d has an empty text", i);
        CHECK(readout_of(fr->mode, &rw, &rh), "frame %d (%s): no readout size in '%s'", i, fr->name, fr->mode);
        CHECK(fr->w <= rw && fr->h <= rh, "frame %d (%s): %dx%d does not fit the %dx%d readout", i, fr->name, fr->w, fr->h, rw, rh);
        /* the readout number must be the one the 'mode' text names */
        CHECK(fr->readout >= 0 && fr->readout < FILM_RO_COUNT, "frame %d (%s): bad readout number %d", i, fr->name, fr->readout);
        if (fr->readout >= 0 && fr->readout < FILM_RO_COUNT)
        {
            CHECK(film_readouts[fr->readout].w == rw && film_readouts[fr->readout].h == rh,
                  "frame %d (%s): readout table says %dx%d, 'mode' text says %dx%d", i, fr->name,
                  film_readouts[fr->readout].w, film_readouts[fr->readout].h, rw, rh);
            CHECK(fr->w <= film_readouts[fr->readout].w && fr->h <= film_readouts[fr->readout].h,
                  "frame %d (%s): does not fit its readout", i, fr->name);
        }
    }

    /* frames of one format share the width (only the height / crop differs).  FILM formats
     * also share one readout; VIDEO sizes may use a different readout per frame */
    for (int f = 0; f < FILM_FORMAT_COUNT; f++)
        for (int k = 1; k < film_formats[f].count; k++)
        {
            const struct film_frame * a = &film_frames[film_formats[f].first];
            const struct film_frame * b = &film_frames[film_formats[f].first + k];
            if (!film_is_video(f))
                CHECK(!strcmp(a->mode, b->mode) && a->readout == b->readout, "format %s: frames use different readouts", film_formats[f].name);
            if (f != 1 && f != 6) /* anamorphic: 2x and 1.33x are squeezed differently; 2/3" 1.85:1 needs the narrower 2.5K window */
                CHECK(a->w == b->w, "format %s: frames have different widths", film_formats[f].name);
        }

    /* the two standards */
    CHECK(FILM_FILM_COUNT == 6 && FILM_FORMAT_COUNT == 11, "6 FILM + 5 VIDEO formats expected");
    CHECK(!film_is_video(0) && !film_is_video(5) && film_is_video(6) && film_is_video(10), "film_is_video boundaries");
    for (int f = FILM_FILM_COUNT; f < FILM_FORMAT_COUNT; f++)
        for (int k = 0; k < film_formats[f].count; k++)
        {
            const struct film_frame * fr = &film_frames[film_formats[f].first + k];
            /* video sizes are 16:9, 4:3, 1.85:1 or 2.35:1 */
            int r1000 = fr->w * 1000 / fr->h;
            CHECK((r1000 > 1740 && r1000 < 1790) || (r1000 > 1320 && r1000 < 1355) ||
                  (r1000 > 1840 && r1000 < 1870) || (r1000 > 2330 && r1000 < 2360),
                  "video frame %s: odd aspect ratio %d/1000", fr->name, r1000);
            /* none of them may need more than the physical sensor: 4.30 um per pixel (2/3" is 8.8 mm = 2047 px) */
            CHECK(fr->w <= 2047, "video frame %s wider than a 2/3\" sensor", fr->name);
        }
    /* the sensor widths, in order (largest first) */
    for (int f = FILM_FILM_COUNT + 1; f < FILM_FORMAT_COUNT; f++)
        CHECK(film_frames[film_formats[f].first].w < film_frames[film_formats[f - 1].first].w,
              "video size %s is not smaller than %s", film_formats[f].name, film_formats[f - 1].name);

    /* film_pick(): readout -> format and frame */
    {
        int fr;
        CHECK(film_pick(-1, 0, 0, &fr) == -1, "no readout -> no format");
        CHECK(film_pick(FILM_RO_COUNT, 0, 0, &fr) == -1, "bad readout -> no format");
        /* the saved choice is kept whenever it can use the readout */
        CHECK(film_pick(FILM_RO_3X3, 1, 1, &fr) == 1 && fr == 1, "A35-ANA frame 1 is kept");
        CHECK(film_pick(FILM_RO_3X3, 0, 2, &fr) == 0 && fr == 2, "A35 frame 2 is kept");
        CHECK(film_pick(FILM_RO_1280, 5, 1, &fr) == 5 && fr == 1, "8mm is kept on 1280p");
        CHECK(film_pick(FILM_RO_1280, 7, 1, &fr) == 7 && fr == 1, "1/2\" 4:3 is kept on 1280p");
        CHECK(film_pick(FILM_RO_25K, 8, 1, &fr) == 8 && fr == 1, "1/2.3\" 4:3 is kept on 2.5K");
        /* a stored frame from another readout is repaired */
        CHECK(film_pick(FILM_RO_1280, 7, 0, &fr) == 7 && fr == 1, "1/2\" on 1280p must be its 4:3 frame");
        CHECK(film_pick(FILM_RO_25K, 7, 1, &fr) == 7 && fr == 0, "1/2\" on 2.5K must be its 16:9 frame");
        CHECK(film_pick(FILM_RO_1620, 6, 0, &fr) == 6 && fr == 1, "2/3\" on 1620p must be its 4:3 frame");
        CHECK(film_pick(FILM_RO_1440, 6, 2, &fr) == 6 && fr == 0, "2/3\" on 1440p must be its 16:9 frame");
        /* a format that cannot use the readout is replaced by the first one that can */
        CHECK(film_pick(FILM_RO_3K, 3, 0, &fr) == 2 && fr == 0, "3K from 16mm -> S16");
        CHECK(film_pick(FILM_RO_3K, 8, 0, &fr) == 2 && fr == 0, "3K from 1/2.3\" -> S16 (the only 3K format)");
        CHECK(film_pick(FILM_RO_1280, 2, 0, &fr) == 4 && fr == 0, "1280p from S16 -> S8");
        CHECK(film_pick(FILM_RO_25K, 0, 0, &fr) == 6 && fr == 2, "2.5K from A35 -> 2/3\" 1.85:1");
        CHECK(film_pick(FILM_RO_1620, 0, 0, &fr) == 6 && fr == 1, "1620p from A35 -> 2/3\" 4:3");
        /* every frame of every format maps back to itself */
        for (int f = 0; f < FILM_FORMAT_COUNT; f++)
            for (int k = 0; k < film_formats[f].count; k++)
            {
                int ro = film_frames[film_formats[f].first + k].readout;
                int got = film_pick(ro, f, k, &fr);
                CHECK(got == f && fr == k, "frame %d of format %d does not map back to itself (got %d/%d)", k, f, got, fr);
            }
        /* no readout is left without a format */
        for (int ro = 0; ro < FILM_RO_COUNT; ro++)
            CHECK(film_pick(ro, 0, 0, &fr) >= 0, "readout %d belongs to no format", ro);
    }

    /* black-bar scale: pixel scale on the 720x480 layer, window always fits the layer */
    for (int i = 1; i < FILM_FRAME_COUNT; i++)
    {
        const struct film_readout * ro = &film_readouts[film_frames[i].readout];
        int nw = film_frames[i].w * 720 / ro->w;
        int nh = film_frames[i].h * ro->kn / ro->kd;
        CHECK(nw <= 720 && nh <= 480, "frame %d (%s): bars %dx%d do not fit the 720x480 layer", i, film_frames[i].name, nw, nh);
    }

    /* film_frame_index: right entry, clamps bad input */
    for (int f = 0; f < FILM_FORMAT_COUNT; f++)
        for (int k = 0; k < film_formats[f].count; k++)
        {
            int idx = film_frame_index(f, k);
            CHECK(idx == film_formats[f].first + k && idx >= 1 && idx < FILM_FRAME_COUNT, "film_frame_index(%d,%d) = %d", f, k, idx);
        }
    CHECK(film_frame_index(-5, 0) == film_formats[0].first, "negative format is clamped");
    CHECK(film_frame_index(99, 0) == film_formats[FILM_FORMAT_COUNT - 1].first, "huge format is clamped");
    CHECK(film_frame_index(3, 99) == film_formats[3].first + film_formats[3].count - 1, "huge frame is clamped to the last frame of the format");
    CHECK(film_frame_index(3, -1) == film_formats[3].first, "negative frame is clamped");

    /* the "Recorded Size" shown in the menu must be exactly what is written:
     * film_align_height() must leave every film frame unchanged, for every data format */
    static const int bpps[] = { 14, 12, 10 };
    for (int i = 1; i < FILM_FRAME_COUNT; i++)
        for (int c = 0; c < 2; c++)
            for (int b = 0; b < 3; b++)
            {
                int h = film_align_height(film_frames[i].w, film_frames[i].h, 4000, bpps[b], c);
                CHECK(h == film_frames[i].h, "frame %d (%s): %s %d-bit would be recorded at height %d, menu says %d",
                      i, film_frames[i].name, c ? "lossless" : "uncompressed", bpps[b], h, film_frames[i].h);
            }

    /* the alignment rule itself: result never taller than asked, and uncompressed frames
     * always satisfy the 16-byte rule (width in bytes * height) */
    static const int widths[] = { 640, 960, 1280, 1344, 1600, 1696, 1736, 1920, 2240, 2384, 2560, 2880, 2912, 3072, 3520, 4096 };
    for (int wi = 0; wi < COUNT(widths); wi++)
        for (int b = 0; b < 3; b++)
            for (int h = 100; h <= 1500; h++)
            {
                int r = film_align_height(widths[wi], h, 4000, bpps[b], 0);
                CHECK(r <= h && r > h - 16, "w=%d bpp=%d h=%d -> %d (too far from h)", widths[wi], bpps[b], h, r);
                CHECK((widths[wi] * bpps[b] / 8 * r) % 16 == 0, "w=%d bpp=%d h=%d -> %d breaks the 16-byte rule", widths[wi], bpps[b], h, r);
                int rc = film_align_height(widths[wi], h, 4000, bpps[b], 1);
                CHECK(rc == (h & ~1), "lossless w=%d h=%d -> %d, expected even", widths[wi], h, rc);
            }
    CHECK(film_align_height(1696, 954, 900, 14, 1) == 900, "max height is respected");

    printf("film_tests: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}
