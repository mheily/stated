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

#ifndef SUBSCRIPTION_H_
#define SUBSCRIPTION_H_

#include "include/state.h"
#include "binding.h"

struct subscription_s {
	SLIST_ENTRY(subscription_s) entry;
	int   sub_fd;
	char *sub_name;
	char *sub_path;
};
typedef struct subscription_s * subscription_t;

static subscription_t subscription_new()
{
	subscription_t sub;

	sub = calloc(1, sizeof(*sub));
	if (!sub) return NULL;
	sub->sub_fd = -1;
	return sub;
}

static inline void subscription_free(subscription_t sub)
{
	if (sub) {
		if (sub->sub_fd >= 0) (void)close(sub->sub_fd);
		free(sub->sub_name);
		free(sub->sub_path);
		free(sub);
	}
}

#endif /* SUBSCRIPTION_H_ */
