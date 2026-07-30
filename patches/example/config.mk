# ALL patches must use an ifndef _patch_<name> guard
# so that multiple includes don't conflict

ifndef _patch_example

_patch_example := 1

# --- dependencies (optional) --------------------------------------------
#include patches/lrclib/config.mk

# external dependencies here if needed
#PKGS += libcurl

# --- this patch (required) ----------------------------------------------

# this will export a macro: PATCH_example, which you can test against
# in other parts of the program to extend their functionality.
PATCHDEFS += -DPATCH_example

# include all your source files here
SRC  += patches/example/example.c #patches/example/test.c ...

# this is so other patches can more easily include your header.
# #include "example.h" instead of #include "patches/example/example.h"
INCS += -Ipatches/example

endif
