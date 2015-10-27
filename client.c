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
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>

#include <sys/queue.h>

#include "log.h"
#include "binding.h"
#include "include/state.h"

static const size_t MAX_MSG_LEN = 4096; //FIXME: not very nice

/* Maximum number of notifications that can be subscribed to */
#define MAX_SUBSCRIPTIONS 512

static struct {
	int kqfd;
	char *userprefix;
	char *userstatedir;
	int subfd[MAX_SUBSCRIPTIONS];
	struct notify_state_s curstate[MAX_SUBSCRIPTIONS];
	SLIST_HEAD(, state_binding_s) bound_states; /* maintaned by state_bind() */
	size_t nsubfd;
	pthread_mutex_t mtx;
} libstate_data;

static int create_user_dirs() {

	/* Programs running as root use the global statedir */
	if (getuid() == 0)
		return 0;

	if (getenv("HOME") == NULL) {
		//TODO: use getpwuid to lookup the home directory
		return -1;
	}
	if (asprintf(&libstate_data.userprefix, "%s/.libstate", getenv("HOME")) < 0)
		return -1;
	if (asprintf(&libstate_data.userstatedir, "%s/run", libstate_data.userprefix) < 0)
		return -1;
	if (access(libstate_data.userprefix, F_OK) != 0) {
		if (mkdir(libstate_data.userprefix, 0700) < 0) {
			return -1;
		}
	}
	if (access(libstate_data.userstatedir, F_OK) != 0) {
		if (mkdir(libstate_data.userstatedir, 0700) < 0) {
			return -1;
		}
	}
	return 0;
}

static int name_to_id(char *name)
{
	if (name[0] == '.') {
		log_error("illegal character in name");
		return -1;
	}

	/* TODO: want to mkdir for each component in a name, e.g. foo.bar.baz becomes
	 *    foo/bar/baz, with directories created for foo and bar
	 */
	return 0;

	for (int i = 0; name[i]; i++) {
		if (name[i] == '.') {
			name[i] = '/';
		}
	}
	return 0;
}

static char *name_to_path(const char *name)
{
	char *id = NULL;
	char *path = NULL;
	bool is_user_path = false;

	const char *user_prefix = "user.";

	if (strncmp(name, user_prefix, strlen(user_prefix)) == 0) {
		id = strdup(name + 5);
		is_user_path = true;
	} else {
		id = strdup(name);
	}
	if (!id) goto err_out;

	if (name_to_id(id) < 0) goto err_out;

	if (is_user_path) {
		if (asprintf(&path, "%s/%s", libstate_data.userstatedir, id) < 0) {
			goto err_out;
		}
	} else {
		if (asprintf(&path, "%s/%s", STATE_PREFIX, id) < 0) {
			goto err_out;
		}
	}

	free(id);
	return path;

err_out:
	free(id);
	free(path);
	return NULL;
}

static int setup_logging()
{
	char *path;

	if (asprintf(&path, "%s/client.log", libstate_data.userprefix) < 0)
		return -1;
	if (log_open(path) < 0) {
		free(path);
		return -1;
	}
	free(path);

	log_debug("userprefix=%s userstatedir=%s", libstate_data.userprefix, libstate_data.userstatedir);

	return 0;
}

int state_init() {
	static bool initialized = false;

	/* FIXME: not threadsafe */
	if (initialized)
		return -1;
	libstate_data.nsubfd = 0;
	SLIST_INIT(&libstate_data.bound_states);
	memset(&libstate_data.subfd, -1, sizeof(libstate_data.subfd));
	if ((libstate_data.kqfd = kqueue()) < 0)
		return -1;
	if (create_user_dirs() < 0)
		return -1;
	if (setup_logging() < 0)
		return -1;
	pthread_mutex_init(&libstate_data.mtx, NULL);
	initialized = true;
	return 0;
}

int state_bind(const char *name, mode_t mode)
{
	state_binding_t sb = NULL;
	mode_t create_mode;

	sb = calloc(1, sizeof(*sb));
	if (!sb) goto err_out;
	sb->name = strdup(name);
	if (!sb->name) goto err_out;
	sb->path = name_to_path(name);
	if (!sb->path) goto err_out;

	if ((sb->fd = open(sb->path, O_CREAT | O_TRUNC | O_WRONLY, mode)) < 0) {
		log_errno("open(2) of %s", sb->path);
		goto err_out;
	}

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_INSERT_HEAD(&libstate_data.bound_states, sb, entry);
	pthread_mutex_unlock(&libstate_data.mtx);

	//TODO: send a global notification that the set of state bindings has changed

	return 0;

err_out:
	log_error("unable to bind to %s", name);
	state_binding_free(sb);
	return -1;
}

int state_subscribe(const char *name)
{
	struct kevent kev;
	char *path = NULL;
	int rv, fd = -1;

	/* TODO: replace this array w/ a SLIST */
	pthread_mutex_lock(&libstate_data.mtx);
	if (libstate_data.nsubfd == MAX_SUBSCRIPTIONS) {
		pthread_mutex_unlock(&libstate_data.mtx);
		goto err_out;
	}
	pthread_mutex_unlock(&libstate_data.mtx);

	if ((path = name_to_path(name)) == NULL)
		goto err_out;

	/** XXX-FIXME will not work if the publisher has not opened the file yet */
	printf("watching %s\n", path);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto err_out;

	EV_SET(&kev, fd, EVFILT_VNODE, EV_ADD, NOTE_ATTRIB | NOTE_WRITE, 0, 0);

	pthread_mutex_lock(&libstate_data.mtx);
	for (int i = 0; i <= MAX_SUBSCRIPTIONS; i++) {
		if (libstate_data.subfd[i] == -1) {
			libstate_data.subfd[i] = fd;
			libstate_data.nsubfd++;

			libstate_data.curstate[i].ns_name = strdup(name); //XXX-NO ERROR CHECKING
			libstate_data.curstate[i].ns_state = NULL;
			libstate_data.curstate[i].ns_len = 0;
			break;
		}
	}
	rv = kevent(libstate_data.kqfd, &kev, 1, NULL, 0, NULL);
	pthread_mutex_unlock(&libstate_data.mtx);

	if (rv < 0)
		goto err_out;

	return 0;

err_out:
	if (fd >= 0) close(fd);
	free(path);
	return -1;
}

int state_publish(const char *name, const char *state, size_t len)
{
	ssize_t written;
	state_binding_t sb = NULL;
	struct {
		size_t len;
		char buf[MAX_MSG_LEN];
	} msg;
	size_t msg_size;

	if (len >= MAX_MSG_LEN) return -1;
	msg.len = len;
	memcpy(&msg.buf, state, len + 1);
	msg_size = sizeof(len) + len + 1;

	SLIST_FOREACH(sb, &libstate_data.bound_states, entry) {
		if (strcmp(sb->name, name) == 0) {
			written = pwrite(sb->fd, &msg, msg_size, 0);
			if (written < msg_size) return -1;
			printf("updated state of %s\n", sb->path);
			return 0;
		}
	}

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
	if ((kev.fflags & NOTE_ATTRIB) || (kev.fflags & NOTE_WRITE)) {
		printf("fd %u written", (unsigned int) kev.ident);
	} else {
		return -1;
	}
	for (int i = 0; i <= MAX_SUBSCRIPTIONS; i++) {
		if (libstate_data.subfd[i] == kev.ident) {
			const size_t bufsz = 4096;
			struct {
				size_t len;
				char buf[bufsz];
			} msg;
			ssize_t nret;

			printf("got name=%s\n", libstate_data.curstate[i].ns_name);
			memcpy(changes, &libstate_data.curstate[i], sizeof(struct notify_state_s));
			for (;;) {
				nret = pread(kev.ident, &msg, sizeof(msg), 0);
				if (nret < 0) return -1;
				printf("read %zd bytes\n", nret);
				//FIXME: handle the case where bufsz is smaller than buf.len.
				// we should malloc a new buffer and re-read from the fd
				if (nret < msg.len || msg.len > (bufsz -1)) {
					return -1;
				}
				memset(&msg.buf[msg.len + 1], 0, 1); // ensure NUL termination
				free(changes->ns_state);
				changes->ns_state = strdup(msg.buf);
				changes->ns_len = msg.len;
				printf("new state: %zu bytes, value: %s\n", changes->ns_len, changes->ns_state);
				break;
			}
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
		DISPATCH_SOURCE_TYPE_READ, state_get_fd(),
		0, dispatch_get_main_queue());
	dispatch_source_set_event_handler(source, ^{
	    char *name, *state;
	    ssize_t count;
	    notify_state_t changes;

	    count = state_check(&changes, 1);
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
  shown in the documentation for state_get_fd().

  You must include <dispatch/dispatch.h> and compile with -fblocks to have
  access to this function.

  @param name the name of the notification to wait for
  @param queue the dispatch queue to run the block on
  @param block the block of code to be executed
*/
#if defined(__block) && defined(dispatch_queue_t)
void state_dispatch(char *name, dispatch_queue_t queue, void (^block)(void));
#endif

/**
  Execute a callback function when one or more notifications are pending.

  This is equivalent to state_dispatch(), but without using blocks.
*/
#ifdef dispatch_queue_t
void state_dispatch_f(char *name, dispatch_queue_t queue, void *context, void (*func)(void *));
#endif

/**
  Disable notifications related to *name*.

  @return 0 if successful, or -1 if an error occurs.
*/
int state_suspend(const char *name)
{
	return -1;
}

/**
  Resume notifications related to *name*

  @return 0 if successful, or -1 if an error occurs.
*/
int state_resume(const char *name)
{
	return -1;
}
