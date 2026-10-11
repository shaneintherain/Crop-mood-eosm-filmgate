#include <dryos.h>
#include <module.h>
#include <config.h>
#include <menu.h>
#include <menu-grid.h>
#include <beep.h>
#include <property.h>
#include <patch.h>
#include <bmp.h>
#include <lvinfo.h>
#include <powersave.h>
#include <raw.h>
#include <fps.h>
#include <shoot.h>
#include <lens.h>
#include <focus.h>
#include <vram.h>
#include "../mlv_lite/mlv_lite.h"
#include <film-formats.h>
#include <settings-check.h>
#include "../dual_iso/dual_iso.h"
#include "histogram.h"

#undef CROP_DEBUG

#ifdef CROP_DEBUG
#define dbg_printf(fmt,...) { printf(fmt, ## __VA_ARGS__); }
#else
#define dbg_printf(fmt,...) {}
#endif


int YUV_HD_S_H_height = 0;
int YUV_HD_S_H_width = 0;
int YUV_HD_S_V_height = 0;
int YUV_HD_S_V_width = 0;
int reg_skip_left = 0;
int reg_skip_right = 0;
int reg_skip_top = 0;
int reg_skip_bottom = 0;
int reg_cmos5 = 0;
int reg_cmos7 = 0;
int reg_height = 0;
int reg_width = 0;
int reg_Preview_H = 0;
int reg_Preview_V = 0;
int reg_YUV_HD_S_H = 0;
int reg_YUV_HD_S_V = 0;

static int zoom = 0;
static int submenu = 0;

static int is_DIGIC_5 = 0;
static int is_EOSM = 0;

static CONFIG_INT("crop.fps_over", fps_over, 0);
static CONFIG_INT("crop.tapdisp", tapdisp, 1);
/* Own key: this used to share "crop.preset_fps" with crop_preset_fps_menu, so each saved
 * value was loaded into both variables (an 18 fps choice came back after every reboot). */
static CONFIG_INT("crop.preset_fps_reduce", crop_preset_fps_reduce, 1);
static CONFIG_INT("crop.preset", crop_preset_index, 3); /* default: 3x3 = S35 */
static CONFIG_INT("crop.shutter_range", shutter_range, 0);
static CONFIG_INT("crop.fix_dual_iso_flicker", fix_dual_iso_flicker, 1);

CONFIG_INT("crop.bit_depth", bit_depth_analog, 1);
#define OUTPUT_14BIT (bit_depth_analog == 0)
#define OUTPUT_12BIT (bit_depth_analog == 1)
#define OUTPUT_11BIT (bit_depth_analog == 2)
#define OUTPUT_10BIT (bit_depth_analog == 3)

// check raw.c
extern int BitDepth_Analog;

static CONFIG_INT("crop.brighten_lv", brighten_lv_method, 0);

static CONFIG_INT("crop.preset_aspect_ratio", crop_preset_ar_menu, 4); /* default: 3:2 readout = S35 */
static int crop_preset_ar = 0;
#define AR_16_9        (crop_preset_ar == 0)
#define AR_2_1         (crop_preset_ar == 1)
#define AR_2_20_1      (crop_preset_ar == 2)
#define AR_2_35_1      (crop_preset_ar == 3)
#define AR_2_39_1      (crop_preset_ar == 4)

static CONFIG_INT("crop.preset_1x1", crop_preset_1x1_res_menu, 3);
static int crop_preset_1x1_res = 0;
#define CROP_2_5K      (crop_preset_1x1_res == 0)
#define CROP_2_8K      (crop_preset_1x1_res == 1)
#define CROP_3K        (crop_preset_1x1_res == 2)
#define CROP_1440p     (crop_preset_1x1_res == 3)
#define CROP_1280p     (crop_preset_1x1_res == 4)
#define CROP_Full_Res  (crop_preset_1x1_res == 5)
#define CROP_1620p     (crop_preset_1x1_res == 6)
#define CROP_1080p     (crop_preset_1x1_res == 7)

CONFIG_INT("crop.preset_1x3", crop_preset_1x3_res_menu, 1);
static int crop_preset_1x3_res = 0;
#define Anam_Highest   (crop_preset_1x3_res == 0)
#define Anam_Higher    (crop_preset_1x3_res == 1)
#define Anam_Medium    (crop_preset_1x3_res == 2)
#define Anam_FLV    (crop_preset_1x3_res == 3)

static CONFIG_INT("crop.preset_3x3", crop_preset_3x3_res_menu, 2); /* default: mv1080 3:2 = S35 */
static int crop_preset_3x3_res = 0;
#define High_FPS       (crop_preset_3x3_res == 0)
#define mv1080         (crop_preset_3x3_res == 1)
#define mv1080_3_2        (crop_preset_3x3_res == 2)

static CONFIG_INT("crop.preset_fps", crop_preset_fps_menu, 0);
static int crop_preset_fps = 0;
#define Framerate_24   (crop_preset_fps == 0)
#define Framerate_25   (crop_preset_fps == 1)
#define Framerate_30   (crop_preset_fps == 2)
#define Framerate_18   (crop_preset_fps == 3)   /* slim 1:1 3:2 (S8 / 8mm) only */

/* customized buttons variables
 * EOS M slim defaults:
 *   SET = Zoom x10; U/D = ISO; L/R = Aperture; INFO off.
 * Arrow modes: 0=OFF, 1=Shutter, 2=Aperture, 3=ISO
 * INFO modes:  0=OFF, 1=Dual ISO, 2=Histogram, 3=Waveform, 4=Zebras,
 *              5=False Color, 6=framing, 7=Quick Panel
 */
CONFIG_INT("crop.button_SET",       SET_button, 1);
static CONFIG_INT("crop.button_H-Shutter", Half_Shutter, 2);
CONFIG_INT("crop.button_INFO",      INFO_button, 6);  /* 6 = Quick Panel (EOS M default) */
static CONFIG_INT("crop.shutter_rec", Shutter_rec, 0); /* EOS M slim: 0=OFF, 1=half-press starts/stops recording */
CONFIG_INT("crop.shutter_zoom", Shutter_zoom, 0); /* EOS M slim: 0=OFF, 1=hold x10, 2=sticky x10 */
CONFIG_INT("crop.arrows_U_D",       Arrows_U_D, 3); /* ISO */
CONFIG_INT("crop.more_hacks",       more_hacks, 1);
static CONFIG_INT("crop.arrows_L_R",       Arrows_L_R, 2); /* Aperture */
/* Settings version.  Keeps the old key name so existing config files keep their number
 * (the button conversions below must run only once).  See crop_settings_load(). */
static CONFIG_INT("crop.button_map_v",     crop_settings_ver, 0);

enum crop_preset {
    CROP_PRESET_OFF = 0,
    CROP_PRESET_3K,     /* only used to pick a row of max_resolutions[] (see crop_preset_yres_lookup) */

    /* these are for EOS M */
    CROP_PRESET_1X1,
    CROP_PRESET_1X3,
    CROP_PRESET_3X3,
    NUM_CROP_PRESETS
};

/* presets are not enabled right away (we need to go to play mode and back)
 * so we keep two variables: what's selected in menu and what's actually used.
 * note: the menu choices are camera-dependent */
static enum crop_preset crop_preset = 0;

/* must be assigned in crop_rec_init */
static enum crop_preset * crop_presets = 0;

/* current menu selection (*/
#define CROP_PRESET_MENU crop_presets[crop_preset_index]

/* menu choices for entry level DIGIC 5 models, EOS M */
static enum crop_preset crop_presets_DIGIC_5[] = {
    CROP_PRESET_OFF,
    CROP_PRESET_1X1,
    CROP_PRESET_1X3,
    CROP_PRESET_3X3,
};

static const char * crop_choices_DIGIC_5[] = {
    "OFF",
    "1:1 crop",
    "1x3",
    "3x3",
};

static const char crop_choices_help_DIGIC_5[] =
    "Turn your camera into crop moods (select one)\n"
    "Center crop on sensor, no pixel binning/skipping in this mode.\n"
    "a.k.a Anamorphic, reads all vertical pixels, reduces aliasing.\n"
    "1080p mode and experimental High Framerate options.\n";
    
    
/* camera-specific parameters */
static uint32_t CMOS_WRITE               = 0;
static uint32_t MEM_CMOS_WRITE           = 0;
static uint32_t ADTG_WRITE               = 0;
static uint32_t MEM_ADTG_WRITE           = 0;
static uint32_t ENGIO_WRITE              = 0;
static uint32_t MEM_ENGIO_WRITE          = 0;
static uint32_t ENG_DRV_OUT              = 0;
static uint32_t ENG_DRV_OUTS             = 0;
static uint32_t PATH_SelectPathDriveMode = 0;

/* from SENSOR_TIMING_TABLE (fps-engio.c) or FPS override submenu */
static int fps_main_clock = 0;
static int default_timerA[11]; /* 1080p  1080p  1080p   720p   720p   zoom   crop   crop   crop   crop   crop */
static int default_timerB[11]; /*   24p    25p    30p    50p    60p     x5    24p    25p    30p    50p    60p */
static int default_fps_1k[11] = { 23976, 25000, 29970, 50000, 59940, 29970, 23976, 25000, 29970, 50000, 59940 };

/* video modes */

/* properties are fired AFTER the new video mode is fully up and running
 * to apply our presets, we need to know the video more DURING the switch
 * we'll peek into the PathDriveMode structure for that
 * 
 * Example: x10 -> x1 on 5D3
 * This sequence cannot be identified just by looking at C0F06804;
 * some ADTG registers that we need to overide are configured before that.
 * 
 * CtrlSrv: DlgLiveView.c PRESS_TELE_MAG_BUTTON KeyRepeat[0]
 *     Gmt: gmtModeChange
 *     Evf: evfModeChangeRequest(4)
 *     Evf: PATH_SelectPathDriveMode S:0 Z:10000 R:0 DZ:0 SM:1
 *      (lots of stuff going on)
 *     Evf: evfModeChangeComplete
 *      (some more stuff)
 *     Gmt: VisibleParam 720, 480, 0, 38, 720, 404.
 *     Gmt: gmtUpdateDispSize (10 -> 1)
 * PropMgr: *** mpu_send(06 05 09 11 01 00)     ; finally triggered PROP_LV_DISPSIZE...
 */

/* faster version than the one from ML core */
static void set_zoom(int zoom)
{
    if (!lv) return;
    if (RECORDING) return;
    if (is_movie_mode() && video_mode_crop) return;
    zoom = COERCE(zoom, 1, 10);
    if (zoom > 1 && zoom < 10) zoom = 5;
    prop_request_change_wait(PROP_LV_DISPSIZE, &zoom, 4, 1000);
}

#ifdef CONFIG_EOSM
/* AF changes rebuild Canon's Live View pipeline just like a zoom or menu
 * return.  Keep that rebuild inside the same transition controller. */
static void eosm_lv_guard_request(void);
#endif
int crop_rec_lv_transition_diag(char *buffer, int size);

/* faster version than the one from ML core */
static void set_lv_af_mode(int lv_af_mode)
{
    if (!lv) return;
    if (RECORDING) return;
    if (lv_af_mode > 3 && lv_af_mode != 0) lv_af_mode = 1;
    prop_request_change(PROP_LIVE_VIEW_AF_SYSTEM, &lv_af_mode, 4);
#ifdef CONFIG_EOSM
    eosm_lv_guard_request();
#endif
}

//Photo mode
static int reciso = 0; /* coming from crop_rec.c */
extern int WEAK_FUNC(reciso) isoless_recovery_iso;

/* Main dial (EOS M: WHEEL_LEFT/RIGHT) → shutter. Clockwise = faster. */
static int slim_handle_main_dial_shutter(unsigned int key)
{
    if (key == MODULE_KEY_WHEEL_RIGHT)
    {
        shutter_toggle(0, 1);
        return 1;
    }
    if (key == MODULE_KEY_WHEEL_LEFT)
    {
        shutter_toggle(0, -1);
        return 1;
    }
    return 0;
}

static int slim_lv_base_zoom(void)
{
    return is_movie_mode() ? 5 : 1;
}

static void slim_zoom_to_x10(void)
{
    extern int kill_canon_gui_mode;

    int base = slim_lv_base_zoom();
    if (!lv || RECORDING || lv_dispsize != base) return;
    if (lv_disp_mode != 0) return;

    set_zoom(10);
    /* Danne EOS M path: Canon owns x10 UI; restore its front buffer. */
    kill_canon_gui_mode = 0;
    if (canon_gui_front_buffer_disabled())
        canon_gui_enable_front_buffer(0);
    wait_lv_frames(1);
    redraw();
}

static void slim_zoom_from_x10(void)
{
    extern int kill_canon_gui_mode;

    if (!lv || RECORDING || lv_dispsize != 10) return;

    set_zoom(1);
    if (is_movie_mode())
    {
        msleep(50);
        set_zoom(5);
    }
    kill_canon_gui_mode = is_movie_mode() ? 1 : 0;
    if (canon_gui_front_buffer_disabled())
        canon_gui_enable_front_buffer(0);
    wait_lv_frames(1);
    redraw();
}

/* Settings → Shutter zoom: half-shutter x10 like SET (hold or sticky). */
static int slim_handle_shutter_zoom(unsigned int key)
{
    if (!Shutter_zoom || !is_EOSM) return 0;
    if (Shutter_rec) return 0;   /* Shutter record owns the half-press */
    if (!lv || gui_menu_shown() || RECORDING) return 0;
    if (lv_disp_mode != 0) return 0;

    int base = slim_lv_base_zoom();

    if (Shutter_zoom == 1)
    {
        if (key == MODULE_KEY_PRESS_HALFSHUTTER && lv_dispsize == base)
        {
            slim_zoom_to_x10();
            return 1;
        }
        if (key == MODULE_KEY_UNPRESS_HALFSHUTTER && lv_dispsize == 10)
        {
            slim_zoom_from_x10();
            return 1;
        }
    }
    else if (Shutter_zoom == 2)
    {
        if (key == MODULE_KEY_PRESS_HALFSHUTTER)
        {
            if (lv_dispsize == base)
                slim_zoom_to_x10();
            else if (lv_dispsize == 10)
                slim_zoom_from_x10();
            return 1;
        }
        /* Swallow half-shutter release so Canon does not disturb x10 preview. */
        if (key == MODULE_KEY_UNPRESS_HALFSHUTTER)
            return 1;
    }

    return 0;
}

/* EOS M Settings -> SET Button. Keep this independent of the legacy SET
 * assignments below: value 2 used to mean ISO on other cameras. */
static int slim_handle_set_button(unsigned int key)
{
    if (!is_EOSM || SET_button != 2 || key != MODULE_KEY_PRESS_SET)
        return 0;

    /* Same policy as idle-LiveView touch: never open menus while recording. */
    if (RECORDING)
        return 1;

    if (lv && is_movie_mode() && !gui_menu_shown() && lv_disp_mode == 0)
        gui_open_last_menu_selection();

    /* Do not fall through to legacy SET=2 (ISO) handling. */
    return 1;
}

/* Instant Dual ISO on/off for INFO/SET shortcuts: config + bottom bar only.
 * CMOS refresh runs from CBR_SHOOT_TASK (do not block the key handler). */
static void slim_toggle_dual_iso(void)
{
    /* Dual ISO is disabled in every mode: the module is not built, and nothing may
     * switch it on through a config variable either. */
}

/* ISO arrow shortcuts: when Dual ISO is ON, step primary+recovery as a pair. */
static void crop_rec_adjust_iso(int sign)
{
    if (lens_info.raw_iso == 0x0)
        return;

    /* Module .mo builds lack CONFIG_SLIM_MENUS; use dual_iso_is_enabled() like the bottom bar. */
    if (dual_iso_is_enabled())
    {
        dual_iso_slim_step_pair(sign > 0 ? 1 : -1);
        return;
    }

    if (sign > 0)
    {
        if (lens_info.raw_iso == ISO_6400)
            return;
    }
    else
    {
        if (lens_info.raw_iso == ISO_100)
            return;
    }
    iso_toggle(0, sign);
}

/* EOS M Settings → INFO Button:
 * 0=OFF, 1=Histogram, 2=Waveform, 3=Zebras, 4=False Color,
 * 5=Framing, 6=Quick Panel.
 * Returns: 1 = handled (block Canon), -1 = pass to Canon, 0 = not our INFO mapping. */
static int slim_handle_info_button(unsigned int key)
{
    if (!is_EOSM || key != MODULE_KEY_INFO)
        return 0;
    if (!INFO_button)
        return 0; /* OFF — Canon INFO / LV cycle */

    /* Outside ML overlay LV, keep Canon INFO for non-framing modes only. */
    if (INFO_button != 5 && lv_disp_mode != 0)
        return -1;

    switch (INFO_button)
    {
        case 1: /* Histogram Off ↔ Performance */
        {
            int h = get_config_var("hist.draw");
            set_config_var("hist.draw", h ? 0 : 1);
            if (!get_config_var("hist.draw")) redraw();
            return 1;
        }

        case 2: /* Waveform Off ↔ Performance */
        {
            int w = get_config_var("waveform.draw");
            set_config_var("waveform.draw", w ? 0 : 1);
            if (!get_config_var("waveform.draw")) redraw();
            return 1;
        }

        case 3: /* Zebras Off ↔ Performance */
        {
            int z = get_config_var("zebra.draw");
            set_config_var("zebra.draw", z ? 0 : 1);
            if (!get_config_var("zebra.draw")) redraw();
            return 1;
        }

        case 4: /* False Color toggle */
        {
            extern int falsecolor_draw;
            if (!falsecolor_draw)
                falsecolor_draw = 1;
            else
            {
                falsecolor_draw = 0;
                redraw();
            }
            return 1;
        }

        case 5: /* Framing ↔ real-time (MLV Lite Preview → Framing) */
            mlv_lite_info_framing_toggle();
            return 1;

        case 6: /* Quick Panel */
            if (!RECORDING && lv && is_movie_mode() &&
                !gui_menu_shown() && lv_disp_mode == 0)
            {
                menu_quick_screen_open();
                gui_open_menu();
            }
            return 1;

        default:
            return 0;
    }
}

/* Arrow assignment: 0=OFF, 1=Shutter, 2=Aperture, 3=ISO.
 * dir: +1 = UP/RIGHT, -1 = DOWN/LEFT. Returns 1 if consumed. */
static int slim_handle_arrow_adjust(int mode, int dir)
{
    if (!mode)
        return 0;
    if (more_hacks && RECORDING)
        return 1;

    if (mode == 1) /* Shutter */
    {
        shutter_toggle(0, dir);
        return 1;
    }
    if (mode == 2) /* Aperture */
    {
        if (!lens_info.aperture)
            return 1;
        if (dir > 0)
        {
            if (lens_info.raw_aperture == lens_info.raw_aperture_max)
                return 1;
            aperture_toggle(0, 1);
        }
        else
        {
            if (lens_info.raw_aperture == lens_info.raw_aperture_min)
                return 1;
            aperture_toggle(0, -1);
        }
        return 1;
    }
    if (mode == 3) /* ISO */
    {
        /* Dual ISO owns the ISO pair while active. Consume the shortcut so
         * an Up/Down ISO assignment cannot alter either ISO value. */
        if (dual_iso_is_enabled())
            return 1;
        if (lens_info.raw_iso == 0x0)
            return 1;
        if (dir > 0)
        {
            if (lens_info.raw_iso == ISO_6400)
                return 1;
            crop_rec_adjust_iso(2);
        }
        else
        {
            if (lens_info.raw_iso == ISO_100)
                return 1;
            crop_rec_adjust_iso(-2);
        }
        return 1;
    }
    return 0;
}

/* customize buttons and buttons shortcuts, FIXME: implement these as feature in ML core? */
static unsigned int photo_keypress_cbr(unsigned int key)
{
    
    if (lv && !gui_menu_shown() && !is_movie_mode())
    {
        if (is_EOSM && slim_handle_shutter_zoom(key))
            return 0;

        if (slim_handle_main_dial_shutter(key))
            return 0;

        {
            int info = slim_handle_info_button(key);
            if (info == 1) return 0;
            if (info == -1) return 1;
        }

        extern int kill_canon_gui_mode;
        /* Quick x10 mode */

        if (lv_dispsize == 1)
        {
            if (((key == MODULE_KEY_PRESS_SET         ) && SET_button  == 1)                 ||
                ((key == MODULE_KEY_INFO              ) && !is_EOSM && INFO_button == 1)     ||
                (!is_EOSM && (key == MODULE_KEY_PRESS_HALFSHUTTER ) && Half_Shutter) )
            {
            if (key == MODULE_KEY_PRESS_HALFSHUTTER && Half_Shutter)
            {
                msleep(400);
            }
                set_zoom(10);
                /* Enable Canon overlays in x10 mode */
                kill_canon_gui_mode = 0;
                if (canon_gui_front_buffer_disabled())
                {
                        canon_gui_enable_front_buffer(0);
                }
                return 0;
            }
        }

        /* Finished from x10 mode? Let's get back to normal preview */
        if (lv_dispsize == 10)
        {
            if (((key == MODULE_KEY_PRESS_SET           ) && SET_button  == 1)                 ||
                ((key == MODULE_KEY_INFO                ) && !is_EOSM && INFO_button == 1)     ||
                (!is_EOSM && (key == MODULE_KEY_UNPRESS_HALFSHUTTER ) && Half_Shutter != 3 && is_manual_focus()) ||
                (!is_EOSM && (key == MODULE_KEY_PRESS_HALFSHUTTER ) && Half_Shutter == 3 && is_manual_focus()) )
            {
                set_zoom(1); // Get to x1 first, sometime we get black preview when going x10 --> x5

                /* Disable Canon overlays in x5 mode */
                kill_canon_gui_mode = 1;
                return 0;
            }
        }
        
        if (lv_dispsize != 10)
        {
            /* U/D and L/R: Shutter / Aperture / ISO (Settings → Up/Down / Left/Right Button) */
            if (key == MODULE_KEY_PRESS_UP && slim_handle_arrow_adjust(Arrows_U_D, 1))
                return 0;
            if (key == MODULE_KEY_PRESS_DOWN && slim_handle_arrow_adjust(Arrows_U_D, -1))
                return 0;
            if (key == MODULE_KEY_PRESS_RIGHT && slim_handle_arrow_adjust(Arrows_L_R, 1))
                return 0;
            if (key == MODULE_KEY_PRESS_LEFT && slim_handle_arrow_adjust(Arrows_L_R, -1))
                return 0;

            /* Legacy SET / non-EOSM INFO ISO/aperture shortcuts (unchanged mappings) */
            if (((key == MODULE_KEY_INFO)       && !is_EOSM && INFO_button == 2) ||
                ((key == MODULE_KEY_PRESS_SET)  && SET_button  == 2))
            {
                crop_rec_adjust_iso(2);
                return 0;
            }
            if (key == MODULE_KEY_INFO && !is_EOSM && INFO_button == 3)
            {
                aperture_toggle(0, -1);
                return 0;
            }
            if (key == MODULE_KEY_PRESS_SET && SET_button == 3)
            {
                aperture_toggle(0, 1);
                return 0;
            }
            
            /* Dual ISO ON / OFF */
            if (((key == MODULE_KEY_INFO)       && !is_EOSM && INFO_button == 4) ||
                ((key == MODULE_KEY_PRESS_SET)  && SET_button == 4))
            {
                if (!RECORDING)
                {
                    slim_toggle_dual_iso();
                    return 0;
                }
            }
            
            /* False color ON / OFF */
            if (((key == MODULE_KEY_INFO)       && !is_EOSM && INFO_button == 5) ||
                ((key == MODULE_KEY_PRESS_SET)  && SET_button  == 5))
            {
                SetGUIRequestMode(0);
                extern int falsecolor_draw;
                if (!falsecolor_draw)
                {
                    falsecolor_draw = 1;
                    return 0;
                }
                if (falsecolor_draw)
                {
                    falsecolor_draw = 0;
                    redraw();
                    return 0;
                }
            }
            
#ifdef CONFIG_SLIM_MENUS
            if (tapdisp == 4 && key == MODULE_KEY_TOUCH_1_FINGER)
#else
            if (tapdisp == 5 && key == MODULE_KEY_TOUCH_1_FINGER)
#endif
            {
                SetGUIRequestMode(0);
                extern int falsecolor_draw;
                if (!falsecolor_draw)
                {
                    falsecolor_draw = 1;
                    return 0;
                }
                if (falsecolor_draw)
                {
                    falsecolor_draw = 0;
                    redraw();
                    return 0;
                }
            }
        }
                        
        /* Opens up last magic lantern menu tab by tapping display */
        if (!is_movie_mode() && !gui_menu_shown() && lv && lv_dispsize != 10)
        {

            if ((key == MODULE_KEY_TOUCH_1_FINGER && tapdisp == 2) || (key == MODULE_KEY_PRESS_SET && SET_button == 7) || (key == MODULE_KEY_INFO && !is_EOSM && INFO_button == 6))
            {
                msleep(100);
                if(lv_disp_mode != 0){
                    // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                    return 1;
                }
                select_menu_by_name("Expo", "Shutter");
                gui_open_menu();
                submenu = 1;
            }
            if ((key == MODULE_KEY_PRESS_SET && SET_button == 6) || (key == MODULE_KEY_TOUCH_1_FINGER && tapdisp == 3) || (key == MODULE_KEY_INFO && !is_EOSM && INFO_button == 7))
            {
                msleep(100);
                if(lv_disp_mode != 0){
                    // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                    return 1;
                }
                select_menu_by_name("Expo", "Aperture");
                gui_open_menu();
                submenu = 1;
            }
            if ((key == MODULE_KEY_PRESS_SET && SET_button == 8) || (key == MODULE_KEY_INFO && !is_EOSM && INFO_button == 8)
#ifndef CONFIG_SLIM_MENUS
                 || (key == MODULE_KEY_TOUCH_1_FINGER && tapdisp == 4)
#endif
            )
            {
                msleep(100);
                if(lv_disp_mode != 0){
                    // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                    return 1;
                }
                select_menu_by_name("Expo", "ISO");
                gui_open_menu();
                submenu = 1;
            }
            if (tapdisp == 1 && key == MODULE_KEY_TOUCH_1_FINGER)
            {
                msleep(100);
                if(lv_disp_mode != 0){
                    // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                    return 1;
                }
                select_menu_by_name("Expo", "White Balance");
                gui_open_menu();
            }
        }
    }

return 1;
}

/* PATH_SelectPathDriveMode S:%d Z:%lx R:%lx DZ:%d SM:%d */
/* offsets verified on 5D3, 6D, 70D, EOSM, 100D, 60D, 80D, 200D */
const struct PathDriveMode
{
    uint32_t SM;            /* 5D3,700D: 0 during zoom, 1 in all other modes; 6D: 2 is flicker-related?! */
    uint32_t fps_mode;      /* 5D3,700D: 0=60p, 1=50p, 2=30p/zoom, 3=25p, 4=24p */
    uint32_t S;             /* 5D3,700D: 0=1080p, 1=720p, 8=zoom, 6=1080crop (700D) */
    uint32_t resolution_idx;/* 5D3,700D: 0=1080p/zoom, 1=720p, 2=640x480 (PathDriveMode->resolution_idx) */
    uint16_t zoom_lo;       /* 5D3,700D: lower word of zoom; unused? */
    uint16_t zoom;          /* 5D3,700D: 1, 5 or 10 */
    uint32_t unk_14;        /* 5D3: unused? */
    uint32_t DZ;            /* 5D3: unused? 700D: 2=1080crop */
    uint32_t unk_1c;
    uint32_t unk_20;
    uint32_t CF;            /* 100D, 80D: ? */
    uint32_t SV;            /* 100D, EOSM, 80D: ? */
    uint32_t unk_2c;
    uint32_t unk_30;
    uint32_t OutputType;   /*  SelectPath: 0 LCD, 1 VIDEO(NTSC), 2 VIDEO(PAL), 3 HDMI(1080i FULL), 4 HDMI(1080i INFO), 5 HDMI(720p FULL), 6 HDMI(720p INFO), 7 HDMI(480), 8 HDMI(576) */
    uint32_t unk_38;
    uint32_t unk_3c;
    uint32_t unk_40;
    uint32_t DT;            /* 100D, EOSM: ? */
} * PathDriveMode = 0;

enum fps_mode {
    FPS_60 = 0,
    FPS_50 = 1,
    FPS_30 = 2,
    FPS_25 = 3,
    FPS_24 = 4,
};

static int is_1080p()
{
    /* properties triggered too late */
    if (PathDriveMode->zoom != 1)
    {
        return 0;
    }

    /* unsure whether fast enough or not; to be tested */
    if (PathDriveMode->DZ)
    {
        return 0;
    }

    /* this snippet seems OK with properties */
    /* note: on 5D2 and 5D3 (maybe also 6D, not sure),
     * sensor configuration in photo mode is identical to 1080p.
     * other cameras may be different */
    return !is_movie_mode() || PathDriveMode->resolution_idx == 0;
}

static int is_720p()
{
    /* properties triggered too late */
    if (PathDriveMode->zoom != 1)
    {
        return 0;
    }

    /* unsure whether fast enough or not; to be tested */
    if (PathDriveMode->DZ)
    {
        return 0;
    }

    if (is_EOSM && !RECORDING_H264)
    {
        /* EOS M stays in 720p30 during standby */
        return 1;
    }

    /* this snippet seems OK with properties */
    return is_movie_mode() && PathDriveMode->resolution_idx == 1;
}

static int is_supported_mode()
{
    if (!lv) return 0;

    if (0)
    {
        printf(
            "Path: SM=%d S=%d res=%d DZ=%d zoom=%d mode=%d\n", 
            PathDriveMode->SM, PathDriveMode->S, PathDriveMode->resolution_idx,
            PathDriveMode->DZ, PathDriveMode->zoom, PathDriveMode->fps_mode
        );
    }
    
    /* EOS M prests will only work in x5 mode, don't patch x1 */
    if (PathDriveMode->zoom == 1)
    {
        if (is_EOSM) 
        {
            return 0;
        }

    }

    if (PathDriveMode->zoom == 10)
    {
        /* leave the x10 zoom unaltered, for focusing */
        return 0; 
    }

    return 1;
}

/* These appear to hold the selected HDMI configuration before applying it
 * we can detect LCD, 480p or 1080i outputs early from here, cool!
 *
 * This LOG from 700D when connecting HDMI to 480p ouput:
 *DisplayMgr:ff330104:88:16: [EDID] dwVideoCode = 2
 *DisplayMgr:ff330118:88:16: [EDID] dwHsize = 720
 *DisplayMgr:ff33012c:88:16: [EDID] dwVsize = 480
 *DisplayMgr:ff330148:88:16: [EDID] ScaningMode = EDID_NON_INTERLACE(p)
 *DisplayMgr:ff330194:88:16: [EDID] VerticalFreq = EDID_FREQ_60Hz
 *DisplayMgr:ff3301b0:88:16: [EDID] AspectRatio = EDID_ASPECT_4x3
 *DisplayMgr:ff3301cc:88:16: [EDID] AudioMode = EDID_AUDIO_LINEAR_PCM
 *DisplayMgr:ff331580:88:16: [EDID] ColorMode = EDID_COLOR_RGB */
const struct EDID_HDMI_INFO
{
    uint32_t dwVideoCode;   /* 0 LCD, 2 480p, 5 1080i */
    uint32_t dwHsize;       /* LCD = 0, 480p = 720, 1080i = 1920 */
    uint32_t dwVsize;       /* LCD = 0, 480p = 480, 1080i = 1080 */
    uint32_t ScaningMode;   /* 0 = EDID_NON_INTERLACE(p), 1 = EDID_INTERLACE(i) */
    uint32_t VerticalFreq;
    uint32_t AspectRatio;   /* 0 = EDID_ASPECT_4x3, 1 = EDID_ASPECT_16x9 */
    uint32_t AudioMode;
    uint32_t ColorMode;
} * EDID_HDMI_INFO = 0;

static int32_t  target_yres = 0;
static int32_t  delta_adtg0 = 0;
static int32_t  delta_adtg1 = 0;
static int32_t  delta_head3 = 0;
static int32_t  delta_head4 = 0;
static uint32_t cmos1_lo = 0, cmos1_hi = 0;
static uint32_t cmos2 = 0;

/* helper to allow indexing various properties of Canon's video modes */
static inline int get_video_mode_index()
{
    if (lv_dispsize > 1)
    {
        if (PathDriveMode->zoom == 5)
        {
            return 5;
        }
    }

    return
        (video_mode_fps == 24) ?  0 :
        (video_mode_fps == 25) ?  1 :
        (video_mode_fps == 30) ?  2 :
        (video_mode_fps == 50) ?  3 :
     /* (video_mode_fps == 60) */ 4 ;
}

/* optical black area sizes */
/* not sure how to adjust them from registers, so... hardcode them here */
static inline void FAST calc_skip_offsets(int * p_skip_left, int * p_skip_right, int * p_skip_top, int * p_skip_bottom)
{
    /* start from LiveView values */
    int skip_left       = 146;
    int skip_right      = 2;
    int skip_top        = 28;
    int skip_bottom     = 0;
    
    skip_left       = 72;
    skip_right      = 0;
    skip_top        = 28;
    skip_bottom     = 0;
    
    switch (crop_preset)
    {
            
        case CROP_PRESET_3X3:
            if (mv1080)
            {
                if (AR_16_9)//976
                {
                    skip_left       = 72 + reg_skip_left;
                    skip_right      = 0 + reg_skip_right;
                    skip_top        = 28 + 92;
                    skip_bottom     = 0 + 92;
                }
                if (AR_2_1)//868
                {
                    skip_left       = 72 + reg_skip_left;
                    skip_right      = 0 + reg_skip_right;
                    skip_top        = 28 + 146;
                    skip_bottom     = 0 + 146;
                }
                if (AR_2_20_1)//790
                {
                    skip_left       = 72 + reg_skip_left;
                    skip_right      = 0 + reg_skip_right;
                    skip_top        = 28 + 185;
                    skip_bottom     = 0 + 185;
                }
                if (AR_2_35_1)//738
                {
                    skip_left       = 72 + reg_skip_left;
                    skip_right      = 0 + reg_skip_right;
                    skip_top        = 28 + 211;
                    skip_bottom     = 0 + 211;
                }
                if (AR_2_39_1)//726
                {
                    skip_left       = 72 + reg_skip_left;
                    skip_right      = 0 + reg_skip_right;
                    skip_top        = 28 + 217;
                    skip_bottom     = 0 + 217;
                }
            }
            if (mv1080_3_2)
            {
                skip_left       = 72;
                skip_right      = 0;
                skip_top        = 28;
                skip_bottom     = 0;
            }
            break;
    }

    if (p_skip_left)   *p_skip_left    = skip_left;
    if (p_skip_right)  *p_skip_right   = skip_right;
    if (p_skip_top)    *p_skip_top     = skip_top;
    if (p_skip_bottom) *p_skip_bottom  = skip_bottom;
}

/* max resolution for each video mode (trial and error) */
/* it's usually possible to push the numbers a few pixels further,
 * at the risk of corrupted frames */
static int max_resolutions[NUM_CROP_PRESETS][6] = {
                                /*   24p   25p   30p   50p   60p   x5 */
    [CROP_PRESET_3K]            = { 1920, 1728, 1504,  760,  680, 1320 },
};

int CMOS_5_Debug = 0;
int CMOS_7_Debug = 0;

/* pack two 6-bit values into a 12-bit one */
#define PACK12(lo,hi) ((((lo) & 0x3F) | ((hi) << 6)) & 0xFFF)

/* pack two 16-bit values into a 32-bit one */
#define PACK32(lo,hi) (((uint32_t)(lo) & 0xFFFF) | ((uint32_t)(hi) << 16))

/* pack two 16-bit values into a 32-bit one */
#define PACK32(lo,hi) (((uint32_t)(lo) & 0xFFFF) | ((uint32_t)(hi) << 16))

static void FAST cmos_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    /* make sure we are in 1080p/720p mode */
    if (!is_supported_mode())
    {
        /* looks like checking properties works fine for detecting
         * changes in video mode, but not for detecting the zoom change */
        return;
    }

    uint16_t* data_buf = (uint16_t*) regs[0];
    int cmos_new[15] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    



    // EOS M presets
    // cmos_new[5] used for vertical offset, cmos_new[7] for horizontal offset
    if (is_DIGIC_5)
    {
        switch (crop_preset)
        {
            case CROP_PRESET_1X1:
            if (!is_720p() || !is_1080p())
            {
                if (CROP_2_5K)
                {
                    cmos_new[5] = 0x2C0;
                    cmos_new[7] = 0xA6A;
                }
                
                if (CROP_1440p)
                {
                    cmos_new[5] = 0x2C0;
                    cmos_new[7] = 0xAA9;
                }
                
                if (CROP_1620p)
                {
                    cmos_new[5] = 0x344 + reg_cmos5;
                    cmos_new[7] = 0xAC8 + reg_cmos7;
                }
                
                if (CROP_2_8K)
                {
                    cmos_new[5] = 0x280;
                    cmos_new[7] = 0xAA9;
                }
                
                if (CROP_3K)
                {
                    cmos_new[5] = 0x240;
                    cmos_new[7] = 0xAA9;
                }
                
                if (CROP_1280p || CROP_1080p)
                {
                    cmos_new[5] = 0x380;
                    cmos_new[7] = 0xACA;
                }
                
                if (CROP_Full_Res)
                {
                    cmos_new[5] = 0x0;
                    cmos_new[7] = 0x3A0;
                }
            }
            break;
            
            case CROP_PRESET_1X3:
            if (AR_16_9)
            {
                if (Anam_Highest)
                {
                    cmos_new[5] = 0xE0;
                    cmos_new[7] = 0xB44;
                }
                if (Anam_Higher)
                {
                    cmos_new[5] = 0x120;
                    cmos_new[7] = 0xB25;
                }
                if (Anam_Medium)
                {
                    cmos_new[5] = 0x1A0;
                    cmos_new[7] = 0xB46;
                }
            }
            if (AR_2_1)
            {
                if (Anam_Highest)
                {
                    cmos_new[5] = 0xA0;
                    cmos_new[7] = 0xB44;
                }
                if (Anam_Higher)
                {
                    cmos_new[5] = 0x120;
                    cmos_new[7] = 0xB25;
                }
                if (Anam_Medium)
                {
                    cmos_new[5] = 0x160;
                    cmos_new[7] = 0xB46;
                }
            }
            if (AR_2_20_1)
            {
                if (Anam_Highest)
                {
                    cmos_new[5] = 0x60;
                    cmos_new[7] = 0xB25;
                }
                if (Anam_Higher)
                {
                    cmos_new[5] = 0xA0;
                    cmos_new[7] = 0xB26;
                }
                if (Anam_Medium)
                {
                    cmos_new[5] = 0x120;
                    cmos_new[7] = 0xB26;
                }
            }
            if (AR_2_35_1 || AR_2_39_1)
            {
                if (Anam_Highest)
                {
                    cmos_new[5] = 0x20;
                    cmos_new[7] = 0xB05;
                }
                if (Anam_Higher)
                {
                    cmos_new[5] = 0xA0;
                    cmos_new[7] = 0xB26;
                }
                if (Anam_Medium)
                {
                    cmos_new[5] = 0xE0;
                    cmos_new[7] = 0xB27;
                }
            }
                if (Anam_FLV)
                {
                    cmos_new[5] = 0x20 + CMOS_5_Debug;
                    cmos_new[7] = 0xC00 + CMOS_7_Debug;
                }
            break;

            case CROP_PRESET_3X3:
                if (High_FPS)
                {
                    if (AR_16_9)   cmos_new[7] = 0x802;
                    if (AR_2_1    || 
                        AR_2_20_1) cmos_new[7] = 0x804;
                    if (AR_2_35_1 ||
                        AR_2_39_1) cmos_new[7] = 0x806; // AR_2_39_1 is actually 2.50:1 preset
                }
                if (mv1080 || mv1080_3_2) cmos_new[7] = 0x800;
                cmos_new[5] = 0x20;
            break;
        }
    }

    /* menu overrides */
    if (cmos1_lo || cmos1_hi)
    {
        cmos_new[1] = PACK12(cmos1_lo,cmos1_hi);
    }

    if (cmos2)
    {
        cmos_new[2] = cmos2;
    }
    
    /* copy data into a buffer, to make the override temporary */
    /* that means: as soon as we stop executing the hooks, values are back to normal */
    static uint16_t copy[512];
    uint16_t* copy_end = &copy[COUNT(copy)];
    uint16_t* copy_ptr = copy;

    while (*data_buf != 0xFFFF)
    {
        *copy_ptr = *data_buf;

        int reg = (*data_buf) >> 12;
        if (cmos_new[reg] != -1)
        {
            *copy_ptr = (reg << 12) | cmos_new[reg];
            dbg_printf("CMOS[%x] = %x\n", reg, cmos_new[reg]);
        }

        data_buf++;
        copy_ptr++;
        if (copy_ptr > copy_end) while(1);
    }
    *copy_ptr = 0xFFFF;

    /* pass our modified register list to cmos_write */
    regs[0] = (uint32_t) copy;
}

static uint32_t nrzi_encode( uint32_t in_val )
{
    uint32_t out_val = 0;
    uint32_t old_bit = 0;
    for (int num = 0; num < 31; num++)
    {
        uint32_t bit = in_val & 1<<(30-num) ? 1 : 0;
        if (bit != old_bit)
            out_val |= (1 << (30-num));
        old_bit = bit;
    }
    return out_val;
}

static uint32_t nrzi_decode( uint32_t in_val )
{
    uint32_t val = 0;
    if (in_val & 0x8000)
        val |= 0x8000;
    for (int num = 0; num < 31; num++)
    {
        uint32_t old_bit = (val & 1<<(30-num+1)) >> 1;
        val |= old_bit ^ (in_val & 1<<(30-num));
    }
    return val;
}

/* adapted from fps_override_shutter_blanking in fps-engio.c */
static int adjust_shutter_blanking(int old)
{
    /* sensor duty cycle: range 0 ... timer B */
    int current_blanking = nrzi_decode(old);
    
    static int previous_blanking = -1;
    
    if (ABS(current_blanking - previous_blanking) == 1) 
    {
        current_blanking = previous_blanking;
    } 
    else 
    {
       previous_blanking = current_blanking;
    }

    int video_mode = get_video_mode_index();

    /* what value Canon firmware assumes for timer B? */
    int fps_timer_b_orig = default_timerB[video_mode];

    int current_exposure = fps_timer_b_orig - current_blanking;
    
    /* wrong assumptions? */
    if (current_exposure < 0)
    {
        return old;
    }

    int default_fps = default_fps_1k[video_mode];
    int current_fps = fps_get_current_x1000();

    dbg_printf("FPS %d->%d\n", default_fps, current_fps);

    float frame_duration_orig = 1000.0 / default_fps;
    float frame_duration_current = 1000.0 / current_fps;

    float orig_shutter = frame_duration_orig * current_exposure / fps_timer_b_orig;

    float new_shutter =
        (shutter_range == 0) ?
        ({
            /* original shutter speed from the altered video mode */
            orig_shutter;
        }) :
        ({
            /* map the available range of 1/4000...1/30 (24-30p) or 1/4000...1/60 (50-60p)
             * from minimum allowed (1/15000 with full-res LV) to 1/fps */
            int max_fps_shutter = (video_mode_fps <= 30) ? 33333 : 64000;
            int default_fps_adj = 1e9 / (1e9 / max_fps_shutter - 250);
            (orig_shutter - 250e-6) * default_fps_adj / current_fps;
        });

    /* what value is actually used for timer B? (possibly after our overrides) */
    int fps_timer_b = (shamem_read(0xC0F06014) & 0xFFFF) + 1;

    dbg_printf("Timer B %d->%d\n", fps_timer_b_orig, fps_timer_b);

    int new_exposure = new_shutter * fps_timer_b / frame_duration_current;
    int new_blanking = COERCE(fps_timer_b - new_exposure, 10, fps_timer_b - 2);

    dbg_printf("Exposure %d->%d (timer B units)\n", current_exposure, new_exposure);

#ifdef CROP_DEBUG
    float chk_shutter = frame_duration_current * new_exposure / fps_timer_b;
    dbg_printf("Shutter %d->%d us\n", (int)(orig_shutter*1e6), (int)(chk_shutter*1e6));
#endif

    dbg_printf("Blanking %d->%d\n", current_blanking, new_blanking);

    return nrzi_encode(new_blanking);
}

extern void fps_override_shutter_blanking();
static int shutter_blanking_idle;

static void FAST adtg_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (!is_supported_mode())
    {
        /* don't patch other video modes */
        return;
    }


    /* This hook is called from the DebugMsg's in adtg_write,
     * so if we change the register list address, it won't be able to override them.
     * Workaround: let's call it here. */
    fps_override_shutter_blanking();

    uint32_t cs = regs[0];
    uint32_t *data_buf = (uint32_t *) regs[1];
    int dst = cs & 0xF;
    
    /* copy data into a buffer, to make the override temporary */
    /* that means: as soon as we stop executing the hooks, values are back to normal */
    static uint32_t copy[512];
    uint32_t* copy_end = &copy[COUNT(copy)];
    uint32_t* copy_ptr = copy;
    
    struct adtg_new
    {
        int dst;
        int reg;
        int val;
    };
    
    /* expand this as required */
    struct adtg_new adtg_new[24] = {{0}};

    /* scan for shutter blanking and make both zoom and non-zoom value equal */
    /* (the values are different when using FPS override with ADTG shutter override) */
    /* (fixme: might be better to handle this in ML core?) */
    /* also scan for one ADTG gain register and get its value to be used for lower bit-depth */
    /* this is similair method to lowering digital gain to get "fake" lossless compression 
       in lower bit-depths, digital gain method only works with native RAW resolutions       */
    /* we will use [ADTG2/4] 8882/8884/8886/8888 registers, they change values slightly with ISO changes */
    int shutter_blanking = 0;
    int analog_gain = 0;
    
    const int blanking_reg_zoom   = 0x805F;
    const int blanking_reg_nozoom = 0x8061;
    const int blanking_reg        = (lv_dispsize == 1) ? blanking_reg_nozoom : blanking_reg_zoom;
    
    int adtg_analog_gain_reg = 0x8882;
    for (uint32_t * buf = data_buf; *buf != 0xFFFFFFFF; buf++)
    {
        int reg = (*buf) >> 16;
        if (reg == blanking_reg)
        {
            int val = (*buf) & 0xFFFF;
            shutter_blanking = val;
        }
        if (reg == adtg_analog_gain_reg)
        {
            int val = (*buf) & 0xFFFF;
            analog_gain = val;
        }
    }

    /* some modes may need adjustments to maintain exposure */
    if (shutter_blanking)
    {
        /* FIXME: remove this kind of hardcoded conditions */
        if (is_DIGIC_5)
        {
            shutter_blanking = adjust_shutter_blanking(shutter_blanking);
        }
    }

    /* a workaround to fix shutter fine-tuning when using more hacks (suspending AeWb task) in mlv_lite */
    /* we need to save last shutter_blanking value into a variable before suspending AeWb task */
    if (!AeWbTask_Disabled())
    {
        /* save shutter_blanking value when shutter_blanking value isn't 0 */
        if (shutter_blanking) shutter_blanking_idle = shutter_blanking;
    }

    /* always use our saved shutter blanking value when "More" hacks is selected (i.e when AeWb is suspended) */
    /* FIXME: exclude this method when shutter-fine isn't used */
    if (is_more_hacks_selected() && AeWbTask_Disabled())
    {
        /* change shutter_blanking value to our saved value when shutter_blanking value isn't 0 */
        if (shutter_blanking) shutter_blanking = shutter_blanking_idle;
    }

    /* all modes may want to override shutter speed */
    /* ADTG[0x8060]: shutter blanking for 3x3 mode  */
    /* ADTG[0x805E]: shutter blanking for zoom mode  */
    adtg_new[0] = (struct adtg_new) {6, blanking_reg_nozoom, shutter_blanking};
    adtg_new[1] = (struct adtg_new) {6, blanking_reg_zoom, shutter_blanking};   

    /* hopefully generic; to be tested later */
    if (1)
    {
        // EOS M presets
        // ADTG2[0x8183] and ADTG2[0x8184] enable horizontal pixel binning instead of skipping
        // in 1080p ADTG2[0x8183] = 0x21, ADTG2[0x8183] = 0x7B, in x5 both are = 0x0 
        // ADTG2[0x800C] = 2: vertical binning/skipping factor = 3, ADTG2[0x800C] = 0 read all vertical lines
        if (is_DIGIC_5)
        {
            switch (crop_preset)
            {
                case CROP_PRESET_1X3:
                if (is_EOSM)
                {
                    adtg_new[2] = (struct adtg_new) {2, 0x800C, 0};
                    adtg_new[3] = (struct adtg_new) {2, 0x8000, 0x6};
                    adtg_new[4] = (struct adtg_new) {2, 0x8183, 0x21};
                    adtg_new[5] = (struct adtg_new) {2, 0x8184, 0x7B};
                    
                }
                break;
                
                case CROP_PRESET_3X3:
                if (is_EOSM)
                {
                    adtg_new[2] = (struct adtg_new) {2, 0x800C, 0x2};
                    adtg_new[3] = (struct adtg_new) {2, 0x8000, 0x6};
                    adtg_new[4] = (struct adtg_new) {2, 0x8183, 0x21};
                    adtg_new[5] = (struct adtg_new) {2, 0x8184, 0x7B};
                }
                break; 
            }
        }

        /* PowerSaveTiming & ReadOutTiming registers */
        /* these need changing in all modes with higher vertical resolution */
        switch (crop_preset)
        {
            case CROP_PRESET_1X1:
            case CROP_PRESET_1X3:
            case CROP_PRESET_3X3:
            {
                /* assuming FPS timer B was overridden before this */
                int fps_timer_b = (shamem_read(0xC0F06014) & 0xFFFF) + 1;
                int readout_end = shamem_read(0xC0F06804) >> 16;    /* fixme: D5 only */

                /* PowerSaveTiming registers */
                /* after readout is finished, we can turn off the sensor until the next frame */
                /* we could also set these to 0; it will work, but the sensor will run a bit hotter */
                /* to be tested to find out exactly how much */
                adtg_new[11] = (struct adtg_new) {6, 0x8172, nrzi_encode(readout_end + 1) }; /* PowerSaveTiming ON (6D/700D) */
                adtg_new[12] = (struct adtg_new) {6, 0x8178, nrzi_encode(readout_end + 1) }; /* PowerSaveTiming ON (5D3/6D/700D) */
                adtg_new[13] = (struct adtg_new) {6, 0x8196, nrzi_encode(readout_end + 1) }; /* PowerSaveTiming ON (5D3) */

                adtg_new[14] = (struct adtg_new) {6, 0x8173, nrzi_encode(fps_timer_b - 5) }; /* PowerSaveTiming OFF (6D/700D) */
                adtg_new[15] = (struct adtg_new) {6, 0x8179, nrzi_encode(fps_timer_b - 5) }; /* PowerSaveTiming OFF (5D3/6D/700D) */
                adtg_new[16] = (struct adtg_new) {6, 0x8197, nrzi_encode(fps_timer_b - 5) }; /* PowerSaveTiming OFF (5D3) */

                adtg_new[17] = (struct adtg_new) {6, 0x82B6, nrzi_encode(readout_end - 1) }; /* PowerSaveTiming ON? (700D); 2 units below the "ON" timing from above */

                /* ReadOutTiming registers */
                /* these shouldn't be 0, as they affect the image */
                adtg_new[18] = (struct adtg_new) {6, 0x82F8, nrzi_encode(readout_end + 1) }; /* ReadOutTiming */
                adtg_new[19] = (struct adtg_new) {6, 0x82F9, nrzi_encode(fps_timer_b - 1) }; /* ReadOutTiming end? */
                break;
            }
        }
    }
    
    /* divid signal to achieve lower bit-depths using negative analog gain */
    if (which_output_format() >= 3) // don't patch if uncompressed RAW is selected
    {
        if (OUTPUT_14BIT)
        {
            BitDepth_Analog = 14;
        }
        
        if (RECORDING || RAW_HISTOGRAM_ENABLED)//When RAW histogram is used turn off the temporary 14bit stuff
        {
            
            if (OUTPUT_12BIT)
            {
                adtg_new[20] = (struct adtg_new) {6, 0x8882, analog_gain / 4};
                adtg_new[21] = (struct adtg_new) {6, 0x8884, analog_gain / 4};
                adtg_new[22] = (struct adtg_new) {6, 0x8886, analog_gain / 4};
                adtg_new[23] = (struct adtg_new) {6, 0x8888, analog_gain / 4};
                BitDepth_Analog = 12;
            }
            
            if (OUTPUT_11BIT)
            {
                adtg_new[20] = (struct adtg_new) {6, 0x8882, analog_gain / 8};
                adtg_new[21] = (struct adtg_new) {6, 0x8884, analog_gain / 8};
                adtg_new[22] = (struct adtg_new) {6, 0x8886, analog_gain / 8};
                adtg_new[23] = (struct adtg_new) {6, 0x8888, analog_gain / 8};
                BitDepth_Analog = 11;
            }
            
            if (OUTPUT_10BIT)
            {
                adtg_new[20] = (struct adtg_new) {6, 0x8882, analog_gain / 16};
                adtg_new[21] = (struct adtg_new) {6, 0x8884, analog_gain / 16};
                adtg_new[22] = (struct adtg_new) {6, 0x8886, analog_gain / 16};
                adtg_new[23] = (struct adtg_new) {6, 0x8888, analog_gain / 16};
                BitDepth_Analog = 10;
            }
        }
    }

    while(*data_buf != 0xFFFFFFFF)
    {
        *copy_ptr = *data_buf;
        int reg = (*data_buf) >> 16;
        for (int i = 0; i < COUNT(adtg_new); i++)
        {
            if ((reg == adtg_new[i].reg) && (dst & adtg_new[i].dst))
            {
                int new_value = adtg_new[i].val;
                dbg_printf("ADTG%x[%x] = %x\n", dst, reg, new_value);
                *(uint16_t*)copy_ptr = new_value;

                if (reg == blanking_reg_zoom || reg == blanking_reg_nozoom)
                {
                    /* also override in original data structure */
                    /* to be picked up on the screen indicators */
                    *(uint16_t*)data_buf = new_value;
                }
            }
        }
        data_buf++;
        copy_ptr++;
        if (copy_ptr >= copy_end) while(1);
    }
    *copy_ptr = 0xFFFFFFFF;
    
    /* pass our modified register list to adtg_write */
    regs[1] = (uint32_t) copy;
}

int analog_gain_is_acive()
{
    if (CROP_PRESET_MENU) // analog gain is only active when preset is selected
    {
        if (bit_depth_analog == 1)
        {
            return 1;
        }
    
        if (bit_depth_analog == 2)
        {
            return 2;
        }
    
        if (bit_depth_analog == 3)
        {
            return 3;
        }
    }
   
    return 0;
}

int crop_rec_is_enabled()
{
    if (CROP_PRESET_MENU)
    {
        return 1;
    }
    
    return 0;
}








/* just for testing */
/* (might be useful for FPS override on e.g. 70D) */


/* adjust Timer B to make scanning Dual-ISO lines static  */
/* Timer B value should be in 4 increment  */
int Adjust_TimerB_For_Dual_ISO(int TimerB)
{   TimerB = (TimerB / 4) * 4;
    return TimerB + 3;
}

/* EOS M reg_override presets */

int preview_debug_1 = 0;
int preview_debug_2 = 0;
int preview_debug_3 = 0;
int preview_debug_4 = 0;

int RAW_H_Debug  = 0;
int RAW_V_Debug  = 0;
int TimerA_Debug = 0;
int TimerB_Debug = 0;

static unsigned TimerB = 0;
static unsigned TimerA = 0;

static unsigned RAW_H = 0;            // RAW width    resolution          0xC0F06804
static unsigned RAW_V = 0;            // RAW vertical resolution          0xC0F06804

/* True 23.976 fps (24000/1001).
 *
 * Sensor frame rate = 32 MHz / (A * B), with A = TimerA + 1 and B = TimerB + 1 (the registers
 * hold value - 1).  24000/1001 needs A * B = 1334666.67, which is not a whole number.  Canon's own
 * 24p mode hits it on average by switching Timer B between two neighbouring values; the presets
 * below use one fixed value, so several of them ran 0.01% - 0.05% too fast (e.g. A=528 B=2527
 * gives 23.983 fps, about 1 second too many per hour).
 *
 * After a preset has chosen its timers, move Timer B (only B) to the value that gets A * B closest
 * to the NTSC target.  Timer A is never touched: it sets the line time and its parity must stay as
 * Canon has it (the core FPS engine keeps it for the same reason); an earlier attempt that also moved
 * A produced vertical line artifacts and a low-res preview.
 * Only done for 24p presets (within 1% of 23.976), so 25p / 30p / high-FPS presets are untouched,
 * and Timer B may shrink only while at least 75% of the original vertical blanking (B - RAW_V) remains. */
#define NTSC24_PRODUCT 1334667   /* 32000000 * 1001 / 24000, rounded */
#define NTSC30_PRODUCT 1067733   /* 32000000 * 1001 / 30000, rounded */
/* defined with the Film Format menu code */
static int slim_video_standard(void);
static void ntsc24_snap_timers(void)
{
    if (!is_EOSM) return;

    /* 24p always; 30p only for the VIDEO standard on the 2.5K readout (29.97 fps) */
    int target;
    if (Framerate_24)
        target = NTSC24_PRODUCT;
    else if (Framerate_30 && crop_preset == CROP_PRESET_1X1 && CROP_2_5K && slim_video_standard())
        target = NTSC30_PRODUCT;
    else
        return;

    int a = TimerA + 1;
    int b0 = TimerB + 1;
    if (a < 2 || b0 < 2) return;

    int p0 = a * b0;
    if (p0 < target - target / 100 || p0 > target + target / 100) return;

    int err0 = ABS(p0 - target);
    int blank0 = b0 - (int)RAW_V;      /* original vertical blanking, in lines */
    int best_b = b0, best_err = err0;
    int b_floor = target / a;

    for (int k = 0; k < 2; k++)
    {
        int b = b_floor + k;
        if (b < 2) continue;

        /* keep most of the original blanking when Timer B gets smaller */
        if (b < b0 && (blank0 <= 0 || (b - (int)RAW_V) * 4 < blank0 * 3)) continue;

        int err = ABS(a * b - target);
        if (err < best_err)
        {
            best_err = err; best_b = b;
        }
    }

    /* only change something when it is clearly better */
    if (best_err < err0 && err0 > 13)
    {
        TimerB = best_b - 1;
    }
}

static unsigned Preview_Control = 0;        // Flag tells we want full real-time preview
static unsigned Preview_Control_Basic = 0;  // Flag tells we want basic preview (cropped preview)

/* used to center preview on RAW buffer in presets which have basic preview (cropped preview) */
static unsigned Preview_x1 = 0;
static unsigned Preview_x2 = 0;
static unsigned Preview_y1 = 0;
static unsigned Preview_y2 = 0;

/* help to shift preview on RAW buffer horizontally in case if active preview width is less than active RAW width */
/* it should be tweaked with 0xC0F383D4 (Preview_R) which also shift preview on RAW buffer (check Preview_Control_Basic) */
static int REG_C0F383DC_Tuning = 0;

/* used to increase processed RAW data in LiveView, also show new image on screen via stretch regs */
static unsigned Preview_H = 0;        // How much width to process        List of registers
static unsigned Preview_V = 0;        // How much height to process       List of registers
static unsigned Preview_R = 0;        // Preview related                  0xC0F383D4
static unsigned YUV_HD_S_H = 0;       // YUV (HD) horizontal stretch      0xC0F11B8C
static unsigned YUV_HD_S_V = 0;       // YUV (HD) vertical stretch        0xC0F11BCC
static unsigned YUV_HD_S_V_E = 0;     // YUV (HD) enable vertical stretch 0xC0F11BC8

/* used to correct aspect ratio on screen */
static unsigned YUV_LV_S_V = 0;       // YUV (LV) vertical stretch        0xC0F11ACC
static unsigned YUV_LV_Buf = 0;       // YUV (LV) buffer size             0xC0F04210

/* used to exceed preview limits */
//static unsigned EDMAC_24_s = 0;       // EDMAC#24 size                    0xC0F26810
//static unsigned EDMAC_24_address = 0; // EDMAC#24 buffer address          0xC0F26808
static unsigned EDMAC_24_Redirect = 0;  // EDMAC#24 re-driect buffer flag
static unsigned Black_Bar = 0;          // Exceed black bar width limit     0xC0F3B038, 0xC0F3B088

// addresses (part of EDMAC#9 configuration structure) which holds vertical HIV size in x5 mode
// on 700D the structure starts in 0x3e1b4 and ends in 0x3e234, it's being loaded in LVx5_StartPreproPath from ff4f2860
// overriding them are needed to exceed vertical preview limit, on 700D it's RAW V - 1 = 0x453 (in x5 mode)

// 0xC0F08184 = RAW V - 1 = 0x453 (in x5), which is also related somehow to EDMAC#9 vertical size and needs to be tweaked
// also EDMAC_24_Redirect is required before increasing 0xC0F08184 value, otherwise RAW data would be corrupted

// AFAIK EDMAC#9 is used for darkframe subtraction for LiveView, it also sets black and white level values (for LiveView)
static uint32_t EDMAC_9_Vertical_1 = 0;         // 0x453 , it's being set in 0xC0F04910 register which control EDMAC#9 Size B
static uint32_t EDMAC_9_Vertical_2 = 0;         // 0x453 , tweaking it has no effect? let's tweak just in case

static unsigned EDMAC_9_Vertical_Change = 0;    // flag to enable/disable EDMAC#9 tweaks

/* used to recover preview height eaten by C0F38024 after increasing C0F38024 horizontal value */
static int Preview_V_Recover = 0;

/* flags to center preview and clear VRAM artifacts (NewShiftVal, NewClearVal are related too) */
static unsigned Shift_Preview = 0;
static unsigned Center_Preview_ON = 0;
static unsigned Clear_Artifacts = 0;
static unsigned Clear_Artifacts_ON = 0;

/* camera-specific ROM addresses */
/* Shift_x5 holds preview shifting value for an output for x5 mode */
static uint32_t Shift_x5_LCD = 0;
static uint32_t Shift_x5_HDMI_480p = 0; // Called VIDEO NTSC 
static uint32_t Shift_x5_HDMI_1080i_Full = 0;
static uint32_t Shift_x5_HDMI_1080i_Info = 0;

/* Clear_Vram_x5 holds preview clear Vram value for an output for x5 mode */
static uint32_t Clear_Vram_x5_LCD = 0;
static uint32_t Clear_Vram_x5_HDMI_480p = 0;
static uint32_t Clear_Vram_x5_HDMI_1080i_Full = 0;
static uint32_t Clear_Vram_x5_HDMI_1080i_Info = 0;

/* address holds darkframe subtraction data for photo mode for (RAW_V) */
static uint32_t HIV_Vertical_Photo_Address = 0;
static uint32_t HIV_Vertical_Address_hook = 0; // LVx5_StartPreproPath, sets HIV address for RAW_V

static inline uint32_t reg_override_1X1(uint32_t reg, uint32_t old_val)
{
    if (CROP_2_5K)
    {
        if (is_EOSM)
        {
            RAW_H         = 0x298 + reg_width;
            RAW_V         = 0x455 + reg_height;
            TimerA        = 0x2CB;
            if (Framerate_24) TimerB = 0x747;
            if (Framerate_25) TimerB = 0x6FA;
            if (Framerate_30) TimerB = 0x5D3;
        }


        // Preview_H should be = active RAW width - 4? , 2520 - 4 = 2516 (active RAW width is 2520)
        // otherwise a black bar will appear in the left part of both YUV (HD) and (LV) dumps 
        // e.g. Preview_H = 2520 --> black bar on the left, also will loss some pixel on the right
        Preview_H     = 2516; 
        Preview_V     = 1080;
        Preview_R     = 0x19000D;
        YUV_HD_S_H    = 0x105027D;
        YUV_HD_S_V    = 0x1050195;
        YUV_HD_S_V_E  = 0;
        Black_Bar     = 2;

        Preview_Control = 1;
        EDMAC_24_Redirect = 0;
        Preview_V_Recover = 0;
        Preview_Control_Basic = 0;
    }
    
/*  initially I wanted to make 2880x1226 preset, RAW data works there, the issue is I could only get 1073 height (from 1226) in preview (width was 2868)
    more likely somehow when increasing width preview in C0F38024, it casues a limit in height, it'a act like give me width pixels in cost 
    of vertical pixels, EDMAC_24_Redirect and EDMAC_9_Vertical_Change doesn't help to get more height in this case, something else is causing a limit?
    BTW: C0F38024 can fix broken preview which casued of increasing RAW horizontal resolution in C0F06804, e.g. on 700D it's:
    
    in x5 mode C0F06804 = 0x4540298 while C0F38024 = 0x453x287. in C0F06804 let's say RAW_V = 454 and RAW_H = 298:
    C0F38024 = ((RAW_V - 1) << 16)  + RAW_H - 0x11*/
    
    /* at this moment I was making 2800x1192 preset instead because it would be more manageable in terms of preview, but . . */
    /* huh, never mind! figuerd it out, we can just increase EDMAC_9_Vertical and C0F08184 *more than actual RAW_V* to recover preview height  */
    
    if (CROP_2_8K)
    {
        if (is_EOSM)
        {
            RAW_H         = 0x2F2 + reg_width;
            RAW_V         = 0x4d3 + reg_height;
            TimerA        = 0x325;
            if (Framerate_24) TimerB = 0x676;
            if (Framerate_25) TimerB = 0x633;
            if (Framerate_30) TimerB = 0x633;  // 30 Doesn't work, make it 25
        }


        Preview_H         = 2868;  // black bar above 2868
        Preview_V         = 1226 + YUV_HD_S_H_height;
        Preview_V_Recover = 171 + YUV_HD_S_H_width;   // trial and error
        
        Preview_R     = 0x19000F;
        
        YUV_HD_S_H    = 0x10502D6;
        YUV_HD_S_V    = 0x10501b8 + YUV_HD_S_V_width;
        YUV_HD_S_V_E  = 0;
        Black_Bar     = 2;

        Preview_Control = 1;
        EDMAC_24_Redirect = 1;
        EDMAC_9_Vertical_Change = 1;
        Preview_Control_Basic = 0;
    } 

    if (CROP_3K)
    {
        /* Active RAW 3072x1308 (2.35:1). Old RAW_V 0x521 gave ~1284 lines (2.39:1). */
        enum { CROP_3K_RAW_V_EXTRA = 0x18 }; /* +24 lines → 1308 active height */

        if (is_EOSM)
        {
            RAW_H    = 0x322 + reg_width;
            RAW_V    = 0x521 + reg_height + CROP_3K_RAW_V_EXTRA;
            TimerB   = 0x60F;
            TimerA   = 0x35B;
        }


        Preview_H         = 2868;  // black bar above 2868
        Preview_V         = 1308;
        Preview_V_Recover = 284 + CROP_3K_RAW_V_EXTRA;

        Preview_R     = 0x190028;
        REG_C0F383DC_Tuning = -26; 

        YUV_HD_S_H    = 0x1050308;
        YUV_HD_S_V    = 0x10501D4;
        YUV_HD_S_V_E  = 0;
        Black_Bar     = 2;

        Preview_Control = 1;
        EDMAC_24_Redirect = 1;
        Preview_Control_Basic = 0;
        EDMAC_9_Vertical_Change = 1;
    }
        
    if (CROP_1440p)
    {
        if (is_EOSM)
        {
            RAW_H    = 0x2A2 + reg_width;
            RAW_V    = 0x5BD + reg_height;
            TimerA   = 0x2DB;
            if (Framerate_24) TimerB = 0x71E;
            if (Framerate_25) TimerB = 0x6D3;
            if (Framerate_30) TimerB = 0x6D3;  // 30 Doesn't work, make it 25
        }


        Preview_H     = 2552;  // 2556 causes preview artifacts
        Preview_V     = 1440;
        Preview_R     = 0x19000E;
        Preview_V_Recover = 22;
        
        YUV_HD_S_H    = 0x1050286;
        YUV_HD_S_V    = 0x105021E;

        Black_Bar     = 2;
        Preview_Control = 1;
        EDMAC_24_Redirect = 1;
        EDMAC_9_Vertical_Change = 1;
        Preview_Control_Basic = 0;
    }
    
    if (CROP_1620p)
    {
        if (is_EOSM)
        {
            RAW_H    = 0x23E + reg_width;
            RAW_V    = 0x671 + reg_height;
            TimerA   = 0x279;
            /* 23.976: TimerB of dannephoto.  25: TimerB 0x7E2 (32 MHz / (0x27A * 0x7E3) = 25.000),
             * about 370 lines of blanking left; the 1440p readout runs 25 with 278.
             * 29.97 was tried (TimerB 0x693, only ~35 lines of blanking): the LCD shows no image. */
            TimerB   = 0x838;
            if (Framerate_25) TimerB = 0x7E2;
        }

        Preview_H     = 2156 + reg_Preview_H;  // 2556 causes preview artifacts
        Preview_V     = 1620 + reg_Preview_V;
        Preview_R     = 0x19000D;
        Preview_V_Recover = 22;
        
        YUV_HD_S_H    = 0x1050220 + reg_YUV_HD_S_H; //+ 50
        YUV_HD_S_V    = 0x1050240 + reg_YUV_HD_S_V;
        
        //doktorkrek suggestion (same as dannephoto — leave 0xC0F11A8C alone)
        YUV_LV_Buf = 0x1B505A0;
        YUV_LV_S_V = 0x10501B2;
        //EngDrvOutLV(0xC0F11A8C, 0x1E0038);
                        
        Black_Bar     = 2;
        Preview_Control = 1;
        EDMAC_24_Redirect = 1;
        EDMAC_9_Vertical_Change = 1;
        Preview_Control_Basic = 0;
    }

    if (CROP_1280p)
    {
        if (is_EOSM)
        {
            RAW_H    = 0x202 + reg_width;
            RAW_V    = 0x51D + reg_height;
            TimerA   = 0x235;
            if (Framerate_24) TimerB = 0x935;
            if (Framerate_25) TimerB = 0x8D4;
            if (Framerate_30) TimerB = 0x75D;
            if (Framerate_18) TimerB = 0xC44;   /* 18.000 fps: 32 MHz / (0x236 * 0xC45) */
        }


        Preview_H     = 1916;
        Preview_V     = 1280;
        Preview_R     = 0x19000D;

        YUV_HD_S_H    = 0x450080;
        YUV_HD_S_V    = 0x250044;

        Black_Bar     = 0;
        Preview_Control = 1;
        EDMAC_24_Redirect = 0;
        EDMAC_9_Vertical_Change = 0;
        Preview_Control_Basic = 0;
    }
    
    if (CROP_1080p)
    {
        if (is_EOSM)
        {
            RAW_H    = 0x202 + reg_width;
            RAW_V    = 0x455 + reg_height;
            TimerA   = 0x235;
            if (Framerate_24) TimerB = 0x935;
            if (Framerate_25) TimerB = 0x8D4;
            if (Framerate_30) TimerB = 0x75D;
        }


        Preview_H     = 1916 + reg_Preview_H;
        Preview_V     = 1080 + reg_Preview_V;
        Preview_R     = 0x19000D;

        YUV_HD_S_H    = 0x450080 + YUV_HD_S_H_width;
        YUV_HD_S_V    = 0x250039 + YUV_HD_S_H_height;

        Black_Bar     = 0;
        Preview_Control = 1;
        EDMAC_24_Redirect = 0;
        EDMAC_9_Vertical_Change = 0;
        Preview_Control_Basic = 0;
    }

    if (CROP_Full_Res) /* 5208x3478 — EOS M slim LV @ 3 FPS; other Digic5 @ 2 FPS */
    {
        if (is_EOSM)
        {
            RAW_H    = 0x538 + reg_width;
            RAW_V    = 0xDB3 + reg_height;
            /* 32000000 / (0x56B * TimerB) ≈ fps */
            TimerB   = is_EOSM ? 0x1E0A : (0x1E03 + 3840); /* EOSM ~3fps; others ~2fps */
            TimerA   = 0x56B;
        }


        Preview_x1 = 0x217;
        Preview_x2 = 0x31F;
        Preview_y1 = 0x593;
        Preview_y2 = 0x84C;

        Preview_Control = 0;
        EDMAC_24_Redirect = 1;
        Preview_V_Recover = 820;  // is this dangrous? this exceeds vertical EDMAC#9 size in photo mode which is 3529 (to 4326)  
        Preview_Control_Basic = 1;
        EDMAC_9_Vertical_Change = 1;
    }

    if (!CROP_3K)
    {
        REG_C0F383DC_Tuning = 0;
    }

    if (Preview_Control)
    {
        if (EDMAC_9_Vertical_Change)
        {
            if (MEM(EDMAC_9_Vertical_1) != (RAW_V - 1) + Preview_V_Recover ||
                MEM(EDMAC_9_Vertical_2) != (RAW_V - 1) + Preview_V_Recover) // set our new value if not set yet
            {
                MEM(EDMAC_9_Vertical_1)  = (RAW_V - 1) + Preview_V_Recover ;
                MEM(EDMAC_9_Vertical_2)  = (RAW_V - 1) + Preview_V_Recover;
            }

            switch (reg)
            {
                case 0xC0F08184: return (RAW_V - 1) + Preview_V_Recover; // used to exceed vertical preview limit
            }
        }
    }

    if (Preview_Control_Basic)
    {
        if (EDMAC_9_Vertical_Change)
        {
            if (MEM(EDMAC_9_Vertical_1) != (RAW_V - 1) + Preview_V_Recover ||
                MEM(EDMAC_9_Vertical_2) != (RAW_V - 1) + Preview_V_Recover) // set our new value if not set yet
            {
                MEM(EDMAC_9_Vertical_1)  = (RAW_V - 1) + Preview_V_Recover ;
                MEM(EDMAC_9_Vertical_2)  = (RAW_V - 1) + Preview_V_Recover;
            }

            switch (reg)
            {
               case 0xC0F08184: return (RAW_V - 1) + Preview_V_Recover; // used to exceed vertical preview limit
            }
        }
    }

    /* true 23.976 fps (see ntsc24_snap_timers) */
    ntsc24_snap_timers();

    /* get rid of moving Dual ISO lines by tweaking Timer B a tiny bit */
    if (fix_dual_iso_flicker && dual_iso_is_enabled())
    {
        TimerB = Adjust_TimerB_For_Dual_ISO(TimerB);
    }

    switch (reg)
    {
        case 0xC0F06804: return (RAW_V << 16) + RAW_H;

        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        {
            return RAW_H + 0x32;
        }

        case 0xC0F0713c: return RAW_V + 0x1;
        case 0xC0F07150: return RAW_V - 0x3A;

        case 0xC0F06014: return TimerB;
        case 0xC0F06010: return TimerA;
        case 0xC0F06008: return TimerA + (TimerA << 16);
        case 0xC0F0600C: return TimerA + (TimerA << 16);
    }

    return 0;
}



static inline uint32_t reg_override_1X3(uint32_t reg, uint32_t old_val)
{
    if (Anam_FLV)
    {
        RAW_H         = 0x1D4 + reg_width;  // from mv1080 mode
        RAW_V         = 0xDB3 + reg_height;
        TimerB        = OUTPUT_10BIT ? 0xf05 - fps_over: (OUTPUT_12BIT || OUTPUT_11BIT) ? 0x112b - fps_over: OUTPUT_14BIT ? 0x1407 - fps_over: 0;
        TimerA        = 0x207 + TimerA_Debug;
        
        //From AR_2_35_1
        Preview_H     = 1728;      // from mv1080 mode
        Preview_V     = 3478;
        Preview_R     = 0x1D000E;  // from mv1080 mode
        YUV_HD_S_H    = 0x10501B5 + YUV_HD_S_H_width + (YUV_HD_S_H_height << 16);
        YUV_HD_S_V    = 0x45015C + YUV_HD_S_V_width + (YUV_HD_S_V_height << 16);
        
        //Works well with focus aid for 1x3 presets
        //EngDrvOutLV(0xc0f11A88, 0x1);
        //YUV_HD_S_H    = 0x10501B5 + YUV_HD_S_H_width - (90 << 16);
        //YUV_HD_S_V    = 0x45015C + 800 + (1000 << 16);
    }
    else
    {
        if (AR_16_9)
        {
            if (Anam_Highest) /* 1504x2538 */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x19A + reg_width; /*  @ 22.250 FPS */
                    RAW_V         = 0xA07 + reg_height;
                    TimerB        = 0xAF7;
                    TimerA        = 0x1FF;  // Danne confirmed that EOS M has 0x1FF limit. it seems same as 100D
                }
                
                
                Preview_H     = 1500;
                Preview_V     = 2538;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x105017B;
                YUV_HD_S_V    = 0x10503BE;
            }
            
            if (Anam_Higher)
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x17A + reg_width;  /* 1376x2322 to achieve 23.976 FPS */
                    RAW_V         = 0x92f + reg_height;
                    TimerB        = 0xA2E;
                    TimerA        = 0x1FF;
                    
                    Preview_H     = 1372;
                    Preview_V     = 2322;
                    Preview_R     = 0x1D000D;
                    YUV_HD_S_H    = 0x105015B + YUV_HD_S_H_width + (YUV_HD_S_H_height << 16);
                    YUV_HD_S_V    = 0x105036D + YUV_HD_S_V_width + (YUV_HD_S_V_height << 16);
                }
                
            }
            
            if (Anam_Medium) /* 1280x2160 @ 23.976 and 25 FPS */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x162 + reg_width;
                    RAW_V         = 0x88D + reg_height;
                    TimerA        = 0x1FF;
                    if (Framerate_24) TimerB = 0xA2E;
                    if (Framerate_25) TimerB = 0x9C3;
                    if (Framerate_30) TimerB = 0x9C3; // 30 Doesn't work, make it 25
                }
                
                
                Preview_H     = 1276;
                Preview_V     = 2160;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x1050142;
                YUV_HD_S_V    = 0x105032F;
            }
        }
        
        if (AR_2_1)
        {
            if (Anam_Highest) /* 1600x2400 */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x1B2 + reg_width; /* @ 23.300 FPS */
                    RAW_V         = 0x97D + reg_height;
                    TimerB        = 0xA79;
                    TimerA        = 0x1FF;
                }
                
                
                Preview_H     = 1596;
                Preview_V     = 2400;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x1050193;
                YUV_HD_S_V    = 0x1050389;
            }
            
            if (Anam_Higher) /* 1472x2208 @ 23.976 and 25 FPS */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x192 + reg_width;
                    RAW_V         = 0x8BD + reg_height;
                    TimerA        = 0x1FF;
                    if (Framerate_24) TimerB = 0xA2D;
                    if (Framerate_25) TimerB = 0x9C3;
                    if (Framerate_30) TimerB = 0x9C3; // 30 Doesn't work, make it 25
                }
                
                
                Preview_H     = 1468;
                Preview_V     = 2208;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x1050173;
                YUV_HD_S_V    = 0x1050341;
            }
            
            if (Anam_Medium) /* 1360x2040 @ 23.976 and 25 FPS */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x176 + reg_width;
                    RAW_V         = 0x815 + reg_height;
                    TimerA        = 0x1FF;
                    if (Framerate_24) TimerB = 0xA2D;
                    if (Framerate_25) TimerB = 0x9C3;
                    if (Framerate_30) TimerB = 0x9C3; // 30 Doesn't work, make it 25
                }
                
                
                Preview_H     = 1356;
                Preview_V     = 2040;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x1050157;
                YUV_HD_S_V    = 0x1050301;
            }
        }
        
        if (AR_2_20_1)
        {
            if (Anam_Highest)
            {
                
                if (is_EOSM) /* 1664x2268 @ 23.976 FPS */
                {
                    RAW_H         = 0x1C2 + reg_width;
                    RAW_V         = 0x8F9 + reg_height;
                    TimerB        = 0xA2D;
                    TimerA        = 0x1FF;
                    
                    Preview_H     = 1660;
                    Preview_V     = 2268;
                    Preview_R     = 0x1D000D;
                    YUV_HD_S_H    = 0x10501A3;
                    YUV_HD_S_V    = 0x1050359;
                }
                
            }
            
            if (Anam_Higher) /* 1552x2218 @ 23.976 FPS */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x1A6 + reg_width;
                    RAW_V         = 0x863 + reg_height;
                    TimerA        = 0x1FF;
                    if (Framerate_24) TimerB = 0xA2E;
                    if (Framerate_25) TimerB = 0x9C3;
                    if (Framerate_30) TimerB = 0x9C3; // 30 Doesn't work, make it 25
                }
                
                
                Preview_H     = 1548;
                Preview_V     = 2216;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x1050187;
                YUV_HD_S_V    = 0x105031C;
            }
            
            if (Anam_Medium) /* 1424x1942 @ 23.976 FPS */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x186 + reg_width;
                    RAW_V         = 0x7B3 + reg_height;
                    TimerA        = 0x1FF;
                    if (Framerate_24) TimerB = 0xA2E;
                    if (Framerate_25) TimerB = 0x9C3;
                    if (Framerate_30) TimerB = 0x9C3; // 30 Doesn't work, make it 25
                }
                
                
                Preview_H     = 1420;
                Preview_V     = 1942;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x1050167;
                YUV_HD_S_V    = 0x10502DB;
            }
        }
        
        if (AR_2_35_1)
        {
            if (Anam_Highest) /* 1736x2216 @ 23.976 FPS */
            {
                if (is_EOSM)
                {
                    RAW_H         = 0x1D4 + reg_width;  // from mv1080 mode
                    RAW_V         = 0x8C3 + reg_height;
                    TimerB        = 0xA07;
                    TimerA        = 0x207;
                }
                
                
                Preview_H     = 1728;      // from mv1080 mode
                Preview_V     = 2214;
                Preview_R     = 0x1D000E;  // from mv1080 mode
                YUV_HD_S_H    = 0x10501B5;
                YUV_HD_S_V    = 0x1050341;
            }
            
            if (Anam_Higher) /* 1600x2040 @ 23.976 FPS */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x1B2 + reg_width;
                    RAW_V         = 0x815 + reg_height;
                    TimerA        = 0x1FF;
                    if (Framerate_24) TimerB = 0xA2D;
                    if (Framerate_25) TimerB = 0x9C3;
                    if (Framerate_30) TimerB = 0x9C3; // 30 Doesn't work, make it 25
                }
                
                
                Preview_H     = 1596;
                Preview_V     = 2040;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x1050193;
                YUV_HD_S_V    = 0x1050301;
            }
            
            if (Anam_Medium) /* 1472x1878 @ 23.976 FPS */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x192 + reg_width;
                    RAW_V         = 0x773 + reg_height;
                    TimerA        = 0x1FF;
                    if (Framerate_24) TimerB = 0xA2D;
                    if (Framerate_25) TimerB = 0x9C3;
                    if (Framerate_30) TimerB = 0x9C3; // 30 Doesn't work, make it 25
                }
                
                
                Preview_H     = 1468;
                Preview_V     = 1878;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x1050173;
                YUV_HD_S_V    = 0x10502C4;
            }
        }
        
        if (AR_2_39_1)
        {
            if (Anam_Highest) /* 1736x2178 @ 23.976 FPS */
            {
                if (is_EOSM)
                {
                    RAW_H         = 0x1D4 + reg_width;  // from mv1080 mode
                    RAW_V         = 0x89f + reg_height;
                    TimerB        = 0xA05;
                    TimerA        = 0x207;
                }
                
                
                Preview_H     = 1728;      // from mv1080 mode
                Preview_V     = 2178;
                Preview_R     = 0x1D000E;  // from mv1080 mode
                YUV_HD_S_H    = 0x10501B5;
                YUV_HD_S_V    = 0x1050336;
            }
            
            if (Anam_Higher) /* 1600x2008 @ 23.976 FPS */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x1B2 + reg_width;
                    RAW_V         = 0x7F5 + reg_height;
                    TimerA        = 0x1FF;
                    if (Framerate_24) TimerB = 0xA2D;
                    if (Framerate_25) TimerB = 0x9C3;
                    if (Framerate_30) TimerB = 0x9C3; // 30 Doesn't work, make it 25
                }
                
                
                Preview_H     = 1596;
                Preview_V     = 2008;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x1050193;
                YUV_HD_S_V    = 0x10502F4;
            }
            
            if (Anam_Medium) /* 1472x1846 @ 23.976 FPS */
            {
                
                if (is_EOSM)
                {
                    RAW_H         = 0x192 + reg_width;
                    RAW_V         = 0x753 + reg_height;
                    TimerA        = 0x1FF;
                    if (Framerate_24) TimerB = 0xA2D;
                    if (Framerate_25) TimerB = 0x9C3;
                    if (Framerate_30) TimerB = 0x9C3; // 30 Doesn't work, make it 25
                }
                
                
                Preview_H     = 1468;
                Preview_V     = 1846;
                Preview_R     = 0x1D000D;
                YUV_HD_S_H    = 0x1050173;
                YUV_HD_S_V    = 0x10502B6;
            }
        }
    }
    Black_Bar = 0;
    YUV_HD_S_V_E  = 0;
    Preview_Control = 1;
    EDMAC_24_Redirect = 1;
    EDMAC_9_Vertical_Change = 1;
    Preview_Control_Basic = 0;
    REG_C0F383DC_Tuning = 0;

    if (Preview_Control)
    {
        if (EDMAC_9_Vertical_Change)
        {
            if (MEM(EDMAC_9_Vertical_1) != RAW_V - 1 || MEM(EDMAC_9_Vertical_2) != RAW_V - 1) // set our new value if not set yet
            {
                MEM(EDMAC_9_Vertical_1)  = RAW_V - 1;
                MEM(EDMAC_9_Vertical_2)  = RAW_V - 1;
            }

            switch (reg)
            {
                case 0xC0F08184: return RAW_V - 1; // used to exceed vertical preview limit
            }
        }
    }

    /* true 23.976 fps (see ntsc24_snap_timers) */
    ntsc24_snap_timers();

    /* get rid of moving Dual ISO lines by tweaking Timer B a tiny bit */
    if (fix_dual_iso_flicker && dual_iso_is_enabled())
    {
        TimerB = Adjust_TimerB_For_Dual_ISO(TimerB);
    }

    switch (reg)
    {
        case 0xC0F06804: return (RAW_V << 16) + RAW_H;

        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        {
            return RAW_H + 0x32;
        }

        case 0xC0F0713c: return RAW_V + 0x1;
        case 0xC0F07150: return RAW_V - 0x3A;

        case 0xC0F06014: return TimerB;
        case 0xC0F06010: return TimerA;
        case 0xC0F06008: return TimerA + (TimerA << 16);
        case 0xC0F0600C: return TimerA + (TimerA << 16);
    }

    return 0;
}

/* these presets have some preview quirks mainly because we lowered RAW resolution lower than default
   RAW resolution in x5 mode, some tasks expect a minimal amount of RAW resolution, otherwise some issues
   start to appear. in our case it's a frozen preview on YUV (LV) path.
   on 700D, for 1736x976, 1736x868 and 1736x790 presets and in order to make preview work, the focus
   box should be centered on screen + lower it one step by using arrow down button. when lowering focus box,
   some functions are being called and set some new values, I don't know which exact function we are looking for.
   in 1736x738 preset preview will be always frozen, changing focus box position doesn't help, but a workaround
   which make preview work with mentioned presets is suspending aewb task, the issue seems related to aewb task 
   in 1736x694 suspending aewb task doesn't help, preview is still frozen, pretty sure it can be solved by lowering
   some preview registers related to RAW resolution (not the ones in crop_rec.c), currently I don't know how to do it,
   or where to look exactly, for 1736x694 it could from YUV (HD) path resolution too */
static inline uint32_t reg_override_3X3(uint32_t reg, uint32_t old_val)
{
    if (High_FPS)
    {
        if (AR_16_9)
        {
            if (is_EOSM) // 1736x976 @ 46.800 FPS
            {
                RAW_H         = 0x1D4 + reg_width;
                RAW_V         = 0x3ED + reg_height;
                TimerB        = crop_preset_fps_reduce == 0x1 ? 0x5a2: 0x50E;
                TimerA        = 0x20F;  // can go lower down to 0x207
            }
            
            
            Preview_H     = 1728;      // from mv1080 mode
            Preview_V     = 976;
            Preview_R     = 0x1D000E;  // from mv1080 mode
            YUV_HD_S_V    = 0x105016C;
        }
        
        if (AR_2_1)
        {
            if (is_EOSM) // 1736x868 @ 50 FPS
            {
                RAW_H         = 0x1D4 + reg_width;
                RAW_V         = 0x381 + reg_height;
                TimerB        = crop_preset_fps_reduce == 0x1 ? 0x501: 0x4CD;
                TimerA        = 0x207;
            }
            
            
            Preview_H     = 1728;
            Preview_V     = 868;
            Preview_R     = 0x1D000E;
            YUV_HD_S_V    = 0x1050143;
        }
        
        if (AR_2_20_1)
        {
            if (is_EOSM) // 1736x790 @ 54 FPS
            {
                RAW_H         = 0x1D4 + reg_width;
                RAW_V         = 0x333 + reg_height;
                TimerB        = crop_preset_fps_reduce == 0x1 ? 0x4ce: 0x472;
                TimerA        = 0x207;
            }
            
            
            Preview_H     = 1728;
            Preview_V     = 790;
            Preview_R     = 0x1D000E;
            YUV_HD_S_V    = 0x1050125;
        }
        
        if (AR_2_35_1)
        {
            if (is_EOSM) // 1736x738 @ 57 FPS
            {
                RAW_H         = 0x1D4 + reg_width;
                RAW_V         = 0x2FF + reg_height;
                TimerB        = crop_preset_fps_reduce == 0x1 ? 0x4ce: 0x436;
                TimerA        = 0x207;
            }
            
            
            Preview_H     = 1728;
            Preview_V     = 738;
            Preview_R     = 0x1D000E;
            YUV_HD_S_V    = 0x1050112;
        }
        
        if (AR_2_39_1 && crop_preset_fps_reduce == 1 && is_EOSM)
        {
            // 1736x726 @ 57 FPS
            RAW_H         = 0x1D4 + reg_width;
            RAW_V         = 0x2F3 + reg_height;
            TimerB        = crop_preset_fps_reduce == 0x1 ? 0x4ce: 0x436;
            TimerA        = 0x207;
            
            Preview_H     = 1728;
            Preview_V     = 726;
            Preview_R     = 0x1D000E;
            YUV_HD_S_V    = 0x1050106;
        }
        
        if (AR_2_39_1 && crop_preset_fps_reduce == 0)  // 2.39:1 doesn't make sense, very similair to 2.35:1, let's make it 2.50:1
        {
            if (is_EOSM) // 1736x694 @ 60 FPS
            {
                RAW_H         = 0x1D4 + reg_width;
                RAW_V         = 0x2D2 + reg_height;
                TimerB        = 0x401;
                TimerA        = 0x207;
            }
            
            
            Preview_H     = 1728;
            Preview_V     = 694;
            Preview_R     = 0x1D000E;
            YUV_HD_S_V    = 0;          // default x5 mode value
            
            YUV_LV_S_V    = 0x1E002B;   // default x5 mode value
            YUV_LV_Buf    = 0x1DF05A0;  // default x5 mode value
        }
    }
    
    /* mv1080 preset made to enable 1080p mode mainly for EOS M (other models don't really need it) */
    if (mv1080)
    {
        if (is_EOSM)
        {
            RAW_H         = 0x1D4 + reg_width;
            RAW_V         = 0x4A5 + reg_height;
        }
        
        if (Framerate_24) {TimerA = 0x20F; TimerB = 0x9DE;}
        if (Framerate_25) {TimerA = 0x27F; TimerB = 0x7CF;}
        if (Framerate_30) {TimerA = 0x20F; TimerB = 0x7E4;}
        
        Preview_H     = 1728;      // from mv1080 mode
        Preview_V     = 1152;      // from mv1080 mode
        Preview_R     = 0x1D000E;  // from mv1080 mode
        YUV_HD_S_V    = 0x450072;
    }
    
    if (mv1080_3_2)
    {
        if (is_EOSM)
        {
            RAW_H         = 0x1D4 + reg_width;
            RAW_V         = 0x4A5 + reg_height;
        }
        
        if (Framerate_24) {TimerA = 0x20F; TimerB = 0x9DE;}
        if (Framerate_25) {TimerA = 0x27F; TimerB = 0x7CF;}
        if (Framerate_30) {TimerA = 0x20F; TimerB = 0x7E4;}
        
        Preview_H     = 1728;      // from mv1080 mode
        Preview_V     = 1152;      // from mv1080 mode
        Preview_R     = 0x1D000E;  // from mv1080 mode
        YUV_HD_S_V    = 0x450072;
    }
    
    YUV_HD_S_H    = 0x10501B5;

    Black_Bar = 0;
    YUV_HD_S_V_E  = 0;
    Preview_Control = 1;
    Preview_Control_Basic = 0;
    REG_C0F383DC_Tuning = 0;

    /* true 23.976 fps (see ntsc24_snap_timers) */
    ntsc24_snap_timers();

    /* get rid of moving Dual ISO lines by tweaking Timer B a tiny bit */
    if (fix_dual_iso_flicker && dual_iso_is_enabled())
    {
        TimerB = Adjust_TimerB_For_Dual_ISO(TimerB);
    }

    switch (reg)
    {
        case 0xC0F06804: return (RAW_V << 16) + RAW_H;

        case 0xC0F06824:
        case 0xC0F06828:
        case 0xC0F0682C:
        case 0xC0F06830:
        {
            return RAW_H + 0x32;
        }

        case 0xC0F0713c: return RAW_V + 0x1;
        case 0xC0F07150: return RAW_V - 0x3A;

        case 0xC0F06014: return TimerB;
        case 0xC0F06010: return TimerA;
        case 0xC0F06008: return TimerA + (TimerA << 16);
        case 0xC0F0600C: return TimerA + (TimerA << 16);
    }

    return 0;
}

static void * get_engio_reg_override_func()
{
    uint32_t (*reg_override_func)(uint32_t, uint32_t) = 
        /* EOS M reg_override_func presets */
        (crop_preset == CROP_PRESET_1X1)        ? reg_override_1X1        :
        (crop_preset == CROP_PRESET_1X3)        ? reg_override_1X3        :
        (crop_preset == CROP_PRESET_3X3)        ? reg_override_3X3        :
                                                  0                       ;
    return reg_override_func;
}

static void FAST engio_write_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    uint32_t (*reg_override_func)(uint32_t, uint32_t) = 
        get_engio_reg_override_func();

    if (!reg_override_func)
    {
        return;
    }

    // is engio_vidmode_ok still needed? PathDriveMode might be enough to detect video modes
    

    if (!is_supported_mode())
    {
        /* don't patch other video modes */
        return;
    }

    for (uint32_t * buf = (uint32_t *) regs[0]; *buf != 0xFFFFFFFF; buf += 2)
    {
        uint32_t reg = *buf;
        uint32_t old = *(buf+1);
        
        int new = reg_override_func(reg, old);
        if (new)
        {
            dbg_printf("[%x] %x: %x -> %x\n", regs[0], reg, old, new);
            *(buf+1) = new;
        }

        /* brighten up LiveView when using negative analog gain in lower bit-depths */

        // this method seems better in terms of stability, it doesn't produce corrupted frames,
        // but it also affect autofocus when using negative analog gain which makes it inaccurate.
        // (read about the other method for more info.)
        if ((brighten_lv_method == 0 && RECORDING) || (brighten_lv_method == 0 && RAW_HISTOGRAM_ENABLED))//When RAW histogram is used turn off the temporary 14bit stuff
        {
            //Workaround when small_hacks is set to More in mlv_lite.c
            if ((!Arrows_U_D && Arrows_L_R != 3 && SET_button != 2 && SET_button != 3) || more_hacks)
            {
                if (OUTPUT_12BIT)
                {
                    EngDrvOutLV(0xC0F42744, 0x2020202);
                }
                if (OUTPUT_11BIT)
                {
                    EngDrvOutLV(0xC0F42744, 0x3030303);
                }
                if (OUTPUT_10BIT)
                {
                    EngDrvOutLV(0xC0F42744, 0x4040404);
                }
            }
            else
            {
                
                if (reg == 0xC0F42744)
                {
                    if (which_output_format() >= 3) // don't patch if we are using uncompressed RAW
                    {
                        if (OUTPUT_12BIT && old != 0x2020202)
                        {
                            *(buf+1) = 0x2020202;
                        }
                
                        if (OUTPUT_11BIT && old != 0x3030303)
                        {
                            *(buf+1) = 0x3030303;
                        }
                
                        if (OUTPUT_10BIT && old != 0x4040404)
                        {
                            *(buf+1) = 0x4040404;
                        }
                    }
                }
            }
        }

        /* it seems more reliable to override them directly from here */
        if (Preview_Control)
        {
            switch (reg)
            {
                case 0xC0F1A00C: *(buf+1) = (Preview_V << 16) + Preview_H - 0x1;  break;
                case 0xC0F11B9C: *(buf+1) = (Preview_V << 16) + Preview_H - 0x1;  break;

                case 0xC0F11B8C: *(buf+1) = YUV_HD_S_H;                           break;
                case 0xC0F11BCC: *(buf+1) = YUV_HD_S_V;                           break;
                case 0xC0F11BC8: *(buf+1) = YUV_HD_S_V_E;                         break;
                case 0xC0F11ACC: *(buf+1) = YUV_LV_S_V;                           break;
                case 0xC0F04210: *(buf+1) = YUV_LV_Buf;                           break;
            }
        }
    }
}

static int change_buffer_now = 0;

static void FAST EngDrvOut_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (!is_supported_mode())
    {
        /* don't patch other video modes */
        return;
    }
    
    uint32_t data = (uint32_t) regs[0];
    uint16_t dst = (data & 0xFFFF0000) >> 16;
    uint16_t reg = data & 0x0000FFFF;
    uint32_t val = (uint32_t) regs[1];

    /* brighten up LiveView when using negative analog gain in lower bit-depths */
    /* this method may produce corrupted frames in some settings (in lowest bit-depth, high ISO and resolution combos?) 
     * but it has accurate AF ...                                                                                   */

    /* These four registers apply positive gain for preview per RGB channel, two for green, one for red, one for blue.
     * The pervious register which was used to brighten LV is 0xC0F42744, altough it can correct preview brightness
     * in LiveView, but according to autofocus data it will stay underexposed, resulting in autofocus failure/loss when
     * using lower bit-depths in lossless. The following four regisers can correct LV image brightness beside it will
     * affect autofocus data --> the four regisers will adjust autofocus data to the correct brightness too . .
     * --> This way autofocus in 10/11/12-bit will be as accurate as in 14-bit.
     * BTW, these regisers used to achieve 12800 digital ISO from Canon.                                           */
    if ((brighten_lv_method == 1 && RECORDING) || (brighten_lv_method == 1 && RAW_HISTOGRAM_ENABLED))
    {
        if (data == 0xC0F37AE4 || data == 0xC0F37AF0 || data == 0xC0F37AFC || data == 0xC0F37B08) 
        {
            if (which_output_format() >= 3) // don't patch if we are using uncompressed RAW 
            {
                if (OUTPUT_12BIT) regs[1] = 0x30100;
                if (OUTPUT_11BIT) regs[1] = 0x40100;
                if (OUTPUT_10BIT) regs[1] = 0x50100;
            }
        }
    }

    // adjust LiveView black level when using lower bit-depths with negative analog gain
    if (data == 0xC0F0819C)
    {
        // 100D doesn't need this
        if (is_EOSM)
        {
            if (lens_info.iso_analog_raw == ISO_400)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC39;
                if (OUTPUT_11BIT) regs[1] = 0xC39;
                if (OUTPUT_12BIT) regs[1] = 0xC39;
            }
            if (lens_info.iso_analog_raw == ISO_800)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC3C;
                if (OUTPUT_11BIT) regs[1] = 0xC3A;
                if (OUTPUT_12BIT) regs[1] = 0xC3A;
            }
            if (lens_info.iso_analog_raw == ISO_1600)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC41;
                if (OUTPUT_11BIT) regs[1] = 0xC40;
                if (OUTPUT_12BIT) regs[1] = 0xC40;
            }
            if (lens_info.iso_analog_raw == ISO_3200)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC4A;
                if (OUTPUT_11BIT) regs[1] = 0xC48;
                if (OUTPUT_12BIT) regs[1] = 0xC48;
            }
            if (lens_info.iso_analog_raw == ISO_6400)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC5C;
                if (OUTPUT_11BIT) regs[1] = 0xC5B;
                if (OUTPUT_12BIT) regs[1] = 0xC58;
            }
            if (lens_info.iso_analog_raw == ISO_12800)
            {
                if (OUTPUT_10BIT) regs[1] = 0xC5C;
                if (OUTPUT_11BIT) regs[1] = 0xC5A;
                if (OUTPUT_12BIT) regs[1] = 0xC58;
            }
        }
    }

    /* makes LiveView smoother when using 3x3 presets for is_DIGIC_5 models when aewb task is active 
     * the values taken when setting focus box to center then pressing down button one time in x5 mode.
     * it seems these also make preview more reliable in these modes, in some focus box positions preview
     * become black without these values */
    if ((CROP_PRESET_MENU == CROP_PRESET_3X3 || CROP_PRESET_MENU == CROP_PRESET_1X3) ||
       ((CROP_PRESET_MENU == CROP_PRESET_1X1))) // also for 1280p preset
    {
        if (data == 0xC0F09050) {regs[1] =   0x3002D0;}
        if (data == 0xC0F09054) {regs[1] =  0x2E006D8;}
    }

    if (dst == 0xC0F2)
    {
        // 0xC0F26808 register sets EDMAC#24 buffer address, change it to Photo mode buffer address
        if (EDMAC_24_Redirect)
        {
            // we need to know when to override 0xC0F26808, detect it from 0xC0F26804, it's always
            // being set to 0x40000000 before setting 0xC0F26808 value
            if (reg == 0x6804 && val == 0x40000000) 
            {
                change_buffer_now = 1;
            }
    
            if (reg == 0x6808 && change_buffer_now == 1) // 0xC0F26808
            {
                if (is_EOSM)
                {
                    regs[1] = 0x1595b00; // Size 0xC0F26810  = 0x3237e  is being set in EngDrvOuts_hook
                }
            
            
                change_buffer_now = 0;
            }
        }
    }

    /* set our preview registers overrides */
    if (dst == 0xC0F3)
    {
        if (Preview_Control)
        {
            switch (reg)
            {
                case 0x8070: regs[1] = ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5;       break;
                case 0x8078: regs[1] = (((Preview_H / 4) + 6) << 16) + 1;                   break;
                case 0x807C: regs[1] = ((Preview_H / 4) + 5) << 16;                         break;
                case 0x8080: regs[1] = ((Preview_V + 0x7) << 16) + 2;                       break;
                case 0x8084: regs[1] = ((Preview_H / 4) + 7) << 16;                         break;
                case 0x8094: regs[1] = ( Preview_V + 0xa) << 16;                            break;
                case 0x80A0: regs[1] = ((Preview_H / 4) + 7) << 16;                         break;
                case 0x80A4: regs[1] = ((Preview_H / 4) + 7) << 16;                         break;
                case 0x8024: 
                if (is_EOSM)
                             regs[1] = ((RAW_V - 1) << 16)  + RAW_H - 0x11;                 
                 break;
                case 0x83D4: regs[1] =   Preview_R;                                         break;
                case 0x83DC: regs[1] = ((Preview_V + 0x1c) << 16)  + Preview_H / 4 + 0x48
                                                                   + REG_C0F383DC_Tuning;   break;
                case 0x8934: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5;     break;
                case 0x8960: regs[1] = ( Preview_V + 0x6) << 16;                            break;
                case 0x89A4: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5;     break;
                case 0x89B4: regs[1] = ((Preview_V + 0x7) << 16)   + Preview_H / 4 + 6;     break;
                case 0x89D4: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5;     break;
                case 0x89E4: regs[1] = ((Preview_V + 0x7) << 16)   + Preview_H / 4 + 7;     break;
                case 0x89EC: regs[1] = ((Preview_H / 4 + 6) << 16) + 1;                     break;
                
            //  case 0xA04C: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5;     break; // It's being set in EngDrvOuts_hook
            //  case 0xA0A0: regs[1] = ((Preview_V + 0xa) << 16)   + Preview_H + 0xb;       break; // It's being set in EngDrvOuts_hook
            //  case 0xA0B0: regs[1] = ((Preview_V + 0xa) << 16)   + Preview_H + 0x8;       break; // It's being set in EngDrvOuts_hook
                case 0xB038: regs[1] =  Black_Bar;                                          break;
                case 0xB088: regs[1] =  Black_Bar;                                          break;
                case 0xB054: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H + 0x7;       break;
                case 0xB070: regs[1] = ((Preview_V + 0x6) << 16)   + Preview_H + 0x57;      break;
                case 0xB074: regs[1] = ( Preview_V        << 16)   + Preview_H + 0x57;      break;
                case 0xB0DC: regs[1] = ( Preview_V        << 16)   + Preview_H + 0x4f;      break;
            }
        }

        // basic preview: fix broken preview casued by increasing width RAW resolution, also center the preview
        if (Preview_Control_Basic)
        {
            switch (reg)
            {
                case 0x8024: 
                if (is_EOSM)
                             regs[1] = ((RAW_V - 1) << 16)  + RAW_H - 0x11;                 
                 break;
                
                /* used here to center Canon cropped preview on RAW buffer */
                case 0x83D4: regs[1] =  (Preview_y1 << 16) + Preview_x1;                    break;
                case 0x83DC: regs[1] =  (Preview_y2 << 16) + Preview_x2;                    break;
            }
        }
    }

    if (dst == 0xC0F4)
    {
        if (Preview_Control)
        {
            switch (reg)
            {
                case 0x2014: regs[1] = ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5;      break;
                case 0x204C: regs[1] = ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5;      break;
                case 0x2194: regs[1] = ( Preview_H / 4) + 5;                               break;
            }
        }
    }
}

static void FAST EngDrvOuts_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (!is_supported_mode())
    {
        /* don't patch other video modes */
        return;
    }

    uint32_t data = (uint32_t) regs[0];
//  uint16_t dst = (data & 0xFFFF0000) >> 16;
//  uint16_t reg = data & 0x0000FFFF;
//  uint32_t * val = (uint32_t*) regs[1];
//  uint32_t num = (uint32_t) regs[2];

    if (Preview_Control)
    {
        /* set our preview registers overrides */
        if (data == 0xC0F3A048)
        {
            *(uint32_t*) (regs[1] + 4)    = ((Preview_V + 0x6) << 16) + Preview_H / 4 + 5; // 0xC0F3A04C
        }
        
        if (data == 0xC0F3A098)
        {
            *(uint32_t*) (regs[1] + 8)    = ((Preview_V + 0xa) << 16) + Preview_H + 0xb;   // 0xC0F3A0A0
            *(uint32_t*) (regs[1] + 0x18) = ((Preview_V + 0xa) << 16) + Preview_H + 0x8;   // 0xC0F3A0B0
        }
    }

    /* change EDMAC#24 buffer size 0xC0F26810 to photo mode buffer size */
    if (EDMAC_24_Redirect)
    {
        if (data == 0xC0F2680C)
        {
            // we need to know when to set buffer size because the channel does other things before
            // setting the final buffer size which we want to change, let's use 0xC0F35084 as flag because
            // it's always being set after "the other things" finish and before setting 0xC0F26810 final size
            if (shamem_read(0xC0F35084) == 0xA1F)
            {
                if (is_EOSM)
                {
                    *(uint32_t*) (regs[1] + 4) = 0x3237e;
                }

            }
        }  
    }
}

/* sometime and for some reaseon not all preview registers get overriden, especially the ones in engio_write hook and when HDMI is connected */
/* while idle let's check preview registers values, if they don't match our values, set values using EngDrvOut call */
uint32_t REG_C0F38024_Val = 0;
void CheckPreviewRegsValuesAndForce()
{
    if (!lv) return;
    if (!CROP_PRESET_MENU) return;
    if (lv_dispsize != 5) return;
    if (PathDriveMode->zoom != 5) return;
    if (Preview_Control_Basic) return;

#ifdef CONFIG_EOSM
    /* EOS M: engio hooks already patch preview; forcing registers here fights Canon
     * and stalls the whole UI (laggy audio meters, menu won't open). Recovery uses
     * normal x5 zoom path instead. */
    return;
#endif

    if (is_EOSM) REG_C0F38024_Val = ((RAW_V - 1) << 16)  + RAW_H - 0x11;

    if (shamem_read(0xC0F38070) != ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5      ||
        shamem_read(0xC0F38078) != (((Preview_H / 4) + 6) << 16) + 1                  ||
        shamem_read(0xC0F3807C) != ((Preview_H / 4) + 5) << 16                        ||
        shamem_read(0xC0F38080) != ((Preview_V + 0x7) << 16) + 2                      ||
        shamem_read(0xC0F38084) != ((Preview_H / 4) + 7) << 16                        ||
        shamem_read(0xC0F38094) != ( Preview_V + 0xa) << 16                           ||
        shamem_read(0xC0F380A0) != ((Preview_H / 4) + 7) << 16                        ||
        shamem_read(0xC0F380A4) != ((Preview_H / 4) + 7) << 16                        ||
        shamem_read(0xC0F38024) != REG_C0F38024_Val                                   ||
        shamem_read(0xC0F383D4) != Preview_R                                          ||
        shamem_read(0xC0F383DC) != ((Preview_V + 0x1c) << 16)  + Preview_H / 4 + 0x48 
                                                               + REG_C0F383DC_Tuning  ||
        shamem_read(0xC0F38934) != ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5    ||
        shamem_read(0xC0F38960) != ( Preview_V + 0x6) << 16                           ||
        shamem_read(0xC0F389A4) != ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5    ||
        shamem_read(0xC0F389B4) != ((Preview_V + 0x7) << 16)   + Preview_H / 4 + 6    ||
        shamem_read(0xC0F389D4) != ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5    ||
        shamem_read(0xC0F389E4) != ((Preview_V + 0x7) << 16)   + Preview_H / 4 + 7    ||
        shamem_read(0xC0F389EC) != ((Preview_H / 4 + 6) << 16) + 1                    ||
        shamem_read(0xC0F42014) != ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5      ||
        shamem_read(0xC0F4204C) != ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5      ||
        shamem_read(0xC0F42194) != ( Preview_H / 4) + 5                               ||
        shamem_read(0xC0F3A04C) != ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5    ||
        shamem_read(0xC0F3A0A0) != ((Preview_V + 0xa) << 16)   + Preview_H + 0xb      ||
        shamem_read(0xC0F3A0B0) != ((Preview_V + 0xa) << 16)   + Preview_H + 0x8      ||
        shamem_read(0xC0F3B054) != ((Preview_V + 0x6) << 16)   + Preview_H + 0x7      ||
        shamem_read(0xC0F3B070) != ((Preview_V + 0x6) << 16)   + Preview_H + 0x57     ||
        shamem_read(0xC0F3B074) != ( Preview_V        << 16)   + Preview_H + 0x57     ||
        shamem_read(0xC0F3B0DC) != ( Preview_V        << 16)   + Preview_H + 0x4f     ||
        shamem_read(0xC0F1A00C) != (Preview_V << 16) + Preview_H - 0x1                ||
        shamem_read(0xC0F11B9C) != (Preview_V << 16) + Preview_H - 0x1                ||
        shamem_read(0xC0F11B8C) != YUV_HD_S_H                                         ||
        shamem_read(0xC0F11BCC) != YUV_HD_S_V                                         ||
        shamem_read(0xC0F11BC8) != YUV_HD_S_V_E                                       ||
        shamem_read(0xC0F11ACC) != YUV_LV_S_V                                         ||
        shamem_read(0xC0F04210) != YUV_LV_Buf                                          )
        {
            EngDrvOutLV(0xC0F38070, ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5);
            EngDrvOutLV(0xC0F38078, (((Preview_H / 4) + 6) << 16) + 1);
            EngDrvOutLV(0xC0F3807C, ((Preview_H / 4) + 5) << 16);
            EngDrvOutLV(0xC0F38080, ((Preview_V + 0x7) << 16) + 2);
            EngDrvOutLV(0xC0F38084, ((Preview_H / 4) + 7) << 16);
            EngDrvOutLV(0xC0F38094, ( Preview_V + 0xa) << 16);
            EngDrvOutLV(0xC0F380A0, ((Preview_H / 4) + 7) << 16);
            EngDrvOutLV(0xC0F380A4, ((Preview_H / 4) + 7) << 16);
            EngDrvOutLV(0xC0F38024, REG_C0F38024_Val);
            EngDrvOutLV(0xC0F383D4, Preview_R);
            EngDrvOutLV(0xC0F383DC, ((Preview_V + 0x1c) << 16)  + Preview_H / 4 + 0x48 
                                                                + REG_C0F383DC_Tuning);
            EngDrvOutLV(0xC0F38934, ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5);
            EngDrvOutLV(0xC0F38960, ( Preview_V + 0x6) << 16);
            EngDrvOutLV(0xC0F389A4, ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5);
            EngDrvOutLV(0xC0F389B4, ((Preview_V + 0x7) << 16)   + Preview_H / 4 + 6);
            EngDrvOutLV(0xC0F389D4, ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5);
            EngDrvOutLV(0xC0F389E4, ((Preview_V + 0x7) << 16)   + Preview_H / 4 + 7);
            EngDrvOutLV(0xC0F389EC, ((Preview_H / 4 + 6) << 16) + 1);
            EngDrvOutLV(0xC0F42014, ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5);
            EngDrvOutLV(0xC0F4204C, ((Preview_V + 0x9) << 16) + Preview_H / 4 + 5);
            EngDrvOutLV(0xC0F42194, ( Preview_H / 4) + 5);
            EngDrvOutLV(0xC0F3A04C, ((Preview_V + 0x6) << 16)   + Preview_H / 4 + 5);
            EngDrvOutLV(0xC0F3A0A0, ((Preview_V + 0xa) << 16)   + Preview_H + 0xb);
            EngDrvOutLV(0xC0F3A0B0, ((Preview_V + 0xa) << 16)   + Preview_H + 0x8);
            EngDrvOutLV(0xC0F3B054, ((Preview_V + 0x6) << 16)   + Preview_H + 0x7);
            EngDrvOutLV(0xC0F3B070, ((Preview_V + 0x6) << 16)   + Preview_H + 0x57);
            EngDrvOutLV(0xC0F3B074, ( Preview_V        << 16)   + Preview_H + 0x57);
            EngDrvOutLV(0xC0F3B0DC, ( Preview_V        << 16)   + Preview_H + 0x4f);
            EngDrvOutLV(0xC0F1A00C, (Preview_V << 16) + Preview_H - 0x1);
            EngDrvOutLV(0xC0F11B9C, (Preview_V << 16) + Preview_H - 0x1);
            EngDrvOutLV(0xC0F11B8C, YUV_HD_S_H);
            EngDrvOutLV(0xC0F11BCC, YUV_HD_S_V);
            EngDrvOutLV(0xC0F11BC8, YUV_HD_S_V_E);
            EngDrvOutLV(0xC0F11ACC, YUV_LV_S_V);
            EngDrvOutLV(0xC0F04210, YUV_LV_Buf);
        }
}

// 0xC0F04908 register sets EDMAC#9 address, I think it holds darkframe subtraction data, change it to photo mode address (use darkframe data from photo mode)
// cleaner preview this way in presets which exceed default vertical RAW resolution (above 1080 vertical pixels), photo mode data should cover height up to 3528
// 0xC0F04908 changes among two addresses in LiveView, one dedicated for RAW_H and other one for RAW_V, we want to change the address for RAW_V (our hook does that)
// note: I am not sure what I am doing
static void Change_HIV_V_Address(uint32_t* regs, uint32_t* stack, uint32_t pc)     
{
    regs[1] = HIV_Vertical_Photo_Address;
}

int is_LCD_Output()
{
    if (PathDriveMode->OutputType == 0)
    {
        return 1;
    }

    return 0;
}

int is_480p_Output()
{
    if (PathDriveMode->OutputType == 7)
    {
        return 1;
    }

    return 0;
}

int is_1080i_Full_Output()
{
    if (PathDriveMode->OutputType == 3)
    {
        return 1;
    }

    return 0;
}

int is_1080i_Info_Output()
{
    if (PathDriveMode->OutputType == 4)
    {
        return 1;
    }

    return 0;
}

static uint32_t ShiftAddress  = 0;
static uint32_t ClearAddress  = 0;
static uint32_t DefaultShift  = 0; // expected value which we want to patch
static uint32_t DefaultClear  = 0; // expected value which we want to patch
static uint32_t NewShiftVal   = 0; // new shift value, trial and error
static uint32_t NewClearVal   = 0; // new clear value, should be same as width from 0xC0F04210?

int GetShiftValue()
{
    if (CROP_PRESET_MENU == CROP_PRESET_1X1)
    {
        switch (crop_preset_1x1_res)
         {
            case 0:
            case 1:
            {
                if (is_LCD_Output())  return 0x1F4A0;
                if (is_480p_Output()) return 0x1B160;
                if (is_1080i_Full_Output()) return 0x5BF2C;
                if (is_1080i_Info_Output()) return 0x5ED58;
            }

            case 2:
            {
                if (is_LCD_Output())  return 0x1F4D0;
                if (is_480p_Output()) return 0x1B18C;
                if (is_1080i_Full_Output()) return 0x5BF9C;
                if (is_1080i_Info_Output()) return 0x5EDB0;
            }

            case 3: 
            {
                if (is_LCD_Output())  return 0xD5C0;
                if (is_480p_Output()) return 0xB9E0;
                if (is_1080i_Full_Output()) return 0x2772C;
                if (is_1080i_Info_Output()) return 0x33B58;
            }

            case 4:
            {
                if (is_LCD_Output()) return 0;
            }
        }        
    }

    if (CROP_PRESET_MENU == CROP_PRESET_1X3 || CROP_PRESET_MENU == CROP_PRESET_3X3)
    {
        switch (crop_preset_ar)
        {
            case 0:
            {
                if (is_LCD_Output())  return 0xD5C0;
                if (is_480p_Output()) return 0xB9E0;
                if (is_1080i_Full_Output()) return 0x2772C;
                if (is_1080i_Info_Output()) return 0x33B58;
            }            
            case 1:
            {
                if (is_LCD_Output())  return 0x15720;
                if (is_480p_Output()) return 0x124C0;
                if (is_1080i_Full_Output()) return 0x3FD2C;
                if (is_1080i_Info_Output()) return 0x46758;
            }            
            case 2:
            {
                if (is_LCD_Output())  return 0x1B6C0;
                if (is_480p_Output()) return 0x17380;
                if (is_1080i_Full_Output()) return 0x51A2C;
                if (is_1080i_Info_Output()) return 0x55758;
            }            
            case 3:
            case 4:
            {
                if (is_LCD_Output())  return 0x1F4A0;
                if (is_480p_Output()) return 0x1B160;
                if (is_1080i_Full_Output()) return 0x5BF2C;
                if (is_1080i_Info_Output()) return 0x5ED58;
            }
        }  
    }

    return 0;
}

void SetAspectRatioCorrectionValues()
{
    if (CROP_PRESET_MENU == CROP_PRESET_1X1)
    {
        if (is_LCD_Output())
        {
            switch (crop_preset_1x1_res)
            {
                case 0:  YUV_LV_Buf = 0x13505A0; YUV_LV_S_V = 0x1050244; break; // CROP_2_5K
                case 1:                                                         // CROP_2_8K
                case 2:  YUV_LV_Buf = 0x13305A0; YUV_LV_S_V = 0x1050248; break; // CROP_3K
                case 3:  YUV_LV_Buf = 0x19505A0; YUV_LV_S_V = 0x10501BA; break; // CROP_1440p
                case 6:  YUV_LV_Buf = 0x1B505A0; YUV_LV_S_V = 0x10501B2; break; // CROP_1620p (dannephoto)
                default: YUV_LV_Buf = 0x1DF05A0; YUV_LV_S_V = 0x1E002B;  break;
            }
        }
        if (is_480p_Output())
        {
            switch (crop_preset_1x1_res)
            {
                case 0:                                                         // CROP_2_5K
                case 1:                                                         // CROP_2_8K
                case 2:                                                         // CROP_3K
                         YUV_LV_Buf = 0x1170520; YUV_LV_S_V = 0x1050282; break;
                case 3:  YUV_LV_Buf = 0x1710520; YUV_LV_S_V = 0x10501E5; break; // CROP_1440p
                default: YUV_LV_Buf = 0x1830520; YUV_LV_S_V = 0x6100AC;  break;
            }
        }
        if (is_1080i_Full_Output())
        {
            switch (crop_preset_1x1_res)
            {
                case 0:                                                         // CROP_2_5K
                case 1:                                                         // CROP_2_8K
                case 2:                                                         // CROP_3K
                         YUV_LV_Buf = 0x1580CA8; YUV_LV_S_V = 0x1050209; break;
                case 3:  YUV_LV_Buf = 0x1C70CA8; YUV_LV_S_V = 0x105018A; break; // CROP_1440p
                default: YUV_LV_Buf = 0x21B0CA8; YUV_LV_S_V = 0x8700AC;  break;
            }
        }
        if (is_1080i_Info_Output())
        {
            switch (crop_preset_1x1_res)
            {
                case 0:                                                         // CROP_2_5K
                case 1:                                                         // CROP_2_8K
                case 2:                                                         // CROP_3K
                         YUV_LV_Buf = 0x1180A50; YUV_LV_S_V = 0x1050280; break;
                case 3:  YUV_LV_Buf = 0x1730A50; YUV_LV_S_V = 0x10501E3; break; // CROP_1440p
                default: YUV_LV_Buf = 0x1B70A50; YUV_LV_S_V = 0x370056;  break;
            }
        }
    }

    if (CROP_PRESET_MENU == CROP_PRESET_1X3 || CROP_PRESET_MENU == CROP_PRESET_3X3)
    {
        if (is_LCD_Output())
        {
            switch (crop_preset_ar)
            {
                case 0: YUV_LV_Buf = 0x19505A0; YUV_LV_S_V = 0x10501BA; break; // AR_16_9
                case 1: YUV_LV_Buf = 0x16805A0; YUV_LV_S_V = 0x10501F2; break; // AR_2_1
                case 2: YUV_LV_Buf = 0x14805A0; YUV_LV_S_V = 0x1050222; break; // AR_2_20_1
                case 3: // AR_2_35_1
                case 4: // AR_2_39_1
                {
                    YUV_LV_Buf = 0x13205A0; YUV_LV_S_V = 0x1050249;
                }
                break;
            }  
        }
        if (is_480p_Output())
        {
            switch (crop_preset_ar)
            {
                case 0: YUV_LV_Buf = 0x1710520; YUV_LV_S_V = 0x10501E5; break; // AR_16_9
                case 1: YUV_LV_Buf = 0x1480520; YUV_LV_S_V = 0x1050222; break; // AR_2_1
                case 2: YUV_LV_Buf = 0x12A0520; YUV_LV_S_V = 0x1050259; break; // AR_2_20_1
                case 3: // AR_2_35_1
                case 4: // AR_2_39_1
                {
                    YUV_LV_Buf = 0x1170520; YUV_LV_S_V = 0x1050282;
                }
                break;
            }  
        }
        if (is_1080i_Full_Output())
        {
            switch (crop_preset_ar)
            {
                case 0: YUV_LV_Buf = 0x1C70CA8; YUV_LV_S_V = 0x105018A; break; // AR_16_9
                case 1: YUV_LV_Buf = 0x1950CA8; YUV_LV_S_V = 0x10501BA; break; // AR_2_1
                case 2: YUV_LV_Buf = 0x1700CA8; YUV_LV_S_V = 0x10501E7; break; // AR_2_20_1
                case 3: // AR_2_35_1
                case 4: // AR_2_39_1
                {
                    YUV_LV_Buf = 0x1580CA8; YUV_LV_S_V = 0x1050209;
                }
                break;
            }  
        }
        if (is_1080i_Info_Output())
        {
            switch (crop_preset_ar)
            {
                case 0: YUV_LV_Buf = 0x1730A50; YUV_LV_S_V = 0x10501E3; break; // AR_16_9
                case 1: YUV_LV_Buf = 0x14A0A50; YUV_LV_S_V = 0x105021F; break; // AR_2_1
                case 2: YUV_LV_Buf = 0x12C0A50; YUV_LV_S_V = 0x1050255; break; // AR_2_20_1
                case 3: // AR_2_35_1
                case 4: // AR_2_39_1
                {
                    YUV_LV_Buf = 0x1180A50; YUV_LV_S_V = 0x1050280;
                }
                break;
            }  
        }
    }

    /* Set default x5 mode values for mv1080 preset, also for Anam_FLV */
    if ((CROP_PRESET_MENU == CROP_PRESET_3X3 && (crop_preset_3x3_res == 1 || crop_preset_3x3_res == 2)) || // mv1080
        (CROP_PRESET_MENU == CROP_PRESET_1X3 && crop_preset_1x3_res == 3))   // Anam_FLV
    {
        if (is_LCD_Output()){        YUV_LV_Buf = 0x1DF05A0; YUV_LV_S_V = 0x1E002B;}
        if (is_480p_Output()){       YUV_LV_Buf = 0x1830520; YUV_LV_S_V = 0x6100AC;}
        if (is_1080i_Full_Output()){ YUV_LV_Buf = 0x21B0CA8; YUV_LV_S_V = 0x8700AC;}
        if (is_1080i_Info_Output()){ YUV_LV_Buf = 0x1B70A50; YUV_LV_S_V = 0x370056;}
    }
}

static void FAST PATH_SelectPathDriveMode_hook(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    /* we need to enable and set preview shifting and clearing artifacts values here especially for clear artifacts value */
    /* I don't know which function load shifting preview value, but it's being loaded and applied many times in LiveView, not just once. */
    /* clear artifacts value is being loaded very early before CMOS, ADTG, ENGIO, ENG_DRV_OUT, ENG_DRV_OUTS stuff */
    /* in [VRAM] VRAM_PTH_StartTripleRamClearInALump[ff962a90] (ff962a90 + 0x8 holds value for clearing artifacts for x5 mode for LCD output on 700D) */
    /* apparently PATH_SelectPathDriveMode sets its arguments before loading/applying any video configuration, e.g: */

    /*  700D DebugLog, x5 mode:
    
        Evf:ff19ce1c:ad:03: PATH_Select S:8 Z:50000 R:0 DZ:0 SM:0 SV:0 DT:0       <-- we are patching it from here before loading it
        Evf:ff19cfb0:ad:03: PathDriveMode Change: 10->2
        Evf:ff37ad64:ad:03: GetPathDriveInfo[2]
        Evf:ff4f2ce8:ad:03: LVx5_SelectPath LCD
        Evf:ff4f3980:ad:01: LVx5_GetVramParam(W:720 H:480)
        Evf:ff37cebc:ad:03: RamClear_SetPath
        Evf:ff37d4c8:ad:03: LV_ResLockTripleRamClearPass
        Evf:ff4ee18c:a9:03: [VRAM] VRAM_PTH_StartTripleRamClearInALump[ff962a90]  <-- clear artifacts value is being loaded here, ff962a90 is array holding six 32 bits values
        Evf:ff37cf1c:ad:03: RamClear_StartPath
        Evf:ff37d084:ad:03: RamClear_LV_RAMCLEAR_COLOR_BLACK
        Evf:ff37cf1c:ad:03: RamClear_StartPath
        Evf:ff37d084:ad:03: RamClear_LV_RAMCLEAR_COLOR_BLACK
        Evf:ff37cf1c:ad:03: RamClear_StartPath
        Evf:ff37d084:ad:03: RamClear_LV_RAMCLEAR_COLOR_BLACK
        Evf:ff36ce48:a9:03: [VRAM]====>> PathRamClearCompleteCBR   */ 

    /* FIXME: we might be able to implement clearing artifacts directly in VRAM_PTH_StartTripleRamClearInALump
              this way we don't to patch ROM addresses for clearing artifacts for x5 mode and for every output on every model */

    /* always unpatch to check output and update addresses and values */
    if (Center_Preview_ON)
    {
        unpatch_memory(ShiftAddress);
        Center_Preview_ON = 0;
    }

    if (Clear_Artifacts_ON)
    {
        unpatch_memory(ClearAddress);
        Clear_Artifacts_ON = 0;
    }

    /* detect current output and update patch parameters */
    if (is_LCD_Output())
    {
        DefaultShift  = 0x0;
        DefaultClear  = 0x0;
        NewClearVal   = 0x5A0;
        ShiftAddress  = Shift_x5_LCD;
        ClearAddress  = Clear_Vram_x5_LCD;
    }

    if (is_480p_Output())
    {
        DefaultShift  = 0x8740;
        DefaultClear  = 0x40;
        NewClearVal   = 0x520;
        ShiftAddress  = Shift_x5_HDMI_480p;
        ClearAddress  = Clear_Vram_x5_HDMI_480p;
    }

    if (is_1080i_Full_Output())
    {
        DefaultShift  = 0x12C;
        DefaultClear  = 0x12C;
        NewClearVal   = 0xCA8;
        ShiftAddress  = Shift_x5_HDMI_1080i_Full;
        ClearAddress  = Clear_Vram_x5_HDMI_1080i_Full;
    }

    if (is_1080i_Info_Output())
    {
        DefaultShift  = 0x16A58;
        DefaultClear  = 0x258;
        NewClearVal   = 0xA50;
        ShiftAddress  = Shift_x5_HDMI_1080i_Info;
        ClearAddress  = Clear_Vram_x5_HDMI_1080i_Info;
    }

    if (CROP_PRESET_MENU == CROP_PRESET_1X1)
    {
        if (crop_preset_1x1_res == 0)  // CROP_2_5K
        {
            Shift_Preview = 1;
            Clear_Artifacts = 1;
            EDMAC_9_Vertical_Change = 0;
        }

        if (crop_preset_1x1_res == 1)  // CROP_2_8K
        {
            Shift_Preview = 1;
            Clear_Artifacts = 1;
            EDMAC_9_Vertical_Change = 1;
        }

        if (crop_preset_1x1_res == 2)  // CROP_3K
        {
            Shift_Preview = 1;
            Clear_Artifacts = 1;
            EDMAC_9_Vertical_Change = 1;
        }

        if (crop_preset_1x1_res == 3)  // CROP_1440p
        {
            Shift_Preview = 1;
            Clear_Artifacts = 1;
            EDMAC_9_Vertical_Change = 1;
        }

        if (crop_preset_1x1_res == 4 || crop_preset_1x1_res == 7)    // CROP_1280p
        {
            Shift_Preview = 0;
            Clear_Artifacts = 0;
            EDMAC_9_Vertical_Change = 0;
        }

        if (crop_preset_1x1_res == 5)    // CROP_Full_Res
        {
            Shift_Preview = 0;
            Clear_Artifacts = 0;
            EDMAC_9_Vertical_Change = 1;
        }
    }
    
    if (crop_preset_1x1_res == 6)    // CROP_1620p
    {
        Shift_Preview = 0;
        Clear_Artifacts = 1;
        EDMAC_9_Vertical_Change = 0;
    }

    if (CROP_PRESET_MENU == CROP_PRESET_1X3)
    {
		if (crop_preset_1x3_res == 3) // Anam_FLV
		{
			Shift_Preview = 0;
			Clear_Artifacts = 0;
		}
		else
		{
		    Shift_Preview = 1;
			Clear_Artifacts = 1;
		}

        EDMAC_9_Vertical_Change = 1;
    }

    if (CROP_PRESET_MENU == CROP_PRESET_3X3)
    {
        Shift_Preview = 1;
        Clear_Artifacts = 1;
        EDMAC_9_Vertical_Change = 0;
        
        if (crop_preset_3x3_res == 1 || crop_preset_3x3_res == 2) // mv1080 doesn't need them
        {
            Shift_Preview = 0;
            Clear_Artifacts = 0;
        }
    }

    NewShiftVal = GetShiftValue();
    SetAspectRatioCorrectionValues();

    /* restore defualt EDMAC#9 vertical size */
    if (EDMAC_9_Vertical_Change == 0 || PathDriveMode->zoom != 5)
    {
        if (MEM(EDMAC_9_Vertical_1) != 0x453 || MEM(EDMAC_9_Vertical_2) != 0x453)
        {
            MEM(EDMAC_9_Vertical_1)  = 0x453;
            MEM(EDMAC_9_Vertical_2)  = 0x453;
        }
        
        EDMAC_9_Vertical_Change = 0;
    }

    if (PathDriveMode->zoom == 5)
    {
        /* patch supported presets if patch not active */
        if (Shift_Preview && !Center_Preview_ON)
        {
            patch_memory(ShiftAddress, DefaultShift, NewShiftVal, "Center");
            Center_Preview_ON = 1;
        }

        if (Clear_Artifacts && !Clear_Artifacts_ON)
        {
            patch_memory(ClearAddress, DefaultClear, NewClearVal, "Clear");
            Clear_Artifacts_ON = 1;
        }

        /* unpatch not supported presets if patch already active */
        if (!Shift_Preview && Center_Preview_ON)
        {
            unpatch_memory(ShiftAddress);
            Center_Preview_ON = 0;
        }

        if (!Clear_Artifacts && Clear_Artifacts_ON)
        {
            unpatch_memory(ClearAddress);
            Clear_Artifacts_ON = 0;
        }
    }

    /* unpatch in all other modes if patch already active  */
    if (PathDriveMode->zoom != 5)
    {
        if (Center_Preview_ON)
        {
            unpatch_memory(ShiftAddress);
            Center_Preview_ON = 0;
        }

        if (Clear_Artifacts_ON)
        {
            unpatch_memory(ClearAddress);
            Clear_Artifacts_ON = 0;
        }
    }
}

static int patch_active = 0;

#ifdef CONFIG_EOSM
/* EOS M Live View is rebuilt asynchronously after boot, Canon-menu return,
 * record-stop and zoom changes.  Keep the crop hooks quiet until the base
 * x5 pipeline has delivered stable RAW dimensions, then apply once. */
#define EOSM_LV_GUARD_SETTLE_MS 350
#define EOSM_LV_GUARD_QUIET_MS 120
#define EOSM_LV_GUARD_STABLE_FRAMES 3
#define EOSM_LV_GUARD_MAX_RECOVERIES 2
#define EOSM_LV_GUARD_CONTENT_SAMPLES 3
static volatile int eosm_lv_guard_pending = 1;
static volatile int eosm_lv_guard_busy = 0;

static int eosm_lv_guard_started;
static int eosm_lv_guard_state;
static int eosm_lv_guard_stable_frames;
static int eosm_lv_guard_width;
static int eosm_lv_guard_height;
static int eosm_lv_guard_pitch;
static uintptr_t eosm_lv_guard_raw_buffer;
static uint32_t eosm_lv_guard_display_buffer;
static int eosm_lv_guard_quiet_since;
static int eosm_lv_guard_retries;
static int eosm_lv_guard_internal_zoom;
static int eosm_lv_guard_content_dark_frames;
static int eosm_lv_guard_content_retries;
static int eosm_lv_guard_route_retries;

enum eosm_lv_guard_state
{
    EOSM_LV_GUARD_WAIT = 0,
    EOSM_LV_GUARD_APPLY_X1,
    EOSM_LV_GUARD_APPLY_X5,
    EOSM_LV_GUARD_VALIDATE,
    EOSM_LV_GUARD_CONTENT,
    EOSM_LV_GUARD_ROUTE,
    EOSM_LV_GUARD_RECOVER_X1,
    EOSM_LV_GUARD_RECOVER_X5,
};

/* Exported to the core touch router: do not accept a new control gesture
 * while Canon is rebuilding the sensor/display path. */
__attribute__((used, noinline))
int crop_rec_lv_transition_busy(void)
{
    return eosm_lv_guard_busy;
}

static void eosm_lv_guard_request(void)
{
    eosm_lv_guard_pending = 1;
    eosm_lv_guard_busy = 1;
    eosm_lv_guard_started = 0;
    eosm_lv_guard_state = EOSM_LV_GUARD_WAIT;
    eosm_lv_guard_stable_frames = 0;
    eosm_lv_guard_width = 0;
    eosm_lv_guard_height = 0;
    eosm_lv_guard_pitch = 0;
    eosm_lv_guard_raw_buffer = 0;
    eosm_lv_guard_display_buffer = 0;
    eosm_lv_guard_quiet_since = 0;
    eosm_lv_guard_retries = 0;
    eosm_lv_guard_content_dark_frames = 0;
    eosm_lv_guard_content_retries = 0;
    eosm_lv_guard_route_retries = 0;
}

static void eosm_lv_guard_set_zoom(int zoom)
{
    eosm_lv_guard_internal_zoom = 1;
    set_zoom(zoom);
}
#else
int crop_rec_lv_transition_busy(void) { return 0; }
int crop_rec_lv_transition_diag(char *buffer, int size)
{
    if (buffer && size > 0) buffer[0] = '\0';
    return 0;
}
static void eosm_lv_guard_request(void) {}
#endif

static void install_patches()
{
    patch_hook_function(CMOS_WRITE, MEM_CMOS_WRITE, &cmos_hook, "crop_rec: CMOS[1,2,6] parameters hook");
    patch_hook_function(ADTG_WRITE, MEM_ADTG_WRITE, &adtg_hook, "crop_rec: ADTG[8000,8806] parameters hook");
    if (ENGIO_WRITE) patch_hook_function(ENGIO_WRITE, MEM_ENGIO_WRITE, engio_write_hook, "crop_rec: video timers hook");
    if (ENG_DRV_OUT) patch_hook_function(ENG_DRV_OUT, MEM(ENG_DRV_OUT), EngDrvOut_hook, "crop_rec: preview stuff 1");
    if (ENG_DRV_OUTS) patch_hook_function(ENG_DRV_OUTS, MEM(ENG_DRV_OUTS), EngDrvOuts_hook, "crop_rec: preview stuff 2");
    if (PATH_SelectPathDriveMode) patch_hook_function(PATH_SelectPathDriveMode, MEM(PATH_SelectPathDriveMode), PATH_SelectPathDriveMode_hook, "crop_rec: preview stuff 3");
    if (HIV_Vertical_Address_hook) patch_hook_function(HIV_Vertical_Address_hook, MEM(HIV_Vertical_Address_hook), Change_HIV_V_Address, "crop_rec: preview stuff 4");
}

static void uninstall_patches()
{
    unpatch_memory(CMOS_WRITE);
    unpatch_memory(ADTG_WRITE);
    if (ENGIO_WRITE) unpatch_memory(ENGIO_WRITE);
    if (ENG_DRV_OUT) unpatch_memory(ENG_DRV_OUT);
    if (ENG_DRV_OUTS) unpatch_memory(ENG_DRV_OUTS);
    if (PATH_SelectPathDriveMode) unpatch_memory(PATH_SelectPathDriveMode);
    if (HIV_Vertical_Address_hook) unpatch_memory(HIV_Vertical_Address_hook);
    if (Clear_Artifacts_ON)
        {
            unpatch_memory(ClearAddress);
            Clear_Artifacts_ON = 0;
        }
    if (Center_Preview_ON)
        {
            unpatch_memory(ShiftAddress);
            Center_Preview_ON = 0 ;
        }
}

static void update_patch()
{
    if (CROP_PRESET_MENU)
    {
        /* update preset */
        crop_preset = CROP_PRESET_MENU;
        
        crop_preset_ar      = crop_preset_ar_menu;
        crop_preset_fps     = crop_preset_fps_menu;
        /* 18 fps exists only in the 1:1 1280p preset; anywhere else fall back to 24 */
        if (crop_preset_fps == 3 && !(CROP_PRESET_MENU == CROP_PRESET_1X1 && crop_preset_1x1_res_menu == 4))
            crop_preset_fps = 0;
        crop_preset_1x1_res = crop_preset_1x1_res_menu;
        crop_preset_1x3_res = crop_preset_1x3_res_menu;
        crop_preset_3x3_res = crop_preset_3x3_res_menu;

        /* install our hooks, if we haven't already do so */
        if (!patch_active)
        {
            install_patches();
            patch_active = 1;
        }
    }
    
    /* assuming we will take a normal picture in LiveView while crop_rec is active
     * clear artifacts patch will be overwritten by Canon back to default value in this case
     * which will give us patch error (in memory patches), this might help with busy screen too
     * when taking a picture and crop mood is active, let's unpatch all patches outside lv 
     * PROP_LV_ACTION then PROP_LV_STOP get triggerd after pressing full shutter button (SW2)
     * and before taking a picture process happens (also before clear artifacts get overwritten) */
    if (CROP_PRESET_MENU && !lv)
    {
        if (patch_active)
        {
            uninstall_patches();

            /* turn off Kill Canon GUI setting */
            extern int kill_canon_gui_mode;
            if (kill_canon_gui_mode != 0)
            {
                kill_canon_gui_mode = 0;
            }

            patch_active = 0;
            crop_preset = 0;
        }
    }
    /* unpatch when no crop presets is selected, also outside movie mode */
    if (!CROP_PRESET_MENU || !is_movie_mode())
    {
        /* undo active patches, if any */
        if (patch_active)
        {
            uninstall_patches();

            /* enable Canon overlays (turn off Kill Canon GUI setting) */
            extern int kill_canon_gui_mode;
            if (kill_canon_gui_mode != 0)
            {
                kill_canon_gui_mode = 0;
                if (canon_gui_front_buffer_disabled())
                {
                    canon_gui_enable_front_buffer(0);
                }
            }

            patch_active = 0;
            crop_preset = 0;
        }
    }
}

/* enable patch when switching LiveView (not in the middle of LiveView) */
/* otherwise you will end up with a halfway configured video mode that looks weird */
PROP_HANDLER(PROP_LV_ACTION)
{
    update_patch();
    eosm_lv_guard_request();
}

/* also try when switching zoom modes */
PROP_HANDLER(PROP_LV_DISPSIZE)
{
    update_patch();
#ifdef CONFIG_EOSM
    if (eosm_lv_guard_internal_zoom)
        eosm_lv_guard_internal_zoom = 0;
    else
#endif
        eosm_lv_guard_request();
}

/* forward reference */
static struct menu_entry crop_rec_menu[];

/* give a warning if picture quality not set to RAW for entry-level models */
static int pic_quality_warning = 0;

static MENU_UPDATE_FUNC(crop_update)
{
    if (is_DIGIC_5)
    {
        /* reveal options for the current crop mode (1:1, 1x3 and 3x3) */
        crop_rec_menu[0].children[0].shidden = (crop_preset_index != 1);  // 1 CROP_PRESET_1X1
        crop_rec_menu[0].children[1].shidden = (crop_preset_index != 2);  // 2 CROP_PRESET_1X3
        crop_rec_menu[0].children[2].shidden = (crop_preset_index != 3);  // 3 CROP_PRESET_3X3
        crop_rec_menu[0].children[5].shidden = (CROP_PRESET_MENU == CROP_PRESET_1X3 || CROP_PRESET_MENU == CROP_PRESET_1X1);  // 3 CROP_PRESET_3X3
        

        if (CROP_PRESET_MENU && lv && patch_active)
        {
            if (raw_lv_is_enabled())
            {
                /* print resolution and binning mode in help section, maybe add FPS too? */
                MENU_SET_HELP("%dx%d %d%s%d",raw_info.width - 72, raw_info.height - 28, raw_capture_info.binning_y + raw_capture_info.skipping_y,
                                                                                       (raw_capture_info.binning_x + raw_capture_info.skipping_x == 1 &&
                                                                                        raw_capture_info.binning_y + raw_capture_info.skipping_y == 1) ? 
                            ":" : "x",                                                  raw_capture_info.binning_x + raw_capture_info.skipping_x);

                /* print picture warning if Image quality not set to RAW from Canon menu */
                if (pic_quality_warning)
                {
                    MENU_SET_WARNING(MENU_WARN_ADVICE, "Set Image quality to RAW, restart camera. This extends recording times.");
                }
            }

            /* print selected preset name in crop mode menu */
            if (CROP_PRESET_MENU == CROP_PRESET_1X1)
            {
                MENU_SET_VALUE("%s %s", crop_preset_1x1_res_menu == 0 ? "2.5K"  : crop_preset_1x1_res_menu == 1 ? "2.8K"     :
                                        crop_preset_1x1_res_menu == 2 ? "3K"    : crop_preset_1x1_res_menu == 3 ? "1440p"    :
                                        crop_preset_1x1_res_menu == 4 ? "1280p" : crop_preset_1x1_res_menu == 5 ? "Full-Res" : "", "1:1 crop");
            }
            if (CROP_PRESET_MENU == CROP_PRESET_1X3)
            {
                MENU_SET_VALUE("%s %s", (crop_preset_1x3_res_menu == 0 && crop_preset_ar_menu == 0) ? "4.5K"  : 
                                        (crop_preset_1x3_res_menu == 1 && crop_preset_ar_menu == 0) ? "4.2K"  :
                                        (crop_preset_1x3_res_menu == 2 && crop_preset_ar_menu == 0) ? "UHD"   :
                                        (crop_preset_1x3_res_menu == 0 && crop_preset_ar_menu == 1) ? "4.8K"  :
                                        (crop_preset_1x3_res_menu == 1 && crop_preset_ar_menu == 1) ? "4.4K"  :
                                        (crop_preset_1x3_res_menu == 2 && crop_preset_ar_menu == 1) ? "4K"    :
                                        (crop_preset_1x3_res_menu == 0 && crop_preset_ar_menu == 2) ? "5K"    :
                                        (crop_preset_1x3_res_menu == 1 && crop_preset_ar_menu == 2) ? "4.6K"  :
                                        (crop_preset_1x3_res_menu == 2 && crop_preset_ar_menu == 2) ? "4.2K"  :
                                        (crop_preset_1x3_res_menu == 0 && crop_preset_ar_menu >= 3) ? "5.2K"  :
                                        (crop_preset_1x3_res_menu == 1 && crop_preset_ar_menu >= 3) ? "4.8K"  :
                                        (crop_preset_1x3_res_menu == 2 && crop_preset_ar_menu >= 3) ? "4.4K"  : "",  "1x3");
            }
            if (CROP_PRESET_MENU == CROP_PRESET_3X3)
            {
                MENU_SET_VALUE("%s %s", (crop_preset_3x3_res_menu == 0 && crop_preset_ar_menu == 0) ? "976p"  : 
                                        (crop_preset_3x3_res_menu == 0 && crop_preset_ar_menu == 1) ? "868p"  :
                                        (crop_preset_3x3_res_menu == 0 && crop_preset_ar_menu == 2) ? "790p"  :
                                        (crop_preset_3x3_res_menu == 0 && crop_preset_ar_menu == 3) ? "738p"  :
                                        (crop_preset_3x3_res_menu == 0 && crop_preset_ar_menu == 4 && crop_preset_fps_reduce == 0 && is_EOSM) ? "694p"  :
                                        (crop_preset_3x3_res_menu == 0 && crop_preset_ar_menu == 4 && crop_preset_fps_reduce == 1 && is_EOSM) ? "726p"  :
                                        (crop_preset_3x3_res_menu == 1)                             ? "1080p"  : "",  "3x3");
                
                if (crop_preset_3x3_res_menu == 0) MENU_SET_RINFO("(HFR)");
            }
        }
    }

    /* hide Framerate and Aspect ratio menus for none supported models */
    crop_rec_menu[0].children[3].shidden = !is_DIGIC_5;  // Aspect ratio
    crop_rec_menu[0].children[4].shidden = !is_DIGIC_5;  // Framerate

    if (CROP_PRESET_MENU && lv)
    {
        if (lv_dispsize == 1)
        {
            MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "To use this mode, exit ML menu & press the zoom button (set to x5).");
        }
    }
}

static MENU_UPDATE_FUNC(crop_preset_1x1_res_update)
{
    if (crop_preset_1x1_res_menu == 0)
    {
        MENU_SET_HELP("2520x1080 @ 23.976, 25 and 30 FPS");
    }
    if (crop_preset_1x1_res_menu == 1)
    {
        MENU_SET_HELP("2880x1206(2.39:1) @ 23.976 and 25 FPS");
    }
    if (crop_preset_1x1_res_menu == 2)
    {
        MENU_SET_HELP("3072x1308 @ 23.976 FPS. Real-Time preview isn't perfect.");
    }
    if (crop_preset_1x1_res_menu == 3)
    {
        MENU_SET_HELP("2560x1440(16:9) @ 23.976 and 25 FPS");
    }
    if (crop_preset_1x1_res_menu == 4)
    {
        MENU_SET_HELP("1920x1280 @ 23.976 and 25 FPS");
    }
    if (crop_preset_1x1_res_menu == 5)
    {
        MENU_SET_HELP("5208x3478 @ 2 FPS. Has cropped centered real-time preview.");
    }
    if (crop_preset_1x1_res_menu == 6)
    {
        MENU_SET_HELP("2160x1620 @ 23.976 FPS");
    }
}

static MENU_UPDATE_FUNC(crop_preset_1x3_res_update)
{
    if (crop_preset_ar_menu == 0) // AR_16_9
    {
        if (crop_preset_1x3_res_menu == 0) // Anam_Highest
        {
            MENU_SET_VALUE("4.5K");
            if (is_EOSM)MENU_SET_HELP("1504x2538 @ 22.250 FPS");
        }

        if (crop_preset_1x3_res_menu == 1) // Anam_Higher
        {
            MENU_SET_VALUE("4.2K");
            if (is_EOSM)MENU_SET_HELP("1376x2322 @ 23.976 FPS");
        }

        if (crop_preset_1x3_res_menu == 2) // Anam_Medium
        {
            MENU_SET_VALUE("UHD");
            MENU_SET_HELP("1280x2160 @ 23.976 and 25 FPS");
        } 
        if (crop_preset_1x3_res_menu == 3) // Anam_Medium
        {
            MENU_SET_VALUE("Full-Res LV");
            MENU_SET_HELP("1736x3476 @ 12, 14 and 16 FPS");
        }
    }

    if (crop_preset_ar_menu == 1) // AR_2_1
    {
        if (crop_preset_1x3_res_menu == 0) // Anam_Highest
        {
            MENU_SET_VALUE("4.8K");
            if (is_EOSM)MENU_SET_HELP("1600x2400 @ 23.300 FPS");
        }

        if (crop_preset_1x3_res_menu == 1) // Anam_Higher
        {
            MENU_SET_VALUE("4.4K");
            MENU_SET_HELP("1472x2208 @ 23.976 and 25 FPS");
        }

        if (crop_preset_1x3_res_menu == 2) // Anam_Medium
        {
            MENU_SET_VALUE("4K");
            MENU_SET_HELP("1360x2040 @ 23.976 and 25 FPS");
        }
    }

    if (crop_preset_ar_menu == 2) // AR_2_20_1
    {
        if (crop_preset_1x3_res_menu == 0) // Anam_Highest
        {
            MENU_SET_VALUE("5K");
            if (is_EOSM)MENU_SET_HELP("1664x2268 @ 23.976 FPS");
        }

        if (crop_preset_1x3_res_menu == 1) // Anam_Higher
        {
            MENU_SET_VALUE("4.6K");
            MENU_SET_HELP("1552x2216 @ 23.976 and 25 FPS");
        }

        if (crop_preset_1x3_res_menu == 2) // Anam_Medium
        {
            MENU_SET_VALUE("4.2K");
            MENU_SET_HELP("1424x1942 @ 23.976 and 25 FPS");
        }
    }

    if (crop_preset_ar_menu == 3)  // AR_2_35_1
    {
        if (crop_preset_1x3_res_menu == 0) // Anam_Highest
        {
            MENU_SET_VALUE("5.2K");
            MENU_SET_HELP("1736x2214 @ 23.976 FPS");
        }

        if (crop_preset_1x3_res_menu == 1) // Anam_Higher
        {
            MENU_SET_VALUE("4.8K");
            MENU_SET_HELP("1600x2040 @ 23.976 and 25 FPS");
        }

        if (crop_preset_1x3_res_menu == 2) // Anam_Medium
        {
            MENU_SET_VALUE("4.4K");
            MENU_SET_HELP("1472x1878 @ 23.976 and 25 FPS");
        }
    }

    if (crop_preset_ar_menu == 4)  // AR_2_39_1
    {
        if (crop_preset_1x3_res_menu == 0) // Anam_Highest
        {
            MENU_SET_VALUE("5.2K");
            MENU_SET_HELP("1736x2178 @ 23.976 FPS");
        }

        if (crop_preset_1x3_res_menu == 1) // Anam_Higher
        {
            MENU_SET_VALUE("4.8K");
            MENU_SET_HELP("1600x2008 @ 23.976 and 25 FPS");
        }

        if (crop_preset_1x3_res_menu == 2) // Anam_Medium
        {
            MENU_SET_VALUE("4.4K");
            MENU_SET_HELP("1472x1846 @ 23.976 and 25 FPS");
        }
    }
}

static MENU_UPDATE_FUNC(crop_preset_3x3_res_update)
{
    if (crop_preset_3x3_res_menu == 0) // High FPS
    {
        if (crop_preset_ar_menu == 0) MENU_SET_VALUE("976p (HFR)"); // AR_16_9
        if (crop_preset_ar_menu == 1) MENU_SET_VALUE("868p (HFR)"); // AR_2_1
        if (crop_preset_ar_menu == 2) MENU_SET_VALUE("790p (HFR)"); // AR_2_20_1
        if (crop_preset_ar_menu == 3) MENU_SET_VALUE("738p (HFR)"); // AR_2_35_1
        if (crop_preset_ar_menu == 4 && crop_preset_fps_reduce == 0 && is_EOSM) // AR_2_39_1  // actually 2.50:1 aspect ratio */
        {
            MENU_SET_VALUE("694p (HFR)"); 
            MENU_SET_WARNING(MENU_WARN_ADVICE, "Real-Time preview doesn't work in 694p (HFR), it's always black.");
        }        
        if (crop_preset_ar_menu == 4 && crop_preset_fps_reduce == 1 && is_EOSM) // AR_2_39_1  // actually 2.50:1 aspect ratio */
        {
            MENU_SET_VALUE("726p (HFR)");
        }
    }
}

static MENU_UPDATE_FUNC(crop_preset_ar_update)
{
    if (CROP_PRESET_MENU == CROP_PRESET_3X3)
    {
        if (crop_preset_3x3_res_menu == 0)  // High_FPS
        {
            if (crop_preset_ar_menu == 4 && crop_preset_fps_reduce == 1 && is_EOSM) MENU_SET_VALUE("2.39:1");
            if (crop_preset_ar_menu == 4 && crop_preset_fps_reduce == 0) MENU_SET_VALUE("2.50:1"); // AR_2_39_1 // we are using AR_2_39_1 as 2.50:1 in this case
        }
        if (crop_preset_3x3_res_menu == 2)  // mv1080
        {
            MENU_SET_VALUE("3:2");
            MENU_SET_WARNING(MENU_WARN_ADVICE, "This option doesn't work in current preset.");
        }
    }

    if (CROP_PRESET_MENU == CROP_PRESET_1X1)
    {
        if (crop_preset_1x1_res_menu == 0) MENU_SET_VALUE("2.33:1");  // CROP_2_5K
        if (crop_preset_1x1_res_menu == 1) MENU_SET_VALUE("2.39:1");  // CROP_2_8K
        if (crop_preset_1x1_res_menu == 2) MENU_SET_VALUE("2.35:1");  // CROP_3K 3072x1308
        if (crop_preset_1x1_res_menu == 3) MENU_SET_VALUE("16:9");    // CROP_1440p
        if (crop_preset_1x1_res_menu == 4) MENU_SET_VALUE("3:2");     // CROP_1280p
        if (crop_preset_1x1_res_menu == 5) MENU_SET_VALUE("3:2");     // CROP_Full_Res
        if (crop_preset_1x1_res_menu == 6) MENU_SET_VALUE("4:3");     // CROP_1620p
        if (crop_preset_1x1_res_menu == 7) MENU_SET_VALUE("16:9");    // CROP_1080p
        MENU_SET_WARNING(MENU_WARN_ADVICE, "This option doesn't work in 1:1 crop.");
    }
}

static MENU_UPDATE_FUNC(crop_preset_fps_update)
{
    if (CROP_PRESET_MENU == CROP_PRESET_1X3)
    {
        if (crop_preset_ar_menu == 0) // AR_16_9
        {
            if (crop_preset_1x3_res_menu == 0) // Anam_Highest
            {
                if (is_EOSM) MENU_SET_VALUE("22.250 FPS");
                if (crop_preset_fps_menu != 0)
                {
                    MENU_SET_WARNING(MENU_WARN_ADVICE, "25 and 30 FPS don't work in current preset.");
                }
            }

            if (crop_preset_1x3_res_menu == 1) // Anam_Higher
            {

                if (is_EOSM)
                {
                    if (crop_preset_fps_menu > 0)
                    {
                        MENU_SET_VALUE("23.976 FPS");
                        MENU_SET_WARNING(MENU_WARN_ADVICE, "25 and 30 FPS don't work in current preset.");
                    }
                }
            }

            if (crop_preset_1x3_res_menu == 2) // Anam_Medium
            {
                if (crop_preset_fps_menu == 2)
                {
                    MENU_SET_VALUE("25 FPS");
                    MENU_SET_WARNING(MENU_WARN_ADVICE, "30 FPS doesn't work in current preset.");
                }
            }
        }

        if (crop_preset_ar_menu == 1) // AR_2_1
        {
            if (crop_preset_1x3_res_menu == 0) // Anam_Highest
            {
                if (is_EOSM) MENU_SET_VALUE("23.300 FPS");
                if (crop_preset_fps_menu != 0)
                {
                    MENU_SET_WARNING(MENU_WARN_ADVICE, "25 and 30 FPS don't work in current preset.");
                }
            }

            if (crop_preset_1x3_res_menu > 0) // Anam_Higher, Anam_Medium
            {
                if (crop_preset_fps_menu == 2)
                {
                    MENU_SET_VALUE("25 FPS");
                    MENU_SET_WARNING(MENU_WARN_ADVICE, "30 FPS doesn't work in current preset.");
                }
            }
        }

        if (crop_preset_ar_menu > 1) // AR_2_20_1, AR_2_35_1, AR_2_39_1
        {
            if (crop_preset_1x3_res_menu == 0) // Anam_Highest
            {
                MENU_SET_VALUE("23.976 FPS");
                if (crop_preset_fps_menu != 0)
                {
                    MENU_SET_WARNING(MENU_WARN_ADVICE, "25 and 30 FPS don't work in current preset.");
                }
            }

            if (crop_preset_1x3_res_menu > 0) // Anam_Higher, Anam_Medium
            {
                if (crop_preset_fps_menu == 2)
                {
                    MENU_SET_VALUE("25 FPS");
                    MENU_SET_WARNING(MENU_WARN_ADVICE, "30 FPS doesn't work in current preset.");
                }
            }
        }
                    
        if (crop_preset_1x3_res_menu == 3) // High FPS
        {
            int current_fps = fps_get_current_x1000();

            if (is_EOSM)
            {
                MENU_SET_VALUE("%d.%03d",current_fps/1000, current_fps%1000);
            }
        }
    }

    if (CROP_PRESET_MENU == CROP_PRESET_1X1)
    {
        if ((crop_preset_1x1_res_menu == 1 || crop_preset_1x1_res_menu == 3) && crop_preset_fps_menu == 2) // CROP_2_8K, CROP_1440p and 30 FPS
        {
            MENU_SET_VALUE("25 FPS");
            MENU_SET_WARNING(MENU_WARN_ADVICE, "30 FPS doesn't work in current preset.");
        }

        if (crop_preset_1x1_res_menu == 2 && crop_preset_fps_menu > 0) // CROP_3K and 25, 30 FPS
        {
            MENU_SET_VALUE("23.976 FPS");
            MENU_SET_WARNING(MENU_WARN_ADVICE, "25 and 30 FPS don't work in current preset.");
        }

        if (crop_preset_1x1_res_menu == 5) // CROP_Full_Res
        {
            MENU_SET_VALUE("2 FPS");
            MENU_SET_WARNING(MENU_WARN_ADVICE, "This option doesn't work with Full-Res LV.");
        }
    }

    if (CROP_PRESET_MENU == CROP_PRESET_3X3)
    {
        
        int current_fps = fps_get_current_x1000();
                    
        if (crop_preset_3x3_res_menu == 0) // High FPS
        {
            if (is_EOSM)
            {
                if (crop_preset_ar_menu == 0) MENU_SET_VALUE("%d.%03d",current_fps/1000, current_fps%1000); // AR_16_9
                if (crop_preset_ar_menu == 1) MENU_SET_VALUE("%d.%03d",current_fps/1000, current_fps%1000);     // AR_2_1
                if (crop_preset_ar_menu == 2) MENU_SET_VALUE("%d.%03d",current_fps/1000, current_fps%1000);     // AR_2_20_1
                if (crop_preset_ar_menu == 3) MENU_SET_VALUE("%d.%03d",current_fps/1000, current_fps%1000);     // AR_2_35_1
                if (crop_preset_ar_menu == 4) MENU_SET_VALUE("%d.%03d",current_fps/1000, current_fps%1000);     // AR_2_39_1  // actually 2.50:1 aspect ratio
            }


            MENU_SET_WARNING(MENU_WARN_ADVICE, "This option doesn't work in (HFR) preset.");
        }
    }
}

static MENU_UPDATE_FUNC(target_yres_update)
{
    MENU_SET_RINFO("from %d", max_resolutions[crop_preset][get_video_mode_index()]);
}

static MENU_UPDATE_FUNC(bit_depth_analog_update)
{
    if (which_output_format() < 3)
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "To use this option, change Data format to lossless in RAW video.");
    }
}

static MENU_UPDATE_FUNC(fix_dual_iso_flicker_update)
{
    if (!dual_iso_is_enabled())
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "This option only works with Dual-ISO.");
    }
}

/* ---- EOS M slim Crop Mode (single screen) ----
 * Mode / Aspect / Preset drive recording config; Resolution is read-only;
 * Frame Rate cycles only valid rates; Bit Depth stays 14/12/11/10 always.
 * Module builds lack CONFIG_SLIM_MENUS — gate with is_EOSM. */

static struct menu_entry slim_more_hacks_menu[] = {
    {
        .name     = "More Hacks",
        .max      = 1,
        .choices  = CHOICES("OFF", "Allow"),
        .priv     = &more_hacks,
        .edit_mode = EM_INLINE_ADJUST,
        .icon_type = IT_DICE,
        .help     = "Allow More hacks even when other settings would block them.",
    },
};

/* Expo → Shutter range (EOS M slim; dial L/R like Mode / Aspect). */
static struct menu_entry expo_shutter_range_eosm[] = {
    {
        .name       = "Shutter range",
        .priv       = &shutter_range,
        .min        = 0,
        .max        = 1,
        .choices    = CHOICES("Original", "Full range"),
        .edit_mode  = EM_INLINE_ADJUST,
        .help       = "Choose the available shutter speed range:",
        .help2      = "Original: default range used by Canon in selected video mode.\n"
                      "Full range: from 1/FPS to minimum exposure time allowed by hardware."
    },
};

/* Framing (value 5) is hidden: stepping skips it, and a stray 5 becomes OFF */
static MENU_SELECT_FUNC(slim_info_button_select)
{
    int dir = delta < 0 ? -1 : 1;
    int v = INFO_button + dir;
    if (v == 5)
        v += dir;
    INFO_button = MOD(v, 7);
}

static MENU_UPDATE_FUNC(slim_info_button_update)
{
    static int last_info_button = -1;
    if (INFO_button == 5)
        INFO_button = 0;
    if (last_info_button == 5 && INFO_button != 5)
        mlv_lite_info_framing_reset();
    last_info_button = INFO_button;
}

/* Settings → INFO / Up-Down / Shutter zoom (EOS M slim). */
static struct menu_entry slim_info_button_menu[] = {
    {
        .name      = "INFO Button",
        .priv      = &INFO_button,
        .max       = 6,
        .choices   = CHOICES("OFF", "Histogram", "Waveform", "Zebras", "False Color", "Framing", "Quick Panel"),
        .edit_mode = EM_INLINE_ADJUST,
        .select    = slim_info_button_select,
        .update    = slim_info_button_update,
        .icon_type = IT_DICE,
        .help      = "Assign INFO to an overlay or the Quick Panel.",
        .help2     = "OFF uses Canon INFO. Idle LV: long-press INFO (or double-press) opens last setting.",
    },
    {
        .name      = "SET Button",
        .priv      = &SET_button,
        .min       = 1,
        .max       = 2,
        .choices   = CHOICES("x10 zoom", "Last settings"),
        .edit_mode = EM_INLINE_ADJUST,
        .icon_type = IT_DICE,
        .help      = "Choose what SET does on the movie LiveView screen.",
        .help2     = "Last settings opens the last changed ML setting, like a LiveView screen tap.",
    },
    {
        .name      = "Up/Down Button",
        .priv      = &Arrows_U_D,
        .max       = 3,
        .choices   = CHOICES("OFF", "Shutter", "Aperture", "ISO"),
        .edit_mode = EM_INLINE_ADJUST,
        .icon_type = IT_DICE,
        .help      = "What UP/DOWN adjust on the movie LiveView screen (not while recording).",
        .help2     = "Shutter: faster/slower. Aperture: open/close. ISO: up/down.",
    },
    {
        .name      = "Shutter zoom",
        .priv      = &Shutter_zoom,
        .max       = 2,
        .choices   = CHOICES("OFF", "ON", "Sticky"),
        .edit_mode = EM_INLINE_ADJUST,
        .icon_type = IT_DICE,
        .help      = "Half-shutter x10 zoom (same as SET). ON: hold to zoom; Sticky: tap to toggle.",
        .help2     = "Only in movie LV with ML overlays. Not active while recording.",
    },
    {
        .name      = "Shutter record",
        .priv      = &Shutter_rec,
        .max       = 1,
        .choices   = CHOICES("OFF", "Half-press"),
        .edit_mode = EM_INLINE_ADJUST,
        .icon_type = IT_DICE,
        .help      = "Half-press the shutter button to start/stop recording (movie mode).",
        .help2     = "Press only halfway: a full press still takes a photo. Replaces Shutter zoom.",
    },
};

/* Mode UI: 0=1x1, 1=1x3, 2=3x3, 3=LV (Full-Res LiveView). */
static int slim_mode_ui = 2; /* default: 3x3 (S35) */
static int slim_unified_preset = 1; /* Highest=0 Higher=1 Medium=2 */
static int slim_bit_depth_ui = 2;   /* 0=10 1=11 2=12 3=14 → bit_depth_analog 3/2/1/0 */
/* Crop register changes are applied asynchronously at frame boundaries.
 * Do not let direct-touch input start another transition while the previous
 * preview geometry is still settling. */
static int slim_touch_crop_ready_at = 0;
#define SLIM_TOUCH_CROP_SETTLE_MS 900

/* 1x1 Aspect Ratio UI: 0=2.33:1, 1=2.35:1, 2=16:9, 3=3:2, 4=4:3 */
static int slim_1x1_ar = 2; /* default 16:9 */
__attribute__((unused)) static const char * const slim_1x1_ar_labels[5] = {
    "2.33:1", "2.35:1", "16:9", "3:2", "4:3"
};

/* ---- Film Format menu ------------------------------------------------------
 * The Movie menu offers eleven formats in two Standards (menu row "Standard"):
 *   FILM   Academy 35mm, A35 Anamorphic, S16, 16mm, S8, 8mm   (the default)
 *   VIDEO  video sensor sizes at 1:1: 2/3", 1/2", 1/2.3", 1/3", 1/4"
 * The old Mode / Aspect Ratio / Preset choices are no longer exposed.  Rows are reused:
 *   "Mode" row         -> Film Format  (or Sensor Size in the VIDEO standard)
 *   "Aspect Ratio" row -> Frame        (Actual / 16:9 Crop / 1.85:1 Crop ...)
 *   "Preset" row       -> hidden
 * Each Frame is cut from one existing sensor readout (no register is changed):
 *   A35, A35 Anamorphic  3x3 binning, 3:2 readout 1736x1160 (anamorphic: squeezed
 *                        windows, 2x: 1.18:1, 1.33x: 4:3, de-squeezed in post)
 *   S16                  1:1 2.35:1 3K    16mm  1:1 16:9 2560x1440
 *   S8, 8mm              1:1 3:2 1920x1280
 *   VIDEO sizes          1:1 1440p, 1620p 4:3, 2.5K or 1280p: see film_frames[]
 * In the VIDEO standard the Frame choice can move to a different readout (for example
 * 2/3" 16:9 is cut from 1440p and 2/3" 4:3 from 1620p).
 * mlv_lite cuts the window out of the readout and asks crop_rec_film_format() which
 * window to use: the number it returns is the index into film_frames[] in
 * src/film-formats.h (0 = not a film format).
 */
/* names, labels, frame choices and recorded sizes all come from src/film-formats.h */
#define SLIM_FILM_FORMATS FILM_FORMAT_COUNT
/* Both are saved in the module config, so the choice survives a reboot.  The sensor
 * readout is already saved by the crop_preset_* settings, but several formats share a
 * readout (A35 / A35 Anamorphic, S8 / 8mm, most video sizes): without these the group
 * would fall back to its first format (and the Frame choice to the first entry) at every start. */
static CONFIG_INT("crop.film_fmt", slim_film_fmt, 0);        /* selected Film Format 0..10 (6 and up = VIDEO) */
static CONFIG_INT("crop.film_frames", slim_film_frames, 0);  /* Frame choice per format, 2 bits each */

static int slim_film_frame_get(int fmt)
{
    int v;
    fmt = COERCE(fmt, 0, FILM_FORMAT_COUNT - 1);
    v = (slim_film_frames >> (2 * fmt)) & 3;
    return COERCE(v, 0, film_formats[fmt].count - 1);
}

static void slim_film_frame_set(int fmt, int v)
{
    slim_film_frames = (slim_film_frames & ~(3 << (2 * fmt))) | ((v & 3) << (2 * fmt));
}

/* the VIDEO standard is active (used by the fps code, which is defined earlier) */
static int slim_video_standard(void)
{
    return film_is_video(slim_film_fmt);
}

/* Sensor readout (FILM_RO_*) that the menu state asks for, or -1 for a leftover legacy mode */
static int slim_film_menu_readout(void)
{
    switch (slim_mode_ui)
    {
        case 2: /* 3x3 3:2 */
            return crop_preset_ar_menu == 4 ? FILM_RO_3X3 : -1;
        case 0:
            switch (slim_1x1_ar)
            {
                case 0: return FILM_RO_25K;
                case 1: return slim_unified_preset == 0 ? FILM_RO_3K : -1; /* 2.8K Higher is legacy */
                case 2: return FILM_RO_1440;
                case 3: return FILM_RO_1280;
                case 4: return FILM_RO_1620;
            }
    }
    return -1;
}

/* Which Film Format does the current menu state correspond to?  Returns
 * 0..10, or -1 for a leftover legacy mode (not in the film list).  Also repairs
 * the saved format and Frame choice so they agree with the readout. */
static int slim_film_sync(void)
{
    int frame;
    int fmt = film_pick(slim_film_menu_readout(), slim_film_fmt, slim_film_frame_get(slim_film_fmt), &frame);
    if (fmt >= 0)
    {
        slim_film_fmt = fmt;
        slim_film_frame_set(fmt, frame);
    }
    return fmt;
}

/* Readout of the real crop mode (not the menu state), or -1 */
static int slim_film_active_readout(void)
{
    if (!crop_rec_is_enabled())
        return -1;
    if (CROP_PRESET_MENU == CROP_PRESET_3X3)
        return crop_preset_3x3_res_menu == 2 ? FILM_RO_3X3 : -1;
    if (CROP_PRESET_MENU == CROP_PRESET_1X1)
    {
        switch (crop_preset_1x1_res_menu)
        {
            case 0: return FILM_RO_25K;
            case 2: return FILM_RO_3K;
            case 3: return FILM_RO_1440;
            case 4: return FILM_RO_1280;
            case 6: return FILM_RO_1620;
        }
    }
    return -1;
}

/* Film Format of the real crop mode: 0..10 or -1 (no writes: also called from mlv_lite) */
static int slim_film_active(void)
{
    int frame;
    return film_pick(slim_film_active_readout(), slim_film_fmt, slim_film_frame_get(slim_film_fmt), &frame);
}

/* used by mlv_lite; reads the real crop mode, not the menu state */
int crop_rec_shutter_record()
{
    return is_EOSM && Shutter_rec;
}

/* 1 when the active recording format is one of the FILM standard formats (shutter shown as an angle) */
int crop_rec_film_standard()
{
    int fmt = slim_film_active();
    return fmt >= 0 && !film_is_video(fmt);
}

/* 1 when the active recording format is one of the VIDEO standard sizes (ISO is shown as Gain) */
int crop_rec_video_standard()
{
    int fmt = slim_film_active();
    return fmt >= 0 && film_is_video(fmt);
}

int crop_rec_film_format()
{
    int frame;
    int fmt = film_pick(slim_film_active_readout(), slim_film_fmt, slim_film_frame_get(slim_film_fmt), &frame);
    if (fmt < 0)
        return 0; /* not a film format */
    return film_frame_index(fmt, frame);
}

static void slim_crop_apply_mode(void);
static void slim_crop_apply_unified_preset(void);
static void slim_crop_clamp_fps(void);

/* How many Preset choices are selectable right now (1 → row should be greyed). */
static int slim_preset_choice_count(void)
{
    if (slim_mode_ui == 3 || slim_film_sync() >= 0)
        return 1; /* LV and film formats: one readout each */
    if (slim_mode_ui == 2)
        return 1; /* 3x3: Highest only — one res per Aspect Ratio */
    if (slim_mode_ui == 0)
    {
        /* 1x1: only 2.35:1 has Higher + Highest */
        return (slim_1x1_ar == 1 && slim_film_sync() < 0) ? 2 : 1;
    }
    /* 1x3: Highest / Higher / Medium */
    return 3;
}

/* Map 3x3 Aspect Ratio → backend High FPS (0) or mv1080 3:2 (2). */
static void slim_crop_apply_3x3_from_ar(void)
{
    int ar = COERCE(crop_preset_ar_menu, 0, 4);
    slim_unified_preset = 0;
    if (ar == 4)
    {
        /* 3:2 → mv1080_3_2 @ 23.976/25/30 */
        crop_preset_3x3_res_menu = 2;
    }
    else
    {
        /* 16:9 / 2:1 / 2.20 / 2.35 → High FPS presets */
        crop_preset_3x3_res_menu = 0;
        crop_preset_fps_reduce = 0; /* full High FPS timers */
    }
}

/* Map 1x1 AR (+ Preset for 2.35:1) → res index, WxH, FPS mask. */
static void slim_1x1_resolve(int *res_idx, int *w, int *h, int *fps_mask)
{
    slim_1x1_ar = COERCE(slim_1x1_ar, 0, 4);

    if (slim_1x1_ar == 0)
    {
        /* 2.33:1 → 2520x1080 @ 24/25/30 — Highest only */
        *res_idx = 0;
        *w = 2520; *h = 1080;
        *fps_mask = 0x7;
        slim_unified_preset = 0;
    }
    else if (slim_1x1_ar == 1)
    {
        /* 2.35:1 — Higher=2880x1226 @24/25; Highest=3072x1308 @24 */
        if (slim_unified_preset > 1)
            slim_unified_preset = 0;
        if (slim_unified_preset == 1)
        {
            *res_idx = 1; /* 2.8K Higher */
            *w = 2880; *h = 1226;
            *fps_mask = 0x3;
        }
        else
        {
            slim_unified_preset = 0;
            *res_idx = 2; /* 3K Highest */
            *w = 3072; *h = 1308;
            *fps_mask = 0x1;
        }
    }
    else if (slim_1x1_ar == 2)
    {
        /* 16:9 → 2560x1440 @ 24/25 — Highest only */
        *res_idx = 3;
        *w = 2560; *h = 1440;
        *fps_mask = 0x3;
        slim_unified_preset = 0;
    }
    else if (slim_1x1_ar == 3)
    {
        /* 3:2 → 1920x1280 @ 24/25 — Highest only */
        *res_idx = 4;
        *w = 1920; *h = 1280;
        /* FILM: 23.976 / 25 / 18 (Super 8 and 8mm).  VIDEO: 23.976 / 25 / 29.97 (TimerB 0x75D = 29.977,
         * untested on this readout; the original 1280p preset never offered 30). */
        *fps_mask = slim_video_standard() ? 0x7 : 0xB;
        slim_unified_preset = 0;
    }
    else
    {
        /* 4:3 → 2160x1620 @ 23.976 FPS (dannephoto CROP_1620p; single TimerB) — Highest only */
        *res_idx = 6;
        *w = 2160; *h = 1620;
        *fps_mask = 0x3;   /* 1620p (2/3" 4:3): 23.976 / 25; 29.97 does not work here */
        slim_unified_preset = 0;
    }
}

static void slim_crop_sync_from_backend(void)
{
    if (CROP_PRESET_MENU == CROP_PRESET_1X1 && crop_preset_1x1_res_menu == 5)
    {
        slim_mode_ui = 3; /* LV */
        slim_unified_preset = 0;
    }
    else if (CROP_PRESET_MENU == CROP_PRESET_1X1)
    {
        slim_mode_ui = 0;
        switch (crop_preset_1x1_res_menu)
        {
            case 0: /* 2.5K */
                slim_1x1_ar = 0;
                slim_unified_preset = 0;
                break;
            case 1: /* 2.8K Higher */
                slim_1x1_ar = 1;
                slim_unified_preset = 1;
                break;
            case 2: /* 3K Highest */
                slim_1x1_ar = 1;
                slim_unified_preset = 0;
                break;
            case 3: /* 1440p */
                slim_1x1_ar = 2;
                slim_unified_preset = 0;
                break;
            case 4: /* 1280p */
                slim_1x1_ar = 3;
                slim_unified_preset = 0;
                break;
            case 6: /* 1620p 4:3 */
                slim_1x1_ar = 4;
                slim_unified_preset = 0;
                break;
            default:
                slim_1x1_ar = 2;
                slim_unified_preset = 0;
                break;
        }
    }
    else if (CROP_PRESET_MENU == CROP_PRESET_1X3)
    {
        slim_mode_ui = 1;
        slim_unified_preset = COERCE(crop_preset_1x3_res_menu, 0, 2);
    }
    else if (CROP_PRESET_MENU == CROP_PRESET_3X3)
    {
        slim_mode_ui = 2;
        slim_unified_preset = 0;
        /* Keep AR; backend res comes from slim_crop_apply_3x3_from_ar. */
    }

    if (OUTPUT_10BIT) slim_bit_depth_ui = 0;
    else if (OUTPUT_11BIT) slim_bit_depth_ui = 1;
    else if (OUTPUT_12BIT) slim_bit_depth_ui = 2;
    else slim_bit_depth_ui = 3; /* 14-bit */
}

static void slim_crop_apply_unified_preset(void)
{
    if (slim_mode_ui == 0)
    {
        /* Apply via slim_1x1_resolve in slim_crop_apply_mode. */
        return;
    }
    if (slim_mode_ui == 2)
    {
        slim_crop_apply_3x3_from_ar();
        return;
    }
    slim_unified_preset = COERCE(slim_unified_preset, 0, 2);
    if (slim_mode_ui == 1 || CROP_PRESET_MENU == CROP_PRESET_1X3)
        crop_preset_1x3_res_menu = slim_unified_preset;
}

static void slim_crop_apply_mode(void)
{
    slim_mode_ui = COERCE(slim_mode_ui, 0, 3);

    if (slim_mode_ui == 3)
    {
        /* LV → 1x1 Full-Res backend @ 5208x3478 */
        crop_preset_index = 1;
        crop_preset_1x1_res_menu = 5;
        slim_unified_preset = 0;
    }
    else
    {
        crop_preset_index = slim_mode_ui + 1; /* 1x1 / 1x3 / 3x3 */
        if (slim_mode_ui == 0)
        {
            int res_idx, w, h, fps_mask;
            slim_1x1_resolve(&res_idx, &w, &h, &fps_mask);
            crop_preset_1x1_res_menu = res_idx;
            (void)w; (void)h; (void)fps_mask;
        }
        else
            slim_crop_apply_unified_preset();
    }
    slim_crop_clamp_fps();
}

static void slim_crop_apply_bit_depth(void)
{
    static const int map[] = { 3, 2, 1, 0 }; /* 10, 11, 12, 14 */
    int prev = bit_depth_analog;
    slim_bit_depth_ui = COERCE(slim_bit_depth_ui, 0, 3);
    bit_depth_analog = map[slim_bit_depth_ui];
    if (bit_depth_analog != prev)
        raw_invalidate_lv_calibration();
}

/* Expected RAW WxH for EOS M (from crop_rec help / reg_override). */
static void slim_crop_expected_res(int *w, int *h)
{
    *w = 1376;
    *h = 2322; /* default Higher 1x3 16:9 */

    if (slim_mode_ui == 3 || (CROP_PRESET_MENU == CROP_PRESET_1X1 && crop_preset_1x1_res_menu == 5))
    {
        *w = 5208;
        *h = 3478;
        return;
    }

    if (CROP_PRESET_MENU == CROP_PRESET_OFF)
        return;

    if (CROP_PRESET_MENU == CROP_PRESET_1X3)
    {
        int p = COERCE(crop_preset_1x3_res_menu, 0, 2);
        int ar = crop_preset_ar_menu;
        if (ar == 0) { /* 16:9 */
            if (p == 0) { *w = 1504; *h = 2538; }
            else if (p == 1) { *w = 1376; *h = 2322; }
            else { *w = 1280; *h = 2160; }
        } else if (ar == 1) { /* 2:1 */
            if (p == 0) { *w = 1600; *h = 2400; }
            else if (p == 1) { *w = 1472; *h = 2208; }
            else { *w = 1360; *h = 2040; }
        } else if (ar == 2) { /* 2.20:1 */
            if (p == 0) { *w = 1664; *h = 2268; }
            else if (p == 1) { *w = 1552; *h = 2216; }
            else { *w = 1424; *h = 1942; }
        } else if (ar == 3) { /* 2.35:1 */
            if (p == 0) { *w = 1736; *h = 2214; }
            else if (p == 1) { *w = 1600; *h = 2040; }
            else { *w = 1472; *h = 1878; }
        } else { /* 2.39:1 */
            if (p == 0) { *w = 1736; *h = 2178; }
            else if (p == 1) { *w = 1600; *h = 2008; }
            else { *w = 1472; *h = 1846; }
        }
        return;
    }

    if (CROP_PRESET_MENU == CROP_PRESET_1X1)
    {
        int res_idx, fps_mask;
        slim_1x1_resolve(&res_idx, w, h, &fps_mask);
        (void)res_idx; (void)fps_mask;
        return;
    }

    if (CROP_PRESET_MENU == CROP_PRESET_3X3)
    {
        /* One resolution per Aspect Ratio (Preset always Highest / High FPS or 3:2). */
        int ar = COERCE(crop_preset_ar_menu, 0, 4);
        if (ar == 0) { *w = 1736; *h = 976; }
        else if (ar == 1) { *w = 1736; *h = 868; }
        else if (ar == 2) { *w = 1736; *h = 790; }
        else if (ar == 3) { *w = 1736; *h = 738; }
        else { *w = 1736; *h = 1160; } /* 3:2 */
        return;
    }
}

/* Bit0=23.976 Bit1=25 Bit2=30 Bit3=18 (1:1 3:2 only) — rates allowed for current Mode/AR/Preset on EOS M.
 * LV: return 0 (handled specially as 3 fps). */
static int slim_crop_fps_mask(void)
{
    if (slim_mode_ui == 3 || (CROP_PRESET_MENU == CROP_PRESET_1X1 && crop_preset_1x1_res_menu == 5))
        return 0;

    if (CROP_PRESET_MENU == CROP_PRESET_OFF)
        return 0x1;

    if (CROP_PRESET_MENU == CROP_PRESET_1X3)
    {
        int p = COERCE(crop_preset_1x3_res_menu, 0, 2);
        int ar = crop_preset_ar_menu;
        if (p == 0) return 0x1;                 /* Highest: fixed ~24 only */
        if (p == 1 && ar == 0) return 0x1;       /* Higher 16:9: 23.976 only */
        return 0x1 | 0x2;                       /* Higher/Medium elsewhere: 24+25 */
    }

    if (CROP_PRESET_MENU == CROP_PRESET_1X1)
    {
        int res_idx, w, h, fps_mask;
        slim_1x1_resolve(&res_idx, &w, &h, &fps_mask);
        (void)res_idx; (void)w; (void)h;
        return fps_mask;
    }

    if (CROP_PRESET_MENU == CROP_PRESET_3X3)
    {
        int ar = COERCE(crop_preset_ar_menu, 0, 4);
        if (ar == 4)
            return 0x1 | 0x2;       /* 3:2 (A35, A35 Anamorphic) → 23.976 / 25; no 30 in FILM */
        return 0; /* High FPS AR: single fixed rate (shown specially) */
    }

    return 0x1;
}

static void slim_crop_clamp_fps(void)
{
    int mask = slim_crop_fps_mask();
    if (!mask)
        return; /* LV @ 3 fps — no 24/25/30 index */
    if (mask & (1 << crop_preset_fps_menu))
        return;
    for (int i = 0; i < 4; i++)
    {
        if (mask & (1 << i))
        {
            crop_preset_fps_menu = i;
            return;
        }
    }
    crop_preset_fps_menu = 0;
}

/* Point the crop settings at the readout of a Film Format's current Frame choice */
static void slim_film_apply(int fmt)
{
    fmt = COERCE(fmt, 0, FILM_FORMAT_COUNT - 1);
    slim_film_fmt = fmt;
    slim_unified_preset = 0; /* every film readout is a single Highest mode */
    switch (film_frames[film_frame_index(fmt, slim_film_frame_get(fmt))].readout)
    {
        case FILM_RO_3X3:  slim_mode_ui = 2; crop_preset_ar_menu = 4; break; /* 3x3 3:2 */
        case FILM_RO_3K:   slim_mode_ui = 0; slim_1x1_ar = 1; break;         /* 2.35:1 3K */
        case FILM_RO_1440: slim_mode_ui = 0; slim_1x1_ar = 2; break;         /* 16:9 */
        case FILM_RO_1280: slim_mode_ui = 0; slim_1x1_ar = 3; break;         /* 3:2 */
        case FILM_RO_25K:  slim_mode_ui = 0; slim_1x1_ar = 0; break;         /* 2.33:1 */
        default:           slim_mode_ui = 0; slim_1x1_ar = 4; break;         /* 1620p 4:3 */
    }
    slim_crop_apply_mode();
}

/* "Standard" row: FILM <-> VIDEO.  Each standard remembers the format last used in it
 * (until the next start; then it begins at its first format). */
static int slim_std_last[2] = { 0, FILM_FILM_COUNT };
static int slim_standard_ui = 0;   /* 0 = FILM, 1 = VIDEO: only mirrors the saved format for the menu */

static MENU_SELECT_FUNC(slim_crop_standard_select)
{
    int cur;
    int std;

    (void)priv; (void)delta;
    slim_crop_sync_from_backend();
    cur = slim_film_sync();
    if (cur < 0)
        cur = slim_film_fmt;
    std = film_is_video(cur) ? 1 : 0;
    slim_std_last[std] = cur;
    slim_film_apply(slim_std_last[1 - std]);
}

static MENU_UPDATE_FUNC(slim_crop_standard_update)
{
    slim_crop_sync_from_backend();
    slim_film_sync();
    slim_standard_ui = film_is_video(slim_film_fmt) ? 1 : 0;
    MENU_SET_VALUE("%s", slim_standard_ui ? "VIDEO" : "FILM");
    MENU_SET_HELP("FILM: motion picture film gates. VIDEO: 2/3\", 1/2\", 1/2.3\", 1/3\", 1/4\" sensor sizes.");
    MENU_SET_ENABLED(1);
}

/* "Mode" row = Film Format */
static MENU_SELECT_FUNC(slim_crop_mode_select)
{
    slim_crop_sync_from_backend();
    int fmt = slim_film_sync();
    if (fmt < 0)
        fmt = 0; /* leaving a legacy mode: always start at A35, whichever way was pressed */
    else
    {
        /* cycle inside the current standard (A35 .. 8mm, or 2/3" .. 1/4"), see film_step() */
        fmt = film_step(fmt, delta);
    }
    slim_film_apply(fmt);
}

static MENU_UPDATE_FUNC(slim_crop_mode_update)
{
    slim_crop_sync_from_backend();
    int fmt = slim_film_sync();
    MENU_SET_NAME(film_is_video(slim_film_fmt) ? "Sensor Size" : "Film Format");
    if (fmt >= 0)
    {
        MENU_SET_VALUE("%s", film_formats[fmt].name);
        if (film_is_video(fmt))
            MENU_SET_HELP("Video sensor size at 1:1. Sets the sensor readout and the recorded window.");
        else
            MENU_SET_HELP("Film gate. Sets the sensor readout and the recorded window.");
    }
    else
    {
        MENU_SET_VALUE("Choose...");
        MENU_SET_HELP("Press left or right to pick a film format.");
    }
    MENU_SET_ENABLED(1);
}

static MENU_SELECT_FUNC(slim_crop_size_select)
{
    (void)priv; (void)delta; /* Recorded Size is read-only */
}

static MENU_SELECT_FUNC(slim_crop_preset_select)
{
    int film_fmt = slim_film_sync();
    if (film_fmt >= 0 && film_formats[film_fmt].count > 1)
    {
        /* Frame: Actual / 16:9 Crop / ... (either arrow cycles) */
        int cur = slim_film_frame_get(film_fmt);
        int next = MOD(cur + (delta < 0 ? -1 : 1), film_formats[film_fmt].count);
        int old_ro = film_frames[film_frame_index(film_fmt, cur)].readout;
        slim_film_frame_set(film_fmt, next);
        /* the VIDEO sizes can use a different readout per Frame: switch to it */
        if (film_frames[film_frame_index(film_fmt, next)].readout != old_ro)
            slim_film_apply(film_fmt);
        return;
    }

    int n = slim_preset_choice_count();
    if (n <= 1)
        return;

    /* Both L and R: Medium → Higher → Highest → Medium… (never reverse).
     * For 1x1 2.35:1 (2 choices): Higher → Highest → Higher… */
    (void)delta;
    if (n == 2)
    {
        slim_unified_preset = (slim_unified_preset == 1) ? 0 : 1;
        slim_crop_apply_mode();
        return;
    }

    if (slim_unified_preset == 2)
        slim_unified_preset = 1;      /* Medium → Higher */
    else if (slim_unified_preset == 1)
        slim_unified_preset = 0;      /* Higher → Highest */
    else
        slim_unified_preset = 2;      /* Highest → Medium */

    slim_crop_apply_unified_preset();
    slim_crop_clamp_fps();
}

/* Like slim_crop_expected_res, but for film formats returns the size that is
 * actually recorded (not the sensor readout). */
static void slim_crop_shown_res(int *w, int *h)
{
    slim_crop_expected_res(w, h);
    if (slim_film_active() >= 0 || slim_film_sync() >= 0)
    {
        int fmt = slim_film_active() >= 0 ? slim_film_active() : slim_film_sync();
        int i = film_frame_index(fmt, slim_film_frame_get(fmt));
        *w = film_frames[i].w;
        *h = film_frames[i].h;
    }
}

static MENU_UPDATE_FUNC(slim_crop_preset_update)
{
    slim_crop_sync_from_backend();

    {
        /* Preset row: read-only "Recorded Size" for film formats (the Frame
         * choice lives in the Aspect Ratio row). */
        int film_fmt = slim_film_sync();
        if (film_fmt >= 0)
        {
            MENU_SET_NAME("Recorded Size");
            {
                int i = film_frame_index(film_fmt, slim_film_frame_get(film_fmt));
                MENU_SET_VALUE("%dx%d", film_frames[i].w, film_frames[i].h);
            }
            MENU_SET_HELP("Size of the recorded picture (read-only).");
            MENU_SET_ENABLED(0);
            return;
        }
    }

    if (slim_mode_ui == 0 && slim_1x1_ar == 1)
    {
        /* Keep only Highest / Higher for 2.35:1 */
        if (slim_unified_preset > 1)
            slim_unified_preset = 0;
        MENU_SET_VALUE("%s", slim_unified_preset == 1 ? "Higher" : "Highest");
        MENU_SET_ENABLED(1);
        return;
    }

    if (slim_preset_choice_count() <= 1)
    {
        slim_unified_preset = 0;
        MENU_SET_VALUE("Highest");
        MENU_SET_ENABLED(0); /* locked — greyed */
        return;
    }

    MENU_SET_VALUE("%s",
        slim_unified_preset == 0 ? "Highest" :
        slim_unified_preset == 1 ? "Higher" : "Medium");
    MENU_SET_ENABLED(1);
}

/* "Aspect Ratio" row = Frame */
static MENU_UPDATE_FUNC(slim_crop_ar_update)
{
    slim_crop_sync_from_backend();
    int fmt = slim_film_sync();
    MENU_SET_NAME("Frame");
    if (fmt < 0)
    {
        MENU_SET_VALUE("-");
        MENU_SET_ENABLED(0);
        return;
    }
    MENU_SET_VALUE("%s", film_frames[film_frame_index(fmt, slim_film_frame_get(fmt))].frame);
    MENU_SET_HELP("Frame inside the %s %s: Actual size or a crop.", film_formats[fmt].name, film_is_video(fmt) ? "sensor" : "gate");
    MENU_SET_ENABLED(film_formats[fmt].count > 1);
}

static MENU_SELECT_FUNC(slim_crop_ar_select)
{
    slim_crop_sync_from_backend();
    slim_crop_preset_select(priv, delta);
}

static MENU_UPDATE_FUNC(slim_crop_res_update)
{
    int w, h;
    slim_crop_sync_from_backend();
    slim_crop_expected_res(&w, &h);
    if (slim_film_sync() >= 0)
    {
        /* Film formats: the Recorded Size row is what matters; hide this one. */
        MENU_SET_SHIDDEN(1);
        return;
    }
    MENU_SET_VALUE("%dx%d", w, h);
    /* Read-only: greyed via enabled=0 */
    MENU_SET_ENABLED(0);
}

/* Quick Screen resolution stays within the current Aspect Ratio. Aspect
 * Ratio is changed only by its own control, so resolution arrows never
 * expose a temporary cross-aspect combination. */
static MENU_SELECT_FUNC(slim_crop_quick_res_select)
{
    int choices;

    slim_crop_sync_from_backend();
    if (slim_mode_ui == 3)
        return; /* Full-Res LV has one fixed resolution. */

    choices = slim_preset_choice_count();
    slim_unified_preset = COERCE(slim_unified_preset, 0, choices - 1);

    /* Up moves toward higher resolution; down toward lower resolution.
     * Wrap inside this Aspect Ratio, never into an adjacent one. */
    slim_unified_preset = MOD(
        slim_unified_preset + (delta > 0 ? -1 : 1), choices);

    if (slim_mode_ui == 0)
        slim_crop_apply_mode();
    else if (slim_mode_ui == 1)
        slim_crop_apply_unified_preset();
    else
        slim_crop_apply_3x3_from_ar();
    slim_crop_clamp_fps();
}

static MENU_UPDATE_FUNC(slim_crop_quick_res_update)
{
    int w, h;
    slim_crop_sync_from_backend();
    slim_crop_shown_res(&w, &h);
    MENU_SET_VALUE("%dx%d", w, h);
    MENU_SET_ENABLED(slim_mode_ui != 3 && slim_preset_choice_count() > 1);
}

/* The direct Live View editor has no Aspect Ratio item of its own. Its
 * Resolution arrows therefore cycle complete, known-good geometry pairs for
 * the selected mode. Keep Quick Screen's separate selector constrained to
 * its Aspect Ratio as designed. */
__attribute__((unused)) static void slim_crop_touch_res_select(int delta)
{
    slim_crop_sync_from_backend();
    if (slim_mode_ui == 3)
        return;

    if (slim_mode_ui == 0)
    {
        static const int res_list[] = { 0, 1, 2, 3, 4, 6 };
        int pos = 0;
        for (int i = 0; i < COUNT(res_list); i++)
            if (crop_preset_1x1_res_menu == res_list[i])
                pos = i;
        pos = MOD(pos + (delta > 0 ? -1 : 1), COUNT(res_list));
        crop_preset_1x1_res_menu = res_list[pos];
        slim_crop_sync_from_backend();
    }
    else if (slim_mode_ui == 1)
    {
        /* Five Aspect Ratios, each with Highest / Higher / Medium. */
        int pos = COERCE(crop_preset_ar_menu, 0, 4) * 3 +
                  COERCE(crop_preset_1x3_res_menu, 0, 2);
        pos = MOD(pos + (delta > 0 ? -1 : 1), 15);
        crop_preset_ar_menu = pos / 3;
        crop_preset_1x3_res_menu = pos % 3;
        slim_unified_preset = crop_preset_1x3_res_menu;
    }
    else /* 3x3: one supported resolution per Aspect Ratio */
    {
        crop_preset_ar_menu = MOD(crop_preset_ar_menu +
                                  (delta > 0 ? -1 : 1), 5);
        slim_crop_apply_3x3_from_ar();
    }

    slim_crop_clamp_fps();
}

static MENU_SELECT_FUNC(slim_crop_fps_select)
{
    if (slim_mode_ui == 3)
        return; /* LV: 3 fps only */

    int mask = slim_crop_fps_mask();
    int bits = (mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1) + ((mask >> 3) & 1);
    if (bits <= 1)
        return;

    /* order: 18 <-> 23.976 <-> 25 <-> 30 (index 3 is 18) */
    static const int order[4] = { 3, 0, 1, 2 };
    int pos = 0;
    for (int k = 0; k < 4; k++) if (order[k] == crop_preset_fps_menu) pos = k;
    for (int step = 0; step < 4; step++)
    {
        pos = MOD(pos + (delta < 0 ? -1 : 1), 4);
        if (mask & (1 << order[pos]))
        {
            crop_preset_fps_menu = order[pos];
            return;
        }
    }
}

static MENU_UPDATE_FUNC(slim_crop_fps_update)
{
    slim_crop_clamp_fps();

    if (slim_mode_ui == 3 || (CROP_PRESET_MENU == CROP_PRESET_1X1 && crop_preset_1x1_res_menu == 5))
    {
        MENU_SET_VALUE("3");
        MENU_SET_ENABLED(0);
        return;
    }

    /* 3x3 High FPS: one fixed rate per Aspect Ratio. */
    if (CROP_PRESET_MENU == CROP_PRESET_3X3)
    {
        int ar = COERCE(crop_preset_ar_menu, 0, 4);
        if (ar < 4)
        {
            static const char * hfr[] = { "46.800", "50", "54", "55.6" };
            MENU_SET_VALUE("%s", hfr[ar]);
            MENU_SET_ENABLED(0);
            return;
        }
        /* ar == 4 (3:2): fall through to 23.976 / 25 / 30 */
    }

    /* EOS M 1x3 Highest 16:9 runs at 22.250, not 23.976. */
    if (CROP_PRESET_MENU == CROP_PRESET_1X3
        && COERCE(crop_preset_1x3_res_menu, 0, 2) == 0
        && crop_preset_ar_menu == 0)
    {
        MENU_SET_VALUE("22.250");
        MENU_SET_ENABLED(0);
        return;
    }

    /* EOS M 1x3 Highest 2:1 (1600x2400) runs at 23.300, not 23.976. */
    if (CROP_PRESET_MENU == CROP_PRESET_1X3 && COERCE(crop_preset_1x3_res_menu, 0, 2) == 0 && crop_preset_ar_menu == 1 && is_EOSM)
    {
        MENU_SET_VALUE("23.300");
        MENU_SET_ENABLED(0);
        return;
    }

    static const char * labels[] = { "23.976", "25", "30", "18" };
    if (slim_video_standard() && crop_preset_fps_menu == 2)
        MENU_SET_VALUE("29.97");   /* VIDEO standard: 2.5K readout snapped to 29.97 */
    else
        MENU_SET_VALUE("%s", labels[COERCE(crop_preset_fps_menu, 0, 3)]);

    /* Only one valid rate → show it greyed (read-only). */
    int mask = slim_crop_fps_mask();
    int bits = (mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1) + ((mask >> 3) & 1);
    if (bits <= 1)
        MENU_SET_ENABLED(0);
}

static MENU_SELECT_FUNC(slim_crop_bit_select)
{
    /* Direct-touch arrows and menu L/R move in opposite directions:
     * 10 <-> 11 <-> 12 <-> 14, wrapping at the ends. */
    slim_bit_depth_ui = MOD(slim_bit_depth_ui + (delta < 0 ? -1 : 1), 4);
    slim_crop_apply_bit_depth();
}

static MENU_UPDATE_FUNC(slim_crop_bit_update)
{
    slim_crop_sync_from_backend();
    MENU_SET_VALUE("%s",
        slim_bit_depth_ui == 0 ? "10 Bit" :
        slim_bit_depth_ui == 1 ? "11 Bit" :
        slim_bit_depth_ui == 2 ? "12 Bit" : "14 Bit");
    /* Never gate Bit Depth on lossless / other settings. */
}

/* Called by the core Live View touch editor.  This intentionally bypasses
 * menu_entry lookup and menu semaphores: the menu task is not active while
 * the camera is displaying Live View, and touching these fields must not
 * enter Canon's menu lock path (Err70 on EOS M). */
/* These entry points are called from core through MODULE_FUNCTION().  Keep
 * them in the module image even though no in-module caller references them;
 * otherwise section garbage collection can discard the exports and the core
 * pointer silently falls back to the weak stub (rendering "--" in the editor).
 */
__attribute__((used, noinline))
int crop_rec_touch_adjust(int control, int delta)
{
    int old_irq = 0;
    int now;

    if (!is_movie_mode() || RECORDING)
        return 0;

    if (control == 0 || control == 1)
    {
        now = get_ms_clock();
        if ((int)(now - slim_touch_crop_ready_at) < 0)
            return 0;

        /* The crop backend reads these configuration words from frame-time
         * callbacks.  Publish mode/AR/resolution as one atomic state so it
         * can never observe a new AR paired with the previous resolution. */
        old_irq = cli();
    }

    switch (control)
    {
        case 0:
            /* Direct Live View editor intentionally offers only 1x1/1x3/3x3.
             * Full-Res LV remains available in the regular Movie menu. */
            slim_crop_sync_from_backend();
            /* Film Format: the on-screen down arrow steps to the next smaller film
             * (A35 > A35-ANA > S16 > 16mm > S8 > 8mm), the up arrow steps back. */
            slim_crop_mode_select(0, -delta);
            break;
        case 1:
            slim_crop_sync_from_backend();
            slim_crop_preset_select(0, delta); /* Frame */
            break;
        case 2: slim_crop_fps_select(0, delta); break;
        case 3: slim_crop_bit_select(0, delta); break;
        default:
            if (control == 0 || control == 1)
                sei(old_irq);
            return 0;
    }

    if (control == 0 || control == 1)
    {
        sei(old_irq);
        slim_touch_crop_ready_at = get_ms_clock() + SLIM_TOUCH_CROP_SETTLE_MS;
        raw_set_dirty();
    }
    return 1;
}

__attribute__((used, noinline))
int crop_rec_touch_get_value(int control, int slot, char *value, int size,
                             int *enabled_out)
{
    int enabled = 1;
    int w, h;

    if (!value || size <= 0 || !enabled_out)
        return 0;

    value[0] = '\0';
    slim_crop_sync_from_backend();

    if (control == 0)
    {
        if (slot == 0)
        {
            int fmt = slim_film_sync();
            snprintf(value, size, "%s", fmt >= 0 ? film_formats[fmt].label : "-");
        }
        else
        {
            slim_crop_shown_res(&w, &h);
            snprintf(value, size, "%dx%d", w, h);
            enabled = slim_mode_ui != 3;
        }
    }
    else if (control == 1)
    {
        if (slim_mode_ui == 3)
        {
            snprintf(value, size, "3");
            enabled = 0;
        }
        else
        {
            int mask = slim_crop_fps_mask();
            int bits = (mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1) + ((mask >> 3) & 1);
            if (CROP_PRESET_MENU == CROP_PRESET_3X3 && crop_preset_ar_menu < 4)
            {
                static const char *hfr[] = { "46.800", "50", "54", "55.6" };
                snprintf(value, size, "%s", hfr[COERCE(crop_preset_ar_menu, 0, 3)]);
            }
            else
            {
                static const char *labels[] = { "23.976", "25", "30", "18" };
                snprintf(value, size, "%s",
                    (slim_video_standard() && crop_preset_fps_menu == 2) ? "29.97" :
                    labels[COERCE(crop_preset_fps_menu, 0, 3)]);
            }
            enabled = bits > 1;
        }
    }
    else if (control == 2)
    {
        snprintf(value, size, "%s",
            slim_bit_depth_ui == 0 ? "10 Bit" :
            slim_bit_depth_ui == 1 ? "11 Bit" :
            slim_bit_depth_ui == 2 ? "12 Bit" : "14 Bit");
    }
    else
    {
        return 0;
    }

    *enabled_out = enabled;
    return 1;
}

/* Keep the closest supported aspect ratio when Custom changes Mode.  Most
 * ratios map exactly; nearest-match is only used when the destination mode
 * does not offer the source ratio (for example 4:3 when leaving 1x1). */
__attribute__((unused)) static int slim_crop_current_ratio_x1000(void)
{
    static const int ratios_1x1[] = { 2330, 2350, 1778, 1500, 1333 };
    static const int ratios_1x3[] = { 1778, 2000, 2200, 2350, 2390 };
    static const int ratios_3x3[] = { 1778, 2000, 2200, 2350, 1500 };

    if (slim_mode_ui == 0)
        return ratios_1x1[COERCE(slim_1x1_ar, 0, 4)];
    if (slim_mode_ui == 2)
        return ratios_3x3[COERCE(crop_preset_ar_menu, 0, 4)];
    return ratios_1x3[COERCE(crop_preset_ar_menu, 0, 4)];
}

__attribute__((unused)) static void slim_crop_set_nearest_ratio(int mode, int ratio_x1000)
{
    static const int ratios_1x1[] = { 2330, 2350, 1778, 1500, 1333 };
    static const int ratios_1x3[] = { 1778, 2000, 2200, 2350, 2390 };
    static const int ratios_3x3[] = { 1778, 2000, 2200, 2350, 1500 };
    const int *ratios = mode == 0 ? ratios_1x1 :
                        mode == 2 ? ratios_3x3 : ratios_1x3;
    int best = 0;
    int best_error = ABS(ratios[0] - ratio_x1000);

    for (int i = 1; i < 5; i++)
    {
        int error = ABS(ratios[i] - ratio_x1000);
        if (error < best_error)
        {
            best = i;
            best_error = error;
        }
    }

    if (mode == 0)
        slim_1x1_ar = best;
    else
        crop_preset_ar_menu = best;
}

/* Movie entries copied to Custom use these stricter rules rather than
 * altering the original Movie page behavior. */
int crop_rec_custom_adjust(int control, int delta)
{
    slim_crop_sync_from_backend();

    if (control == 0) /* Mode row = Film Format */
    {
        slim_crop_mode_select(0, delta);
        return 1;
    }

    if (control == 1) /* Aspect Ratio row = Frame */
    {
        slim_crop_preset_select(0, delta);
        return 1;
    }

    if (control == 2) /* Preset row is hidden */
        return 1;

    return 0;
}

/* Force a relocation to both callbacks for linkers that perform section GC. */
static void *crop_rec_touch_exports[] __attribute__((used)) = {
    (void *)&crop_rec_touch_adjust,
    (void *)&crop_rec_touch_get_value,
    (void *)&crop_rec_custom_adjust,
    (void *)&crop_rec_film_format,
    (void *)&crop_rec_film_standard,
    (void *)&crop_rec_video_standard,
    (void *)&crop_rec_shutter_record,
    (void *)&crop_rec_lv_transition_busy,
    (void *)&crop_rec_lv_transition_diag,
};

static struct menu_entry crop_rec_menu_eosm[] =
{
    {
        .name       = "Standard",
        .priv       = &slim_standard_ui,
        .select     = slim_crop_standard_select,
        .update     = slim_crop_standard_update,
        .max        = 1,
        .choices    = CHOICES("FILM", "VIDEO"),
        .edit_mode  = EM_INLINE_ADJUST,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .help       = "FILM or VIDEO sensor-size formats.",
    },
    {
        .name       = "Mode",
        .priv       = &slim_mode_ui,
        .select     = slim_crop_mode_select,
        .update     = slim_crop_mode_update,
        .min        = 0,
        .max        = 3,
        .choices    = CHOICES("1x1", "1x3", "3x3", "LV"),
        .edit_mode  = EM_INLINE_ADJUST,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .help       = "Film format (A35, A35 Anamorphic, S16, 16mm, S8, 8mm) or video sensor size.",
    },
    {
        .name       = "Aspect Ratio",
        .priv       = &crop_preset_ar_menu,
        .select     = slim_crop_ar_select,
        .update     = slim_crop_ar_update,
        .max        = 4,
        .choices    = CHOICES("16:9", "2:1", "2.20:1", "2.35:1", "2.39:1"),
        .edit_mode  = EM_INLINE_ADJUST,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .help       = "Frame inside the film gate (Actual or a crop).",
    },
    {
        .name       = "Preset",
        .priv       = &slim_unified_preset,
        .select     = slim_crop_size_select,
        .update     = slim_crop_preset_update,
        .max        = 2,
        .choices    = CHOICES("Highest", "Higher", "Medium"),
        .edit_mode  = EM_INLINE_ADJUST,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .help       = "Recorded picture size (read-only).",
    },
    {
        .name       = "Resolution",
        .update     = slim_crop_res_update,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .help       = "RAW resolution from Mode, Aspect Ratio and Preset (read-only).",
    },
    {
        .name       = "Quick Resolution",
        .select     = slim_crop_quick_res_select,
        .update     = slim_crop_quick_res_update,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .shidden    = 1,
        .help       = "Quick Screen selector for resolutions in this Aspect Ratio.",
    },
    {
        .name       = "Frame Rate",
        .priv       = &crop_preset_fps_menu,
        .select     = slim_crop_fps_select,
        .update     = slim_crop_fps_update,
        .max        = 3,
        .choices    = CHOICES("23.976", "25", "30", "18"),
        .edit_mode  = EM_INLINE_ADJUST,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .help       = "Frame rates supported by the current configuration.",
    },
    {
        .name       = "Bit Depth",
        .priv       = &slim_bit_depth_ui,
        .select     = slim_crop_bit_select,
        .update     = slim_crop_bit_update,
        .max        = 3,
        .choices    = CHOICES("10 Bit", "11 Bit", "12 Bit", "14 Bit"),
        .edit_mode  = EM_INLINE_ADJUST,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .help       = "Lossless RAW bit depth. Always available.",
    },
};

static struct menu_entry crop_rec_menu[] =
{
    // FIXME: how to handle menu in cleaner way for is_DIGIC_5 models?
    {
        .name       = "Crop mood",
        .priv       = &crop_preset_index,
        .update     = crop_update,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .children =  (struct menu_entry[]) {
            {
                .name       = "Preset:",   // CROP_PRESET_1X1
                .priv       = &crop_preset_1x1_res_menu,
                .update     = crop_preset_1x1_res_update,
                .max        = 7,
                .choices    = CHOICES("2.5K", "2.8K", "3K", "1440p", "1280p", "Full-Res LV", "1620p 4:3", "1920x1080p"),
                .help       = "Choose 1:1 preset.",
                .shidden    = 1,
            },
            {
                .name       = "Preset: ",  // CROP_PRESET_1X3
                .priv       = &crop_preset_1x3_res_menu,
                .update     = crop_preset_1x3_res_update,
                .max        = 3,
                .choices    = CHOICES("Highest", "Higher", "Medium", "Full-Res LV"),  // dummy choices, strings are being changed depending on aspect ratio and res
                .help       = "Choose 1x3 preset.",
                .shidden    = 1,
            },
            {
                .name       = "Preset:  ",  // CROP_PRESET_3X3
                .priv       = &crop_preset_3x3_res_menu,
                .update     = crop_preset_3x3_res_update,
                .max        = 2,
                .choices    = CHOICES("High FPS", "1080p", "1080p 3:2"),
                .help       = "HFR: High framerate and regular HD1080p modes.\n"
                              "Enable 1080p video mode from Canon.",
                .shidden    = 1,
            },
            /*
            {
                .name       = "Preset shortcuts",
                .priv       = &presets,
                .max        = 7,
                .choices    = CHOICES("None selected", "4.2K (16:9)", "1440p (16:9)", "976p (16:9)", "1080p (16:9)", "4.8K (2.35:1)", "2.8k (2.35:1)", "738p (2.35:1)"),
                .help       = "16:9 presets",
            },
             */
            {
                .name       = "Aspect ratio:",
                .priv       = &crop_preset_ar_menu,
                .update     = &crop_preset_ar_update,
                .max        = 4,
                .choices    = CHOICES("16:9", "2:1", "2.20:1", "2.35:1", "2.39:1"),
                .help       = "Select aspect ratio for current preset.",
                .shidden    = 1,
            },
            {
                .name       = "Framerate:",
                .priv       = &crop_preset_fps_menu,
                .update     = &crop_preset_fps_update,
                .max        = 2,
                .choices    = CHOICES("23.976 FPS", "25 FPS", "30 FPS"),
                .help       = "Select framerate for current preset.",
                .shidden    = 1,
            },
            {
                .name       = "Reduced framerate HFR",
                .priv       = &crop_preset_fps_reduce,
                .max        = 1,
                .choices    = CHOICES("OFF", "Activated"),
                .help       = "Reduces framerates slightly for HFR presets",
                .shidden    = 1,
            },
            {
                .name       = "Bit-depth",
                .priv       = &bit_depth_analog,
                .update     = bit_depth_analog_update,
                .max        = 3,
                .choices    = CHOICES("14-bit", "12-bit","11-bit", "10-bit"),
                .help       = "Choose bit-depth for lossless RAW video compression.",
            },
            {            
                .name       = "Fix Dual-ISO flicker",
                .priv       = &fix_dual_iso_flicker,
                .update     = fix_dual_iso_flicker_update,
                .max        = 1,
                .choices    = CHOICES("OFF", "ON"),
                .icon_type  = IT_DICE,
                .help       = "Removes Dual-ISO lines waterfall effect by reducing FPS a tiny bit.",
                .help2      = "This fixes a flicker and crawling cuased by moving Dual-ISO lines."
            },
            {
                .name   = "Preview Debug 1",
                .priv   = &preview_debug_1,
                .max    = 0xFFFFFFF,
                .unit   = UNIT_HEX,
                .help   = "Preview Debug.",
                .advanced = 1,
            },
            {
                .name   = "Preview Debug 2",
                .priv   = &preview_debug_2,
                .max    = 0xFFFFFFF,
                .unit   = UNIT_HEX,
                .help   = "Preview Debug.",
                .advanced = 1,
            },
            {
                .name   = "Preview Debug 4",
                .priv   = &preview_debug_4,
                .max    = 5208,
                .unit   = UNIT_DEC,
                .help   = "Preview Debug.",
                .advanced = 1,
            },
            {
                .name   = "Preview Debug 3",
                .priv   = &preview_debug_3,
                .max    = 3478,
                .unit   = UNIT_DEC,
                .help   = "Preview Debug.",
                .advanced = 1,
            },
            {
                .name   = "CMOS 5 Debug",
                .priv   = &CMOS_5_Debug,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "CMOS 5 Debug.",
                .advanced = 1,
            },
            {
                .name   = "CMOS 7 Debug",
                .priv   = &CMOS_7_Debug,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "CMOS 7 Debug.",
                .advanced = 1,
            },
            {
                .name   = "TimerA Debug",
                .priv   = &TimerA_Debug,
                .min    = -10000,
                .max    = 10000,
                .unit   = UNIT_DEC,
                .help   = "TimerA Debug.",
                .advanced = 1,
            },
            {
                .name   = "TimerB Debug",
                .priv   = &TimerB_Debug,
                .min    = -10000,
                .max    = 10000,
                .unit   = UNIT_DEC,
                .help   = "TimerB Debug.",
                .advanced = 1,
            },
            {
                .name   = "RAW H Debug",
                .priv   = &RAW_H_Debug,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "RAW H Debug.",
                .advanced = 1,
            },
            {
                .name   = "RAW V Debug",
                .priv   = &RAW_V_Debug,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "RAW V Debug.",
                .advanced = 1,
            },
            {
                .name   = "YUV_HD_S_H_height",
                .priv   = &YUV_HD_S_H_height,
                .min    = -20000,
                .max    = 20000,
                .unit   = UNIT_DEC,
                .help  = "Alter height.",
                .advanced = 1,
            },
            {
                .name   = "YUV_HD_S_H_width",
                .priv   = &YUV_HD_S_H_width,
                .min    = -20000,
                .max    = 20000,
                .unit   = UNIT_DEC,
                .help  = "Alter width. Scrambles preview",
                .advanced = 1,
            },
            {
                .name   = "YUV_HD_S_V_height",
                .priv   = &YUV_HD_S_V_height,
                .min    = -20000,
                .max    = 20000,
                .unit   = UNIT_DEC,
                .help  = "height offset",
                .advanced = 1,
            },
            {
                .name   = "YUV_HD_S_V_width",
                .priv   = &YUV_HD_S_V_width,
                .min    = -20000,
                .max    = 20000,
                .unit   = UNIT_DEC,
                .help  = "width offset",
                .advanced = 1,
            },
            {
                .name   = "Target YRES",
                .priv   = &target_yres,
                .update = target_yres_update,
                .max    = 3870,
                .unit   = UNIT_DEC,
                .help   = "Desired vertical resolution (only for presets with higher resolution).",
                .help2  = "Decrease if you get corrupted frames (dial the desired resolution here).",
                .advanced = 1,
            },
            {
                .name   = "Delta ADTG 0",
                .priv   = &delta_adtg0,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help   = "ADTG 0x8178, 0x8196, 0x82F8",
                .help2  = "May help pushing the resolution a little. Start with small increments.",
                .advanced = 1,
            },
            {
                .name   = "Delta ADTG 1",
                .priv   = &delta_adtg1,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help   = "ADTG 0x8179, 0x8197, 0x82F9",
                .help2  = "May help pushing the resolution a little. Start with small increments.",
                .advanced = 1,
            },
            {
                .name   = "Delta HEAD3",
                .priv   = &delta_head3,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help2  = "May help pushing the resolution a little. Start with small increments.",
                .advanced = 1,
            },
            {
                .name   = "Delta HEAD4",
                .priv   = &delta_head4,
                .min    = -500,
                .max    = 500,
                .unit   = UNIT_DEC,
                .help2  = "May help pushing the resolution a little. Start with small increments.",
                .advanced = 1,
            },
            {
                .name   = "CMOS[1] lo",
                .priv   = &cmos1_lo,
                .max    = 63,
                .unit   = UNIT_DEC,
                .help   = "Start scanline (very rough). Use for vertical positioning.",
                .advanced = 1,
            },
            {
                .name   = "CMOS[1] hi",
                .priv   = &cmos1_hi,
                .max    = 63,
                .unit   = UNIT_DEC,
                .help   = "End scanline (very rough). Increase if white bar at bottom.",
                .help2  = "Decrease if you get strange colors as you move the camera.",
                .advanced = 1,
            },
            {
                .name   = "CMOS[2]",
                .priv   = &cmos2,
                .max    = 0xFFF,
                .unit   = UNIT_HEX,
                .help   = "Horizontal position / binning.",
                .help2  = "Use for horizontal centering.",
                .advanced = 1,
            },
            {
                .name   = "reg_skip_left",
                .priv   = &reg_skip_left,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "skip left",
                .advanced = 1,
            },
            {
                .name   = "reg_skip_right",
                .priv   = &reg_skip_right,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "skip right",
                .advanced = 1,
            },
            {
                .name   = "reg_skip_top",
                .priv   = &reg_skip_top,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "skip top",
                .advanced = 1,
            },
            {
                .name   = "reg_skip_bottom",
                .priv   = &reg_skip_bottom,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "skip bottom",
                .advanced = 1,
            },
            {
                .name   = "reg_cmos5",
                .priv   = &reg_cmos5,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "cmos5",
                .advanced = 1,
            },
            {
                .name   = "reg_cmos7",
                .priv   = &reg_cmos7,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "cmos7",
                .advanced = 1,
            },
            {
                .name   = "reg_height",
                .priv   = &reg_height,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "height",
                .advanced = 1,
            },
            {
                .name   = "reg_width",
                .priv   = &reg_width,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "width",
                .advanced = 1,
            },
            {
                .name   = "reg_Preview_H",
                .priv   = &reg_Preview_H,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "Preview_H",
                .advanced = 1,
            },
            {
                .name   = "reg_Preview_V",
                .priv   = &reg_Preview_V,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "Preview_V",
                .advanced = 1,
            },
            {
                .name   = "reg_YUV_HD_S_H",
                .priv   = &reg_YUV_HD_S_H,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "YUV_HD_S_H",
                .advanced = 1,
            },
            {
                .name   = "reg_YUV_HD_S_V",
                .priv   = &reg_YUV_HD_S_V,
                .min    = -1000,
                .max    = 1000,
                .unit   = UNIT_DEC,
                .help  = "YUV_HD_S_V",
                .advanced = 1,
            },
            {
                .name    = "Brighten LV method",
                .priv    = &brighten_lv_method,
                .max     = 1,
                .choices = CHOICES("AeWb", "EVF"),
                .help    = "Select a method to brighten LV when using negative analog gain.",
                .help2   = "Brighten LV using a regiser related to AeWb task. Stable, inaccurate AF.\n"
                           "Brighten LV using regisers related to EVF task. Less stable, accurate AF.",
                .advanced = 1,
            },
            MENU_ADVANCED_TOGGLE,
            MENU_EOL,
        },
    },
};

            static struct menu_entry movie_menu_bitdepth[] =
            {
                {
                    .name       = "Bit-depth",
                    .priv       = &bit_depth_analog,
                    .update     = bit_depth_analog_update,
                    .max        = 3,
                    .choices    = CHOICES("14-bit", "12-bit","11-bit", "10-bit"),
                    .help       = "Choose bit-depth for lossless RAW video compression.",
                },
            };

            static struct menu_entry movie_menu_shutter_range[] =
            {
                {
                    .name       = "Shutter range",
                    .priv       = &shutter_range,
                    .max        = 1,
                    .choices    = CHOICES("Original", "Full range"),
                    .help       = "Choose the available shutter speed range:",
                    .help2      = "Original: default range used by Canon in selected video mode.\n"
                                  "Full range: from 1/FPS to minimum exposure time allowed by hardware."
                },
            };

            static struct menu_entry movie_menu_framerate[] =
            {
                {
                    .name       = "Framerate:",
                    .priv       = &crop_preset_fps_menu,
                    .update     = &crop_preset_fps_update,
                    .max        = 2,
                    .choices    = CHOICES("23.976 FPS", "25 FPS", "30 FPS"),
                    .help       = "Select framerate for current preset.",
                    .shidden    = 1,
                },
            };

            static struct menu_entry movie_menu_ratio[] =
            {
                {
                    .name       = "Aspect ratio:",
                    .priv       = &crop_preset_ar_menu,
                    .update     = &crop_preset_ar_update,
                    .max        = 4,
                    .choices    = CHOICES("16:9", "2:1", "2.20:1", "2.35:1", "2.39:1"),
                    .help       = "Select aspect ratio for current preset.",
                    .shidden    = 1,
                },
            };

            static struct menu_entry movie_menu_fps[] =
            {
                {
                    .name   = "FPS modifier",
                    .priv   = &fps_over,
                    .min    = -100000,
                    .max    = 100000,
                    .unit   = UNIT_DEC,
                    .help   = "Increase, decrease fps. Will apply to Full-Res LV 1x3",
                },
            };


static MENU_UPDATE_FUNC(customize_buttons_update)
{
    if (!CROP_PRESET_MENU && !patch_active)
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "This setting only work with Crop mood.");
    }
}

static MENU_UPDATE_FUNC(Half_Shutter_update)
{
    if (Half_Shutter && !is_manual_focus())
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Half-Shutter assignment only works with manual focus.");
    }
}

static struct menu_entry customize_buttons_menu[] =
{
    {
        .name       = "Customize buttons",
        .select     = menu_open_submenu,
        .update     = customize_buttons_update,
        .depends_on = DEP_LIVEVIEW,
        .help       = "Assign some phyiscal camera buttons to do some tasks.",
        .help2      = "This setting might block some ML features from using assigned buttons.",
        .children   =  (struct menu_entry[]) {
            {
               .name     = "Half-Shutter",
               .max      = 3,
               .choices  = CHOICES("OFF", "Zoom x10", "Focus aid", "Sticky Zoom"),
               .update   = Half_Shutter_update,
               .priv     = &Half_Shutter,
               .help     = "Assign Half-Shutter button to a task.",
               .help2    = "Get close up focus while filming\n"
            },
            {
               .name     = "SET Button",
#ifdef CONFIG_SLIM_MENUS
               .max      = 7,
               .choices  = CHOICES("OFF", "Zoom x10", "ISO", "Aperture +", "Dual ISO", "False color", "Aperture Expo", "Shutter Expo"),
#else
               .max      = 8,
               .choices  = CHOICES("OFF", "Zoom x10", "ISO", "Aperture +", "Dual ISO", "False color", "Aperture Expo", "Shutter Expo", "ISO Expo"),
#endif
               .priv     = &SET_button,
               .help     = "Assign SET button to a task.",
            },
            {
                .name     = "INFO Button",
#ifdef CONFIG_SLIM_MENUS
                .max      = 7,
                .choices  = CHOICES("OFF", "Zoom x10", "ISO", "Aperture -", "Dual ISO", "False color", "Shutter Expo", "Aperture Expo"),
#else
                .max      = 8,
                .choices  = CHOICES("OFF", "Zoom x10", "ISO", "Aperture -", "Dual ISO", "False color", "Shutter Expo", "Aperture Expo", "ISO Expo"),
#endif
                .priv     = &INFO_button,
                .help     = "Assign INFO button to a task.",
            },
            {
                .name   = "Tap display",
                .priv   = &tapdisp,
#ifdef CONFIG_SLIM_MENUS
                .max    = 4,
                .choices = CHOICES("OFF", "Preset list", "Shutter Expo", "Aperture Expo", "False color"),
#else
                .max    = 5,
                .choices = CHOICES("OFF", "Preset list", "Shutter Expo", "Aperture Expo", "ISO Expo", "False color"),
#endif
                .help     = "Assign Tap display to a task.",
            },
            {
                .name     = "U/D Arrows",
                .max      = 2,
                .choices  = CHOICES("OFF", "ISO", "Aperture"),
                .priv     = &Arrows_U_D,
                .help     = "Assign Up and Down arrows to a task.",
            },
            {
                .name     = "More_hacks",
                .max      = 1,
                .choices  = CHOICES("OFF", "Allow"),
                .priv     = &more_hacks,
                .help     = "More hacks always allowed",
            },
            /* not working with eosm
            {
                .name     = "L/R Arrows",
                .max      = 2,
                .choices  = CHOICES("OFF", "ISO", "Aperture"),
                .priv     = &Arrows_L_R,
                .help     = "Assign Left and Right arrows to a task.",
            },
             */

            MENU_EOL,
        },
    }
};

static int settings_changed = 0;
static int crop_rec_needs_lv_refresh()
{
    if (!lv)
    {
        return 0;
    }

    if (CROP_PRESET_MENU)
    {
        if (is_supported_mode())
        {
            if (!patch_active || CROP_PRESET_MENU != crop_preset)
            {
                return 1;
            }

            /* Currently settings_changed is only supprted for EOS M */
            if (is_DIGIC_5)
            {
                if (settings_changed) return 1;
            }
        }
    }
    else /* crop disabled */
    {
        if (patch_active)
        {
            return 1;
        }
    }

    return 0;
}


/* variables for EOS M help to detect if settings changed */
static int old_ar_preset;
static int old_crop_preset_index;
static int old_fps_preset;
static int old_1x1_preset;
static int old_1x3_preset;
static int old_3x3_preset;
static int old_dual_iso;
static int old_diso_fix;
static int old_bit_depth;
static int old_crop_preset_fps_reduce;
static int old_fps_over;
static int old_shutter_range;

int check_if_settings_changed()
{
    if (old_crop_preset_index != crop_preset_index  ||
        old_ar_preset  != crop_preset_ar_menu       ||
        old_fps_preset != crop_preset_fps_menu      ||
        old_1x1_preset != crop_preset_1x1_res_menu  ||
        old_1x3_preset != crop_preset_1x3_res_menu  ||
        old_3x3_preset != crop_preset_3x3_res_menu  ||
        old_dual_iso   != dual_iso_is_enabled()     ||
        (old_bit_depth  != bit_depth_analog && Anam_FLV)         ||
        old_diso_fix   != fix_dual_iso_flicker      ||
        old_crop_preset_fps_reduce != crop_preset_fps_reduce ||
        old_fps_over != fps_over ||
        old_shutter_range != shutter_range)
    {
        return 1;
    }

    return 0;
}

#ifdef CONFIG_EOSM
static int crop_rec_lv_dirty = 1;

static int eosm_lv_guard_pipeline_ready(void)
{
    uint32_t display_buffer;

    if (!liveview_display_idle() || !CROP_PRESET_MENU || !patch_active)
        return 0;

    /* All EOS M movie crop presets use x5 for their stable preview path. */
    if (!is_movie_mode() || lv_dispsize != 5 || PathDriveMode->zoom != 5)
        return 0;

    /* Dimensions alone are not enough: early boot may still expose the
     * previous RAW geometry while Canon is replacing the actual buffers. */
    display_buffer = YUV422_LV_BUFFER_DISPLAY_ADDR;
    if (raw_info.width <= 0 || raw_info.height <= 0 || raw_info.pitch <= 0 ||
        !raw_info.buffer || !display_buffer)
        return 0;

    if (raw_info.width == eosm_lv_guard_width &&
        raw_info.height == eosm_lv_guard_height &&
        raw_info.pitch == eosm_lv_guard_pitch &&
        (uintptr_t)raw_info.buffer == eosm_lv_guard_raw_buffer &&
        display_buffer == eosm_lv_guard_display_buffer)
        eosm_lv_guard_stable_frames++;
    else
    {
        eosm_lv_guard_width = raw_info.width;
        eosm_lv_guard_height = raw_info.height;
        eosm_lv_guard_pitch = raw_info.pitch;
        eosm_lv_guard_raw_buffer = (uintptr_t)raw_info.buffer;
        eosm_lv_guard_display_buffer = display_buffer;
        eosm_lv_guard_stable_frames = 1;
        eosm_lv_guard_quiet_since = 0;
    }

    if (eosm_lv_guard_stable_frames < EOSM_LV_GUARD_STABLE_FRAMES)
        return 0;

    /* Canon often performs one final asynchronous write after raw geometry
     * becomes valid. Require a quiet buffer window before Crop Rec owns x5. */
    if (!eosm_lv_guard_quiet_since)
        eosm_lv_guard_quiet_since = get_ms_clock();

    return get_ms_clock() - eosm_lv_guard_quiet_since >= EOSM_LV_GUARD_QUIET_MS;
}

/* The buffer can be stable while Canon is still exposing the previous movie
 * layout (for example 2520x1080 at 29.97 fps before a 1x3 preset arrives).
 * Do not hand that transitional layout to the UI or recorder as a valid crop
 * frame.  The EOS M RAW buffer includes a small sensor margin around the
 * selected output, hence the deliberately narrow positive allowance. */
static int eosm_lv_guard_selected_geometry_ready(void)
{
    int expected_w, expected_h;

    /* Full-resolution LV is a separate 3 fps path, not a movie crop layout. */
    if (slim_mode_ui == 3 ||
        (CROP_PRESET_MENU == CROP_PRESET_1X1 && crop_preset_1x1_res_menu == 5))
        return 1;

    slim_crop_expected_res(&expected_w, &expected_h);
    if (expected_w <= 0 || expected_h <= 0)
        return 0;

    return raw_info.width  >= expected_w && raw_info.width  <= expected_w + 128 &&
           raw_info.height >= expected_h && raw_info.height <= expected_h + 64;
}

/* A valid RAW geometry does not guarantee that Canon restored the visible
 * YUV path. Sample a sparse center grid from the displayed UYVY buffer: this
 * is intentionally tiny and read-only, so it cannot disturb EDMAC or RAW
 * recording. Return false only for a uniformly video-black screen. */
static int eosm_lv_guard_display_luma_max(void)
{
    const uint8_t *vram;
    int x, y;
    int luma_max = 0;
    int width = vram_lv.width;
    int height = vram_lv.height;
    int pitch = vram_lv.pitch;
    static const uint8_t x_pos[] = { 2, 4, 6, 8 };
    static const uint8_t y_pos[] = { 3, 5, 7 };

    if (!YUV422_LV_BUFFER_DISPLAY_ADDR || width < 64 || height < 64 ||
        pitch < width * 2)
        return 255; /* unavailable data is handled by the geometry guard */

    vram = (const uint8_t *)UNCACHEABLE(YUV422_LV_BUFFER_DISPLAY_ADDR);
    for (y = 0; y < COUNT(y_pos); y++)
    {
        int py = height * y_pos[y] / 10;
        for (x = 0; x < COUNT(x_pos); x++)
        {
            int px = width * x_pos[x] / 10;
            /* UYVY: luma is the second byte of each two-byte pixel. */
            luma_max = MAX(luma_max, vram[py * pitch + px * 2 + 1]);
        }
    }

    return luma_max;
}

static int eosm_lv_guard_display_has_content(void)
{
    return eosm_lv_guard_display_luma_max() > 20;
}

/* These are the display-route values already supplied by the Crop Rec ENGIO
 * hook. Check only the final scaler/buffer-format registers; sensor timing,
 * RAW geometry and EDMAC routing are deliberately outside this recovery. */
static int eosm_lv_guard_display_route_ready(void)
{
    if (!Preview_Control || !YUV_LV_Buf)
        return 1;

    return shamem_read(0xC0F11B8C) == YUV_HD_S_H &&
           shamem_read(0xC0F11BCC) == YUV_HD_S_V &&
           shamem_read(0xC0F11BC8) == YUV_HD_S_V_E &&
           shamem_read(0xC0F11ACC) == YUV_LV_S_V &&
           shamem_read(0xC0F04210) == YUV_LV_Buf;
}

static void eosm_lv_guard_reapply_display_route(void)
{
    EngDrvOutLV(0xC0F11B8C, YUV_HD_S_H);
    EngDrvOutLV(0xC0F11BCC, YUV_HD_S_V);
    EngDrvOutLV(0xC0F11BC8, YUV_HD_S_V_E);
    EngDrvOutLV(0xC0F11ACC, YUV_LV_S_V);
    EngDrvOutLV(0xC0F04210, YUV_LV_Buf);
}

/* Exported for LVRECOV.LOG. The signature changes only when the transition
 * controller changes state, so the recorder log stays event-only. */
__attribute__((used, noinline))
int crop_rec_lv_transition_diag(char *buffer, int size)
{
    int luma = eosm_lv_guard_display_luma_max();
    int route_ok = eosm_lv_guard_display_route_ready();
    int signature = (eosm_lv_guard_pending ? 1 : 0) |
        (eosm_lv_guard_busy ? 2 : 0) |
        (eosm_lv_guard_state << 2) |
        (eosm_lv_guard_content_dark_frames << 6) |
        (eosm_lv_guard_content_retries << 10) |
        (eosm_lv_guard_route_retries << 12) |
        (route_ok ? 1 << 14 : 0);

    if (buffer && size > 0)
        snprintf(buffer, size,
            "guard=%d/%d/%d luma=%d dark=%d contentfix=%d route=%d routefix=%d",
            eosm_lv_guard_pending, eosm_lv_guard_busy, eosm_lv_guard_state,
            luma, eosm_lv_guard_content_dark_frames,
            eosm_lv_guard_content_retries, route_ok, eosm_lv_guard_route_retries);

    return signature;
}

static void eosm_lv_guard_clear(void)
{
    eosm_lv_guard_pending = 0;
    eosm_lv_guard_busy = 0;
    crop_rec_lv_dirty = 0;
    settings_changed = 0;
}

static int eosm_lv_guard_step(int menu_shown, int mlv_busy)
{
    int now;

    if (!eosm_lv_guard_pending)
        return 0;

    if (!CROP_PRESET_MENU || !is_movie_mode())
    {
        eosm_lv_guard_pending = 0;
        eosm_lv_guard_busy = 0;
        return 0;
    }

    if (!lv || menu_shown || RECORDING_RAW || mlv_busy)
        return 1;

    /* x10 is Canon's focusing view, not the custom x5 preview.  Do not hold
     * the guard open there; exiting x10 requests a fresh normal transition. */
    if (lv_dispsize == 10)
    {
        eosm_lv_guard_pending = 0;
        eosm_lv_guard_busy = 0;
        return 0;
    }

    now = get_ms_clock();
    if (!eosm_lv_guard_started)
        eosm_lv_guard_started = now;

    eosm_lv_guard_busy = 1;

    switch (eosm_lv_guard_state)
    {
        case EOSM_LV_GUARD_WAIT:
            /* Give Canon most of the 0.5-second window, then require three
             * matching RAW frames before touching the custom preview regs. */
            if (now - eosm_lv_guard_started < EOSM_LV_GUARD_SETTLE_MS)
                return 1;
            /* Canon often returns to x1 after boot or a Canon menu.  Restore
             * the crop module's normal x5 preview before validating frames. */
            if (lv_dispsize == 1)
            {
                eosm_lv_guard_set_zoom(5);
                eosm_lv_guard_stable_frames = 0;
                eosm_lv_guard_quiet_since = 0;
                return 1;
            }
            if (!eosm_lv_guard_pipeline_ready())
                return 1;

            /* Always rebuild x5 in a controlled way. This makes menu exits,
             * record-stop and boot use exactly the same known-good path,
             * instead of trusting whatever preview state Canon left behind. */
            eosm_lv_guard_set_zoom(1);
            eosm_lv_guard_state = EOSM_LV_GUARD_APPLY_X1;
            eosm_lv_guard_started = now;
            eosm_lv_guard_stable_frames = 0;
            eosm_lv_guard_quiet_since = 0;
            return 1;

        case EOSM_LV_GUARD_APPLY_X1:
            if (lv_dispsize != 1)
                return 1;
            if (now - eosm_lv_guard_started < 80)
                return 1;
            eosm_lv_guard_set_zoom(5);
            eosm_lv_guard_state = EOSM_LV_GUARD_APPLY_X5;
            eosm_lv_guard_started = now;
            eosm_lv_guard_stable_frames = 0;
            eosm_lv_guard_quiet_since = 0;
            return 1;

        case EOSM_LV_GUARD_APPLY_X5:
            if (now - eosm_lv_guard_started < EOSM_LV_GUARD_SETTLE_MS ||
                !eosm_lv_guard_pipeline_ready())
                return 1;
            eosm_lv_guard_state = EOSM_LV_GUARD_VALIDATE;
            eosm_lv_guard_started = now;
            return 1;

        case EOSM_LV_GUARD_VALIDATE:
            /* Verify x5 remains quiet after Canon has had time to consume the
             * final zoom request. A quiet buffer alone is not enough: Canon
             * may still be serving the old preset's geometry here. */
            if (!eosm_lv_guard_pipeline_ready() ||
                now - eosm_lv_guard_started < EOSM_LV_GUARD_QUIET_MS)
                return 1;
            if (eosm_lv_guard_selected_geometry_ready() &&
                !crop_rec_needs_lv_refresh())
            {
                /* Geometry is correct. Confirm the LCD path has delivered a
                 * real frame before releasing the transition controller. */
                eosm_lv_guard_state = EOSM_LV_GUARD_CONTENT;
                eosm_lv_guard_started = now;
                eosm_lv_guard_content_dark_frames = 0;
                return 1;
            }

            /* Rebuild the selected x5 path again when Canon retained a stable
             * but wrong geometry. This is the automatic equivalent of the
             * manual zoom-out/zoom-in recovery, without ever exposing the
             * temporary resolution as a yellow exclamation mark. */
            if (eosm_lv_guard_retries >= EOSM_LV_GUARD_MAX_RECOVERIES)
            {
                /* Keep waiting for Canon instead of declaring the wrong preset
                 * ready. A later LV property update restarts this guard. */
                eosm_lv_guard_state = EOSM_LV_GUARD_WAIT;
                eosm_lv_guard_started = now;
                eosm_lv_guard_stable_frames = 0;
                eosm_lv_guard_quiet_since = 0;
                eosm_lv_guard_retries = 0;
                return 1;
            }

            /* A failed validation gets a controlled x1 -> x5 rebuild.
             * Do not use x10: it is a focus-only Canon path. */
            eosm_lv_guard_set_zoom(1);
            eosm_lv_guard_state = EOSM_LV_GUARD_RECOVER_X1;
            eosm_lv_guard_started = now;
            eosm_lv_guard_stable_frames = 0;
            eosm_lv_guard_quiet_since = 0;
            eosm_lv_guard_retries++;
            return 1;

        case EOSM_LV_GUARD_CONTENT:
            if (now - eosm_lv_guard_started < EOSM_LV_GUARD_QUIET_MS)
                return 1;

            if (eosm_lv_guard_display_has_content())
            {
                eosm_lv_guard_state = EOSM_LV_GUARD_ROUTE;
                eosm_lv_guard_started = now;
                return 1;
            }

            /* A genuine dark scene is possible. Require consecutive samples,
             * then make only one extra recovery attempt and accept a persistently
             * black scene afterward rather than trapping the user in a loop. */
            if (++eosm_lv_guard_content_dark_frames < EOSM_LV_GUARD_CONTENT_SAMPLES)
                return 1;

            if (eosm_lv_guard_content_retries++ == 0)
            {
                eosm_lv_guard_set_zoom(1);
                eosm_lv_guard_state = EOSM_LV_GUARD_RECOVER_X1;
                eosm_lv_guard_started = now;
                eosm_lv_guard_stable_frames = 0;
                eosm_lv_guard_quiet_since = 0;
                eosm_lv_guard_retries++;
                return 1;
            }

            eosm_lv_guard_clear();
            return 0;

        case EOSM_LV_GUARD_ROUTE:
            if (now - eosm_lv_guard_started < EOSM_LV_GUARD_QUIET_MS)
                return 1;

            if (eosm_lv_guard_display_route_ready())
            {
                eosm_lv_guard_clear();
                return 0;
            }

            /* Canon may overwrite its final display-route values after the
             * frame itself is ready. Restore only the five existing Crop Rec
             * route values once, then leave Canon in control if it disagrees. */
            if (eosm_lv_guard_route_retries++ == 0)
            {
                eosm_lv_guard_reapply_display_route();
                eosm_lv_guard_started = now;
                return 1;
            }

            eosm_lv_guard_clear();
            return 0;

        case EOSM_LV_GUARD_RECOVER_X1:
            if (lv_dispsize != 1)
            {
                /* Canon has not acknowledged x1 yet; keep waiting, but do
                 * not issue more property writes into the same transition. */
                return 1;
            }
            if (now - eosm_lv_guard_started < 80)
                return 1;
            eosm_lv_guard_set_zoom(5);
            eosm_lv_guard_state = EOSM_LV_GUARD_RECOVER_X5;
            eosm_lv_guard_started = now;
            eosm_lv_guard_stable_frames = 0;
            eosm_lv_guard_quiet_since = 0;
            return 1;

        case EOSM_LV_GUARD_RECOVER_X5:
            if (now - eosm_lv_guard_started < EOSM_LV_GUARD_SETTLE_MS ||
                !eosm_lv_guard_pipeline_ready())
                return 1;
            eosm_lv_guard_state = EOSM_LV_GUARD_VALIDATE;
            eosm_lv_guard_started = now;
            return 1;
    }

    eosm_lv_guard_request();
    return 1;
}
#endif

/* ------------------------------------------------------------------------
 * Record-stop Live View check
 *
 * After a recording stops, mlv_lite resumes Live View.  It used to follow that
 * with an unconditional x5 -> x1 -> x5 zoom bounce, which makes the screen go
 * dark and show half-built frames for about a second, every time.  Instead,
 * mlv_lite now just asks for a check here; we look at the real display state
 * a moment later and only bounce the zoom if Live View did not come back
 * healthy.  A failed or doubtful check does exactly what the old code did.
 * ---------------------------------------------------------------------- */
#ifndef YUV422_LV_BUFFER_DISPLAY_ADDR
/* EOS M 2.0.2 (modules are built without the platform consts.h) */
#define YUV422_LV_BUFFER_DISPLAY_ADDR (*(uint32_t*)(0x3E650+0x118))
#endif

#define LV_CHECK_DELAY_MS 450      /* let Canon finish resuming Live View */
#define LV_CHECK_GIVEUP_MS 6000    /* never keep a request around longer than this */

static volatile int lv_check_pending = 0;
static volatile int lv_check_since = 0;

/* called by mlv_lite once a recording has fully stopped */
void crop_rec_request_lv_check(void)
{
    lv_check_since = get_ms_clock();
    lv_check_pending = 1;
}

/* highest luma in a sparse grid of the displayed frame (read-only) */
static int lv_check_display_luma_max(void)
{
    const uint8_t *vram;
    int x, y;
    int luma_max = 0;
    int width = vram_lv.width;
    int height = vram_lv.height;
    int pitch = vram_lv.pitch;
    static const uint8_t x_pos[] = { 2, 4, 6, 8 };
    static const uint8_t y_pos[] = { 3, 5, 7 };

    if (!YUV422_LV_BUFFER_DISPLAY_ADDR || width < 64 || height < 64 ||
        pitch < width * 2)
        return 0; /* no usable data: treat as not healthy */

    vram = (const uint8_t *)UNCACHEABLE(YUV422_LV_BUFFER_DISPLAY_ADDR);
    for (y = 0; y < COUNT(y_pos); y++)
    {
        int py = height * y_pos[y] / 10;
        for (x = 0; x < COUNT(x_pos); x++)
        {
            int px = width * x_pos[x] / 10;
            /* UYVY: luma is the second byte of each two-byte pixel */
            luma_max = MAX(luma_max, vram[py * pitch + px * 2 + 1]);
        }
    }
    return luma_max;
}

/* are the display-route registers set the way our x5 preset expects? */
static int lv_check_route_ok(void)
{
    if (!Preview_Control || !YUV_LV_Buf)
        return 1;

    return shamem_read(0xC0F11B8C) == YUV_HD_S_H &&
           shamem_read(0xC0F11BCC) == YUV_HD_S_V &&
           shamem_read(0xC0F11BC8) == YUV_HD_S_V_E &&
           shamem_read(0xC0F11ACC) == YUV_LV_S_V &&
           shamem_read(0xC0F04210) == YUV_LV_Buf;
}

/* Returns 1 if Live View looks fine and the zoom bounce can be skipped. */
static int lv_check_is_healthy(void)
{
    if (lv_dispsize != 5 || !patch_active || !CROP_PRESET_MENU || !is_movie_mode())
        return 0;
    if (PathDriveMode->zoom != 5)
        return 0;
    if (!lv_check_route_ok())
        return 0;
    if (lv_check_display_luma_max() <= 20)
        return 0;
    return 1;
}

/* when closing ML menu, check whether we need to refresh the LiveView */
static unsigned int crop_rec_polling_cbr(unsigned int unused)
{
    
    /* touch/Movie-tab shortcuts inject Q+SET into an open menu — not used on slim. */
#ifndef CONFIG_SLIM_MENUS
    if (gui_menu_shown() && submenu && !RECORDING)
    {
        if (is_movie_mode())
        {
            module_send_keypress(MODULE_KEY_Q);
        }
        module_send_keypress(MODULE_KEY_PRESS_SET);
        submenu = 0;
    }
#endif
#ifdef CONFIG_EOSM
    /* crop_rec_lv_dirty is module-level; also checked at startup */
#else
    /* also check at startup */
    static int lv_dirty = 1;
#endif

#ifdef CONFIG_EOSM
    int mlv_busy = mlv_raw_rec_busy();
    static int eosm_lv_was_active = 0;
    static int eosm_recording_was_active = 0;
    static int eosm_display_mode = -1;
#else
    int mlv_busy = 0;
#endif

    int menu_shown = gui_menu_shown();
#ifdef CONFIG_EOSM
    /* Cover every path back into Movie Live View, including Canon menus
     * (which may not set gui_menu_shown), recording stop and boot. */
    if ((lv && !eosm_lv_was_active) ||
        (eosm_recording_was_active && !RECORDING) ||
        (eosm_display_mode != -1 && eosm_display_mode != lv_disp_mode))
        eosm_lv_guard_request();
    eosm_lv_was_active = lv;
    eosm_recording_was_active = RECORDING;
    eosm_display_mode = lv_disp_mode;

    static int crop_rec_menu_was_shown = 0;
    if (lv && menu_shown)
        crop_rec_menu_was_shown = 1;
    else if (crop_rec_menu_was_shown && lv && !menu_shown)
    {
        crop_rec_lv_dirty = 1;
        crop_rec_menu_was_shown = 0;
    }
#else
    if (lv && menu_shown)
    {
        lv_dirty = 1;
    }
#endif
    
    if (!lv || menu_shown || RECORDING_RAW || mlv_busy)
    {
        /* outside LV: no need to do anything */
        /* don't change while browsing the menu, but shortly after closing it */
        /* don't change while recording raw, or while mlv_lite is starting/stopping */
        return CBR_RET_CONTINUE;
    }

#ifndef CONFIG_EOSM
    if (lv_check_pending)
    {
        int waited = get_ms_clock() - lv_check_since;

        if (waited >= LV_CHECK_GIVEUP_MS)
        {
            lv_check_pending = 0;
        }
        else if (waited >= LV_CHECK_DELAY_MS)
        {
            lv_check_pending = 0;

            if (!lv_check_is_healthy() && lv_dispsize == 5)
            {
                /* same recovery the old record-stop code ran every time */
                info_led_on();
                gui_uilock(UILOCK_EVERYTHING);
                set_zoom(1);
                set_zoom(5);
                gui_uilock(UILOCK_NONE);
                info_led_off();
            }
        }
    }
#endif

#ifdef CONFIG_EOSM
    if (eosm_lv_guard_step(menu_shown, mlv_busy))
        return CBR_RET_CONTINUE;
#endif
    
    /* check if any of our settings are changed */
    /* for EOS M */
    if (check_if_settings_changed())
    {
#ifdef CONFIG_EOSM
        crop_rec_lv_dirty = 1;
#else
        lv_dirty = 1;
#endif
        settings_changed = 1;
    }

#ifdef CONFIG_EOSM
    if (crop_rec_lv_dirty)
#else
    if (lv_dirty)
#endif
    {
        int needs_refresh = crop_rec_needs_lv_refresh();
        /* do we need to refresh LiveView? */
            if (needs_refresh)
            {
                /* let's check this once again, just in case */
                /* (possible race condition that would result in unnecessary refresh) */
#ifdef CONFIG_EOSM
                /* CheckPreviewRegsValuesAndForce is intentionally a no-op
                 * on EOS M, so avoid an unnecessary delayed polling wait. */
#else
                wait_lv_frames(2);
                if (crop_rec_needs_lv_refresh())
                {
                    info_led_on();
                    gui_uilock(UILOCK_EVERYTHING);
                    int old_zoom = lv_dispsize;
                    set_zoom(lv_dispsize == 1 ? 5 : 1);
                    set_zoom(old_zoom);
                    gui_uilock(UILOCK_NONE);
                    info_led_off();
                }
#endif
            }
#ifdef CONFIG_EOSM
        crop_rec_lv_dirty = 0;
#else
        lv_dirty = 0;
#endif
        settings_changed = 0;
    }



    /* EOS M preferences */
    if (is_DIGIC_5 && lv)
    {
        // all of our presets work in x5 mode because of preview, even none-cropped ones
        if (CROP_PRESET_MENU && !RECORDING && is_movie_mode()) 
        {
            // WB value will change in ML, but won't be applied until we refresh LV manually, that's because
            // of setting LV zoom to x5 zoom directly after we enter LV, this delay helps to avoid this issue
            /* Let´s keep things snappy. Removing this for now.
            if (lv_dispsize == 1)
            {
                gui_uilock(UILOCK_EVERYTHING);
                msleep(1100); 
                gui_uilock(UILOCK_NONE);
            }
             */
             
            {
                static int eosm_af_multi_set = 0;
                static int eosm_af_single_set = 0;
                if (CROP_PRESET_MENU != CROP_PRESET_3X3)
                {
                    eosm_af_multi_set = 0;
                    eosm_af_single_set = 0;
                }

            if (is_manual_focus())
            {
                /* while we are using manual focus and 3x3 presets change AF method to FlexiZone - Multi 
                 * this way preview will always work in 3x3 presets, also 738p HFR preset will have working preview while idle */
                if (lv_af_mode == 1 && CROP_PRESET_MENU == CROP_PRESET_3X3 && !eosm_af_multi_set)
                {
                    gui_uilock(UILOCK_EVERYTHING);
                    set_lv_af_mode(3); // Set it to FlexiZone - Multi
                    gui_uilock(UILOCK_NONE);
                    eosm_af_multi_set = 1;
                    NotifyBox(2500,"AF mode was set to FlexiZone Multi");
                }
                else
                {
                    if (lv_dispsize == 1) set_zoom(5);
                }
            }

            // our presets works only in x5 mode (when lv_af_mode set to "Tracking", we can't enter x5 mode anymore)
            // "Multi" uses x1 mode while focusing and it does work, but won't work with 100D due to crash mentioned in 
            // is_supported_mode(), "Single" appears to be the best in our case beside we can use it in x10 for focusing.
            // well, "Single" can also do focusing in x5 mode, but since we are modifying preview, autofocus in x5 mode
            // won't give accurate results, it seems modifying preiew break AF data
            // FlexiZone - Single = 1, Tracking = 2, FlexiZone - Multi = 3
            if (!is_manual_focus() && lv_af_mode != 1 && !eosm_af_single_set) // AF mode not set to FlexiZone - Single
            {
                gui_uilock(UILOCK_EVERYTHING);
                set_lv_af_mode(1); // Set it to FlexiZone - Single
                gui_uilock(UILOCK_NONE);
                eosm_af_single_set = 1;
                NotifyBox(2500,"AF mode was set to FlexiZone Single");
            }

            if (!is_manual_focus() && lv_af_mode == 1)
            {
                if (lv_dispsize == 1) set_zoom(5);
            }
            }
        }

        /* while idle, check our preview resgisters, force the new values if not set yet */
#ifndef CONFIG_EOSM
        if (!lv_dirty && !crop_rec_needs_lv_refresh() && CROP_PRESET_MENU && !RECORDING && lv_dispsize == 5 && PathDriveMode->zoom == 5)
        {
            if (Preview_Control && !Preview_Control_Basic) // presets with basic preview don't need it
            {
                CheckPreviewRegsValuesAndForce();
            }
        }
#endif

        // FIXME: for now, "More" hacks must be on in order to get wokring preview in 3x3 presets while recording
        // see notes in reg_override_3X3
        {
            static int slim_3x3_more_hacks_done = 0;
            if (CROP_PRESET_MENU != CROP_PRESET_3X3)
                slim_3x3_more_hacks_done = 0;
            else if (raw_lv_is_enabled() && !is_more_hacks_selected() && crop_preset_3x3_res_menu != 1
                     && !slim_3x3_more_hacks_done)
            {
                if (is_EOSM)
                    mlv_lite_set_small_hacks_more();
                else
                    menu_set_str_value_from_script("RAW video", "Small hacks", "More", 2);
                slim_3x3_more_hacks_done = 1;
                NotifyBox(2000, "Small Hacks set to More");
            }
        }

        /* disable Canon overlays in x5 mode for cleaner preview */
        extern int kill_canon_gui_mode;
        if (lv && patch_active && CROP_PRESET_MENU)
        {
            if (PathDriveMode->zoom == 5 && kill_canon_gui_mode != 1)
            {
                kill_canon_gui_mode = 1;
            }

            /* enable Canon overlays in x10 mode */
            if (PathDriveMode->zoom == 10 && kill_canon_gui_mode != 0)
            {
                kill_canon_gui_mode = 0;
                if (canon_gui_front_buffer_disabled())
                {
                    canon_gui_enable_front_buffer(0);
                    redraw();
                }
            }
        }

        /* on entry-level models, setting picture quality to RAW from Canon menu gains extra SRM chunk 
         * https://www.magiclantern.fm/forum/index.php?topic=26521.msg239231#msg239231 (+31 MB of RAM)  
         * let's check picture quality on startup, also when the user change it to other than RAW          
         * let's inform the user to change pic quality back to RAW, and a camera restart would required 
         * this extends recording times at high resolutions, also allows to record Full-Res LV @ 2 FPS */
        if (patch_active && CROP_PRESET_MENU && is_movie_mode())
        {
            if (pic_quality != 0x4060000)
            {
                pic_quality_warning = 1;
            }
        }

        if (!menu_shown)
        {
            // check crop_rec configurations while outside ML menu
            old_crop_preset_index = crop_preset_index;
            old_ar_preset  = crop_preset_ar_menu;
            old_fps_preset = crop_preset_fps_menu;
            old_1x1_preset = crop_preset_1x1_res_menu;
            old_1x3_preset = crop_preset_1x3_res_menu;
            old_3x3_preset = crop_preset_3x3_res_menu;
            old_dual_iso   = dual_iso_is_enabled();
            old_diso_fix   = fix_dual_iso_flicker;
            old_crop_preset_fps_reduce = crop_preset_fps_reduce;
            old_fps_over = fps_over;
            old_shutter_range = shutter_range;
            if (Anam_FLV)
            {
                old_bit_depth  = bit_depth_analog;
            }
        }
    }

    return CBR_RET_CONTINUE;
}

    /* customize buttons and buttons shortcuts, FIXME: implement these as feature in ML core? */
static unsigned int crop_rec_keypress_cbr(unsigned int key)
{
    extern int kill_canon_gui_mode;

#ifdef CONFIG_EOSM
    /* The transition controller owns Live View until its post-x5 validation
     * passes. Swallow only controls that can alter preview state; REC and
     * MENU remain available for normal camera safety and escape behavior. */
    if (eosm_lv_guard_busy && lv && !RECORDING &&
        (key == MODULE_KEY_TOUCH_1_FINGER ||
         key == MODULE_KEY_PRESS_SET ||
         key == MODULE_KEY_PRESS_UP || key == MODULE_KEY_PRESS_DOWN ||
         key == MODULE_KEY_PRESS_LEFT || key == MODULE_KEY_PRESS_RIGHT ||
         key == MODULE_KEY_WHEEL_LEFT || key == MODULE_KEY_WHEEL_RIGHT ||
         key == MODULE_KEY_INFO))
        return 0;
#endif

    /* Close the touch editor at the REC press itself, before the asynchronous
     * RAW/H.264 recording state flag changes.  gui-common blocks every touch
     * once RECORDING is set, regardless of Global Draw. */
    if (is_EOSM && key == MODULE_KEY_REC && lvinfo_touch_editor_is_open())
        lvinfo_touch_editor_close();

    /* EOS M Live View taps are routed by gui-common.c (Quick Panel, grid,
     * and Last Settings). Do not let the legacy crop.tapdisp shortcuts race
     * that router during boot or Canon INFO transitions. */
    if (is_EOSM && key == MODULE_KEY_TOUCH_1_FINGER)
        return 1;

    /* Recording: touch is blocked in gui-common (idle LV touch is allowed). */

    //Reset zoom when stopping recording
    
    //Prevent black screen?
    if (key == MODULE_KEY_REC && RECORDING)
    {
        set_zoom(1);
    }
    
    //Focus aid function
    if (Half_Shutter == 2 && RECORDING && (crop_preset == CROP_PRESET_3X3 || crop_preset == CROP_PRESET_1X1))
    {
        if (key == MODULE_KEY_REC && RECORDING)
        {
            zoom = 0;
        }
        
        //Resets the preview zoom when releasing halfshutter. Better for when using autofocus
        if (!get_halfshutter_pressed() && zoom)
        {
            zoom = 0;
            if (crop_preset == CROP_PRESET_3X3)
            {
                EngDrvOutLV(0xc0f11A88, 0x0);
            }
            CheckPreviewRegsValuesAndForce();
        }
        
        if (get_halfshutter_pressed() && !gui_menu_shown() && lv && is_movie_mode() && !zoom)
        {
            zoom = 1;
            EngDrvOutLV(0xc0f11B8C, 0x0);
            EngDrvOutLV(0xc0f11BCC, 0x0);
            EngDrvOutLV(0xc0f11BC8, 0x0);
            if (crop_preset == CROP_PRESET_1X3)
            {
                EngDrvOutLV(0xc0f11A88, 0x1);
            }
        }
    }
    
    //Focus aid function
    if (Half_Shutter == 2 && RECORDING && crop_preset == CROP_PRESET_1X3)
    {
        if (key == MODULE_KEY_REC && RECORDING)
        {
            zoom = 0;
        }
        //Resets the preview zoom when releasing halfshutter. Better for when using autofocus
        if (!get_halfshutter_pressed() && zoom)
        {
            zoom = 0;
            if (crop_preset == CROP_PRESET_1X3)
            {
                EngDrvOutLV(0xc0f11A88, 0x0);
                CheckPreviewRegsValuesAndForce();
            }
        }
        
        if (get_halfshutter_pressed() && !gui_menu_shown() && lv && is_movie_mode() && !zoom)
        {
            zoom = 1;

            if (Anam_FLV)
            {
                EngDrvOutLV(0xc0f11A88, 0x1);
                YUV_HD_S_H    = 0x10501B5 + YUV_HD_S_H_width - (90 << 16);
                YUV_HD_S_V    = 0x45015C + 800 + (1000 << 16);
                CheckPreviewRegsValuesAndForce();
            }
            else if (Anam_Higher || Anam_Highest)
            {
                EngDrvOutLV(0xc0f11A88, 0x1);
                YUV_HD_S_H    = 0x105015B + 4000 + (6000 << 16);
                YUV_HD_S_V    = 0x105036D + YUV_HD_S_V_width + (20000 << 16);
                CheckPreviewRegsValuesAndForce();
            }
            else
            {
                EngDrvOutLV(0xc0f11B8C, 0x0);
                EngDrvOutLV(0xc0f11BCC, 0x0);
                EngDrvOutLV(0xc0f11BC8, 0x0);
                if (crop_preset == CROP_PRESET_1X3)
                {
                    EngDrvOutLV(0xc0f11A88, 0x1);
                }
            }
            
        }
    }
    
    /* we need to use customize buttons in LiveView while ML isn't showing and when using Crop mood */
    if (lv && !gui_menu_shown())
    {
        /* EOS M slim: INFO Button mapping must work even before patch_active settles. */
        if ((is_EOSM && is_movie_mode()) || (CROP_PRESET_MENU && patch_active))
        {
            if (slim_handle_main_dial_shutter(key))
                return 0;

            if (slim_handle_shutter_zoom(key))
                return 0;

            if (slim_handle_set_button(key))
                return 0;

            {
                int info = slim_handle_info_button(key);
                if (info == 1) return 0;
                if (info == -1) return 1;
            }

            /* Quick x10 mode */
            if (lv_dispsize == 5 && !RECORDING)
            {
                if (((key == MODULE_KEY_PRESS_SET         ) && SET_button  == 1)                 ||
                    ((key == MODULE_KEY_INFO              ) && !is_EOSM && INFO_button == 1)     ||
                    (!is_EOSM && (key == MODULE_KEY_PRESS_HALFSHUTTER ) && Half_Shutter && is_manual_focus()) )
                {
                    if(lv_disp_mode != 0){
                        // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                        return 1;
                    }
                    set_zoom(10);

                    /* Enable Canon overlays in x10 mode */
                    kill_canon_gui_mode = 0;
                    if (canon_gui_front_buffer_disabled())
                    {
                            canon_gui_enable_front_buffer(0);
                    }
                    return 0;
                }
            }

            /* Finished from x10 mode? Let's get back to normal preview */
            if (lv_dispsize == 10 && !RECORDING)
            {
                
                if (((key == MODULE_KEY_PRESS_SET           ) && SET_button  == 1)                 ||
                    ((key == MODULE_KEY_INFO                ) && !is_EOSM && INFO_button == 1)     ||
                    (!is_EOSM && (key == MODULE_KEY_UNPRESS_HALFSHUTTER ) && Half_Shutter != 3 && is_manual_focus()) ||
                    (!is_EOSM && (key == MODULE_KEY_PRESS_HALFSHUTTER ) && Half_Shutter == 3 && is_manual_focus()) )
                {
                    set_zoom(1); // Get to x1 first, sometime we get black preview when going x10 --> x5
                    msleep(50);
                    set_zoom(5);

                    /* Disable Canon overlays in x5 mode */
                    kill_canon_gui_mode = 1;
                    return 0;
                }
            }

            /* EOS M idle movie LV + ML overlays: Up/Down Settings remaps only. */
            if (is_EOSM && !RECORDING && lv_dispsize != 10 && lv_disp_mode == 0)
            {
                if (key == MODULE_KEY_PRESS_UP && slim_handle_arrow_adjust(Arrows_U_D, 1))
                    return 0;
                if (key == MODULE_KEY_PRESS_DOWN && slim_handle_arrow_adjust(Arrows_U_D, -1))
                    return 0;
            }

            if (lv_dispsize == 5)
            {
                /* Non-EOSM (or during REC): legacy U/D L/R shortcuts */
                if (!is_EOSM || RECORDING)
                {
                    if (key == MODULE_KEY_PRESS_UP && slim_handle_arrow_adjust(Arrows_U_D, 1))
                        return 0;
                    if (key == MODULE_KEY_PRESS_DOWN && slim_handle_arrow_adjust(Arrows_U_D, -1))
                        return 0;
                    if (key == MODULE_KEY_PRESS_RIGHT && slim_handle_arrow_adjust(Arrows_L_R, 1))
                        return 0;
                    if (key == MODULE_KEY_PRESS_LEFT && slim_handle_arrow_adjust(Arrows_L_R, -1))
                        return 0;
                }

                if (((key == MODULE_KEY_INFO)       && !is_EOSM && INFO_button == 2) ||
                    ((key == MODULE_KEY_PRESS_SET)  && SET_button  == 2))
                {
                    if(lv_disp_mode != 0){
                        // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                        return 1;
                    }
                    
                    if (more_hacks && RECORDING) return 0;
                    crop_rec_adjust_iso(2);
                    return 0;
                }

                if (key == MODULE_KEY_INFO && !is_EOSM && INFO_button == 3)
                {
                    if(lv_disp_mode != 0){
                        // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                        return 1;
                    }
                    if (more_hacks && RECORDING) return 0;
                    aperture_toggle(0, -1);
                    return 0;
                }
                if (key == MODULE_KEY_PRESS_SET && SET_button == 3)
                {
                    if (more_hacks && RECORDING) return 0;
                    aperture_toggle(0, 1);
                    return 0;
                }

                /* Dual ISO ON / OFF */
                if (((key == MODULE_KEY_INFO)       && !is_EOSM && INFO_button == 4) ||
                    ((key == MODULE_KEY_PRESS_SET)  && SET_button == 4))
                {
                    if(lv_disp_mode != 0){
                        // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                        return 1;
                    }
                    
                    if (!RECORDING)
                    {
                        slim_toggle_dual_iso();
                        return 0;
                    }
                }

                /* False color ON / OFF */
                if (((key == MODULE_KEY_INFO)       && !is_EOSM && INFO_button == 5) ||
                    ((key == MODULE_KEY_PRESS_SET)  && SET_button  == 5))
                {
                    if(lv_disp_mode != 0){
                        // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                        return 1;
                    }
                    
                    extern int falsecolor_draw;
                    if (!falsecolor_draw)
                    {
                        falsecolor_draw = 1;
                        return 0;
                    }
                    if (falsecolor_draw)
                    {
                        falsecolor_draw = 0;
                        redraw();
                        return 0;
                    }
                }
                
#ifdef CONFIG_SLIM_MENUS
                if (tapdisp == 4 && key == MODULE_KEY_TOUCH_1_FINGER)
#else
                if (tapdisp == 5 && key == MODULE_KEY_TOUCH_1_FINGER)
#endif
                {
                    SetGUIRequestMode(0);
                    msleep(100);
                    extern int falsecolor_draw;
                    if (!falsecolor_draw)
                    {
                        falsecolor_draw = 1;
                        return 0;
                    }
                    if (falsecolor_draw)
                    {
                        falsecolor_draw = 0;
                        redraw();
                        return 0;
                    }
                }
            }
            
            
            if (lv_dispsize != 10)
            {
                if ((tapdisp == 2 && key == MODULE_KEY_TOUCH_1_FINGER) || (SET_button == 7 && key == MODULE_KEY_PRESS_SET) || (!is_EOSM && INFO_button == 6 && key == MODULE_KEY_INFO))
                {
                    msleep(100);
                    if(lv_disp_mode != 0){
                        // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                        return 1;
                    }
                    select_menu_by_name("Movie", "Shutter Expo");
                    gui_open_menu();
                    msleep(10);
                    submenu = 1;
                }
                if ((tapdisp == 3 && key == MODULE_KEY_TOUCH_1_FINGER) || (SET_button == 6 && key == MODULE_KEY_PRESS_SET) || (!is_EOSM && INFO_button == 7 && key == MODULE_KEY_INFO))
                {
                    msleep(100);
                    if(lv_disp_mode != 0){
                        // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                        return 1;
                    }
                    select_menu_by_name("Movie", "Aperture Expo");
                    gui_open_menu();
                    msleep(10);
                    submenu = 1;
                }
#ifdef CONFIG_SLIM_MENUS
                if ((SET_button == 8 && key == MODULE_KEY_PRESS_SET) || (!is_EOSM && INFO_button == 8 && key == MODULE_KEY_INFO))
                {
                    msleep(100);
                    if(lv_disp_mode != 0){
                        return 1;
                    }
                    select_menu_by_name("Expo", "ISO");
                    gui_open_menu();
                    msleep(10);
                    submenu = 1;
                }
#else
                if ((tapdisp == 4 && key == MODULE_KEY_TOUCH_1_FINGER) || (SET_button == 8 && key == MODULE_KEY_PRESS_SET) || (INFO_button == 8 && key == MODULE_KEY_INFO))
                {
                    msleep(100);
                    if(lv_disp_mode != 0){
                        // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                        return 1;
                    }
                    select_menu_by_name("Movie", "ISO Expo");
                    gui_open_menu();
                    msleep(10);
                    submenu = 1;
                }
#endif
                if (tapdisp == 1 && key == MODULE_KEY_TOUCH_1_FINGER)
                {
                    msleep(100);
                    if(lv_disp_mode != 0){
                        // Use INFO key to cycle LV as normal when not in the LV with ML overlays
                        return 1;
                    }
                    select_menu_by_name("Movie", "Crop mood");
                    gui_open_menu();
                    msleep(10);
                    submenu = 1;
                }
            }
            
            /* Block SET/Arrows while recording to prevent changing focus box position.
             * When changing focus box position, a part of preview configuration changes, we don't 
             * want that to happen, to avoid corrupted frames, black preview or instability 
             * is there another way to block focus box from shifting, and make its position static? */
            if (RECORDING)
            {
                if (((key == MODULE_KEY_PRESS_SET)   && !SET_button) ||
                    ((key == MODULE_KEY_PRESS_UP)    && !Arrows_U_D) ||
                    ((key == MODULE_KEY_PRESS_DOWN)  && !Arrows_U_D) ||
                    ((key == MODULE_KEY_PRESS_LEFT)  && !Arrows_L_R) ||
                    ((key == MODULE_KEY_PRESS_RIGHT) && !Arrows_L_R))
                {
                    return 0;
                }
            }

            /* Block INFO button while recording to prevent switching among Full and Info preview in 1080i output */
            /* Preview configuration must not change during RAW recording, even Canon block preview switch in H.264 */
            if ((is_1080i_Full_Output() || is_1080i_Info_Output()) && RECORDING)
            {
                if (!INFO_button) // Not assigned?
                {
                    if (key == MODULE_KEY_INFO)
                    {
                        return 0; // Block INFO button
                    }
                }
            }                
        }
    }

    return 1;
}

/* Bottom-bar name of the active film format (A35, A35-ANA, S16, 16mm, S8, 8mm) */
static void slim_film_label(char * buffer, int size)
{
    int fmt = slim_film_active();
    if (fmt >= 0)
        snprintf(buffer, size, "%s", film_formats[fmt].label);
}

/* Display recording status in top info bar */
static LVINFO_UPDATE_FUNC(crop_info)
{
    LVINFO_BUFFER(16);
    
    if (patch_active)
    {
        if (lv_dispsize > 1)
        {
            switch (crop_preset)
            {
                case CROP_PRESET_1X1:
                    if (CROP_2_5K)     snprintf(buffer, sizeof(buffer), "2.5K");
                    if (CROP_2_8K)     snprintf(buffer, sizeof(buffer), "2.8K");
                    if (CROP_3K)       snprintf(buffer, sizeof(buffer), "3K");
                    if (CROP_1440p)    snprintf(buffer, sizeof(buffer), "1440p");
                    if (CROP_1280p)    snprintf(buffer, sizeof(buffer), "1280p");
                    if (CROP_1080p)    snprintf(buffer, sizeof(buffer), "1080p");
                    if (CROP_1620p)    snprintf(buffer, sizeof(buffer), "1620p");
                    if (CROP_Full_Res) snprintf(buffer, sizeof(buffer), "FLV");
                    break;
                case CROP_PRESET_1X3:
                    if (AR_16_9)
                    {
                        if (Anam_Highest) snprintf(buffer, sizeof(buffer), "4.5K");
                        if (Anam_Higher)  snprintf(buffer, sizeof(buffer), "4.2K");
                        if (Anam_Medium)  snprintf(buffer, sizeof(buffer), "UHD");
                    }
                    if (AR_2_1)
                    {
                        if (Anam_Highest) snprintf(buffer, sizeof(buffer), "4.8K");
                        if (Anam_Higher)  snprintf(buffer, sizeof(buffer), "4.4K");
                        if (Anam_Medium)  snprintf(buffer, sizeof(buffer), "4K");
                    }
                    if (AR_2_20_1)
                    {
                        if (Anam_Highest) snprintf(buffer, sizeof(buffer), "5K");
                        if (Anam_Higher)  snprintf(buffer, sizeof(buffer), "4.6K");
                        if (Anam_Medium)  snprintf(buffer, sizeof(buffer), "4.2K");
                    }
                    if (AR_2_35_1 || AR_2_39_1)
                    {
                        if (Anam_Highest) snprintf(buffer, sizeof(buffer), "5.2K");
                        if (Anam_Higher)  snprintf(buffer, sizeof(buffer), "4.8K");
                        if (Anam_Medium)  snprintf(buffer, sizeof(buffer), "4.4K");
                    }
                    break;
                case CROP_PRESET_3X3:
                    if (High_FPS)
                    {
                        if (AR_16_9)    snprintf(buffer, sizeof(buffer), "976p");
                        if (AR_2_1)     snprintf(buffer, sizeof(buffer), "868p");
                        if (AR_2_20_1)  snprintf(buffer, sizeof(buffer), "790p");
                        if (AR_2_35_1)  snprintf(buffer, sizeof(buffer), "738p");
                        if (AR_2_39_1 && crop_preset_fps_reduce == 0)  snprintf(buffer, sizeof(buffer), "694p"); // Actually 2.50:1 AR
                        if (AR_2_39_1 && crop_preset_fps_reduce == 1 && is_EOSM)  snprintf(buffer, sizeof(buffer), "726p");
                    }
                    if (mv1080) snprintf(buffer, sizeof(buffer), "1080p");
                    break;
            }
        }
    }

    /* append info about current binning mode */

    if (raw_lv_is_enabled())
    {
        /* fixme: raw_capture_info is only updated when LV RAW is active */

        /* When not in the zoom-branch naming path above, still name 1620p. */
        if (!buffer[0] && patch_active && crop_preset == CROP_PRESET_1X1 && CROP_1620p)
            snprintf(buffer, sizeof(buffer), "1620p");
        if (patch_active)
            slim_film_label(buffer, sizeof(buffer));

        if (patch_active && slim_film_active() >= 0)
        {
            /* film format: the name alone (no 1:1 / 3x3 tag) */
        }
        else if (raw_capture_info.binning_x + raw_capture_info.skipping_x == 1 &&
            raw_capture_info.binning_y + raw_capture_info.skipping_y == 1)
        {
            STR_APPEND(buffer, "%s1:1", buffer[0] ? " " : "");
        }
        else
        {
            STR_APPEND(buffer, "%s%dx%d",
                buffer[0] ? " " : "",
                raw_capture_info.binning_y + raw_capture_info.skipping_y,
                raw_capture_info.binning_x + raw_capture_info.skipping_x
            );
        }
    }

    if (crop_rec_needs_lv_refresh())
    {
        if (!streq(buffer, SYM_WARNING))
        {
            STR_APPEND(buffer, " " SYM_WARNING);
        }
        item->color_fg = COLOR_YELLOW;
    }
}

/* Display the Frame (aspect) of the active film format in the bottom bar */
static LVINFO_UPDATE_FUNC(frame_info)
{
    LVINFO_BUFFER(16);
    int fmt = slim_film_active();
    if (patch_active && fmt >= 0)
    {
        snprintf(buffer, sizeof(buffer), "%s",
            film_frames[film_frame_index(fmt, slim_film_frame_get(fmt))].frame);
        int n = strlen(buffer);
        if (n > 5 && streq(buffer + n - 5, " Crop"))
            buffer[n - 5] = 0;

        /* anamorphic frames ("2x 1.18:1", "1.33x 4:3"): add room on both sides so the text
         * does not crowd the film name on its left or the bit depth on its right */
        if (strstr(buffer, "x ") != NULL)
        {
            char tmp[16];
            snprintf(tmp, sizeof(tmp), "%s", buffer);
            snprintf(buffer, sizeof(buffer), "  %s  ", tmp);
        }
    }
}

/* Display Bitdepth in ML bottom bar */
static LVINFO_UPDATE_FUNC(bitdepth_info)
{
    LVINFO_BUFFER(8);

    int lossless_format = which_output_format() >= 3;
    /* which_output_format() == 0 means 14-bit uncompressed
     * which_output_format() == 1 means 12-bit uncompressed
     * which_output_format() == 2 means 10-bit uncompressed  */
    if (patch_active && is_movie_mode())
    {
        if ((OUTPUT_14BIT && lossless_format) || which_output_format() == 0) snprintf(buffer, sizeof(buffer), "14 Bit");
        if ((OUTPUT_12BIT && lossless_format) || which_output_format() == 1) snprintf(buffer, sizeof(buffer), "12 Bit");
        if ( OUTPUT_11BIT && lossless_format)                                snprintf(buffer, sizeof(buffer), "11 Bit");
        if ((OUTPUT_10BIT && lossless_format) || which_output_format() == 2) snprintf(buffer, sizeof(buffer), "10 Bit");
        item->color_fg = COLOR_GREEN1;
    }
}

static struct lvinfo_item info_items[] = {
    {
        .name = "Crop info",
        .which_bar = LV_BOTTOM_BAR_ONLY,
        .update = crop_info,
        .preferred_position = -128,  /* film format, far left */
        .priority = 1,
    },
    {
        .name = "Frame info",
        .which_bar = LV_BOTTOM_BAR_ONLY,
        .update = frame_info,
        .preferred_position = -120,  /* aspect, next to the film format */
        .priority = 1,
    },
    {
        .name = "Bitdepth info",
        .which_bar = LV_BOTTOM_BAR_ONLY,
        .update = bitdepth_info,
        .preferred_position = -112,  /* bit depth, third */
        .priority = 1,
    }
};

/* better put here too from raw.c since eosm is more or less 100% crop_rec based */
int raw_lv_settings_still_valid()
{
    /* Analog-gain bit depths: fixed whites matching 10/12-bit clip points.
     * 14-bit: keep raw_info.white_level from LV calibration/autodetect —
     * forcing 16200 hid real clipping on zebras/histogram. */
    if (OUTPUT_10BIT) raw_info.white_level = 2870;
    if (OUTPUT_11BIT) raw_info.white_level = 3692;
    if (OUTPUT_12BIT) raw_info.white_level = 5336;
    return 1;
}

static unsigned int raw_info_update_cbr(unsigned int unused)
{
    if (patch_active)
    {
        /* not implemented yet */
        raw_capture_info.offset_x = raw_capture_info.offset_y   = SHRT_MIN;

        if (!is_DIGIC_5) // needed for 700D and similair models
        {
            if (lv_dispsize > 1)
            {
                /* raw backend gets it right */
                return 0;
            }
        }

        /* update horizontal pixel binning parameters */
        switch (crop_preset)
        {
            case CROP_PRESET_1X1:
                raw_capture_info.binning_x    = raw_capture_info.binning_y  = 1;
                raw_capture_info.skipping_x   = raw_capture_info.skipping_y = 0;
                break;

            case CROP_PRESET_1X3:
            case CROP_PRESET_3X3:
                raw_capture_info.binning_x = 3; raw_capture_info.skipping_x = 0;
                break;
        }

        /* update vertical pixel binning / line skipping parameters */
        switch (crop_preset)
        {
            case CROP_PRESET_1X3:
            case CROP_PRESET_1X1:
                raw_capture_info.binning_y = 1; raw_capture_info.skipping_y = 0;
                break;

            case CROP_PRESET_3X3:
            {
                int b = 1;
                int s = 2;
                raw_capture_info.binning_y = b; raw_capture_info.skipping_y = s;
                break;
            }
        }

        if (is_EOSM)
        {
            /* update skip offsets */
            int skip_left, skip_right, skip_top, skip_bottom;
            calc_skip_offsets(&skip_left, &skip_right, &skip_top, &skip_bottom);
            raw_set_geometry(raw_info.width, raw_info.height, skip_left, skip_right, skip_top, skip_bottom);

            /* crop modes use non-square pixel aspect (e.g. 1x3) */
            raw_set_preview_rect(skip_left + 14, skip_top + 8,
                raw_info.width - skip_left - skip_right - 28,
                raw_info.height - skip_top - skip_bottom - 16,
                1);
            raw_force_aspect_ratio(
                raw_capture_info.binning_x + raw_capture_info.skipping_x,
                raw_capture_info.binning_y + raw_capture_info.skipping_y
            );
        }
    }
    return 0;
}

/* ---- Saved settings -----------------------------------------------------------
 * Every saved setting is listed in MODULE_CONFIGS at the end of this file.  The table below
 * gives the range each one can really have (taken from the menus); a value outside its
 * range is reset to the one in the last column.  crop_settings_ver is the settings version:
 * to change what a saved value MEANS, add a block to crop_settings_load() for the new
 * version and raise CROP_SETTINGS_VERSION.  Blocks run once per config file.
 */
#define CROP_SETTINGS_VERSION 4

static const struct setting_range crop_settings[] = {
    SETTING(crop_preset_index,          1,       3,      1),  /* slim: 1..3 (3x3 / 1x1 / 1x3) */
    SETTING(crop_preset_ar_menu,        0,       4,      4),
    SETTING(crop_preset_1x1_res_menu,   0,       7,      3),
    SETTING(crop_preset_1x3_res_menu,   0,       3,      1),
    SETTING(crop_preset_3x3_res_menu,   0,       2,      2),
    SETTING(crop_preset_fps_menu,       0,       3,      0),
    SETTING(crop_preset_fps_reduce,     0,       1,      1),
    SETTING(slim_film_fmt,              0,       FILM_FORMAT_COUNT - 1, 0),
    SETTING(slim_film_frames,           0,       (1 << (2 * FILM_FORMAT_COUNT)) - 1, 0),
    SETTING(bit_depth_analog,           0,       3,      1),
    SETTING(shutter_range,              0,       1,      0),
    SETTING(fix_dual_iso_flicker,       0,       1,      1),
    SETTING(brighten_lv_method,         0,       1,      0),
    SETTING(fps_over,             -100000,  100000,      0),
    SETTING(SET_button,                 1,       2,      1),  /* slim: x10 zoom / last settings */
    SETTING(INFO_button,                0,       6,      0),
    SETTING(Shutter_rec,                0,       1,      0),
    SETTING(Arrows_U_D,                 0,       3,      3),
    SETTING(Shutter_zoom,               0,       2,      0),
    SETTING(tapdisp,                    0,       5,      1),
};

static void crop_settings_load(void)
{
    /* version 1: arrow and INFO button choices were renumbered */
    if (crop_settings_ver < 1)
    {
        /* Old arrows: 0=OFF, 1=ISO, 2=Aperture -> New: 0=OFF, 1=Shutter, 2=Aperture, 3=ISO */
        if (Arrows_U_D == 1) Arrows_U_D = 3;
        if (Arrows_L_R == 1) Arrows_L_R = 3;
        /* Old INFO: 0=OFF,1=Aperture,2=FC,3=DualISO,4=framing
         * New INFO: 0=OFF,1=DualISO,2=Hist,3=Wave,4=FC,5=framing */
        if (INFO_button == 1) INFO_button = 0;
        else if (INFO_button == 2) INFO_button = 4;
        else if (INFO_button == 3) INFO_button = 1;
        else if (INFO_button == 4) INFO_button = 5;
        crop_settings_ver = 1;
    }
    /* version 2: INFO 4=False Color, 5=framing -> 4=Zebras, 5=False Color, 6=framing */
    if (crop_settings_ver < 2)
    {
        if (INFO_button == 5) INFO_button = 6;
        else if (INFO_button == 4) INFO_button = 5;
        crop_settings_ver = 2;
    }
    /* version 3: Dual ISO removed from the list: 2..7 -> 1..6, old Dual ISO -> OFF */
    if (crop_settings_ver < 3)
    {
        if (INFO_button == 1) INFO_button = 0;
        else if (INFO_button > 1) INFO_button--;
        crop_settings_ver = 3;
    }

    /* version 4: Framing is hidden from the INFO button list (value 5 stays unused): -> OFF.
     * Checked on every load, so a stray 5 can never come back. */
    if (INFO_button == 5)
        INFO_button = 0;
    if (crop_settings_ver < 4)
        crop_settings_ver = 4;

    /* never lower the number (a config file touched by a newer build keeps its version) */
    if (crop_settings_ver < CROP_SETTINGS_VERSION)
        crop_settings_ver = CROP_SETTINGS_VERSION;

    /* every setting inside its range (old SET button choices, damaged files, ...) */
    settings_check(crop_settings, COUNT(crop_settings));
}

static unsigned int crop_rec_init()
{
    //Will place afframe so that 2_1 HFR presets will work at least after restart
    if (!is_manual_focus())
    {
        center_lv_afframe();
        msleep(200);
        move_lv_afframe(0, 100);
        msleep(200);
        clear_lv_afframe();
    }
    
    if (is_camera("EOSM", "2.0.2"))
    {
        CMOS_WRITE = 0x2998C;
        MEM_CMOS_WRITE = 0xE92D41F0;
        
        ADTG_WRITE = 0x2986C;
        MEM_ADTG_WRITE = 0xE92D43F8;
        
        ENGIO_WRITE = 0xFF2C19AC;
        MEM_ENGIO_WRITE = 0xE51FC15C;
        
        ENG_DRV_OUT = 0xFF2C1694;
        ENG_DRV_OUTS = 0xFF2C17B8;
        
        PathDriveMode = (void *) 0x892E8;   /* argument of PATH_SelectPathDriveMode */
        PATH_SelectPathDriveMode = 0x14AC4; // it's being called from RAM
        
        EDMAC_9_Vertical_1 = 0x5976C;
        EDMAC_9_Vertical_2 = 0x5979C;
        HIV_Vertical_Photo_Address = 0x4FE5F070;
        HIV_Vertical_Address_hook = 0xFF500D04;
        
        EDID_HDMI_INFO = (void *) 0x821CC;
        
        Shift_x5_LCD = 0xFF96EA3C;
        Shift_x5_HDMI_480p = 0xFF96F3FC;
        Shift_x5_HDMI_1080i_Full = 0xFF96FF54;
        Shift_x5_HDMI_1080i_Info = 0xFF970558;

        Clear_Vram_x5_LCD = 0xFF96EA60;
        Clear_Vram_x5_HDMI_480p = 0xFF96F420;
        Clear_Vram_x5_HDMI_1080i_Full = 0xFF96FF90;
        Clear_Vram_x5_HDMI_1080i_Info = 0xFF970594;
        
        is_EOSM = 1;
        is_DIGIC_5 = 1;
        crop_presets                = crop_presets_DIGIC_5;
        crop_rec_menu[0].choices    = crop_choices_DIGIC_5;
        crop_rec_menu[0].max        = COUNT(crop_choices_DIGIC_5) - 1;
        crop_rec_menu[0].help       = crop_choices_help_DIGIC_5;
    }

    /* default FPS timers are the same on all these models */
    if (is_EOSM)
    {
        fps_main_clock = 32000000;
                                       /* 24p,  25p,  30p,  50p,  60p,   x5, c24p, c25p, c30p */
        memcpy(default_timerA, (int[]) {  528,  640,  528,  640,  528,  716,  546,  640,  546 }, 36);
        memcpy(default_timerB, (int[]) { 2527, 2000, 2022, 1000, 1011, 1491, 2444, 2000, 1955 }, 36);
                                   /* or 2528        2023        1012        2445        1956 */
    }

    /* FPS in x5 zoom may be model-dependent; assume exact */
    default_fps_1k[5] = (uint64_t) fps_main_clock * 1000ULL / default_timerA[5] / default_timerB[5];

    printf("[crop_rec] checking FPS timer values...\n");
    for (int i = 0; i < COUNT(default_fps_1k); i++)
    {
        if (default_timerA[i])
        {
            int fps_i = (uint64_t) fps_main_clock * 1000ULL / default_timerA[i] / default_timerB[i];
            if (fps_i == default_fps_1k[i])
            {
                printf("%d) %s%d.%03d: A=%d B=%d (exact)\n", i, FMT_FIXEDPOINT3(default_fps_1k[i]), default_timerA[i], default_timerB[i]);

                if (i == 5 && default_fps_1k[i] != 29970)
                {
                    printf("-> unusual FPS in x5 zoom\n", i);
                }
            }
            else
            {
                int fps_p = (uint64_t) fps_main_clock * 1000ULL / default_timerA[i] / (default_timerB[i] + 1);
                if (fps_i > default_fps_1k[i] && fps_p < default_fps_1k[i])
                {
                    printf("%d) %s%d.%03d: A=%d B=%d/%d (averaged)\n", i, FMT_FIXEDPOINT3(default_fps_1k[i]), default_timerA[i], default_timerB[i], default_timerB[i] + 1);
                }
                else
                {
                    printf("%d) %s%d.%03d: A=%d B=%d (%s%d.%03d ?!?)\n", i, FMT_FIXEDPOINT3(default_fps_1k[i]), default_timerA[i], default_timerB[i], FMT_FIXEDPOINT3(fps_i));
                    return CBR_RET_ERROR;
                }

                /* assume 25p is exact on all models */
                if (i == 1)
                {
                    printf("-> 25p check error\n");
                    return CBR_RET_ERROR;
                }
            }
        }
    }


    if (is_EOSM)
    {
        /* Saved settings: convert older values, then range-check all of them. */
        crop_settings_load();

        /* Derive Mode UI (incl. LV) then push 1x1 combo / Full-Res. */
        slim_crop_sync_from_backend();
        slim_film_sync();   /* saved Film Format / Frame agree with the saved readout from the start */
        slim_crop_apply_mode();
        slim_crop_apply_bit_depth();

        /* EOS M slim: fixed, not user choices */
        more_hacks = 1;
        Half_Shutter = 0;   /* half-shutter x10 only via Shutter zoom setting */
        Arrows_L_R = 0;     /* no Left/Right remap: leave L/R to Canon */

        /* Flat Movie-page crop settings (no Crop Mode submenu / Customize Buttons). */
        menu_add("Movie", crop_rec_menu_eosm, COUNT(crop_rec_menu_eosm));
        menu_add("Expo", expo_shutter_range_eosm, COUNT(expo_shutter_range_eosm));
        menu_add("Settings", slim_info_button_menu, COUNT(slim_info_button_menu));
        anamorphic_preview_add_slim_menu();
        menu_add("Settings", slim_more_hacks_menu, COUNT(slim_more_hacks_menu));
        lvinfo_add_items(info_items, COUNT(info_items));
        return 0;
    }

    if (!is_EOSM)
        menu_add("Movie", movie_menu_fps, COUNT(movie_menu_fps));
    menu_add("Movie", movie_menu_bitdepth, COUNT(movie_menu_bitdepth));
    menu_add("Movie", crop_rec_menu, COUNT(crop_rec_menu));
    menu_add("Movie", customize_buttons_menu, COUNT(customize_buttons_menu));
    menu_add("Movie", movie_menu_shutter_range, COUNT(movie_menu_shutter_range));
    menu_add("Movie", movie_menu_framerate, COUNT(movie_menu_framerate));
    menu_add("Movie", movie_menu_ratio, COUNT(movie_menu_ratio));
    lvinfo_add_items (info_items, COUNT(info_items));

    return 0;
}

static unsigned int crop_rec_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(crop_rec_init)
    MODULE_DEINIT(crop_rec_deinit)
MODULE_INFO_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(crop_preset_index)
    MODULE_CONFIG(fps_over)
    MODULE_CONFIG(shutter_range)
    MODULE_CONFIG(bit_depth_analog)
    MODULE_CONFIG(crop_preset_1x1_res_menu)
    MODULE_CONFIG(crop_preset_1x3_res_menu)
    MODULE_CONFIG(crop_preset_3x3_res_menu)
    MODULE_CONFIG(crop_preset_ar_menu)
    MODULE_CONFIG(crop_preset_fps_menu)
    MODULE_CONFIG(crop_preset_fps_reduce)
    MODULE_CONFIG(slim_film_fmt)
    MODULE_CONFIG(slim_film_frames)
    MODULE_CONFIG(fix_dual_iso_flicker)
    MODULE_CONFIG(brighten_lv_method)
    MODULE_CONFIG(Half_Shutter)
    MODULE_CONFIG(SET_button)
    MODULE_CONFIG(INFO_button)
    MODULE_CONFIG(Shutter_zoom)
    MODULE_CONFIG(Shutter_rec)
    MODULE_CONFIG(tapdisp)
    MODULE_CONFIG(Arrows_L_R)
    MODULE_CONFIG(Arrows_U_D)
    MODULE_CONFIG(more_hacks)
    MODULE_CONFIG(crop_settings_ver)
MODULE_CONFIGS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_SHOOT_TASK, crop_rec_polling_cbr, 0)
    MODULE_CBR(CBR_RAW_INFO_UPDATE, raw_info_update_cbr, 0)
    MODULE_CBR(CBR_KEYPRESS, crop_rec_keypress_cbr, 0)
    MODULE_CBR(CBR_KEYPRESS, photo_keypress_cbr, 0)
MODULE_CBRS_END()

MODULE_PROPHANDLERS_START()
    MODULE_PROPHANDLER(PROP_LV_ACTION)
    MODULE_PROPHANDLER(PROP_LV_DISPSIZE)
MODULE_PROPHANDLERS_END()
