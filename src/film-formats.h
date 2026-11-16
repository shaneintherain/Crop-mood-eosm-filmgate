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
 * Widths are multiples of 16 (8 for the full-width CROP MOOD frames, 1736) and heights
 * are even, so mlv_lite (width rounded down to 8) and film_align_res_y() in
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

#define FILM_FRAME_COUNT   32   /* entries in film_frames[], including OFF at index 0 */
#define FILM_FORMAT_COUNT  13   /* Film Formats in the Movie menu (all three standards) */
#define FILM_FILM_COUNT    6    /* the first six are the FILM standard */
#define FILM_VIDEO_COUNT   5    /* the next five are the VIDEO standard */
#define FILM_CM_FIRST      (FILM_FILM_COUNT + FILM_VIDEO_COUNT)   /* the rest are CROP MOOD */

/* Sensor readouts the formats are cut from (the crop_rec 1:1 / 3x3 modes).  kn/kd is the
 * scale of one readout pixel on the camera's 720x480 LCD layer (used for the black-bar frame). */
enum { FILM_RO_3X3, FILM_RO_3K, FILM_RO_1440, FILM_RO_1280, FILM_RO_25K, FILM_RO_1X3, FILM_RO_1620, FILM_RO_28K, FILM_RO_NARROW, FILM_RO_COUNT };

struct film_readout
{
    int w;                  /* readout width  (pixels) */
    int h;                  /* readout height (pixels) */
    int kn;                 /* LCD scale numerator   (rows -> LCD lines) */
    int kd;                 /* LCD scale denominator */
    int bin_x;              /* columns per recorded pixel: 1 (1:1, 3x3) or 3 (1x3: desqueeze 3x in post) */
    int fit_w;              /* width of the readout's preview on the 720 px LCD layer */
    int dx, dy;             /* recorded window moved from the readout centre (raw pixels, + = right / down),
                             * so the frame sits on the sensor centre; the frame line moves with it */
};

static const struct film_readout film_readouts[FILM_RO_COUNT] __attribute__((unused)) =
{
    /* dx / dy: measured with the V9 framing charts (camera locked off, calibrated on CROP MOOD 3x3 3:2);
     * the readouts' own start rows were also moved (crop_rec cmos_new[7], 120-row steps), dx / dy is
     * the rest.  3x3, 1x3 and 2.8K were centred within the measuring error. */
    { 1736, 1160, 90, 217, 1, 720,   0,   0 },  /* 3x3 3:2   (measured on the camera) */
    { 3072, 1308, 15,  64, 1, 720,   0,   0 },  /* 1:1 3K    (measured) */
    { 2560, 1440,  9,  32, 1, 720,   0,  22 },  /* 1:1 1440p (measured); was 98 rows low, start moved up 120 */
    { 1920, 1280,  3,   8, 1, 720, -38, -20 },  /* 1:1 1280p (measured); was 140 rows low, 38 px right; start up 120 */
    { 2520, 1080,  2,   7, 1, 720,  14, -38 },  /* 1:1 2.5K  (720 / 2520); was 38 rows low, 14 px left */
    { 1736, 2214,  9,  65, 3, 720,   0,   0 },  /* 1x3 2.35:1: squeezed 3x, the camera desqueezes the preview:
                                                 * 5208 columns -> 720 px, 2214 rows -> ~306 lines (centred) */
    { 2160, 1620,  9,  32, 1, 608, -40,  42 },  /* 1:1 4:3 1620p: preview at the 1440p scale, 84 % of the width
                                                 * (crop_rec_preview_fit, LCD and 1080i); was 70 rows low, 40 px
                                                 * right; start up 120 (50 too far), 42 rows is all the room */
    { 2880, 1206,  1,   4, 1, 720, -24,   0 },  /* 1:1 2.8K (Crop Mood "Higher"): 720 / 2880.  It records 1206 rows
                                                 * (not 1226).  Measured 37 px right, 19 rows high; the 2832 wide
                                                 * frame leaves room to move 24 px left (all the room) */
    { 1600, 1280,  3,   8, 1, 720,   8, -20 },  /* 1:1 Narrow (FilmGate): the 1280p rows (same start, same dy).  The
                                                 * columns move only in ~124 px steps (cmos_new[5]); 1536 wide sat
                                                 * ~40 px left (LCD), so 1600 wide (centre 32 px right) and dx 8.
                                                 * Preview stretched to the full width for now (1.2x sideways) */
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
/* w is what the file holds; for 1x3 frames the picture is w * 3 wide after desqueezing
 * (see film_frame_desq_w) */

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
    /* Super 16: the tall frames in 1x3 (1:1 cannot read that many rows at 24 fps), 2.35:1 at 1:1.
     * 1x3 widths: 968 x 3 = 2904 (gate 2912); heights multiples of 8 (uncompressed alignment) */
    { "Super 16 Actual",     968, 1720, "12.52x7.41mm gate (1x3: 2904x1720 desqueezed)", "1x3 2.35:1 1736x2214", "Actual",      FILM_RO_1X3 },
    { "Super 16 16:9 Crop",  968, 1632, "Super 16 gate width, cropped to 16:9 (1x3)",    "1x3 2.35:1 1736x2214", "16:9 Crop",   FILM_RO_1X3 },
    { "Super 16 1.85:1 Crop",968, 1568, "Super 16 gate width, cropped to 1.85:1 (1x3)",  "1x3 2.35:1 1736x2214", "1.85:1 Crop", FILM_RO_1X3 },
    /* 2.35:1 from the 2.8K readout: 2832x1204 (the readout gives 1206 rows; uncompressed needs a multiple
     * of 4).  12.18 mm wide, 97 % of the 12.52 mm gate and 2.5 % narrower than Actual, so the window can
     * move 24 px left to the sensor centre.  24 / 25 fps, 806 clocks per line (3K: 860) */
    { "Super 16 2.35:1 Crop",2832,1204, "Super 16 gate width (97 %), cropped to 2.35:1","1:1 2.35:1 2880x1206 Higher", "2.35:1 Crop", FILM_RO_28K },
    /* 16mm: Actual in 1x3 (792 x 3 = 2376, gate 2386), the crops at 1:1 */
    { "16mm Actual",         792, 1736, "10.26x7.49mm gate (1x3: 2376x1736 desqueezed)", "1x3 2.35:1 1736x2214", "Actual",      FILM_RO_1X3 },
    { "16mm 16:9 Crop",     2384, 1340, "16mm gate width, cropped to 16:9",  "1:1 16:9 2560x1440", "16:9 Crop", FILM_RO_1440 },
    { "16mm 1.85:1 Crop",   2384, 1288, "16mm gate width, cropped to 1.85:1","1:1 16:9 2560x1440", "1.85:1 Crop", FILM_RO_1440 },
    { "16mm 2.35:1 Crop",   2384, 1012, "16mm gate width, cropped to 2.35:1","1:1 16:9 2560x1440", "2.35:1 Crop", FILM_RO_1440 },
    /* Super 8 */
    { "Super 8 Actual",     1344,  930, "5.79x4.01mm gate",                  "1:1 Narrow 1600x1280", "Actual", FILM_RO_NARROW },
    { "Super 8 16:9 Crop",  1344,  756, "Super 8 gate width, cropped to 16:9","1:1 Narrow 1600x1280", "16:9 Crop", FILM_RO_NARROW },
    /* 8mm */
    { "8mm Actual",         1040,  764, "4.5x3.3mm gate",                    "1:1 Narrow 1600x1280", "Actual", FILM_RO_NARROW },
    { "8mm 16:9 Crop",      1040,  584, "8mm gate width, cropped to 16:9",   "1:1 Narrow 1600x1280", "16:9 Crop", FILM_RO_NARROW },
    /* ---- VIDEO standard: video sensor sizes at 1:1 pixel scale (4.30 um per pixel) ---- */
    /* 1" type (13.2 x 8.8 mm, 3:2), in 1x3 (1:1 cannot read that many rows of that width at 24 fps):
     * full width, 1024 x 3 = 3072 (0.01 mm over 13.2 mm) */
    { "1\" Actual",         1024, 2046, "1\" sensor (13.2 x 8.8 mm), 3:2 (1x3: 3072x2046)",            "1x3 2.35:1 1736x2214", "Actual",    FILM_RO_1X3 },
    { "1\" 16:9 Crop",      1024, 1728, "1\" sensor width (13.2 mm), cropped to 16:9 (1x3: 3072x1728)", "1x3 2.35:1 1736x2214", "16:9 Crop", FILM_RO_1X3 },
    /* 2/3" (8.8 x 6.6 mm) */
    { "2/3\" 16:9 Crop",    2032, 1144, "2/3\" sensor width (8.8 mm), cropped to 16:9",   "1:1 16:9 2560x1440",  "16:9 Crop",  FILM_RO_1440 },
    { "2/3\" 4:3",          2032, 1536, "2/3\" sensor (8.8 x 6.6 mm), 4:3",              "1:1 4:3 2160x1620",   "4:3",        FILM_RO_1620 },
    /* 1/2" (6.4 x 4.8 mm) */
    /* both 1/2" frames use the 1280p readout, so the 16:9 window is as wide as the 4:3 one on the screen (it was cut from 2.5K, which previews smaller) */
    { "1/2\" 16:9 Crop",    1472,  828, "1/2\" sensor width (6.4 mm), cropped to 16:9",   "1:1 Narrow 1600x1280", "16:9 Crop", FILM_RO_NARROW },
    { "1/2\" 4:3",          1472, 1104, "1/2\" sensor (6.4 x 4.8 mm), 4:3",              "1:1 Narrow 1600x1280", "4:3", FILM_RO_NARROW },
    /* 1/3" (4.8 x 3.6 mm) */
    { "1/3\" 16:9 Crop",    1104,  620, "1/3\" sensor width (4.8 mm), cropped to 16:9",   "1:1 Narrow 1600x1280", "16:9 Crop", FILM_RO_NARROW },
    { "1/3\" 4:3",          1104,  828, "1/3\" sensor (4.8 x 3.6 mm), 4:3",              "1:1 Narrow 1600x1280", "4:3", FILM_RO_NARROW },
    /* 1/4" (3.6 x 2.7 mm) */
    { "1/4\" 16:9 Crop",     832,  468, "1/4\" sensor width (3.6 mm), cropped to 16:9",   "1:1 Narrow 1600x1280", "16:9 Crop", FILM_RO_NARROW },
    { "1/4\" 4:3",           832,  624, "1/4\" sensor (3.6 x 2.7 mm), 4:3",              "1:1 Narrow 1600x1280", "4:3", FILM_RO_NARROW },
    /* ---- CROP MOOD standard: the legacy Crop Mood modes that span the full sensor width ---- */
    /* 3x3: whole sensor, binned 3x3; frames cut from the 3:2 readout (48 fps: the 738-row one).
     * 1736 wide (the full readout; a multiple of 8, not 16, like the original Crop Mood presets) */
    { "Crop Mood 3x3 3:2",   1736, 1152, "full sensor width, 3x3 binning, 3:2",    "3x3 3:2 1736x1160", "3:2",    FILM_RO_3X3 },
    { "Crop Mood 3x3 16:9",  1736,  976, "full sensor width, 3x3 binning, 16:9",   "3x3 3:2 1736x1160", "16:9",   FILM_RO_3X3 },
    { "Crop Mood 3x3 2.35:1",1736,  736, "full sensor width, 3x3 binning, 2.35:1", "3x3 3:2 1736x1160", "2.35:1", FILM_RO_3X3 },
    /* 1x3: full sensor width, 3 columns binned (desqueeze 3x horizontally: 5208x2214) */
    { "Crop Mood 1x3 2.35:1",1736, 2214, "full sensor width, 1x3 binning (desqueeze 3x to 5208x2214)", "1x3 2.35:1 1736x2214", "2.35:1", FILM_RO_1X3 },
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
    { "S16",            "S16",     6, 4 },
    { "16mm",           "16mm",   10, 4 },
    { "S8",             "S8",     14, 2 },
    { "8mm",            "8mm",    16, 2 },
    /* VIDEO standard */
    { "1\"",            "1\"",    18, 2 },
    { "2/3\"",          "2/3\"",  20, 2 },
    { "1/2\"",          "1/2\"",  22, 2 },
    { "1/3\"",          "1/3\"",  24, 2 },
    { "1/4\"",          "1/4\"",  26, 2 },
    /* CROP MOOD standard */
    { "3x3",            "3x3",    28, 3 },
    { "1x3",            "1x3",    31, 1 },
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

/* picture width after desqueezing (1x3 frames are stored 3x narrower) */
static inline int film_frame_desq_w(int idx)
{
    return film_frames[idx].w * film_readouts[film_frames[idx].readout].bin_x;
}

static inline int film_is_video(int fmt)
{
    return fmt >= FILM_FILM_COUNT && fmt < FILM_CM_FIRST;
}

/* CROP MOOD standard (legacy full-width 3x3 / 1x3 modes) */
static inline int film_is_cm(int fmt)
{
    return fmt >= FILM_CM_FIRST;
}

/* the standard a Film Format belongs to: 0 = FILM, 1 = VIDEO, 2 = CROP MOOD */
static inline int film_standard(int fmt)
{
    return film_is_cm(fmt) ? 2 : film_is_video(fmt) ? 1 : 0;
}

/* first Film Format and number of formats of a standard (0, 1, 2) */
static inline int film_standard_first(int std)
{
    return std == 2 ? FILM_CM_FIRST : std == 1 ? FILM_FILM_COUNT : 0;
}

static inline int film_standard_count(int std)
{
    return std == 2 ? FILM_FORMAT_COUNT - FILM_CM_FIRST : std == 1 ? FILM_VIDEO_COUNT : FILM_FILM_COUNT;
}

/* Next / previous Film Format, staying inside the current standard: FILM formats cycle
 * among themselves, VIDEO sizes among themselves and CROP MOOD modes among themselves, so the quick menu never has to page
 * through all the lists.  dir < 0 steps back, otherwise forward. */
static inline int film_step(int fmt, int dir)
{
    int base = film_standard_first(film_standard(fmt));
    int n = film_standard_count(film_standard(fmt));
    int pos = fmt - base + (dir < 0 ? -1 : 1);
    if (pos < 0) pos = n - 1;
    if (pos >= n) pos = 0;
    return base + pos;
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
