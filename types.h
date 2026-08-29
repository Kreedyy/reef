#ifndef TYPES_H
#define TYPES_H

typedef union {
  int i;
  unsigned int ui;
  float f;
  const char *s;
  const void *v;
  bool b;
} Arg;

#endif /* TYPES_H */
