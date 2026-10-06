#include "dryos.h"
#include "bmp.h"
#include "slim-font.h"

#ifdef CONFIG_SLIM_MENUS

static int slim_ui_font_attempted = 0;
static uint32_t slim_ui_font_base = FONT_CANON;

uint32_t slim_ui_font_spec(uint32_t foreground, uint32_t background)
{
    /* Jost Medium is normally already loaded at boot (rbf_init); if not, this
     * loads it on first use.  The font file is packaged in ML/FONTS. */
    if (!slim_ui_font_attempted)
    {
        slim_ui_font_attempted = 1;
        slim_ui_font_base = font_by_name("jost-medium", foreground, background);
    }

    return FONT(slim_ui_font_base, foreground, background);
}

int slim_ui_font_height(void)
{
    return (int)fontspec_font(slim_ui_font_spec(COLOR_WHITE, COLOR_BLACK))->height;
}

#else

uint32_t slim_ui_font_spec(uint32_t foreground, uint32_t background)
{
    return FONT(FONT_CANON, foreground, background);
}

int slim_ui_font_height(void)
{
    return (int)fontspec_font(FONT_CANON)->height;
}

#endif
