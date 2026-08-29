#ifndef THEME_NCMPCPP_H
#define THEME_NCMPCPP_H

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

#define COLOR_DEFAULT          COLOR_YELLOW_
#define COLOR_BORDER           COLOR_GREY_
#define COLOR_BORDER_FOCUSED   COLOR_GREEN_
#define COLOR_TITLE            COLOR_WHITE_
#define COLOR_ARTIST           COLOR_MAGENTA_
#define COLOR_ALBUM            COLOR_CYAN_
#define COLOR_TRACK            COLOR_DEFAULT
#define COLOR_STATE_PLAYER     TRANSPARENT
#define COLOR_STATE_MODES_ON   COLOR_MAGENTA_
#define COLOR_STATE_MODES_OFF  COLOR_GREY_
#define COLOR_TAB              TRANSPARENT
#define COLOR_TAB_ACTIVE       TRANSPARENT
#define COLOR_PROGRESS         COLOR_GREEN_
#define COLOR_TIME             COLOR_MAGENTA_
#define COLOR_VOLUME           TRANSPARENT
#define COLOR_COLUMN_HEADER    COLOR_YELLOW_
#define COLOR_KEYBIND          TRANSPARENT
#define COLOR_ERROR            COLOR_RED_
#define COLOR_ACTIVE           COLOR_YELLOW_
#define COLOR_HIGHLIGHT        COLOR_CYAN_
#define COLOR_BACKGROUND       TRANSPARENT
#define COLOR_HIGHLIGHT_BG     TRANSPARENT
#define COLOR_ACTIVE_BG        TRANSPARENT

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

#endif /* THEME_NCMPCPP_H */
