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
#include <sys/uio.h>
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

	sub->sub_fd = open(sub->sub_path, O_CREAT | O_RDONLY);
	if (sub->sub_fd < 0) {
		log_errno("open(2) of %s", sub->sub_path);
		goto err_out;
	}

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_INSERT_HEAD(&libstate_data.subscriptions, sub, entry);
	pthread_mutex_unlock(&libstate_data.mtx);

	EV_SET(&kev, sub->sub_fd, EVFILT_VNODE, EV_ADD, NOTE_ATTRIB | NOTE_WRITE, 0, 0);
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
	const char nul = '\0';
	ssize_t written;
	state_binding_t sbp, sb = NULL;
	struct iovec iov[3];

	iov[0].iov_base = &len;
	iov[0].iov_len = sizeof(len);
	iov[1].iov_base = (char *) state;
	iov[1].iov_len = len;
	iov[2].iov_base = (char *) &nul;
	iov[2].iov_len = 1;

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

	written = pwritev(sb->fd, iov, 3, 0);
	if (written < (len + sizeof(len) + 1)) {
		if (written < 0) {
			log_errno("pwritev(3)");
		} else {
			log_error("short write");
		}
		return -1;
	}

	return 0;
}

/* Update the current state of a subscription */
static int subscription_update(subscription_t sub)
{
	struct stat sb;
	size_t      newlen;
	ssize_t     nret;

retry:
	if (fstat(sub->sub_fd, &sb) < 0) {
		log_errno("fstat");
		return -1;
	}
	if (sb.st_size == SIZE_MAX) return -1;
	if (sub->sub_bufsz <= sb.st_size) {
		char *newbuf = realloc(sub->sub_buf, sb.st_size + 1);
		if (newbuf == NULL) {
			log_errno("realloc");
			return -1;
		}
		sub->sub_buf = newbuf;
		sub->sub_bufsz = sb.st_size + 1;
	}
	nret = pread(sub->sub_fd, sub->sub_buf, sb.st_size, 0);
	if (nret < 0) {
		log_errno("pread(2)");
		return -1;
	}
	if (nret < sizeof(size_t)) {
		log_warning("state file %s is invalid; too short", sub->sub_path);
		return -1;
	}
	memcpy(&newlen, sub->sub_buf, sizeof(newlen));
	if (newlen >= nret) {
		/* FIXME: DoS risk, could loop forever with a malicious statefile */
		log_debug("size of statefile grew; will re-read it");
		goto retry;
	}
	sub->sub_buflen = newlen;
	return 0;
}

int	state_get(const char *key, char **value)
{
	subscription_t subp, sub = NULL;

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_FOREACH(subp, &libstate_data.subscriptions, entry) {
		if (strcmp(subp->sub_name, key) == 0) {
			sub = subp;
			break;
		}
	}
	pthread_mutex_unlock(&libstate_data.mtx);
	if (sub == NULL) {
		log_debug("no entry for lookup");
		*value = NULL;
		return -1;
	}
	if (subscription_update(sub) < 0) {
		log_debug("failed to update subscription");
		*value = NULL;
		return -1;
	}

	*value = sub->sub_buf + sizeof(size_t);
	return sub->sub_buflen;
}

ssize_t state_check(char **key, char **value) {
	const struct timespec ts = { 0, 0 };
	subscription_t subp, sub = NULL;
	struct kevent kev;
	ssize_t nret;

	if (key == NULL || value == NULL) return -1;
	*value = NULL;

	nret = kevent(libstate_data.kqfd, NULL, 0, &kev, 1, &ts);
	if (nret < 0)
		return -1;
	if (nret == 0)
		return 0;
	if ((kev.fflags & NOTE_ATTRIB) || (kev.fflags & NOTE_WRITE)) {
		log_debug("fd %u written", (unsigned int) kev.ident);
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

	if (subscription_update(sub) < 0) {
		log_error("failed to update the state of %s", sub->sub_name);
		return (-1);
	}

	*key = sub->sub_name;
	*value = sub->sub_buf + sizeof(size_t);
	return sub->sub_buflen;
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
