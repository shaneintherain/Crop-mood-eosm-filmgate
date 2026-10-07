/** \file
 * Range check for saved settings (module config files).
 *
 * A module lists each of its saved settings once, with the range of values the menus can
 * really produce.  settings_check() runs at start-up, right after the config file has been
 * loaded: anything outside its range (damaged file, hand-edited value, a leftover from an
 * older build) is reset to a safe value before any code uses it.  This matters most for
 * settings that are used as an index into a table.
 *
 * Header-only, no camera code, so tests/settings_tests.c can run it on a computer.
 */
#ifndef _SETTINGS_CHECK_H_
#define _SETTINGS_CHECK_H_

struct setting_range
{
    int * var;      /* the saved setting */
    int min;        /* lowest valid value */
    int max;        /* highest valid value */
    int fix;        /* value to use if it is outside min..max (normally the default) */
};

#define SETTING(var, min, max, fix)  { &(var), (min), (max), (fix) }

/* Resets every out-of-range setting.  Returns how many were reset. */
static inline int settings_check(const struct setting_range * table, int count)
{
    int reset = 0;
    for (int i = 0; i < count; i++)
    {
        int v = *table[i].var;
        if (v < table[i].min || v > table[i].max)
        {
            *table[i].var = table[i].fix;
            reset++;
        }
    }
    return reset;
}

#endif
