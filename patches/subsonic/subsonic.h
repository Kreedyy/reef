#ifndef PATCH_SUBSONIC_H
#define PATCH_SUBSONIC_H

#include <stdbool.h>
#include <ncurses.h>

#define SUBSONIC_STATUS_SECONDS 3

static const char *const subsonic_api_version = "1.16.1";

void subsonic_ping_server(void);
bool subsonic_is_connected(void);
void subsonic_get_artists(void);
void subsonic_stream(char *id);
void subsonic_download(char *id);
void subsonic_search3(char *q);

#endif /* PATCH_SUBSONIC_H */
