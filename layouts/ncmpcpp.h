#ifndef LAYOUT_NCMPCPP_H
#define LAYOUT_NCMPCPP_H

#include <ncurses.h>

#include "layout.h"
#include "mpd.h"
#include "types.h"
#include "ui.h"

static inline void
ncmpcpp_header(WINDOW *win)
{
  int width = getmaxx(win);
  int line_row = getmaxy(win) - 1;
  int mw, mx;

  style_on(win, STYLE_BORDER);
  mvwhline(win, line_row, 0, 0, width);
  style_off(win, STYLE_BORDER);

  style_on(win, STYLE_TAB_ACTIVE);
  draw_text(win, 0, padding_left, width, tab_name(active_tab()));
  style_off(win, STYLE_TAB_ACTIVE);
  draw_volume(win, 0, width);

  mw = modes_width();
  mx = width - padding_right - mw;
  if (mx < 0)
    mx = 0;
  draw_modes(win, line_row, mx, mw);
}

static inline void
ncmpcpp_footer(WINDOW *win)
{
  int width = getmaxx(win);
  int row = getmaxy(win) - 1;
  const char *text;
  int title_x, title_width;

  if (mpd_status_line(win, row))
    return;

  draw_progress_bar(win, 0, width);

  switch (get_player_state()) {
    case MPD_STATE_PLAY:
      text = "Playing: ";
      break;
    case MPD_STATE_PAUSE:
      text = "Paused: ";
      break;
    default:
      text = "";
      break;
  }

  style_on(win, STYLE_STATE_PLAYER);
  draw_text(win, row, padding_left, width, text);
  style_off(win, STYLE_STATE_PLAYER);

  title_x = padding_left + text_width(text);
  title_width = width - padding_right - elapsed_width() - 1 - title_x;
  if (title_width < 0)
    title_width = 0;

  style_on(win, STYLE_TITLE);
  draw_text(win, row, title_x, title_width, get_title());
  style_off(win, STYLE_TITLE);
  draw_elapsed(win, row, width);
}

static const Bar bars[] = {
  /* edge        size  draw function    redraw condition */
  { BAR_TOP,     2,    ncmpcpp_header,  REDRAW_MIXER  | REDRAW_FOCUS |
    REDRAW_PLAYER },
  { BAR_BOTTOM,  2,    ncmpcpp_footer,  REDRAW_PLAYER | REDRAW_TICK },
};

static inline Rect
layout_frame(WINDOW *win, bool focused)
{
  (void)focused;
  return (Rect){ 0, 0, getmaxy(win), getmaxx(win) };
}

static inline void
layout_arrange(Rect area, int active, Rect *out, int n)
{
  int i;

  for (i = 0; i < n; i++)
    out[i] = (i == active) ? area : (Rect){ 0, 0, 0, 0 };
}

#endif /* LAYOUT_NCMPCPP_H */
