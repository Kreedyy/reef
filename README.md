# Reef
Reef is an extensible, customizable and extremely lightweight TUI mpd client
built in C that follows the [suckless philosophy](https://suckless.org/philosophy/), with some minor sensible adjustments.

This project has no "versions", each commit is a
version you use to help pinpoint issues.  
Append `-v` or `--version` to see which origin commit you built off of.

Reading your version:
```
     local commit          origin commit         n commits ahead/behind
reef local: a1c3c6a-dirty  origin: main@ac04e6b  +n/-n
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

This will create `config.h` and `config.local.mk` for you to modify.

To add patches edit the `config.local.mk` file.  
A patch may pull in extra dependencies it relies on.

If the program fails to build after adding a patch try running:  
`make clean all 2>&1 | tee build.log`  
and inspecting the `build.log` file for any missing package errors.

Installing:  
`make install`

Default install path is `/usr/local/bin/` so make sure it is in your PATH.

## Contributing

### Recommended
- Add mouse click support
- Album art patch
- Visualizer patch

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
- Indent `case`s inside a `switch`

Match the codebase if anything else was missed.
