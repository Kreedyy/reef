# Description

Non-blocking HTTP for other patches, built on libcurl's multi interface.
Because there are no threads, callbacks run inline on the main thread
and may safely touch ncurses / global state.

This is a *library* patch, only other patches need to include this.

# Dependencies

- libcurl

# Authors

- **Creator:** [Kreedy](https://git.gay/kreedy)
  - Github: [Kreedyy](https://github.com/kreedyy) **(either works)**

## Using

The `lrclib` patch is an example/reference implementation.

In your patch's `config.mk` add:

```make
include patches/http/config.mk
```

Then, in your code:

```c
/* patches/foo/foo.c */
#include "http.h"

/* the request outlives the function that starts it, so anything the callback
 * needs goes on the heap and follows as the user pointer */
typedef struct {
  char title[256];
} Request;

/* callback function */
static void on_done(const HttpResponse *r, void *user) {
  Request *req = user; /* the same pointer that was passed in */

  /* ok is false only if the transfer failed (DNS, connection, timeout).
   * a server that answered 404 or 500 is still ok, so check status too. */
  if (r->ok && r->status == 200) {
    /* r->data is NUL-terminated (r->len bytes) but is freed the moment this
     * returns, so copy out whatever you keep. This runs on the main thread,
     * so touching ncurses / calling ui_redraw() here is safe. */
    parse_and_stash(r->data, req->title);
    ui_redraw(REDRAW_PLAYER);
  }

  free(req); /* the callback fired, so we own the free */
}

void fetch(const char *title) {
  Request *req = calloc(1, sizeof *req);

  if (req == NULL)
    return;

  snprintf(req->title, sizeof req->title, "%s", title);

  char *q = http_escape(title); /* escape anything in a query string */
  if (q == NULL) {
    free(req);
    return;
  }

  char url[1024];
  snprintf(url, sizeof url, "https://host/api?title=%s", q);
  http_escape_free(q);

  /* returns immediately, on_done fires later once from reef's event
   * loop. On false it never fires, so the request is still ours to free. */
  if (!http_get(url, NULL, on_done, req))
    free(req);
}
```

The UI never blocks on the network, `fetch()` returns while the transfer is
still in flight. Pass `NULL` as `user` if the callback doesn't need context.

When a server wants an auth token or some other header, pass an array of
`"Key: Value"` strings ending in `NULL`:

```c
const char *headers[] = { "Authorization: Bearer abc123", NULL };
http_get(url, headers, on_done, req);
```

Stuff that belong in the query string instead go through `http_escape()`.

`http_post()` works exactly the same, plus a body:

```c
const char *headers[] = { "Content-Type: application/json", NULL };
http_post(url, "{\"id\":7}", headers, on_done, req);
```

Writes often answer 201 or 204 rather than 200, and 204 has an empty body, so
test the 2xx range instead of `status == 200` and don't require `len > 0`.

## How it works

`reef.c` owns the event loop. When this patch is compiled in, the loop:

- Registers in-flight transfer sockets into its `poll()` set via
`http_fill_pollfds()`, so it wakes the instant the network has data rather
than on a fixed tick.
- Clamps its `poll()` timeout via `http_tune_timeout()` so curl's own timers
still fire when no socket is readable.
- Calls `http_pump()` after each `poll()` to advance transfers and deliver any
that finished.


