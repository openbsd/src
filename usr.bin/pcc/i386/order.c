/*	$OpenBSD: order.c,v 1.1 2007/10/22 13:39:37 otto Exp $	*/
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
	if( p->n_op == PLUS || p->n_op == MINUS ){
		if( r->n_op == ICON ){
			if (isreg(p->n_left) == 0)
				(void)geninsn(p->n_left, INAREG);
			/* Converted in ormake() */
			return;
		}
		if (r->n_op == LS && r->n_right->n_op == ICON &&
		    r->n_right->n_lval == 2 && p->n_op == PLUS) {
			if (isreg(p->n_left) == 0)
				(void)geninsn(p->n_left, INAREG);
			if (isreg(r->n_left) == 0)
				(void)geninsn(r->n_left, INAREG);
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
	switch (q->op) {
	case OPLOG:
		{
			static struct rspecial s[] = { { NEVER, EAX }, { 0 } };
			return s;
		}

	case STASG:
	case STARG:
		{
			static struct rspecial s[] = {
				{ NEVER, EAX }, { NEVER, EDX },
				{ NEVER, ECX }, { 0 } };
			return s;
		}

	case SCONV:
		if ((q->ltype & (TINT|TUNSIGNED|TSHORT|TUSHORT)) && 
		    q->rtype == (TCHAR|TUCHAR)) {
			static struct rspecial s[] = { 
				{ NOLEFT, ESI }, { NOLEFT, EDI }, { 0 } };
			return s;
		} else if ((q->ltype & (TINT|TUNSIGNED)) &&
		    q->rtype == TLONGLONG) {
			static struct rspecial s[] = {
				{ NLEFT, EAX }, { NRES, EAXEDX },
				{ NEVER, EAX }, { NEVER, EDX }, { 0 } };
			return s;
		} else if (q->ltype == TSHORT &&
		    q->rtype == (TLONGLONG|TULONGLONG)) {
			static struct rspecial s[] = {
				{ NRES, EAXEDX },
				{ NEVER, EAX }, { NEVER, EDX }, { 0 } };
			return s;
		} else if (q->ltype == TCHAR &&
		    q->rtype == (TLONGLONG|TULONGLONG)) {
			static struct rspecial s[] = {
				{ NRES, EAXEDX },
				{ NEVER, EAX }, { NEVER, EDX }, { 0 } };
			return s;
		}
		break;
	case DIV:
		if (q->lshape == SBREG) {
			static struct rspecial s[] = {
				{ NEVER, AL }, { NEVER, AH },
				{ NLEFT, AL }, { NRES, AL },
				{ NORIGHT, AH }, { NORIGHT, AL }, { 0 } };
				return s;
		} else if (q->lshape == SAREG) {
			static struct rspecial s[] = {
				{ NEVER, EAX }, { NEVER, EDX },
				{ NLEFT, EAX }, { NRES, EAX },
				{ NORIGHT, EDX }, { NORIGHT, EAX }, { 0 } };
			return s;
		} else if (q->lshape & SCREG) {
			static struct rspecial s[] = {
				{ NEVER, EAX }, { NEVER, EDX },
				{ NEVER, ECX }, { NRES, EAXEDX }, { 0 } };
			return s;
		}
		break;
	case MOD:
		if (q->lshape == SBREG) {
			static struct rspecial s[] = {
				{ NEVER, AL }, { NEVER, AH },
				{ NLEFT, AL }, { NRES, AH },
				{ NORIGHT, AH }, { NORIGHT, AL }, { 0 } };
			return s;
		} else if (q->lshape == SAREG) {
			static struct rspecial s[] = {
				{ NEVER, EAX }, { NEVER, EDX },
				{ NLEFT, EAX }, { NRES, EDX },
				{ NORIGHT, EDX }, { NORIGHT, EAX }, { 0 } };
			return s;
		} else if (q->lshape & SCREG) {
			static struct rspecial s[] = {
				{ NEVER, EAX }, { NEVER, EDX },
				{ NEVER, ECX }, { NRES, EAXEDX }, { 0 } };
			return s;
		}
		break;
	case MUL:
		if (q->lshape == SBREG) {
			static struct rspecial s[] = {
				{ NEVER, AL }, { NEVER, AH },
				{ NLEFT, AL }, { NRES, AL }, { 0 } };
			return s;
		} else if (q->lshape & SCREG) {
			static struct rspecial s[] = {
				{ NEVER, EAX }, { NEVER, EDX },
				{ NEVER, ECX }, { NRES, EAXEDX }, { 0 } };
			return s;
		}
		break;
	case LS:
	case RS:
		if (q->visit & (INAREG|INBREG)) {
			static struct rspecial s[] = {
				{ NRIGHT, CL }, { NOLEFT, ECX }, { 0 } };
			return s;
		} else if (q->visit & INCREG) {
			static struct rspecial s[] = {
				{ NEVER, EAX }, { NEVER, EDX },
				{ NEVER, ECX }, { NRES, EAXEDX }, { 0 } };
			return s;
		}
		break;

	default:
		break;
	}
	comperr("nspecial entry %d", q - table);
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
