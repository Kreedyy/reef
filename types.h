#pragma once

typedef union {
  int i;
  unsigned int ui;
  float f;
  const char *s;
  const void *v;
} Arg;
