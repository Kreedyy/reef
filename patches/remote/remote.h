#ifndef PATCH_REMOTE_H
#define PATCH_REMOTE_H

#include <stdbool.h>

/* prints the version line with the live origin in it, so the origin field
 * names where the remote branch actually is now rather than where it was when this
 * was built. Blocks for at most timeout_ms, so this is only for the
 * -v/--version path, which exits before the event loop starts.
 */
bool remote_print_version(void);

#endif /* PATCH_REMOTE_H */
