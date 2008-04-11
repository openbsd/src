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

#include <assert.h>

# include "pass2.h"

#include <string.h>

int canaddr(NODE *);

/*
 * Check size of offset in OREG.  Called by oregok() to see if an
 * OREG can be generated.
 *
 * returns 0 if it can, 1 otherwise.
 */
int
notoff(TWORD t, int r, CONSZ off, char *cp)
{
#if 0
	if (off >= 32767 || off <= -32768)
		printf("; notoff %lld TOO BIG!\n", off);
#endif
	if (cp && cp[0]) return 1;
	return (off >= 32768 || off <= -32769);
}

/*
 * Generate instructions for an OREG.
 * Called by swmatch().
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
			if (isreg(p->n_left) == 0)
				(void)geninsn(p->n_left, INAREG);
			/* Converted in ormake() */
			return;
		}
	}
	(void)geninsn(p, INAREG);
}

/*
 * Unable to convert to OREG (notoff() returned failure).  Output
 * suitable instructions to replace OREG.
 */
void
myormake(NODE *q)
{
	NODE *p;

	if (x2debug)
		printf("myormake(%p)\n", q);

        p = q->n_left;

	/*
	 * This handles failed OREGs conversions, due to the offset
	 * being too large for an OREG.
	 */
	if ((p->n_op == PLUS || p->n_op == MINUS) && p->n_right->n_op == ICON) {
		if (isreg(p->n_left) == 0)
			(void)geninsn(p->n_left, INAREG);
		if (isreg(p->n_right) == 0)
			(void)geninsn(p->n_right, INAREG);
		(void)geninsn(p, INAREG);
	} else if (p->n_op == REG) {
		q->n_op = OREG;
		q->n_lval = p->n_lval;
		q->n_rval = p->n_rval;
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

	/* Turns currently anything into OREG on x86 */
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
	if (x2debug)
		printf("nspecial: op=%d, visit=0x%x: %s", q->op, q->visit, q->cstring);

	switch (q->op) {

	/* soft-float stuff */
        case RS:
        case LS:
                if (q->lshape == SBREG) {
                        static struct rspecial s[] = {
                                { NLEFT, R3R4 },
                                { NRIGHT, R5 },
                                { NRES, R3R4 },
                                { 0 },
                        };
                        return s;
                } else if (q->lshape == SAREG) {
                        static struct rspecial s[] = {
                                { NLEFT, R3 },
                                { NRIGHT, R4 },
                                { NRES, R3 },
                                { 0 },
                        };
                        return s;
                }

		cerror("nspecial LS/RS");
		break;

	case UMINUS:
	case SCONV:
		if (q->lshape == SBREG && q->rshape == SAREG) {
			static struct rspecial s[] = {
				{ NLEFT, R3R4 },
				{ NRES, R3 },
				{ 0 }
			};
			return s;
		} else if (q->lshape == SAREG && q->rshape == SBREG) {
			static struct rspecial s[] = {
				{ NLEFT, R3 },
				{ NRES, R3R4 },
				{ 0 }
			};
			return s;
		} else if (q->lshape == SAREG && q->rshape == SAREG) {
			static struct rspecial s[] = {
				{ NLEFT, R3 },
				{ NRES, R3 },
				{ 0 }
			};
			return s;
		} else if (q->lshape == SBREG && q->rshape == SBREG) {
			static struct rspecial s[] = {
				{ NLEFT, R3R4 },
				{ NRES, R3R4 },
				{ 0 }
			};
			return s;
		} else if (q->lshape == SCREG && q->rshape == SBREG) {
			static struct rspecial s[] = {
				{ NLEFT, F1 },
				{ NEVER, F0 }, /* stomped on */
				{ NRES, R3R4 },
				{ 0 }
			};
			return s;
		} else if (q->lshape == SBREG && q->rshape == SCREG) {
			static struct rspecial s[] = {
				{ NLEFT, R3R4 },
				{ NEVER, F0 }, /* stomped on */
				{ NRES, F1 },
				{ 0 }
			};
			return s;
		} else {
			static struct rspecial s[] = {
				{ NOLEFT, R0 },
				{ 0 } };
			return s;
		}

		break;

	case OPLOG:
		if (q->lshape == SBREG) {
			static struct rspecial s[] = {
				{ NLEFT, R3R4 },
				{ NRIGHT, R5R6 },
				{ NRES, R3 },
				{ 0 }
			};
			return s;
		} else if (q->lshape == SAREG) {
			static struct rspecial s[] = {
				{ NLEFT, R3 },
				{ NRIGHT, R4 },
				{ NRES, R3 },
				{ 0 }
			};
			return s;
		}

		cerror("nspecial oplog");
		break;

	case PLUS:
	case MINUS:
	case MUL:
	case DIV:
	case MOD:
		if (q->lshape == SBREG && 
		    (q->ltype & (TDOUBLE|TLDOUBLE|TLONGLONG|TULONGLONG))) {
			static struct rspecial s[] = {
				{ NLEFT, R3R4 },
				{ NRIGHT, R5R6 },
				{ NRES, R3R4 },
				{ 0 }
			};
			return s;
		} else if (q->lshape == SAREG && q->ltype & TFLOAT) {
			static struct rspecial s[] = {
				{ NLEFT, R3 },
				{ NRIGHT, R4 },
				{ NRES, R3 },
				{ 0 }
			};
			return s;
		} else if (q->lshape == SAREG) {
			static struct rspecial s[] = {
				{ NOLEFT, R0 },
				{ 0 } };
			return s;
		}

		cerror("nspecial mul");
		break;

	case STASG:
		{
			static struct rspecial s[] = {
				{ NEVER, R3 },
				{ NRIGHT, R4 },
				{ NEVER, R5 },
				{ 0 } };
			return s;
		}
		break;

	case OPLTYPE:
		{
			if (q->visit & SAREG) {
				static struct rspecial s[] = {
					{ NEVER, R0 },
//					{ NRES, R3 }, // hack - i don't know why
					{ 0 } };
				return s;
			}
		}
		break;

	case ASSIGN:
		if (q->lshape & SNAME) {
			static struct rspecial s[] = {
				{ NEVER, R0 },
				{ 0 } };
			return s;
		} else if (q->rshape & SNAME) {
			static struct rspecial s[] = {
				{ NOLEFT, R0 },
				{ 0 } };
			return s;
		} else if (q->lshape & SOREG) {
			static struct rspecial s[] = {
				{ NOLEFT, R0 },
				{ 0 } };
			return s;
		} else if (q->rshape & SOREG) {
			static struct rspecial s[] = {
				{ NORIGHT, R0 },
				{ 0 } };
			return s;
		}
		/* fallthough */

	case UMUL:
	case AND:
	case OR:
	case ER:
		{
			static struct rspecial s[] = {
				{ NOLEFT, R0 },
				{ 0 } };
			return s;
		}

	default:
		break;
	}

	comperr("nspecial entry %d: %s", q - table, q->cstring);
	return 0; /* XXX gcc */
}

/*
 * Set evaluation order of a binary node if it differs from default.
 */
int
setorder(NODE *p)
{
	return 0; /* nothing differs on x86 */
}

/*
 * Set registers "live" at function calls (like arguments in registers).
 * This is for liveness analysis of registers.
 */
int *
livecall(NODE *p)
{
	static int r[] = { R10, R9, R8, R7, R6, R5, R4, R3, R30, R31, -1 };
	int num = 1;

        if (p->n_op != CALL && p->n_op != FORTCALL && p->n_op != STCALL)
                return &r[8-0];

        for (p = p->n_right; p->n_op == CM; p = p->n_left)
                num += szty(p->n_right->n_type);
        num += szty(p->n_right->n_type);

        num = (num > 8 ? 8 : num);

        return &r[8 - num];
}

/*
 * Signal whether the instruction is acceptable for this target.
 */
int
acceptable(struct optab *op)
{
	if ((op->visit & FEATURE_PIC) != 0)
		return (kflag != 0);
	return features(op->visit & 0xffff0000);
}
