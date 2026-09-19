#define _GNU_SOURCE 1 /* getrandom() is broken */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "hash.h"

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
  defined(__DragonFly__) || defined(__APPLE__)
#define HAS_ARC4RANDOM 1
#elif defined(__linux__)
#define HAS_GETRANDOM 1
#include <sys/syscall.h>
#include <unistd.h>
#endif

uint8_t *
hash_salt(size_t len) {
  uint8_t *buf = malloc(len);

  if (!buf)
    return NULL;
  
#ifdef HAS_ARC4RANDOM
  arc4random_buf(buf, len);
  return buf;
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

  if (got < len) {
    hash_free_generic(buf, len);
    return NULL;
  }

  return buf;
#endif
}

void
hash_salt_free(uint8_t *salt, size_t len) {
  hash_free_generic(salt, len);
}
