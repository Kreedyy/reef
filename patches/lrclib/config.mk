ifndef _patch_lrclib

_patch_lrclib := 1

include patches/http/config.mk

PATCHDEFS += -DPATCH_lrclib
SRC  += patches/lrclib/fetcher.c patches/lrclib/draw.c patches/lrclib/solver.c
INCS += -Ipatches/lrclib

endif
