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

#include <string.h>
#include <stdio.h>


#include "include/notify.h"

static char *name_to_path(const char *name)
{
	char *path = NULL;

	const char *user_prefix = "user.";
	//.sysnotifydir = "/var/run/notifyd/system",
	//.usernotifydir = "/var/run/notifyd/user",

	if (strncmp(name, user_prefix, strlen(user_prefix)) == 0) {
		puts("user");
	}
	return path;
}

int notify_post(const char *name, void *state, size_t len)
{
	char *path;

	if ((path = name_to_path(name)) == NULL)
		return -1;

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
ssize_t notify_check(notify_state_t *changes, size_t nchanges)
{
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
int notify_get_fd(void)
{
	return -1;
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
