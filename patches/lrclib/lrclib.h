#pragma once

void lrclib_provider(const char *artist, const char *title);

bool lrclib_sync_active();

/* hijacks keyboard input */
void lrclib_handle_input(int input);
int lrclib_get_input_key();
int lrclib_get_exit_key();

void draw_lrclib_sync(WINDOW *win);

#define LRCLIB_TABS \
{ "Sync", draw_lrclib_sync, REDRAW_FOCUS | REDRAW_KEYPRESS },
