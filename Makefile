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

all: notifyd notifyd-mkuser libstate.so

notifyd:
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c log.c

notifyd-mkuser:
	$(CC) -static $(CFLAGS) $(LDFLAGS) -o $@ notifyd-mkuser.c

libstate.so:
	$(CC) -fPIC -shared $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) -o $@ client.c
	
notifyd-debug:
	CFLAGS="$(DEBUGFLAGS)" $(MAKE) launchd

clean:
	rm -f *.o
	rm -f notifyd notifyd-mkuser
	
install:
	test -e notifyd || $(MAKE) notifyd
	install -m 755 -o 0 -g 0 notifyd $(SBINDIR)
	install -m 4755 -o 0 -g 0 notifyd-mkuser $(LIBEXECDIR)
	install -m 755 -o 0 -g 0 rc.FreeBSD /usr/local/etc/rc.d/notifyd
	install -d -m 755 -o 0 -g 0 /var/run/notifyd /var/run/notifyd/system /var/run/notifyd/user

.PHONY: all clean notifyd notifyd-mkuser libstate.so
