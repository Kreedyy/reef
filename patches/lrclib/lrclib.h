#pragma once
#include "types.h"

void lrclib_provider(const char *artist, const char *title);

bool lrclib_sync_active(void);
bool lrclib_insert_mode_active(void);
void lrclib_set_insert_mode(const Arg *arg);
void lrclib_clear(const Arg *arg);

/* hijacks keyboard input */
void lrclib_handle_input(int input);

void lrclib_move(int delta);
void lrclib_edge(int dir);
int lrclib_page_rows(void);

void draw_lrclib_sync(WINDOW *win);

#define LRCLIB_TABS \
{ "Sync", draw_lrclib_sync, REDRAW_FOCUS | REDRAW_KEYPRESS },

static const Keybind lrclib_keybinds[] = {
  /* key  function       argument */
  { 'i',  lrclib_set_insert_mode,   { .b = true  } },
  {  27,  lrclib_set_insert_mode,   { .b = false } },
  { 'C',  lrclib_clear,             {0} },
};

#define LRCLIB_KEYBINDS \
  TAB_KEYBINDS(draw_lrclib_sync, lrclib_keybinds),
