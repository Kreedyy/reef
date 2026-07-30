#pragma once
#include <mpd/client.h>
#include <stdbool.h>

#include "types.h"

/* layouts can use these for customization */
const char *get_title(void);
const char *get_artist(void);
const char *get_album(void);
unsigned get_bitrate(void);
int get_volume(void);
bool get_repeat(void);
bool get_random(void);
bool get_single(void);
bool get_consume(void);
bool is_playing(void);
int get_current_song_id(void);
bool has_song_loaded(void);
unsigned get_elapsed_time(void);
unsigned get_elapsed_ms(void);
unsigned get_total_time(void);

/* keybinds can use these */
void play_next(const Arg *arg);
void play_prev(const Arg *arg);
void seek_seconds(const Arg *args);
void toggle_pause(const Arg *arg);
void set_volume(const Arg *arg);
void toggle_repeat(const Arg *arg);
void toggle_random(const Arg *arg);
void toggle_single(const Arg *arg);
void toggle_consume(const Arg *arg);
void update_database(const Arg *arg);
void clear_queue(const Arg *arg);

/* layouts and keybinds probably do not need the rest */
typedef struct {
  char artist[256];
  char title[256];
  char album[256];
  char uri[512];
  unsigned duration; /* seconds */
  int id;
  int pos;
} Song;

typedef struct {
  Song *items;
  int count;
  int cap;
} SongList;

typedef struct {
  char path[512];
  char name[256];
} Dir;

typedef struct {
  Dir *items;
  int count;
  int cap;
} DirList;

typedef struct {
  char uri[512];
  char title[256];
  char artist[256];
  char album[256];
  char album_artist[256];
  char genre[256];
  char date[64];
  char track[64];
  unsigned duration;
  long mtime; /* last modified, seconds since epoch. 0 if unknown */
  bool valid;
} SongInfo;

/* each returns a stable pointer and reloads itself lazily:
 * the queue when MPD's queue changes, the library when the database
 * is updated (reef signals that via mpd_invalidate_library). */
const SongList *mpd_queue(void);
const SongList *mpd_library(void);
void mpd_invalidate_library(void);

void mpd_browse(const char *path, DirList *dirs, SongList *songs);
bool mpd_song_info(const char *uri, SongInfo *out);
void mpd_free_dirlist(DirList *list);

const char *mpd_music_directory(void);

typedef struct {
  const char *any;
  const char *artist;
  const char *album;
  const char *album_artist;
  const char *title;
  const char *filename;
  const char *genre;
  bool exact;
} SearchQuery;

/* run q and fill out. With no constraints the result is empty (rather than
 * the entire library). */
void mpd_search(const SearchQuery *q, SongList *out);

void queue_add(const char *uri);
void queue_add_and_play(const char *uri);
void queue_play_id(int id);
void queue_delete_id(int id);

bool init_mpd(void);
bool mpd_connected(void);
bool mpd_error_active(void);
const char *mpd_error(void);

void destroy_mpd(void);

enum mpd_state get_player_state(void);
int mpd_idle_fd(void);
enum mpd_idle mpd_drain_events(void);
void mpd_refresh_status(void);

void mpd_drop_connection(void);
