#pragma once

#include <stdbool.h>
#include <stddef.h>

/* a finished request. Valid only for the duration of the callback so copy
 * anything you need to keep. ok is false only when the transfer itself failed.
 * A server that answered 404 or 500 is still ok, so check status for http
 * error codes */
typedef struct {
  bool ok;
  long status;      /* http status code */
  const char *data; /* body, NUL terminated */
  size_t len;       /* body length, not counting the terminator */
} HttpResponse;

/* fires once from the event loop when the request finishes.
 * user is handed back */
typedef void (*HttpCallback)(const HttpResponse *resp, void *user);

/* queue an asynchronous GET. Returns false if the request could not be
 * started, otherwise cb fires once. headers works as it does for http_post().
 * Pass NULL when there are none */
bool http_get(const char *url, const char *const *headers, HttpCallback cb,
    void *user);

/* asynchronous POST. body is copied. headers is a NULL terminated array of
 * "Key: Value" strings, or NULL
 * Example:
 * const char *headers[] = { "Content-Type: application/json",
 *                           "Accept: application/json",
 *                           NULL };
 * the closing NULL is what ends the array, leaving it out runs off the end */
bool http_post(const char *url, const char *body, const char *const *headers,
    HttpCallback cb, void *user);

/* URL encode s for use in a query string. Free the result with
 * http_escape_free(). Returns NULL on failure */
char *http_escape(const char *s);
void http_escape_free(char *s);

/* true while any transfer is in flight */
bool http_busy(void);

/* patches do not need the functions below, this is all handled by the main
 * program */

struct pollfd;

void http_init(void);
void http_cleanup(void);

int http_fill_pollfds(struct pollfd *fds, int max);

int http_tune_timeout(int timeout_ms);

void http_pump(void);
