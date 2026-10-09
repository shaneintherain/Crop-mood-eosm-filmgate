/* Tests for src/iso-gain.h (ISO 100 = 0 dB, +6 dB per stop) - run on a computer, see tests/run.sh */
#include <stdio.h>
#include <string.h>
#include "iso-gain.h"

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    /* the full-stop ISOs of the Expo menu (raw: 72, 80, 88 ... 120) */
    static const struct { int raw; int iso; const char * text; const char * spaced; } stops[] = {
        {  72,  100, "0dB",   "0 dB"   },
        {  80,  200, "+6dB",  "+6 dB"  },
        {  88,  400, "+12dB", "+12 dB" },
        {  96,  800, "+18dB", "+18 dB" },
        { 104, 1600, "+24dB", "+24 dB" },
        { 112, 3200, "+30dB", "+30 dB" },
        { 120, 6400, "+36dB", "+36 dB" },
    };
    for (int i = 0; i < 7; i++)
    {
        char b[32];
        iso_gain_text(b, sizeof(b), stops[i].raw, 0);
        CHECK(!strcmp(b, stops[i].text), "ISO %d: '%s', expected '%s'", stops[i].iso, b, stops[i].text);
        iso_gain_text(b, sizeof(b), stops[i].raw, 1);
        CHECK(!strcmp(b, stops[i].spaced), "ISO %d: '%s', expected '%s'", stops[i].iso, b, stops[i].spaced);
        CHECK(iso_gain_db(stops[i].raw) == 6 * i, "ISO %d must be %d dB", stops[i].iso, 6 * i);
    }

    /* third-stop ISOs: 125 = +2, 160 = +4, 250 = +8, 320 = +10, 500 = +14, 640 = +16, 1000 = +20 ... */
    static const int thirds[][2] = { {75,2}, {77,4}, {83,8}, {85,10}, {91,14}, {93,16}, {99,20}, {101,22}, {107,26}, {109,28}, {115,32}, {117,34} };
    for (int i = 0; i < 12; i++)
        CHECK(iso_gain_db(thirds[i][0]) == thirds[i][1], "raw %d: %d dB, expected %d", thirds[i][0], iso_gain_db(thirds[i][0]), thirds[i][1]);

    /* never decreasing as the ISO goes up, and within 1 dB of the exact value 20*log10(iso/100) */
    int prev = -1000;
    for (int raw = 72; raw <= 144; raw++)
    {
        int db = iso_gain_db(raw);
        CHECK(db >= prev, "gain goes down at raw %d", raw);
        prev = db;
        double exact = (raw - 72) * 6.0206 / 8;
        CHECK(db - exact <= 0.51 && exact - db <= 0.51, "raw %d: %d dB vs exact %.2f", raw, db, exact);
    }
    char b[32];
    iso_gain_text(b, sizeof(b), 64, 0);
    CHECK(!strcmp(b, "-6dB"), "below ISO 100 is negative: '%s'", b);

    printf("gain_tests: %d checks, %d failed\n", checks, failures);
    return failures != 0;
}
