#include <ncurses.h>

#include "types.h"
#include "ui.h"

/* When insert mode is disabled it should highlight the lyrics */
static bool insert_mode_active = false;

static char lyrics[1024];
static size_t lyrics_len;

bool
lrclib_insert_mode_active(void) {
  return insert_mode_active;
}

void
lrclib_set_insert_mode(const Arg *arg) {
  insert_mode_active = arg->b;
}

void
lrclib_handle_input(int input) {
  if (input >= 0 && input < 256 && lyrics_len + 1 < sizeof(lyrics))
    lyrics[lyrics_len++] = (char)input;
}

static void
sync_line(void) {

}

static void
draw_lrclib_keybinds(WINDOW *win) {
  int height, width;
  getmaxyx(win, height, width);
  char buf[128];

  if (insert_mode_active)
    snprintf(buf, sizeof(buf), "Exit:Esc");
  else
    snprintf(buf, sizeof(buf), "Edit:i");

  style_on(win, STYLE_KEYBIND);
  draw_text(win, height - 2, 3, width, buf);
  style_off(win, STYLE_KEYBIND);
}

static void
draw_lrclib_lyrics(WINDOW *win) {
  int padding = 6;
  
  int height, width;
  getmaxyx(win, height, width);

  style_on(win, STYLE_DEFAULT);
  draw_text(win, 1, padding, width, lyrics);
  style_off(win, STYLE_DEFAULT);
}

void
draw_lrclib_sync(WINDOW *win) {
  draw_lrclib_keybinds(win);
  draw_lrclib_lyrics(win);
}

bool
lrclib_sync_active(void) {
  return tab_active(draw_lrclib_sync);
}
