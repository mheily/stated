/*
 * binding.h
 *
 *  Created on: Oct 24, 2015
 *      Author: mark
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
