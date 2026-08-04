#include <ncurses.h>
#include <stddef.h>

#include "config.h"
#include "keybinds.h"
#include "mpd.h"
#include "ui.h"

#ifdef PATCH_lrclib
#include "lrclib.h"
#endif


#define TABLE_BITS 7
#define TABLE_SIZE (1u << TABLE_BITS)
#define LENGTH(X) (sizeof(X) / sizeof((X)[0]))

static Keybind table[TABLE_SIZE];

static unsigned
hash(int key) {
  /* knuth's multiplicative hash. Its good distribution lives in the high
   * bits of the product, so shift them down to index the table rather
   * than taking the low bits with a modulo */
  return ((unsigned)key * 2654435761u) >> (32 - TABLE_BITS);
}

static void
bind_insert(int key, void (*action)(const Arg *), Arg arg) {
  unsigned i = hash(key);
  unsigned start = i;

  while (table[i].action) {
    i = (i + 1) % TABLE_SIZE;
    if (i == start)
      return;
  }
  table[i] = (Keybind){ key, action, arg };
}

static Keybind *
bind_lookup(int key) {
  unsigned i = hash(key);
  unsigned start = i;

  while (table[i].action) {
    if (table[i].key == key)
      return &table[i];
    i = (i + 1) % TABLE_SIZE;
    if (i == start)
      break;
  }
  return NULL;
}

void
cycle_tab(const Arg *arg) {
  if (arg->i < 0)
    focus_prev_tab();
  else
    focus_next_tab();
}

int running = 1;

void
quit(const Arg *arg) {
  (void)arg;
  running = 0;
}

static const Keybind *
local_lookup(int key) {
  size_t t, n;

  for (t = 0; t < LENGTH(tab_keybinds); t++) {
    if (!tab_keybinds[t].draw || !tab_active(tab_keybinds[t].draw))
      continue;
    for (n = 0; n < tab_keybinds[t].count; n++)
      if (tab_keybinds[t].binds[n].key == key)
        return &tab_keybinds[t].binds[n];
  }
  return NULL;
}

typedef enum { ARG_NONE, ARG_I, ARG_B } ArgKind;

static int
arg_match(const Arg *bound, const Arg *want, ArgKind kind) {
  switch (kind) {
    case ARG_I:
      return bound->i == want->i;
    case ARG_B:
      return bound->b == want->b;
    case ARG_NONE:
      break;
  }
  return 1;
}

static int
scan(const Keybind *binds, size_t count, void (*action)(const Arg *),
     const Arg *arg, ArgKind kind) {
  size_t n;

  for (n = 0; n < count; n++)
    if (binds[n].action == action && arg_match(&binds[n].arg, arg, kind))
      return binds[n].key;
  return -1;
}

static int
lookup_key(void (*action)(const Arg *), const Arg *arg, ArgKind kind) {
  const Keybind *shadow;
  size_t t;
  int key;

  for (t = 0; t < LENGTH(tab_keybinds); t++) {
    if (!tab_keybinds[t].draw || !tab_active(tab_keybinds[t].draw))
      continue;
    key = scan(tab_keybinds[t].binds, tab_keybinds[t].count, action, arg,
               kind);
    if (key >= 0)
      return key;
  }

  key = scan(keybinds, LENGTH(keybinds), action, arg, kind);
  if (key < 0)
    return -1;

  /* the global key exists but the focused tab points it elsewhere, so as
   * far as this tab is concerned the action is unbound */
  shadow = local_lookup(key);
  if (shadow &&
    (shadow->action != action || !arg_match(&shadow->arg, arg, kind)))
    return -1;
  return key;
}

int
key_for_action(void (*action)(const Arg *)) {
  return lookup_key(action, NULL, ARG_NONE);
}

int
key_for_action_i(void (*action)(const Arg *), int i) {
  return lookup_key(action, &(Arg){ .i = i }, ARG_I);
}

int
key_for_action_b(void (*action)(const Arg *), bool b) {
  return lookup_key(action, &(Arg){ .b = b }, ARG_B);
}

void
init_keybinds(void) {
  size_t i;

  for (i = 0; i < LENGTH(keybinds); i++)
    bind_insert(keybinds[i].key, keybinds[i].action, keybinds[i].arg);
}

void
handle_key(int input) {
  const Keybind *kb;

  /* patches that have things which need control over the input
 * can insert themselves here in a similar way to navigation in ui.c
 *
 * #ifdef PATCH_example
 *  if (example_active()) {
 *    example_input(input);
 *    update_panels();
 *    doupdate();
 *    return;
 *  }
 * #endif
 */

#ifdef PATCH_lrclib
  if (lrclib_sync_active()) {
    if (input == key_for_action_b(lrclib_set_insert_mode, false))
      lrclib_set_insert_mode(&(Arg){ .b = false });
    else if (input == key_for_action_b(lrclib_set_insert_mode, true))
      lrclib_set_insert_mode(&(Arg){ .b = true });
    if (lrclib_insert_mode_active())
      lrclib_handle_input(input);
    update_panels();
    doupdate();
    ui_redraw(REDRAW_KEYPRESS);
    return;
  }
#endif

  if (search_edit_active()) {
    search_edit_key(input);
    update_panels();
    doupdate();
    ui_redraw(REDRAW_KEYPRESS);
    return;
  }
  if (find_active()) {
    find_input(input);
    update_panels();
    doupdate();
    ui_redraw(REDRAW_KEYPRESS);
    return;
  }

  /* the focused tab gets first refusal on the key, keybinds[] only sees
   * what it did not name */
  kb = local_lookup(input);
  if (!kb)
    kb = bind_lookup(input);

  if (!kb)
    focus_tab_by_key(input);
  else if (kb->action)
    kb->action(&kb->arg);
  update_panels();
  doupdate();
  ui_redraw(REDRAW_KEYPRESS);
}
