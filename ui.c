#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <ncurses.h>
#include <panel.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utf8proc.h>
#include <wchar.h>

#include "config.h"
#include "keybinds.h"
#include "layout.h"
#include "lyrics.h"
#include "mpd.h"
#include "theme.h"
#include "types.h"
#include "ui.h"

#define LENGTH(X) (sizeof(X) / sizeof((X)[0]))

#define BAR_COUNT ((int)LENGTH(bars))
#define TAB_COUNT ((int)LENGTH(tabs))

static WINDOW *bar_windows[LENGTH(bars)];
static PANEL *bar_panels[LENGTH(bars)];

static WINDOW *tab_windows[LENGTH(tabs)];
static PANEL *tab_panels[LENGTH(tabs)];

const char *keybind_hint_string(void);

#if SHOW_KEYBIND_BAR
static WINDOW *keybind_window;
static PANEL *keybind_panel;

#define KEYBIND_LINE_MAX 256

static int
keybind_rows(int width, char out[][KEYBIND_LINE_MAX]) {
  const char *text = keybind_hint_string();
  const char *p;
  int usable, row = 0, used = 0;

  usable = width - padding_left - padding_right;
  if (usable < 1)
    usable = 1;

  if (out)
    out[0][0] = '\0';

  for (p = text; *p;) {
    const char *sep;
    int len, gap;

    while (*p == ' ')
      p++;
    if (!*p)
      break;

    sep = strstr(p, "  ");
    len = sep ? (int)(sep - p) : (int)strlen(p);
    if (len > KEYBIND_LINE_MAX - 1)
      len = KEYBIND_LINE_MAX - 1;

    gap = used ? 2 : 0;
    if (used && used + gap + len > usable) {
      if (row + 1 >= KEYBIND_BAR_MAX_ROWS)
        break;
      row++;
      used = 0;
      gap = 0;
      if (out)
        out[row][0] = '\0';
    }

    if (out) {
      if (gap)
        strncat(out[row], "  ", KEYBIND_LINE_MAX - strlen(out[row]) - 1);

      strncat(out[row], p, (size_t)len);
    }
    used += gap + len;

    p = sep ? sep + 2 : p + len;
  }

  return row + 1;
}
#endif

static int active_index;

static void apply_layout(void);
static void browser_invalidate(void);
static const char *find_prompt(void);

static int style_pair[STYLE_COUNT];
static bool color_ready;

#define CUSTOM_PAIR_MAX 64
static struct {
  unsigned fg, bg;
  int pair;
} custom_pair[CUSTOM_PAIR_MAX];
static int custom_pair_count;

static int
channel_distance(int r1, int g1, int b1, int r2, int g2, int b2) {
  int dr = r1 - r2, dg = g1 - g2, db = b1 - b2;

  return dr * dr + dg * dg + db * db;
}

/* xterm defaults, used only to pick a nearest match on 8/16 color terms */
static const unsigned ansi16[16] = {
  0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080,
  0x008080, 0xc0c0c0, 0x808080, 0xff0000, 0x00ff00, 0xffff00,
  0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,
};

static int
nearest_ansi(int r, int g, int b, int count) {
  int i, best = 0, best_distance = -1;

  for (i = 0; i < count; i++) {
    int distance = channel_distance(
      r, g, b, (ansi16[i] >> 16) & 0xFF,
      (ansi16[i] >> 8) & 0xFF, ansi16[i] & 0xFF);

    if (best_distance < 0 || distance < best_distance) {
      best_distance = distance;
      best = i;
    }
  }
  return best;
}

static int
nearest_256(int r, int g, int b) {
  static const int level[6] = { 0, 95, 135, 175, 215, 255 };

  int index[3];
  int channel[3] = { r, g, b };
  int c, i, cube, cube_distance, step, grey, grey_distance;

  for (c = 0; c < 3; c++) {
    index[c] = 0;

    for (i = 1; i < 6; i++)
      if (abs(channel[c] - level[i]) < abs(channel[c] - level[index[c]]))
        index[c] = i;
  }

  cube = 16 + 36 * index[0] + 6 * index[1] + index[2];
  cube_distance = channel_distance(r, g, b, level[index[0]], level[index[1]],
                                   level[index[2]]);

  step = ((r + g + b) / 3 - 8) / 10;
  if (step < 0)
    step = 0;
  if (step > 23)
    step = 23;
  grey = 8 + 10 * step;
  grey_distance = channel_distance(r, g, b, grey, grey, grey);

  return grey_distance < cube_distance ? 232 + step : cube;
}

static int
resolve_color(unsigned rgb) {
  int r, g, b;

  if (rgb == TRANSPARENT)
    return -1;

  r = (rgb >> 16) & 0xFF;
  g = (rgb >> 8) & 0xFF;
  b = rgb & 0xFF;

  if (COLORS >= 0x1000000)
    return (int)rgb;
  if (COLORS >= 256)
    return nearest_256(r, g, b);
  if (COLORS >= 16)
    return nearest_ansi(r, g, b, 16);
  if (COLORS >= 8)
    return nearest_ansi(r, g, b, 8);
  return -1;
}

static void
init_theme(void) {
  int i;

  if (!has_colors())
    return;

  start_color();
  use_default_colors();

  for (i = 0; i < STYLE_COUNT; i++) {
    int fg = resolve_color(theme[i].fg);
    int bg = resolve_color(theme[i].bg);

    init_extended_pair(i + 1, fg, bg);

    style_pair[i] = i + 1;
  }

  color_ready = true;
}

attr_t
style(int slot) {
  attr_t attr = theme[slot].attr;

  return color_ready ? attr | COLOR_PAIR(style_pair[slot]) : attr;
}

/* for colors a patch owns outright, which no theme knows to define.
 * Allocates a pair on first use and reuses it after */
attr_t
style_custom(const Style *custom) {
  int i, pair;

  if (!color_ready)
    return custom->attr;

  for (i = 0; i < custom_pair_count; i++)
    if (custom_pair[i].fg == custom->fg &&
      custom_pair[i].bg == custom->bg)
      return custom->attr | COLOR_PAIR(custom_pair[i].pair);

  if (custom_pair_count >= CUSTOM_PAIR_MAX)
    return custom->attr;

  pair = STYLE_COUNT + 1 + custom_pair_count;
  init_extended_pair(pair, resolve_color(custom->fg),
                     resolve_color(custom->bg));

  custom_pair[custom_pair_count].fg = custom->fg;
  custom_pair[custom_pair_count].bg = custom->bg;
  custom_pair[custom_pair_count].pair = pair;
  custom_pair_count++;

  return custom->attr | COLOR_PAIR(pair);
}

void
style_on(WINDOW *win, int slot) {
  wattr_on(win, style(slot), NULL);
}

void
style_off(WINDOW *win, int slot) {
  wattr_off(win, style(slot), NULL);
}

void
init_ncurses(void) {
  int i;

  setlocale(LC_ALL, "");

  initscr();
  init_theme();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  set_escdelay(25);
  mouseinterval(0);
  curs_set(0);

  for (i = 0; i < BAR_COUNT; i++) {
    bar_windows[i] = newwin(1, 1, 0, 0);
    bar_panels[i] = new_panel(bar_windows[i]);
  }
  for (i = 0; i < TAB_COUNT; i++) {
    tab_windows[i] = newwin(1, 1, 0, 0);
    tab_panels[i] = new_panel(tab_windows[i]);
  }

#if SHOW_KEYBIND_BAR
  keybind_window = newwin(1, 1, 0, 0);
  keybind_panel = new_panel(keybind_window);
#endif

  active_index = (starting_tab >= 0 && starting_tab < TAB_COUNT) ?
    starting_tab : 0;

  apply_layout();
  ui_redraw(REDRAW_ALL);
}

void
destroy_ncurses(void) {
  endwin();
}


/* resize and move a panel in one go, a zero area rect hides it instead */
static void
place(PANEL *panel, WINDOW **win, Rect r) {
  WINDOW *old = *win, *fresh;

  if (r.h <= 0 || r.w <= 0) {
    hide_panel(panel);
    return;
  }

  /* wresize() corrupts heap here on resize spam for some reason, so this
   * is why we rebuild it instead */
  fresh = newwin(r.h, r.w, r.y, r.x);
  if (fresh == NULL)
    return;

  replace_panel(panel, fresh);
  *win = fresh;
  if (old != NULL)
    delwin(old);
  show_panel(panel);
}

/* resolve a bar size, negative means a percentage of the axis it eats into */
static int
bar_size(int size, int axis) {
  int n = size < 0 ? axis * -size / 100 : size;

  if (n < 0)
    n = 0;
  return n > axis ? axis : n;
}

/* carve each bar off the area the previous ones left, and return the remainder
 * for the panes */
static Rect
place_bars(int height, int width) {
  Rect area = { 0, 0, height, width };
  int i;

#if SHOW_KEYBIND_BAR
  {
    int n = keybind_rows(area.w, NULL);

    if (n > area.h)
      n = area.h;
    place(keybind_panel, &keybind_window,
          (Rect){ area.y + area.h - n, area.x, n, area.w });
    area.h -= n;
  }
#endif

  for (i = 0; i < BAR_COUNT; i++) {
    Rect r;
    int n;

    /* a layout may define LAYOUT_BAR_HIDDEN(bar, active) to drop a
     * bar for the focused tab. Hide it and leave its space for the
     * content rather than carving it. */
#ifdef LAYOUT_BAR_HIDDEN
    if (LAYOUT_BAR_HIDDEN(i, active_index)) {
      place(bar_panels[i], &bar_windows[i], (Rect){ 0, 0, 0, 0 });
      continue;
    }
#endif

    switch (bars[i].edge) {
      case BAR_TOP:
        n = bar_size(bars[i].size, area.h);
        r = (Rect){ area.y, area.x, n, area.w };
        area.y += n;
        area.h -= n;
        break;
      case BAR_BOTTOM:
        n = bar_size(bars[i].size, area.h);
        r = (Rect){ area.y + area.h - n, area.x, n, area.w };
        area.h -= n;
        break;
      case BAR_LEFT:
        n = bar_size(bars[i].size, area.w);
        r = (Rect){ area.y, area.x, area.h, n };
        area.x += n;
        area.w -= n;
        break;
      default: /* BAR_RIGHT */
        n = bar_size(bars[i].size, area.w);
        r = (Rect){ area.y, area.x + area.w - n, area.h, n };
        area.w -= n;
        break;
    }

    place(bar_panels[i], &bar_windows[i], r);
  }

  return area;
}

static void
apply_layout(void) {
  Rect rects[LENGTH(tabs)] = { { 0, 0, 0, 0 } };
  Rect content;
  int i, height, width;

  getmaxyx(stdscr, height, width);

  content = place_bars(height, width);
  layout_arrange(content, active_index, rects, TAB_COUNT);

  for (i = 0; i < TAB_COUNT; i++)
    place(tab_panels[i], &tab_windows[i], rects[i]);

  if (!panel_hidden(tab_panels[active_index]))
    top_panel(tab_panels[active_index]);
}

void
resize(void) {
  apply_layout();
  ui_redraw(REDRAW_ALL);
}

void
ui_redraw(unsigned flags) {
  int i;

#if SHOW_KEYBIND_BAR
  if ((flags & REDRAW_PLAYER) && !panel_hidden(keybind_panel)) {
    if (find_active()) {
      werase(keybind_window);
      style_on(keybind_window, STYLE_KEYBIND);
      draw_text(keybind_window, 0, padding_left,
                getmaxx(keybind_window) - padding_left,
                find_prompt());
      style_off(keybind_window, STYLE_KEYBIND);
    } else {
      char rows[KEYBIND_BAR_MAX_ROWS][KEYBIND_LINE_MAX];
      int count;

      if (keybind_rows(getmaxx(keybind_window), NULL) !=
        getmaxy(keybind_window))
        apply_layout();

      count = keybind_rows(getmaxx(keybind_window), rows);

      werase(keybind_window);
      style_on(keybind_window, STYLE_KEYBIND);
      for (i = 0; i < count && i < getmaxy(keybind_window);
        i++)
        draw_text(keybind_window, i, padding_left,
                  getmaxx(keybind_window) -
                  padding_left,
                  rows[i]);
      style_off(keybind_window, STYLE_KEYBIND);
    }
  }
#endif

  for (i = 0; i < BAR_COUNT; i++) {
    if (!(bars[i].redraw_on & flags) ||
      panel_hidden(bar_panels[i]))
      continue;
    werase(bar_windows[i]);
    bars[i].draw(bar_windows[i]);
  }

  for (i = 0; i < TAB_COUNT; i++) {
    WINDOW *outer, *inner;
    Rect in;

    if (!(tabs[i].redraw_on & flags) ||
      panel_hidden(tab_panels[i]))
      continue;

    outer = tab_windows[i];
    werase(outer);

    /* the layout draws whatever chrome it wants and tells us what
     * is left. The pane then gets a window that is exactly its
     * usable area, so pane draw functions never have to know
     * whether the layout uses borders. */
    in = layout_frame(outer, i == active_index);
    if (in.h <= 0 || in.w <= 0)
      continue;

    inner = derwin(outer, in.h, in.w, in.y, in.x);
    if (inner == NULL) {
      tabs[i].draw(outer);
      continue;
    }

    tabs[i].draw(inner);
    delwin(inner);
    touchwin(outer);
  }

  update_panels();
  doupdate();
}

void
ui_on_mpd_events(enum mpd_idle events) {
  unsigned flags = 0;

  if (events & MPD_IDLE_PLAYER)
    flags |= REDRAW_PLAYER;
  if (events & MPD_IDLE_QUEUE)
    flags |= REDRAW_QUEUE;
  if (events & MPD_IDLE_DATABASE)
    flags |= REDRAW_DATABASE;
  if (events & MPD_IDLE_MIXER)
    flags |= REDRAW_MIXER;
  if (events & MPD_IDLE_OPTIONS)
    flags |= REDRAW_PLAYER;

  if (events & MPD_IDLE_PLAYER)
    lyrics_prefetch();

  if (events & MPD_IDLE_DATABASE) {
    mpd_invalidate_library();
    browser_invalidate();
  }

  if (flags)
    ui_redraw(flags);
}

int
active_tab(void) {
  return active_index;
}

int
tab_count(void) {
  return TAB_COUNT;
}

const char *
tab_name(int index) {
  return tabs[index].name;
}

int
tab_with_draw(void (*draw)(WINDOW *win)) {
  int i;

  for (i = 0; i < TAB_COUNT; i++)
    if (tabs[i].draw == draw)
      return i;
  return -1;
}

bool
tab_active(void (*draw)(WINDOW *win)) {
  return tabs[active_index].draw == draw;
}

static void
focus_tab(int index) {
  active_index = index;
  apply_layout();

  clearok(curscr, TRUE);
  ui_redraw(REDRAW_ALL);
}

void
focus_next_tab(void) {
  focus_tab((active_index + 1) % TAB_COUNT);
}

void
focus_prev_tab(void) {
  focus_tab((active_index - 1 + TAB_COUNT) % TAB_COUNT);
}

void
focus_tab_by_key(int key) {
  int index = key - '1';
  if (index < 0 || index >= TAB_COUNT)
    return;
  focus_tab(index);
}

bool
mpd_status_line(WINDOW *win, int row) {
  const char *err;

  if (!mpd_error_active())
    return false;

  err = mpd_error();
  style_on(win, STYLE_ERROR);
  draw_text(win, row, padding_left, getmaxx(win) - padding_left,
            err[0] ? err : "mpd: disconnected");
  style_off(win, STYLE_ERROR);
  return true;
}

/* columns one code point takes. Must be the same wcwidth() ncurses uses.
 * ncurses decides which cells are dirty, so a cell we count differently is a
 * cell it never repaints. utf8proc has its own table and is used only for
 * clustering below, which libc cannot do. Unprintable counts as one but
 * draw_text() replaces those anyway. */
static int
char_width(utf8proc_int32_t c) {
  int w = wcwidth((wchar_t)c);

  return w < 0 ? 1 : w;
}

/* C0, DEL and C1. ncurses acts on \n, \r, \b and \t and renders the rest in
 * ^X notation, so either way one of these paints cells we did not budget for.
 * They must never reach the terminal. */
static bool
is_control(utf8proc_int32_t c) {
  return c < 0x20 || c == 0x7F || (c >= 0x80 && c <= 0x9F);
}

/* stands in for one byte we refuse to draw, replacement character */
static const char replacement[] = "\xEF\xBF\xBD"; /* U+FFFD */

/* is this an emoji base? U+200D only means "join into one emoji" next to one of
 * these. Elsewhere it is a real character */
static bool
is_pictographic(utf8proc_int32_t c) {
  return utf8proc_get_property(c)->boundclass ==
  UTF8PROC_BOUNDCLASS_EXTENDED_PICTOGRAPHIC;
}

typedef struct {
  const char *draw;
  int len;
  int width;
  char buf[64];
} Cluster;

static utf8proc_ssize_t
next_cluster(const utf8proc_uint8_t *bytes, utf8proc_ssize_t len, Cluster *c) {
  utf8proc_int32_t first, prev, state = 0;
  utf8proc_ssize_t n, used, look, p;
  bool vs16 = false, zwj = false, pict;
  int j = 0;

  c->draw = (const char *)bytes;
  c->len = 0;
  c->width = 0;

  if (len <= 0)
    return 0;

  n = utf8proc_iterate(bytes, len, &first);
  if (n <= 0) {
    c->draw = replacement;
    c->len = (int)sizeof(replacement) - 1;
    c->width = 1;
    return 1;
  }
  if (is_control(first)) {
    c->draw = replacement;
    c->len = (int)sizeof(replacement) - 1;
    c->width = 1;
    return n;
  }

  used = n;
  prev = first;
  pict = is_pictographic(first);
  c->width = char_width(first);

  for (look = n; look < len;) {
    utf8proc_int32_t next;
    utf8proc_ssize_t m;

    m = utf8proc_iterate(bytes + look, len - look, &next);
    if (m <= 0 ||
      utf8proc_grapheme_break_stateful(prev, next, &state))
      break;
    if (next == 0xFE0F)
      vs16 = true;
    if (next == 0x200D)
      zwj = true;
    if (is_pictographic(next))
      pict = true;
    c->width += char_width(next); /* both joiners are 0 anyway */
    used += m;
    prev = next;
    look += m;
  }

  /* U+FE0F is presentation only, so it always goes. U+200D goes only when
   * this really is an emoji sequence. */
  zwj = zwj && pict;
  if (!vs16 && !zwj) {
    c->len = (int)used;
    return used;
  }

  c->width = 0;
  for (p = 0; p < used;) {
    utf8proc_int32_t cp;
    utf8proc_ssize_t m = utf8proc_iterate(bytes + p, used - p, &cp);

    if (m <= 0)
      break;
    if (cp != 0xFE0F && !(cp == 0x200D && zwj)) {
      if (j + (int)m >= (int)sizeof(c->buf))
        break;
      memcpy(c->buf + j, bytes + p, (size_t)m);
      j += (int)m;
      c->width += char_width(cp);
    }
    p += m;
  }
  c->buf[j] = '\0';
  c->draw = c->buf;
  c->len = j;
  return used;
}

static int
clip(const char *text, int width, int *used) {
  const utf8proc_uint8_t *bytes = (const utf8proc_uint8_t *)text;
  utf8proc_ssize_t len = (utf8proc_ssize_t)strlen(text);
  utf8proc_ssize_t pos = 0;
  int columns = 0;

  while (pos < len) {
    Cluster c;
    utf8proc_ssize_t n;

    n = next_cluster(bytes + pos, len - pos, &c);
    if (n <= 0 || columns + c.width > width)
      break;

    columns += c.width;
    pos += n;
  }

  *used = columns;
  return (int)pos;
}

int
text_width(const char *text) {
  int columns;

  clip(text, INT_MAX, &columns);
  return columns;
}

void
draw_text(WINDOW *win, int row, int x, int width, const char *text) {
  const utf8proc_uint8_t *bytes = (const utf8proc_uint8_t *)text;
  utf8proc_ssize_t len, pos = 0;
  int col = 0;

  if (width <= 0)
    return;

  len = (utf8proc_ssize_t)strlen(text);

  while (pos < len) {
    Cluster c;
    utf8proc_ssize_t n;

    n = next_cluster(bytes + pos, len - pos, &c);
    if (n <= 0 || col + c.width > width)
      break;
    /* place every cluster at its own absolute column, so a glyph
     * the terminal draws wider than ncurses' wcwidth() expects
     * cannot shove the rest of the row */
    mvwaddnstr(win, row, x + col, c.draw, c.len);
    col += c.width;
    pos += n;
  }
}

void
draw_text_centered(WINDOW *win, int row, int x, int width, const char *text) {
  int columns;

  if (width <= 0)
    return;
  clip(text, width, &columns);
  draw_text(win, row, x + (width - columns) / 2, columns, text);
}

void
draw_text_right(WINDOW *win, int row, int x, int width, const char *text) {
  int columns;

  if (width <= 0)
    return;
  clip(text, width, &columns);
  draw_text(win, row, x + width - columns, columns, text);
}

void
draw_volume(WINDOW *win, int row, int width) {
  char buf[16];

  if (get_volume() < 0)
    snprintf(buf, sizeof(buf), "Vol: N/A");
  else
    snprintf(buf, sizeof(buf), "Vol: %d%%", get_volume());

  style_on(win, STYLE_VOLUME);
  draw_text_right(win, row, 0, width - padding_right, buf);
  style_off(win, STYLE_VOLUME);
}

static const struct {
  void (*toggle)(const Arg *);
  bool (*active)(void);
  char fallback;
} modes[] = {
  { toggle_repeat, get_repeat, 'r' },
  { toggle_random, get_random, 'z' },
  { toggle_consume, get_consume, 'c' },
  { toggle_single, get_single, 's' },
};

int
modes_width(void) {
  return (int)LENGTH(modes) + 2;
}

static void
mode_cell(WINDOW *win, int row, int *col, int right, char ch, int slot) {
  if (*col >= right)
    return;
  style_on(win, slot);
  mvwaddch(win, row, *col, (chtype)(unsigned char)ch);
  style_off(win, slot);
  (*col)++;
}

void
draw_modes(WINDOW *win, int row, int x, int width) {
  size_t i;
  int right, col;

  if (width <= 0)
    return;

  right = x + width;
  col = x;

  mode_cell(win, row, &col, right, '[', STYLE_DEFAULT);
  for (i = 0; i < LENGTH(modes); i++) {
    int key = key_for_action(modes[i].toggle);
    char c = (key > ' ' && key < 0x7f) ? (char)key
      : modes[i].fallback;

    mode_cell(win, row, &col, right, c,
              modes[i].active() ? STYLE_STATE_MODES_ON
              : STYLE_STATE_MODES_OFF);
  }
  mode_cell(win, row, &col, right, ']', STYLE_DEFAULT);
}

static void
elapsed_string(char *buf, size_t size) {
  unsigned elapsed = get_elapsed_time();
  unsigned total = get_total_time();

  snprintf(buf, size, "[%u:%02u/%u:%02u]", elapsed / 60, elapsed % 60,
           total / 60, total % 60);
}

int
elapsed_width(void) {
  char buf[32];

  elapsed_string(buf, sizeof(buf));
  return text_width(buf);
}

void
draw_elapsed(WINDOW *win, int row, int width) {
  char buf[32];

  elapsed_string(buf, sizeof(buf));

  style_on(win, STYLE_TIME);
  draw_text_right(win, row, 0, width - padding_right, buf);
  style_off(win, STYLE_TIME);
}

static void
progress_cell(WINDOW *win, int row, int col, char ch) {
  if (ch == ' ')
    mvwaddch(win, row, col, ' ' | A_REVERSE);
  else
    mvwaddch(win, row, col, (chtype)(unsigned char)ch);
}

void
draw_progress_bar(WINDOW *win, int row, int width) {
  unsigned long total_ms = (unsigned long)get_total_time() * 1000;
  unsigned long elapsed_ms;
  int i, filled;

  if (!total_ms || width <= 0)
    return;

  elapsed_ms = get_elapsed_ms();
  if (elapsed_ms > total_ms)
    elapsed_ms = total_ms;

  filled = (int)(elapsed_ms * width / total_ms);

  style_on(win, STYLE_PROGRESS);
  for (i = 0; i < filled; i++)
    progress_cell(win, row, i, progress_bar_tail);
  if (filled < width)
    progress_cell(win, row, filled, progress_bar_head);
  style_off(win, STYLE_PROGRESS);
}

static const char *
key_label(int key) {
  static char buf[8];

  switch (key) {
    case ' ':
      return "Space";
    case '\t':
      return "Tab";
    case '\n':
    case '\r':
    case KEY_ENTER:
      return "Enter";
    case KEY_UP:
      return "Up";
    case KEY_DOWN:
      return "Down";
    case KEY_LEFT:
      return "Left";
    case KEY_RIGHT:
      return "Right";
    case KEY_NPAGE:
      return "PgDn";
    case KEY_PPAGE:
      return "PgUp";
    case KEY_HOME:
      return "Home";
    case KEY_END:
      return "End";
    case KEY_DC:
      return "Del";
    case KEY_IC:
      return "Ins";
    case KEY_BTAB:
      return "S-Tab";
    case 27:
      return "Esc";
  }
  if (key > ' ' && key < 0x7f) {
    buf[0] = (char)key;
    buf[1] = '\0';
    return buf;
  }
  return "?";
}

static void
hint_append(char *buf, size_t size, size_t *len, const char *label, int key) {
  int n;

  if (key < 0)
    return;
  n = snprintf(buf + *len, size - *len, "%s%s:%s", *len ? "  " : "",
               label, key_label(key));
  if (n > 0 && (size_t)n < size - *len)
    *len += (size_t)n;
}

void
hint_add(char *buf, size_t size, size_t *len, const char *label,
         void (*action)(const Arg *)) {
  hint_append(buf, size, len, label, key_for_action(action));
}

void
hint_add_i(char *buf, size_t size, size_t *len, const char *label,
           void (*action)(const Arg *), int i) {
  hint_append(buf, size, len, label, key_for_action_i(action, i));
}

const char *
keybind_hint_string(void) {
  static char buf[256];

  const char *play;
  size_t len = 0;

  buf[0] = '\0';
  play = get_player_state() == MPD_STATE_PLAY ? "Pause" : "Play";

  hint_add(buf, sizeof(buf), &len, "Filter", filter_results);
  hint_add(buf, sizeof(buf), &len, play, toggle_pause);
  hint_add(buf, sizeof(buf), &len, "Prev", play_prev);
  hint_add(buf, sizeof(buf), &len, "Next", play_next);
  hint_add(buf, sizeof(buf), &len, "Add", add_to_queue);
  hint_add(buf, sizeof(buf), &len, "Select", select_item);
  hint_add(buf, sizeof(buf), &len, "Delete", delete_selected);
  hint_add(buf, sizeof(buf), &len, "Clear", clear_queue);
  hint_add(buf, sizeof(buf), &len, "Repeat", toggle_repeat);
  hint_add(buf, sizeof(buf), &len, "Shuffle", toggle_random);
  hint_add(buf, sizeof(buf), &len, "Single", toggle_single);
  hint_add(buf, sizeof(buf), &len, "Update", update_database);
  hint_add(buf, sizeof(buf), &len, "Quit", quit);

  return buf;
}

typedef struct {
  int artist_x, artist_w;
  int track_x, track_w;
  int album_x, album_w;
  int time_x, time_w;
} Columns;

static void
song_time_string(char *buf, size_t size, unsigned duration) {
  if (duration >= 3600)
    snprintf(buf, size, "%u:%02u:%02u", duration / 3600,
             (duration / 60) % 60, duration % 60);
  else
    snprintf(buf, size, "%u:%02u", duration / 60, duration % 60);
}

static Columns
compute_columns(WINDOW *win) {
  Columns c;
  int width = getmaxx(win);
  int inner, flexible, artist_w, album_w, track_w;

  inner = width - padding_left - padding_right;
  flexible = inner - col_width_time - 3 * QUEUE_COLUMN_GAP;
  if (flexible < 0)
    flexible = 0;

  artist_w = flexible * col_pct_artist / 100;
  album_w = flexible * col_pct_album / 100;
  /* track gets col_pct_track's share plus whatever integer rounding
   * drops, so the three always sum to exactly flexible and leave no
   * gap before Time */
  track_w = flexible - artist_w - album_w;
  if (track_w < 10)
    track_w = 10;

  c.artist_x = padding_left;
  c.artist_w = artist_w;
  c.track_x = c.artist_x + artist_w + QUEUE_COLUMN_GAP;
  c.track_w = track_w;
  c.album_x = c.track_x + track_w + QUEUE_COLUMN_GAP;
  c.album_w = album_w;
  c.time_x = c.album_x + album_w + QUEUE_COLUMN_GAP;
  c.time_w = col_width_time;

  return c;
}

static void
draw_column_headers(WINDOW *win, const Columns *c) {
  style_on(win, STYLE_COLUMN_HEADER);
  draw_text(win, 0, c->artist_x, c->artist_w, "Artist");
  draw_text(win, 0, c->track_x, c->track_w, "Track");
  if (c->album_w > 0)
    draw_text(win, 0, c->album_x, c->album_w, "Album");
  draw_text(win, 0, c->time_x, c->time_w, "Time");
  style_off(win, STYLE_COLUMN_HEADER);
}

static void
draw_song_row(WINDOW *win, int row, const Columns *c, const Song *song,
              int current, int selected, int marked) {
  char time_buf[16];

  song_time_string(time_buf, sizeof(time_buf), song->duration);

  if (marked)
    wattr_on(win, A_UNDERLINE, NULL);

  if (selected) {
    style_on(win, STYLE_ACTIVE);
    draw_text(win, row, c->artist_x, c->artist_w, song->artist);
    draw_text(win, row, c->track_x, c->track_w, song->title);
    if (c->album_w > 0)
      draw_text(win, row, c->album_x, c->album_w,
                song->album);
    draw_text(win, row, c->time_x, c->time_w, time_buf);
    style_off(win, STYLE_ACTIVE);
  } else if (current) {
    style_on(win, STYLE_HIGHLIGHT);
    draw_text(win, row, c->artist_x, c->artist_w, song->artist);
    draw_text(win, row, c->track_x, c->track_w, song->title);
    if (c->album_w > 0)
      draw_text(win, row, c->album_x, c->album_w,
                song->album);
    draw_text(win, row, c->time_x, c->time_w, time_buf);
    style_off(win, STYLE_HIGHLIGHT);
  } else {
    style_on(win, STYLE_ARTIST);
    draw_text(win, row, c->artist_x, c->artist_w, song->artist);
    style_off(win, STYLE_ARTIST);

    style_on(win, STYLE_TRACK);
    draw_text(win, row, c->track_x, c->track_w, song->title);
    style_off(win, STYLE_TRACK);

    style_on(win, STYLE_ALBUM);
    if (c->album_w > 0)
      draw_text(win, row, c->album_x, c->album_w,
                song->album);
    style_off(win, STYLE_ALBUM);

    style_on(win, STYLE_TIME);
    draw_text(win, row, c->time_x, c->time_w, time_buf);
    style_off(win, STYLE_TIME);
  }

  if (marked)
    wattr_off(win, A_UNDERLINE, NULL);
}

/* cursor is the selected row, offset is the first row on screen,
 * page remembers how many rows fit, so page-wise scrolling can
 * work from a keybind that has no access to the window. Each scrollable pane
 * keeps its own, so switching tabs does not lose your place. */
typedef struct {
  int cursor;
  int offset;
  int page;

  int *marked; /* per item multi-selection marks */
  int marked_cap;
  int marked_count;
  int marked_for; /* the list count the marks are valid for (-1 = stale) */
} ListView;

static ListView queue_view;

static void
marks_sync(ListView *v, int count) {
  if (v->marked_for == count)
    return;
  if (count > v->marked_cap) {
    int *m = realloc(v->marked, (size_t)count * sizeof(*m));

    if (m == NULL)
      return;
    v->marked = m;
    v->marked_cap = count;
  }
  if (v->marked != NULL && count > 0)
    memset(v->marked, 0, (size_t)count * sizeof(*v->marked));
  v->marked_count = 0;
  v->marked_for = count;
}

static void
marks_clear(ListView *v) {
  if (v->marked != NULL && v->marked_cap > 0)
    memset(v->marked, 0,
           (size_t)v->marked_cap * sizeof(*v->marked));
  v->marked_count = 0;
  v->marked_for = -1;
}

typedef struct {
  char path[512]; /* current directory ("" = root) */

  DirList dirs;   /* current dir's subfolders */
  SongList songs; /* current dir's songs */
  ListView view;  /* cursor/offset/marks over the center list */

  DirList parent_dirs;   /* parent's subfolders (left context column) */
  SongList parent_songs; /* parent's songs (left context column) */
  int parent_sel;        /* index of the current dir in the parent list or -1 */
  int parent_off;        /* left column scroll offset */

  DirList preview_dirs; /* highlighted center subfolder's */
  SongList preview;     /* ... and its songs */
  char preview_path[512];

  int page; /* visible rows, for page scrolling */
  bool loaded;
  char restore_path[512]; /* on the next load, put the center cursor on
                             this child */

  SongInfo info; /* highlighted center song's tags */
  char info_uri[512];
} Browser;

static Browser browser;

static void
draw_song_list(WINDOW *win, const SongList *list, ListView *view,
               bool highlight_current) {
  Columns cols = compute_columns(win);
  int count = list->count;
  int row, visible, max_offset, current_id;

  draw_column_headers(win, &cols);
  marks_sync(view, count);

  visible = getmaxy(win) - 1;
  if (visible < 1)
    visible = 1;
  view->page = visible;

  if (view->cursor >= count)
    view->cursor = count - 1;
  if (view->cursor < 0)
    view->cursor = 0;

  if (view->cursor < view->offset)
    view->offset = view->cursor;
  if (view->cursor >= view->offset + visible)
    view->offset = view->cursor - visible + 1;

  max_offset = count - visible;
  if (max_offset < 0)
    max_offset = 0;
  if (view->offset > max_offset)
    view->offset = max_offset;
  if (view->offset < 0)
    view->offset = 0;

  current_id = highlight_current ? get_current_song_id() : -1;

  for (row = 0; row < visible; row++) {
    int i = view->offset + row;
    const Song *song;
    bool current, selected, marked;

    if (i >= count)
      break;

    song = &list->items[i];
    current = current_id >= 0 && song->id == current_id;
    selected = i == view->cursor;
    marked = view->marked != NULL && view->marked[i];
    draw_song_row(win, row + 1, &cols, song, current, selected,
                  marked);
  }
}

void
draw_now_playing(WINDOW *win) {
  draw_song_list(win, mpd_queue(), &queue_view, 1);
}


static bool
browse_active(void) {
  return tab_active(draw_browse);
}

static bool
lyrics_active(void) {
  return tab_active(draw_lyrics);
}

static int
browser_count(void) {
  return browser.dirs.count + browser.songs.count;
}

static void
browser_load(void) {
  int i, n;

  mpd_browse(browser.path, &browser.dirs, &browser.songs);
  browser.loaded = true;
  browser.preview_path[0] = '\0';

  browser.parent_sel = -1;
  if (browser.path[0] != '\0') {
    char parent[512];
    char *slash;

    snprintf(parent, sizeof(parent), "%s", browser.path);
    slash = strrchr(parent, '/');
    if (slash != NULL)
      *slash = '\0';
    else
      parent[0] = '\0';
    mpd_browse(parent, &browser.parent_dirs,
               &browser.parent_songs);
    for (i = 0; i < browser.parent_dirs.count; i++)
      if (strcmp(browser.parent_dirs.items[i].path,
                 browser.path) == 0) {
        browser.parent_sel = i;
        break;
      }
  } else {
    browser.parent_dirs.count = 0;
    browser.parent_songs.count = 0;
  }

  if (browser.restore_path[0] != '\0') {
    for (i = 0; i < browser.dirs.count; i++)
      if (strcmp(browser.dirs.items[i].path,
                 browser.restore_path) == 0) {
        browser.view.cursor = i;
        break;
      }
    browser.restore_path[0] = '\0';
  }

  n = browser_count();
  if (browser.view.cursor >= n)
    browser.view.cursor = n > 0 ? n - 1 : 0;
  if (browser.view.cursor < 0)
    browser.view.cursor = 0;
}

static void
browser_invalidate(void) {
  browser.loaded = false;
  browser.preview_path[0] = '\0';
  browser.info_uri[0] = '\0';
}

static const char *
browser_selected_uri(void) {
  int i = browser.view.cursor - browser.dirs.count;

  if (i >= 0 && i < browser.songs.count)
    return browser.songs.items[i].uri;
  return NULL;
}

static const char *
browser_hovered_dir(void) {
  if (browser.view.cursor < browser.dirs.count)
    return browser.dirs.items[browser.view.cursor].path;
  return NULL;
}

static void
browser_sync_preview(void) {
  const char *p = browser_hovered_dir();

  if (p == NULL) {
    browser.preview_path[0] = '\0';
    browser.preview_dirs.count = 0;
    browser.preview.count = 0;
    return;
  }
  if (strcmp(p, browser.preview_path) != 0) {
    mpd_browse(p, &browser.preview_dirs, &browser.preview);
    snprintf(browser.preview_path, sizeof(browser.preview_path),
             "%s", p);
  }
}

static void
browser_sync_info(void) {
  const char *uri = browser_selected_uri();

  if (uri == NULL) {
    browser.info.valid = false;
    browser.info_uri[0] = '\0';
    return;
  }
  if (strcmp(uri, browser.info_uri) != 0) {
    mpd_song_info(uri, &browser.info);
    snprintf(browser.info_uri, sizeof(browser.info_uri), "%s",
             uri);
  }
}

static void
browser_ensure_loaded(void) {
  if (!browser.loaded)
    browser_load();
  browser_sync_preview();
  browser_sync_info();
}

static void
browser_move(int delta) {
  int n = browser_count();

  if (n == 0)
    return;
  browser.view.cursor += delta;
  if (browser.view.cursor < 0)
    browser.view.cursor = 0;
  if (browser.view.cursor >= n)
    browser.view.cursor = n - 1;
}

static void
browser_edge(bool bottom) {
  int n = browser_count();

  browser.view.cursor = bottom ? (n > 0 ? n - 1 : 0) : 0;
}

static void
browser_descend(const char *path) {
  snprintf(browser.path, sizeof(browser.path), "%s", path);
  browser.view.cursor = browser.view.offset = 0;
  marks_clear(&browser.view);
  browser.loaded = false;
}

static void
browser_up(void) {
  char *slash;

  if (browser.path[0] == '\0')
    return;

  snprintf(browser.restore_path, sizeof(browser.restore_path), "%s",
           browser.path);

  slash = strrchr(browser.path, '/');
  if (slash != NULL)
    *slash = '\0';
  else
    browser.path[0] = '\0';

  browser.view.cursor = browser.view.offset = 0;
  marks_clear(&browser.view);
  browser.loaded = false;
}

static void
browser_enter(void) {
  if (browser.view.cursor < browser.dirs.count) {
    browser_descend(browser.dirs.items[browser.view.cursor].path);
  } else {
    int i = browser.view.cursor - browser.dirs.count;

    if (i < browser.songs.count)
      queue_add_and_play(browser.songs.items[i].uri);
  }
}

static void
browser_nav_right(void) {
  if (browser.view.cursor < browser.dirs.count)
    browser_descend(browser.dirs.items[browser.view.cursor].path);
}

static void
browser_nav_left(void) {
  browser_up();
}

static int
scroll_offset(int cursor, int offset, int count, int visible) {
  int max_off;

  if (visible < 1)
    visible = 1;
  if (cursor < offset)
    offset = cursor;
  if (cursor >= offset + visible)
    offset = cursor - visible + 1;
  max_off = count - visible;
  if (max_off < 0)
    max_off = 0;
  if (offset > max_off)
    offset = max_off;
  if (offset < 0)
    offset = 0;
  return offset;
}

static void
browser_row(WINDOW *win, int row, int x, int w, const char *text, int slot,
            bool selected, bool marked, bool secondary) {
  int tw;

  if (w <= 0)
    return;
  tw = w - 1;
  if (marked)
    wattr_on(win, A_UNDERLINE, NULL);
  if (selected && secondary) {
    style_on(win, STYLE_HIGHLIGHT);
    draw_text(win, row, x + 1, tw, text);
    style_off(win, STYLE_HIGHLIGHT);
  } else if (selected) {
    style_on(win, STYLE_ACTIVE);
    draw_text(win, row, x + 1, tw, text);
    style_off(win, STYLE_ACTIVE);
  } else {
    style_on(win, slot);
    draw_text(win, row, x + 1, tw, text);
    style_off(win, slot);
  }
  if (marked)
    wattr_off(win, A_UNDERLINE, NULL);
}

static void
draw_browser_left(WINDOW *win, int x, int w, int height) {
  int dcount = browser.parent_dirs.count;
  int n = dcount + browser.parent_songs.count;
  int sel = browser.parent_sel;
  int r;

  browser.parent_off = scroll_offset(sel >= 0 ? sel : 0,
                                     browser.parent_off, n, height);

  for (r = 0; r < height; r++) {
    int i = browser.parent_off + r;
    char buf[600];
    int slot;
    bool here;

    if (i >= n)
      break;

    if (i < dcount) {
      snprintf(buf, sizeof(buf), "D %s",
               browser.parent_dirs.items[i].name);
      slot = STYLE_ARTIST;
    } else {
      const Song *s =
        &browser.parent_songs.items[i - dcount];

      snprintf(buf, sizeof(buf), "S %s",
               s->title[0] ? s->title : s->uri);
      slot = STYLE_DEFAULT;
    }

    here = i == sel;
    browser_row(win, r, x, w, buf, slot, here, 0, here);
  }
}

static void
draw_browser_mid(WINDOW *win, int x, int w, int height) {
  int dcount = browser.dirs.count;
  int n = browser_count();
  int r;

  marks_sync(&browser.view, n);
  browser.view.offset = scroll_offset(browser.view.cursor,
                                      browser.view.offset, n, height);

  for (r = 0; r < height; r++) {
    int i = browser.view.offset + r;
    char buf[600];
    int slot, sel, marked;

    if (i >= n)
      break;

    if (i < dcount) {
      snprintf(buf, sizeof(buf), "D %s",
               browser.dirs.items[i].name);
      slot = STYLE_ARTIST;
    } else {
      const Song *s = &browser.songs.items[i - dcount];

      snprintf(buf, sizeof(buf), "S %s",
               s->title[0] ? s->title : s->uri);
      slot = STYLE_DEFAULT;
    }

    sel = i == browser.view.cursor;
    marked = browser.view.marked != NULL && browser.view.marked[i];
    browser_row(win, r, x, w, buf, slot, sel, marked, 0);
  }
}

static void
draw_browser_field(WINDOW *win, int *row, int x, int w, const char *label,
                   const char *value) {
  char lbl[64];
  int lw;

  if (value == NULL || value[0] == '\0')
    return;

  snprintf(lbl, sizeof(lbl), "%s: ", label);
  style_on(win, STYLE_COLUMN_HEADER);
  draw_text(win, *row, x + 1, w - 1, lbl);
  style_off(win, STYLE_COLUMN_HEADER);

  lw = text_width(lbl);
  style_on(win, STYLE_DEFAULT);
  draw_text(win, *row, x + 1 + lw, w - 1 - lw, value);
  style_off(win, STYLE_DEFAULT);
  (*row)++;
}

static void
draw_browser_preview_list(WINDOW *win, int *row, int x, int w, int height,
                          const DirList *dirs, const SongList *songs) {
  int i;

  for (i = 0; i < dirs->count && *row < height; i++) {
    char buf[600];

    snprintf(buf, sizeof(buf), "D %s", dirs->items[i].name);
    style_on(win, STYLE_ARTIST);
    draw_text(win, (*row)++, x + 1, w - 1, buf);
    style_off(win, STYLE_ARTIST);
  }
  for (i = 0; i < songs->count && *row < height; i++) {
    const Song *s = &songs->items[i];
    char buf[514];

    snprintf(buf, sizeof(buf), "S %s",
             s->title[0] ? s->title : s->uri);
    style_on(win, STYLE_DEFAULT);
    draw_text(win, (*row)++, x + 1, w - 1, buf);
    style_off(win, STYLE_DEFAULT);
  }
}

static void
draw_browser_info(WINDOW *win, int x, int w, int height) {
  const SongInfo *in = &browser.info;
  char dur[16];
  int row = 0;

  if (in->valid) {
    style_on(win, STYLE_COLUMN_HEADER);
    draw_text(win, row++, x + 1, w - 1, "[Info]");
    style_off(win, STYLE_COLUMN_HEADER);

    draw_browser_field(win, &row, x, w, "File", in->uri);
    draw_browser_field(win, &row, x, w, "Title", in->title);
    draw_browser_field(win, &row, x, w, "Artist", in->artist);
    draw_browser_field(win, &row, x, w, "Album", in->album);
    draw_browser_field(win, &row, x, w, "Album Artist",
                       in->album_artist);
    draw_browser_field(win, &row, x, w, "Genre", in->genre);
    draw_browser_field(win, &row, x, w, "Date", in->date);
    draw_browser_field(win, &row, x, w, "Track", in->track);

    snprintf(dur, sizeof(dur), "%u:%02u", in->duration / 60,
             in->duration % 60);
    draw_browser_field(win, &row, x, w, "Time", dur);

    if (in->mtime > 0) {
      time_t t = (time_t)in->mtime;
      struct tm *tmv = localtime(&t);
      if (tmv != NULL) {
        char when[64];

        strftime(when, sizeof(when), "%Y-%m-%d %H:%M",
                 tmv);
        draw_browser_field(win, &row, x, w, "Modified",
                           when);
      }
    }
    return;
  }

  style_on(win, STYLE_COLUMN_HEADER);
  draw_text(win, row++, x + 1, w - 1, "[Preview]");
  style_off(win, STYLE_COLUMN_HEADER);

  if (browser.preview_dirs.count == 0 && browser.preview.count == 0) {
    style_on(win, STYLE_TAB);
    draw_text(win, row, x + 1, w - 1, "empty");
    style_off(win, STYLE_TAB);
    return;
  }

  draw_browser_preview_list(win, &row, x, w, height,
                            &browser.preview_dirs, &browser.preview);
}

void
draw_browse(WINDOW *win) {
  int width = getmaxx(win);
  int height = getmaxy(win);
  int left_w, info_w, mid_x, info_x, right_div, mid_w;

  browser_ensure_loaded();
  browser.page = height;

  left_w = width * 30 / 100;
  info_w = width * 30 / 100;
  mid_x = left_w + 1;
  info_x = width - info_w;
  right_div = info_x - 1;
  mid_w = right_div - mid_x;

  style_on(win, STYLE_BORDER_FOCUSED);
  mvwvline(win, 0, left_w, 0, height);
  mvwvline(win, 0, right_div, 0, height);
  style_off(win, STYLE_BORDER_FOCUSED);

  draw_browser_left(win, 0, left_w, height);
  draw_browser_mid(win, mid_x, mid_w, height);
  draw_browser_info(win, info_x, info_w, height);
}

static void
cursor_repaint(void) {
  ui_redraw(REDRAW_ALL);
}


enum {
  SF_ANY,
  SF_ARTIST,
  SF_ALBUM,
  SF_ALBUM_ARTIST,
  SF_TITLE,
  SF_FILENAME,
  SF_GENRE,
  SF_MODE,
  SF_RESET,
  SF_COUNT
};
#define SF_TEXT_COUNT SF_MODE

static const char *search_labels[SF_COUNT] = {
  "Any Tag", "Artist", "Album", "Album Artist", "Title",
  "Filename", "Genre", "Search mode", "Reset",
};

static struct {
  char text[SF_TEXT_COUNT][256];
  int cursor;
  bool exact;
  SongList results;
  ListView view;
  int focus;
  bool editing;
  int page;
} search;

static bool
search_active(void) {
  return tab_active(draw_search);
}

static void
search_run(void) {
  SearchQuery q = {
    .any = search.text[SF_ANY],
    .artist = search.text[SF_ARTIST],
    .album = search.text[SF_ALBUM],
    .album_artist = search.text[SF_ALBUM_ARTIST],
    .title = search.text[SF_TITLE],
    .filename = search.text[SF_FILENAME],
    .genre = search.text[SF_GENRE],
    .exact = search.exact,
  };
  mpd_search(&q, &search.results);
  search.view.cursor = search.view.offset = 0;
  marks_clear(&search.view);
  if (search.results.count == 0)
    search.focus = 0;
}

static void
search_reset(void) {
  int i;

  for (i = 0; i < SF_TEXT_COUNT; i++)
    search.text[i][0] = '\0';
  search.exact = false;
  search.results.count = 0;
  search.view.cursor = search.view.offset = 0;
  marks_clear(&search.view);
  search.focus = 0;
}

static int
search_field_row(int field) {
  if (field <= SF_GENRE)
    return field;
  if (field == SF_MODE)
    return SF_GENRE + 2;
  return SF_GENRE + 4;
}

static void
draw_search_form(WINDOW *win, int x, int w, int height) {
  int i;

  for (i = 0; i < SF_COUNT; i++) {
    int row = search_field_row(i);
    char line[512];
    bool sel;

    if (row >= height)
      break;

    if (i == SF_RESET) {
      snprintf(line, sizeof(line), "%s", search_labels[i]);
    } else {
      const char *value, *caret;

      if (i < SF_TEXT_COUNT)
        value = search.text[i];
      else
        value = search.exact ? "Exact" : "Contains";
      caret = (i == search.cursor && search.editing) ? "_"
        : "";
      snprintf(line, sizeof(line), "%-13s: %s%s",
               search_labels[i], value, caret);
    }

    sel = i == search.cursor;
    if (sel) {
      style_on(win, STYLE_ACTIVE);
      draw_text(win, row, x + 1, w - 2, line);
      style_off(win, STYLE_ACTIVE);
    } else {
      style_on(win, STYLE_DEFAULT);
      draw_text(win, row, x + 1, w - 2, line);
      style_off(win, STYLE_DEFAULT);
    }
  }
}

static void
draw_search_results(WINDOW *win, int x, int w, int height) {
  int n = search.results.count;
  int r;

  marks_sync(&search.view, n);
  search.view.offset = scroll_offset(search.view.cursor,
                                     search.view.offset, n, height);

  for (r = 0; r < height; r++) {
    int i = search.view.offset + r;
    const Song *s;
    char buf[261]; /* suppress warning with <261 */
    bool sel, marked;

    if (i >= n)
      break;
    s = &search.results.items[i];
    if (s->artist[0] != '\0')
      snprintf(buf, sizeof(buf), "S %s - %s", s->artist,
               s->title);
    else
      snprintf(buf, sizeof(buf), "S %s", s->title);

    sel = search.focus == 1 && i == search.view.cursor;
    marked = search.view.marked != NULL && search.view.marked[i];
    browser_row(win, r, x, w, buf, STYLE_DEFAULT, sel, marked, 0);
  }
}

void
draw_search(WINDOW *win) {
  int width = getmaxx(win);
  int height = getmaxy(win);
  int form_w, res_x, res_w;

  search.page = height;

  form_w = width * 38 / 100;
  res_x = form_w + 1;
  res_w = width - res_x;

  if (width < 24 || res_w < 1) {
    draw_search_form(win, 0, width, height);
    return;
  }

  style_on(win, STYLE_BORDER_FOCUSED);
  mvwvline(win, 0, form_w, 0, height);
  style_off(win, STYLE_BORDER_FOCUSED);

  draw_search_form(win, 0, form_w, height);
  draw_search_results(win, res_x, res_w, height);
}

bool
search_edit_active(void) {
  return search_active() && search.editing;
}

void
search_edit_key(int key) {
  char *field = search.text[search.cursor];
  int len = (int)strlen(field);

  if (key == 27 || key == '\n' || key == '\r' || key == KEY_ENTER) {
    search.editing = false;
    search_run();
    cursor_repaint();
    return;
  }
  if (key == KEY_BACKSPACE || key == 127 || key == 8) {
    if (len > 0)
      field[len - 1] = '\0';
  } else if (key >= 32 && key < 127 &&
    len < (int)sizeof(search.text[0]) - 1) {
    field[len] = (char)key;
    field[len + 1] = '\0';
  } else {
    return;
  }
  cursor_repaint();
}

static void
search_enter(void) {
  if (search.focus == 1) {
    if (search.view.cursor < search.results.count)
      queue_add_and_play(
        search.results.items[search.view.cursor].uri);
    return;
  }
  if (search.cursor < SF_TEXT_COUNT)
    search.editing = true;
  else if (search.cursor == SF_MODE) {
    search.exact = !search.exact;
    search_run();
  } else
  search_reset();
}

static void
search_move(int delta) {
  if (search.focus == 1) {
    int n = search.results.count;

    if (n == 0)
      return;
    search.view.cursor += delta;
    if (search.view.cursor < 0)
      search.view.cursor = 0;
    if (search.view.cursor >= n)
      search.view.cursor = n - 1;
  } else {
    search.cursor += delta;
    if (search.cursor < 0)
      search.cursor = 0;
    if (search.cursor >= SF_COUNT)
      search.cursor = SF_COUNT - 1;
  }
}

static ListView *
active_view(const SongList **list_out) {
  void (*draw)(WINDOW *) = tabs[active_index].draw;

  if (draw == draw_now_playing) {
    if (list_out)
      *list_out = mpd_queue();
    return &queue_view;
  }
  return NULL;
}

static void
cursor_by(int delta) {
  const SongList *list;
  ListView *view;

  /* patches hook their tab in here for up/down navigation,
   * see docs/patches.md */
#ifdef PATCH_example
  if (example_active()) {
    example_move(delta);
    cursor_repaint();
    return;
  }
#endif
  if (browse_active()) {
    browser_move(delta);
    cursor_repaint();
    return;
  }
  if (lyrics_active()) {
    lyrics_scroll(delta);
    cursor_repaint();
    return;
  }
  if (search_active()) {
    search_move(delta);
    cursor_repaint();
    return;
  }

  view = active_view(&list);
  if (view == NULL || list->count == 0)
    return;

  view->cursor += delta;
  if (view->cursor < 0)
    view->cursor = 0;
  if (view->cursor >= list->count)
    view->cursor = list->count - 1;

  cursor_repaint();
}

void
cursor_move(const Arg *arg) {
  cursor_by(arg->i);
}

void
cursor_page(const Arg *arg) {
  int page;

  if (browse_active()) {
    page = browser.page > 0 ? browser.page : 1;
  } else if (lyrics_active()) {
    page = lyrics_page_rows();
  } else if (search_active()) {
    page = search.page > 0 ? search.page : 1;
  } else {
    ListView *view = active_view(NULL);

    page = view && view->page > 0 ? view->page : 1;
  }
  cursor_by(arg->i * page);
}

void
cursor_edge(const Arg *arg) {
  const SongList *list;
  ListView *view;

  if (browse_active()) {
    browser_edge(arg->i > 0);
    cursor_repaint();
    return;
  }
  if (lyrics_active()) {
    lyrics_scroll_edge(arg->i > 0);
    cursor_repaint();
    return;
  }
  if (search_active()) {
    if (search.focus == 1 && search.results.count > 0)
      search.view.cursor =
        arg->i > 0 ? search.results.count - 1 : 0;
    else if (search.focus == 0)
      search.cursor = arg->i > 0 ? SF_COUNT - 1 : 0;
    cursor_repaint();
    return;
  }

  view = active_view(&list);
  if (view == NULL || list->count == 0)
    return;

  view->cursor = arg->i > 0 ? list->count - 1 : 0;
  cursor_repaint();
}

void
nav(const Arg *arg) {
  /* patches hook their tab in here for left/right navigation,
   * see docs/patches.md */
#ifdef PATCH_example
  if (example_active()) {
    example_nav(arg->i);
    cursor_repaint();
    return;
  }
#endif
  if (search_active()) {
    if (arg->i < 0)
      search.focus = 0;
    else if (search.results.count > 0)
      search.focus = 1;
    cursor_repaint();
    return;
  }
  if (browse_active()) {
    if (arg->i < 0)
      browser_nav_left();
    else
      browser_nav_right();
    cursor_repaint();
    return;
  }
  cursor_repaint();
}

static const Song *
selected_song(void) {
  const SongList *list;
  ListView *view = active_view(&list);

  if (view == NULL || list->count == 0)
    return NULL;
  return &list->items[view->cursor];
}

void
play_selected(const Arg *arg) {
  const Song *song;

  (void)arg;
  if (browse_active()) {
    browser_enter();
    cursor_repaint();
    return;
  }
  if (search_active()) {
    search_enter();
    cursor_repaint();
    return;
  }

  song = selected_song();
  if (song == NULL)
    return;

  if (song->id >= 0)
    queue_play_id(song->id);
  else
    queue_add_and_play(song->uri);
}

static bool
contains_ci(const char *hay, const char *needle) {
  const char *h;

  if (needle[0] == '\0')
    return true;
  for (h = hay; *h; h++) {
    const char *a = h, *b = needle;

    while (*a && *b &&
      tolower((unsigned char)*a) ==
      tolower((unsigned char)*b)) {
      a++;
      b++;
    }
    if (*b == '\0')
      return true;
  }
  return false;
}

static bool
song_matches(const Song *s, const char *q) {
  return contains_ci(s->title, q) || contains_ci(s->artist, q) ||
  contains_ci(s->album, q);
}

typedef struct {
  ListView *view;
  int count;
  const SongList *songs;
  const DirList *dirs;
  int dir_count;
} Focus;

static bool
focus_get(Focus *f) {
  const SongList *list;
  ListView *v;

  if (browse_active()) {
    f->view = &browser.view;
    f->count = browser_count();
    f->songs = &browser.songs;
    f->dirs = &browser.dirs;
    f->dir_count = browser.dirs.count;
    return true;
  }
  if (search_active()) {
    if (search.focus != 1)
      return false;
    f->view = &search.view;
    f->count = search.results.count;
    f->songs = &search.results;
    f->dirs = NULL;
    f->dir_count = 0;
    return true;
  }
  v = active_view(&list);
  if (v == NULL)
    return false;
  f->view = v;
  f->count = list->count;
  f->songs = list;
  f->dirs = NULL;
  f->dir_count = 0;
  return true;
}

static bool
focus_matches(const Focus *f, int i, const char *q) {
  if (i < f->dir_count)
    return contains_ci(f->dirs->items[i].name, q);
  return song_matches(&f->songs->items[i - f->dir_count], q);
}

static int
focus_scan(const Focus *f, const char *q, int from, int dir) {
  int n = f->count;
  int step;

  if (n == 0)
    return -1;
  for (step = 0; step < n; step++) {
    int i = ((from + dir * step) % n + n) % n;

    if (focus_matches(f, i, q))
      return i;
  }
  return -1;
}

static void
focus_enqueue(const Focus *f, int i) {
  if (i < f->dir_count)
    queue_add(f->dirs->items[i].path);
  else
    queue_add(f->songs->items[i - f->dir_count].uri);
}

void
add_to_queue(const Arg *arg) {
  Focus f;
  bool any;
  int i;

  (void)arg;
  if (!focus_get(&f) || f.count == 0)
    return;
  marks_sync(f.view, f.count);

  any = f.view->marked != NULL && f.view->marked_count > 0;
  for (i = 0; i < f.count; i++) {
    if (any ? !f.view->marked[i] : i != f.view->cursor)
      continue;
    focus_enqueue(&f, i);
  }
  if (any)
    marks_clear(f.view);
}

void
delete_selected(const Arg *arg) {
  const SongList *list;
  ListView *view;
  int i;

  (void)arg;
  if (browse_active())
    return;

  view = active_view(&list);
  if (view == NULL || list->count == 0)
    return;

  marks_sync(view, list->count);
  if (view->marked != NULL && view->marked_count > 0) {
    for (i = 0; i < list->count; i++)
      if (view->marked[i] && list->items[i].id >= 0)
        queue_delete_id(list->items[i].id);
    marks_clear(view);
  } else {
    const Song *song = &list->items[view->cursor];

    if (song->id >= 0)
      queue_delete_id(song->id);
  }
}

void
select_item(const Arg *arg) {
  Focus f;
  int c;

  (void)arg;
  if (!focus_get(&f) || f.count == 0)
    return;
  marks_sync(f.view, f.count);
  if (f.view->marked == NULL)
    return;

  c = f.view->cursor;
  f.view->marked[c] = !f.view->marked[c];
  f.view->marked_count += f.view->marked[c] ? 1 : -1;
  if (f.view->cursor < f.count - 1)
    f.view->cursor++;
  cursor_repaint();
}

static bool find_mode;
static char find_query[128];
static int find_origin;

bool
find_active(void) {
  return find_mode;
}

void
filter_results(const Arg *arg) {
  Focus f;

  (void)arg;
  if (!focus_get(&f))
    return;
  find_mode = true;
  find_query[0] = '\0';
  find_origin = f.view->cursor;
  cursor_repaint();
}

void
find_input(int key) {
  Focus f;
  int len, hit;

  if (!focus_get(&f)) {
    find_mode = false;
    return;
  }

  if (key == 27) {
    find_mode = false;
    if (find_origin < f.count)
      f.view->cursor = find_origin;
    cursor_repaint();
    return;
  }
  if (key == '\n' || key == '\r' || key == KEY_ENTER) {
    find_mode = false;
    cursor_repaint();
    return;
  }

  len = (int)strlen(find_query);
  if (key == KEY_BACKSPACE || key == 127 || key == 8) {
    if (len > 0)
      find_query[--len] = '\0';
  } else if (key >= 32 && key < 127 &&
    len < (int)sizeof(find_query) - 1) {
    find_query[len] = (char)key;
    find_query[len + 1] = '\0';
  } else {
    return;
  }

  hit = focus_scan(&f, find_query, find_origin, 1);
  if (hit >= 0)
    f.view->cursor = hit;
  cursor_repaint();
}

void
find_next(const Arg *arg) {
  Focus f;
  int hit;

  (void)arg;
  if (!focus_get(&f) || find_query[0] == '\0')
    return;
  hit = focus_scan(&f, find_query, f.view->cursor + 1, 1);
  if (hit >= 0) {
    f.view->cursor = hit;
    cursor_repaint();
  }
}

void
find_prev(const Arg *arg) {
  Focus f;
  int hit;

  (void)arg;
  if (!focus_get(&f) || find_query[0] == '\0')
    return;
  hit = focus_scan(&f, find_query, f.view->cursor - 1, -1);
  if (hit >= 0) {
    f.view->cursor = hit;
    cursor_repaint();
  }
}

static const char *
find_prompt(void) {
  static char buf[160];

  snprintf(buf, sizeof(buf), "Find: %s", find_query);
  return buf;
}
