/** \file
 * Film Formats for the EOS M: the one table that crop_rec and mlv_lite both read.
 *
 * Recording windows that match real film gates at 1:1 pixel scale on the EOS M sensor
 * (22.3 mm / 5184 px = ~4.30 um per pixel).
 *   "Actual" = the true physical gate size at 1:1.
 *   "Crop"   = a smaller window, because the true gate would need more pixels than the
 *              camera can read out in a stable video mode.
 * The window is centered inside the current crop_rec readout (see the 'mode' column).
 *
 * Widths are multiples of 16 and heights are even, so film_align_res_y() in
 * mlv_lite leaves them unchanged and the "Recorded Size" shown in the Movie menu is
 * exactly what is written.  tests/film_tests.c checks this (and the rest of this table)
 * on a computer.
 *
 * To change a size or a name: edit it here, once.  Nothing else needs to match.
 *
 * Header-only (static const data), so each module gets its own private copy.
 */
#ifndef _FILM_FORMATS_H_
#define _FILM_FORMATS_H_

#define FILM_FRAME_COUNT   14   /* entries in film_frames[], including OFF at index 0 */
#define FILM_FORMAT_COUNT  6    /* Film Formats in the Movie menu */

/* One recording window.  Index 0 is OFF (no film window). */
struct film_frame
{
    const char * name;      /* full name (mlv_lite help text) */
    int w;                  /* recorded width  (pixels) */
    int h;                  /* recorded height (pixels) */
    const char * gate;      /* physical size, for the help text */
    const char * mode;      /* crop_rec 1:1 mode needed */
    const char * frame;     /* text of the Movie menu "Frame" row */
};

static const struct film_frame film_frames[FILM_FRAME_COUNT] __attribute__((unused)) =
{
    { "OFF",                   0,    0, "",                                  "", "" },
    /* Academy 35 (3x3 readout) */
    { "A35 16:9 Crop",      1696,  954, "Academy 35mm gate width, cropped to 16:9",   "3x3 3:2 1736x1160", "16:9 Crop" },
    { "A35 1.85:1 Crop",    1696,  916, "Academy 35mm gate width, cropped to 1.85:1", "3x3 3:2 1736x1160", "1.85:1 Crop" },
    { "A35 2.35:1 Crop",    1696,  722, "Academy 35mm gate width, cropped to 2.35:1", "3x3 3:2 1736x1160", "2.35:1 Crop" },
    /* Academy 35 anamorphic (same readout, squeezed windows) */
    { "A35 Anamorphic 2x",  1376, 1152, "2x anamorphic gate (1.18:1), full sensor height", "3x3 3:2 1736x1160", "2x 1.18:1" },
    { "A35 Anamorphic 1.33x",1536,1152, "1.33x anamorphic gate (4:3)",       "3x3 3:2 1736x1160", "1.33x 4:3" },
    /* Super 16 */
    { "Super 16 2.35:1 Crop",2912,1238, "Super 16 gate width, cropped to 2.35:1","1:1 2.35:1 3072x1308 Highest", "2.35:1 Crop" },
    /* 16mm */
    { "16mm 16:9 Crop",     2384, 1340, "16mm gate width, cropped to 16:9",  "1:1 16:9 2560x1440", "16:9 Crop" },
    { "16mm 1.85:1 Crop",   2384, 1288, "16mm gate width, cropped to 1.85:1","1:1 16:9 2560x1440", "1.85:1 Crop" },
    { "16mm 2.35:1 Crop",   2384, 1012, "16mm gate width, cropped to 2.35:1","1:1 16:9 2560x1440", "2.35:1 Crop" },
    /* Super 8 */
    { "Super 8 Actual",     1344,  930, "5.79x4.01mm gate",                  "1:1 3:2 1920x1280", "Actual" },
    { "Super 8 16:9 Crop",  1344,  756, "Super 8 gate width, cropped to 16:9","1:1 3:2 1920x1280", "16:9 Crop" },
    /* 8mm */
    { "8mm Actual",         1040,  764, "4.5x3.3mm gate",                    "1:1 3:2 1920x1280", "Actual" },
    { "8mm 16:9 Crop",      1040,  584, "8mm gate width, cropped to 16:9",   "1:1 3:2 1920x1280", "16:9 Crop" },
};

/* The six Film Formats.  Each owns 'count' consecutive entries of film_frames[]
 * starting at 'first' (its "Frame" choices). */
struct film_format
{
    const char * name;      /* Movie menu "Film Format" row */
    const char * label;     /* bottom bar */
    int first;              /* index of its first frame in film_frames[] (never 0) */
    int count;              /* number of Frame choices */
};

static const struct film_format film_formats[FILM_FORMAT_COUNT] __attribute__((unused)) =
{
    { "Academy 35mm",   "A35",     1, 3 },
    { "A35 Anamorphic", "A35-ANA", 4, 2 },
    { "S16",            "S16",     6, 1 },
    { "16mm",           "16mm",    7, 3 },
    { "S8",             "S8",     10, 2 },
    { "8mm",            "8mm",    12, 2 },
};

/* index into film_frames[] for a Film Format and a Frame choice (out-of-range input is clamped) */
static inline int film_frame_index(int fmt, int frame)
{
    if (fmt < 0) fmt = 0;
    if (fmt >= FILM_FORMAT_COUNT) fmt = FILM_FORMAT_COUNT - 1;
    if (frame < 0) frame = 0;
    if (frame >= film_formats[fmt].count) frame = film_formats[fmt].count - 1;
    return film_formats[fmt].first + frame;
}

/* Height rule for a recording window w pixels wide (bpp = bits per pixel in the file):
 * lossless frames only need an even height; uncompressed frames must be a multiple of
 * 16 bytes (the EDMAC rule), which fixes the height step from the width in bytes.
 * Used by mlv_lite for every window; every entry of film_frames[] is already aligned,
 * so for the film formats this returns h unchanged (tests/film_tests.c checks that). */
static inline int film_align_height(int w, int h, int max_h, int bpp, int compressed)
{
    if (h > max_h)
        h = max_h;

    if (compressed)
        return h & ~1;

    switch ((w * bpp / 8) % 8)
    {
        case 0:  return h & ~1;
        case 4:  return h & ~3;
        case 2:
        case 6:  return h & ~7;
        default: return h & ~15;
    }
}

#endif
