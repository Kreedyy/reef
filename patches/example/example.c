#include "example.h"
#include "mpd.h"
#include "ui.h"
#include <ncurses.h>

#define EXAMPLE_COLUMNS 2
#define EXAMPLE_ROWS 4
#define EXAMPLE_LIST_ROWS 6

static const char *items[EXAMPLE_COLUMNS][EXAMPLE_ROWS] = {
  { "left one", "left two", "left three", "left four" },
  { "right one", "right two", "right three", "right four" },
};

static const char *list_items[EXAMPLE_LIST_ROWS] = {
  "list one", "list two", "list three",
  "list four", "list five", "list six",
};

static int column;
static int grid_cursor;
static int list_cursor;

bool
example_active(void) {
  return tab_active(draw_example_testtab1) || tab_active(draw_example_testtab2);
}

static int
clamp_cursor(int value, int count) {
  if (value < 0)
    return 0;
  return value >= count ? count - 1 : value;
}

void
example_move(int delta) {
  if (tab_active(draw_example_testtab2))
    list_cursor = clamp_cursor(list_cursor + delta,
                               EXAMPLE_LIST_ROWS);
  else
    grid_cursor = clamp_cursor(grid_cursor + delta, EXAMPLE_ROWS);
}

void
example_nav(int dir) {
  if (tab_active(draw_example_testtab1))
    column = dir < 0 ? 0 : EXAMPLE_COLUMNS - 1;
}

void
example_reset(const Arg *arg) {
  (void)arg;

  if (tab_active(draw_example_testtab2)) {
    list_cursor = 0;
    return;
  }
  grid_cursor = 0;
  column = 0;
}

static void
draw_hints(WINDOW *win, int width, bool columns) {
  char buf[64] = "";
  size_t len = 0;

  if (columns) {
    hint_add_i(buf, sizeof(buf), &len, "Left", nav, -1);
    hint_add_i(buf, sizeof(buf), &len, "Right", nav, 1);
  }
  hint_add_i(buf, sizeof(buf), &len, "Down", cursor_move, 1);
  hint_add_i(buf, sizeof(buf), &len, "Up", cursor_move, -1);

  hint_add(buf, sizeof(buf), &len, "Reset", example_reset);

  draw_text_centered(win, 0, 0, width, buf);
}

static void
draw_row(WINDOW *win, int row, int x, int width, const char *text,
         bool selected) {
  if (selected)
    style_on(win, STYLE_HIGHLIGHT);
  draw_text(win, row, x, width, text);
  if (selected)
    style_off(win, STYLE_HIGHLIGHT);
}

static int
block_top(int height, int n) {
  int top = height / 2 - n / 2;

  return top < 1 ? 1 : top;
}

void
draw_example_testtab1(WINDOW *win) {
  int height, width, colw, top, col, row;

  getmaxyx(win, height, width);
  colw = width / EXAMPLE_COLUMNS;
  top = block_top(height, EXAMPLE_ROWS);

  draw_hints(win, width, true);

  for (col = 0; col < EXAMPLE_COLUMNS; col++)
    for (row = 0; row < EXAMPLE_ROWS; row++)
      draw_row(win, top + row, col * colw, colw, items[col][row],
               col == column && row == grid_cursor);
}

void
draw_example_testtab2(WINDOW *win) {
  int height, width, top, row;

  getmaxyx(win, height, width);
  top = block_top(height, EXAMPLE_LIST_ROWS);

  draw_hints(win, width, false);

  for (row = 0; row < EXAMPLE_LIST_ROWS; row++)
    draw_row(win, top + row, 0, width, list_items[row],
             row == list_cursor);
}

void
draw_example_testtab3(WINDOW *win) {
  int height, width, top;
  char buf[64] = "";
  size_t len = 0;

  getmaxyx(win, height, width);
  top = block_top(height, 2);

  hint_add(buf, sizeof(buf), &len,
           get_player_state() == MPD_STATE_PLAY ? "Pause" : "Play",
           toggle_pause);
  hint_add(buf, sizeof(buf), &len, "Next", play_next);
  draw_text_centered(win, 0, 0, width, buf);

  draw_text_centered(win, top, 0, width,
                     has_song_loaded() ? get_title() : "nothing playing");
  draw_text_centered(win, top + 1, 0, width, get_artist());
}
