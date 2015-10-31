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
#include <stdlib.h>
#include <unistd.h>

#include "../include/state.h"

static int test_result;

#define skip(_msg) do { \
	printf("skipped: %s\n", _msg); \
	return 2; \
} while (0)
		
#define fail() do { \
	fprintf(stderr, "%s(%s:%d): FAIL\n", __func__, __FILE__, __LINE__);  \
	test_result = 0; \
	return 0; \
} while (0)

#define run_test(_func) do { \
	printf("%-24s", ""#_func); \
	test_result = test_##_func(); \
	switch (test_result) { \
	case 0: puts("failed"); exit(1); break; \
	case 1: puts("passed"); break; \
	case 2: /* skipped */ break; \
	default: puts("ERROR"); abort(); ;; \
	} \
} while (0)


int test_state_init()
{
	if (state_init(0, 0) < 0) fail();
	state_atexit();

	return 1;
}

int test_state_openlog()
{
	if (state_init(0, 0) < 0) fail();
	if (state_openlog("ntest.log") < 0) fail();
	if (state_closelog() < 0) fail();
	state_atexit();
	return 1;

}

int test_state_get_fd()
{
	return (state_get_event_fd() != 0);
}

int test_state_bind()
{
	const char *name = "user.example.status";
	if (state_init(0, 0) < 0) fail();
	if (state_bind(name) < 0) fail();
	state_atexit();
	return 1;
}

int test_state_unbind()
{
	const char *name = "user.unbind.test";

	if (state_init(0, 0) < 0) fail();
	if (state_bind(name) != 0) fail();
	if (state_unbind(".invalid") == 0) fail();
	if (state_unbind(name) != 0) fail();
	state_atexit();
	return 1;
}

int test_state_publish()
{
	const char *name = "user.example.status";
	const char *state = "I feel fine";
	if (state_init(0, 0) < 0) fail();
	if (state_bind(name) != 0) fail();
	if (state_publish(name, state, strlen(state)) < 0) fail();
	state_atexit();
	return 1;
}

int test_state_subscribe()
{
	const char *name = "user.example.status";

	if (state_init(0,0) < 0) fail();
	if (state_subscribe(name) < 0) fail();
	state_atexit();

	return 1;
}

int test_state_unsubscribe()
{
	const char *name = "user.example.test_unsubscribe";
	char *key, *value;
	ssize_t len;

	if (state_init(0,0) < 0) fail();
	if (state_unsubscribe("---invalid name---") != -1) fail();
	if (state_subscribe(name) < 0) fail();
	if (state_unsubscribe(name) < 0) fail();
	if (state_bind(name) < 0) fail();
	if (state_publish(name, "a", 1) < 0) fail();
	if ((len = state_check(&key, &value)) > 0) fail();
	state_atexit();

	return 1;
}

int test_state_check()
{
	const char *name = "user.example.status";
	const char *state = "I feel fine";
	char *key, *value;
	size_t len;

	if (state_init(0, 0) < 0) fail();
	if (state_bind(name) != 0) fail();
	if (state_subscribe(name) < 0) fail();
	if (state_publish(name, state, strlen(state)) < 0) fail();
	len = state_check(&key, &value);
	if (len <= 0) fail();
	if (strcmp(key, name) != 0) fail();
	if (strcmp(value, state) != 0) fail();
	state_atexit();

	return 1;
}

int test_state_get()
{
	const char *name = "user.example.status";
	const char *state = "OK";
	char *value;

	if (state_init(0, 0) < 0) fail();
	if (state_bind(name) != 0) fail();
	if (state_subscribe(name) < 0) fail();
	if (state_publish(name, state, strlen(state)) < 0) fail();
	if (state_get(name, &value) < 0) fail();
	if (strcmp(value, state) != 0) fail();

	if (state_get("...an invalid name....", &value) >= 0) fail();
	if (value) fail();
	state_atexit();

	return 1;
}

int test_multiple_state_changes()
{
	const char *name = "user.multiple_state_changes";
	char *key, *value;
	ssize_t len;

	if (state_init(0, 0) < 0) fail();
	if (state_bind(name) < 0) fail();
	if (state_subscribe(name) < 0) fail();
	if (state_publish(name, "a", 1) < 0) fail();
	if ((len = state_check(&key, &value)) < 1) fail();
	if (strcmp(key, name) != 0) fail();
	if (strcmp(value, "a") != 0) fail();
	/* Make sure that the internal state is cleared,
	   and that it does not return multiple events */
	if ((len = state_check(&key, &value)) != 0) {
		printf("unexpected event: %s=%s\n", key, value);
		fail();
	}

	/* Make sure another publish() causes a single event */
	if (state_publish(name, "b", 1) < 0) fail();
	if ((len = state_check(&key, &value)) <= 0) fail();
	if (strcmp(key, name) != 0) fail();
	if (strcmp(value, "b") != 0) fail();
	if ((len = state_check(&key, &value)) != 0) fail();
	if (key || value) fail();
	state_atexit();

	return 1;
}

int test_system_namespace()
{
	const char *name = "system.name";
	char *key, *value;
	ssize_t len;

	if (getuid() != 0) skip("this test must be run as root");
	if (state_init(0, 0) < 0) fail();
	if (state_bind(name) < 0) fail();
	if (state_subscribe(name) < 0) fail();
	if (state_publish(name, "hi", 2) < 0) fail();
	if ((len = state_check(&key, &value)) < 1) fail();
	if (strcmp(key, name) != 0) fail();
	if (strcmp(value, "hi") != 0) fail();
	state_atexit();

	return 1;
}

int main(int argc, char *argv[]) {
	//TODO: clear any existing state files

	(void) unlink("ntest.log");

	atexit(state_atexit);

 	/* Unit tests, looking at each function in the API */
	if (argc == 1 || strcmp(argv[1], "unit") == 0) {
		run_test(state_openlog);
		run_test(state_get_event_fd);
		run_test(state_subscribe);
		run_test(state_unsubscribe);
		run_test(state_bind);
		run_test(state_unbind);
		run_test(state_publish);
		run_test(state_check);
		run_test(state_get);
	}

 	/* Acceptance tests, looking for specific behavior */
	if (argc == 1 || strcmp(argv[1], "behavior") == 0) {
		run_test(multiple_state_changes);
		run_test(system_namespace);
	}

	puts("+OK All tests passed.");
	exit(0);
}
