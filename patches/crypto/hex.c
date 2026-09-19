#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "crypto.h"

static const char HEX_CHARS[] = "0123456789abcdef";

char *
crypto_to_hex(const uint8_t *data, size_t len) {
  char *hex = malloc(2 * len + 1);

  if (!hex)
    return NULL;

  size_t i;

  for (i = 0; i < len; i++) {
    hex[2 * i] = HEX_CHARS[data[i] >> 4];
    hex[2 * i + 1] = HEX_CHARS[data[i] & 0x0f];
  }
  hex[2 * len] = '\0';

  return hex;
}

void
crypto_to_hex_free(char *hex) {
  if (!hex)
    return;

  crypto_free_generic(hex, strlen(hex + 1));
}
