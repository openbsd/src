/*	$OpenBSD: name2id.c,v 1.1 2005/06/13 21:16:18 henning Exp $ */

/*
 * Copyright (c) 2004, 2005 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <net/route.h>

#include <stdlib.h>
#include <string.h>

#include "bgpd.h"

#define	IDVAL_MAX	50000

struct rt_label {
	TAILQ_ENTRY(rt_label)	entry;
	char			name[RTLABEL_LEN];
	u_int16_t		id;
	int			ref;
};

TAILQ_HEAD(rt_labels, rt_label)	rt_labels = TAILQ_HEAD_INITIALIZER(rt_labels);


u_int16_t
rtlabel_name2id(char *name)
{
	struct rt_label		*label, *p = NULL;
	u_int16_t		 new_id = 1;

	if (!name[0])
		return (0);

	TAILQ_FOREACH(label, &rt_labels, entry)
		if (strcmp(name, label->name) == 0) {
			label->ref++;
			return (label->id);
		}

	/*
	 * to avoid fragmentation, we do a linear search from the beginning
	 * and take the first free slot we find. if there is none or the list
	 * is empty, append a new entry at the end.
	 */

	if (!TAILQ_EMPTY(&rt_labels))
		for (p = TAILQ_FIRST(&rt_labels); p != NULL &&
		    p->id == new_id; p = TAILQ_NEXT(p, entry))
			new_id = p->id + 1;

	if (new_id > IDVAL_MAX)
		return (0);

	if ((label = calloc(1, sizeof(struct rt_label))) == NULL)
		return (0);
	strlcpy(label->name, name, sizeof(label->name));
	label->id = new_id;
	label->ref++;

	if (p != NULL)	/* insert new entry before p */
		TAILQ_INSERT_BEFORE(p, label, entry);
	else		/* either list empty or no free slot in between */
		TAILQ_INSERT_TAIL(&rt_labels, label, entry);

	return (label->id);
}

const char *
rtlabel_id2name(u_int16_t id)
{
	struct rt_label	*label;

	if (!id)
		return("");

	TAILQ_FOREACH(label, &rt_labels, entry)
		if (label->id == id)
			return (label->name);

	return ("");
}

void
rtlabel_unref(u_int16_t id)
{
	struct rt_label	*p, *next;

	if (id == 0)
		return;

	for (p = TAILQ_FIRST(&rt_labels); p != NULL; p = next) {
		next = TAILQ_NEXT(p, entry);
		if (id == p->id) {
			if (--p->ref == 0) {
				TAILQ_REMOVE(&rt_labels, p, entry);
				free(p);
			}
			break;
		}
	}
}
