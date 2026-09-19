#include <string.h>
#define _GNU_SOURCE 1 /* getrandom() is broken */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "crypto.h"

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
  defined(__DragonFly__) || defined(__APPLE__)
#define HAS_ARC4RANDOM 1
#elif defined(__linux__)
#define HAS_GETRANDOM 1
#include <sys/syscall.h>
#include <unistd.h>
#endif

const char CHAR_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"\
                           "abcdefghijklmnopqrstuvwxyz0123456789";
#define CHAR_TABLE_LEN (sizeof(CHAR_TABLE) - 1)

bool
crypto_random_bytes(uint8_t *buf, size_t len) {
#ifdef HAS_ARC4RANDOM
  arc4random_buf(buf, len);
  return true;
#else
  size_t got = 0;
#ifdef HAS_GETRANDOM
  while (got < len) {
    ssize_t n = syscall(SYS_getrandom, buf + got, len - got, 0); /* getrandom() is broken */
    if (n < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    got += (size_t)n;
  }
#endif
  if (got < len) {
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
      got += fread(buf + got, 1, len - got, f);
      fclose(f);
    }
  }

  return got == len;
#endif
}

char *
crypto_random_text(size_t len) {
  char *out = malloc(len + 1);

  if (!out)
    return NULL;

  const unsigned limit = (256 / CHAR_TABLE_LEN) * CHAR_TABLE_LEN;

  size_t have = 0;

  size_t i;

  while (have < len) {
    uint8_t chunk[64];
    size_t want = len - have < sizeof(chunk) ? len - have : sizeof(chunk);

    if (!crypto_random_bytes(chunk, want)) {
      crypto_free_generic(out, len + 1);
      return NULL;
    }
    
    for (i = 0; i < want && have < len; i++) {
      if (chunk[i] < limit) {
        out[have++] = CHAR_TABLE[chunk[i] % CHAR_TABLE_LEN];
      }
    }
  }

  out[len] = '\0';

  return out;
  
}

void
crypto_random_text_free(char *text) {
  if (!text)
    return;

  crypto_free_generic(text, strlen(text) + 1);
}
