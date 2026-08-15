#pragma once

#include <stdbool.h>

char *cred_get(const char *cmd);
char *cred_get_notty(const char *cmd, bool *needs_tty);

/* zeroes the secret before releasing it. NULL is fine */
void cred_free(char *secret);
