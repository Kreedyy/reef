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
static const char *subsonic_format = "json";

/* https://subsonic.org/pages/api.jsp */

typedef struct {

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

  char *pw_ui = ui_cred_get(subsonic_password_cmd);

  if (pw_ui == NULL)
    return;

  size_t pw_len = strlen(pw_ui);

  size_t salt_len = 16;
  uint8_t *salt = hash_salt(salt_len);

  uint8_t combined_raw[64];
  memcpy(combined_raw, pw_ui, pw_len);
  memcpy(combined_raw + pw_len, salt, salt_len);

  uint8_t *digest = hash_md5_hash(combined_raw, pw_len + salt_len);

  char *digest_hex = hash_to_hex(digest, MD5_RAW_LEN);
  char *salt_hex = hash_to_hex(salt, salt_len);

  char url[1024];
  snprintf(url, sizeof(url),
      "%s/rest/ping.view?u=%s&t=%s&s=%s&v=%s&c=%s&f=%s",
      subsonic_url, subsonic_user, digest_hex, salt_hex,
      subsonic_api_version, client_name, subsonic_format);


  //TEST
  FILE *f = fopen("TEST", "w");
  fprintf(f, "digest: %s\nsalt: %s\n", digest_hex, salt_hex);
  
  fprintf(f, "%s", url);
  fclose(f);
}
