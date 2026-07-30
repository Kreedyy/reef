#pragma once

#include <ncurses.h>

/* don't paint this cell, the terminal's own color shows through meaning
 * transparent terminals are respected. It is 0 so an unset slot gets
 * set as transparent rather than black, which
 * means black must be written as 0x010101 */
#define TRANSPARENT 0x000000

typedef struct {
  unsigned fg;
  unsigned bg;
  int attr;
} Style;

enum {
  STYLE_DEFAULT,
  STYLE_BORDER,
  STYLE_BORDER_FOCUSED,
  STYLE_TITLE,
  STYLE_ARTIST,
  STYLE_ALBUM,
  STYLE_STATE_PLAYER,
  STYLE_STATE_MODES_ON,
  STYLE_STATE_MODES_OFF,
  STYLE_TAB,
  STYLE_TAB_ACTIVE,
  STYLE_PROGRESS,
  STYLE_TIME,
  STYLE_VOLUME,
  STYLE_COLUMN_HEADER,
  STYLE_KEYBIND,
  STYLE_ERROR,
  STYLE_HIGHLIGHT,

  STYLE_COUNT
};
