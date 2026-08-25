
/* https://www.musicpd.org/doc/libmpdclient/files.html */

#include <mpd/client.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "config.h"
#include "cred.h"
#include "mpd.h"
#include "types.h"
#include "ui.h"

struct mpd_connection *mpd;

static struct mpd_connection *mpd_idle;

static const enum mpd_idle idle_mask = MPD_IDLE_PLAYER | MPD_IDLE_MIXER |
  MPD_IDLE_QUEUE | MPD_IDLE_DATABASE |
  MPD_IDLE_OPTIONS |
  MPD_IDLE_STORED_PLAYLIST;

static struct {
  int volume;
  enum mpd_state state;
  int song_id;
  unsigned elapsed_ms;
  unsigned total;
  unsigned kbit_rate;
  unsigned queue_version;
  bool repeat, random, single, consume;
} st = { -1, MPD_STATE_UNKNOWN, -1, 0, 0, 0, 0, 0, 0, 0, 0 };

static char song_title[256];
static char song_artist[256];
static char song_album[256];

static char mpd_error_buf[256];
static bool mpd_ok = false;

static unsigned long disconnected_at;
#define MPD_ERROR_GRACE_MS 500

#define MPD_TIMEOUT_MS 2000

/* cached listings, reloaded lazily.
 * See mpd_queue()/mpd_library() */
static SongList queue;
static unsigned queue_loaded_version;
static bool queue_loaded;

static SongList library;
static bool library_loaded;

static PlaylistList playlists;
static bool playlists_loaded;

static unsigned long
mono_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)ts.tv_sec * 1000 +
  (unsigned long)ts.tv_nsec / 1000000;
}

#define ELAPSED_RESYNC_MS 1000
#define ELAPSED_SETTLE_MS 250
#define ELAPSED_SETTLE_FOR_MS 2000
#define ELAPSED_JUMP_MS 100

static unsigned long elapsed_synced_at;
static unsigned long elapsed_settling_until;

static const char *
mpd_server_error_name(enum mpd_server_error error) {
  switch (error) {
    case MPD_SERVER_ERROR_UNK:
      return "MPD_SERVER_ERROR_UNK";
    case MPD_SERVER_ERROR_NOT_LIST:
      return "MPD_SERVER_ERROR_NOT_LIST";
    case MPD_SERVER_ERROR_ARG:
      return "MPD_SERVER_ERROR_ARG";
    case MPD_SERVER_ERROR_PASSWORD:
      return "MPD_SERVER_ERROR_PASSWORD";
    case MPD_SERVER_ERROR_PERMISSION:
      return "MPD_SERVER_ERROR_PERMISSION";
    case MPD_SERVER_ERROR_UNKNOWN_CMD:
      return "MPD_SERVER_ERROR_UNKNOWN_CMD";
    case MPD_SERVER_ERROR_NO_EXIST:
      return "MPD_SERVER_ERROR_NO_EXIST";
    case MPD_SERVER_ERROR_PLAYLIST_MAX:
      return "MPD_SERVER_ERROR_PLAYLIST_MAX";
    case MPD_SERVER_ERROR_SYSTEM:
      return "MPD_SERVER_ERROR_SYSTEM";
    case MPD_SERVER_ERROR_PLAYLIST_LOAD:
      return "MPD_SERVER_ERROR_PLAYLIST_LOAD";
    case MPD_SERVER_ERROR_UPDATE_ALREADY:
      return "MPD_SERVER_ERROR_UPDATE_ALREADY";
    case MPD_SERVER_ERROR_PLAYER_SYNC:
      return "MPD_SERVER_ERROR_PLAYER_SYNC";
    case MPD_SERVER_ERROR_EXIST:
      return "MPD_SERVER_ERROR_EXIST";
  }
  return "MPD_SERVER_ERROR_UNK";
}

static bool check_conn(struct mpd_connection *conn);

static void
set_error(const char *msg) {
  snprintf(mpd_error_buf, sizeof(mpd_error_buf), "mpd: %s", msg ? msg :
           "Unknown error");
}

static void
clear_status(void) {
  st.volume = -1;
  st.state = MPD_STATE_UNKNOWN;
  st.song_id = -1;
  st.elapsed_ms = st.total = 0;
  st.kbit_rate = 0;
  st.queue_version = 0;
  st.repeat = st.random = st.single = st.consume = false;
  song_title[0] = song_artist[0] = song_album[0] = '\0';

  library.count = 0;
  library_loaded = false;

  playlists.count = 0;
  playlists_loaded = false;
}

static void
copy_tag(char *dst, size_t size, const struct mpd_song *song,
         enum mpd_tag_type tag) {
  const char *value = mpd_song_get_tag(song, tag, 0);

  snprintf(dst, size, "%s", value ? value : "");
}

static void
refresh_song_tags(void) {
  struct mpd_song *song;

  song_title[0] = song_artist[0] = song_album[0] = '\0';

  if (mpd == NULL)
    return;

  song = mpd_run_current_song(mpd);

  if (!check_conn(mpd) || song == NULL)
    return;

  copy_tag(song_title, sizeof(song_title), song, MPD_TAG_TITLE);
  copy_tag(song_artist, sizeof(song_artist), song, MPD_TAG_ARTIST);
  copy_tag(song_album, sizeof(song_album), song, MPD_TAG_ALBUM);

  if (song_title[0] == '\0')
    snprintf(song_title, sizeof(song_title), "%s",
             mpd_song_get_uri(song));

  mpd_song_free(song);
}

static Song *
songlist_push(SongList *list) {
  if (list->count == list->cap) {
    int cap = list->cap ? list->cap * 2 : 64;

    Song *items =
      realloc(list->items, (size_t)cap * sizeof(*items));

    if (items == NULL)
      return NULL;

    list->items = items;
    list->cap = cap;
  }
  return &list->items[list->count++];
}

static void
song_from_mpd(Song *dst, const struct mpd_song *song) {
  copy_tag(dst->artist, sizeof(dst->artist), song, MPD_TAG_ARTIST);
  copy_tag(dst->title, sizeof(dst->title), song, MPD_TAG_TITLE);
  copy_tag(dst->album, sizeof(dst->album), song, MPD_TAG_ALBUM);
  snprintf(dst->uri, sizeof(dst->uri), "%s", mpd_song_get_uri(song));

  if (dst->title[0] == '\0')
    snprintf(dst->title, sizeof(dst->title), "%s",
             mpd_song_get_uri(song));

  dst->duration = mpd_song_get_duration(song);
  dst->id = (int)mpd_song_get_id(song);
  dst->pos = (int)mpd_song_get_pos(song);
}

static void
load_queue(void) {
  struct mpd_song *song;

  queue.count = 0;

  if (mpd == NULL)
    return;

  if (!mpd_send_list_queue_meta(mpd)) {
    check_conn(mpd);
    return;
  }

  while ((song = mpd_recv_song(mpd)) != NULL) {
    Song *slot = songlist_push(&queue);

    if (slot != NULL)
      song_from_mpd(slot, song);

    mpd_song_free(song);
  }

  if (!mpd_response_finish(mpd))
    check_conn(mpd);

  queue_loaded = true;
  queue_loaded_version = st.queue_version;
}

const SongList *
mpd_queue(void) {
  if (mpd != NULL &&
    (!queue_loaded || st.queue_version != queue_loaded_version))
    load_queue();
  return &queue;
}

static void
load_library(void) {
  struct mpd_entity *entity;

  library.count = 0;

  if (mpd == NULL)
    return;

  if (!mpd_send_list_all_meta(mpd, NULL)) {
    check_conn(mpd);
    return;
  }

  while ((entity = mpd_recv_entity(mpd)) != NULL) {
    if (mpd_entity_get_type(entity) == MPD_ENTITY_TYPE_SONG) {
      Song *slot = songlist_push(&library);
      if (slot != NULL) {
        song_from_mpd(slot,
                      mpd_entity_get_song(entity));

        slot->id = -1;
        slot->pos = -1;
      }
    }

    mpd_entity_free(entity);
  }

  if (!mpd_response_finish(mpd))
    check_conn(mpd);

  library_loaded = true;
}

const SongList *
mpd_library(void) {
  if (mpd != NULL && !library_loaded)
    load_library();
  return &library;
}

void
mpd_invalidate_library(void) {
  library_loaded = false;
}

static Playlist *
playlists_push(PlaylistList *list) {
  if (list->count == list->cap) {
    int cap = list->cap ? list->cap * 2 : 16;
    Playlist *items =
      realloc(list->items, (size_t)cap * sizeof(*items));

    if (items == NULL)
      return NULL;

    list->items = items;
    list->cap = cap;
  }
  return &list->items[list->count++];
}

static int
playlist_cmp(const void *a, const void *b) {
  return strcasecmp(((const Playlist *)a)->name,
                    ((const Playlist *)b)->name);
}

static void
load_playlists(void) {
  struct mpd_playlist *pl;

  playlists.count = 0;

  if (mpd == NULL)
    return;

  if (!mpd_send_list_playlists(mpd)) {
    check_conn(mpd);
    return;
  }

  while ((pl = mpd_recv_playlist(mpd)) != NULL) {
    Playlist *slot = playlists_push(&playlists);

    if (slot != NULL)
      snprintf(slot->name, sizeof(slot->name), "%s",
               mpd_playlist_get_path(pl));

    mpd_playlist_free(pl);
  }

  if (!mpd_response_finish(mpd))
    check_conn(mpd);

  if (playlists.count > 1)
    qsort(playlists.items, (size_t)playlists.count,
          sizeof(*playlists.items), playlist_cmp);

  playlists_loaded = true;
}

const PlaylistList *
mpd_playlists(void) {
  if (mpd != NULL && !playlists_loaded)
    load_playlists();
  return &playlists;
}

void
mpd_invalidate_playlists(void) {
  playlists_loaded = false;
}

void
mpd_playlist_songs(const char *name, SongList *out) {
  struct mpd_song *song;

  out->count = 0;

  if (mpd == NULL || name == NULL)
    return;

  if (!mpd_send_list_playlist_meta(mpd, name)) {
    check_conn(mpd);
    return;
  }

  while ((song = mpd_recv_song(mpd)) != NULL) {
    Song *slot = songlist_push(out);

    if (slot != NULL) {
      song_from_mpd(slot, song);

      slot->id = -1;
      slot->pos = out->count - 1;
    }

    mpd_song_free(song);
  }

  if (!mpd_response_finish(mpd))
    check_conn(mpd);
}

static Dir *
dirlist_push(DirList *list) {
  if (list->count == list->cap) {
    int cap = list->cap ? list->cap * 2 : 32;
    Dir *items =
      realloc(list->items, (size_t)cap * sizeof(*items));
    if (items == NULL)
      return NULL;
    list->items = items;
    list->cap = cap;
  }
  return &list->items[list->count++];
}

void
mpd_free_dirlist(DirList *list) {
  free(list->items);
  list->items = NULL;
  list->cap = list->count = 0;
}

static const char *
base_name(const char *path) {
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

void
mpd_browse(const char *path, DirList *dirs, SongList *songs) {
  struct mpd_entity *entity;

  dirs->count = 0;
  songs->count = 0;

  if (mpd == NULL)
    return;

  if (!mpd_send_list_meta(mpd, path ? path : "")) {
    check_conn(mpd);
    return;
  }

  while ((entity = mpd_recv_entity(mpd)) != NULL) {
    enum mpd_entity_type type = mpd_entity_get_type(entity);

    if (type == MPD_ENTITY_TYPE_DIRECTORY) {
      const char *p = mpd_directory_get_path(
        mpd_entity_get_directory(entity));
      Dir *slot = dirlist_push(dirs);

      if (slot != NULL) {
        snprintf(slot->path, sizeof(slot->path), "%s",
                 p);
        snprintf(slot->name, sizeof(slot->name), "%s",
                 base_name(p));
      }
    } else if (type == MPD_ENTITY_TYPE_SONG) {
      Song *slot = songlist_push(songs);

      if (slot != NULL) {
        song_from_mpd(slot,
                      mpd_entity_get_song(entity));

        slot->id = -1;
        slot->pos = -1;
      }
    }

    mpd_entity_free(entity);
  }

  if (!mpd_response_finish(mpd))
    check_conn(mpd);
}

bool
mpd_song_info(const char *uri, SongInfo *out) {
  struct mpd_entity *entity;

  out->valid = false;

  if (mpd == NULL || uri == NULL)
    return false;

  if (!mpd_send_list_meta(mpd, uri)) {
    check_conn(mpd);
    return false;
  }

  while ((entity = mpd_recv_entity(mpd)) != NULL) {
    if (!out->valid &&
      mpd_entity_get_type(entity) == MPD_ENTITY_TYPE_SONG) {
      const struct mpd_song *song =
        mpd_entity_get_song(entity);

      snprintf(out->uri, sizeof(out->uri), "%s",
               mpd_song_get_uri(song));
      copy_tag(out->title, sizeof(out->title), song,
               MPD_TAG_TITLE);
      copy_tag(out->artist, sizeof(out->artist), song,
               MPD_TAG_ARTIST);
      copy_tag(out->album, sizeof(out->album), song,
               MPD_TAG_ALBUM);
      copy_tag(out->album_artist, sizeof(out->album_artist),
               song, MPD_TAG_ALBUM_ARTIST);
      copy_tag(out->genre, sizeof(out->genre), song,
               MPD_TAG_GENRE);
      copy_tag(out->date, sizeof(out->date), song,
               MPD_TAG_DATE);
      copy_tag(out->track, sizeof(out->track), song,
               MPD_TAG_TRACK);

      out->duration = mpd_song_get_duration(song);
      out->mtime = (long)mpd_song_get_last_modified(song);
      out->valid = true;
    }
    mpd_entity_free(entity);
  }

  if (!mpd_response_finish(mpd))
    check_conn(mpd);

  return out->valid;
}

/* mpd's music_directory or "" when it can't be learned. mpd only reveals it
 * via the config command to clients on a LOCAL SOCKET. reef's main
 * connection is usually TCP (MPD_HOST defaults to localhost), where the
 * command is refused. So ask over a dedicated local socket connection. */
const char *
mpd_music_directory(void) {
  static char dir[512];
  static bool cached;

  const char *cands[4];
  const char *host, *xdg, *home;
  char xdg_sock[512], home_sock[512];
  int i, n = 0;

  if (cached)
    return dir;

  cached = true;

  dir[0] = '\0';

  host = getenv("MPD_HOST");

  if (host != NULL && (host[0] == '/' || host[0] == '@'))
    cands[n++] = host;

  xdg = getenv("XDG_RUNTIME_DIR");

  if (xdg != NULL &&
    snprintf(xdg_sock, sizeof(xdg_sock), "%s/mpd/socket", xdg) <
    (int)sizeof(xdg_sock))
    cands[n++] = xdg_sock;

  cands[n++] = "/run/mpd/socket";

  home = getenv("HOME");

  if (home != NULL &&
    snprintf(home_sock, sizeof(home_sock), "%s/.config/mpd/socket",
             home) < (int)sizeof(home_sock))
    cands[n++] = home_sock;

  for (i = 0; i < n && dir[0] == '\0'; i++) {
    struct mpd_connection *c = mpd_connection_new(cands[i], 0, MPD_TIMEOUT_MS);

    if (c == NULL)
      continue;

    if (mpd_connection_get_error(c) == MPD_ERROR_SUCCESS &&
      mpd_send_command(c, "config", NULL)) {
      struct mpd_pair *pair =
        mpd_recv_pair_named(c, "music_directory");

      if (pair != NULL) {
        snprintf(dir, sizeof(dir), "%s", pair->value);
        mpd_return_pair(c, pair);
      }

      mpd_response_finish(c);
    }

    mpd_connection_free(c);
  }
  return dir;
}

void
mpd_search(const SearchQuery *q, SongList *out) {
  struct mpd_song *song;
  bool any;

  out->count = 0;
  if (mpd == NULL)
    return;

  any = (q->any && q->any[0]) || (q->artist && q->artist[0]) ||
    (q->album && q->album[0]) ||
    (q->album_artist && q->album_artist[0]) ||
    (q->title && q->title[0]) || (q->filename && q->filename[0]) ||
    (q->genre && q->genre[0]);
  if (!any)
    return;

  if (!mpd_search_db_songs(mpd, q->exact)) {
    check_conn(mpd);
    return;
  }
  if (q->any && q->any[0])
    mpd_search_add_any_tag_constraint(mpd, MPD_OPERATOR_DEFAULT,
                                      q->any);
  if (q->artist && q->artist[0])
    mpd_search_add_tag_constraint(mpd, MPD_OPERATOR_DEFAULT,
                                  MPD_TAG_ARTIST, q->artist);
  if (q->album && q->album[0])
    mpd_search_add_tag_constraint(mpd, MPD_OPERATOR_DEFAULT,
                                  MPD_TAG_ALBUM, q->album);
  if (q->album_artist && q->album_artist[0])
    mpd_search_add_tag_constraint(mpd, MPD_OPERATOR_DEFAULT,
                                  MPD_TAG_ALBUM_ARTIST,
                                  q->album_artist);
  if (q->title && q->title[0])
    mpd_search_add_tag_constraint(mpd, MPD_OPERATOR_DEFAULT,
                                  MPD_TAG_TITLE, q->title);
  if (q->genre && q->genre[0])
    mpd_search_add_tag_constraint(mpd, MPD_OPERATOR_DEFAULT,
                                  MPD_TAG_GENRE, q->genre);
  if (q->filename && q->filename[0])
    mpd_search_add_uri_constraint(mpd, MPD_OPERATOR_DEFAULT,
                                  q->filename);

  if (!mpd_search_commit(mpd)) {
    check_conn(mpd);
    return;
  }

  while ((song = mpd_recv_song(mpd)) != NULL) {
    Song *slot = songlist_push(out);

    if (slot != NULL) {
      song_from_mpd(slot, song);
      slot->id = -1;
      slot->pos = -1;
    }

    mpd_song_free(song);
  }

  if (!mpd_response_finish(mpd))
    check_conn(mpd);
}

const char *
get_title(void) {
  return song_title;
}

const char *
get_artist(void) {
  return song_artist;
}

const char *
get_album(void) {
  return song_album;
}

unsigned
get_bitrate(void) {
  return st.kbit_rate;
}

void
mpd_drop_connection(void) {
  if (mpd_idle != NULL) {
    mpd_connection_free(mpd_idle);
    mpd_idle = NULL;
  }

  if (mpd != NULL) {
    mpd_connection_free(mpd);
    mpd = NULL;
  }

  if (mpd_ok)
    disconnected_at = mono_ms();

  mpd_ok = false;
  clear_status();
}

static bool
check_conn(struct mpd_connection *conn) {
  enum mpd_error err;

  if (conn == NULL)
    return false;

  err = mpd_connection_get_error(conn);
  if (err == MPD_ERROR_SUCCESS)
    return true;

  if (err == MPD_ERROR_SERVER)
    set_error(mpd_server_error_name(
      mpd_connection_get_server_error(conn)));
  else if (err == MPD_ERROR_SYSTEM)
    set_error(strerror(mpd_connection_get_system_error(conn)));
  else
    set_error(mpd_connection_get_error_message(conn));

  if (!mpd_connection_clear_error(conn)) {
    mpd_drop_connection();
    return false;
  }

  return true;
}

static bool
mpd_authenticate(void) {
  char *pw;
  bool ok = false;

  if (mpd_password_cmd == NULL || mpd_password_cmd[0] == '\0')
    return true;

  pw = ui_cred_get(mpd_password_cmd);
  if (pw == NULL) {
    set_error("password command gave nothing");
    return false;
  }

  if (!mpd_run_password(mpd, pw))
    set_error(mpd_connection_get_error_message(mpd));
  else if (!mpd_run_password(mpd_idle, pw))
    set_error(mpd_connection_get_error_message(mpd_idle));
  else
    ok = true;

  cred_free(pw);
  return ok;
}

bool
init_mpd(void) {
  enum mpd_error err;

  mpd_drop_connection();
  mpd_error_buf[0] = '\0';

  mpd = mpd_connection_new(NULL, 0, MPD_TIMEOUT_MS);
  if (mpd == NULL) {
    set_error("out of memory");
    return false;
  }

  err = mpd_connection_get_error(mpd);
  if (err != MPD_ERROR_SUCCESS) {
    switch (err) {
      case MPD_ERROR_SERVER:
        set_error(mpd_server_error_name(
          mpd_connection_get_server_error(mpd)));
        break;
      case MPD_ERROR_SYSTEM:
        set_error(strerror(
          mpd_connection_get_system_error(mpd)));
        break;
      default:
        set_error(mpd_connection_get_error_message(mpd));
        break;
    }
    mpd_drop_connection();
    return false;
  }

  mpd_idle = mpd_connection_new(NULL, 0, MPD_TIMEOUT_MS);
  if (mpd_idle == NULL ||
    mpd_connection_get_error(mpd_idle) != MPD_ERROR_SUCCESS) {
    set_error(mpd_idle ? mpd_connection_get_error_message(mpd_idle)
              : "out of memory");
    mpd_drop_connection();
    return false;
  }

  if (!mpd_authenticate()) {
    mpd_drop_connection();
    return false;
  }

  mpd_ok = true;
  mpd_refresh_status();
  mpd_send_idle_mask(mpd_idle, idle_mask);
  return true;
}

int
mpd_idle_fd(void) {
  return mpd_idle ? mpd_connection_get_fd(mpd_idle) : -1;
}

enum mpd_idle
mpd_drain_events(void) {
  enum mpd_idle events;

  if (mpd_idle == NULL)
    return 0;

  events = mpd_recv_idle(mpd_idle, 0);

  if (!mpd_response_finish(mpd_idle) && !check_conn(mpd_idle))
    return 0;

  if (!mpd_send_idle_mask(mpd_idle, idle_mask) && !check_conn(mpd_idle))
    return 0;

  return events;
}

static bool
elapsed_moved(unsigned expected, unsigned reported) {
  unsigned gap = reported > expected ? reported - expected :
    expected - reported;

  return gap > ELAPSED_JUMP_MS;
}

void
mpd_refresh_status(void) {
  struct mpd_status *status;
  enum mpd_state previous_state;
  unsigned expected;
  int previous_song_id;

  if (mpd == NULL)
    return;

  previous_song_id = st.song_id;
  previous_state = st.state;
  expected = get_elapsed_ms();

  status = mpd_run_status(mpd);
  if (status == NULL) {
    clear_status();
    check_conn(mpd);
    return;
  }

  st.volume = mpd_status_get_volume(status);
  st.state = mpd_status_get_state(status);
  st.song_id = mpd_status_get_song_id(status);
  st.elapsed_ms = mpd_status_get_elapsed_ms(status);
  st.total = mpd_status_get_total_time(status);
  st.kbit_rate = mpd_status_get_kbit_rate(status);
  st.queue_version = mpd_status_get_queue_version(status);
  st.repeat = mpd_status_get_repeat(status);
  st.random = mpd_status_get_random(status);
  st.single = mpd_status_get_single(status);
  st.consume = mpd_status_get_consume(status);
  mpd_status_free(status);

  elapsed_synced_at = mono_ms();

  if (st.state == MPD_STATE_PLAY &&
    (previous_state != MPD_STATE_PLAY ||
    elapsed_moved(expected, st.elapsed_ms)))
    elapsed_settling_until = elapsed_synced_at + ELAPSED_SETTLE_FOR_MS;

  if (st.song_id != previous_song_id)
    refresh_song_tags();
}

void
mpd_resync_elapsed(void) {
  unsigned long now, interval;

  if (mpd == NULL || st.state != MPD_STATE_PLAY)
    return;

  now = mono_ms();
  interval = now < elapsed_settling_until ? ELAPSED_SETTLE_MS :
    ELAPSED_RESYNC_MS;

  if (now - elapsed_synced_at < interval)
    return;

  mpd_refresh_status();
}

bool
mpd_connected(void) {
  return mpd_ok;
}

bool
mpd_error_active(void) {
  return !mpd_ok && mono_ms() - disconnected_at >= MPD_ERROR_GRACE_MS;
}

const char *
mpd_error(void) {
  return mpd_error_buf;
}

void
play_next(const Arg *arg) {
  (void)arg;
  if (mpd == NULL)
    return;

  mpd_run_next(mpd);
  check_conn(mpd);
}

void
play_prev(const Arg *arg) {
  int threshold;

  if (mpd == NULL)
    return;

  threshold = arg != NULL ? arg->i : 0;
  if (threshold > 0 && (int)get_elapsed_time() >= threshold)
    mpd_run_seek_current(mpd, 0, 0);
  else
    mpd_run_previous(mpd);

  check_conn(mpd);
}

void
seek_seconds(const Arg *args) {
  const bool relative = true;

  if (mpd == NULL)
    return;

  mpd_run_seek_current(mpd, args->i, relative);
  check_conn(mpd);
}

void
toggle_pause(const Arg *arg) {
  enum mpd_state player_state;

  (void)arg;
  if (mpd == NULL)
    return;

  player_state = get_player_state();

  if (player_state == MPD_STATE_UNKNOWN ||
    player_state == MPD_STATE_STOP)
    return;

  mpd_run_pause(mpd, player_state == MPD_STATE_PLAY);
  check_conn(mpd);
}

void
toggle_repeat(const Arg *arg) {
  struct mpd_status *status;
  bool enabled;

  (void)arg;
  if (mpd == NULL)
    return;

  status = mpd_run_status(mpd);
  if (status == NULL) {
    check_conn(mpd);
    return;
  }
  enabled = mpd_status_get_repeat(status);
  mpd_status_free(status);

  mpd_run_repeat(mpd, !enabled);
  check_conn(mpd);
}

void
toggle_random(const Arg *arg) {
  struct mpd_status *status;
  bool enabled;

  (void)arg;
  if (mpd == NULL)
    return;

  status = mpd_run_status(mpd);
  if (status == NULL) {
    check_conn(mpd);
    return;
  }
  enabled = mpd_status_get_random(status);
  mpd_status_free(status);

  mpd_run_random(mpd, !enabled);
  check_conn(mpd);
}

void
toggle_single(const Arg *arg) {
  struct mpd_status *status;
  bool enabled;

  (void)arg;
  if (mpd == NULL)
    return;

  status = mpd_run_status(mpd);
  if (status == NULL) {
    check_conn(mpd);
    return;
  }
  enabled = mpd_status_get_single(status);
  mpd_status_free(status);

  mpd_run_single(mpd, !enabled);
  check_conn(mpd);
}

void
toggle_consume(const Arg *arg) {
  struct mpd_status *status;
  bool enabled;

  (void)arg;
  if (mpd == NULL)
    return;

  status = mpd_run_status(mpd);
  if (status == NULL) {
    check_conn(mpd);
    return;
  }
  enabled = mpd_status_get_consume(status);
  mpd_status_free(status);

  mpd_run_consume(mpd, !enabled);
  check_conn(mpd);
}

void
update_database(const Arg *arg) {
  (void)arg;
  if (mpd == NULL)
    return;

  mpd_run_update(mpd, NULL);
  check_conn(mpd);
}

void
queue_add(const char *uri) {
  if (mpd == NULL || uri == NULL)
    return;

  mpd_run_add(mpd, uri);
  check_conn(mpd);
}

void
queue_add_and_play(const char *uri) {
  int id;

  if (mpd == NULL || uri == NULL)
    return;

  id = mpd_run_add_id(mpd, uri);
  if (check_conn(mpd) && id >= 0) {
    mpd_run_play_id(mpd, id);
    check_conn(mpd);
  }
}

void
queue_play_id(int id) {
  if (mpd == NULL || id < 0)
    return;

  mpd_run_play_id(mpd, id);
  check_conn(mpd);
}

void
queue_delete_id(int id) {
  if (mpd == NULL || id < 0)
    return;

  mpd_run_delete_id(mpd, id);
  check_conn(mpd);
}

void
playlist_add(const char *name, const char *uri) {
  if (mpd == NULL || name == NULL || uri == NULL || name[0] == '\0')
    return;

  mpd_run_playlist_add(mpd, name, uri);
  check_conn(mpd);
}

void
playlist_delete_pos(const char *name, int pos) {
  if (mpd == NULL || name == NULL || name[0] == '\0' || pos < 0)
    return;

  mpd_run_playlist_delete(mpd, name, (unsigned)pos);
  check_conn(mpd);
}

void
playlist_remove(const char *name) {
  if (mpd == NULL || name == NULL || name[0] == '\0')
    return;

  mpd_run_rm(mpd, name);
  check_conn(mpd);
}

void
playlist_load(const char *name) {
  if (mpd == NULL || name == NULL || name[0] == '\0')
    return;

  mpd_run_load(mpd, name);
  check_conn(mpd);
}

void
clear_queue(const Arg *arg) {
  (void)arg;
  if (mpd == NULL)
    return;

  mpd_run_clear(mpd);
  check_conn(mpd);
}

bool
is_playing(void) {
  return st.state == MPD_STATE_PLAY;
}

bool
has_song_loaded(void) {
  return st.state == MPD_STATE_PLAY || st.state == MPD_STATE_PAUSE;
}

int
get_volume(void) {
  return st.volume;
}

bool
get_repeat(void) {
  return st.repeat;
}
bool
get_random(void) {
  return st.random;
}
bool
get_single(void) {
  return st.single;
}
bool
get_consume(void) {
  return st.consume;
}

unsigned
get_elapsed_ms(void) {
  unsigned long elapsed, total_ms;

  if (st.state != MPD_STATE_PLAY)
    return st.elapsed_ms;

  elapsed = st.elapsed_ms + (mono_ms() - elapsed_synced_at);
  total_ms = (unsigned long)st.total * 1000;
  if (total_ms && elapsed > total_ms)
    elapsed = total_ms;
  return (unsigned)elapsed;
}

unsigned
get_elapsed_time(void) {
  return get_elapsed_ms() / 1000;
}

unsigned
get_total_time(void) {
  return st.total;
}

void
set_volume(const Arg *arg) {
  if (mpd == NULL)
    return;

  mpd_run_change_volume(mpd, arg->i);
  check_conn(mpd);
}

enum mpd_state
get_player_state(void) {
  return st.state;
}

int
get_current_song_id(void) {
  return st.song_id;
}

void
destroy_mpd(void) {
  mpd_drop_connection();
  free(queue.items);
  queue.items = NULL;
  queue.cap = queue.count = 0;
  free(library.items);
  library.items = NULL;
  library.cap = library.count = 0;
  free(playlists.items);
  playlists.items = NULL;
  playlists.cap = playlists.count = 0;
}
