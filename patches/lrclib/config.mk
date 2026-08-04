ifndef _patch_lrclib

_patch_lrclib := 1

include patches/http/config.mk
include patches/json/config.mk

PATCHDEFS += -DPATCH_lrclib
PKGS += libcrypto
SRC  += patches/lrclib/fetcher.c patches/lrclib/draw.c \
        patches/lrclib/solver.c patches/lrclib/publish.c
INCS += -Ipatches/lrclib

CFLAGS  += -pthread
LDFLAGS += -pthread

endif
