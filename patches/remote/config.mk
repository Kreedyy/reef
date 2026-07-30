ifndef _patch_remote

_patch_remote := 1

include patches/http/config.mk

PATCHDEFS += -DPATCH_remote \
	-DREMOTE_LOCAL="\"$(LOCAL)\"" \
	-DREMOTE_AHEAD="\"$(AHEAD)\"" \
	-DREMOTE_BASECOUNT="\"$(BASECOUNT)\"" \
	-DREMOTE_BRANCH="\"$(BRANCH)\""

STAMP += $(LOCAL)$(AHEAD)$(BASECOUNT)

SRC  += patches/remote/remote.c
INCS += -Ipatches/remote

endif
