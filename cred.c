#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cred.h"

#define CRED_MAX 4096

/* -O2 deletes a plain memset() over a buffer that is never read again.
 * using a volatile pointer forces an indirect call it cannot drop */
static void *(*volatile cred_memset)(void *, int, size_t) = memset;

static void
cred_wipe(void *buf, size_t len) {
  if (buf != NULL && len > 0)
    cred_memset(buf, 0, len);
}

void
cred_free(char *secret) {
  if (secret == NULL)
    return;
  cred_wipe(secret, strlen(secret));
  free(secret);
}

static pid_t
cred_spawn(const char *cmd, int *fd, bool tty) {
  int pipefd[2];
  pid_t pid;

  if (pipe(pipefd) == -1)
    return -1;

  pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return -1;
  }

  if (pid == 0) {
    int devnull;

    close(pipefd[0]);
    if (dup2(pipefd[1], STDOUT_FILENO) == -1)
      _exit(127);
    if (pipefd[1] != STDOUT_FILENO)
      close(pipefd[1]);

    if (!tty) {
      /* the agent is a separate process so the /dev/null below never
       * reaches it. Without this it draws pinentry over the ui */
      unsetenv("GPG_TTY");

      devnull = open("/dev/null", O_RDWR);

      if (devnull != -1) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull != STDIN_FILENO && devnull != STDERR_FILENO)
          close(devnull);
      }
    }

    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
    _exit(127);
  }

  close(pipefd[1]);
  *fd = pipefd[0];
  return pid;
}

/* reads until the child closes its stdout,
 * keeping the first cap bytes in buf */
static bool
cred_read(int fd, char *buf, size_t cap, size_t *len) {
  char sink[512];
  ssize_t n;

  *len = 0;
  for (;;) {
    if (*len < cap)
      n = read(fd, buf + *len, cap - *len);
    else
      n = read(fd, sink, sizeof(sink));

    if (n == -1) {
      if (errno == EINTR)
        continue;
      cred_wipe(sink, sizeof(sink));
      return false;
    }
    if (n == 0) {
      cred_wipe(sink, sizeof(sink));
      return true;
    }
    if (*len < cap)
      *len += (size_t)n;
  }
}

/* length of the first line, everything after it is metadata
 * as far as we are concerned */
static size_t
cred_first_line(const char *buf, size_t len) {
  size_t n = 0;

  while (n < len && buf[n] != '\n')
    n++;
  if (n > 0 && buf[n - 1] == '\r')
    n--;
  return n;
}

static char *
cred_run(const char *cmd, bool tty, bool *retryable) {
  char buf[CRED_MAX];
  char *secret;
  size_t len, line;
  int fd = -1, status = 0;
  pid_t pid, waited;
  bool read_ok;

  if (retryable != NULL)
    *retryable = false;

  if (cmd == NULL || cmd[0] == '\0')
    return NULL;

  pid = cred_spawn(cmd, &fd, tty);
  if (pid == -1)
    return NULL;

  read_ok = cred_read(fd, buf, sizeof(buf), &len);
  close(fd);

  do {
    waited = waitpid(pid, &status, 0);
  } while (waited == -1 && errno == EINTR);

  if (!read_ok || waited == -1 || !WIFEXITED(status) ||
    WEXITSTATUS(status) != 0) {
    if (retryable != NULL && read_ok && waited != -1 && WIFEXITED(status) &&
      WEXITSTATUS(status) != 127)
      *retryable = true;
    cred_wipe(buf, sizeof(buf));
    return NULL;
  }

  line = cred_first_line(buf, len);
  if (line == 0) {
    cred_wipe(buf, sizeof(buf));
    return NULL;
  }

  secret = malloc(line + 1);
  if (secret != NULL) {
    memcpy(secret, buf, line);
    secret[line] = '\0';
  }
  cred_wipe(buf, sizeof(buf));
  return secret;
}

char *
cred_get(const char *cmd) {
  return cred_run(cmd, true, NULL);
}

char *
cred_get_notty(const char *cmd, bool *needs_tty) {
  return cred_run(cmd, false, needs_tty);
}
