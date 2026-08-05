#pragma once

#include <stdbool.h>
#include <stddef.h>

/* raw value of key, or NULL */
const char *json_value(const char *json, const char *end, const char *key);

/* string value of key, unescaped into out */
bool json_string(const char *json, const char *end, const char *key, char *out,
    size_t n);

/* next object in the range, *after set past it, or NULL */
const char *json_next_object(const char *json, const char *end,
    const char **after);

/* escapes s as a JSON string value, quotes not included */
size_t json_escape(const char *s, char *out, size_t n);
