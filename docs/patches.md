# Patches

A patch is a directory under `patches/` plus a `config.mk`.  
Enable patches in `config.local.mk`, which `make` creates for you on the first build.

Each name pulls in `patches/<name>/config.mk`, and that file is what defines
`-DPATCH_<name>` for everyone else to test against.  
[`patches/example`](../patches/example/), [`patches/http`](../patches/http/) and [`patches/lrclib`](../patches/lrclib/) are some patches you can reference.

## Files

A patch should have *one* header file which exposes functions others can use.
The header file *must* keep the same name as the patch dir name, this is
to avoid collisions and make naming consistent.
Source files can be named whatever as they are contained within your patch.

A patch's `config.mk` adds its own define, sources and dependencies.  
The `ifndef` guard is required, without it a patch pulled in by two others would
append everything twice:

```make
ifndef _patch_<name>

_patch_<name> := 1

PATCHDEFS += -DPATCH_<name>
SRC       += patches/<name>/foo.c
PKGS      += libfoo
INCS      += -Ipatches/<name>

endif
```

Include a README.md too, see [`patches/example/README.md`](../patches/example/README.md) for formatting.

## Cross referencing

One patch can build on another by including its `config.mk`, which is safe
because of the `ifndef` guard above.

By default the lyrics tab only reads local .lrc files. The `lrclib` patch
fetches from lrclib.net when no local file is found, and to do that it pulls in
`patches/http`:

```make
include patches/http/config.mk
```

`http` is a *library patch*: it exposes non-blocking `http_get()` and `http_post()`
built on libcurl and is never enabled on its own, only pulled in
by patches that need networking.  
See [its README.md](../patches/http/README.md).

A patch can also check `#ifdef PATCH_<name>` to extend itself only when another
patch happens to be enabled, without depending on it.

## Adding a tab

Declare the draw function and a `_TABS` macro in your patch header, then have
`config.def.h` include that header under an `#ifdef PATCH_*` guard.  
All functions ***must*** include patch name to not collide with other existing ones:

```c
/* patches/login/login.c */
void draw_login_form(WINDOW *win) {
  int h, w;
  getmaxyx(win, h, w);
  draw_text_centered(win, h / 2, 0, w, "Log in");
}

/* patches/login/login.h */
void draw_login_form(WINDOW *win);

#define LOGIN_TABS \
/* title   draw function   redraw on */
{ "Login", draw_login_form, REDRAW_FOCUS } \
{ "Test",  draw_login_form, REDRAW_FOCUS } \
{ "Test2", draw_login_form, REDRAW_FOCUS },

/* you can add multiple using \, make sure last one has a comma */
```

Then add two guarded blocks to `config.def.h`, the include near the top and the
tab entry inside `tabs[]`:

```c
/* config.def.h */
#ifdef PATCH_login
#include "login.h"
#endif

/* existing stuff... */

static const Pane tabs[] = {
  /* existing tabs... */

#ifdef PATCH_login
  LOGIN_TABS
#endif
};
```

`REDRAW_*` flags are in `layout.h`. Pick the narrowest that keeps your pane
current.  
In other words only redraw what is needed for your pane to stay
updated.  
For drawing helpers see [layouts.md](layouts.md), and for color [themes.md](themes.md).

## Extending core behaviour

[`patches/example.c`](../patches/example/example.c) is one example implementation on how
to extend core behaviour.  
It adds itself to the cursor navigation functions
inside of `ui.c` so that the bound keys from `config.h` can be reused.

```c
/* patches/example.h */
void draw_example_testtab1(WINDOW *win);

bool example_active(void);

void example_move(int delta);
```

Add yourself with a guarded block:

```c
/* ui.c */
static void
cursor_by(int delta) {

  /* existing stuff... */

#ifdef PATCH_example
  if (example_active()) {
    example_move(delta);
    cursor_repaint();
    return;
  }
#endif

  /* existing stuff... */
}
```

**`cursor_repaint()` and `return` are both required.**

With several tabs, `example_active()` is what decides which of them the keys reach,
and `ui.c` calls it from one guarded block per key group, not one per tab.  
So `example_active()` has to `||` together every tab that wants a cursor.  
A tab left out of it keeps the default behaviour, which is what you want for one that has
nothing to move.  
Then sort out which tab it is inside your own move function,
and give each its own cursor so switching tabs does not drag the other's
selection along.

There are four functions that handle the cursor navigation, see [ui.h](../ui.h).

A patch with an input field wants raw keys instead, before the keybind table is
consulted, so typing `q` inserts a `q` rather than quitting.  
Same idea, but the block goes at the top of `handle_key()` in [`keybinds.c`](../keybinds.c).

Any part of the core program can be extended like this, these are just already implemented.  

## Drawing text

Use `draw_text()`, `draw_text_centered()`, `draw_text_right()` and
`text_width()` from `ui.h` instead of ones ncurses ship.  
This is because ncurses' count bytes and not columns, which makes special
characters overflow when disagreeing with terminal.  
The exported functions from `ui.h` aim to solve this.

## Updating a patch

Open an issue or PR mentioning the patch's author for further discussion.
