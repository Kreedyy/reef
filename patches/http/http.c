#include <curl/curl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "http.h"

typedef struct Request {
  CURL *easy;
  char *data;
  size_t len;
  size_t cap;
  struct curl_slist *headers;
  HttpCallback cb;
  void *user;
  struct Request *next;
} Request;

static CURLM *multi;
static Request *inflight;
static int active;
static CURL *escaper;

static size_t
sink(char *ptr, size_t size, size_t nmemb, void *ud) {
  Request *r = ud;
  size_t add = size * nmemb;
  if (r->len + add + 1 > r->cap) {
    size_t cap = r->cap ? r->cap : 4096;
    char *grown;

    while (cap < r->len + add + 1)
      cap *= 2;
    grown = realloc(r->data, cap);
    if (grown == NULL)
      return 0;
    r->data = grown;
    r->cap = cap;
  }
  memcpy(r->data + r->len, ptr, add);
  r->len += add;
  r->data[r->len] = '\0';
  return add;
}

void
http_init(void) {
  if (multi != NULL)
    return;
  curl_global_init(CURL_GLOBAL_DEFAULT);
  multi = curl_multi_init();
  escaper = curl_easy_init();
}

static Request *
new_request(const char *url, HttpCallback cb, void *user) {
  Request *r;

  if (multi == NULL)
    http_init();
  if (multi == NULL)
    return NULL;

  r = calloc(1, sizeof(*r));
  if (r == NULL)
    return NULL;
  r->cb = cb;
  r->user = user;
  r->easy = curl_easy_init();
  if (r->easy == NULL) {
    free(r);
    return NULL;
  }

  curl_easy_setopt(r->easy, CURLOPT_URL, url);
  curl_easy_setopt(r->easy, CURLOPT_WRITEFUNCTION, sink);
  curl_easy_setopt(r->easy, CURLOPT_WRITEDATA, r);
  curl_easy_setopt(r->easy, CURLOPT_PRIVATE, r);
  curl_easy_setopt(r->easy, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(r->easy, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(r->easy, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(r->easy, CURLOPT_USERAGENT, "reef");
  return r;
}

static void
discard_request(Request *r) {
  curl_easy_cleanup(r->easy);
  curl_slist_free_all(r->headers);
  free(r);
}

static bool
apply_headers(Request *r, const char *const *headers) {
  int i;

  for (i = 0; headers != NULL && headers[i] != NULL; i++) {
    struct curl_slist *grown =
      curl_slist_append(r->headers, headers[i]);
    if (grown == NULL)
      return false;
    r->headers = grown;
  }
  if (r->headers != NULL)
    curl_easy_setopt(r->easy, CURLOPT_HTTPHEADER, r->headers);
  return true;
}

static bool
submit_request(Request *r) {
  if (curl_multi_add_handle(multi, r->easy) != CURLM_OK) {
    discard_request(r);
    return false;
  }
  r->next = inflight;
  inflight = r;
  active++;
  return true;
}

bool
http_get(const char *url, const char *const *headers, HttpCallback cb,
         void *user) {
  Request *r = new_request(url, cb, user);

  if (r == NULL)
    return false;
  if (!apply_headers(r, headers)) {
    discard_request(r);
    return false;
  }
  return submit_request(r);
}

bool
http_post(const char *url, const char *body, const char *const *headers,
          HttpCallback cb, void *user) {
  Request *r = new_request(url, cb, user);

  if (r == NULL)
    return false;
  if (!apply_headers(r, headers)) {
    discard_request(r);
    return false;
  }

  curl_easy_setopt(r->easy, CURLOPT_POST, 1L);
  curl_easy_setopt(r->easy, CURLOPT_POSTFIELDSIZE,
                   (long)(body ? strlen(body) : 0));
  curl_easy_setopt(r->easy, CURLOPT_COPYPOSTFIELDS, body ? body : "");

  return submit_request(r);
}

static void
unlink_request(Request *r) {
  Request **pp = &inflight;
  while (*pp != NULL && *pp != r)
    pp = &(*pp)->next;
  if (*pp != NULL)
    *pp = r->next;
}

static void
finish(Request *r, bool ok) {
  long status = 0;
  curl_easy_getinfo(r->easy, CURLINFO_RESPONSE_CODE, &status);

  active--;

  if (r->cb != NULL) {
    HttpResponse resp;
    resp.ok = ok;
    resp.status = status;
    resp.data = r->data != NULL ? r->data : "";
    resp.len = r->len;
    r->cb(&resp, r->user);
  }

  curl_multi_remove_handle(multi, r->easy);
  curl_easy_cleanup(r->easy);
  curl_slist_free_all(r->headers);
  unlink_request(r);
  free(r->data);
  free(r);
}

void
http_pump(void) {
  CURLMsg *msg;
  int running = 0, in_queue;

  if (multi == NULL || active == 0)
    return;

  curl_multi_perform(multi, &running);

  while ((msg = curl_multi_info_read(multi, &in_queue)) != NULL) {
    char *priv = NULL;

    if (msg->msg != CURLMSG_DONE)
      continue;
    curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &priv);
    if (priv != NULL)
      finish((Request *)priv, msg->data.result == CURLE_OK);
  }
}

bool
http_busy(void) {
  return active > 0;
}

int
http_fill_pollfds(struct pollfd *fds, int max) {
  fd_set rd, wr, ex;
  int fd, maxfd = -1, n = 0;

  if (multi == NULL || active == 0 || max <= 0)
    return 0;

  FD_ZERO(&rd);
  FD_ZERO(&wr);
  FD_ZERO(&ex);
  if (curl_multi_fdset(multi, &rd, &wr, &ex, &maxfd) != CURLM_OK ||
    maxfd < 0)
    return 0;

  for (fd = 0; fd <= maxfd && n < max; fd++) {
    short events = 0;

    if (FD_ISSET(fd, &rd))
      events |= POLLIN;
    if (FD_ISSET(fd, &wr))
      events |= POLLOUT;
    if (FD_ISSET(fd, &ex))
      events |= POLLPRI;
    if (events != 0) {
      fds[n].fd = fd;
      fds[n].events = events;
      fds[n].revents = 0;
      n++;
    }
  }
  return n;
}

int
http_tune_timeout(int timeout_ms) {
  long ct = -1;
  int want;

  if (!http_busy())
    return timeout_ms;

  curl_multi_timeout(multi, &ct);
  want = (ct < 0 || ct > 1000) ? 1000 : (int)ct;

  if (timeout_ms < 0)
    return want;
  return timeout_ms < want ? timeout_ms : want;
}

char *
http_escape(const char *s) {
  if (escaper == NULL || s == NULL)
    return NULL;
  return curl_easy_escape(escaper, s, 0);
}

void
http_escape_free(char *s) {
  if (s != NULL)
    curl_free(s);
}

void
http_cleanup(void) {
  if (multi == NULL)
    return;
  while (inflight != NULL) {
    Request *r = inflight;

    inflight = r->next;
    curl_multi_remove_handle(multi, r->easy);
    curl_easy_cleanup(r->easy);
    curl_slist_free_all(r->headers);
    free(r->data);
    free(r);
  }
  active = false;
  if (escaper != NULL) {
    curl_easy_cleanup(escaper);
    escaper = NULL;
  }
  curl_multi_cleanup(multi);
  multi = NULL;
  curl_global_cleanup();
}
