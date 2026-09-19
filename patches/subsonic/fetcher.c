#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "cred.h"
#include "hash.h"
#include "http.h"
#include "json.h"
#include "subsonic.h"
#include "ui.h"

static const char *client_name = "reef";

/* https://subsonic.org/pages/api.jsp */

typedef struct {
  char artists[256];
} Request;

static void
subsonic_ping_server_on_result(const HttpResponse *resp, void *user) {
  Request *req = user;

  const char *text = NULL;
  char *result = NULL;

  if (resp->ok && resp->status == 200 && resp->len > 0) {
    const char *end = resp->data + resp->len;

    result = malloc(resp->len + 1);
    if (result != NULL) {
    
    }
  }
}

void
subsonic_ping_server(void) {
  if (subsonic_password_cmd == NULL || subsonic_password_cmd[0] == '\0')
    return;

  Request *req = calloc(1, sizeof(*req));

  char *pw = ui_cred_get(subsonic_password_cmd);
  uint salt_len = 10;

  uint8_t *salt_raw = hash_salt(salt_len);
  char *salt = hash_to_hex(salt_raw, salt_len);
  hash_salt_free(salt_raw, salt_len);

  char pw_hash_hex[strlen(pw) + strlen(salt)];

  snprintf(pw_hash_hex, sizeof(pw_hash_hex), pw, salt);
  cred_free(pw);

  if (pw == NULL) {
    return;
  }

  char url[1024];
  snprintf(url, sizeof(url),
      "%s/rest/ping.view?u=%s&t=%s&s=%s&v=%s&c=%s",
      subsonic_url, subsonic_user, pw_hash_hex, salt,
      subsonic_api_version, client_name);

  hash_to_hex_free(salt);
}
