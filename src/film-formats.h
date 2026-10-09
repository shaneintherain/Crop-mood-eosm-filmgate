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

#define FILM_FRAME_COUNT   25   /* entries in film_frames[], including OFF at index 0 */
#define FILM_FORMAT_COUNT  11   /* Film Formats in the Movie menu (both standards) */
#define FILM_FILM_COUNT    6    /* the first six are the FILM standard; the rest are VIDEO */

/* Sensor readouts the formats are cut from (the crop_rec 1:1 / 3x3 modes).  kn/kd is the
 * scale of one readout pixel on the camera's 720x480 LCD layer (used for the black-bar frame). */
enum { FILM_RO_3X3, FILM_RO_3K, FILM_RO_1440, FILM_RO_1280, FILM_RO_25K, FILM_RO_1620, FILM_RO_COUNT };

struct film_readout
{
    int w;                  /* readout width  (pixels) */
    int h;                  /* readout height (pixels) */
    int kn;                 /* LCD scale numerator   */
    int kd;                 /* LCD scale denominator */
};

static const struct film_readout film_readouts[FILM_RO_COUNT] __attribute__((unused)) =
{
    { 1736, 1160, 90, 217 },    /* 3x3 3:2   (measured on the camera) */
    { 3072, 1308, 15,  64 },    /* 1:1 3K    (measured) */
    { 2560, 1440,  9,  32 },    /* 1:1 1440p (measured) */
    { 1920, 1280,  3,   8 },    /* 1:1 1280p (measured) */
    { 2520, 1080,  2,   7 },    /* 1:1 2.5K  (720 / 2520; not yet measured) */
    { 2160, 1620, 437, 1620 },  /* 1:1 1620p (measured: the LCD stretches this one horizontally) */
};

/* One recording window.  Index 0 is OFF (no film window). */
struct film_frame
{
    const char * name;      /* full name (mlv_lite help text) */
    int w;                  /* recorded width  (pixels) */
    int h;                  /* recorded height (pixels) */
    const char * gate;      /* physical size, for the help text */
    const char * mode;      /* crop_rec 1:1 mode needed */
    const char * frame;     /* text of the Movie menu "Frame" row */
    int readout;            /* FILM_RO_*: the sensor readout it is cut from */
};

static const struct film_frame film_frames[FILM_FRAME_COUNT] __attribute__((unused)) =
{
    { "OFF",                   0,    0, "",                                  "", "", 0 },
    /* Academy 35 (3x3 readout) */
    { "A35 16:9 Crop",      1696,  954, "Academy 35mm gate width, cropped to 16:9",   "3x3 3:2 1736x1160", "16:9 Crop", FILM_RO_3X3 },
    { "A35 1.85:1 Crop",    1696,  916, "Academy 35mm gate width, cropped to 1.85:1", "3x3 3:2 1736x1160", "1.85:1 Crop", FILM_RO_3X3 },
    { "A35 2.35:1 Crop",    1696,  722, "Academy 35mm gate width, cropped to 2.35:1", "3x3 3:2 1736x1160", "2.35:1 Crop", FILM_RO_3X3 },
    /* Academy 35 anamorphic (same readout, squeezed windows) */
    { "A35 Anamorphic 2x",  1376, 1152, "2x anamorphic gate (1.18:1), full sensor height", "3x3 3:2 1736x1160", "2x 1.18:1", FILM_RO_3X3 },
    { "A35 Anamorphic 1.33x",1536,1152, "1.33x anamorphic gate (4:3)",       "3x3 3:2 1736x1160", "1.33x 4:3", FILM_RO_3X3 },
    /* Super 16 */
    { "Super 16 2.35:1 Crop",2912,1238, "Super 16 gate width, cropped to 2.35:1","1:1 2.35:1 3072x1308 Highest", "2.35:1 Crop", FILM_RO_3K },
    /* 16mm */
    { "16mm 16:9 Crop",     2384, 1340, "16mm gate width, cropped to 16:9",  "1:1 16:9 2560x1440", "16:9 Crop", FILM_RO_1440 },
    { "16mm 1.85:1 Crop",   2384, 1288, "16mm gate width, cropped to 1.85:1","1:1 16:9 2560x1440", "1.85:1 Crop", FILM_RO_1440 },
    { "16mm 2.35:1 Crop",   2384, 1012, "16mm gate width, cropped to 2.35:1","1:1 16:9 2560x1440", "2.35:1 Crop", FILM_RO_1440 },
    /* Super 8 */
    { "Super 8 Actual",     1344,  930, "5.79x4.01mm gate",                  "1:1 3:2 1920x1280", "Actual", FILM_RO_1280 },
    { "Super 8 16:9 Crop",  1344,  756, "Super 8 gate width, cropped to 16:9","1:1 3:2 1920x1280", "16:9 Crop", FILM_RO_1280 },
    /* 8mm */
    { "8mm Actual",         1040,  764, "4.5x3.3mm gate",                    "1:1 3:2 1920x1280", "Actual", FILM_RO_1280 },
    { "8mm 16:9 Crop",      1040,  584, "8mm gate width, cropped to 16:9",   "1:1 3:2 1920x1280", "16:9 Crop", FILM_RO_1280 },
    /* ---- VIDEO standard: video sensor sizes at 1:1 pixel scale (4.30 um per pixel) ---- */
    /* 2/3" (8.8 x 6.6 mm) */
    { "2/3\" 16:9 Crop",    2032, 1144, "2/3\" sensor width (8.8 mm), cropped to 16:9",   "1:1 16:9 2560x1440",  "16:9 Crop",  FILM_RO_1440 },
    { "2/3\" 4:3",          2032, 1524, "2/3\" sensor (8.8 x 6.6 mm), 4:3",              "1:1 4:3 2160x1620",   "4:3", FILM_RO_1620 },
    { "2/3\" 1.85:1 Crop",  1984, 1072, "2/3\" sensor width, cropped to 1.85:1",         "1:1 2.33:1 2520x1080", "1.85:1 Crop", FILM_RO_25K },
    /* 1/2" (6.4 x 4.8 mm) */
    { "1/2\" 16:9 Crop",    1472,  828, "1/2\" sensor width (6.4 mm), cropped to 16:9",   "1:1 2.33:1 2520x1080", "16:9 Crop", FILM_RO_25K },
    { "1/2\" 4:3",          1472, 1104, "1/2\" sensor (6.4 x 4.8 mm), 4:3",              "1:1 3:2 1920x1280",   "4:3", FILM_RO_1280 },
    /* 1/2.3" (6.17 x 4.55 mm) */
    { "1/2.3\" 16:9 Crop",  1424,  800, "1/2.3\" sensor width (6.17 mm), cropped to 16:9", "1:1 2.33:1 2520x1080", "16:9 Crop", FILM_RO_25K },
    { "1/2.3\" 4:3",        1424, 1056, "1/2.3\" sensor (6.17 x 4.55 mm), 4:3",          "1:1 2.33:1 2520x1080", "4:3", FILM_RO_25K },
    /* 1/3" (4.8 x 3.6 mm) */
    { "1/3\" 16:9 Crop",    1104,  620, "1/3\" sensor width (4.8 mm), cropped to 16:9",   "1:1 2.33:1 2520x1080", "16:9 Crop", FILM_RO_25K },
    { "1/3\" 4:3",          1104,  828, "1/3\" sensor (4.8 x 3.6 mm), 4:3",              "1:1 2.33:1 2520x1080", "4:3", FILM_RO_25K },
    /* 1/4" (3.6 x 2.7 mm) */
    { "1/4\" 16:9 Crop",     832,  468, "1/4\" sensor width (3.6 mm), cropped to 16:9",   "1:1 2.33:1 2520x1080", "16:9 Crop", FILM_RO_25K },
    { "1/4\" 4:3",           832,  624, "1/4\" sensor (3.6 x 2.7 mm), 4:3",              "1:1 2.33:1 2520x1080", "4:3", FILM_RO_25K },
};

/* The Film Formats: the first FILM_FILM_COUNT are the FILM standard, the rest the VIDEO
 * standard (video sensor sizes).  Each owns 'count' consecutive entries of film_frames[]
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
    /* VIDEO standard */
    { "2/3\"",          "2/3\"",  14, 3 },
    { "1/2\"",          "1/2\"",  17, 2 },
    { "1/2.3\"",        "1/2.3\"",19, 2 },
    { "1/3\"",          "1/3\"",  21, 2 },
    { "1/4\"",          "1/4\"",  23, 2 },
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

static inline int film_is_video(int fmt)
{
    return fmt >= FILM_FILM_COUNT;
}

/* does a Film Format have a Frame choice cut from this readout? */
static inline int film_format_has_readout(int fmt, int readout)
{
    for (int i = 0; i < film_formats[fmt].count; i++)
        if (film_frames[film_formats[fmt].first + i].readout == readout)
            return 1;
    return 0;
}

/* Which Film Format and Frame does a sensor readout mean?
 *   readout  FILM_RO_*, or -1 when the camera is in some other mode
 *   cur_fmt / cur_frame  the saved choice: kept whenever it can use this readout, so
 *            readouts shared by several formats (A35 / A35 Anamorphic, S8 / 8mm, the
 *            video sizes) remember which one was picked
 * Returns the Film Format (or -1) and stores the Frame choice in *frame_out. */
static inline int film_pick(int readout, int cur_fmt, int cur_frame, int * frame_out)
{
    int fmt = -1;

    *frame_out = 0;
    if (readout < 0 || readout >= FILM_RO_COUNT)
        return -1;

    if (cur_fmt >= 0 && cur_fmt < FILM_FORMAT_COUNT && film_format_has_readout(cur_fmt, readout))
        fmt = cur_fmt;
    else
        for (int i = 0; i < FILM_FORMAT_COUNT && fmt < 0; i++)
            if (film_format_has_readout(i, readout))
                fmt = i;

    if (fmt < 0)
        return -1;

    if (fmt == cur_fmt && cur_frame >= 0 && cur_frame < film_formats[fmt].count &&
        film_frames[film_formats[fmt].first + cur_frame].readout == readout)
    {
        *frame_out = cur_frame;
        return fmt;
    }

    for (int i = 0; i < film_formats[fmt].count; i++)
    {
        if (film_frames[film_formats[fmt].first + i].readout == readout)
        {
            *frame_out = i;
            break;
        }
    }
    return fmt;
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
