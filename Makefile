# Makefile for git-generic-hooks
#
# Copyright (c) 2009 Benedikt Meurer <benedikt.meurer@googlemail.com>
# 

CC=gcc
CFLAGS=-O2 -Wall -Werror
PREFIX=/usr/local

HOOKS=	applypatch-msg commit-msg post-commit post-receive post-update \
	pre-applypatch pre-commit pre-rebase prepare-commit-msg update

all: git-generic-hook config $(HOOKS)

install: all
	install -d -m 0755 $(PREFIX)/bin
	install -c -m 0755 -s git-generic-hook $(PREFIX)/bin/git-generic-hook
	install -d -m 755 $(PREFIX)/share/git-generic-hooks/templates
	install -c -m 0644 config $(PREFIX)/share/git-generic-hooks/templates/config
	install -c -m 0644 description $(PREFIX)/share/git-generic-hooks/templates/description
	install -d -m 0755 $(PREFIX)/share/git-generic-hooks/templates/hooks
	@for hook in $(HOOKS); do \
		install -d -m 0755 "$(PREFIX)/share/git-generic-hooks/$${hook}.d"; \
		install -c -m 0755 "$${hook}" "$(PREFIX)/share/git-generic-hooks/templates/hooks/$${hook}"; \
	done
	install -d -m 0755 $(PREFIX)/share/git-generic-hooks/templates/info
	install -c -m 0644 exclude $(PREFIX)/share/git-generic-hooks/templates/info/exclude
	install -c -m 0755 post-receive.d/99gitnotify $(PREFIX)/share/git-generic-hooks/post-receive.d/99gitnotify
	install -c -m 0755 post-update.d/99updateserverinfo $(PREFIX)/share/git-generic-hooks/post-update.d/99updateserverinfo
	install -c -m 0755 update.d/10verify $(PREFIX)/share/git-generic-hooks/update.d/10verify

git-generic-hook: git-generic-hook.c
	$(CC) $(CFLAGS) -o $@ $<

config: config.in
	sed -e "s,@PREFIX@,${PREFIX},g" < $< > $@

$(HOOKS): hook.in
	sed -e "s,@PREFIX@,${PREFIX},g" -e "s,@HOOK@,${@},g" < $< > $@

clean:
	rm -f git-generic-hook
	rm -f config
	rm -f $(HOOKS)
