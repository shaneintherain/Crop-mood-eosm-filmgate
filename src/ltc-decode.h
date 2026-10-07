/*
 * Linear timecode (LTC / SMPTE) decoder for a mono stream of 16-bit audio samples.
 *
 * Pure C with no dependencies, so it can be tested on a computer.  Feed it the samples
 * of one channel; whenever a complete 80-bit LTC frame has been read it updates the
 * timecode fields.  Handles either polarity, DC offset and any frame rate; the bit
 * period is learned from the signal itself.
 *
 * Forward-running timecode only.
 */
#ifndef LTC_DECODE_H
#define LTC_DECODE_H

#include <stdint.h>

typedef struct
{
    /* configuration */
    int rate;                   /* sample rate, Hz */

    /* signal tracking */
    int peak;                   /* slowly decaying peak of |sample| */
    int state;                  /* +1 / -1: which side of zero we are on, 0 = unknown */
    int since;                  /* samples since the last zero crossing */
    int bitlen_x16;             /* bit period in samples, x16 */
    int half;                   /* a short (half-bit) interval is waiting for its partner */
    uint8_t bits[80];           /* the last 80 bits, oldest first */

    /* results */
    int h, m, s, f;             /* last complete timecode */
    int drop;                   /* drop-frame flag */
    uint32_t frames_ok;         /* number of valid frames decoded since init */
    uint32_t total;             /* samples fed since init */
    uint32_t last_ok;           /* value of 'total' when the last valid frame completed */
} ltc_t;

static inline void ltc_init(ltc_t * t, int rate, int fps_x1000)
{
    uint8_t * p = (uint8_t *) t;
    for (unsigned i = 0; i < sizeof(*t); i++) p[i] = 0;
    t->rate = rate;
    if (fps_x1000 < 8000 || fps_x1000 > 60000) fps_x1000 = 24000;
    /* samples per bit = rate / (fps * 80), kept x16: rate * 16 * 1000 / (fps_x1000 * 80) */
    t->bitlen_x16 = rate * 200 / fps_x1000;
}

/* one complete 80-bit frame is in t->bits: pick it apart */
static inline void ltc_frame_done(ltc_t * t)
{
    const uint8_t * b = t->bits;
    static const uint8_t sync[16] = { 0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,1 };
    for (int i = 0; i < 16; i++)
        if (b[64 + i] != sync[i])
            return;

    int f = b[0] + 2*b[1] + 4*b[2] + 8*b[3] + 10 * (b[8] + 2*b[9]);
    int s = b[16] + 2*b[17] + 4*b[18] + 8*b[19] + 10 * (b[24] + 2*b[25] + 4*b[26]);
    int m = b[32] + 2*b[33] + 4*b[34] + 8*b[35] + 10 * (b[40] + 2*b[41] + 4*b[42]);
    int h = b[48] + 2*b[49] + 4*b[50] + 8*b[51] + 10 * (b[56] + 2*b[57]);

    if (f > 29 || s > 59 || m > 59 || h > 23)
        return;     /* the sync word matched by chance: not a real frame */

    t->f = f; t->s = s; t->m = m; t->h = h;
    t->drop = b[10];
    t->frames_ok++;
    t->last_ok = t->total;
}

static inline void ltc_push_bit(ltc_t * t, int bit)
{
    for (int i = 0; i < 79; i++)
        t->bits[i] = t->bits[i + 1];
    t->bits[79] = bit;
    ltc_frame_done(t);
}

/* one zero crossing, 'd' samples after the previous one */
static inline void ltc_interval(ltc_t * t, int d)
{
    int d16 = d * 16;
    int bl = t->bitlen_x16;

    if (d16 > bl * 5 / 2 || d16 < bl / 5)
    {
        /* far too long or too short to be part of the signal: forget what we had */
        t->half = 0;
        return;
    }

    if (d16 * 4 > bl * 3)
    {
        /* long interval = a 0 bit */
        t->half = 0;
        t->bitlen_x16 += (d16 - bl) / 8;
        ltc_push_bit(t, 0);
    }
    else if (t->half)
    {
        /* second short interval = a 1 bit */
        t->half = 0;
        t->bitlen_x16 += (2 * d16 - bl) / 16;
        ltc_push_bit(t, 1);
    }
    else
    {
        t->half = 1;
    }
}

/* feed 'n' samples; sample i is s[i * stride] (stride 2 = left or right of a stereo buffer) */
static inline void ltc_feed(ltc_t * t, const int16_t * s, int n, int stride)
{
    for (int i = 0; i < n; i++)
    {
        int x = s[i * stride];
        int a = x < 0 ? -x : x;

        /* peak follower: instant attack, decay time of about 0.1 s at 48 kHz */
        if (a > t->peak) t->peak = a;
        else t->peak -= (t->peak >> 12) + (t->peak > 0);
        if (t->peak < 0) t->peak = 0;

        t->total++;
        t->since++;

        /* no usable signal: stay quiet and wait */
        if (t->peak < 120)
        {
            t->state = 0;
            t->half = 0;
            t->since = 0;
            continue;
        }

        int thr = t->peak / 4;
        int ns = t->state;
        if (x > thr) ns = 1;
        else if (x < -thr) ns = -1;

        if (ns != t->state)
        {
            if (t->state != 0)
                ltc_interval(t, t->since);
            t->state = ns;
            t->since = 0;
        }
    }
}

/* did a valid frame complete within the last 'ms' milliseconds of audio? */
static inline int ltc_recent(const ltc_t * t, int ms)
{
    return t->frames_ok && (t->total - t->last_ok) < (uint32_t)(t->rate / 1000 * ms);
}

#endif
