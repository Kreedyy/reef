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

typedef struct {
  char *digest;
  size_t digest_len;
  char *salt;
  size_t salt_len;
} Cred;

void
subsonic_get_credentials(Cred *c) {
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
subsonic_free_credentials(Cred *c) {
  if (!c || c == NULL)
    return;
  crypto_free_generic(c->digest, c->digest_len);
  crypto_free_generic(c->salt, c->salt_len);
}

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
  
  Cred c;
  subsonic_get_credentials(&c);

  char url[1024];
  snprintf(url, sizeof(url),
      "%s/rest/ping.view?u=%s&t=%s&s=%s&v=%s&c=%s&f=%s",
      subsonic_url, subsonic_user, c.digest, c.salt,
      subsonic_api_version, client_name, subsonic_format);

  subsonic_free_credentials(&c);

  // TEST
  FILE *f = fopen("TEST", "w");
  fprintf(f, "%s", url);
  fclose(f);
}


