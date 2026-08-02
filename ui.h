#pragma once
#include <mpd/client.h>
#include <panel.h>

#include "layout.h"
#include "theme.h"
#include "types.h"

void init_ncurses(void);
void destroy_ncurses(void);
void resize(void);

/* repaint every bar and pane whose redraw condition overlaps with flags arg */
void ui_redraw(unsigned flags);
void ui_on_mpd_events(enum mpd_idle events);

void focus_next_tab(void);
void focus_prev_tab(void);
void focus_tab_by_key(int key);

void play_selected(const Arg *arg);
void add_to_queue(const Arg *arg);
void delete_selected(const Arg *arg);
void select_item(const Arg *arg);
void filter_results(const Arg *arg);
void find_next(const Arg *arg);
void find_prev(const Arg *arg);

bool find_active(void);
void find_input(int key);

bool search_edit_active(void);
void search_edit_key(int key);

/* if your patch needs to move a cursor, add an ifdef guard
 * for your patch within these navigation functions */

/* used for navigating left/right .i = -1 / .i = 1 */
void nav(const Arg *arg);

/* used for navigating up/down .i = -1 / .i = 1 */
void cursor_move(const Arg *arg);

/* moves cursor a page up/down .i = -1 / .i = 1 */
void cursor_page(const Arg *arg);

/* moves cursor to top/bottom .i = -1 / .i = 1 */
void cursor_edge(const Arg *arg);

void draw_now_playing(WINDOW *win);
void draw_browse(WINDOW *win);
void draw_lyrics(WINDOW *win);
void draw_search(WINDOW *win);

/* attributes for a STYLE_* slot, style_custom for a patch owned color */
attr_t style(int slot);
attr_t style_custom(const Style *custom);
void style_on(WINDOW *win, int slot);
void style_off(WINDOW *win, int slot);

int active_tab(void);
int tab_count(void);
const char *tab_name(int index);

/* there are two ways to ask about a tab by its draw function.
 * Pick by what you need back as they are not interchangeable:
 *
 *   tab_with_draw()  where is it?     -> index, for addressing a slot
 *   tab_active()     is it focused?   -> bool,  for gating behaviour
 *
 */

/* index of the tab drawn by this function, or -1 when it is not in tabs[].
 *
 * For layouts: layout_arrange() fills out[] indexed by tab, so this is how you
 * reach one particular tab's slot to give it geometry the others do not get.
 * Same for LAYOUT_BAR_HIDDEN() when a bar should hide for one specific tab. */
int tab_with_draw(void (*draw)(WINDOW *win));

/* true when the tab drawn by this function is the focused one.
 * See docs/patches.md. */
bool tab_active(void (*draw)(WINDOW *win));

void draw_progress_bar(WINDOW *win, int row, int width);

/* draw with these, not mvwaddstr()/wprintw(). Measure with text_width(), not
 * strlen(). These measure using columns instead of bytes to prevent a
 * disagreement between ncurses and the terminal. */
int text_width(const char *text);
void draw_text(WINDOW *win, int row, int x, int width, const char *text);
void draw_text_centered(WINDOW *win, int row, int x, int width,
    const char *text);
void draw_text_right(WINDOW *win, int row, int x, int width, const char *text);

/* default drawing function for elapsed time. You can customize these
 * by drawing them yourself in a layout. See layouts/rcmp.h for an
 * example on how */
void draw_elapsed(WINDOW *win, int row, int width);
int elapsed_width(void); /* columns draw_elapsed needs */

/* default drawing function for drawing playback mode: [zxcv], and volume.
 * You can customize this yourself in a layout but is not recommended.
 * See layouts/rcmp.h for similar example with draw_elapsed */
void draw_volume(WINDOW *win, int row, int width);
void draw_modes(WINDOW *win, int row, int x, int width);
int modes_width(void); /* columns draw_modes needs */

/* draws the connection error and returns true when mpd is unreachable */
bool mpd_status_line(WINDOW *win, int row);


/* append "Label:key" to buf, looking the key up in the user's keybinds so a
 * hint line never hardcodes one. Appends nothing when the action is unbound.
 * *len tracks the length written so far, start it at 0.
 *
 * Use hint_add_i() for an action bound several times with different Args
 * (cursor_move, nav, cycle_tab). */
void hint_add(char *buf, size_t size, size_t *len, const char *label,
    void (*action)(const Arg *));
void hint_add_i(char *buf, size_t size, size_t *len, const char *label,
    void (*action)(const Arg *), int i);
void hint_add_b(char *buf, size_t size, size_t *len, const char *label,
    void (*action)(const Arg *), bool b);
