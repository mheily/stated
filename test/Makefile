ntest: ntest.c
	$(CC) $(CFLAGS) -g -O0 $(LDFLAGS) -L.. -I.. -lstate -o $@ ntest.c -Wl,-rpath=..

check:
	## WORKAROUND: this should be done within ntest
	find ~/.libstate/run -type f -exec rm -f {} \;
	##
	cd .. ; CFLAGS="-g -O0 -DDEBUG" $(MAKE) all
	$(MAKE) ntest
	./ntest


.PHONY: ntest check
