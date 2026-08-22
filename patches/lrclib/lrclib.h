#pragma once
#include "types.h"

void lrclib_provider(const char *artist, const char *title);

bool lrclib_sync_active(void);
bool lrclib_insert_mode_active(void);
void lrclib_set_insert_mode(const Arg *arg);
void lrclib_clear(const Arg *arg);

void lrclib_sync_line(const Arg *arg);

void lrclib_trim_trailing(void);

/* hijacks keyboard input */
void lrclib_handle_input(int input);

void lrclib_move(int delta);
void lrclib_edge(int dir);
int lrclib_page_rows(void);

void draw_lrclib_sync(WINDOW *win);

typedef void (*LrclibSolveCb)(const char *nonce, void *user);

bool lrclib_solve_start(const char *prefix, const char *target_hex,
    LrclibSolveCb done, void *user);

bool lrclib_solve_active(void);

bool lrclib_publish(const char *body);

void lrclib_publish_current(const Arg *arg);

void lrclib_save_lrc(const Arg *arg);

typedef enum {
  LRCLIB_PUBLISH_IDLE,
  LRCLIB_PUBLISH_BUSY,
  LRCLIB_PUBLISH_OK,
  LRCLIB_PUBLISH_ERROR
} LrclibPublishState;

LrclibPublishState lrclib_publish_state(void);

const char *lrclib_publish_message(void);

size_t lrclib_line_count(void);
const char *lrclib_line_text(size_t i, long *time_ms);

int lrclib_solve_fd(void);

void lrclib_solve_pump(void);

void lrclib_solve_cancel(void);

#define LRCLIB_TABS \
{ "Sync", draw_lrclib_sync, REDRAW_FOCUS | REDRAW_KEYPRESS },

#define KEY_ESC 27

static const Keybind lrclib_keybinds[] = {
  /* key       function                  argument */
  { 'i',       lrclib_set_insert_mode,   { .b = true  } },
  {  KEY_ESC,  lrclib_set_insert_mode,   { .b = false } },
  { 'C',       lrclib_clear,             {0} },
  { 'S',       lrclib_save_lrc,          {0} },
  { 'U',       lrclib_publish_current,   {0} },
  { ' ',       lrclib_sync_line,         {0} },
};

#define LRCLIB_KEYBINDS \
  TAB_KEYBINDS(draw_lrclib_sync, lrclib_keybinds),
