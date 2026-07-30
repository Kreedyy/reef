ORIGIN = origin/main

LOCAL    = $$(git rev-parse --short HEAD 2>/dev/null)$$(git diff --quiet 2>/dev/null || echo -dirty)
AHEAD    = $$(git rev-list --count $(ORIGIN)..HEAD 2>/dev/null)
BASE     = $$(git merge-base HEAD $(ORIGIN) 2>/dev/null)

BASECOUNT = $$(git rev-list --count $(BASE) 2>/dev/null)

BRANCH   = $(patsubst $(firstword $(subst /, ,$(ORIGIN)))/%,%,$(ORIGIN))

STAMP    = $(VERSION)

VERSION  = $$(u=$(ORIGIN); d=$$(git diff --quiet 2>/dev/null || echo -dirty); h=$$(git rev-parse --short HEAD 2>/dev/null) && b=$$(git merge-base HEAD $$u 2>/dev/null) && set -- $$(git rev-list --left-right --count HEAD...$$u 2>/dev/null) && [ $$\# -eq 2 ] && echo "local: $$h$$d  origin: $${u\#*/}@$$(git rev-parse --short $$b)  +$$1/-$$2" || git describe --tags --always --dirty 2>/dev/null)

PKG_CONFIG = pkg-config

PKGS     = panelw ncursesw libmpdclient libutf8proc
PKG_INCS = `$(PKG_CONFIG) --cflags $(PKGS)`
PKG_LIBS = `$(PKG_CONFIG) --libs $(PKGS)`

PREFIX    = /usr/local

CC = cc

-include config.local.mk
