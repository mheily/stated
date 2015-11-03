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

include Makefile.inc

SUBDIRS=	statestat statectl

# Files to include in the tarball
DISTFILES = *.[ch] *.in rc.* README.md Makefile include doc

DEBUGFLAGS=-g -O0 -DDEBUG

all: stated libstate.so libstate.a
	for dir in $(SUBDIRS) ; do cd $$dir && $(MAKE) && cd .. ; done

stated: platform.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c log.c

libstate.a: client.c log.c platform.h
	$(CC) -static -c client.c log.c
	ar rcs libstate.a client.o log.o
	
libstate.so: client.c log.c platform.h
	$(CC) -fPIC -shared $(CFLAGS) $(DEBUGFLAGS) $(LDFLAGS) -o $@ client.c log.c
	
stated-debug:
	CFLAGS="$(DEBUGFLAGS)" $(MAKE) stated

state.3.gz:
	cd doc && $(MAKE) clean all
	cp doc/doxygen/man/man3/state.h.3 state.3
	gzip state.3

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
	cd doc ; $(MAKE) clean
	rm -f *.o platform.h state.3.gz
	rm -f libstate.so stated
	for dir in $(SUBDIRS) ; do cd $$dir && $(MAKE) clean && cd .. ; done
	
check:
	cd test && $(MAKE) check

dist:
	$(MAKE) clean 
	mkdir $(PACKAGE_NAME)-$(PACKAGE_VERSION)
	cp -R $(DISTFILES) $(PACKAGE_NAME)-$(PACKAGE_VERSION)
	find $(PACKAGE_NAME)-$(PACKAGE_VERSION) -name '.gitignore' -exec rm {} \;
	tar cvf $(PACKAGE_NAME)-$(PACKAGE_VERSION).tar.gz $(PACKAGE_NAME)-$(PACKAGE_VERSION)
	rm -rf $(PACKAGE_NAME)-$(PACKAGE_VERSION)

install: state.3.gz
	test -e stated || $(MAKE) stated
	install -d -m 755 $$DESTDIR$(SBINDIR) $$DESTDIR$(LIBDIR) $$DESTDIR$(MANDIR)/man3 $$DESTDIR/usr/local/etc/rc.d/
	install -s -m 755 stated $$DESTDIR$(SBINDIR)
	install -s -m 644 libstate.so $$DESTDIR$(LIBDIR)/libstate.so.$(PACKAGE_VERSION)
	ln -sf libstate.so.$(PACKAGE_VERSION) $$DESTDIR$(LIBDIR)/libstate.so
	ln -sf libstate.so.$(PACKAGE_VERSION) $$DESTDIR$(LIBDIR)/libstate.so.$(MAJOR_VERSION)
	ln -sf libstate.so.$(PACKAGE_VERSION) $$DESTDIR$(LIBDIR)/libstate.so.$(MAJOR_VERSION).$(MINOR_VERSION)
	install -m 644 state.3.gz $$DESTDIR$(MANDIR)/man3
	test `uname` = "FreeBSD" && install -m 755 rc.FreeBSD $$DESTDIR/usr/local/etc/rc.d/stated || true
	for dir in $(SUBDIRS) ; do cd $$dir && $(MAKE) ; done
	
.PHONY: all clean stated libstate.so
