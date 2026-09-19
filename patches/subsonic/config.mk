ifndef _patch_subsonic

_patch_subsonic := 1

include patches/http/config.mk
include patches/json/config.mk
include patches/hash/config.mk

PATCHDEFS += -DPATCH_subsonic
SRC  += patches/subsonic/fetcher.c
INCS += -Ipatches/subsonic

endif
