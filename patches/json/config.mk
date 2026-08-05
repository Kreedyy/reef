ifndef _patch_json

_patch_json := 1

PATCHDEFS += -DPATCH_json
SRC  += patches/json/json.c
INCS += -Ipatches/json

endif
