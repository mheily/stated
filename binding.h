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

#ifndef BINDING_H_
#define BINDING_H_

struct state_binding_s {
	SLIST_ENTRY(state_binding_s) entry;
	int fd;
	char *name;
	char *path;
	size_t maxlen; /* Maximum amount of state data that can be published */
};
typedef struct state_binding_s * state_binding_t;

static inline void state_binding_free(state_binding_t sb)
{
	if (sb) {
		if (sb->fd >= 0) {
			(void) close(sb->fd);
		}
		free(sb->name);
		free(sb->path);
		free(sb);
	}
}

#endif /* BINDING_H_ */
