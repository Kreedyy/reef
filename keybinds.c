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

int
key_for_action(void (*action)(const Arg *)) {
  size_t i;

  for (i = 0; i < LENGTH(keybinds); i++)
    if (keybinds[i].action == action)
      return keybinds[i].key;
  return -1;
}

int
key_for_action_i(void (*action)(const Arg *), int i) {
  size_t n;

  for (n = 0; n < LENGTH(keybinds); n++)
    if (keybinds[n].action == action && keybinds[n].arg.i == i)
      return keybinds[n].key;
  return -1;
}

void
init_keybinds(void) {
  size_t i;

  for (i = 0; i < LENGTH(keybinds); i++)
    bind_insert(keybinds[i].key, keybinds[i].action, keybinds[i].arg);
}

void
handle_key(int input) {
  Keybind *kb;

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
    if (input == 'i') {
      lrclib_handle_input(input);
      update_panels();
      doupdate();
      ui_redraw(REDRAW_KEYPRESS);
      return;
    }
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

  kb = bind_lookup(input);
  if (kb)
    kb->action(&kb->arg);
  else
    focus_tab_by_key(input);
  update_panels();
  doupdate();
  ui_redraw(REDRAW_KEYPRESS);
}
