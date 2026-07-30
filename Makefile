.SUFFIXES:

include config.mk


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

$(OBJ): config.h config.mk config.local.mk .stamp

.stamp: FORCE
	@printf '%s\n' "$(STAMP)" > $@.tmp
	@cmp -s $@.tmp $@ 2>/dev/null && rm -f $@.tmp || mv -f $@.tmp $@

FORCE:


config.h:
	cp config.def.h $@

config.local.mk:
	@printf '%s\n' \
		'# Enable patches by name here, matching the directory under patches/' \
		'PATCHES = remote ' > $@

clean:
	rm -f reef $(OBJ) $(DEP) patches/*/*.o patches/*/*.d .stamp .stamp.tmp

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
