#ifndef PATCH_HASH_H
#define PATCH_HASH_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define MD5_RAW_LEN 16

char *hash_to_hex(const uint8_t *data, size_t len);
void hash_to_hex_free(char *hex);

uint8_t *hash_md5_hash(const void *data, size_t len);
void hash_md5_free(uint8_t *digest);

/* salt is a random set of bytes, of length len */
uint8_t *hash_salt(size_t len);
void hash_salt_free(uint8_t *salt, size_t len);

/* generic function to zero a pointer of length len */
void hash_free_generic(void *p, size_t len) {
  if (!p)
    return;

  size_t i;
  volatile unsigned char *v = p;

  for (i = 0; i < len; i++)
    v[i] = 0;
  free(p);
}

#endif /* PATCH_HASH_H */
