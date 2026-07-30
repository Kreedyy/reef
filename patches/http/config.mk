ifndef _patch_http

_patch_http := 1

PATCHDEFS += -DPATCH_http
SRC  += patches/http/http.c
PKGS += libcurl
INCS += -Ipatches/http

endif
