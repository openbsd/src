/*	$OpenBSD: irr_prefix.c,v 1.1 2007/03/03 11:45:30 henning Exp $ */

/*
 * Copyright (c) 2007 Henning Brauer <henning@openbsd.org>
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
#include <sys/param.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "irrfilter.h"

int	 prefix_set_compare(struct prefix_set *, struct prefix_set *);
struct prefix_set
	*prefix_set_find(char *);

RB_HEAD(prefix_set_h, prefix_set)	prefix_set_h;
RB_PROTOTYPE(prefix_set_h, prefix_set, entry, prefix_set_compare)
RB_GENERATE(prefix_set_h, prefix_set, entry, prefix_set_compare)

struct prefix_set	*curpfxs = NULL;

struct prefix_set *
prefixset_get(char *as)
{
	struct prefix_set	*pfxs;
	int			 r;

	if ((pfxs = prefix_set_find(as)) != NULL)
		return (pfxs);

	/* nothing found, resolve and store */
	if ((pfxs = calloc(1, sizeof(*pfxs))) == NULL)
		err(1, "get_prefixset calloc");
	if ((pfxs->as = strdup(as)) == NULL)
		err(1, "get_prefixset strdup");
	RB_INSERT(prefix_set_h, &prefix_set_h, pfxs);

	curpfxs = pfxs;
	if ((r = whois(as, QTYPE_ROUTE)) == -1)
		errx(1, "whois error, prefixset_get %s", as);
	curpfxs = NULL;

	return (pfxs);
}

int
prefixset_addmember(char *s)
{
	void	*p;
	u_int	 i;

	/* yes, there are dupes... e. g. from multiple sources */
	for (i = 0; i < curpfxs->prefixcnt; i++)
		if (!strcmp(curpfxs->prefix[i], s))
			return (0);

	if ((p = realloc(curpfxs->prefix,
	    (curpfxs->prefixcnt + 1) * sizeof(char *))) == NULL)
		err(1, "prefixset_addmember strdup");
	curpfxs->prefix = p;
	curpfxs->prefixcnt++;

	if ((curpfxs->prefix[curpfxs->prefixcnt - 1] =
	    strdup(s)) == NULL)
		err(1, "prefixset_addmember strdup");

	return (1);
}


/* RB helpers */
int
prefix_set_compare(struct prefix_set *a, struct prefix_set *b)
{
	return (strcmp(a->as, b->as));
}

struct prefix_set *
prefix_set_find(char *as)
{
	struct prefix_set	s;

	s.as = as;
	return (RB_FIND(prefix_set_h, &prefix_set_h, &s));
}
