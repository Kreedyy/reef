#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "cred.h"
#include "crypto.h"
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

  size_t salt_len = 16;
  char *salt = crypto_random_text(salt_len);

  char combined[128];
  snprintf(combined, sizeof(combined), "%s%s", pw_ui, salt);

  uint8_t *digest = crypto_md5_hash(combined, strlen(combined));

  char *digest_hex = crypto_to_hex(digest, MD5_DIGEST_LEN);

  char url[1024];
  snprintf(url, sizeof(url),
      "%s/rest/ping.view?u=%s&t=%s&s=%s&v=%s&c=%s&f=%s",
      subsonic_url, subsonic_user, digest_hex, salt,
      subsonic_api_version, client_name, subsonic_format);

  // TEST
  FILE *f = fopen("TEST", "w");
  fprintf(f, "%s", url);
  fclose(f);
}
