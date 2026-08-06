#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "json.h"
#include "lyrics.h"
#include "mpd.h"

/* carried through the async request so the callback knows which track it is */
typedef struct {
  char artist[256];
  char title[256];
  char album[256];
} Request;

static bool
ci_contains(const char *hay, const char *needle) {
  size_t hn, nn, i;

  if (needle[0] == '\0')
    return true;
  hn = strlen(hay);
  nn = strlen(needle);
  if (nn > hn)
    return false;
  for (i = 0; i + nn <= hn; i++) {
    size_t j = 0;

    while (j < nn && tolower((unsigned char)hay[i + j]) ==
      tolower((unsigned char)needle[j]))
      j++;
    if (j == nn)
      return true;
  }
  return false;
}

static bool
ci_equal(const char *a, const char *b) {
  size_t i;

  for (i = 0; a[i] != '\0' && b[i] != '\0'; i++)
    if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
      return false;
  return a[i] == b[i];
}

static bool
field_matches(const char *field, const char *query) {
  if (query[0] == '\0')
    return true;
  if (field[0] == '\0')
    return false;
  return ci_contains(field, query) || ci_contains(query, field);
}

/* how close field is to query: 3 the same, 2 one inside the other,
 * 1 empty, 0 different */
static int
field_score(const char *field, const char *query) {
  if (field[0] == '\0' || query[0] == '\0')
    return field[0] == query[0] ? 3 : 1;
  if (ci_equal(field, query))
    return 3;
  return ci_contains(field, query) || ci_contains(query, field) ? 2 : 0;
}

static int
match_score(const Request *req, const char *artist, const char *title,
            const char *album) {
  return field_score(title, req->title) * 16 +
    field_score(artist, req->artist) * 4 + field_score(album, req->album);
}

static bool
has_text(const char *obj, const char *after, const char *key) {
  const char *v = json_value(obj, after, key);

  return v != NULL && v[0] == '"' && v[1] != '"';
}

static bool
find_lyrics(const char *json, const char *end, const Request *req,
            const char *key, char *out, size_t cap) {
  const char *p = json, *after;
  const char *obj;
  const char *best = NULL, *best_after = NULL;
  int best_score = 0;

  while ((obj = json_next_object(p, end, &after)) != NULL) {
    char artist[256], title[256], album[256];
    int score;

    json_string(obj, after, "artistName", artist, sizeof(artist));
    json_string(obj, after, "trackName", title, sizeof(title));
    json_string(obj, after, "albumName", album, sizeof(album));
    p = after;

    if (!field_matches(title, req->title) ||
      !field_matches(artist, req->artist) || !has_text(obj, after, key))
      continue;

    score = match_score(req, artist, title, album);
    if (best == NULL || score > best_score) {
      best = obj;
      best_after = after;
      best_score = score;
    }
  }

  return best != NULL && json_string(best, best_after, key, out, cap) &&
    out[0] != '\0';
}

static void
on_result(const HttpResponse *resp, void *user) {
  Request *req = user;

  const char *text = NULL;
  char *result = NULL;
  if (resp->ok && resp->status == 200 && resp->len > 0) {
    const char *end = resp->data + resp->len;

    result = malloc(resp->len + 1);
    if (result != NULL &&
      (find_lyrics(resp->data, end, req, "syncedLyrics", result,
        resp->len + 1) ||
      find_lyrics(resp->data, end, req, "plainLyrics", result,
        resp->len + 1)))
      text = result;
  }

  lyrics_set(req->artist, req->title, text);

  free(result);
  free(req);
}

void
lrclib_provider(const char *artist, const char *title) {
  Request *req = calloc(1, sizeof(*req));
  char url[1024];
  char *a, *t;

  if (req == NULL) {
    lyrics_set(artist, title, NULL);
    return;
  }
  snprintf(req->artist, sizeof(req->artist), "%s", artist);
  snprintf(req->title, sizeof(req->title), "%s", title);
  snprintf(req->album, sizeof(req->album), "%s", get_album());

  a = http_escape(artist);
  t = http_escape(title);
  if (a == NULL || t == NULL) {
    http_escape_free(a);
    http_escape_free(t);
    free(req);
    lyrics_set(artist, title, NULL);
    return;
  }

  snprintf(url, sizeof(url), "https://lrclib.net/api/search?q=%s+%s", t, a);
  http_escape_free(a);
  http_escape_free(t);

  if (!http_get(url, NULL, on_result, req)) {
    free(req);
    lyrics_set(artist, title, NULL);
  }
}


