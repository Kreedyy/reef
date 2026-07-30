#pragma once

typedef enum {
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT
} LyricsAlign;

/* a provider fetches lyrics for current track when no local copy exists. It
 * must be asynchronous and when it has them, call lyrics_set().
 * See patches/lrclib for reference.
 *
 * Always call lyrics_set(), pass NULL for the text on failure.
 * That is what hands the track on to the next registered provider.
 * Not doing this leaves the tab showing fetching lyrics forever. */
typedef void (*LyricsProvider)(const char *artist, const char *title);
void lyrics_set(const char *artist, const char *title, const char *text);

/* patches should not touch these */

void lyrics_init(void);

void lyrics_prefetch(void);

void lyrics_scroll(int delta);
void lyrics_scroll_edge(int bottom);
int lyrics_page_rows(void);
