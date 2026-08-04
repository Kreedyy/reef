#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ncurses.h>

#include "http.h"
#include "json.h"
#include "keybinds.h"
#include "lrclib.h"
#include "lyrics.h"
#include "mpd.h"
#include "ui.h"

#define URL_CHALLENGE "https://lrclib.net/api/request-challenge"
#define URL_PUBLISH "https://lrclib.net/api/publish"

typedef struct {
  char prefix[65];
  char target[128];
  char *body;
} Request;

static void
request_free(Request *req) {
  if (req == NULL)
    return;
  free(req->body);
  free(req);
}

static LrclibPublishState pub_state;
static char pub_message[96];
static bool pub_synced; /* whether the body in flight carries timestamps */

static void
publish_status(LrclibPublishState state, const char *msg) {
  pub_state = state;
  snprintf(pub_message, sizeof(pub_message), "%s", msg);
  ui_redraw(REDRAW_FOCUS);
}

static void
publish_statusf(LrclibPublishState state, const char *msg, long code) {
  pub_state = state;
  snprintf(pub_message, sizeof(pub_message), "%.70s (%ld)", msg, code);
  ui_redraw(REDRAW_FOCUS);
}

LrclibPublishState
lrclib_publish_state(void) {
  return pub_state;
}

const char *
lrclib_publish_message(void) {
  return pub_message;
}

static void
on_published(const HttpResponse *resp, void *user) {
  if (!resp->ok)
    publish_status(LRCLIB_PUBLISH_ERROR, "Publish failed: no response");
  else if (resp->status >= 200 && resp->status < 300)
    publish_status(LRCLIB_PUBLISH_OK,
        pub_synced ? "Published synced" : "Published plain");
  else
    publish_statusf(LRCLIB_PUBLISH_ERROR, "Publish rejected", resp->status);

  request_free(user);
}

static void
on_solved(const char *nonce, void *user) {
  Request *req = user;
  char token[192];
  const char *headers[3];

  if (nonce == NULL) {
    request_free(req);
    return;
  }

  snprintf(token, sizeof(token), "X-Publish-Token: %s:%s", req->prefix, nonce);
  headers[0] = "Content-Type: application/json";
  headers[1] = token;
  headers[2] = NULL;

  if (http_post(URL_PUBLISH, req->body, headers, on_published, req)) {
    publish_status(LRCLIB_PUBLISH_BUSY, "Publishing...");
    return;
  }
  publish_status(LRCLIB_PUBLISH_ERROR, "Could not send the lyrics");
  request_free(req);
}

static void
on_challenge(const HttpResponse *resp, void *user) {
  Request *req = user;
  const char *end;

  if (!resp->ok) {
    publish_status(LRCLIB_PUBLISH_ERROR, "Challenge failed: no response");
    request_free(req);
    return;
  }
  if (resp->status < 200 || resp->status >= 300) {
    publish_statusf(LRCLIB_PUBLISH_ERROR, "Challenge refused", resp->status);
    request_free(req);
    return;
  }

  end = resp->data + resp->len;
  if (!json_string(resp->data, end, "prefix", req->prefix, sizeof(req->prefix)) ||
    !json_string(resp->data, end, "target", req->target, sizeof(req->target))) {
    publish_status(LRCLIB_PUBLISH_ERROR, "Challenge made no sense");
    request_free(req);
    return;
  }

  if (lrclib_solve_start(req->prefix, req->target, on_solved, req)) {
    publish_status(LRCLIB_PUBLISH_BUSY, "Solving challenge...");
    return;
  }
  publish_status(LRCLIB_PUBLISH_ERROR, "Could not start the solver");
  request_free(req);
}

bool
lrclib_publish(const char *body) {
  Request *req;

  if (lrclib_solve_active())
    return false;

  req = calloc(1, sizeof(*req));
  if (req == NULL)
    return false;

  req->body = malloc(strlen(body) + 1);
  if (req->body == NULL) {
    free(req);
    return false;
  }
  strcpy(req->body, body);

  if (!http_post(URL_CHALLENGE, "", NULL, on_challenge, req)) {
    request_free(req);
    return false;
  }
  return true;
}

#define BODY_MAX 32768

typedef struct {
  char *buf;
  size_t cap;
  size_t len;
  bool ok;
} Buf;

static void
buf_add(Buf *b, const char *s) {
  size_t n = strlen(s);

  if (!b->ok || b->len + n >= b->cap) {
    b->ok = false;
    return;
  }
  memcpy(b->buf + b->len, s, n);
  b->len += n;
  b->buf[b->len] = '\0';
}

/* appends s escaped, without the surrounding quotes */
static void
buf_add_escaped(Buf *b, const char *s) {
  size_t room, n;

  if (!b->ok || b->len >= b->cap) {
    b->ok = false;
    return;
  }
  room = b->cap - b->len;
  n = json_escape(s, b->buf + b->len, room);
  if (n >= room) {
    b->ok = false;
    return;
  }
  b->len += n;
}

/* returns whether the body carries synced lyrics as well as plain ones */
static bool
body_build(Buf *b) {
  size_t i, count = lrclib_line_count();
  bool all_synced = count > 0;
  char num[64];

  buf_add(b, "{\"trackName\":\"");
  buf_add_escaped(b, get_title());
  buf_add(b, "\",\"artistName\":\"");
  buf_add_escaped(b, get_artist());
  buf_add(b, "\",\"albumName\":\"");
  buf_add_escaped(b, get_album());
  snprintf(num, sizeof(num), "\",\"duration\":%u,\"plainLyrics\":\"",
      get_total_time());
  buf_add(b, num);

  for (i = 0; i < count; i++) {
    if (i > 0)
      buf_add(b, "\\n");
    buf_add_escaped(b, lrclib_line_text(i, NULL));
  }

  for (i = 0; i < count; i++) {
    long ms = -1;

    lrclib_line_text(i, &ms);
    if (ms < 0) {
      all_synced = false;
      break;
    }
  }

  buf_add(b, "\",\"syncedLyrics\":\"");
  for (i = 0; all_synced && i < count; i++) {
    long ms = -1;
    const char *text = lrclib_line_text(i, &ms);

    snprintf(num, sizeof(num), "%s[%02ld:%02ld.%02ld] ",
        i > 0 ? "\\n" : "", ms / 60000, ms / 1000 % 60, ms % 1000 / 10);
    buf_add(b, num);
    buf_add_escaped(b, text);
  }
  buf_add(b, "\"}");

  return all_synced;
}

void
lrclib_publish_current(const Arg *arg) {
  static char buf[BODY_MAX];
  Buf b = { buf, sizeof(buf), 0, true };

  (void)arg;

  if (lrclib_solve_active() || pub_state == LRCLIB_PUBLISH_BUSY) {
    publish_status(LRCLIB_PUBLISH_BUSY, "Already publishing");
    return;
  }
  if (!has_song_loaded()) {
    publish_status(LRCLIB_PUBLISH_ERROR, "No track playing");
    return;
  }

  lrclib_trim_trailing();

  if (lrclib_line_count() == 0) {
    publish_status(LRCLIB_PUBLISH_ERROR, "Nothing to publish");
    return;
  }

  pub_synced = body_build(&b);
  if (!b.ok) {
    publish_status(LRCLIB_PUBLISH_ERROR, "Lyrics are too long to publish");
    return;
  }

  if (!lrclib_publish(buf)) {
    publish_status(LRCLIB_PUBLISH_ERROR, "Could not reach lrclib");
    return;
  }
  publish_status(LRCLIB_PUBLISH_BUSY, "Requesting challenge...");
}

static bool
lrc_build(Buf *b) {
  size_t i, count = lrclib_line_count();
  bool all_synced = count > 0;
  char stamp[32];

  for (i = 0; i < count; i++) {
    long ms = -1;

    lrclib_line_text(i, &ms);
    if (ms < 0) {
      all_synced = false;
      break;
    }
  }

  for (i = 0; i < count; i++) {
    long ms = -1;
    const char *text = lrclib_line_text(i, &ms);

    if (all_synced) {
      snprintf(stamp, sizeof(stamp), "[%02ld:%02ld.%02ld]", ms / 60000,
          ms / 1000 % 60, ms % 1000 / 10);
      buf_add(b, stamp);
    }
    buf_add(b, text);
    buf_add(b, "\n");
  }

  return all_synced;
}

void
lrclib_save_lrc(const Arg *arg) {
  static char text[BODY_MAX];
  Buf b = { text, sizeof(text), 0, true };
  char path[1024];
  const char *name;
  char msg[96];
  bool synced;

  (void)arg;

  if (!has_song_loaded()) {
    publish_status(LRCLIB_PUBLISH_ERROR, "No track playing");
    return;
  }

  lrclib_trim_trailing();
  if (lrclib_line_count() == 0) {
    publish_status(LRCLIB_PUBLISH_ERROR, "Nothing to save");
    return;
  }

  synced = lrc_build(&b);
  if (!b.ok) {
    publish_status(LRCLIB_PUBLISH_ERROR, "Lyrics are too long to save");
    return;
  }

  if (!lyrics_save(get_artist(), get_title(), text, path, sizeof(path))) {
    publish_status(LRCLIB_PUBLISH_ERROR, "Could not write the lrc");
    return;
  }

  lyrics_set(get_artist(), get_title(), text);

  name = strrchr(path, '/');
  name = name != NULL ? name + 1 : path;
  /* a long "artist - title.lrc" gets clipped rather than eating the message */
  snprintf(msg, sizeof(msg), "Saved %.60s%s", name,
      synced ? "" : " (plain)");
  publish_status(LRCLIB_PUBLISH_OK, msg);
}
