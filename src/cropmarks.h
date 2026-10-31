#ifndef _cropmarks_h_
#define _cropmarks_h_
/**
 * @brief Set custom movie cropmarks in LiveView
 *
 * @param x X coordinate
 * @param y Y coordinate
 * @param w Width
 * @param h Height
 * @addtogroup cropmarks
 */
void set_movie_cropmarks(int x, int y, int w, int h);
/**
 * @brief Reset LiveView movie cropmarks
 */
void reset_movie_cropmarks();

void crop_set_dirty(int redraw_timeout);

/**
 * @brief Paint the Live View frame directly: transparent inside the movie cropmarks, the 2 px
 * white frame line around them and (if fill_outside) black everywhere else.  Lets a screen that
 * is being closed (splash, menu) hand over without first clearing to an empty overlay.
 * @return 1 if done, 0 if there are no movie cropmarks (caller should clear and redraw instead)
 */
int film_canvas_paint(int fill_outside);
int film_canvas_restore_area(int x, int y, int w, int h);

#endif
