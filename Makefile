.SUFFIXES:

include config.mk

-include version.mk

VERSION ?= unknown


SRC = reef.c mpd.c ui.c keybinds.c lyrics.c
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

LAYOUTS = $(wildcard layouts/*.h)

INCS = -I.

PATCHDEFS =
include $(PATCHES:%=patches/%/config.mk)

REEFCPPFLAGS = -D_POSIX_C_SOURCE=200809L -DVERSION="\"$(VERSION)\"" $(PATCHDEFS)
REEFCFLAGS   = -std=c99 -pedantic -Wall -Wextra -Wstrict-prototypes \
							 -Werror=implicit -Werror=return-type -MMD -MP \
							 -O2 $(INCS) $(PKG_INCS) $(REEFCPPFLAGS) $(CFLAGS)
LDLIBS     = $(PKG_LIBS)

all: reef

reef: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS) $(LDLIBS)

$(OBJ): config.h config.mk config.local.mk version.mk

GIT = git -c safe.directory="$(CURDIR)"

version.mk: FORCE
	@u='$(ORIGIN)'; d=; a=; c=; v=unknown; \
		h=$$($(GIT) rev-parse --short HEAD 2>/dev/null); \
		if [ -n "$$h" ]; then \
		$(GIT) diff --quiet 2>/dev/null || d=-dirty; \
		b=$$($(GIT) merge-base HEAD "$$u" 2>/dev/null); \
		if [ -n "$$b" ] && set -- $$($(GIT) rev-list --left-right --count \
		HEAD..."$$u" 2>/dev/null) && [ $$# -eq 2 ]; then \
		a=$$1; c=$$($(GIT) rev-list --count "$$b" 2>/dev/null); \
		v="local: $$h$$d  origin: $${u#*/}@$$($(GIT) rev-parse --short "$$b")  +$$1/-$$2"; \
		else \
		v=$$($(GIT) describe --tags --always --dirty 2>/dev/null); \
		[ -n "$$v" ] || v=$$h$$d; \
		fi; \
		elif [ -f $@ ]; then \
		exit 0; \
		fi; \
		printf '%s\n' \
		"VERSION = $$v" \
		"LOCAL = $$h$$d" \
		"AHEAD = $$a" \
		"BASECOUNT = $$c" > $@.tmp; \
		cmp -s $@.tmp $@ 2>/dev/null && rm -f $@.tmp || mv -f $@.tmp $@

FORCE:


config.h:
	cp config.def.h $@

config.local.mk:
	@printf '%s\n' \
		'# Enable patches by name here, matching the directory under patches/' \
		'PATCHES = remote ' > $@

clean:
	rm -f reef $(OBJ) $(DEP) patches/*/*.o patches/*/*.d version.mk version.mk.tmp

install: all
	mkdir -p $(PREFIX)/bin
	cp -f reef $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/reef

uninstall:
	rm -f $(PREFIX)/bin/reef

.SUFFIXES: .c .o
.c.o:
	$(CC) -c $(REEFCFLAGS) -o $@ $<

.PHONY: all clean install uninstall FORCE

-include $(DEP)
