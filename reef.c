#include <errno.h>
#include <ncurses.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <time.h>
#include <unistd.h>

#include "keybinds.h"
#include "lyrics.h"
#include "mpd.h"
#include "types.h"
#include "ui.h"

#ifdef PATCH_http
#include "http.h"
#endif

#ifdef PATCH_lrclib
#include "lrclib.h"
#endif

#ifdef PATCH_remote
#include "remote.h"
#endif

#define TICK_MS 50

#define RECONNECT_MS 200

#ifdef REEF_DEBUG
/* point stderr at debug.log before curses starts */
static void
debug_log_init(void) {
  if (!freopen("debug.log", "w", stderr))
    return;
  setvbuf(stderr, NULL, _IONBF, 0);
}

const char *__ubsan_default_options(void);
const char *__asan_default_options(void);

const char *
__ubsan_default_options(void) {
  return "print_stacktrace=1";
}

const char *
__asan_default_options(void) {
  return "fast_unwind_on_malloc=0"
    ":malloc_context_size=50"
    ":detect_stack_use_after_return=1";
}
#endif

enum {
  POLL_STDIN,
  POLL_MPD,
#ifdef PATCH_lrclib
  POLL_LRCLIB, /* the publish challenge solver, idle unless it is searching */
#endif
  POLL_COUNT
};

/* extra poll() slots the http patch fills, so the loop wakes on network
 * readiness instead of a fixed tick */
#ifdef PATCH_http
#define HTTP_MAX_FDS 16
#else
#define HTTP_MAX_FDS 0
#endif

static void
upgrade_truecolor(void) {
  const char *colorterm = getenv("COLORTERM");
  const char *term = getenv("TERM");
  char direct[64];
  int err;

  if (!colorterm || !term)
    return;
  if (!strstr(colorterm, "truecolor") && !strstr(colorterm, "24bit"))
    return;
  if (strstr(term, "-direct"))
    return;

  if (snprintf(direct, sizeof(direct), "%s-direct", term) >=
    (int)sizeof(direct))
    return;

  if (setupterm(direct, STDOUT_FILENO, &err) != OK)
    return;

  del_curterm(cur_term);
  setenv("TERM", direct, 1);
}

static unsigned long
mono_ms(void) {
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)ts.tv_sec * 1000 +
  (unsigned long)ts.tv_nsec / 1000000;
}

static int
due_in(unsigned long from, unsigned long period) {
  unsigned long waited = mono_ms() - from;

  return waited >= period ? 0 : (int)(period - waited);
}

static void
print_patches(void) {
  printf("patches: %s\n", REEF_PATCHES[0] ? REEF_PATCHES : "");
}

int
main(int argc, char *argv[]) {
  struct pollfd fds[POLL_COUNT + HTTP_MAX_FDS];
  int timeout, nfds, ready, ch;
  unsigned long last_tick, last_attempt, redraw_at;

#ifdef REEF_DEBUG
  debug_log_init();
#endif

  if (argc > 1 && (strcmp(argv[1], "-v") == 0 ||
    strcmp(argv[1], "--version") == 0)) {
#ifdef PATCH_remote
    remote_print_version();
#else
    printf("reef %s  remote: false\n", VERSION);
#endif
    print_patches();
    return 0;
  }

  init_mpd();
  upgrade_truecolor();
  init_ncurses();
  init_keybinds();
  init_lyrics();
#ifdef PATCH_http
  http_init();
#endif

  fds[POLL_STDIN].fd = STDIN_FILENO;
  fds[POLL_STDIN].events = POLLIN;
  fds[POLL_MPD].fd = mpd_idle_fd();
  fds[POLL_MPD].events = POLLIN;

  last_tick = last_attempt = redraw_at = mono_ms();

  while (running) {
    fds[POLL_MPD].fd = mpd_idle_fd();
#ifdef PATCH_lrclib
    fds[POLL_LRCLIB].fd = lrclib_solve_fd();
    fds[POLL_LRCLIB].events = POLLIN;
#endif

    if (!mpd_connected()) {
      timeout = due_in(last_attempt, RECONNECT_MS);
    } else if (is_playing()) {
      int line = lyrics_next_line_in();

      timeout = due_in(last_tick, TICK_MS);
      if (line >= 0 && line < timeout)
        timeout = line;
      redraw_at = mono_ms() + (unsigned long)timeout;
    } else {
      timeout = -1;
    }

    nfds = POLL_COUNT;
#ifdef PATCH_http
    nfds += http_fill_pollfds(fds + POLL_COUNT, HTTP_MAX_FDS);
    timeout = http_tune_timeout(timeout);
#endif

    ready = poll(fds, nfds, timeout);
    if (ready < 0 && errno != EINTR)
      mpd_drop_connection();

    if (fds[POLL_MPD].revents & (POLLHUP | POLLERR | POLLNVAL)) {
      mpd_drop_connection();
    } else if (fds[POLL_MPD].revents & POLLIN) {
      enum mpd_idle events = mpd_drain_events();

      if (events && mpd_connected()) {
        mpd_refresh_status();
        ui_on_mpd_events(events);
      }
    } else if (!mpd_connected() &&
      mono_ms() - last_attempt >= RECONNECT_MS) {
      last_attempt = mono_ms();
      if (init_mpd()) {
        mpd_refresh_status();
        ui_redraw(REDRAW_ALL);
      } else if (mpd_error_active()) {
        ui_redraw(REDRAW_PLAYER);
      }
    } else if (is_playing() && mono_ms() >= redraw_at) {
      last_tick = mono_ms();
      mpd_resync_elapsed();
      ui_redraw(REDRAW_TICK);
    }

#ifdef PATCH_http
    http_pump();
#endif

#ifdef PATCH_lrclib
    lrclib_solve_pump();
#endif

    while (running && (ch = getch()) != ERR) {
      switch (ch) {
        case KEY_RESIZE:
          resize();
          break;
        default:
          handle_key(ch);
          break;
      }
    }
  }

#ifdef PATCH_lrclib
  lrclib_solve_cancel();
#endif

#ifdef PATCH_http
  http_cleanup();
#endif
  destroy_ncurses();
  destroy_mpd();
  return 0;
}
