/* $OpenBSD: tag.c,v 1.29 2020/03/13 16:14:14 schwarze Exp $ */
/*
 * Copyright (c) 2015,2016,2018,2019,2020 Ingo Schwarze <schwarze@openbsd.org>
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
 *
 * Functions to tag syntax tree nodes.
 * For internal use by mandoc(1) validation modules only.
 */
#include <sys/types.h>

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc_aux.h"
#include "mandoc_ohash.h"
#include "roff.h"
#include "tag.h"

struct tag_entry {
	struct roff_node **nodes;
	size_t	 maxnodes;
	size_t	 nnodes;
	int	 prio;
	char	 s[];
};

static struct ohash	 tag_data;


/*
 * Set up the ohash table to collect nodes
 * where various marked-up terms are documented.
 */
void
tag_alloc(void)
{
	mandoc_ohash_init(&tag_data, 4, offsetof(struct tag_entry, s));
}

void
tag_free(void)
{
	struct tag_entry	*entry;
	unsigned int		 slot;

	if (tag_data.info.free == NULL)
		return;
	entry = ohash_first(&tag_data, &slot);
	while (entry != NULL) {
		free(entry->nodes);
		free(entry);
		entry = ohash_next(&tag_data, &slot);
	}
	ohash_delete(&tag_data);
	tag_data.info.free = NULL;
}

/*
 * Set a node where a term is defined,
 * unless it is already defined at a lower priority.
 */
void
tag_put(const char *s, int prio, struct roff_node *n)
{
	struct tag_entry	*entry;
	const char		*se;
	size_t			 len;
	unsigned int		 slot;

	assert(prio <= TAG_FALLBACK);

	if (s == NULL) {
		if (n->child == NULL || n->child->type != ROFFT_TEXT)
			return;
		s = n->child->string;
		if (s[0] == '\\' && (s[1] == '&' || s[1] == 'e'))
			s += 2;
	}

	/*
	 * Skip whitespace and escapes and whatever follows,
	 * and if there is any, downgrade the priority.
	 */

	len = strcspn(s, " \t\\");
	if (len == 0)
		return;

	se = s + len;
	if (*se != '\0' && prio < TAG_WEAK)
		prio = TAG_WEAK;

	slot = ohash_qlookupi(&tag_data, s, &se);
	entry = ohash_find(&tag_data, slot);

	/* Build a new entry. */

	if (entry == NULL) {
		entry = mandoc_malloc(sizeof(*entry) + len + 1);
		memcpy(entry->s, s, len);
		entry->s[len] = '\0';
		entry->nodes = NULL;
		entry->maxnodes = entry->nnodes = 0;
		ohash_insert(&tag_data, slot, entry);
	}

	/*
	 * Lower priority numbers take precedence.
	 * If a better entry is already present, ignore the new one.
	 */

	else if (entry->prio < prio)
			return;

	/*
	 * If the existing entry is worse, clear it.
	 * In addition, a tag with priority TAG_FALLBACK
	 * is only used if the tag occurs exactly once.
	 */

	else if (entry->prio > prio || prio == TAG_FALLBACK) {
		while (entry->nnodes > 0)
			entry->nodes[--entry->nnodes]->flags &= ~NODE_ID;

		if (prio == TAG_FALLBACK) {
			entry->prio = TAG_DELETE;
			return;
		}
	}

	/* Remember the new node. */

	if (entry->maxnodes == entry->nnodes) {
		entry->maxnodes += 4;
		entry->nodes = mandoc_reallocarray(entry->nodes,
		    entry->maxnodes, sizeof(*entry->nodes));
	}
	entry->nodes[entry->nnodes++] = n;
	entry->prio = prio;
	n->flags |= NODE_ID;
	if (n->child == NULL || n->child->string != s || *se != '\0') {
		assert(n->string == NULL);
		n->string = mandoc_strndup(s, len);
	}
}

enum tag_result
tag_check(const char *test_tag)
{
	unsigned int slot;

	if (ohash_first(&tag_data, &slot) == NULL)
		return TAG_EMPTY;
	else if (test_tag != NULL && ohash_find(&tag_data,
	    ohash_qlookup(&tag_data, test_tag)) == NULL)
		return TAG_MISS;
	else
		return TAG_OK;
}
