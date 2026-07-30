#pragma once

#include <ncurses.h>

#include "layout.h"
#include "mpd.h"
#include "types.h"
#include "ui.h"

#define MAX_TILES 5

static inline void
example_header(WINDOW *win)
{
  int width = getmaxx(win);
  const char *state;
  char title[512];
  int line_row, mw, mx;

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

  if (get_artist()[0])
    snprintf(title, sizeof(title), "%s  -  %s", get_artist(),
        get_title());
  else
    snprintf(title, sizeof(title), "%s", get_title());

  style_on(win, STYLE_STATE_PLAYER);
  draw_text(win, 0, padding_left, width, state);
  style_off(win, STYLE_STATE_PLAYER);

  style_on(win, STYLE_TITLE);
  draw_text(win, 0, padding_left + text_width(state) + 1, width, title);
  style_off(win, STYLE_TITLE);

  draw_volume(win, 0, width);

  line_row = getmaxy(win) - 1;
  style_on(win, STYLE_BORDER);
  mvwhline(win, line_row, 0, 0, width);
  style_off(win, STYLE_BORDER);

  mw = modes_width();
  mx = width - padding_right - mw;
  if (mx < 0)
    mx = 0;
  draw_modes(win, line_row, mx, mw);
}

static inline void
example_footer(WINDOW *win)
{
  int width = getmaxx(win);

  if (mpd_status_line(win, 0))
    return;

  draw_progress_bar(win, 0, width - elapsed_width() - padding_right - 1);
  draw_elapsed(win, 0, width);
}

static inline void
example_left(WINDOW *win)
{
  int width = getmaxx(win);
  int i;

  for (i = 0; i < tab_count(); i++) {
    int slot = (i == active_tab()) ? STYLE_TAB_ACTIVE : STYLE_TAB;

    style_on(win, slot);
    draw_text(win, i, padding_left, width - padding_left - 1,
        tab_name(i));
    style_off(win, slot);
  }

  style_on(win, STYLE_BORDER);
  mvwvline(win, 0, width - 1, 0, getmaxy(win));
  style_off(win, STYLE_BORDER);
}

static inline void
example_right(WINDOW *win)
{
  int width = getmaxx(win);

  style_on(win, STYLE_BORDER);
  mvwvline(win, 0, 0, 0, getmaxy(win));
  style_off(win, STYLE_BORDER);

  draw_volume(win, 0, width);
  draw_elapsed(win, 1, width);
}

static const Bar bars[] = {
  /* edge        size     draw function   redraw condition */
  { BAR_TOP,     2,       example_header, REDRAW_PLAYER | REDRAW_MIXER },
  { BAR_BOTTOM,  1,       example_footer, REDRAW_PLAYER | REDRAW_TICK },
  { BAR_BOTTOM,  2,       example_header, REDRAW_PLAYER | REDRAW_MIXER },
  { BAR_LEFT,    16,      example_left,   REDRAW_FOCUS },
  { BAR_RIGHT,   PCT(20), example_right,  REDRAW_MIXER | REDRAW_TICK },
};

static inline bool
example_bar_hidden(int bar, int active)
{
  return bars[bar].draw == example_right && active >= MAX_TILES;
}

#define LAYOUT_BAR_HIDDEN(bar, active) example_bar_hidden(bar, active)

static inline Rect
layout_frame(WINDOW *win, bool focused)
{
  int slot = focused ? STYLE_BORDER_FOCUSED : STYLE_BORDER;

  style_on(win, slot);
  box(win, 0, 0);
  style_off(win, slot);
  return (Rect){ 1, 1, getmaxy(win) - 2, getmaxx(win) - 2 };
}

static inline void
layout_arrange(Rect area, int active, Rect *out, int n)
{
  int left_w, right_w, mid_w, top_h, mid_x, right_x;
  int i;

  for (i = 0; i < n; i++)
    out[i] = (Rect){ 0, 0, 0, 0 };

  if (active >= MAX_TILES) {
    if (active < n)
      out[active] = area;
    return;
  }

  left_w = area.w / 3;
  right_w = area.w / 3;
  mid_w = area.w - left_w - right_w;
  top_h = area.h / 2;
  mid_x = area.x + left_w;
  right_x = mid_x + mid_w;

  {
    Rect cells[] = {
      { area.y, area.x, top_h, left_w },                    /* top left */
      { area.y + top_h, area.x, area.h - top_h, left_w },   /* bottom left */
      { area.y, mid_x, area.h, mid_w },                     /* middle */
      { area.y, right_x, top_h, right_w },                  /* top right */
      { area.y + top_h, right_x, area.h - top_h, right_w }, /* bottom right */
    };

    for (i = 0; i < n && i < MAX_TILES; i++)
      out[i] = cells[i];
  }
}
