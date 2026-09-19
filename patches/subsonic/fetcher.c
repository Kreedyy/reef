#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "http.h"
#include "json.h"
#include "subsonic.h"

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
  Request *req = calloc(1, sizeof(*req));

  char url[1024];
  snprintf(url, sizeof(url),
      "/rest/ping.view?u=%s&t=%s&s=%s&v=%s&c=%s",
      subsonic_api_version, client_name);
}
