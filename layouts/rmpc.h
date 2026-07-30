#pragma once

#include <ncurses.h>

#include "layout.h"
#include "mpd.h"
#include "types.h"
#include "ui.h"

#define RMPC_HEADER_CELL_LEFT 13 + padding_left
#define RMPC_HEADER_CELL_RIGHT 8 - padding_right

static inline void
rmpc_header(WINDOW *win)
{
  int width = getmaxx(win);
  const char *state, *album_string;
  char position[64], title[512], album[512], percent[16];
  unsigned elapsed, total;
  int left_edge, right_edge, mid_x, mid_w;
  bool show_album_text = false;
  int player_state;
  int right_x, right_w, volume, mw, mx;

  style_on(win, STYLE_BORDER_FOCUSED);
  box(win, 0, 0);
  mvwvline(win, 1, RMPC_HEADER_CELL_LEFT, 0, 2);
  mvwvline(win, 1, width - RMPC_HEADER_CELL_RIGHT - 1, 0, 2);
  style_off(win, STYLE_BORDER_FOCUSED);

  left_edge = RMPC_HEADER_CELL_LEFT;
  right_edge = width - RMPC_HEADER_CELL_RIGHT;

  switch (get_player_state()) {
    case MPD_STATE_PLAY:
      state = "[Playing]";
      break;
    case MPD_STATE_PAUSE:
      state = "[Paused]";
      break;
    default:
      state = "[Stopped]";
      break;
  }
  style_on(win, STYLE_STATE_PLAYER);
  draw_text(win, 1, 1 + padding_left, left_edge - padding_left, state);
  style_off(win, STYLE_STATE_PLAYER);

  elapsed = get_elapsed_time();
  total = get_total_time();
  snprintf(position, sizeof(position), "%u:%02u / %u:%02u", elapsed / 60,
      elapsed % 60, total / 60, total % 60);
  style_on(win, STYLE_TIME);
  draw_text(win, 2, 1 + padding_left, left_edge - padding_left,
      position);
  style_off(win, STYLE_TIME);

  mid_x = left_edge + 2;

  /* this will draw artist and title text in the center of header
   * remainder: full_width -= (left_col_width + right_col_width) */
  /* mid_w = right_edge - left_edge - 2; */

  /* this will draw the text in the center based off stdscr width but
   * clips earlier, this looks nicer because symmetry with tabs etc but
   * one above is "technically" correct */
  mid_w = right_edge - left_edge -
    (RMPC_HEADER_CELL_LEFT - RMPC_HEADER_CELL_RIGHT) - 2;

  if (get_artist()[0])
    snprintf(title, sizeof(title), "%s - %s", get_artist(),
        get_title());
  else
    snprintf(title, sizeof(title), "%s", get_title());
  style_on(win, STYLE_TITLE);
  draw_text_centered(win, 1, mid_x, mid_w, title);
  style_off(win, STYLE_TITLE);

  player_state = get_player_state();
  if (player_state == MPD_STATE_PLAY || player_state == MPD_STATE_PAUSE)
    show_album_text = true;
  album_string = get_album()[0]    ? get_album()
    : show_album_text ? "Unknown Album"
    : "";
  snprintf(album, sizeof(album), show_album_text ? "(%s)" : "",
      album_string);
  style_on(win, STYLE_ALBUM);
  draw_text_centered(win, 2, mid_x, mid_w, album);
  style_off(win, STYLE_ALBUM);

  right_x = right_edge + 1;
  right_w = width - right_x - 1 - padding_right;
  volume = get_volume();

  if (volume < 0)
    snprintf(percent, sizeof(percent), "N/A");
  else
    snprintf(percent, sizeof(percent), "%d%%", volume);
  style_on(win, STYLE_VOLUME);
  draw_text_right(win, 1, right_x, right_w, percent);
  style_off(win, STYLE_VOLUME);

  mw = modes_width();
  mx = right_x + right_w - mw;
  if (mx < right_x)
    mx = right_x;
  draw_modes(win, 2, mx, right_x + right_w - mx);
}

static inline void
rmpc_tabs(WINDOW *win)
{
  const int gap = 4;
  int width = getmaxx(win);
  int i, total = 0, x, limit;

  for (i = 0; i < tab_count(); i++)
    total += text_width(tab_name(i)) + (i ? gap : 0);

  x = (width - total) / 2;
  if (x < padding_left)
    x = padding_left;

  limit = width - padding_right;

  for (i = 0; i < tab_count(); i++) {
    int slot;

    if (i)
      x += gap;
    slot = (i == active_tab()) ? STYLE_TAB_ACTIVE : STYLE_TAB;
    style_on(win, slot);
    draw_text(win, 0, x, limit - x, tab_name(i));
    style_off(win, slot);
    x += text_width(tab_name(i));
  }
}

static inline void
rmpc_status(WINDOW *win)
{
  int width = getmaxx(win);
  int bar_w;

  if (mpd_status_line(win, 0))
    return;

  bar_w = width - padding_left - padding_right;
  if (bar_w > 0) {
    WINDOW *sub = derwin(win, 1, bar_w, 0, padding_left);

    if (sub) {
      draw_progress_bar(sub, 0, bar_w);
      delwin(sub);
      touchwin(win);
    }
  }
}

static const Bar bars[] = {
  /* edge       size draw function redraw condition */
  { BAR_TOP,    4,   rmpc_header,  REDRAW_PLAYER | REDRAW_MIXER | REDRAW_TICK },
  { BAR_TOP,    1,   rmpc_tabs,    REDRAW_FOCUS },
  { BAR_BOTTOM, 1,   rmpc_status,  REDRAW_PLAYER | REDRAW_TICK },
};

static inline Rect
layout_frame(WINDOW *win, bool focused)
{
  int height, width, slot;

  getmaxyx(win, height, width);

  slot = focused ? STYLE_BORDER_FOCUSED : STYLE_BORDER;
  style_on(win, slot);
  box(win, 0, 0);
  style_off(win, slot);

  return (Rect){ 1, 1, height - 2, width - 2 };
}

static inline void
layout_arrange(Rect area, int active, Rect *out, int n)
{
  int i;

  for (i = 0; i < n; i++)
    out[i] = (i == active) ? area : (Rect){ 0, 0, 0, 0 };
}
