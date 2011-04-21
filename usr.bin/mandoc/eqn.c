/*	$Id: eqn.c,v 1.2 2011/04/21 22:59:54 schwarze Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "roff.h"
#include "libmandoc.h"
#include "libroff.h"

/* ARGSUSED */
enum rofferr
eqn_read(struct eqn_node **epp, int ln, const char *p, int offs)
{
	size_t		 sz;
	struct eqn_node	*ep;

	if (0 == strcmp(p, ".EN")) {
		*epp = NULL;
		return(ROFF_EQN);
	}

	ep = *epp;

	sz = strlen(&p[offs]);
	ep->eqn.data = mandoc_realloc(ep->eqn.data, ep->eqn.sz + sz + 1);
	if (0 == ep->eqn.sz)
		*ep->eqn.data = '\0';

	ep->eqn.sz += sz;
	strlcat(ep->eqn.data, &p[offs], ep->eqn.sz + 1);
	return(ROFF_IGN);
}

struct eqn_node *
eqn_alloc(int pos, int line)
{
	struct eqn_node	*p;

	p = mandoc_calloc(1, sizeof(struct eqn_node));
	p->eqn.line = line;
	p->eqn.pos = pos;

	return(p);
}

/* ARGSUSED */
void
eqn_end(struct eqn_node *e)
{

	/* Nothing to do. */
}

void
eqn_free(struct eqn_node *p)
{

	free(p->eqn.data);
	free(p);
}
