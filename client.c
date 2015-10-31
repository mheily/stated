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
#include "platform.h"
#include "subscription.h"
#include "include/state.h"

static struct {
	int kqfd;
	char *userprefix;
	char *userstatedir;
	SLIST_HEAD(, subscription_s) subscriptions;
	SLIST_HEAD(, state_binding_s) bindings;
	pthread_mutex_t mtx;
	bool initialized;
} libstate_data;

static int create_user_dirs(void)
{
	if (getenv("HOME") == NULL) {
		//TODO: use getpwuid to lookup the home directory
		return -1;
	}
	if (asprintf(&libstate_data.userprefix, "%s/.libstate", getenv("HOME"))
			< 0)
		return -1;
	if (asprintf(&libstate_data.userstatedir, "%s/run",
			libstate_data.userprefix) < 0)
		return -1;
	if (access(libstate_data.userprefix, F_OK) != 0) {
		if (mkdir(libstate_data.userprefix, 0700) < 0)
			return -1;
	}
	if (access(libstate_data.userstatedir, F_OK) != 0) {
		if (mkdir(libstate_data.userstatedir, 0700) < 0)
			return -1;
	}
	return 0;
}

static int validate_name(char *name)
{
	if (name[0] == '.') {
		log_error("illegal character in name");
		return -1;
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
	if (!id) {
		log_errno("strdup(3)");
		goto err_out;
	}

	if (validate_name(id) < 0)
		goto err_out;

	if (is_user_path) {
		if (asprintf(&path, "%s/%s", libstate_data.userstatedir, id)
				< 0) {
			goto err_out;
		}
	} else {
		if (asprintf(&path, "%s/%s", STATE_PREFIX, id) < 0) {
			goto err_out;
		}
	}

	free(id);
	return path;

	err_out: free(id);
	free(path);
	return NULL;
}

static state_binding_t state_binding_lookup(const char *name)
{
	state_binding_t sbp;

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_FOREACH(sbp, &libstate_data.bindings, entry)
	{
		if (strcmp(sbp->name, name) == 0) {
			pthread_mutex_unlock(&libstate_data.mtx);
			return sbp;
		}
	}
	pthread_mutex_unlock(&libstate_data.mtx);
	return NULL;
}

static subscription_t subscription_lookup(const char *name)
{
	subscription_t sub;

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_FOREACH(sub, &libstate_data.subscriptions, entry)
	{
		if (strcmp(sub->sub_name, name) == 0) {
			pthread_mutex_unlock(&libstate_data.mtx);
			return sub;
		}
	}
	pthread_mutex_unlock(&libstate_data.mtx);
	return NULL;
}

/* Update the current state of a subscription */
static int subscription_update(subscription_t sub)
{
	struct stat sb;
	size_t newlen;
	ssize_t nret;

	retry: if (fstat(sub->sub_fd, &sb) < 0) {
		log_errno("fstat");
		return -1;
	}
	if (sb.st_size == SIZE_MAX)
		return -1;
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
		log_warning("state file %s is invalid; too short",
				sub->sub_path);
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


int state_init(int abi_version, int flags)
{
	/* These are not used yet */
	(void) abi_version;
	(void) flags;

	/* FIXME: not threadsafe */
	if (libstate_data.initialized)
		return -1;
	SLIST_INIT(&libstate_data.bindings);
	SLIST_INIT(&libstate_data.subscriptions);
	if ((libstate_data.kqfd = kqueue()) < 0)
		return -1;
	if (create_user_dirs() < 0)
		return -1;
	pthread_mutex_init(&libstate_data.mtx, NULL);
	libstate_data.initialized = true;
	return 0;
}

int state_openlog(const char *path)
{
	return log_open(path);
}

int state_closelog(void)
{
	return log_close();
}

void state_atexit(void)
{
	state_binding_t sbp, sbp_tmp;
	subscription_t sub, sub_tmp;

	if (!libstate_data.initialized)
		return;

	free(libstate_data.userprefix);
	free(libstate_data.userstatedir);
	(void) close(libstate_data.kqfd);
	SLIST_FOREACH_SAFE(sbp, &libstate_data.bindings, entry, sbp_tmp) {
		SLIST_REMOVE(&libstate_data.bindings, sbp, state_binding_s,
				entry);
		state_binding_free(sbp);
	}
	SLIST_FOREACH_SAFE(sub, &libstate_data.subscriptions, entry, sub_tmp) {
		SLIST_REMOVE(&libstate_data.subscriptions, sub, subscription_s,
				entry);
		subscription_free(sub);
	}
	log_debug("shutting down");
	(void) pthread_mutex_destroy(&libstate_data.mtx);
	(void) log_close();
	libstate_data.initialized = false;
}

int state_bind(const char *name)
{
	state_binding_t sb = NULL;

	sb = calloc(1, sizeof(*sb));
	if (!sb)
		goto err_out;
	sb->name = strdup(name);
	if (!sb->name)
		goto err_out;
	sb->path = name_to_path(name);
	if (!sb->path)
		goto err_out;

	if ((sb->fd = open(sb->path, O_CREAT | O_TRUNC | O_WRONLY, 0644)) < 0) {
		log_errno("open(2) of %s", sb->path);
		goto err_out;
	}

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_INSERT_HEAD(&libstate_data.bindings, sb, entry);
	pthread_mutex_unlock(&libstate_data.mtx);

	return 0;

	err_out:
	log_error("unable to bind to %s", name);
	state_binding_free(sb);
	return -1;
}

int state_unbind(const char *name)
{
	state_binding_t sb;

	sb = state_binding_lookup(name);
	if (sb == NULL) {
		log_error("name not bound: %s", name);
		return (-1);
	}
	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_REMOVE(&libstate_data.bindings, sb, state_binding_s, entry);
	pthread_mutex_lock(&libstate_data.mtx);
	state_binding_free(sb);
	log_debug("unbound %s", name);

	return 0;
}

int state_subscribe(const char *name)
{
	subscription_t sub;
	struct kevent kev;
	int rv;

	sub = subscription_new();
	if (!sub)
		return -1;
	sub->sub_name = strdup(name);
	sub->sub_path = name_to_path(name);
	if (!sub->sub_name || !sub->sub_path)
		goto err_out;

	sub->sub_fd = open(sub->sub_path, O_CREAT | O_RDONLY, 0644);
	if (sub->sub_fd < 0) {
		log_errno("open(2) of %s", sub->sub_path);
		goto err_out;
	}

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_INSERT_HEAD(&libstate_data.subscriptions, sub, entry);
	pthread_mutex_unlock(&libstate_data.mtx);

	EV_SET(&kev, sub->sub_fd, EVFILT_VNODE, EV_ADD | EV_CLEAR,
			NOTE_WRITE | NOTE_DELETE, 0, 0);
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

int state_unsubscribe(const char *name)
{
	subscription_t sub;
	struct kevent kev;
	int rv;

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_FOREACH(sub, &libstate_data.subscriptions, entry)
	{
		if (strcmp(sub->sub_name, name) == 0) {
			SLIST_REMOVE(&libstate_data.subscriptions, sub, subscription_s, entry);
			pthread_mutex_unlock(&libstate_data.mtx);
			subscription_free(sub);
			return 0;
		}
	}
	pthread_mutex_unlock(&libstate_data.mtx);

	return -1;
}


int state_publish(const char *name, const char *state, size_t len)
{
	const char nul = '\0';
	ssize_t written;
	state_binding_t sb;
	struct iovec iov[3];

	iov[0].iov_base = &len;
	iov[0].iov_len = sizeof(len);
	iov[1].iov_base = (char *) state;
	iov[1].iov_len = len;
	iov[2].iov_base = (char *) &nul;
	iov[2].iov_len = 1;

	sb = state_binding_lookup(name);
	if (sb == NULL) {
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

int state_get(const char *key, char **value)
{
	subscription_t sub;

	if ((sub = subscription_lookup(key)) == NULL) {
		log_debug("subscription lookup for `%s' failed", key);
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

ssize_t state_check(char **key, char **value)
{
	const struct timespec ts =
	{ 0, 0 };
	subscription_t subp, sub = NULL;
	struct kevent kev;
	ssize_t nret;

	if (key == NULL || value == NULL)
		return -1;

	nret = kevent(libstate_data.kqfd, NULL, 0, &kev, 1, &ts);
	if (nret < 0) {
		log_errno("kevent(2)");
		goto err_out;
	}
	if (nret == 0) {
		log_debug("no events were pending");
		*key = *value = NULL;
		return 0;
	}

	pthread_mutex_lock(&libstate_data.mtx);
	SLIST_FOREACH(subp, &libstate_data.subscriptions, entry)
	{
		if (subp->sub_fd == kev.ident) {
			sub = subp;
			break;
		}
	}
	pthread_mutex_unlock(&libstate_data.mtx);
	if (subp == NULL) {
		log_error(
				"recieved an event for fd %d which is not associated with a subscription",
				(int )kev.ident);
		goto err_out;
	}

	if (kev.fflags & NOTE_WRITE) {
		log_debug("fd %u written", (unsigned int) kev.ident);

		if (subscription_update(sub) < 0) {
			log_error("failed to update the state of %s", sub->sub_name);
			goto err_out;
		}
	}
	if (kev.fflags & NOTE_DELETE) {
		log_debug("state file %s was deleted; removing subscription", sub->sub_path);
		pthread_mutex_lock(&libstate_data.mtx);
		SLIST_REMOVE(&libstate_data.subscriptions, sub, subscription_s,
				entry);
		pthread_mutex_unlock(&libstate_data.mtx);
		subscription_free(sub);
	}

	*key = sub->sub_name;
	*value = sub->sub_buf + sizeof(size_t);
	return sub->sub_buflen;

err_out:
	*key = NULL;
	*value = NULL;
	return -1;
}

int state_get_event_fd(void)
{
	return libstate_data.kqfd;
}
