#ifndef PATCH_crypto_H
#define PATCH_crypto_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define MD5_DIGEST_LEN 16

char *crypto_to_hex(const uint8_t *data, size_t len);
void crypto_to_hex_free(char *hex);

uint8_t *crypto_md5_hash(const void *data, size_t len);
void crypto_md5_free(uint8_t *digest);

/* salt is a random set of bytes, of length len */
uint8_t *crypto_salt(size_t len);
void crypto_salt_free(uint8_t *salt, size_t len);

bool crypto_random_bytes(uint8_t *buf, size_t len);
char *crypto_random_text(size_t len);

/* generic function to zero a pointer of length len */
static inline void crypto_free_generic(void *p, size_t len) {
  if (!p)
    return;

  size_t i;
  volatile unsigned char *v = p;

  for (i = 0; i < len; i++)
    v[i] = 0;
  free(p);
}

#endif /* PATCH_crypto_H */
