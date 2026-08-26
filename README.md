# Reef
Reef is an extensible, customizable and extremely lightweight TUI mpd client
built in C that follows the [suckless philosophy](https://suckless.org/philosophy/), with some minor sensible [adjustments](#formatting).

This project has no "versions", each commit is a
version you use to help pinpoint issues.

By default the [`remote`](patches/remote) patch is added to track the remote origin commit history,
without the patch reef will track the local origin commit history meaning you
will have to manually pull in remote changes to see if you are on the latest version.

Append `-v` or `--version` to see which origin commit you built off of.

Reading your version:
```
     local commit          origin commit         ahead/behind  tracking remote?
reef local: a1c3c6a-dirty  origin: main@ac04e6b  +n/-n         remote: true
```
`-dirty` means the tree had uncommitted changes when you built.  
The origin
is the merge base with `origin/main`, so it names the upstream commit you
branched from.

Official repos:  
[git.gay](https://git.gay/kreedy/reef)  
[Github](https://github.com/kreedyy/reef)

## Building

Base Dependencies:  
`gmake`  
`ncurses`  
`libmpdclient`  
`libutf8proc`  

First build run:  
`make`

This will create `config.h` and `config.patch.mk` for you to modify.

To add patches edit the `config.patch.mk` file.  
A patch may pull in extra dependencies it relies on.

If the program fails to build after adding a patch try running:  
`make clean all 2>&1 | tee build.log`  
and inspecting the `build.log` file for any missing package errors.

If it fails to build after pulling changes make sure your `config.h`  
includes any new changes from `config.def.h` by running:  
`git diff --no-index config.def.h config.h`

Installing:  
`make install`

Default install path is `/usr/local/bin/` so make sure it is in your PATH.  
This can be changed with `PREFIX`:  
`make PREFIX=/usr install`

## Contributing

### Recommended
- Themes
- Add mouse click support
- Album art patch
- Visualizer patch
- Improve documentation and comments

### Documentation
- [Themes](docs/themes.md)
- [Layouts](docs/layouts.md)
- [Patches](docs/patches.md)

### Formatting
Each item takes precedence over the ones before it:
- [suckless coding style](https://suckless.org/coding_style/)
- Tab = 2 spaces
- Use the `#pragma once` guard
- Use the `bool` type
- Indent `case` inside a `switch`

Match the codebase if anything else was missed.
