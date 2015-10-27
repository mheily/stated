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
#include "subscription.h"
#include "include/state.h"

static const size_t MAX_MSG_LEN = 4096; //FIXME: not very nice

static struct {
	int kqfd;
	char *userprefix;
	char *userstatedir;
	SLIST_HEAD(, subscription_s) subscriptions; /* maintained by state_subscribe() */
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
	SLIST_INIT(&libstate_data.subscriptions);
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
	subscription_t sub;
	struct kevent kev;
	int rv;

	sub = subscription_new();
	if (!sub) return -1;
	sub->sub_name = strdup(name);
	sub->sub_path = name_to_path(name);
	if (!sub->sub_name || !sub->sub_path)
		goto err_out;

	/** XXX-FIXME will not work if the publisher has not opened the file yet */
	printf("watching %s\n", sub->sub_path);
	sub->sub_fd = open(sub->sub_path, O_RDONLY);
	if (sub->sub_fd < 0) {
		log_errno("open(2) of %s", sub->sub_path);
		goto err_out;
	}

	EV_SET(&kev, sub->sub_fd, EVFILT_VNODE, EV_ADD, NOTE_ATTRIB | NOTE_WRITE, 0, 0);

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_INSERT_HEAD(&libstate_data.subscriptions, sub, entry);
	pthread_mutex_unlock(&libstate_data.mtx);

	rv = kevent(libstate_data.kqfd, &kev, 1, NULL, 0, NULL);
	if (rv < 0) {
		log_errno("kevent(2)");
		goto err_out;
	}

	return 0;

err_out:
	subscription_free(sub);
	return -1;
}

int state_publish(const char *name, const char *state, size_t len)
{
	ssize_t written;
	state_binding_t sbp, sb = NULL;
	struct {
		size_t len;
		char buf[MAX_MSG_LEN];
	} msg;
	size_t msg_size;

	if (len >= MAX_MSG_LEN) return -1;
	msg.len = len;
	memcpy(&msg.buf, state, len + 1);
	msg_size = sizeof(len) + len + 1;

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_FOREACH(sbp, &libstate_data.bound_states, entry) {
		if (strcmp(sbp->name, name) == 0) {
			sb = sbp;
			break;
		}
	}
	pthread_mutex_unlock(&libstate_data.mtx);
	if (sbp == NULL) {
		log_error("tried to publish to an unbound name: %s", name);
		return (-1);
	}

	written = pwrite(sb->fd, &msg, msg_size, 0);
	if (written < msg_size) return -1;
	printf("updated state of %s\n", sb->path);

	return 0;
}

ssize_t state_check(notify_state_t changes, size_t nchanges) {
	const struct timespec ts = { 0, 0 };
	subscription_t subp, sub = NULL;
	struct kevent kev; //FIXME: make this a bigger buffer
	ssize_t nret;

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

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_FOREACH(subp, &libstate_data.subscriptions, entry) {
		if (subp->sub_fd == kev.ident) {
			sub = subp;
			break;
		}
	}
	pthread_mutex_unlock(&libstate_data.mtx);
	if (subp == NULL) {
		log_error("recieved an event for fd %d which is not associated with a subscription", (int)kev.ident);
		return (-1);
	}

	const size_t bufsz = 4096;
	struct {
		size_t len;
		char buf[bufsz];
	} msg;

	printf("got name=%s\n", sub->sub_name);
	changes[0].ns_name = strdup(sub->sub_name);
	if (!changes[0].ns_name) {
		log_error("strdup(3)");
		return -1;
	}

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

int state_get_fd(void)
{
	return libstate_data.kqfd;
}

int state_suspend(const char *name)
{
	//STUB
	return -1;
}

int state_resume(const char *name)
{
	//STUB
	return -1;
}
