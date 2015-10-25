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
#include <syslog.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>
#include <unistd.h>

#include "include/state.h"
#include "log.h"

struct state_s {
	int kqfd;
} state;

struct options_s {
	bool daemon;
	int log_level;
	char *pkgdatadir;
	char *sysnotifydir;
	char *usernotifydir;
	size_t tmpfs_size;
} options = {
	.daemon = true,
	.log_level = 0,
	.pkgdatadir = STATE_PREFIX,
	.sysnotifydir = "/var/state/system",
	.usernotifydir = "/var/state/user",
	.tmpfs_size = 268435456,
};

void usage() {
	printf("todo: usage\n");
}

static void signal_handler(int signum) {
	(void) signum;
}

static void setup_signal_handlers()
{
	const int signals[] = {SIGHUP, SIGUSR1, SIGCHLD, SIGINT, SIGTERM, 0};
	int i;
    struct kevent kev;

    for (i = 0; signals[i] != 0; i++) {
        EV_SET(&kev, signals[i], EVFILT_SIGNAL, EV_ADD, 0, 0, &setup_signal_handlers);
        if (kevent(state.kqfd, &kev, 1, NULL, 0, NULL) < 0) abort();
        if (signal(signals[i], signal_handler) == SIG_ERR) abort();
    }
}

static void mount_data_dirs() {
	char *buf;

	if (access(STATE_PREFIX, F_OK) < 0) {
		if (errno == ENOENT) {
			if (mkdir(STATE_PREFIX, 0755) < 0) abort();
		} else {
			abort();
		}
	}
	if (asprintf(&buf, "mount -t tmpfs -o size=%zu tmpfs %s",
			options.tmpfs_size, options.sysnotifydir) < 0)
		abort();

	if (system(buf) != 0)
		abort();
	free(buf);

	if (asprintf(&buf, "mount -t tmpfs -o size=%zu tmpfs %s",
			options.tmpfs_size, options.usernotifydir) < 0)
		abort();

	if (system(buf) != 0)
		abort();
	free(buf);
}

static void umount_data_dirs() {
	char *buf;

	if (asprintf(&buf, "umount -f %s %s", options.sysnotifydir, options.usernotifydir) < 0)
		abort();

	if (system(buf) != 0)
		abort();
	free(buf);
}

static void do_shutdown() {
	umount_data_dirs();
}

static void main_loop() {
	struct kevent kev;

	log_debug("main loop");
	for (;;) {
		if (kevent(state.kqfd, NULL, 0, &kev, 1, NULL) < 1) {
			if (errno == EINTR) {
				continue;
			} else {
				log_errno("kevent");
				abort();
			}
		}
		if (kev.udata == &setup_signal_handlers) {
			switch (kev.ident) {
			case SIGHUP:
				break;
			case SIGUSR1:
				break;
			case SIGCHLD:
				break;
			case SIGINT:
			case SIGTERM:
				log_notice("caught signal %lu, exiting", kev.ident);
				do_shutdown();
				exit(0);
				break;
			default:
				log_error("caught unexpected signal");
			}
		} else {
			log_warning("spurious wakeup, no known handlers");
		}
	}
}

int main(int argc, char *argv[]) {
    if (options.daemon && daemon(0, 0) < 0) {
		fprintf(stderr, "Unable to daemonize");
		exit(EX_OSERR);
		log_open("/var/log/notifyd.log");
    } else {
        log_open("/dev/stderr");
    }

    if ((state.kqfd = kqueue()) < 0) abort();

    setup_signal_handlers();
	mount_data_dirs();
	main_loop();
	exit(EXIT_SUCCESS);
}
