#
# Copyright (c) 2015 Mark Heily <mark@heily.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

PACKAGE_VERSION ?= 0.1.0
MAJOR_VERSION != echo $(PACKAGE_VERSION) | cut -f1 -d.
MINOR_VERSION != echo $(PACKAGE_VERSION) | cut -f2 -d.
PATCH_VERSION != echo $(PACKAGE_VERSION) | cut -f3 -d.

PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
BINDIR ?= $(PREFIX)/bin
SBINDIR ?= $(PREFIX)/sbin
LIBEXECDIR ?= $(PREFIX)/libexec

STATEDIR != test -d /run && echo /run || echo /var/state
USE_KQUEUE != test -e /usr/include/sys/event.h && echo 1 || echo 0
USE_INOTIFY != test -e /usr/include/sys/inotify.h && echo 1 || echo 0

DEBUGFLAGS=-g -O0 -DDEBUG

all: stated libstate.so

stated: platform.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c log.c

libstate.so: platform.h
	$(CC) -fPIC -shared $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) -o $@ client.c log.c
	
stated-debug:
	CFLAGS="$(DEBUGFLAGS)" $(MAKE) stated

platform.h: platform.h.in
	cat platform.h.in | sed "\
		s,@@PACKAGE_VERSION@@,$(PACKAGE_VERSION),; \
		s,@@MAJOR_VERSION@@,$(MAJOR_VERSION),; \
		s,@@MINOR_VERSION@@,$(MINOR_VERSION),; \
		s,@@PATCH_VERSION@@,$(PATCH_VERSION),; \
		s,@@STATE_PREFIX@@,$(STATEDIR),; \
		s,@@USE_KQUEUE@@,$(USE_KQUEUE),; \
		s,@@USE_INOTIFY@@,$(USE_INOTIFY),; \
	" > platform.h

clean:
	rm -f *.o platform.h
	rm -f libstate.so stated
	
check:
	cd test && $(MAKE) check

install:
	test -e stated || $(MAKE) stated
	install -m 755 -o 0 -g 0 stated $(SBINDIR)
	install -m 644 -o 0 -g 0 libstate.so $(LIBDIR)/libstate.so.$(PACKAGE_VERSION)
	ln -sf libstate.so.$(PACKAGE_VERSION) $(LIBDIR)/libstate.so.$(MAJOR_VERSION)
	ln -sf libstate.so.$(PACKAGE_VERSION) $(LIBDIR)/libstate.so.$(MAJOR_VERSION).$(MINOR_VERSION)
	test `uname` = "FreeBSD" && install -m 755 -o 0 -g 0 rc.FreeBSD /usr/local/etc/rc.d/stated || true

.PHONY: all clean stated libstate.so
