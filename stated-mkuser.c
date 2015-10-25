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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/state.h"

int main(int argc, char *argv[]) {
	char *path;
	uid_t uid;
	uid = getuid();
	/* TODO: It would be nice to mount a separate tmpfs for each user,
	 *   so one user cannot fill up the notification space of another user.
	 */
	if (asprintf(&path, "%s/user/%u", STATE_PREFIX, getuid()) < 0)
		exit(EX_OSERR);
	if (mkdir(path, 0755) < 0)
		exit(EX_OSERR);
	if (chown(path, getuid(), 0) < 0)
		exit(EX_OSERR);
	exit(EX_OK);
}