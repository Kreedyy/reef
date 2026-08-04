#pragma once
#include <ncurses.h>
#include <stddef.h>

#include "types.h"

typedef struct {
  int key;
  void (*action)(const Arg *arg);
  Arg arg;
} Keybind;

typedef struct {
  void (*draw)(WINDOW *win);
  const Keybind *binds;
  size_t count;
} TabKeybind;

#define TAB_KEYBINDS(draw, binds) \
  { draw, binds, sizeof(binds) / sizeof((binds)[0]) }

int key_for_action(void (*action)(const Arg *arg));
int key_for_action_i(void (*action)(const Arg *arg), int i);

void cycle_tab(const Arg *arg);

/* patches should not touch these */
extern int running;

void init_keybinds(void);

void quit(const Arg *arg);

void handle_key(int input);
