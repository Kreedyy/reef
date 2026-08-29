#ifndef THEME_REEF_H
#define THEME_REEF_H

/*
 * AUTHOR: Kreedy (https://git.gay/kreedy) / (https://github.com/kreedyy)
 */

#include "theme.h"

#define COLOR_GREEN_            0x33dea5
#define COLOR_BLUE_LIGHT_       0x2b486d
#define COLOR_BLUE_DARK_        0xbac2de
#define COLOR_GREY_             0xcdd6f4
#define COLOR_GREY_SHADE_       0x45475a
#define COLOR_RED_              0xf38ba8

#define COLOR_DEFAULT          COLOR_BLUE_DARK_
#define COLOR_BORDER           COLOR_GREY_SHADE_
#define COLOR_BORDER_FOCUSED   COLOR_GREEN_
#define COLOR_TITLE            COLOR_GREY_
#define COLOR_ARTIST           COLOR_GREEN_
#define COLOR_TRACK            COLOR_DEFAULT
#define COLOR_ALBUM            COLOR_BLUE_LIGHT_
#define COLOR_STATE_PLAYER     COLOR_BLUE_LIGHT_
#define COLOR_STATE_MODES_ON   COLOR_GREEN_
#define COLOR_STATE_MODES_OFF  COLOR_BLUE_LIGHT_
#define COLOR_TAB              COLOR_GREEN_
#define COLOR_TAB_ACTIVE       COLOR_GREEN_
#define COLOR_PROGRESS         COLOR_GREEN_
#define COLOR_TIME             COLOR_GREEN_
#define COLOR_VOLUME           COLOR_GREEN_
#define COLOR_COLUMN_HEADER    COLOR_GREEN_
#define COLOR_KEYBIND          COLOR_BLUE_LIGHT_
#define COLOR_ERROR            COLOR_RED_
#define COLOR_HIGHLIGHT        COLOR_GREEN_
#define COLOR_ACTIVE           COLOR_DEFAULT
#define COLOR_BACKGROUND       TRANSPARENT
#define COLOR_HIGHLIGHT_BG     COLOR_BLUE_LIGHT_
#define COLOR_ACTIVE_BG        TRANSPARENT

static const Style theme[STYLE_COUNT] = {
  /* slot                     fg                     bg                  attribute */
  [STYLE_DEFAULT]         = { COLOR_DEFAULT,         COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_BORDER]          = { COLOR_BORDER,          COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_BORDER_FOCUSED]  = { COLOR_BORDER_FOCUSED,  COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_TITLE]           = { COLOR_TITLE,           COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_ARTIST]          = { COLOR_ARTIST,          COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_TRACK]           = { COLOR_TRACK,           COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_ALBUM]           = { COLOR_ALBUM,           COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_TIME]            = { COLOR_TIME,            COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_STATE_PLAYER]    = { COLOR_STATE_PLAYER,    COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_STATE_MODES_ON]  = { COLOR_STATE_MODES_ON,  COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_STATE_MODES_OFF] = { COLOR_STATE_MODES_OFF, COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_TAB]             = { COLOR_TAB,             COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_TAB_ACTIVE]      = { COLOR_TAB_ACTIVE,      COLOR_BACKGROUND,   A_REVERSE },
  [STYLE_PROGRESS]        = { COLOR_PROGRESS,        COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_VOLUME]          = { COLOR_VOLUME,          COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_COLUMN_HEADER]   = { COLOR_COLUMN_HEADER,   COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_KEYBIND]         = { COLOR_KEYBIND,         COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_ERROR]           = { COLOR_ERROR,           COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_HIGHLIGHT]       = { COLOR_HIGHLIGHT,       COLOR_HIGHLIGHT_BG, A_NORMAL  },
  [STYLE_ACTIVE]          = { COLOR_ACTIVE,          COLOR_ACTIVE_BG,    A_REVERSE },
};

#endif /* THEME_REEF_H */
