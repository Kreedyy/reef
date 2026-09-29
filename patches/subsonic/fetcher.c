#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static bool is_connected = false;

static Cred cred;

static void
wipe(void *p, size_t len) {
  volatile unsigned char *v = p;

  while (len-- > 0)
    *v++ = 0;
}

void
subsonic_free_password(void) {
  crypto_free_generic(cred.digest, cred.digest_len);
  crypto_free_generic(cred.salt, cred.salt_len);
  cred.digest = NULL;
  cred.digest_len = 0;
  cred.salt = NULL;
  cred.salt_len = 0;
}

static bool
subsonic_get_password(void) {
  const size_t salt_len = 16;
  char combined[128];
  char *pw_ui, *salt, *digest_hex;
  uint8_t *digest;

  if (cred.digest != NULL && cred.salt != NULL)
    return true;

  if (subsonic_password_cmd == NULL || subsonic_password_cmd[0] == '\0')
    return false;

  if ((pw_ui = ui_cred_get(subsonic_password_cmd)) == NULL)
    return false;

  if ((salt = crypto_random_text(salt_len)) == NULL) {
    cred_free(pw_ui);
    return false;
  }

  snprintf(combined, sizeof(combined), "%s%s", pw_ui, salt);
  cred_free(pw_ui);

  digest = crypto_md5_hash(combined, strlen(combined));
  wipe(combined, sizeof(combined));

  if (digest == NULL) {
    crypto_free_generic(salt, salt_len + 1);
    return false;
  }

  digest_hex = crypto_to_hex(digest, MD5_DIGEST_LEN);
  crypto_md5_free(digest);

  if (digest_hex == NULL) {
    crypto_free_generic(salt, salt_len + 1);
    return false;
  }

  cred.digest = digest_hex;
  cred.digest_len = MD5_DIGEST_LEN * 2 + 1;
  cred.salt = salt;
  cred.salt_len = salt_len + 1;
  return true;
}

static bool
url_builder(char *out, size_t size, char *end_point, char *q) {
  char *params = (q != NULL) ? q : "";

  if (subsonic_url == NULL || subsonic_user == NULL)
    return false;

  if (!subsonic_get_password() || cred.digest == NULL || cred.salt == NULL)
    return false;

  snprintf(out, size,
      "%s/rest/%s?u=%s&t=%s&s=%s&v=%s&c=%s&f=%s&%s",
      subsonic_url, end_point, subsonic_user, cred.digest, cred.salt,
      subsonic_api_version, client_name, subsonic_format, params);

  return true;
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

  if (!url_builder(url, sizeof(url), "ping.view", NULL)) {
    is_connected = false;
    return;
  }

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

  if (!url_builder(url, sizeof(url), "getArtists", NULL))
    return;

  if (!http_get(url, NULL, on_artists, NULL)) {
    return;
  }
  return;
}

void
subsonic_stream(char *id) {
  char url[1024];
  size_t len;

  if (!url_builder(url, sizeof(url), "stream", "format=raw&id="))
    return;

  len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", id);

  queue_add_and_play(url);
}

static void
on_download(const HttpResponse *resp, void *user) {
  char *path;
  char disp[256], new_path[256];
  char *name, *end, *slash;

  path = user;

  if (resp->ok && http_header(resp, "content-disposition", disp, sizeof(disp)) &&
      (name = strstr(disp, "filename=\"")) != NULL &&
      (end = strchr(name += 10, '"')) != NULL && end > name) {
    *end = '\0';
    for (char *c = name; *c; c++)
      if (*c == '/')
        *c = '_';
    

    slash = strrchr(path, '/');
    snprintf(new_path, sizeof(new_path), "%.*s/%s", (int)(slash - path),
        path, name);
    rename(path, new_path);
  }
  free(path);


  /* do something maybe */

}

/* static int
mkdir_p(char *path) {
  for (char *p = path + 1; *p; p++) {
    if (*p != '/') {
      continue;
    }
    *p = '\0';
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
      *p = '/';
      return -1;
    }
    *p = '/';
  }

  return mkdir(path, 0755) != 0 && errno != EEXIST ? -1 : 0;
} */

/* static const char *
download_dir(char *buf, size_t size) {
  const char *dir;
  const char *home;

  const char *FALLBACK_DIR = "music/reef";

  dir = mpd_music_directory();

  if (dir[0] != '\0')
    return dir;

  if ((home = getenv("HOME")) == NULL)
    return NULL;
  
  if ((size_t)snprintf(buf, size, "%s/%s", home, FALLBACK_DIR) >= size)
    return NULL;

  if (mkdir_p(buf) != 0)
    return NULL;

  return buf;

} */

void
subsonic_download(char *id) {
  char url[1024], path[512];
  const char *music_dir;
  char *user_path;
  size_t len;

  if (!url_builder(url, sizeof(url), "download", "id="))
    return;

  len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", id);

  music_dir = mpd_music_directory();

  if (music_dir[0] == '\0')
    return;

  snprintf(path, sizeof(path), "%s/%s", music_dir, id);

  if ((user_path = strdup(path)) == NULL)
    return;

  if (!http_download(url, NULL, path, on_download, user_path))
    free(user_path);
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
  size_t len;

  if (!url_builder(url, sizeof(url), "search3", "query="))
    return;

  len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", q);

  if (!http_get(url, NULL, on_search3, NULL)) {
    return;
  }
}

void
subsonic_get_artist(char *id) {
  char url[1024];
  size_t len;

  if (!url_builder(url, sizeof(url), "getArtist", "id="))
    return;

  len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", id);
}

void
subsonic_get_album(char *id) {
  char url[1024];
  size_t len;

  if (!url_builder(url, sizeof(url), "getAlbum", "id="))
    return;

  len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", id);

}

static void
on_get_song(const HttpResponse *resp, void *user) {
  (void)user; /* get rid of warning */

  if (resp->ok && resp->status == 200 && resp->len > 0) {
    const char *end = resp->data + resp->len;

    const char *subsonic_response = json_value(resp->data, end,
        "subsonic-response");
    const char *subsonic_song = (subsonic_response != NULL) ?
      json_value(subsonic_response, end, "song") : NULL;

    char title[256];
    json_string(subsonic_song, end, "title", title, sizeof(title));

    char suffix[64];
    json_string(subsonic_song, end, "suffix", suffix, sizeof(suffix));


    /* TEST */
    fprintf(stderr, "TITLE: %s\nSUFFIX: %s", title, suffix);

  }

}
void
subsonic_get_song(char *id) {
  char url[1024];
  size_t len;

  if (!url_builder(url, sizeof(url), "getSong", "id="))
    return;

  len = strlen(url);
  snprintf(url + len, sizeof(url) - len, "%s", id);

  if (!http_get(url, NULL, on_get_song, NULL)) {
    return;
  }
}

void
subsonic_get_playlists(void) {
  char url[1024];

  if (!url_builder(url, sizeof(url), "getPlaylists", NULL))
    return;
}
