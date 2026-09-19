ifndef _patch_crypto

_patch_crypto := 1

PATCHDEFS += -DPATCH_crypto
SRC  += patches/crypto/md5.c patches/crypto/hex.c patches/crypto/random.c
INCS += -Ipatches/crypto

endif
