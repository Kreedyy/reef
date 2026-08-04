#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#include "keybinds.h"
#include "lrclib.h"
#include "mpd.h"
#include "types.h"
#include "ui.h"

static bool insert_mode_active = false;

#define SYNC_LINE_MAX 75
#define SYNC_MAX_LINES 75

typedef struct {
  long time_ms;
  char text[SYNC_LINE_MAX];
  size_t len;
} SyncLine;

static SyncLine lines[SYNC_MAX_LINES];
static size_t line_count;

static size_t cur_line;
static size_t cur_col;

static size_t scroll_row;
static int scroll_col;

static int view_rows = 1;

static void
lines_ready(void) {
  if (line_count > 0)
    return;
  lines[0] = (SyncLine){ .time_ms = -1, .text = { 0 }, .len = 0 };
  line_count = 1;
}

bool
lrclib_insert_mode_active(void) {
  return insert_mode_active;
}

static void clamp_col(void);

static bool
is_blank(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void
lrclib_trim_trailing(void) {
  size_t i;

  for (i = 0; i < line_count; i++) {
    SyncLine *ln = &lines[i];

    while (ln->len > 0 && is_blank(ln->text[ln->len - 1]))
      ln->len--;
    ln->text[ln->len] = '\0';
  }

  while (line_count > 0 && lines[line_count - 1].len == 0)
    line_count--;

  if (line_count == 0) {
    cur_line = 0;
    cur_col = 0;
    scroll_row = 0;
    scroll_col = 0;
    return;
  }

  if (cur_line >= line_count)
    cur_line = line_count - 1;
  if (scroll_row >= line_count)
    scroll_row = line_count - 1;
  clamp_col();
}

void
lrclib_set_insert_mode(const Arg *arg) {
  if (arg->b)
    lines_ready();
  else
    lrclib_trim_trailing();
  insert_mode_active = arg->b;
}

void
lrclib_clear(const Arg *arg) {
  (void)arg;
  line_count = 0;
  cur_line = 0;
  cur_col = 0;
  scroll_row = 0;
  scroll_col = 0;
  insert_mode_active = false;
}

static size_t
prev_char(const char *text, size_t col) {
  size_t i = col;

  while (i > 0 && ((unsigned char)text[--i] & 0xC0) == 0x80);
  return i;
}

static size_t
next_char(const char *text, size_t col, size_t len) {
  size_t i = col;

  if (i < len)
    i++;
  while (i < len && ((unsigned char)text[i] & 0xC0) == 0x80)
    i++;
  return i;
}

static void
insert_byte(char c) {
  SyncLine *ln = &lines[cur_line];

  if (ln->len + 1 >= SYNC_LINE_MAX)
    return;
  memmove(ln->text + cur_col + 1, ln->text + cur_col, ln->len - cur_col);
  ln->text[cur_col++] = c;
  ln->text[++ln->len] = '\0';
}

static void
split_line(void) {
  SyncLine *ln, *next;

  if (line_count >= SYNC_MAX_LINES)
    return;

  memmove(&lines[cur_line + 2], &lines[cur_line + 1],
          (line_count - cur_line - 1) * sizeof(*lines));
  ln = &lines[cur_line];
  next = &lines[cur_line + 1];

  next->time_ms = -1;
  next->len = ln->len - cur_col;
  memcpy(next->text, ln->text + cur_col, next->len);
  next->text[next->len] = '\0';

  ln->len = cur_col;
  ln->text[ln->len] = '\0';

  line_count++;
  cur_line++;
  cur_col = 0;
}

static void
join_prev_line(void) {
  SyncLine *ln = &lines[cur_line];
  SyncLine *prev = &lines[cur_line - 1];

  if (prev->len + ln->len >= SYNC_LINE_MAX)
    return;

  memcpy(prev->text + prev->len, ln->text, ln->len);
  cur_col = prev->len;
  prev->len += ln->len;
  prev->text[prev->len] = '\0';

  memmove(&lines[cur_line], &lines[cur_line + 1],
          (line_count - cur_line - 1) * sizeof(*lines));
  line_count--;
  cur_line--;
}

static void
backspace(void) {
  SyncLine *ln = &lines[cur_line];
  size_t start;

  if (cur_col == 0) {
    if (cur_line > 0)
      join_prev_line();
    return;
  }
  start = prev_char(ln->text, cur_col);
  memmove(ln->text + start, ln->text + cur_col, ln->len - cur_col);
  ln->len -= cur_col - start;
  ln->text[ln->len] = '\0';
  cur_col = start;
}

static void
delete(void) {
  SyncLine *ln = &lines[cur_line];
  size_t end;

  if (cur_col >= ln->len) {
    size_t at = cur_col, before = line_count;

    if (cur_line + 1 >= line_count)
      return;
    cur_line++;
    cur_col = 0;
    join_prev_line();
    if (line_count == before) {
      cur_line--;
      cur_col = at;
    }
    return;
  }

  end = next_char(ln->text, cur_col, ln->len);
  memmove(ln->text + cur_col, ln->text + end, ln->len - end);
  ln->len -= end - cur_col;
  ln->text[ln->len] = '\0';
}


static void
clamp_col(void) {
  SyncLine *ln = &lines[cur_line];

  if (cur_col > ln->len)
    cur_col = ln->len;
  while (cur_col > 0 && ((unsigned char)ln->text[cur_col] & 0xC0) == 0x80)
    cur_col--;
}

void
lrclib_move(int delta) {
  lines_ready();

  if (delta < 0)
    cur_line = cur_line > (size_t)-delta ? cur_line - (size_t)-delta : 0;
  else
    cur_line += (size_t)delta;

  if (cur_line >= line_count)
    cur_line = line_count - 1;
  clamp_col();
}

void
lrclib_edge(int dir) {
  lines_ready();
  cur_line = dir > 0 ? line_count - 1 : 0;
  clamp_col();
}

int
lrclib_page_rows(void) {
  return view_rows > 0 ? view_rows : 1;
}

void
lrclib_handle_input(int input) {
  lines_ready();

  switch (input) {
    case KEY_BACKSPACE:
    case 127:
    case '\b':
      backspace();
      return;
    case KEY_DC:
      delete();
      return;
    case KEY_ENTER:
    case '\n':
    case '\r':
      split_line();
      return;
    case KEY_LEFT:
      if (cur_col > 0)
        cur_col = prev_char(lines[cur_line].text, cur_col);
      else if (cur_line > 0)
        cur_col = lines[--cur_line].len;
      return;
    case KEY_RIGHT:
      if (cur_col < lines[cur_line].len)
        cur_col = next_char(lines[cur_line].text, cur_col,
                            lines[cur_line].len);
      else if (cur_line + 1 < line_count)
        cur_line++, cur_col = 0;
      return;
    case KEY_UP:
      lrclib_move(-1);
      return;
    case KEY_DOWN:
      lrclib_move(1);
      return;
    case KEY_PPAGE:
      lrclib_move(-lrclib_page_rows());
      return;
    case KEY_NPAGE:
      lrclib_move(lrclib_page_rows());
      return;
    case KEY_HOME:
      cur_col = 0;
      return;
    case KEY_END:
      cur_col = lines[cur_line].len;
      return;
  }

  if (input >= 0x20 && input < 0x100 && input != 0x7f)
    insert_byte((char)input);
}

void
lrclib_sync_line(const Arg *arg) {
  (void)arg;

  lines_ready();
  if (!has_song_loaded())
    return;

  lines[cur_line].time_ms = (long)get_elapsed_ms();
  lrclib_move(1);
}

size_t
lrclib_line_count(void) {
  return line_count;
}

const char *
lrclib_line_text(size_t i, long *time_ms) {
  if (i >= line_count)
    return NULL;
  if (time_ms != NULL)
    *time_ms = lines[i].time_ms;
  return lines[i].text;
}

static void
draw_lrclib_status(WINDOW *win) {
  const char *msg = lrclib_publish_message();
  int height, width, slot;

  if (msg[0] == '\0')
    return;

  getmaxyx(win, height, width);
  slot = lrclib_publish_state() == LRCLIB_PUBLISH_ERROR ? STYLE_ERROR
    : STYLE_HIGHLIGHT;
  style_on(win, slot);
  draw_text_right(win, height - 2, 3, width - 6, msg);
  style_off(win, slot);
}

static void
draw_lrclib_keybinds(WINDOW *win) {
  int height, width;
  getmaxyx(win, height, width);
  char buf[64];
  size_t len = 0;

  if (insert_mode_active) {
    hint_add_b(buf, sizeof(buf), &len, "Exit", lrclib_set_insert_mode, false);
  }
  else {
    hint_add(buf, sizeof(buf), &len, "Sync", lrclib_sync_line);
    hint_add_b(buf, sizeof(buf), &len, "Edit", lrclib_set_insert_mode, true);
    hint_add(buf, sizeof(buf), &len, "Clear", lrclib_clear);
    hint_add(buf, sizeof(buf), &len, "Save", lrclib_save_lrc);
    hint_add(buf, sizeof(buf), &len, "Publish", lrclib_publish_current);
  }


  style_on(win, STYLE_KEYBIND);
  draw_text(win, height - 2, 3, width, buf);
  style_off(win, STYLE_KEYBIND);
}

static int
cursor_column(void) {
  char prefix[SYNC_LINE_MAX];

  memcpy(prefix, lines[cur_line].text, cur_col);
  prefix[cur_col] = '\0';
  return text_width(prefix);
}

static void
cursor_cell(char *buf, size_t size) {
  const SyncLine *ln = &lines[cur_line];
  size_t end;

  if (cur_col >= ln->len) {
    buf[0] = ' ';
    buf[1] = '\0';
    return;
  }
  end = next_char(ln->text, cur_col, ln->len);
  if (end - cur_col >= size)
    end = cur_col + size - 1;
  memcpy(buf, ln->text + cur_col, end - cur_col);
  buf[end - cur_col] = '\0';
}

static size_t
byte_at_column(const char *text, int col) {
  int used;

  return col <= 0 ? 0 : (size_t)text_clip(text, col, &used);
}

static void
scroll_follow(int rows, int avail, int cursor_col) {
  if (cur_line < scroll_row)
    scroll_row = cur_line;
  else if (cur_line >= scroll_row + (size_t)rows)
    scroll_row = cur_line - (size_t)rows + 1;

  if (cursor_col < scroll_col)
    scroll_col = cursor_col;
  else if (cursor_col >= scroll_col + avail)
    scroll_col = cursor_col - avail + 1;
}

static void
draw_cursor(WINDOW *win, int row, int x, int width, int cursor_col) {
  char cell[8];
  int col = x + cursor_col - scroll_col;

  if (col < x || col >= width)
    return;
  cursor_cell(cell, sizeof(cell));
  style_on(win, STYLE_ACTIVE);
  draw_text(win, row, col, width - col, cell);
  style_off(win, STYLE_ACTIVE);
}

static void
draw_lrclib_lyrics(WINDOW *win) {
  /* wide enough for a mm:ss.xx stamp plus a column of gap */
  int padding = 9;
  int height, width, rows, avail, cursor_col;
  size_t i;

  getmaxyx(win, height, width);
  rows = height - 2;
  avail = width - padding;

  if (line_count == 0 || rows < 1 || avail < 1)
    return;

  view_rows = rows;

  cursor_col = cursor_column();
  scroll_follow(rows, avail, cursor_col);

  for (i = scroll_row; i < line_count && (int)(i - scroll_row) < rows; i++) {
    const SyncLine *ln = &lines[i];
    size_t off = byte_at_column(ln->text, scroll_col);
    int row = (int)(i - scroll_row);
    /* out of insert mode the cursor is the only thing saying which line the
     * sync key will stamp, so the row itself has to show it */
    int slot = (i == cur_line && !insert_mode_active) ? STYLE_ACTIVE
      : STYLE_DEFAULT;

    if (ln->time_ms >= 0) {
      char stamp[32];
      /* clamped so a silly timestamp cannot push the gutter out of shape */
      int mins = (int)(ln->time_ms / 60000);
      int secs = (int)(ln->time_ms / 1000 % 60);
      int cs = (int)(ln->time_ms % 1000 / 10);

      snprintf(stamp, sizeof(stamp), "%02d:%02d.%02d", mins > 99 ? 99 : mins,
               secs, cs);
      style_on(win, STYLE_TIME);
      draw_text(win, row, 0, padding - 1, stamp);
      style_off(win, STYLE_TIME);
    }

    style_on(win, slot);
    draw_text(win, row, padding, avail, ln->text + off);
    style_off(win, slot);
  }

  if (insert_mode_active)
    draw_cursor(win, (int)(cur_line - scroll_row), padding, width,
                cursor_col);
}

void
draw_lrclib_sync(WINDOW *win) {
  draw_lrclib_keybinds(win);
  draw_lrclib_status(win);
  draw_lrclib_lyrics(win);
}

bool
lrclib_sync_active(void) {
  return tab_active(draw_lrclib_sync);
}
