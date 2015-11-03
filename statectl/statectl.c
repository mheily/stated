/*
 * Copyright (c) 2015 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/state.h"

static void usage()
{
	puts("TODO: print usage");
}

static void get_state(const char *key)
{
	char *value;
	ssize_t len;

	if (state_subscribe(key) < 0) {
		printf("");
		exit(EX_DATAERR);
	}
	len = state_get(key, &value);
	if (len < 0) {
		printf("");
		exit(EX_DATAERR);
	}
	printf("%s", value);
}

static void set_state(const char *key, const char *value)
{
	if (state_bind(key) < 0) {
		puts("ERROR: unable to bind to key");
		exit(EX_DATAERR);
	}
	if (state_publish(key, value, strlen(value)) < 0) {
		puts("ERROR: unable to publish new value");
		exit(EX_DATAERR);
	}
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		usage();
		exit(EX_USAGE);
	}
	state_init(0,0);
	if (strcmp(argv[1], "get") == 0) {
		get_state(argv[2]);
	} else if (strcmp(argv[1], "set") == 0) {
		if (argc < 4) {
			usage();
			exit(EX_USAGE);
		}
		set_state(argv[2], argv[3]);
	} else {
		usage();
		exit(EX_USAGE);
	}
	exit(EXIT_SUCCESS);
}
