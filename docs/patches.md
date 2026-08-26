# Patches

A patch is a directory under `patches/` plus a `config.mk`.  
Enable patches in `config.patch.mk`, which `make` creates for you on the first build.

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
two others:

```make
include patches/http/config.mk
include patches/json/config.mk
```

Both are *library patches*, never enabled on their own and only pulled in by  
patches that need them. `http` exposes non-blocking `http_get()` and  
`http_post()` built on libcurl, and `json` reads the fields back out of what
they answer with.  
See [http](../patches/http/README.md) and [json](../patches/json/README.md).

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

---

Find/Filter (`/`) needs one more block in `focus_get()`, which is what hands  
the keys the list to work on.

Your patch owns the list and the cursor, `focus_get()` only points at them:

```c
/* patches/example/example.c */
SongList example_songs;  /* your rows */
ListView example_view;   /* their cursor */
```

```c
/* ui.c */
static bool
focus_get(RowList *f) {

  /* existing stuff... */

#ifdef PATCH_example
  if (example_active()) {
    /* row_list(playlists, dirs, songs, cursor) NULL for the kinds you do not have */
    *f = row_list(NULL, NULL, &example_songs, &example_view);
    return true;
  }
#endif

  /* existing stuff... */
}
```

The view cannot be `NULL` as `filter_results()` reads its cursor.  
The same block hands your tab multi-selection and the add keys (`a`, `A`) as
well, they all ask `focus_get()` for the list.

A `RowList` carries playlists, directories and songs. Find matches each of them by  
its own rule in `focus_matches()`. A row that is none of the three is read as a song.  

For rows that are only text, borrow a `DirList`. It is the lightest of the three
and find matches a directory on its `name`, so your string goes there:

```c
/* patches/example/example.c */
static Dir example_rows[] = {
  { .name = "first row"  },  /* .path stays empty */
  { .name = "second row" },
};

/* items, count, cap */
static DirList example_list = { example_rows, 2, 2 };
```

Then hand that over as the dirs argument instead of the songs one:

```c
/* ui.c, inside your focus_get() block */
*f = row_list(NULL, &example_list, NULL, &example_view);
```

Leaving `path` empty is fine for find, but `a` (add to queue) sends it to mpd,
so fill it in or leave those keys out.

Find draws nothing by itself. All it does is work out which row matched, put  
that row number into `view->cursor` and ask for a repaint.  
Showing it is your draw function's job: highlighting that row and  
scrolling so the row is on screen.

A patch with an input field wants raw keys instead, before the keybind table is
consulted, so typing `q` inserts a `q` rather than quitting.  
Same idea, but the block goes at the top of `handle_key()` in [`keybinds.c`](../keybinds.c).

Any part of the core program can be extended like this, these are just already implemented.  

## Adding a keybind

### Global

A key that belongs to the whole program goes in `keybinds[]` in `config.def.h`,
guarded so it only exists when the patch does:

```c
/* config.def.h */
static const Keybind keybinds[] = {
  /* existing binds... */

#ifdef PATCH_login
  { 'L', login_prompt, {0} },
#endif
};
```

### Local

A key that only makes sense on your own tab goes in a keybind array of your own
instead and `tab_keybinds[]` points at it.  
It overrides `keybinds[]` *while that tab is focused* and every key it does not  
name still falls through to the global array.

Declare the array and a `_KEYBINDS` macro in your patch header:

```c
/* patches/login/login.h */
void login_submit(const Arg *arg);
void login_clear(const Arg *arg);

static const Keybind login_keybinds[] = {
  /* key  function       argument */
  { 'r',  login_submit,  {0} },
  { 'a',  login_clear,   {0} },
  { 'C',  NULL,          {0} },
};

/* one entry per tab that wants these keys, tabs left out keep the
 * global binds unchanged.
 * Make sure the last entry ends with a comma !! */
#define LOGIN_KEYBINDS \
TAB_KEYBINDS(draw_login_form, login_keybinds), \
TAB_KEYBINDS(draw_login_settings, login_keybinds),
```

Then the guarded block in `config.def.h`:

```c
/* config.def.h */
static const TabKeybind tab_keybinds[] = {

#ifdef PATCH_login
  LOGIN_KEYBINDS
#endif

  { NULL, NULL, 0 },
};
```

`hint_add()` looks the focused tab's binds up before `keybinds[]` does.  
It also removes the hint inside the keybind bar on a tab where said  
key is overwritten.

If a tab wants to consume raw keys i.e. typing out `q` instead of quitting  
look at `handle_key()` in [`keybinds.c`](keybinds.c).

## Drawing text

Use `draw_text()`, `draw_text_centered()`, `draw_text_right()` and
`text_width()` from `ui.h` instead of ones ncurses ship.  
This is because ncurses' count bytes and not columns, which makes special
characters overflow when disagreeing with terminal.  
The exported functions from `ui.h` aim to solve this.

## Credentials

Fetch with `ui_cred_get()` and free with `cred_free()`:

```c
/* config.def.h */
#ifdef PATCH_login
static const char *const login_password_cmd = NULL;
#endif

/* login.c */
char *pw = ui_cred_get(login_password_cmd);

if (pw == NULL)
  return;

login_send(pw);
cred_free(pw);
```

We do not want to store credentials, so free as soon as you are done.  
A value other than `NULL` only means the command printed something, not that it works.  
If the service rejects it, reporting it is your job.

## Updating a patch

Open an issue or PR mentioning the patch's author for further discussion.
