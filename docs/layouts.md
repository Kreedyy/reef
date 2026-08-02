# Layouts

A layout owns the screen geometry: which bars exist, their sizes, and how the
leftover area is split among the panes in `tabs[]`.

Select one from `config.h`:

    #include "layouts/ncmpcpp.h"

- `ncmpcpp.h` [ncmpcpp](https://github.com/ncmpcpp/ncmpcpp) clone
- `rmpc.h` [rmpc](https://github.com/mierak/rmpc) clone

## Creating

The reference implementation is `layouts/example.h`. It tiles five panes at
once and the stock `tabs[]` only has four, so one cell starts out empty.  
It shows how a layout behaves both with and without patches that
contribute extra tabs.

To understand how this works, change layout to `#include "layouts/example.h"`
and rebuild. After you've seen how it tiles tabs,  
try enabling the `example` patch in `config.local.mk` and rebuilding again.

### bars[]

```c
/* example.h */
static const Bar bars[] = {
  /* edge        size      draw            redraw on                     */
  { BAR_TOP,     2,        example_header, REDRAW_PLAYER | REDRAW_MIXER  },
  { BAR_BOTTOM,  1,        example_footer, REDRAW_PLAYER | REDRAW_TICK   },
  { BAR_BOTTOM,  2,        example_header, REDRAW_PLAYER | REDRAW_MIXER  },
  { BAR_LEFT,    16,       example_left,   REDRAW_FOCUS                  },
  { BAR_RIGHT,   PCT(20),  example_right,  REDRAW_MIXER | REDRAW_TICK    },
};
```

Each bar is carved off the area the previous ones left **in list order**, so
the sidebars only span what's still free between the header and footer, and
the pane grid only gets what the sidebars leave.  
List bars outermost first as they will be closer to the terminal edge.  
For example first `BAR_BOTTOM` ends up lowest, the first `BAR_TOP` highest and so on.

`size` is in cells (rows for `BAR_TOP`/`BAR_BOTTOM`, columns for
`BAR_LEFT`/`BAR_RIGHT`), or `PCT(n)` for a percentage of the axis.

`redraw_on` is the set of events that repaint the bar.

Example from above:  
The header shows player state, title and volume `REDRAW_PLAYER | REDRAW_MIXER`.  
The footer animates the progress bar, so it adds `REDRAW_TICK`.  
The left sidebar just lists the tabs, so `REDRAW_FOCUS` is enough.

A bar repaints only when its mask overlaps the event.  
For more details see [Redraw flags](#redraw-flags).

### arrange()

Every layout must include the `layout_arrange()` function. `ui.c` uses it
to stay agnostic and keep all drawing related stuff to itself.  
This function defines when and where to draw panes,
`ui.c` and patches handle what they draw.

```c
static inline void layout_arrange(Rect area, int active, Rect *out, int n);
```

`area` is what the bars left. Fill `out[0..n]` with a rect per pane, a
zero area rect hides that pane.  
reef draws **every** pane whose rect is
non-empty, so this is also how you show several panes at once.

The simplest `layout_arrange()` example is `ncmpcpp.h`, one pane
fills `area` and the rest are hidden:

```c
/* ncmpcpp.h */
static inline void layout_arrange(Rect area, int active, Rect *out, int n) {
  for (int i = 0; i < n; i++)
    out[i] = (i == active) ? area : (Rect){ 0, 0, 0, 0 };
}
```

`example.h` is instead a grid that tiles up to `MAX_TILES` number of panes.
Any other tab gets placed in a separate fullscreen pane like the ncmpcpp
layout does.

```c
/* example.h */
#define MAX_TILES 5

static inline void layout_arrange(Rect area, int active, Rect *out, int n) {
  for (int i = 0; i < n; i++)
    out[i] = (Rect){ 0, 0, 0, 0 };

  /* a tab past the grid has no cell, so it fills the whole area */
  if (active >= MAX_TILES) {
    if (active < n)
      out[active] = area;
    return;
  }

  int left_w  = area.w / 3;
  int right_w = area.w / 3;
  int mid_w   = area.w - left_w - right_w; /* remainder keeps the columns exact */
  int top_h   = area.h / 2;
  int mid_x   = area.x + left_w;
  int right_x = mid_x + mid_w;

  Rect cells[] = {
    { area.y,         area.x,  top_h,          left_w  }, /* top left     */
    { area.y + top_h, area.x,  area.h - top_h, left_w  }, /* bottom left  */
    { area.y,         mid_x,   area.h,         mid_w   }, /* middle        */
    { area.y,         right_x, top_h,          right_w }, /* top right    */
    { area.y + top_h, right_x, area.h - top_h, right_w }, /* bottom right */
  };

  for (int i = 0; i < n && i < MAX_TILES; i++)
    out[i] = cells[i];
}
```

**Write it in terms of `n`,** never a fixed pane count as patches may add panes.  
Each pane inside the grid is always visible and `active` only picks which
border `layout_frame()` highlights, on the fullscreen tab `active` selects
the single visible pane.  
See [frame()](#frame).

#### Removing bars on a tab

A layout can define `LAYOUT_BAR_HIDDEN(bar, active)` as reef calls it for
every bar and drops the ones it returns nonzero for, reclaiming their space.  
See `example_bar_hidden()` in [`example.h`](../layouts/example.h), which uses it to hide a bar on any
fullscreen tab. The gating logic is from your layout.

### frame()

Every layout must include the `layout_frame()` function. `ui.c` uses it
to stay agnostic and keep all drawing related stuff to itself.  
This function handles styling the layout, such as borders.

```c
static inline Rect layout_frame(WINDOW *win, bool focused);
```

Called on a pane's erased window before it draws. Draw any chrome, return the
rect the pane may use.  
The pane is handed exactly that area and never learns
whether you drew a border.  
`example.h` boxes every pane, brighter border on the
focused one:

```c
static inline Rect layout_frame(WINDOW *win, bool focused) {
  int slot = focused ? STYLE_BORDER_FOCUSED : STYLE_BORDER;
  style_on(win, slot);
  box(win, 0, 0);
  style_off(win, slot);
  return (Rect){ 1, 1, getmaxy(win) - 2, getmaxx(win) - 2 };
}
```

Returning the inset rect is what lets each grid cell show its own border.
A split layout can't paint the gutter between panes, so the borders are the
separation.  
Return the whole window for no chrome, as `ncmpcpp.h` does.

### Drawing

From `ui.h`:

```c
/* used for navigating left/right .i = -1 / .i = 1 */
void nav(const Arg *arg);
/* used for navigating up/down    .i = -1 / .i = 1 */
void cursor_move(const Arg *arg);
/* moves cursor a page up/down    .i = -1 / .i = 1 */
void cursor_page(const Arg *arg);
/* moves cursor to top/bottom     .i = -1 / .i = 1 */
void cursor_edge(const Arg *arg);

int active_tab(void);
int tab_count(void);
const char *tab_name(int index);

/* returns index of tab with a specific drawing function, or -1 */
int  tab_with_draw(void (*draw)(WINDOW *win));

/* check if a tab with a specific drawing function is focused */
bool tab_active(void (*draw)(WINDOW *win));

attr_t style(int slot);
void   style_on(WINDOW *win, int slot);
void   style_off(WINDOW *win, int slot);
attr_t style_custom(const Style *custom);

int  text_width(const char *text);
void draw_text(WINDOW *win, int row, int x, int width, const char *text);
void draw_text_centered(WINDOW *win, int row, int x, int width, const char *text);
void draw_text_right(WINDOW *win, int row, int x, int width, const char *text);

void draw_progress_bar(WINDOW *win, int row, int width);
void draw_volume(WINDOW *win, int row, int width);       /* right-aligned */
void draw_elapsed(WINDOW *win, int row, int width);      /* right-aligned */
int  elapsed_width(void);                                /* columns it needs */
void draw_modes(WINDOW *win, int row, int x, int width); /* the [zxcv] flags */
int  modes_width(void);                                  /* columns it needs */
bool mpd_status_line(WINDOW *win, int row);
```

Also stuff from `mpd.h`, the `STYLE_*` slots, the theme's `COLOR_*` names
([themes.md](themes.md)), and `padding_left` / `padding_right` /
`progress_bar_head` / `progress_bar_tail` from `config.h` can be used.

Any status bar starts with `if (mpd_status_line(win, row)) return;`
otherwise a disconnect freezes the UI with no message.

ALWAYS draw with `draw_text*()` and measure with `text_width()`,
not `mvwaddstr` + `strlen`.  
`draw_text*()` use columns and never paint
outside `[x, x + width]`, so unusual characters cannot run into the next column.  
See [patches.md](patches.md#drawing-text) for more info.

Color via `style_on()`/`style_off()`, not `wattron()`/`wattroff`, so themes can
restyle you.  
See [themes.md](themes.md#patches-that-need-colors) for more info.

### Redraw flags

A bar or pane repaints when its `redraw_on` overlaps the event.  
Flags in
`layout.h`:

REDRAW_PLAYER   -> play/pause/seek/song change  
REDRAW_QUEUE    -> queue changed  
REDRAW_DATABASE -> database updated  
REDRAW_MIXER    -> volume changed  
REDRAW_TICK     -> elapsed time advanced (every 250ms)  
REDRAW_FOCUS    -> focused tab changed  
REDRAW_KEYPRESS -> user pressed a key  

Some are tied to what mpd reports back from the idle thread, others from user
input.  
Some things like song elapsed time do not get reported so REDRAW_TICK
is needed.  
REDRAW_TICK updates every 250ms so avoid using this if pawsible.

## Notes

Layout headers compile into several files, so every function must be
`static inline`.

As opposed to `tabs[]`, `bars[]` is the layout's own, so a patch cannot
contribute a bar.  
Wiring one in is the layout's job, both the
entry and the `#include` of that patch's header belong inside an
`#ifdef PATCH_<name>` guard.  
See [patches.md](patches.md).
