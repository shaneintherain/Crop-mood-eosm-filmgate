/* crop_rec interface */
extern WEAK_FUNC(ret_0) int analog_gain_is_acive();
extern WEAK_FUNC(ret_0) int crop_rec_is_enabled();
extern WEAK_FUNC(ret_0) int is_LCD_Output();
extern WEAK_FUNC(ret_0) int is_480p_Output();
extern WEAK_FUNC(ret_0) int is_1080i_Full_Output();
extern WEAK_FUNC(ret_0) int is_1080i_Info_Output();
/* Core Live View touch editor interface.  control: 0=crop/mode+resolution,
 * 1=frame rate, 2=bit depth. */
extern WEAK_FUNC(ret_0) int crop_rec_touch_adjust(int control, int delta);
extern WEAK_FUNC(ret_0) int crop_rec_touch_get_value(int control, int slot,
                                                      char *value, int size,
                                                      int *enabled);
/* Custom page Movie controls: 0=Mode, 1=Aspect Ratio, 2=Preset. */
extern WEAK_FUNC(ret_0) int crop_rec_custom_adjust(int control, int delta);
/* Film Format / Frame chosen in the Movie menu: 0 = none, otherwise an index
 * 1..23 into the film_frames[] table in src/film-formats.h. */
extern WEAK_FUNC(ret_0) int crop_rec_film_format();
/* 1 when the active recording format is a FILM standard one (A35 ... 8mm): the shutter is shown
 * as an angle first.  0 for the VIDEO standard and for every other mode. */
extern WEAK_FUNC(ret_0) int crop_rec_film_standard();
/* 1 when the active recording format is a VIDEO standard one (2/3" ... 1/4"): ISO is shown as Gain. */
extern WEAK_FUNC(ret_0) int crop_rec_video_standard();
/* Settings -> Shutter record: 1 = a half-press of the shutter button starts/stops recording. */
extern WEAK_FUNC(ret_0) int crop_rec_shutter_record();
