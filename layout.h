#ifndef LAYOUT_H
#define LAYOUT_H

#include <ncurses.h>
#include <stdbool.h>

/* everything a layout and config.def.h need to describe the window set.
 * See docs/layouts.md for more info */

typedef struct {
  int y, x, h, w;
} Rect;

/* which edge a sidebar is pinned to. Each bar uses remaining space from
 * previous bars so ordering matters.
 */
enum {
  BAR_TOP,
  BAR_BOTTOM,
  BAR_LEFT,
  BAR_RIGHT,
};

#define PCT(n) (-(n))

/* resons a window might need repainting. A bar or pane declares the subset it
 * cares about and reef repaints on the overlap */
enum {
  REDRAW_PLAYER = 1 << 0,   /* play/pause/seek/song change */
  REDRAW_QUEUE = 1 << 1,    /* queue contents changed */
  REDRAW_DATABASE = 1 << 2, /* database updated */
  REDRAW_MIXER = 1 << 3,    /* volume changed */
  REDRAW_TICK = 1 << 4,     /* elapsed time advanced */
  REDRAW_FOCUS = 1 << 5,    /* the focused tab changed */
  REDRAW_KEYPRESS = 1 << 6, /* user pressed a key */
};

#define REDRAW_ALL (~0u)

/* pinned to one edge of the area that previous bars left */
typedef struct {
  int edge;
  int size;
  void (*draw)(WINDOW *win);
  unsigned redraw_on;
} Bar;

/* a pane in the content area the bars left. The layout's arrange() places it
 * and decides whether it is visible, so a pane never computes its own
 * geometry. */
typedef struct {
  const char *name;
  void (*draw)(WINDOW *win);
  unsigned redraw_on;
} Pane;

#endif /* LAYOUT_H */
