#ifndef LYRICS_H
#define LYRICS_H

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

/* writes text as this track's .lrc in the lyrics directory, the same file
 * lyrics_set() caches to and the same one the tab reads back. Fills path with
 * where it went when path is not NULL. Returns false if it could not be
 * written */
bool lyrics_save(const char *artist, const char *title, const char *text,
    char *path, size_t cap);

/* patches should not touch these */

void init_lyrics(void);

void lyrics_prefetch(void);

int lyrics_next_line_in(void);

void lyrics_scroll(int delta);
void lyrics_scroll_edge(int bottom);
int lyrics_page_rows(void);

#endif /* LYRICS_H */
