#include <ctype.h>
#include <dirent.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utf8proc.h>

#include "config.h"
#include "layout.h"
#include "lyrics.h"
#include "mpd.h"
#include "ui.h"

#ifdef PATCH_lrclib
#include "lrclib.h"
#endif

typedef enum {
  LYRICS_EMPTY,
  LYRICS_LOADING,
  LYRICS_LOADED,
  LYRICS_NONE
} LyricsStatus;

typedef struct {
  long time_ms;
  char *text;
} LyricsLine;

static char cur_artist[256];
static char cur_title[256];

static LyricsLine *lines;
static int line_count;
static int line_cap;
static bool synced;
static int scroll_off;
static int view_height = 1;

static LyricsStatus status;

#define MAX_PROVIDERS 8
static LyricsProvider providers[MAX_PROVIDERS];
static int provider_count;
static int provider_next;

#define KEY_CAP 192

#define NAME_CAP 256

#define SCORE_EXACT    100
#define SCORE_TITLE    95
#define SCORE_CONTAINS 90
#define SCORE_MIN      80


static void
lines_clear(void) {
  int i;

  for (i = 0; i < line_count; i++)
    free(lines[i].text);
  line_count = 0;
}

static void
lines_push(long time_ms, const char *text) {
  char *copy;

  if (line_count == line_cap) {
    int cap = line_cap ? line_cap * 2 : 64;
    LyricsLine *grown =
      realloc(lines, (size_t)cap * sizeof(*grown));
    if (grown == NULL)
      return;
    lines = grown;
    line_cap = cap;
  }
  copy = strdup(text);
  if (copy == NULL)
    return;
  lines[line_count].time_ms = time_ms;
  lines[line_count].text = copy;
  line_count++;
}

#define LRC_MAX_MINUTES 600
#define LRC_MAX_SECONDS (LRC_MAX_MINUTES * 60)

static bool
parse_ts(const char *tag, const char **end, long *ms) {
  const char *cur;
  long minutes = 0, seconds = 0, frac = 0, total;
  int frac_digits = 0;

  if (*tag != '[')
    return false;
  cur = tag + 1;
  if (!isdigit((unsigned char)*cur))
    return false;
  while (isdigit((unsigned char)*cur)) {
    if (minutes <= LRC_MAX_MINUTES)
      minutes = minutes * 10 + (*cur - '0');
    cur++;
  }
  if (*cur != ':')
    return false;
  cur++;
  if (!isdigit((unsigned char)*cur))
    return false;
  while (isdigit((unsigned char)*cur)) {
    if (seconds <= LRC_MAX_SECONDS)
      seconds = seconds * 10 + (*cur - '0');
    cur++;
  }
  if (*cur == '.' || *cur == ':') {
    cur++;
    while (isdigit((unsigned char)*cur)) {
      if (frac_digits < 3) {
        frac = frac * 10 + (*cur - '0');
        frac_digits++;
      }
      cur++;
    }
  }
  if (*cur != ']')
    return false;
  total = minutes * 60 + seconds;
  if (total > LRC_MAX_SECONDS)
    return false;
  while (frac_digits < 3) {
    frac *= 10;
    frac_digits++;
  }
  *ms = total * 1000 + frac;
  *end = cur + 1;
  return true;
}

static void
set_text(const char *text) {
  const char *cursor;

  lines_clear();
  synced = false;
  scroll_off = 0;

  cursor = text;
  while (*cursor != '\0') {
    const char *nl, *rest, *close;
    char line[1024];
    size_t len, clen;
    long ts = -1;
    bool had_tag = false, skip;

    nl = strchr(cursor, '\n');
    len = nl != NULL ? (size_t)(nl - cursor) : strlen(cursor);
    if (len > 0 && cursor[len - 1] == '\r')
      len--;

    clen = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
    memcpy(line, cursor, clen);
    line[clen] = '\0';
    while (clen > 0 && isspace((unsigned char)line[clen - 1]))
      line[--clen] = '\0';

    rest = line;
    for (;;) {
      const char *tag_end;
      long ms;

      if (*rest != '[')
        break;
      if (parse_ts(rest, &tag_end, &ms)) {
        if (ts < 0)
          ts = ms;
        had_tag = true;
        rest = tag_end;
        continue;
      }
      close = strchr(rest, ']');
      if (close == NULL)
        break;
      had_tag = true;
      rest = close + 1;
    }

    while (isspace((unsigned char)*rest))
      rest++;

    skip = had_tag && *rest == '\0';
    if (!skip) {
      lines_push(ts, rest);
      if (ts >= 0)
        synced = true;
    }

    if (nl == NULL)
      break;
    cursor = nl + 1;
  }
  status = line_count > 0 ? LYRICS_LOADED : LYRICS_NONE;
}

static int
current_line(void) {
  long elapsed;
  int i, idx = -1;

  if (!synced)
    return -1;
  elapsed = (long)get_elapsed_ms();
  for (i = 0; i < line_count; i++)
    if (lines[i].time_ms >= 0 && lines[i].time_ms <= elapsed)
      idx = i;
  return idx;
}

static void
sanitize(char *dst, const char *src, size_t cap) {
  size_t i = 0;
  for (; src[i] != '\0' && i + 1 < cap; i++)
    dst[i] = src[i] == '/' ? '_' : src[i];
  dst[i] = '\0';
}

static bool
lyrics_dir(char *buf, size_t cap) {
  const char *cfg = lyrics_directory;
  const char *music, *base;
  int written;

  if (cfg == NULL || cfg[0] == '\0')
    cfg = "LRC";

  if (cfg[0] == '/') {
    written = snprintf(buf, cap, "%s", cfg);
    return written > 0 && (size_t)written < cap;
  }
  if (cfg[0] == '~' && cfg[1] == '/') {
    const char *home = getenv("HOME");

    if (home == NULL)
      return false;
    written = snprintf(buf, cap, "%s/%s", home, cfg + 2);
    return written > 0 && (size_t)written < cap;
  }

  music = mpd_music_directory();
  base = music != NULL && music[0] != '\0' ? music : getenv("HOME");
  if (base == NULL)
    return false;
  written = snprintf(buf, cap, "%s/%s", base, cfg);
  return written > 0 && (size_t)written < cap;
}

static bool
lyrics_path(const char *artist, const char *title, const char *ext, char *buf,
            size_t cap) {
  char dir[700];
  char safe_artist[256], safe_title[256];
  int written;

  if (!lyrics_dir(dir, sizeof(dir)))
    return false;
  sanitize(safe_artist, artist, sizeof(safe_artist));
  sanitize(safe_title, title, sizeof(safe_title));
  written = snprintf(buf, cap, "%s/%s - %s.%s", dir, safe_artist,
                     safe_title, ext);
  return written > 0 && (size_t)written < cap;
}


/* keys are what filenames and tags are compared as, casefolded, stripped of
 * accents and with every separator dropped, so "éXAmple - éxàmple.lrc" and
 * "example_example.lrc" collapse into the same thing */
static void
make_key(char *dst, size_t cap, const char *src) {
  utf8proc_uint8_t *norm = NULL;
  const char *cur = src;
  size_t i = 0;

  if (utf8proc_map((const utf8proc_uint8_t *)src, 0, &norm,
                   UTF8PROC_NULLTERM | UTF8PROC_STABLE | UTF8PROC_DECOMPOSE |
                   UTF8PROC_COMPAT | UTF8PROC_CASEFOLD | UTF8PROC_STRIPMARK |
                   UTF8PROC_LUMP) > 0 && norm != NULL)
    cur = (const char *)norm;

  for (; *cur != '\0' && i + 1 < cap; cur++) {
    unsigned char c = (unsigned char)*cur;

    /* bytes above ASCII are kept whole, scripts that do not case or
     * separate still have to survive the fold */
    if (isalnum(c) || c >= 0x80)
      dst[i++] = (char)c;
  }
  dst[i] = '\0';
  free(norm);
}

static int
similarity(const char *a, const char *b) {
  int row[KEY_CAP];
  size_t la = strlen(a), lb = strlen(b), longest, i, j;

  longest = la > lb ? la : lb;
  if (longest == 0 || la >= KEY_CAP || lb >= KEY_CAP)
    return 0;

  for (j = 0; j <= lb; j++)
    row[j] = (int)j;

  for (i = 1; i <= la; i++) {
    int prev = row[0];

    row[0] = (int)i;
    for (j = 1; j <= lb; j++) {
      int diag = prev, sub, del, ins;

      prev = row[j];
      sub = diag + (a[i - 1] != b[j - 1]);
      del = row[j] + 1;
      ins = row[j - 1] + 1;
      row[j] = sub < del ? sub : del;
      if (ins < row[j])
        row[j] = ins;
    }
  }
  return (int)(((longest - (size_t)row[lb]) * 100) / longest);
}

/* swapped is title - artist instead of artist - title */
static int
score_key(const char *key, const char *want, const char *swapped,
          const char *artist, const char *title) {
  int score, alt;

  if (strcmp(key, want) == 0)
    return SCORE_EXACT;
  if (title[0] != '\0' && strcmp(key, title) == 0)
    return SCORE_TITLE;

  score = similarity(want, key);
  alt = similarity(swapped, key);
  if (alt > score)
    score = alt;

  if (title[0] != '\0' && score < SCORE_CONTAINS &&
    strstr(key, title) != NULL &&
    (artist[0] == '\0' || strstr(key, artist) != NULL))
    score = SCORE_CONTAINS;
  return score;
}

/* picks the .lrc in dir whose name comes closest to the playing track,
 * SCORE_MIN deciding how far off it is allowed to be */
static bool
match_local(const char *artist, const char *title, char *buf, size_t cap) {
  char dir[700], joined[sizeof(cur_artist) + sizeof(cur_title) + 4];
  char want[KEY_CAP], swapped[KEY_CAP], akey[KEY_CAP], tkey[KEY_CAP];
  char key[KEY_CAP];
  char best[NAME_CAP];
  size_t best_len = 0;
  int best_score = 0;
  int written;
  DIR *dp;
  struct dirent *ent;

  if (!lyrics_dir(dir, sizeof(dir)))
    return false;

  snprintf(joined, sizeof(joined), "%s - %s", artist, title);
  make_key(want, sizeof(want), joined);
  snprintf(joined, sizeof(joined), "%s - %s", title, artist);
  make_key(swapped, sizeof(swapped), joined);
  make_key(akey, sizeof(akey), artist);
  make_key(tkey, sizeof(tkey), title);
  if (want[0] == '\0')
    return false;

  dp = opendir(dir);
  if (dp == NULL)
    return false;

  best[0] = '\0';
  while ((ent = readdir(dp)) != NULL) {
    const char *dot = strrchr(ent->d_name, '.');
    char stem[NAME_CAP];
    size_t len;
    int score;

    if (dot == NULL || strcasecmp(dot, ".lrc") != 0)
      continue;

    len = (size_t)(dot - ent->d_name);
    if (len == 0 || len >= sizeof(stem))
      continue;
    memcpy(stem, ent->d_name, len);
    stem[len] = '\0';

    make_key(key, sizeof(key), stem);
    if (key[0] == '\0')
      continue;

    score = score_key(key, want, swapped, akey, tkey);
    len = strlen(key);

    if (score > best_score || (score == best_score && best[0] != '\0' &&
      len < best_len)) {
      snprintf(best, sizeof(best), "%s", ent->d_name);
      best_score = score;
      best_len = len;
    }
  }
  closedir(dp);

  if (best[0] == '\0' || best_score < SCORE_MIN)
    return false;

  written = snprintf(buf, cap, "%s/%s", dir, best);
  return written > 0 && (size_t)written < cap;
}

static char *
read_file(const char *path) {
  FILE *fp = fopen(path, "rb");
  char *buf = NULL;

  if (fp == NULL)
    return NULL;
  if (fseek(fp, 0, SEEK_END) == 0) {
    long sz = ftell(fp);

    if (sz >= 0 && fseek(fp, 0, SEEK_SET) == 0) {
      buf = malloc((size_t)sz + 1);
      if (buf != NULL) {
        size_t rd = fread(buf, 1, (size_t)sz, fp);

        buf[rd] = '\0';
      }
    }
  }
  fclose(fp);
  return buf;
}

static bool
load_local(const char *artist, const char *title) {
  char path[1024];
  char *text = NULL;

  if (lyrics_path(artist, title, "lrc", path, sizeof(path)))
    text = read_file(path);
  /* the exact name is what save_local writes, anything else is a name the
   * user brought with them, so it only gets looked for once that misses */
  if (text == NULL && match_local(artist, title, path, sizeof(path)))
    text = read_file(path);
  if (text == NULL)
    return false;
  set_text(text);
  free(text);
  return true;
}

static void
save_local(const char *artist, const char *title, const char *text) {
  char dir[700];
  char path[1024];
  FILE *fp;

  if (!lyrics_dir(dir, sizeof(dir)))
    return;
  mkdir(dir, 0755);

  if (!lyrics_path(artist, title, "lrc", path, sizeof(path)))
    return;
  fp = fopen(path, "wb");
  if (fp == NULL)
    return;
  fputs(text, fp);
  fclose(fp);
}

static void
try_next_provider(void) {
  while (provider_next < provider_count) {
    LyricsProvider fetch = providers[provider_next++];

    if (fetch == NULL)
      continue;
    status = LYRICS_LOADING;
    fetch(cur_artist, cur_title);
    return;
  }
  lines_clear();
  status = LYRICS_NONE;
}

static void
sync_current(void) {
  const char *artist = get_artist();
  const char *title = get_title();
  if (artist == NULL)
    artist = "";
  if (title == NULL)
    title = "";

  if (strcmp(artist, cur_artist) != 0 || strcmp(title, cur_title) != 0) {
    snprintf(cur_artist, sizeof(cur_artist), "%s", artist);
    snprintf(cur_title, sizeof(cur_title), "%s", title);

    lines_clear();

    status = LYRICS_EMPTY;

    provider_next = 0;
  }

  if (cur_title[0] == '\0') {
    status = LYRICS_EMPTY;
    return;
  }
  if (status != LYRICS_EMPTY)
    return;

  if (load_local(cur_artist, cur_title))
    return;

  try_next_provider();
}

void
lyrics_set_provider(LyricsProvider fn) {
  if (fn == NULL || provider_count == MAX_PROVIDERS)
    return;
  providers[provider_count++] = fn;
}

void
lyrics_init(void) {
#ifdef PATCH_lrclib
  lyrics_set_provider(lrclib_provider);
#endif

  /* Add other providers here */
}

void
lyrics_prefetch(void) {
  sync_current();
}

void
lyrics_set(const char *artist, const char *title, const char *text) {
  bool have = text != NULL && text[0] != '\0';
  if (have)
    save_local(artist, title, text);

  if (strcmp(artist, cur_artist) != 0 || strcmp(title, cur_title) != 0)
    return;

  if (have) {
    set_text(text);
  } else {
    try_next_provider();
    if (status == LYRICS_LOADING)
      return;
  }
  ui_redraw(REDRAW_PLAYER);
}

void
lyrics_scroll(int delta) {
  if (synced)
    return;
  scroll_off += delta;
  if (scroll_off < 0)
    scroll_off = 0;
}

void
lyrics_scroll_edge(int bottom) {
  if (synced)
    return;
  scroll_off = bottom ? line_count : 0;
}

int
lyrics_page_rows(void) {
  return view_height > 0 ? view_height : 1;
}

static int block_left;
static int block_w;

static void
compute_block(int width) {
  int i;

  block_w = 0;
  for (i = 0; i < line_count; i++) {
    int line_width = text_width(lines[i].text);

    if (line_width > block_w)
      block_w = line_width;
  }
  if (block_w > width)
    block_w = width;
  block_left = (width - block_w) / 2;
  if (block_left < 0)
    block_left = 0;
}

static void
draw_line(WINDOW *win, int row, int width, int idx, int cur) {
  int slot; 
  int line_width, col;

  if (row < 0)
    return;
  slot = synced ? (idx == cur ? STYLE_ACTIVE : STYLE_DEFAULT) : STYLE_DEFAULT;

  line_width = text_width(lines[idx].text);
  switch (lyrics_align) {
    case ALIGN_LEFT:
      col = block_left;
      break;
    case ALIGN_RIGHT:
      col = block_left + block_w - line_width;
      break;
    case ALIGN_CENTER: /* FALLTHROUGH */
    default:
      col = (width - line_width) / 2;
      break;
  }
  if (col < 0)
    col = 0;

  style_on(win, slot);
  draw_text(win, row, col, width - col, lines[idx].text);
  style_off(win, slot);
}

void
draw_lyrics(WINDOW *win) {
  int height, width;
  int i, row, cur, top;

  sync_current();

  getmaxyx(win, height, width);
  view_height = height;

  if (status != LYRICS_LOADED) {
    const char *msg = status == LYRICS_LOADING
      ? "Fetching lyrics..."
      : status == LYRICS_NONE ? "No lyrics found"
      : "No song playing";

    style_on(win, STYLE_DEFAULT);
    draw_text_centered(win, height / 2, 0, width, msg);
    style_off(win, STYLE_DEFAULT);
    return;
  }

  cur = current_line();
  compute_block(width);

  if (line_count <= height) {
    int start = (height - line_count) / 2;

    for (i = 0; i < line_count; i++)
      draw_line(win, start + i, width, i, cur);
    return;
  }

  if (synced) {
    int anchor = cur >= 0 ? cur : 0;

    top = anchor - height / 2;
  } else {
    top = scroll_off;
  }
  if (top > line_count - height)
    top = line_count - height;
  if (top < 0)
    top = 0;
  if (!synced)
    scroll_off = top;

  for (row = 0; row < height; row++) {
    int idx = top + row;

    if (idx >= line_count)
      break;
    draw_line(win, row, width, idx, cur);
  }
}
