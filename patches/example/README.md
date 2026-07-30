# Description
Example patch. Adds three tabs to demonstrate what a patch could do:
- `testtab1`: two columns of dummy rows, with a cursor that moves up/down and left/right.
- `testtab2`: a single list, cursor up/down only.
- `testtab3`: the playing song, no cursor at all.

Between them they cover:

- **Contributing tabs** without touching any layout calculations/geometry:  
The entries come from `EXAMPLE_TABS` in `example.h`, which [`config.def.h`](../../config.def.h) pulls
into `tabs[]` under `#ifdef PATCH_example`.
- **Hooking into core behaviour:**  
See [`ui.c`](../../ui.c)'s `nav()` and `cursor_by()` functions.  
`example_active()` lists only the tabs that want the cursor keys, so
`testtab3` keeps reef's default handling.
- **Drawing a hint line** whose keys come from the user's `config.h` rather than
from hardcoded strings, via `hint_add()` and `hint_add_i()` from `ui.h`.

See [docs/patches.md](../../docs/patches.md) for a more thorough walkthrough.

# Dependencies
List other patches/packages that this patch includes/depends on:

- [Lrclib](../lrclib)
- libcurl

(Doesn't actually pull these in but this is how you format them)

# Authors
The creator should always be at the top. Contributors are ordered from latest
to oldest under the creator.  
The linked git site should be where you
contributed from and is assumed to be your main way of contact unless
specified otherwise with **(primary)**.

- **Creator:** [Kreedy](https://git.gay/kreedy)
  - Github: [Kreedyy](https://github.com/kreedyy)
  - Mail: kreedy@email.com
  - (Other contact methods)

- **Contributor**: [Kreedy](https://git.gay/kreedy)
  - Mail: kreedy@email.com **(primary)**
