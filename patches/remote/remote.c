#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "http.h"
#include "remote.h"

/* Any Forgejo api root works, its compare endpoint
 * answers {"total_commits":N,...}
 */
#define API    "https://git.gay/api/v1"
#define REPO   "Kreedy/reef"

#define TIMEOUT_MS 3000

#define MAX_FDS 8

#define SHORT 7

#define REMOTE_UNKNOWN (-1)

typedef struct {
  bool done;
  int behind;
  char tip[SHORT + 1];
} Result;

static long long
now_ms(void) {
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static const char *
json_value(const char *json, const char *key) {
  size_t kl = strlen(key);
  const char *s;

  for (s = json; *s != '\0'; s++) {
    const char *b = s;

    if (strncmp(s, key, kl) != 0)
      continue;
    while (b > json && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' ||
      b[-1] == '\r'))
      b--;
    if (b > json && b[-1] != '{' && b[-1] != ',')
      continue;
    s += kl;
    return s + strspn(s, " \t:");
  }
  return NULL;
}

static void
short_sha(const char *p, char *out, size_t n) {
  size_t i;

  for (i = 0; i + 1 < n && *p != '\0' &&
    strchr("0123456789abcdef", *p) != NULL; i++)
    out[i] = *p++;
  out[i] = '\0';
}

/* the body is a one element array, [{"url":"...","sha":"...",...}], holding
 * the newest commit on the branch. X-Total-Count carries how many commits the tip reaches, and taking
 * BASECOUNT off that is how far behind this build is.
 * */
static void
on_commits(const HttpResponse *resp, void *user) {
  char total[16];
  Result *res = user;
  const char *p;
  long behind;

  res->done = true;
  if (!resp->ok || resp->status != 200)
    return;

  if (!http_header(resp, "x-total-count", total, sizeof(total)))
    return;
  if (total[0] < '0' || total[0] > '9')
    return;

  behind = strtol(total, NULL, 10) - strtol(REMOTE_BASECOUNT, NULL, 10);
  if (behind < 0)
    return;

  p = json_value(resp->data, "\"sha\"");
  if (p == NULL || *p != '"')
    return;
  short_sha(p + 1, res->tip, sizeof(res->tip));

  res->behind = (int)behind;
}

bool
remote_print_version(void) {
  Result res = { false, REMOTE_UNKNOWN, { '\0' } };
  long long deadline;

  if (REMOTE_BASECOUNT[0] == '\0')
    return false;

  if (!http_get(API "/repos/" REPO "/commits?sha=" REMOTE_BRANCH
                "&limit=1&stat=false&files=false", NULL, on_commits, &res))
    return false;

  deadline = now_ms() + TIMEOUT_MS;
  while (!res.done) {
    struct pollfd fds[MAX_FDS];
    long long left = deadline - now_ms();
    int nfds;

    if (left <= 0)
      break;

    nfds = http_fill_pollfds(fds, MAX_FDS);
    poll(fds, (nfds_t)nfds, http_tune_timeout((int)left));
    http_pump();
  }

  http_cleanup();

  if (res.behind == REMOTE_UNKNOWN || res.tip[0] == '\0')
    return false;

  printf("reef local: %s  origin: %s@%s  +%s/-%d  remote: true\n",
         REMOTE_LOCAL, REMOTE_BRANCH, res.tip, REMOTE_AHEAD, res.behind);
  return true;
}
