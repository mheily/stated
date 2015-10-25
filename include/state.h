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

#ifndef _STATE_H_
#define _STATE_H_

#include <sys/stat.h>

#define STATE_PREFIX "/var/state"

/** A notification about a state change. */
struct notify_state_s {
  char   *ns_name;	
  char   *ns_state;	/* The current state */
  size_t  ns_len;	/* The amount of data stored in ns_state */
};
typedef struct notify_state_s * notify_state_t;

/** 
  Initialize the state notification mechanism.

  @return 0 if successful, or -1 if an error occurs.
*/
int state_init();

/**
  Acquire the ability to publish notifications about a *name*

  @param name the name to acquire
  @param mode who can subscribe (owner, group, and/or everybody)

  @return 0 if successful, or -1 if an error occurs.
 */
int state_bind(const char *name, mode_t mode);

/**
  Subscribe to notifications about a *name*.

  @param name	The name of interest
  @return 0 if successful, or -1 if an error occurs.
*/
int state_subscribe(const char *name);

/**
  Publish a notification about *name* and update the *state*.

  @param name	The name to generate a notification for
  @param state	The new state to report
  @param len	The length of the *state* variable

  @return 0 if successful, or -1 if an error occurs.
*/
int state_publish(const char *name, const char *state, size_t len);

/** 
  Check for pending notifications, and return the current state.

  @param changes A buffer to write the notifications to
  @param nchanges The number of changes that can be stored in the buffer

  @return the number of notifications retrieved,
	  0 if no notifications are available,
	  or -1 if an error occurs.
*/
ssize_t state_check(notify_state_t changes, size_t nchanges);

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
int state_get_fd(void);

/**
  Submit a block to be executed when one or more state change notifications are pending.

  This is basically a convenience function that implements the example code
  shown in the documentation for notify_get_fd(). 

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
int state_suspend(const char *name);

/**
  Resume notifications related to *name*

  @return 0 if successful, or -1 if an error occurs.
*/
int state_resume(const char *name);

#endif /* _STATE_H */