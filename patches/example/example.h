#pragma once

#include "layout.h"
#include <ncurses.h>
#include <stdbool.h>

void draw_example_testtab1(WINDOW *win);
void draw_example_testtab2(WINDOW *win);
void draw_example_testtab3(WINDOW *win);

bool example_active(void);
void example_move(int delta);
void example_nav(int dir);

/* config.def.h pulls this into tabs[] under #ifdef PATCH_example. One patch may
 * contribute as many tabs as it likes: the macro is a list of initialisers, so
 * every line here becomes its own entry in tabs[]. Each carries its own redraw
 * mask, so a tab only repaints for the events it actually shows.
 * Make sure the last entry ends with a comma !! */
#define EXAMPLE_TABS \
{ "testtab1", draw_example_testtab1, REDRAW_FOCUS }, \
{ "testtab2", draw_example_testtab2, REDRAW_FOCUS }, \
{ "testtab3", draw_example_testtab3, REDRAW_PLAYER | REDRAW_TICK },
