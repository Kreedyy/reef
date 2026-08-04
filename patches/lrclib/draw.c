#include <ncurses.h>
#include <string.h>

#include "keybinds.h"
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

void
lrclib_set_insert_mode(const Arg *arg) {
  if (arg->b)
    lines_ready();
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

static void
sync_line(void) {

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
    hint_add_b(buf, sizeof(buf), &len, "Edit", lrclib_set_insert_mode, true);
    hint_add(buf, sizeof(buf), &len, "Clear", lrclib_clear);
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
  int padding = 6;
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

  style_on(win, STYLE_DEFAULT);
  for (i = scroll_row; i < line_count && (int)(i - scroll_row) < rows; i++) {
    const SyncLine *ln = &lines[i];
    size_t off = byte_at_column(ln->text, scroll_col);

    draw_text(win, (int)(i - scroll_row), padding, avail, ln->text + off);
  }
  style_off(win, STYLE_DEFAULT);

  if (insert_mode_active)
    draw_cursor(win, (int)(cur_line - scroll_row), padding, width,
                cursor_col);
}

void
draw_lrclib_sync(WINDOW *win) {
  draw_lrclib_keybinds(win);
  draw_lrclib_lyrics(win);
}

bool
lrclib_sync_active(void) {
  return tab_active(draw_lrclib_sync);
}
