#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "keybinds.h"
#include "lrclib.h"

#define SHA256_LEN 32
#define SOLVE_THREADS_MAX 8
#define SOLVE_BATCH 8192 /* nonces hashed between stop flag checks */
#define NONCE_MAX 24     /* room for any unsigned long in decimal */
#define PREFIX_MAX 128

typedef struct Solve Solve;

typedef struct {
  Solve *solve;
  unsigned long start;
} Worker;

struct Solve {
  char prefix[PREFIX_MAX];
  size_t prefix_len;
  unsigned char target[SHA256_LEN];
  uint32_t head; /* first four target bytes, for the fast reject */

  pthread_mutex_t lock;
  bool stop;
  bool won;
  unsigned long nonce;

  int stride;   /* lanes the search was split into, never changes */
  int nthreads; /* lanes that actually started */
  pthread_t worker[SOLVE_THREADS_MAX];
  Worker arg[SOLVE_THREADS_MAX];
};

/* the one search in flight, NULL while idle */
static Solve *solve;
static pthread_t reaper;
static int wake[2] = { -1, -1 };
static LrclibSolveCb solve_cb;
static void *solve_user;

static int
hex_val(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}

static int
hex_decode(const char *hex, unsigned char *out, size_t n) {
  size_t len = strlen(hex), i;

  if (len % 2 != 0 || len / 2 > n)
    return -1;
  for (i = 0; i < len; i += 2) {
    int hi = hex_val(hex[i]), lo = hex_val(hex[i + 1]);

    if (hi < 0 || lo < 0)
      return -1;
    out[i / 2] = (unsigned char)(hi << 4 | lo);
  }
  return (int)(len / 2);
}

/* decimal nonce into out, returns the length written.
 * done by hand because snprintf() is slow at this scale */
static int
utoa(unsigned long v, char *out) {
  char tmp[NONCE_MAX];
  int i = 0, j = 0;

  do {
    tmp[i++] = (char)('0' + v % 10);
    v /= 10;
  } while (v != 0);
  while (i > 0)
    out[j++] = tmp[--i];
  return j;
}

/* lrclib's reference solver compares digest and target as big endian numbers
 * but stops one byte short of the end, so this does too. The first four bytes
 * settle it unless they tie the target exactly, which is rare enough that the
 * rest of the digest is almost never looked at */
static bool
nonce_wins(const unsigned char *digest, const Solve *s) {
  uint32_t head = (uint32_t)digest[0] << 24 | (uint32_t)digest[1] << 16 |
    (uint32_t)digest[2] << 8 | (uint32_t)digest[3];

  if (head != s->head)
    return head < s->head;
  return memcmp(digest + 4, s->target + 4, SHA256_LEN - 5) <= 0;
}

static bool
stop_requested(Solve *s) {
  bool stop;

  pthread_mutex_lock(&s->lock);
  stop = s->stop;
  pthread_mutex_unlock(&s->lock);
  return stop;
}

/* handing EVP_sha256() to EVP_DigestInit_ex() makes openssl 3 look the digest
 * up through its provider on every single call, behind a lock that every
 * thread contends for. Fetching one per worker instead measured much faster
 * meaning the lock is what the search was spending most of it's time on.
 * Do not fold this back into EVP_sha256().
 *
 * libressl has no providers, no lock and no EVP_MD_fetch() to call, so it
 * keeps EVP_sha256() and is just as fast if not faster in most cases */
#if OPENSSL_VERSION_NUMBER >= 0x30000000L && !defined(LIBRESSL_VERSION_NUMBER)
#define HAVE_EVP_MD_FETCH 1
#endif

static void *
worker_run(void *arg) {
  Worker *w = arg;
  Solve *s = w->solve;
  char input[PREFIX_MAX + NONCE_MAX];
  unsigned char digest[EVP_MAX_MD_SIZE];
#ifdef HAVE_EVP_MD_FETCH
  EVP_MD *md = EVP_MD_fetch(NULL, "SHA256", NULL);
#else
  const EVP_MD *md = EVP_sha256();
#endif
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  unsigned long n;
  unsigned int len;
  int batch = 0;

  if (ctx != NULL && md != NULL) {
    memcpy(input, s->prefix, s->prefix_len);

    for (n = w->start; ; n += (unsigned long)s->stride) {
      int k;

      if (++batch >= SOLVE_BATCH) {
        batch = 0;
        if (stop_requested(s))
          break;
      }

      k = utoa(n, input + s->prefix_len);
      EVP_DigestInit_ex(ctx, md, NULL);
      EVP_DigestUpdate(ctx, input, s->prefix_len + (size_t)k);
      EVP_DigestFinal_ex(ctx, digest, &len);

      if (nonce_wins(digest, s)) {
        pthread_mutex_lock(&s->lock);
        if (!s->stop) {
          s->stop = true;
          s->won = true;
          s->nonce = n;
        }
        pthread_mutex_unlock(&s->lock);
        break;
      }
    }
  }

  EVP_MD_CTX_free(ctx);
#ifdef HAVE_EVP_MD_FETCH
  EVP_MD_free(md);
#endif
  return NULL;
}

/* waits for the workers so the main loop never has to, then pokes the pipe.
 * One byte is all lrclib_solve_pump() needs to know it can collect */
static void *
reaper_run(void *arg) {
  Solve *s = arg;
  char b = 1;
  ssize_t rc;
  int i;

  for (i = 0; i < s->nthreads; i++)
    pthread_join(s->worker[i], NULL);

  do {
    rc = write(wake[1], &b, 1);
  } while (rc < 0 && errno == EINTR);

  return NULL;
}

static void
solve_clear(void) {
  if (solve != NULL) {
    pthread_mutex_destroy(&solve->lock);
    free(solve);
    solve = NULL;
  }
  if (wake[0] >= 0) {
    close(wake[0]);
    close(wake[1]);
    wake[0] = wake[1] = -1;
  }
  solve_cb = NULL;
  solve_user = NULL;
}

static bool
pipe_open(void) {
  int i;

  if (pipe(wake) < 0)
    return false;
  for (i = 0; i < 2; i++) {
    int fl = fcntl(wake[i], F_GETFL);

    if (fl < 0 || fcntl(wake[i], F_SETFL, fl | O_NONBLOCK) < 0 ||
      fcntl(wake[i], F_SETFD, FD_CLOEXEC) < 0) {
      close(wake[0]);
      close(wake[1]);
      wake[0] = wake[1] = -1;
      return false;
    }
  }
  return true;
}

static int
cpu_threads(void) {
  long n = sysconf(_SC_NPROCESSORS_ONLN);

  if (n < 1)
    return 1;
  if (n > SOLVE_THREADS_MAX)
    return SOLVE_THREADS_MAX;
  return (int)n;
}

bool
lrclib_solve_start(const char *prefix, const char *target_hex,
                   LrclibSolveCb done, void *user) {
  Solve *s;
  int i;

  if (solve != NULL)
    return false; /* one challenge at a time */

  s = calloc(1, sizeof(*s));
  if (s == NULL)
    return false;

  s->prefix_len = strlen(prefix);
  if (s->prefix_len >= sizeof(s->prefix) ||
    hex_decode(target_hex, s->target, sizeof(s->target)) != SHA256_LEN) {
    free(s);
    return false;
  }
  memcpy(s->prefix, prefix, s->prefix_len);
  s->head = (uint32_t)s->target[0] << 24 | (uint32_t)s->target[1] << 16 |
    (uint32_t)s->target[2] << 8 | (uint32_t)s->target[3];

  if (pthread_mutex_init(&s->lock, NULL) != 0) {
    free(s);
    return false;
  }
  if (!pipe_open()) {
    pthread_mutex_destroy(&s->lock);
    free(s);
    return false;
  }

  /* stride is fixed before anything starts. A lane that fails to spawn just
   * goes unsearched, which costs a little time and nothing else */
  s->stride = cpu_threads();
  for (i = 0; i < s->stride; i++) {
    s->arg[i].solve = s;
    s->arg[i].start = (unsigned long)i;
    if (pthread_create(&s->worker[i], NULL, worker_run, &s->arg[i]) != 0)
      break;
    s->nthreads = i + 1;
  }

  solve = s;
  solve_cb = done;
  solve_user = user;

  if (s->nthreads == 0 ||
    pthread_create(&reaper, NULL, reaper_run, s) != 0) {
    pthread_mutex_lock(&s->lock);
    s->stop = true;
    pthread_mutex_unlock(&s->lock);
    for (i = 0; i < s->nthreads; i++)
      pthread_join(s->worker[i], NULL);
    solve_clear();
    return false;
  }

  return true;
}

bool
lrclib_solve_active(void) {
  return solve != NULL;
}

int
lrclib_solve_fd(void) {
  return wake[0];
}

void
lrclib_solve_pump(void) {
  char nonce[NONCE_MAX];
  LrclibSolveCb cb;
  void *user;
  unsigned long n;
  bool won;
  char b;

  if (solve == NULL || wake[0] < 0)
    return;
  if (read(wake[0], &b, 1) != 1)
    return; /* still searching */

  pthread_join(reaper, NULL);

  won = solve->won;
  n = solve->nonce;
  cb = solve_cb;
  user = solve_user;
  solve_clear();

  if (cb != NULL) {
    snprintf(nonce, sizeof(nonce), "%lu", n);
    cb(won ? nonce : NULL, user);
  }
}

void
lrclib_solve_cancel(void) {
  LrclibSolveCb cb;
  void *user;

  if (solve == NULL)
    return;

  pthread_mutex_lock(&solve->lock);
  solve->stop = true;
  pthread_mutex_unlock(&solve->lock);
  pthread_join(reaper, NULL);

  cb = solve_cb;
  user = solve_user;
  solve_clear();

  if (cb != NULL)
    cb(NULL, user);
}
