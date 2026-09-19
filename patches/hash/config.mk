ifndef _patch_hash

_patch_hash := 1

PATCHDEFS += -DPATCH_hash
SRC  += patches/hash/md5.c patches/hash/hex.c patches/hash/salt.c
INCS += -Ipatches/hash

endif
