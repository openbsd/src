/*	$OpenBSD: order.c,v 1.3 2008/04/11 20:45:52 stefan Exp $	*/

/*
 * Copyright (c) 2007 Michael Shalayeff
 * Copyright (c) 2003 Anders Magnusson (ragge@ludd.luth.se).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


# include "pass2.h"

#include <string.h>

int canaddr(NODE *);

/* is it legal to make an OREG or NAME entry which has an
 * offset of off, (from a register of r), if the
 * resulting thing had type t */
int
notoff(TWORD t, int r, CONSZ off, char *cp)
{
	return(0);  /* YES */
}

/*
 * Turn a UMUL-referenced node into OREG.
 * Be careful about register classes, this is a place where classes change.
 */
void
offstar(NODE *p, int shape)
{
	NODE *r;

	if (x2debug)
		printf("offstar(%p)\n", p);

	if (isreg(p))
		return; /* Is already OREG */

	r = p->n_right;
	if (p->n_op == PLUS || p->n_op == MINUS) {
		if (r->n_op == ICON) {
			if (isreg(p->n_left) == 0)
				(void)geninsn(p->n_left, INAREG);
			/* Converted in ormake() */
			return;
		}
	}
	(void)geninsn(p, INAREG);
}

/*
 * Do the actual conversion of offstar-found OREGs into real OREGs.
 */
void
myormake(NODE *q)
{
	NODE *p, *r;

	if (x2debug)
		printf("myormake(%p)\n", q);

	p = q->n_left;
	if (p->n_op == PLUS && (r = p->n_right)->n_op == LS &&
	    r->n_right->n_op == ICON && r->n_right->n_lval == 2 &&
	    p->n_left->n_op == REG && r->n_left->n_op == REG) {
		q->n_op = OREG;
		q->n_lval = 0;
		q->n_rval = R2PACK(p->n_left->n_rval, r->n_left->n_rval, 0);
		tfree(p);
	}
}

/*
 * Shape matches for UMUL.  Cooperates with offstar().
 */
int
shumul(NODE *p)
{

	if (x2debug)
		printf("shumul(%p)\n", p);

	/* Turns currently anything into OREG on hppa */
	return SOREG;
}

/*
 * Rewrite operations on binary operators (like +, -, etc...).
 * Called as a result of table lookup.
 */
int
setbin(NODE *p)
{
	if (x2debug)
		printf("setbin(%p)\n", p);

	return 0;
}

/* setup for assignment operator */
int
setasg(NODE *p, int cookie)
{
	if (x2debug)
		printf("setasg(%p,%s)\n", p, prcook(cookie));

	if (p->n_left->n_op == FLD && !isreg(p->n_left->n_left)) {
		NODE *l, *r;
		int reg;

		geninsn(p->n_left->n_left, INAREG);

		reg = DECRA(p->n_left->n_left->n_reg, 0);
		l = tcopy(p->n_left->n_left);
		p->n_left->n_left->n_op = REG;
		p->n_left->n_left->n_rval = reg;
		p->n_left->n_left->n_lval = 0;
		r = tcopy(p->n_left->n_left);

		geninsn(p->n_left, INAREG);
		l = mkbinode(ASSIGN, l, r, l->n_type);
		geninsn(l, INAREG);
		return (1);
	}

	return (0);
}

/* setup for unary operator */
int
setuni(NODE *p, int cookie)
{
	if (x2debug)
		printf("setuni(%p,%s)\n", p, prcook(cookie));

	return 0;
}

/*
 * Special handling of some instruction register allocation.
 */
struct rspecial *
nspecial(struct optab *q)
{
	comperr("nspecial entry %d", q - table);
	return 0; /* XXX gcc */
}

/*
 * Set evaluation order of a binary node if it differs from default.
 */
int
setorder(NODE *p)
{
	return 0; /* nothing differs on hppa */
}

/*
 * Set registers "live" at function calls (like arguments in registers).
 * This is for liveness analysis of registers.
 */
int *
livecall(NODE *p)
{
	static int r[5], *s = &r[4];
	 
	*s = -1;
	if (p->n_op == UCALL || p->n_op == UFORTCALL || p->n_op == USTCALL ||
	    p->n_op == FORTCALL)
		return s;

	for (p = p->n_right; p->n_op == CM; p = p->n_left)
		if (p->n_right->n_op == ASSIGN &&
		    p->n_right->n_left->n_op == REG)
			*--s = p->n_right->n_left->n_rval;

	if (p->n_op == ASSIGN &&
	    p->n_left->n_op == REG)
		*--s = p->n_left->n_rval;

	return s;
}

/*
 * Signal whether the instruction is acceptable for this target.
 */
int
acceptable(struct optab *op)
{
	return 1;
}
