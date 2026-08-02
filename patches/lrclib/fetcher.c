#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "lyrics.h"

/* carried through the async request so the callback knows which track it is */
typedef struct {
  char artist[256];
  char title[256];
} Request;

static bool
json_string(const char *json, const char *end, const char *key, char *out,
            size_t n) {
  char pat[64];
  const char *p = NULL, *s;
  size_t o = 0;
  int pl;

  pl = snprintf(pat, sizeof(pat), "\"%s\"", key);
  if (pl <= 0)
    return false;

  for (s = json; s + pl <= end; s++) {
    const char *b = s;

    if (memcmp(s, pat, (size_t)pl) != 0)
      continue;
    while (b > json && (b[-1] == ' ' || b[-1] == '\t' ||
      b[-1] == '\n' || b[-1] == '\r'))
      b--;
    if (b > json && b[-1] != '{' && b[-1] != ',')
      continue;
    p = s + pl;
    break;
  }
  if (p == NULL)
    return false;

  while (p < end && (*p == ' ' || *p == '\t' || *p == ':'))
    p++;
  if (p >= end || *p != '"')
    return false;
  p++;

  while (p < end && *p != '"' && o + 1 < n) {
    char c = *p++;

    if (c == '\\' && p < end) {
      char e = *p++;

      switch (e) {
        case 'n':
          c = '\n';
          break;
        case 't':
          c = '\t';
          break;
        case 'r':
          continue;
        case '"':
          c = '"';
          break;
        case '\\':
          c = '\\';
          break;
        case '/':
          c = '/';
          break;
        case 'u':
          if (p + 4 <= end) {
            char hex[5] = { p[0], p[1], p[2], p[3],
              '\0' };
            long cp = strtol(hex, NULL, 16);

            p += 4;
            c = cp > 0 && cp < 128 ? (char)cp
              : '?';
          } else {
            c = '?';
          }
          break;
        default:
          c = e;
          break;
      }
    }
    out[o++] = c;
  }
  out[o] = '\0';
  return true;
}

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
field_matches(const char *field, const char *query) {
  if (query[0] == '\0')
    return true;
  if (field[0] == '\0')
    return false;
  return ci_contains(field, query) || ci_contains(query, field);
}

static const char *
next_object(const char *p, const char **afterp) {
  const char *start;
  int depth = 0, in_str = false;

  while (*p != '\0' && *p != '{')
    p++;
  if (*p == '\0')
    return NULL;
  start = p;
  for (; *p != '\0'; p++) {
    if (in_str) {
      if (*p == '\\' && p[1] != '\0')
        p++;
      else if (*p == '"')
        in_str = false;
    } else if (*p == '"') {
      in_str = true;
    } else if (*p == '{') {
      depth++;
    } else if (*p == '}' && --depth == 0) {
      *afterp = p + 1;
      return start;
    }
  }
  return NULL;
}

static bool
find_lyrics(const char *json, const Request *req, const char *key, char *out,
            size_t cap) {
  const char *p = json, *after;
  const char *obj;
  while ((obj = next_object(p, &after)) != NULL) {
    char artist[256] = "", title[256] = "";

    json_string(obj, after, "artistName", artist, sizeof(artist));
    json_string(obj, after, "trackName", title, sizeof(title));
    if (field_matches(title, req->title) &&
      field_matches(artist, req->artist) &&
      json_string(obj, after, key, out, cap) && out[0] != '\0')
      return true;
    p = after;
  }
  return false;
}

static void
on_result(const HttpResponse *resp, void *user) {
  Request *req = user;

  const char *text = NULL;
  char *result = NULL;
  if (resp->ok && resp->status == 200 && resp->len > 0) {
    result = malloc(resp->len + 1);
    if (result != NULL &&
      (find_lyrics(resp->data, req, "syncedLyrics", result, resp->len + 1) ||
      find_lyrics(resp->data, req, "plainLyrics", result, resp->len + 1)))
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

  a = http_escape(artist);
  t = http_escape(title);
  if (a == NULL || t == NULL) {
    http_escape_free(a);
    http_escape_free(t);
    free(req);
    lyrics_set(artist, title, NULL);
    return;
  }

  snprintf(url, sizeof(url), "https://lrclib.net/api/search?q=%s+%s", a, t);
  http_escape_free(a);
  http_escape_free(t);

  if (!http_get(url, NULL, on_result, req)) {
    free(req);
    lyrics_set(artist, title, NULL);
  }
}


