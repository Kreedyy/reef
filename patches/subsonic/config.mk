ifndef _patch_subsonic

_patch_subsonic := 1

include patches/http/config.mk
include patches/json/config.mk

PATCHDEFS += -DPATCH_subsonic
#PKGS += libcrypto
SRC  += patches/subsonic/fetcher.c
INCS += -Ipatches/subsonic

#CFLAGS  += -pthread
#LDFLAGS += -pthread

endif
