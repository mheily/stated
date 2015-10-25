ntest: ntest.c
	$(CC) $(CFLAGS) $(LDFLAGS) -L.. -I.. -lstate -o $@ ntest.c -Wl,-rpath=..

check:
	cd .. ; CFLAGS="-g -O0 -DDEBUG" $(MAKE) all
	$(MAKE) ntest
	./ntest


.PHONY: ntest check
