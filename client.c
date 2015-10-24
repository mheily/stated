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

#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>

#include <sys/queue.h>

#include "include/notify.h"

/* Maximum number of notifications that can be subscribed to */
#define MAX_SUBSCRIPTIONS 512

static struct {
	int kqfd;
	int subfd[MAX_SUBSCRIPTIONS];
	struct notify_state_s curstate[MAX_SUBSCRIPTIONS];
	size_t nsubfd;
} libstate_data;

static char *name_to_path(const char *name)
{
	char *path = NULL;

	const char *user_prefix = "user.";
	//.sysnotifydir = "/var/run/notifyd/system",
	//.usernotifydir = "/var/run/notifyd/user",

	if (strncmp(name, user_prefix, strlen(user_prefix)) == 0) {
		if (asprintf(&path, "/var/run/notifyd/user/%s", name + 5) < 0)
			return (NULL);
		for (int i = 0; path[i]; i++) {
			if (path[i] == '.') {
				path[i] = '/';
				break;
			}
		}
	} else {
		abort();//STUB
	}
	return path;
}

int state_init() {
	static bool initialized = false;

	/* FIXME: not threadsafe */
	if (initialized)
		return -1;
	libstate_data.nsubfd = 0;
	memset(&libstate_data.subfd, -1, sizeof(libstate_data.subfd));
	if ((libstate_data.kqfd = kqueue()) < 0)
		return -1;
	initialized = true;
	return 0;
}

int notify_post(const char *name, const char *state, size_t len)
{
	char *path = NULL;
	int fd = -1;

	if ((path = name_to_path(name)) == NULL)
		goto err_out;
	if ((fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0)
		goto err_out;
	if (write(fd, state, len) < len)
		goto err_out;
	if (close(fd) < 0)
		goto err_out;
	printf("updated state of %s\n", path);
	free(path);
	return 0;

err_out:
	if (fd >= 0) close(fd);
	free(path);
	return -1;
}

int state_subscribe(const char *name)
{
	struct kevent kev;
	char *path = NULL;
	int fd = -1;

	if (libstate_data.nsubfd == MAX_SUBSCRIPTIONS)
		goto err_out;
	if ((path = name_to_path(name)) == NULL)
		goto err_out;

	/** XXX-FIXME will not work if the publisher has not opened the file yet */
	printf("watching %s\n", path);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto err_out;

	for (int i = 0; i <= MAX_SUBSCRIPTIONS; i++) {
		if (libstate_data.subfd[i] == -1) {
			libstate_data.subfd[i] = fd;
			libstate_data.nsubfd++;

			libstate_data.curstate[i].ns_name = path;
			libstate_data.curstate[i].ns_state = NULL;
			libstate_data.curstate[i].ns_len = 0;
			break;
		}
	}

	EV_SET(&kev, fd, EVFILT_VNODE, EV_ADD, NOTE_ATTRIB | NOTE_WRITE, 0, 0);
	if (kevent(libstate_data.kqfd, &kev, 1, NULL, 0, NULL) < 0)
		goto err_out;

	return 0;

err_out:
	if (fd >= 0) close(fd);
	free(path);
	return -1;
}

/** 
  Check for pending notifications, and return the current state.

  @param changes A buffer to write the notifications to
  @param nchanges The number of changes that can be stored in the buffer

  @return the number of notifications retrieved,
	  0 if no notifications are available,
	  or -1 if an error occurs.
*/
ssize_t state_check(notify_state_t changes, size_t nchanges) {
	const struct timespec ts = { 0, 0 };
	struct kevent kev; //FIXME: make this a bigger buffer
	int nret;

	nret = kevent(libstate_data.kqfd, NULL, 0, &kev, 1, &ts);
	if (nret < 0)
		return -1;
	if (nret == 0)
		return 0;
	if (kev.fflags & NOTE_ATTRIB) {
		printf("fd %u written", (unsigned int) kev.ident);
	} else {
		return -1;
	}
	for (int i = 0; i <= MAX_SUBSCRIPTIONS; i++) {
		if (libstate_data.subfd[i] == kev.ident) {
			printf("got name=%s\n", libstate_data.curstate[i].ns_name);
			memcpy(changes, &libstate_data.curstate[i], sizeof(struct notify_state_s));
			puts("TODO - read the current state");
			return 1;
		}
	}
	return -1;
}

/**
  Get a file descriptor that can be monitored for readability.
  When one more notifications are pending, the file descriptor will
  become ready for reading. This descriptor can be added to your
  application's event loop.

  This descriptor could be used with libdispatch by following
  this example:

	dispatch_source_t source = dispatch_source_create(
		DISPATCH_SOURCE_TYPE_READ, notify_get_fd(),
		0, dispatch_get_main_queue());
	dispatch_source_set_event_handler(source, ^{
	    char *name, *state;
	    ssize_t count;
	    notify_state_t changes;

	    count = notify_check(&changes, 1);
	    if (count == 1 && strcmp(changes.ns_name, "foo") == 0) {
	       printf("the current state of foo is %s\n", changes.ns_state);
	    }
    	});
    	dispatch_resume(source);
	
  @return a file descriptor, or -1 if an error occurred.
*/
int state_get_fd(void)
{
	return libstate_data.kqfd;
}

/**
  Submit a block to be executed when one or more notifications are pending.

  This is basically a convenience function that implements the example code
  shown in the documentation for notify_get_fd(). 

  You must include <dispatch/dispatch.h> and compile with -fblocks to have
  access to this function.

  @param name the name of the notification to wait for
  @param queue the dispatch queue to run the block on
  @param block the block of code to be executed
*/
#if defined(__block) && defined(dispatch_queue_t)
void notify_dispatch(char *name, dispatch_queue_t queue, void (^block)(void));
#endif

/**
  Execute a callback function when one or more notifications are pending.

  This is equivalent to notify_dispatch(), but without using blocks.
*/
#ifdef dispatch_queue_t
void notify_dispatch_f(char *name, dispatch_queue_t queue, void *context, void (*func)(void *));
#endif

/**
  Disable notifications related to *name*.

  @return 0 if successful, or -1 if an error occurs.
*/
int notify_suspend(const char *name)
{
	return -1;
}

/**
  Resume notifications related to *name*

  @return 0 if successful, or -1 if an error occurs.
*/
int notify_resume(const char *name)
{
	return -1;
}
