#pragma once

#include <stdbool.h>

#include "keybinds.h"
#include "layout.h"
#include "lyrics.h"
#include "mpd.h"
#include "theme.h"
#include "types.h"
#include "ui.h"

#ifdef PATCH_example
#include "example.h"
#endif

#ifdef PATCH_lrclib
#include "lrclib.h"
#endif

static const Pane tabs[] = {
  /* name     draw              redraw on */
  { "Queue",  draw_now_playing, REDRAW_PLAYER | REDRAW_QUEUE },
  { "Browse", draw_browse,      REDRAW_DATABASE },  
  { "Search", draw_search,      REDRAW_DATABASE },
  { "Lyrics", draw_lyrics,      REDRAW_PLAYER | REDRAW_TICK },

#ifdef PATCH_lrclib
  LRCLIB_TABS
#endif

#ifdef PATCH_example
  EXAMPLE_TABS
#endif
};

static const int starting_tab = 0;

/* the minimum duration needed for a play previous track key stroke
 * to actually play prev track instead of restarting current track */
static const int minimum_duration_for_prev_track = 5; /* seconds */

/* Change progress bar characters, ' ' will color an entire column
 *  ====> <--Head
 *  ^^^^
 *  Tail
 * */
static const char progress_bar_tail = '='; /* default: '=' */
static const char progress_bar_head = '>'; /* default: '>' */

static const int padding_left = 1;
static const int padding_right = 1;

/* where downloaded .lrc lyrics are stored. A bare name (the default) is a
 * subdirectory of MPD's music directory, falling back to $HOME when MPD will
 * not report it; an absolute path or ~/path is used as-is.
 * default: ".lrc" */
static const char *const lyrics_directory = ".lrc";

/* default: 1
 * options: 0 = off, 1 = on */
#define SHOW_KEYBIND_BAR 1

#define KEYBIND_BAR_MAX_ROWS 2

/* gap between columns inside the Queue tab */
#define QUEUE_COLUMN_GAP 2

/* default: ALIGN_LEFT
 * options: ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT */
static const LyricsAlign lyrics_align = ALIGN_LEFT;

static const int seek_amount_seconds = 5;

static const int delta_volume = 5;

/* if you wish to create your own layout, consult docs/layouts.md
 * Feel free to submit your custom layout
 * default: rmpc
 * options: rmpc, ncmpcpp */
#include "layouts/rmpc.h"

/* you can override any color of the theme below by defining it here, BEFORE
 * the #include. See themes/reef.h for the full list.
 *
 * #define COLOR_TITLE  0xcddfff
 * #define COLOR_ARTIST 0xff16f4
 * ...
 */

/* if you wish to create your own theme, consult docs/themes.md
 * Feel free to submit your custom theme
 * default: reef.h
 * options: reef.h, ncmpcpp.h, rmpc.h */
#include "themes/reef.h"


/* https://samuallb.github.io/ncurses/NCurses/Key.html */

#define KEY_TAB '\t'

/* Global keybinds */
static const Keybind keybinds[] = {
  /* key       function         argument */

  /* Playback */
  { 'p',       toggle_pause,    {0} },
  { '>',       play_next,       {0} },
  { '<',       play_prev,       { .i = minimum_duration_for_prev_track } },
  { 'g',       seek_seconds,    { .i = seek_amount_seconds } },
  { 'f',       seek_seconds,    { .i = -seek_amount_seconds } },

  /* Volume */
  { '+',       set_volume,      { .i = delta_volume } },
  { '-',       set_volume,      { .i = -delta_volume } },

  /* Playback modes */
  { 'z',       toggle_repeat,   {0} },
  { 'x',       toggle_random,   {0} },
  { 'c',       toggle_consume,  {0} },
  { 'v',       toggle_single,   {0} },

  /* Movement */
  { 'j',       cursor_move,     { .i = 1 } },  /* down */
  { KEY_DOWN,  cursor_move,     { .i = 1 } },
  { 'k',       cursor_move,     { .i = -1 } }, /* up */
  { KEY_UP,    cursor_move,     { .i = -1 } },
  { 'h',       nav,             { .i = -1 } }, /* left */
  { KEY_LEFT,  nav,             { .i = -1 } },
  { 'l',       nav,             { .i = 1 } },  /* right */
  { KEY_RIGHT, nav,             { .i = 1 } },
  { KEY_NPAGE, cursor_page,     { .i = 1 } },  /* page down */
  { KEY_PPAGE, cursor_page,     { .i = -1 } }, /* page up */
  { KEY_HOME,  cursor_edge,     { .i = -1 } }, /* go to top */
  { KEY_END,   cursor_edge,     { .i = 1 } },  /* go to bottom */

  { KEY_TAB,   cycle_tab,       { .i = 1 } },  /* next tab */
  { KEY_BTAB,  cycle_tab,       { .i = -1 } }, /* previous tab */

  /* Actions */
  { '\n',      play_selected,   {0} }, /* play highlighted */
  { KEY_ENTER, play_selected,   {0} },
  { 'a',       add_to_queue,    {0} }, /* add highlighted to queue */
  { 'd',       delete_selected, {0} }, /* remove highlighted */
  { KEY_DC,    delete_selected, {0} },
  { 'C',       clear_queue,     {0} }, /* clear queue */
  { ' ',       select_item,     {0} }, /* select */
  { KEY_IC,    select_item,     {0} },

  /* Find */
  { '/',       filter_results,  {0} }, /* find */
  { 'n',       find_next,       {0} }, /* go to next result */
  { 'N',       find_prev,       {0} }, /* go to previous result */

  /* Library */
  { 'U',       update_database, {0} }, /* update database */

  { 'q',       quit,            {0} }, /* quit */

  /* Patches (see docs/patches.md)*/

};

/* Tab specific keybinds, if you wish to rebind these
 * modify said patch's header file */
static const TabKeybind tab_keybinds[] = {

#ifdef PATCH_lrclib
  LRCLIB_KEYBINDS
#endif

#ifdef PATCH_example
  EXAMPLE_KEYBINDS
#endif

  { NULL, NULL, 0 },
};

/* columns are in % of queue width. Time column is fixed width and Track
 * will take up any remaining space to avoid gaps */
static const int col_pct_artist = 20;
static const int col_pct_track = 60;
static const int col_pct_album = 20;

/* fixed chars, 5 fits mm:ss */
static const int col_width_time = 5;
