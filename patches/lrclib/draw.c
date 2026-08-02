#include <ncurses.h>

#include "types.h"
#include "ui.h"

/* When insert mode is disabled it should highlight the lyrics */
static bool insert_mode_active = false;


void
lrclib_handle_input(int input) {

}

static void
sync_line() {

}

static void
draw_keybinds(WINDOW *win) {
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

void
draw_lrclib_sync(WINDOW *win) {
  draw_keybinds(win);
}

bool
lrclib_sync_active() {
  return tab_active(draw_lrclib_sync);
}
