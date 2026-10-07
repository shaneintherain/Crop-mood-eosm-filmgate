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
    }

    /* frames of one format share the readout and the width (only the height / crop differs) */
    for (int f = 0; f < FILM_FORMAT_COUNT; f++)
        for (int k = 1; k < film_formats[f].count; k++)
        {
            const struct film_frame * a = &film_frames[film_formats[f].first];
            const struct film_frame * b = &film_frames[film_formats[f].first + k];
            CHECK(!strcmp(a->mode, b->mode), "format %s: frames use different readouts", film_formats[f].name);
            if (f != 1) /* anamorphic: 2x and 1.33x have different squeezed widths */
                CHECK(a->w == b->w, "format %s: frames have different widths", film_formats[f].name);
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
