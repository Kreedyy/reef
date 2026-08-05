#include <stdio.h>
#include <string.h>

#include "json.h"

static bool
is_space(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static const char *
skip_string(const char *p, const char *end) {
  for (p++; p < end; p++) {
    if (*p == '\\')
      p++;
    else if (*p == '"')
      return p + 1;
  }
  return end;
}

static bool
hex4(const char *p, const char *end, long *out) {
  long v = 0;
  int i;

  if (end - p < 4)
    return false;
  for (i = 0; i < 4; i++) {
    char c = p[i];

    if (c >= '0' && c <= '9')
      v = v * 16 + (c - '0');
    else if (c >= 'a' && c <= 'f')
      v = v * 16 + (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F')
      v = v * 16 + (c - 'A' + 10);
    else
      return false;
  }
  *out = v;
  return true;
}

static bool
unescape(const char **pp, const char *end, long *cp) {
  const char *p = *pp;
  char e = *p++;
  long lo;

  switch (e) {
    case 'n':
      *cp = '\n';
      break;
    case 't':
      *cp = '\t';
      break;
    case 'b':
    case 'f':
    case 'r':
      *pp = p;
      return false;
    case 'u':
      if (!hex4(p, end, cp)) {
        *pp = end;
        return false;
      }
      p += 4;
      if (*cp >= 0xd800 && *cp <= 0xdbff && end - p >= 6 && p[0] == '\\' &&
        p[1] == 'u' && hex4(p + 2, end, &lo) && lo >= 0xdc00 &&
        lo <= 0xdfff) {
        *cp = 0x10000 + ((*cp - 0xd800) << 10) + (lo - 0xdc00);
        p += 6;
      } else if (*cp >= 0xd800 && *cp <= 0xdfff) {
        *pp = p;
        return false;
      }
      break;
    default:
      *cp = (unsigned char)e;
      break;
  }
  *pp = p;
  return true;
}

static bool
put_utf8(char *out, size_t n, size_t *o, long cp) {
  char b[4];
  size_t len, i;

  if (cp < 0x80) {
    b[0] = (char)cp;
    len = 1;
  } else if (cp < 0x800) {
    b[0] = (char)(0xc0 | (cp >> 6));
    b[1] = (char)(0x80 | (cp & 0x3f));
    len = 2;
  } else if (cp < 0x10000) {
    b[0] = (char)(0xe0 | (cp >> 12));
    b[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
    b[2] = (char)(0x80 | (cp & 0x3f));
    len = 3;
  } else {
    b[0] = (char)(0xf0 | (cp >> 18));
    b[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    b[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
    b[3] = (char)(0x80 | (cp & 0x3f));
    len = 4;
  }
  if (*o + len + 1 > n)
    return false;
  for (i = 0; i < len; i++)
    out[(*o)++] = b[i];
  return true;
}

const char *
json_value(const char *json, const char *end, const char *key) {
  size_t kl = strlen(key);
  const char *p;
  int depth = 0;

  for (p = json; p < end && *p != '{'; p++)
    ;

  for (; p < end; p++) {
    if (*p == '{' || *p == '[') {
      depth++;
      continue;
    }
    if (*p == '}' || *p == ']') {
      if (--depth == 0)
        break;
      continue;
    }
    if (*p != '"')
      continue;

    /* a string starts here. Ours if it reads as the key, a colon follows, and
     * depth says it belongs to this object rather than a nested one */
    if (depth == 1 && (size_t)(end - p) > kl + 1 &&
      memcmp(p + 1, key, kl) == 0 && p[kl + 1] == '"') {
      const char *v = p + kl + 2;

      while (v < end && is_space(*v))
        v++;
      if (v < end && *v == ':') {
        for (v++; v < end && is_space(*v); v++)
          ;
        return v < end ? v : NULL;
      }
    }
    /* step over the whole string, which is what keeps a key name sitting
     * inside some value from matching */
    p = skip_string(p, end) - 1;
  }
  return NULL;
}

bool
json_string(const char *json, const char *end, const char *key, char *out,
            size_t n) {
  const char *p = json_value(json, end, key);
  size_t o = 0;

  if (n == 0)
    return false;
  out[0] = '\0';
  if (p == NULL || *p != '"')
    return false;

  for (p++; p < end && *p != '"'; ) {
    long cp;

    if (*p != '\\') {
      unsigned char c = (unsigned char)*p;
      size_t len = 1;

      if (c >= 0xf0)
        len = 4;
      else if (c >= 0xe0)
        len = 3;
      else if (c >= 0xc0)
        len = 2;

      if ((size_t)(end - p) < len || o + len + 1 > n)
        break;
      while (len-- > 0)
        out[o++] = *p++;
      continue;
    }
    if (++p >= end)
      break;
    if (unescape(&p, end, &cp) && !put_utf8(out, n, &o, cp))
      break;
  }
  out[o] = '\0';
  return true;
}

const char *
json_next_object(const char *json, const char *end, const char **after) {
  const char *p = json, *start;
  int depth = 0;

  while (p < end && *p != '{')
    p++;
  if (p == end)
    return NULL;
  start = p;

  for (; p < end; p++) {
    if (*p == '"')
      p = skip_string(p, end) - 1;
    else if (*p == '{')
      depth++;
    else if (*p == '}' && --depth == 0) {
      *after = p + 1;
      return start;
    }
  }
  return NULL;
}

size_t
json_escape(const char *s, char *out, size_t n) {
  size_t o = 0;

  for (; *s != '\0'; s++) {
    unsigned char c = (unsigned char)*s;
    char buf[7];
    const char *rep = buf;
    size_t len = 2;

    switch (c) {
      case '"':
        rep = "\\\"";
        break;
      case '\\':
        rep = "\\\\";
        break;
      case '\n':
        rep = "\\n";
        break;
      case '\t':
        rep = "\\t";
        break;
      case '\r':
        rep = "\\r";
        break;
      case '\b':
        rep = "\\b";
        break;
      case '\f':
        rep = "\\f";
        break;
      default:
        /* the rest of the control range has no short form, 0x20 up is fine
         * as it stands, UTF-8 included */
        if (c < 0x20) {
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          len = 6;
        } else {
          buf[0] = (char)c;
          len = 1;
        }
        break;
    }
    for (; len > 0; len--, rep++, o++) {
      if (o + 1 < n)
        out[o] = *rep;
    }
  }
  if (n > 0)
    out[o < n ? o : n - 1] = '\0';
  return o;
}
