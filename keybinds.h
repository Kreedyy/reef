#pragma once
#include "types.h"

typedef struct {
  int key;
  void (*action)(const Arg *arg);
  Arg arg;
} Keybind;

extern int running;

void init_keybinds(void);
void cycle_tab(const Arg *arg);
void quit(const Arg *arg);
int key_for_action(void (*action)(const Arg *arg));
int key_for_action_i(void (*action)(const Arg *arg), int i);

void handle_key(int input);
