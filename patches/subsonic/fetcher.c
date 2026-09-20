#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "config.h"
#include "cred.h"
#include "crypto.h"
#include "http.h"
#include "json.h"
#include "mpd.h"
#include "subsonic.h"
#include "ui.h"

static const char *client_name = "reef";
static const char *subsonic_format = "json";

/* https://subsonic.org/pages/api.jsp */
/* https://subsonic.org/pages/api.jsp#stream use format=raw */

typedef struct {
  char *digest;
  size_t digest_len;
  char *salt;
  size_t salt_len;
} Cred;

typedef struct {
  char *path;
  char *title;
} Download;

static bool is_connected = false;

static void
subsonic_get_password(Cred *c) {
  c->digest = NULL;
  c->digest_len = 0;
  c->salt = NULL;
  c->salt_len = 0;

  if (subsonic_password_cmd == NULL || subsonic_password_cmd[0] == '\0')
    return;


  char *pw_ui = ui_cred_get(subsonic_password_cmd);

  if (pw_ui == NULL)
    return;

  size_t salt_len = 16;
  char *salt = crypto_random_text(salt_len);

  char combined[128];
  snprintf(combined, sizeof(combined), "%s%s", pw_ui, salt);

  uint8_t *digest = crypto_md5_hash(combined, strlen(combined));

  char *digest_hex = crypto_to_hex(digest, MD5_DIGEST_LEN);

  c->digest = digest_hex;
  c->digest_len = MD5_DIGEST_LEN * 2;
  c->salt = salt;
  c->salt_len = salt_len;

  cred_free(pw_ui);
  crypto_md5_free(digest);
}

void
subsonic_free_password(Cred *c) {
  if (!c || c == NULL)
    return;
  crypto_free_generic(c->digest, c->digest_len);
  crypto_free_generic(c->salt, c->salt_len);
}

static void
url_builder(char *out, size_t size, char *end_point, char *q) {
  Cred c = {0};
  subsonic_get_password(&c);

  char *params = (q != NULL) ? q : "";

  snprintf(out, size,
      "%s/rest/%s?u=%s&t=%s&s=%s&v=%s&c=%s&f=%s&%s",
      subsonic_url, end_point, subsonic_user, c.digest, c.salt,
      subsonic_api_version, client_name, subsonic_format, params);

  subsonic_free_password(&c);
}

static void
on_ping_server(const HttpResponse *resp, void *user) {
  (void)user; /* get rid of warning */

  /* TEST */
  fprintf(stderr, "Full: %s\n", resp->data);

  is_connected = false;

  char status[16];
  if (resp->ok && resp->status == 200 && resp->len > 0) {
    const char *end = resp->data + resp->len;

    const char *subsonic_response = json_value(resp->data, end, "subsonic-response");

    if (subsonic_response != NULL) {
      json_string(subsonic_response, end, "status", status, sizeof(status));
      is_connected = true;
    }
  }

  /* TEST */
  fprintf(stderr, "Status: %s", status);
}

void
subsonic_ping_server(void) {

  /* NOTE: for future; if ping fails avoid trying to fetch anything else.
   * retry ping after 5s then keep mult 2? or keybind? */

  char url[1024];
  url_builder(url, sizeof(url), "ping.view", NULL);

  // TEST
  fprintf(stderr, "%s\n", url);

  if (!http_get(url, NULL, on_ping_server, NULL)) {
    is_connected = false;
    return;
  }
  is_connected = true;
  return;
}

bool
subsonic_is_connected(void) {
  subsonic_ping_server();
  return is_connected;
}

static void
on_artists(const HttpResponse *resp, void *user) {
  (void)user; /* get rid of warning */

  if (resp->ok && resp->status == 200 && resp->len > 0) {
    const char *end = resp->data + resp->len;

    const char *subsonic_response = json_value(resp->data, end,
        "subsonic-response");
    const char *subsonic_artists = (subsonic_response != NULL) ?
      json_value(subsonic_response, end, "artists") : NULL;
    const char *subsonic_index = (subsonic_response != NULL) ?
      json_value(subsonic_artists, end, "index") : NULL;

    /* TEST */
    fprintf(stderr, "Index: %s", subsonic_index);

  }

}

void
subsonic_get_artists(void) {

  char url[1024];
  url_builder(url, sizeof(url), "getArtists", NULL);

  if (!http_get(url, NULL, on_artists, NULL)) {
    return;
  }
  return;
}

void
subsonic_stream(char *id) {
  char url[1024];
  url_builder(url, sizeof(url), "stream", "format=raw&id=");
  size_t len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", id);

  queue_add_and_play(url);
}

static void
on_download(const HttpResponse *resp, void *user) {
  (void)user; /* get rid of warning */
  (void)resp;

  // Download *d;
  //
  // d = user;

  /* do something maybe */

  // free(d->path);
  // free(d->title);
  // free(d);
}

void
subsonic_download(char *id) {
  char url[1024];
  url_builder(url, sizeof(url), "download", "id=");
  size_t len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", id);

  const char *music_dir = mpd_music_directory();

  // const char *path = strlen(music_dir) > 0 ? music_dir : getenv("HOME");
  const char *path = "/home/kreedy/repos/reef/FILE.flac";

  if (!http_download(url, NULL, path, on_download, NULL)) {
    return;
  }
  return;
}

static void
on_search3(const HttpResponse *resp, void *user) {
  (void)user; /* get rid of warning */

  if (resp->ok && resp->status == 200 && resp->len > 0) {
    const char *end = resp->data + resp->len;

    const char *subsonic_response = json_value(resp->data, end,
        "subsonic-response");
    const char *subsonic_search = (subsonic_response != NULL) ?
      json_value(subsonic_response, end, "searchResult3") : NULL;

    /* TEST */
    fprintf(stderr, "SearchResult3: %s", subsonic_search);

  }

}

void
subsonic_search3(char *q) {
  char url[1024];
  url_builder(url, sizeof(url), "search3", "query=");
  size_t len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", q);

  if (!http_get(url, NULL, on_search3, NULL)) {
    return;
  }
}

void
subsonic_get_artist(char *id) {
  char url[1024];
  url_builder(url, sizeof(url), "getArtist", "id=");
  size_t len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", id);
}

void
subsonic_get_album(char *id) {
  char url[1024];
  url_builder(url, sizeof(url), "getAlbum", "id=");
  size_t len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", id);

}

void
subsonic_get_song(char *id) {
  char url[1024];
  url_builder(url, sizeof(url), "getSong", "id=");
  size_t len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", id);
}

void
subsonic_get_playlists(void) {
  char url[1024];
  url_builder(url, sizeof(url), "getPlaylists", NULL);
}
