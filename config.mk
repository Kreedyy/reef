ORIGIN = origin/main

BRANCH   = $(patsubst $(firstword $(subst /, ,$(ORIGIN)))/%,%,$(ORIGIN))

PKG_CONFIG = pkg-config

PKGS     = panelw ncursesw libmpdclient libutf8proc
PKG_INCS = `$(PKG_CONFIG) --cflags $(PKGS)`
PKG_LIBS = `$(PKG_CONFIG) --libs $(PKGS)`

PREFIX    = /usr/local

CC = cc

-include config.local.mk
