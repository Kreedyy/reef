ifndef _patch_lrclib

_patch_lrclib := 1

include patches/http/config.mk

PATCHDEFS += -DPATCH_lrclib
SRC  += patches/lrclib/lrclib.c
INCS += -Ipatches/lrclib

endif
