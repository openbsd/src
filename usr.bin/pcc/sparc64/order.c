/*	$OpenBSD: order.c,v 1.2 2008/04/11 20:45:52 stefan Exp $	*/
/*
 * Copyright (c) 2008 David Crawshaw <david@zentus.com>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
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

#include "pass2.h"

int
notoff(TWORD t, int r, CONSZ off, char *cp)
{
	return 0;
}

/*
 * Turn a UMUL-referenced node into OREG.
 */
void
offstar(NODE *p, int shape)
{
	if (x2debug)
		printf("offstar(%p)\n", p);

	if (p->n_op == PLUS || p->n_op == MINUS) {
		if (p->n_right->n_op == ICON) {
			if (isreg(p->n_left) == 0)
				(void)geninsn(p->n_left, INAREG);
			/* Converted in ormake() */
			return;
		}
	}
	(void)geninsn(p, INAREG);
}

void
myormake(NODE *q)
{
}

int
shumul(NODE *p)
{
	return SOREG;
}

int
setbin(NODE *p)
{
	return 0;
}

int
setasg(NODE *p, int cookie)
{
	return 0;
}

int
setuni(NODE *p, int cookie)
{
	return 0;
}

struct rspecial *
nspecial(struct optab *q)
{
	switch (q->op) {
	case STASG: {
		static struct rspecial s[] = {
			{ NEVER, O0 },
			{ NRIGHT, O1 },
			{ NEVER, O2 },
			{ 0 }
		};
		return s;
	}
	}

	comperr("unknown nspecial %d: %s", q - table, q->cstring);
	return 0; /* XXX */
}

int
setorder(NODE *p)
{
	return 0;
}

int *
livecall(NODE *p)
{
	static int ret[] = { O0, O1, O2, O3, O4, O5, O6, O7, -1 };
	return ret;
}

int
acceptable(struct optab *op)
{
	return 1;
}
