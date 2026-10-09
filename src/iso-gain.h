/** \file
 * ISO shown as video-style Gain (VIDEO standard): ISO 100 = 0 dB, every doubling = +6 dB.
 *
 * Header-only and free of camera code, so tests/gain_tests.c can check it on a computer.
 * raw ISO values count 8 per stop and ISO 100 is raw 72 (ISO_100 in lens.h).
 */
#ifndef _ISO_GAIN_H_
#define _ISO_GAIN_H_

#include <stdio.h>

/* gain in whole dB for a raw ISO value (one stop = 6.02 dB, rounded to the nearest dB) */
static inline int iso_gain_db(int raw_iso)
{
    int d = raw_iso - 72;
    int a = d < 0 ? -d : d;
    int db = (a * 6020 / 8 + 500) / 1000;
    return d < 0 ? -db : db;
}

/* "+12dB" / "0dB" / "-2dB"; with_space gives "+12 dB" for the menus */
static inline void iso_gain_text(char * buf, int size, int raw_iso, int with_space)
{
    int db = iso_gain_db(raw_iso);
    snprintf(buf, size, "%s%d%sdB", db > 0 ? "+" : "", db, with_space ? " " : "");
}

#endif
