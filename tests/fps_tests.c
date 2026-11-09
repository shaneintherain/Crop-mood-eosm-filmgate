/* Tests for std_fps_snap() in modules/crop_rec/crop_rec.c (exact 18 / 23.976 / 25 / 29.97 fps) -
 * run on a computer, see tests/run.sh */
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ABS(x) ((x) < 0 ? -(x) : (x))
static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

#include "crop_rec_fps_snippet.h"

#define shown_x100 std_fps_shown_x100

int main(void)
{
    /* EOS M preset timers (register values, so A = TimerA + 1, B = TimerB + 1) and RAW_V */
    static const struct { const char * name; int ta, tb, raw_v, want_x100, max_a; int is_1x3; } p[] = {
        { "2.5K 24",          0x2CB, 0x747, 0x455, 2398, 0, 0 },
        { "2.5K 25",          0x2CB, 0x6FA, 0x455, 2500, 0, 0 },
        { "2.5K 30",          0x2CB, 0x5D3, 0x455, 2997, 0, 0 },
        { "2.8K 24",          0x325, 0x676, 0x4D3, 2398, 0, 0 },
        { "2.8K 25",          0x325, 0x633, 0x4D3, 2500, 0, 0 },
        { "3K 24",            0x35B, 0x60F, 0x521, 2398, 0, 0 },
        { "1440p 24",         0x2DB, 0x71E, 0x5BD, 2398, 732, 0 },   /* nothing within 3 %: B only */
        { "1440p 25",         0x2DB, 0x6D3, 0x5BD, 2499, 732, 0 },   /* tight (failed with a longer line): B +1 */
        { "1620p 24",         0x279, 0x838, 0x671, 2398, 0, 0 },
        { "1620p 25",         0x279, 0x7E2, 0x671, 2500, 0, 0 },
        { "1280p 24",         0x235, 0x935, 0x51D, 2398, 0, 0 },
        { "1280p 25",         0x235, 0x8D4, 0x51D, 2500, 0, 0 },
        { "1280p 30",         0x235, 0x75D, 0x51D, 2998, 566, 0 },   /* nothing within 3 %: 29.977 stays */
        { "1280p 18",         0x235, 0xC44, 0x51D, 1800, 0, 0 },
        { "Narrow 24",        0x1F1, 0xA77, 0x51D, 2398, 498, 0 },   /* already exact: untouched */
        { "Narrow 25",        0x1ED, 0xA1E, 0x51D, 2500, 494, 0 },
        { "Narrow 30",        0x1E9, 0x882, 0x51D, 2997, 490, 0 },
        { "Narrow 18",        0x1E9, 0xE2B, 0x51D, 1800, 490, 0 },
        { "1x3 Higher 16:9",  0x1FF, 0xA2E, 0x92F, 2397, 0, 1 },   /* tight: nothing closer within 1 line */
        { "1x3 Medium 25",    0x1FF, 0x9C3, 0x88D, 2500, 512, 1 },
        { "1x3 2.35 Highest", 0x207, 0xA07, 0x8C3, 2397, 0, 1 },   /* tight: 2567 lines, as V8 */
        { "1x3 2.39 Highest", 0x207, 0xA05, 0x89F, 2397, 0, 1 },
        { "1x3 2.35 Medium",  0x1FF, 0xA2D, 0x773, 2398, 0, 1 },
        { "3x3 24",           0x20F, 0x9DE, 0x4A5, 2398, 0, 0 },
        { "3x3 25",           0x27F, 0x7CF, 0x4A5, 2500, 640, 0 },
        { "3x3 30",           0x20F, 0x7E4, 0x4A5, 2997, 0, 0 },
    };
    for (unsigned i = 0; i < sizeof(p) / sizeof(p[0]); i++)
    {
        int a0 = p[i].ta + 1, b0 = p[i].tb + 1, a = a0, b = b0;
        if (!std_fps_snap(a0, b0, p[i].raw_v, !p[i].is_1x3, &a, &b)) { a = a0; b = b0; }
        double fps = 32e6 / ((double)a * b);
        CHECK(shown_x100(a, b) == p[i].want_x100, "%s: %.4f fps shows %d, expected %d", p[i].name, fps, shown_x100(a, b), p[i].want_x100);
        CHECK(a >= a0 && a <= a0 + a0 / 32, "%s: line %d -> %d (never shorter, at most 3 %% longer)", p[i].name, a0, a);
        CHECK((a - a0) % 2 == 0, "%s: Timer A parity changed (%d -> %d)", p[i].name, a0, a);
        if ((b0 - p[i].raw_v) * a0 < STD_FPS_TIGHT_CLOCKS)
            CHECK(a == a0 && ABS(b - b0) <= 1, "%s: tight preset, A %d -> %d, B %d -> %d", p[i].name, a0, a, b0, b);
        else
            CHECK((b - p[i].raw_v) * a >= STD_FPS_TIGHT_CLOCKS, "%s: blanking below 8 ms after the change", p[i].name);
        CHECK(b - p[i].raw_v >= (3 * (b0 - p[i].raw_v)) / 4, "%s: vertical blanking %d -> %d", p[i].name, b0 - p[i].raw_v, b - p[i].raw_v);
        if (p[i].is_1x3) CHECK(a == a0, "%s: 1x3 keeps its line (%d -> %d)", p[i].name, a0, a);
        if (p[i].max_a) CHECK(a <= p[i].max_a, "%s: line %d, expected at most %d", p[i].name, a, p[i].max_a);
        printf("  %-18s %8.4f -> %8.4f fps  A %4d -> %4d  B %4d -> %4d\n", p[i].name, 32e6 / ((double)a0 * b0), fps, a0, a, b0, b);
    }

    /* not near a standard rate: left alone (36 fps, 48 fps, 1x3 Highest 16:9 at 22.26 fps) */
    static const int other[][3] = { { 0x20F, 0x693, 0x4A5 }, { 0x207, 0x501, 0x3DB }, { 0x1FF, 0xAF7, 0xA07 } };
    for (int i = 0; i < 3; i++)
    {
        int a = 0, b = 0;
        CHECK(!std_fps_snap(other[i][0] + 1, other[i][1] + 1, other[i][2], 1, &a, &b), "rate %d is not a standard rate: must stay", i);
    }

    printf("fps_tests: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
