/*	$OpenBSD: order.c,v 1.5 2008/04/11 20:45:52 stefan Exp $	*/
/*
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

/*
 * MIPS port by Jan Enoksson (janeno-1@student.ltu.se) and
 * Simon Olsson (simols-1@student.ltu.se) 2005.
 */

#include "pass2.h"

/*
 * is it legal to make an OREG or NAME entry which has an offset of off,
 * (from a register of r), if the resulting thing had type t
 */
int
notoff(TWORD t, int r, CONSZ off, char *cp)
{
	/*
	 * although the hardware doesn't permit offsets greater
	 * than +/- 32K, the assembler fixes it for us.
	 */
	return 0;		/* YES */
}

/*
 * Turn a UMUL-referenced node into OREG.
 */
void
offstar(NODE * p, int shape)
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

/*
 * Do the actual conversion of offstar-found OREGs into real OREGs.
 */
void
myormake(NODE * q)
{
	if (x2debug)
		printf("myormake(%p)\n", q);
}

/*
 * Shape matches for UMUL.  Cooperates with offstar().
 */
int
shumul(NODE * p)
{
	if (x2debug)
		printf("shumul(%p)\n", p);

	/* Always turn it into OREG */
	return SOREG;
}

/*
 * Rewrite operations on binary operators (like +, -, etc...).
 * Called as a result of table lookup.
 */
int
setbin(NODE * p)
{

	if (x2debug)
		printf("setbin(%p)\n", p);
	return 0;

}

/* setup for assignment operator */
int
setasg(NODE * p, int cookie)
{
	if (x2debug)
		printf("setasg(%p)\n", p);
	return (0);
}

/* setup for unary operator */
int
setuni(NODE * p, int cookie)
{
	return 0;
}

/*
 * Special handling of some instruction register allocation.
 * - left is the register that left node wants.
 * - right is the register that right node wants.
 * - res is in which register the result will end up.
 * - mask is registers that will be clobbered.
 */
struct rspecial *
nspecial(struct optab * q)
{
	switch (q->op) {

	case SCONV:
		if (q->lshape == SBREG && q->rshape == SCREG) {
			static struct rspecial s[] = {
				{ NLEFT, A0A1 },
				{ NRES, F0 },
				{ 0 }
			};
			return s;
		} else if (q->lshape == SCREG && q->rshape == SBREG) {
			static struct rspecial s[] = {
				{ NLEFT, F0 },
				{ NRES, A0A1 },
				{ 0 }
			};
			return s;
		} else if (q->lshape == SAREG && q->rshape == SCREG) {
			static struct rspecial s[] = {
				{ NLEFT, A0 },
				{ NRES, F0 },
				{ 0 }
			};
			return s;
		}
		break;

	case MOD:
	case DIV:
		if (q->lshape == SBREG) {
			static struct rspecial s[] = {
				{ NLEFT, A0A1 },
				{ NRIGHT, A2A3 },
				{ NRES, V0V1 },
				{ 0 },
			};
			return s;
		} else if (q->lshape == SAREG) {
			static struct rspecial s[] = {
				{ NLEFT, A0 },
				{ NRIGHT, A1 },
				{ NRES, V0 },
				{ 0 },
			};
			return s;
		}

	case RS:
	case LS:
		if (q->lshape == SBREG) {
			static struct rspecial s[] = {
				{ NLEFT, A0A1 },
				{ NRIGHT, A2 },
				{ NRES, V0V1 },
				{ 0 },
			};
			return s;
		} else if (q->lshape == SAREG) {
			static struct rspecial s[] = {
				{ NLEFT, A0 },
				{ NRIGHT, A1 },
				{ NRES, V0 },
				{ 0 },
			};
			return s;
		}
		break;

	case STARG:
                {
                        static struct rspecial s[] = {
                                { NEVER, A0 },
                                { NLEFT, A1 },
                                { NEVER, A2 },
                                { 0 }
			};
                        return s;
                }

        case STASG:
                {
                        static struct rspecial s[] = {
                                { NEVER, A0 },
                                { NRIGHT, A1 },
                                { NEVER, A2 },
                                { 0 }
			};
                        return s;
                }
	}

	comperr("nspecial entry %d: %s", q - table, q->cstring);

	return 0;		/* XXX gcc */
}

/*
 * Set evaluation order of a binary node if it differs from default.
 */
int
setorder(NODE * p)
{
	return 0;		/* nothing differs */
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
