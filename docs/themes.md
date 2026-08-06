# Themes

Layouts reference slots by name, so any theme works with any layout.

Select one from `config.h`:

    #include "themes/reef.h"

- `reef.h` Signature theme by [Kreedy](https://git.gay/kreedy)
- `ncmpcpp.h` [ncmpcpp](https://github.com/ncmpcpp/ncmpcpp) clone
- `rmpc.h` [rmpc](https://github.com/mierak/rmpc) clone

## The colors

A theme defines one `COLOR_*` per `STYLE_*` slot in `theme.h`:  
`COLOR_TITLE` is what `STYLE_TITLE` draws with, `COLOR_ERROR` what `STYLE_ERROR`
draws with, and so on.

Keep `COLOR_BACKGROUND` transparent unless you want a solid background,  
which is not recommended for themes you wanna submit as themes should respect the terminal's background setting.

## The slot table

Each slot picks a color plus attributes:

```c
static const Style theme[STYLE_COUNT] = {
  /* slot                fg                bg                  attr */
  [STYLE_BORDER]     = { COLOR_BORDER,     COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_TITLE]      = { COLOR_TITLE,      COLOR_BACKGROUND,   A_NORMAL  },
  [STYLE_TAB_ACTIVE] = { COLOR_TAB_ACTIVE, COLOR_BACKGROUND,   A_REVERSE },
  [STYLE_HIGHLIGHT]  = { COLOR_HIGHLIGHT,  COLOR_HIGHLIGHT_BG, A_NORMAL  },
};
```

The full slot list is the `STYLE_*` enum in `theme.h`. Nothing forces the  
pairing, but the matching names are there to keep the table readable.  
Meaning if you want to reuse a color across multiple `STYLE_*` options you  
should copy paste the same color for the respective `COLOR_*`s rather than  
doing i.e. `[STYLE_BORDER] = { COLOR_TITLE }`.

## Attributes

`attr` is one or more attributes OR'd together (`A_REVERSE | A_UNDERLINE`):

| attribute     | effect                                        |
|---------------|-----------------------------------------------|
| `A_NORMAL`    | nothing                                       |
| `A_REVERSE`   | swap foreground and background                |
| `A_UNDERLINE` | underline                                     |
| `A_DIM`       | reduced brightness                            |
| `A_ITALIC`    | italic                                        |
| `A_STANDOUT`  | terminal's "best" highlight (usually reverse) |
| `A_BLINK`     | blinking text                                 |

These are ncurses attributes and anything ncurses defines technically works,  
the table is just the useful subset that has been tested.

`A_STANDOUT` works the same as `A_REVERSE` from my understanding and testing.

## Colors and the terminal

Colors are 24-bit hex, `0xRRGGBB`. Exact 24-bit rendering needs a direct-color  
`TERM`/`COLORTERM` but xterm-256color and xterm-16color are supported.

`TRANSPARENT` means *do not paint*, meaning the terminal's own color (and its transparency) shows through.  
It is `0x000000` so an unset slot falls back to
that. This means you must write pure black differently, i.e. `0x010101`.

## Adding a theme

Copy a file in `themes/`, retune the colors, point `config.h` at it. Touch the
slot table only to change an *attribute*.

## Patches that need colors

In order of preference:

1. **Reuse a slot.** Slots are named by meaning, so use the one closest to what you need.
2. **A theme color.** You can build a `Style` from the theme's `COLOR_*` names and draw with `style_custom()`.  
This lets you define a custom style within your patch for easier use and extended customizability,  
while still following whichever theme is selected:

   ```c
   static const Style accent = { COLOR_ARTIST, COLOR_BACKGROUND, A_BLINK | A_UNDERLINE | A_ITALIC };
   attr_t a = style_custom(&accent);
   wattr_on(win, a, NULL);
   draw_text(win, 0, 0, getmaxx(win), "Log in");
   wattr_off(win, a, NULL);
   ```

   `style_custom()` allocates a pair on first use and reuses it, so no
   coordination with `theme.h` is needed.

   Call it while drawing, not once into a global. Before the theme is
   initialised it has no pair to hand out and returns the attributes alone, so a
   result cached that early stays colorless for the rest of the run.
3. **Raw hex.** This is not theme-able so avoid using hardcoded colors.
