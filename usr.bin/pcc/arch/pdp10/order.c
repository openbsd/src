/*	$OpenBSD: order.c,v 1.2 2007/09/15 22:04:38 ray Exp $	*/
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

int canaddr(NODE *);

/*
 * should the assignment op p be stored,
 * given that it lies as the right operand of o
 * (or the left, if o==UMUL)
 */
void
stoasg(NODE *p, int o)
{
	if (x2debug)
		printf("stoasg(%p, %o)\n", p, o);
}

/* should we delay the INCR or DECR operation p */
int
deltest(NODE *p)
{
	TWORD ty = p->n_type;

	return ty == PTR+CHAR || ty == PTR+UCHAR ||
	    ty == PTR+SHORT || ty == PTR+USHORT;
}

/*
 * Check if p can be autoincremented.
 * Nothing can be autoincremented on PDP10.
 */
int
autoincr(NODE *p)
{
	return 0;
}

/* is it legal to make an OREG or NAME entry which has an
 * offset of off, (from a register of r), if the
 * resulting thing had type t */
int
notoff(TWORD t, int r, CONSZ off, char *cp)
{
	return(0);  /* YES */
}

int radebug = 0;

int
offstar(NODE *p)
{
	NODE *q;

	if (x2debug)
		printf("offstar(%p)\n", p);

	if( p->n_op == PLUS || p->n_op == MINUS ){
		if( p->n_right->n_op == ICON ){
			q = p->n_left;
			if (q->n_op != REG)
				geninsn(q, INTAREG|INAREG);
			p->n_su = -1;
			return 1;
		}
	}
	geninsn(p, INTAREG|INAREG);
	return 0;
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
 * findops() failed, see if we can rewrite it to match.
 */
int
setbin(NODE *p)
{
	TWORD ty;
	NODE *r, *s;

	ty = p->n_type;
	switch (p->n_op) {
	case MINUS:
		switch (ty) {
		case PTR+CHAR:
		case PTR+UCHAR:
		case PTR+SHORT:
		case PTR+USHORT:
			/*
			 * Must negate the right side and change op to PLUS.
			 */
			r = p->n_right;
			if (r->n_op == ICON) {
				r->n_lval = -r->n_lval;
			} else {
				s = talloc();
				s->n_type = r->n_type;
				s->n_op = UMINUS;
				s->n_left = r;
				p->n_right = s;
			}
			p->n_op = PLUS;
			return 1;
		}
	}
	return 0;
}

/* setup for assignment operator */
int
setasg(NODE *p, int cookie)
{
	return(0);
}

/* setup for unary operator */
int
setuni(NODE *p, int cookie)
{
	return 0;
}

int
special(NODE *p, int shape)
{
	switch (shape) {
	case SUSHCON:
		if (p->n_op == ICON && p->n_name[0] == '\0' &&
		    (p->n_lval > 0 && p->n_lval <= 0777777))
			return 1;
		break;

	case SNSHCON:
		if (p->n_op == ICON && p->n_name[0] == '\0' &&
		    (p->n_lval < 0 && p->n_lval > -01000000))
			return 1;
		break;
	case SILDB:
		if (p->n_op == ASSIGN && p->n_left->n_op == REG &&
		    p->n_right->n_op == PLUS &&
		    p->n_right->n_left->n_op == REG &&
		    p->n_right->n_right->n_op == ICON && 
		    p->n_right->n_right->n_lval == 1 &&
		    p->n_right->n_left->n_rval == p->n_left->n_rval)
			return 1;
		break;
	}
	return 0;
}
