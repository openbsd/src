/*	$OpenBSD: order.c,v 1.3 2007/12/22 13:13:27 stefan Exp $	*/
/*
 * Copyright (c) 2006 Anders Magnusson (ragge@ludd.luth.se).
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
	if (r != 2 && r != 3)
		return 1; /* can only index ac2 and ac3 */
	if (t == CHAR || t == UCHAR) {
		if (off < -256 || off > 254)
			return 1;
	} else if (off < -128 || off > 127)
		return 1;
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
	if( p->n_op == PLUS || p->n_op == MINUS ){
		if( r->n_op == ICON ){
			if (isreg(p->n_left) == 0 ||
			    (p->n_left->n_op == REG &&
			    p->n_left->n_rval != 2 && p->n_left->n_rval != 3))
				(void)geninsn(p->n_left, INBREG);
			/* Converted in ormake() */
			return;
		}
	}
	(void)geninsn(p, INBREG);
}

/*
 * Do the actual conversion of offstar-found OREGs into real OREGs.
 */
void
myormake(NODE *q)
{
	if (x2debug)
		printf("myormake(%p)\n", q);
}

/*
 * Shape matches for UMUL.  Cooperates with offstar().
 */
int
shumul(NODE *p)
{

	if (x2debug)
		printf("shumul(%p)\n", p);

	/* Turns currently anything into OREG on x86 */
	return SOREG;
}

/*
 * Rewrite increment/decrement operation.
 */
int
setincr(NODE *p)
{
	if (x2debug)
		printf("setincr(%p)\n", p);

	return(0);
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
		printf("setasg(%p)\n", p);
	return(0);
}

/* setup for unary operator */
int
setuni(NODE *p, int cookie)
{
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
	return 0;
}
/*
 * Set registers "live" at function calls (like arguments in registers).
 * This is for liveness analysis of registers.
 */
int *
livecall(NODE *p)
{
	static int r[1] = { -1 }; /* Terminate with -1 */

	return &r[0];
}

/*
 * Signal whether the instruction is acceptable for this target.
 */
int
acceptable(struct optab *op)
{
	return 1;
}
