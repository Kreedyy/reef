#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "crypto.h"

typedef struct{
  uint64_t count;
  uint32_t buffer[4];
  uint8_t input[64];
} MD5_CTX;

static uint32_t S[] = {
  7, 12, 17, 22, 7, 12, 17, 22,
  7, 12, 17, 22, 7, 12, 17, 22,
  5,  9, 14, 20, 5,  9, 14, 20,
  5,  9, 14, 20, 5,  9, 14, 20,
  4, 11, 16, 23, 4, 11, 16, 23,
  4, 11, 16, 23, 4, 11, 16, 23,
  6, 10, 15, 21, 6, 10, 15, 21,
  6, 10, 15, 21, 6, 10, 15, 21
};

static uint32_t K[] = {
  0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
  0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
  0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
  0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
  0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
  0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
  0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
  0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
  0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
  0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
  0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
  0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
  0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
  0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
  0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
  0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const uint8_t PADDING[64] = { 0x80 };

#define F(X, Y, Z) ((X & Y) | (~X & Z))
#define G(X, Y, Z) ((X & Z) | (Y & ~Z))
#define H(X, Y, Z) (X ^ Y ^ Z)
#define I(X, Y, Z) (Y ^ (X | ~Z))

#define ROTATE(x, n) ((x << n) | (x >> (32 - n)))

#define STEP(f, a, b, c, d, w, k, s) \
  (a) += f((b), (c), (d)) + (w) + (k);\
  (a) = (b) + ROTATE((a), (s))

#define R0(i) (i)
#define R1(i) ((5 * (i) + 1) % 16)
#define R2(i) ((3 * (i) + 5) % 16)
#define R3(i) ((7 * (i)) % 16)

#define a0 0x67452301
#define b0 0xefcdab89
#define c0 0x98badcfe
#define d0 0x10325476

static void
md5_block(uint32_t state[4], const uint8_t block[64]) {
  uint32_t word[16];
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];

  int i;

  for(i = 0; i < 16; i++) {
    word[i] = (uint32_t)block[4 * i] |
      (uint32_t)block[4 * i + 1] << 8 |
      (uint32_t)block[4 * i + 2] << 16 |
      (uint32_t)block[4 * i + 3] << 24;
  }

  for(i = 0; i < 64; i++) {
    int rnd = i / 16;
    int g = rnd == 0 ? R0(i) : rnd == 1 ? R1(i): rnd == 2 ? R2(i) : R3(i);

    switch (rnd) {
      case 0: STEP(F, a, b, c, d, word[g], K[i], S[i]); break;
      case 1: STEP(G, a, b, c, d, word[g], K[i], S[i]); break;
      case 2: STEP(H, a, b, c, d, word[g], K[i], S[i]); break;
      case 3: STEP(I, a, b, c, d, word[g], K[i], S[i]); break;
    }

    uint32_t new_a = d;
    d = c;
    c = b;
    b = a;
    a = new_a;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

uint8_t *
crypto_md5_hash(const void *data, size_t len) {
  uint32_t state[4] = { a0, b0, c0, d0 };
  const uint8_t *p = data;
  uint8_t *out = malloc(16);

  size_t i;

  if (!out)
    return NULL;

  for (i = 0; i + 64 <= len; i += 64)
    md5_block(state, p + i);

  uint8_t tail[128] = { 0 };
  size_t rem = len - i;
  size_t pad_len = rem < 56 ? 56 - rem : 120 - rem;
  size_t total = rem + pad_len + 8;
  uint64_t bits = (uint64_t)len * 8;

  memcpy(tail, p + i, rem);
  memcpy(tail + rem, PADDING, pad_len);

  for (i = 0; i < 8; i++)
    tail[rem + pad_len + i] = (uint8_t)(bits >> (8 * i));

  for (i = 0; i < total; i += 64)
    md5_block(state, tail + i);

  for (i = 0; i < 4; i++) {
    out[4 * i] = (uint8_t)state[i];
    out[4 * i + 1] = (uint8_t)(state[i] >> 8);
    out[4 * i + 2] = (uint8_t)(state[i] >> 16);
    out[4 * i + 3] = (uint8_t)(state[i] >> 24);
  }

  return out;
}

void
crypto_md5_free(uint8_t *digest) {
  crypto_free_generic(digest, MD5_DIGEST_LEN);
}
