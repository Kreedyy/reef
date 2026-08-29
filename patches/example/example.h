#ifndef PATCH_EXAMPLE_H
#define PATCH_EXAMPLE_H

#include "keybinds.h"
#include "layout.h"
#include "types.h"
#include <ncurses.h>
#include <stdbool.h>

void draw_example_testtab1(WINDOW *win);
void draw_example_testtab2(WINDOW *win);
void draw_example_testtab3(WINDOW *win);

bool example_active(void);
void example_move(int delta);
void example_nav(int dir);

void example_reset(const Arg *arg);

/* config.def.h pulls this into tabs[] under #ifdef PATCH_example. One patch may
 * contribute as many tabs as it likes: the macro is a list of initialisers, so
 * every line here becomes its own entry in tabs[]. Each carries its own redraw
 * mask, so a tab only repaints for the events it actually shows.
 * Make sure the last entry ends with a comma !! */
#define EXAMPLE_TABS \
{ "testtab1", draw_example_testtab1, REDRAW_FOCUS | REDRAW_KEYPRESS }, \
{ "testtab2", draw_example_testtab2, REDRAW_FOCUS | REDRAW_KEYPRESS }, \
{ "testtab3", draw_example_testtab3, REDRAW_PLAYER | REDRAW_TICK },

/* keys that only exist while one of this patch's tabs is focused. 'r' is
 * free globally so it simply becomes a new key, 'a' is add_to_queue in
 * keybinds[] and is taken over here, and an entry like { 'C', NULL, {0} }
 * would leave clear_queue unreachable on these tabs.
 *
 * The array stays private to the patch, tab_keybinds[] only points at it */
static const Keybind example_keybinds[] = {
  /* key  function        argument */
  { 'r',  example_reset,  {0} },
  { 'a',  example_reset,  {0} },
/*{ 'C',  NULL,           {0} },  */
};

/* config.def.h pulls this into tab_keybinds[] under #ifdef PATCH_example.
 * One entry per tab that wants these keys, tabs left out of it keep the
 * global keybinds unchanged. testtab3 is one of those.
 * Make sure the last entry ends with a comma !! */
#define EXAMPLE_KEYBINDS \
TAB_KEYBINDS(draw_example_testtab1, example_keybinds), \
TAB_KEYBINDS(draw_example_testtab2, example_keybinds),

#endif /* PATCH_EXAMPLE_H */
