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

/** \file state.h
 *
 * A state notification mechanism
 */

#include <sys/stat.h>

/**
  Initialize the state notification mechanism.

  @param ABI_version The ABI version number for compatibility. This should be set to zero.
  @param flags Reserved for future use. This should be set to zero.
  @return 0 if successful, or -1 if an error occurs.
*/
int state_init(int abi_version, int flags);

/** 
  Free all resources used by the state notification mechanism.
  This is intended to be registered with atexit(3) to run when
  the program exits.
*/
void state_atexit(void);

/**
  Acquire the ability to publish notifications about a *name*

  @param name the name to acquire

  @return 0 if successful, or -1 if an error occurs.
 */
int state_bind(const char *name);

/**
  Stop publishing information about <name>

  @param name the name to unbind

  @return 0 if successful, or -1 if an error occurs.
 */
int state_unbind(const char *name);

/**
  Subscribe to notifications about a *name*.

  @param name	The name of interest
  @return 0 if successful, or -1 if an error occurs.
*/
int state_subscribe(const char *name);

/**
  Stop subscribing to notifications about <name>

  @return 0 if successful, or -1 if an error occurs.
*/
int state_unsubscribe(const char *name);

/**
  Publish a notification about *name* and update the *state*.
  You must call state_bind() before using this function.

  @param name	The name to generate a notification for
  @param state	The new state to report
  @param len	The length of the *state* variable

  @return 0 if successful, or -1 if an error occurs.
*/
int state_publish(const char *name, const char *state, size_t len);

/** 
  Check for pending notifications, and return the current state.

  @param key Will be filled in with the published name
  @param value The current value of the state

  @return the length of the *value* string, or,
	  0 if no new notifications were available,
	  or -1 if an error occurs.
*/
ssize_t state_check(char **key, char **value);

/**
  Get the current state of a <name>.

  @param name the name of the notification
  @param value a string that will be modified to point at the current state

  @return the length of the <value> string, or -1 if an error occurred
*/
int state_get(const char *key, char **value);

/**
  Get a file descriptor that can be monitored for readability.
  When one more notifications are pending, the file descriptor will
  become ready for reading. This descriptor can be added to your
  application's event loop.

  This descriptor could be used with libdispatch by following
  this example:

	dispatch_source_t source = dispatch_source_create(
		DISPATCH_SOURCE_TYPE_READ, state_get_event_fd(),
		0, dispatch_get_main_queue());
	dispatch_source_set_event_handler(source, ^{
	    char *name, *state;
	    ssize_t len;

	    len = state_check(&name, &state);
	    if (len > 0) {
	       printf("state update: %s is now %s\n", name, state);
	    }
    	});
    	dispatch_resume(source);
	
  @return a file descriptor, or -1 if an error occurred.
*/
int state_get_event_fd(void);

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
  Open a logfile.

  @param logfile the path to the logfile
  @return 0 if successful, or -1 if an error occurs.
*/
int state_openlog(const char *logfile);

/**
  Close the logfile.

  @return 0 if successful, or -1 if an error occurs.
*/
int state_closelog(void);

#endif /* _STATE_H */
