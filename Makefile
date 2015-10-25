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

PREFIX?=/usr/local
BINDIR:=$(PREFIX)/bin
SBINDIR:=$(PREFIX)/sbin
LIBEXECDIR:=$(PREFIX)/libexec
DEBUGFLAGS=-g -O0 -DDEBUG

all: stated stated-mkuser libstate.so

stated:
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c log.c

stated-mkuser:
	$(CC) -static $(CFLAGS) $(LDFLAGS) -o $@ stated-mkuser.c

libstate.so:
	$(CC) -fPIC -shared $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) -o $@ client.c
	
stated-debug:
	CFLAGS="$(DEBUGFLAGS)" $(MAKE) launchd

clean:
	rm -f *.o
	rm -f stated stated-mkuser
	
check:
	cd test && $(MAKE) check

install:
	test -e stated || $(MAKE) stated
	install -m 755 -o 0 -g 0 stated $(SBINDIR)
	install -m 4755 -o 0 -g 0 stated-mkuser $(LIBEXECDIR)
	install -m 755 -o 0 -g 0 rc.FreeBSD /usr/local/etc/rc.d/stated
	install -d -m 755 -o 0 -g 0 /var/state /var/state/system /var/state/user

.PHONY: all clean stated stated-mkuser libstate.so
