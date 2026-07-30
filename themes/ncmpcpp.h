#pragma once

/*
 * AUTHOR: Kreedy (https://git.gay/kreedy) / (https://github.com/kreedyy)
 */

#include "theme.h"

#define COLOR_RED_             0xff0000
#define COLOR_GREEN_           0x00ff00
#define COLOR_YELLOW_          0xffff00
#define COLOR_MAGENTA_         0xff00ff
#define COLOR_CYAN_            0x00ffff
#define COLOR_WHITE_           0xffffff
#define COLOR_GREY_            0x808080

#ifndef COLOR_DEFAULT
#define COLOR_DEFAULT          COLOR_YELLOW_
#endif

#ifndef COLOR_BORDER
#define COLOR_BORDER           COLOR_GREY_
#endif

#ifndef COLOR_BORDER_FOCUSED
#define COLOR_BORDER_FOCUSED   COLOR_GREEN_
#endif

#ifndef COLOR_TITLE
#define COLOR_TITLE            COLOR_WHITE_
#endif

#ifndef COLOR_ARTIST
#define COLOR_ARTIST           COLOR_MAGENTA_
#endif

#ifndef COLOR_ALBUM
#define COLOR_ALBUM            COLOR_CYAN_
#endif

#ifndef COLOR_TRACK
#define COLOR_TRACK            COLOR_DEFAULT
#endif


#ifndef COLOR_STATE_PLAYER
#define COLOR_STATE_PLAYER     TRANSPARENT
#endif

#ifndef COLOR_STATE_MODES_ON
#define COLOR_STATE_MODES_ON   COLOR_MAGENTA_
#endif

#ifndef COLOR_STATE_MODES_OFF
#define COLOR_STATE_MODES_OFF  COLOR_GREY_
#endif

#ifndef COLOR_TAB
#define COLOR_TAB              TRANSPARENT
#endif

#ifndef COLOR_TAB_ACTIVE
#define COLOR_TAB_ACTIVE       TRANSPARENT
#endif

#ifndef COLOR_PROGRESS
#define COLOR_PROGRESS         COLOR_GREEN_
#endif

#ifndef COLOR_TIME
#define COLOR_TIME             COLOR_MAGENTA_
#endif

#ifndef COLOR_VOLUME
#define COLOR_VOLUME           TRANSPARENT
#endif

#ifndef COLOR_COLUMN_HEADER
#define COLOR_COLUMN_HEADER    COLOR_YELLOW_
#endif

#ifndef COLOR_KEYBIND
#define COLOR_KEYBIND          TRANSPARENT
#endif

#ifndef COLOR_ERROR
#define COLOR_ERROR            COLOR_RED_
#endif

#ifndef COLOR_ACTIVE
#define COLOR_ACTIVE           COLOR_YELLOW_
#endif

#ifndef COLOR_HIGHLIGHT
#define COLOR_HIGHLIGHT        COLOR_CYAN_
#endif

#ifndef COLOR_BACKGROUND
#define COLOR_BACKGROUND       TRANSPARENT
#endif

#ifndef COLOR_HIGHLIGHT_BG
#define COLOR_HIGHLIGHT_BG     TRANSPARENT
#endif

#ifndef COLOR_ACTIVE_BG
#define COLOR_ACTIVE_BG        TRANSPARENT
#endif

static const Style theme[STYLE_COUNT] = {
  /* slot                     fg                     bg                  attribute */
  [STYLE_DEFAULT]         = { COLOR_DEFAULT,         COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_BORDER]          = { COLOR_BORDER,          COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_BORDER_FOCUSED]  = { COLOR_BORDER_FOCUSED,  COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_TITLE]           = { COLOR_TITLE,           COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_ARTIST]          = { COLOR_ARTIST,          COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_TRACK]           = { COLOR_TRACK,           COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_ALBUM]           = { COLOR_ALBUM,           COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_TIME]            = { COLOR_TIME,            COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_STATE_PLAYER]    = { COLOR_STATE_PLAYER,    COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_STATE_MODES_ON]  = { COLOR_STATE_MODES_ON,  COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_STATE_MODES_OFF] = { COLOR_STATE_MODES_OFF, COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_TAB]             = { COLOR_TAB,             COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_TAB_ACTIVE]      = { COLOR_TAB_ACTIVE,      COLOR_BACKGROUND,   A_REVERSE  },
  [STYLE_PROGRESS]        = { COLOR_PROGRESS,        COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_VOLUME]          = { COLOR_VOLUME,          COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_COLUMN_HEADER]   = { COLOR_COLUMN_HEADER,   COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_KEYBIND]         = { COLOR_KEYBIND,         COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_ERROR]           = { COLOR_ERROR,           COLOR_BACKGROUND,   A_NORMAL   },
  [STYLE_HIGHLIGHT]       = { COLOR_HIGHLIGHT,       COLOR_HIGHLIGHT_BG, A_REVERSE  },
  [STYLE_ACTIVE]          = { COLOR_ACTIVE,          COLOR_ACTIVE_BG,    A_REVERSE  },
};
