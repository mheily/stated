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

#include "../include/state.h"
#define run_test(_func) do { \
	puts("running: "#_func); \
	if (!test_##_func()) { puts("failed"); } else { puts("passed"); } \
} while (0)

int test_state_init()
{
	return (state_init() == 0);
}

int test_state_get_fd()
{
	return (state_get_fd() != 0);
}

int test_state_bind()
{
	const char *name = "user.example.status";
	return (state_bind(name, 0644) == 0);
}

int test_state_publish()
{
	const char *name = "user.example.status";
	const char *state = "I feel fine";
	return (state_publish(name, state, strlen(state)) == 0);
}

int test_state_subscribe()
{
	const char *name = "user.example.status";
	return (state_subscribe(name) == 0);
}

int test_state_check()
{
	const char *name = "user.example.status";
	struct notify_state_s res;

	if (state_check(&res, 1) < 1) return 0;
	return (strcmp(res.ns_state, "I feel fine") == 0);
}

int main(int argc, char *argv[]) {
	run_test(state_init);
	run_test(state_get_fd);
	run_test(state_bind);
	run_test(state_publish); // KLUDGE: ensures that the name fd exists, we should test subscribe() before publish()
	run_test(state_subscribe);
	run_test(state_publish);
	run_test(state_check);
	puts("ok");
	exit(0);
}
