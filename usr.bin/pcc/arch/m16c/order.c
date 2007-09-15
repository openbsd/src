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
# include <strings.h>

int canaddr(NODE *);

/*
 * should the assignment op p be stored,
 * given that it lies as the right operand of o
 * (or the left, if o==UNARY MUL)
 */
/*
void
stoasg(NODE *p, int o)
{
	if (x2debug)
		printf("stoasg(%p, %o)\n", p, o);
}
*/
/* should we delay the INCR or DECR operation p */
int
deltest(NODE *p)
{
	return 0;
}

/*
 * Check if p can be autoincremented.
 * XXX - nothing can be autoincremented for now.
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

/*
 * Turn a UMUL-referenced node into OREG.
 */
int
offstar(NODE *p, int shape)
{
	if (x2debug)
		printf("offstar(%p)\n", p);

	if( p->n_op == PLUS || p->n_op == MINUS ){
		if( p->n_right->n_op == ICON ){
			geninsn(p->n_left, INBREG);
			p->n_su = -1;
			return 1;
		}
	}
	geninsn(p, INBREG);
	return 0;
}

/*
 * Shape matches for UMUL.  Cooperates with offstar().
 */
int
shumul(NODE *p)
{
//	NODE *l = p->n_left;

#ifdef PCC_DEBUG
	if (x2debug) {
		printf("shumul(%p)\n", p);
		fwalk(p, e2print, 0);
	}
#endif

	/* Can only generate OREG of BREGs (or FB) */
	if (p->n_op == REG && (isbreg(p->n_rval) || p->n_rval == FB))
		return SOREG;
#if 0
	if ((p->n_op == PLUS || p->n_op == MINUS) &&
	    (l->n_op == REG && (isbreg(l->n_rval) || l->n_rval == FB)) &&
	    p->n_right->n_op == ICON)
		return SOREG;
	return 0;
#else
	return SOREG;
#endif
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

#if 0
/*
 * register allocation for instructions with special preferences.
 */
regcode
regalloc(NODE *p, struct optab *q, int wantreg)
{
	regcode regc;

	if (q->op == DIV || q->op == MOD) {
		/*
		 * 16-bit div.
		 */
		if (regblk[R0] & 1 || regblk[R2] & 1)
			comperr("regalloc: needed regs inuse, node %p", p);
		if (p->n_su & DORIGHT) {
			regc = alloregs(p->n_right, A0);
			if (REGNUM(regc) != A0) {
				p->n_right = movenode(p->n_right, A0);
				if ((p->n_su & RMASK) == ROREG) {
					p->n_su &= ~RMASK;
					p->n_su |= RREG;
					p->n_right->n_su &= ~LMASK;
					p->n_right->n_su |= LOREG;
				}
				freeregs(regc);
				regblk[A0] |= 1;
			}
		}
		regc = alloregs(p->n_left, R0);
		if (REGNUM(regc) != R0) {
			p->n_left = movenode(p->n_left, R0);
			freeregs(regc);
			regblk[R0] |= 1;
		}
		if ((p->n_su & RMASK) && !(p->n_su & DORIGHT)) {
			regc = alloregs(p->n_right, A0);
			if (REGNUM(regc) != A0) {
				p->n_right = movenode(p->n_right, A0);
				if ((p->n_su & RMASK) == ROREG) {
					p->n_su &= ~RMASK;
					p->n_su |= RREG;
					p->n_right->n_su &= ~LMASK;
					p->n_right->n_su |= LOREG;
				}
			}
		}
		regblk[A0] &= ~1;
		regblk[R0] &= ~1;
		regblk[R2] &= ~1;
		if (q->op == DIV) {
			MKREGC(regc, R0, 1);
			regblk[R0] |= 1;
		} else {
			MKREGC(regc, R2, 1);
			regblk[R2] |= 1;
		}
	} else
		comperr("regalloc");
	p->n_rall = REGNUM(regc);
	return regc;
}
#endif

/*
 * Special handling of some instruction register allocation.
 * - left is the register that left node wants.
 * - right is the register that right node wants.
 * - res is in which register the result will end up.
 * - mask is registers that will be clobbered.
 *
 *  XXX - Fix this function
 */
struct rspecial *
nspecial(struct optab *q)
{
    switch (q->op) {

    case DIV:
    case MOD:
	if(q->ltype & (TINT|TSHORT)){
	    static struct rspecial s[] = {
		{ NRES, R0 }, { NRES, R2}, { 0 } };
	    return s;
	}
	/*
	else if(q->ltype & TCHAR) {
	    static struct rspecial s[] = {
		{ NRES, R0L }, { NRES, R0H}, { 0 } };
	    return s;
	    }*/
	break;

    case MUL:
	/*
	if(q->ltype & (TINT|TSHORT)){
	    static struct rspecial s[] = {
		{ NRES, R0 }, { NRES, R2}, { 0 } };
	    return s;
	    }*/
	comperr("multiplication not implemented");
	break;
	
    default:
	break;
    }
    comperr("nspecial entry %d", q - table);
    return 0; /* XXX gcc */
}


/*
 * Splitup a function call and give away its arguments first.
 * Calling convention used ("normal" in IAR syntax) is:
 * - 1-byte parameters in R0L if possible, otherwise in R0H.
 * - 2-byte pointers in A0.
 * - 2-byte non-pointers in R0 if no byte-size arguments are found in
 *   in the first 6 bytes of parameters, otherwise R2 or at last A0.
 * - 4-byte parameters in R2R0.
 */
void
gencall(NODE *p, NODE *prev)
{
	NODE *n = 0; /* XXX gcc */
	static int storearg(NODE *);
	int o = p->n_op;
	int ty = optype(o);

	if (ty == LTYPE)
		return;

	switch (o) {
	case CALL:
		/* swap arguments on some hardop-converted insns */
		/* Normal call, just push args and be done with it */
		p->n_op = UCALL;
//printf("call\n");
		/* Check if left can be evaluated directly */
		if (p->n_left->n_op == UMUL) {
			TWORD t = p->n_left->n_type;
			int k = BITOOR(freetemp(szty(t)));
			NODE *n = mklnode(OREG, k, FB, t);
			NODE *q = tcopy(n);
			pass2_compile(ipnode(mkbinode(ASSIGN, n, p->n_left,t)));
			p->n_left = q;
		}
		gencall(p->n_left, p);
		p->n_rval = storearg(p->n_right);
//printf("end call\n");
		break;

	case UFORTCALL:
	case FORTCALL:
		comperr("FORTCALL");

	case USTCALL:
	case STCALL:
		/*
		 * Structure return.  Look at the node above
		 * to decide about buffer address:
		 * - FUNARG, allocate space on stack, don't remove.
		 * - nothing, allocate space on stack and remove.
		 * - STASG, get the address of the left side as arg.
		 * - FORCE, this ends up in a return, get supplied addr.
		 * (this is not pretty, but what to do?)
		 */
		if (prev == NULL || prev->n_op == FUNARG) {
			/* Create nodes to generate stack space */
			n = mkbinode(ASSIGN, mklnode(REG, 0, STKREG, INT),
			    mkbinode(MINUS, mklnode(REG, 0, STKREG, INT),
			    mklnode(ICON, p->n_stsize, 0, INT), INT), INT);
//printf("stsize %d\n", p->n_stsize);
			pass2_compile(ipnode(n));
		} else if (prev->n_op == STASG) {
			n = prev->n_left;
			if (n->n_op == UMUL)
				n = nfree(n);
			else if (n->n_op == NAME) {
				n->n_op = ICON; /* Constant reference */
				n->n_type = INCREF(n->n_type);
			} else
				comperr("gencall stasg");
		} else if (prev->n_op == FORCE) {
			; /* do nothing here */
		} else {
			comperr("gencall bad op %d", prev->n_op);
		}

		/* Deal with standard arguments */
		gencall(p->n_left, p);
		if (o == STCALL) {
			p->n_op = USTCALL;
			p->n_rval = storearg(p->n_right);
		} else
			p->n_rval = 0;
		/* push return struct address */
		if (prev == NULL || prev->n_op == FUNARG) {
			n = mklnode(REG, 0, STKREG, INT);
			if (p->n_rval)
				n = mkbinode(PLUS, n,
				    mklnode(ICON, p->n_rval, 0, INT), INT);
			pass2_compile(ipnode(mkunode(FUNARG, n, 0, INT)));
			if (prev == NULL)
				p->n_rval += p->n_stsize/4;
		} else if (prev->n_op == FORCE) {
			/* return value for this function */
			n = mklnode(OREG, 8, FPREG, INT);
			pass2_compile(ipnode(mkunode(FUNARG, n, 0, INT)));
			p->n_rval++;
		} else {
			pass2_compile(ipnode(mkunode(FUNARG, n, 0, INT)));
			n = p;
			*prev = *p;
			nfree(n);
		}
//printf("end stcall\n");
		break;

	default:
		if (ty != UTYPE)
			gencall(p->n_right, p);
		gencall(p->n_left, p);
		break;
	}
}

/*
 * Create separate node trees for function arguments.
 * This is partly ticky, the strange calling convention 
 * may cause a bunch of code reorganization here.
 */
static int
storearg(NODE *p)
{
	NODE *n, *q, **narry;
	int nch, k, i, nn, rary[4];
	int r0l, r0h, r2, a0, stk, sz;
	TWORD t;
	int maxrargs = 0;

	if (p->n_op == CM)
		maxrargs = p->n_stalign;

	/* count the arguments */
	for (i = 1, q = p; q->n_op == CM; q = q->n_left)
		i++;
	nn = i;

	/* allocate array to store arguments */
	narry = tmpalloc(sizeof(NODE *)*nn);

	/* enter nodes into array */
	for (q = p; q->n_op == CM; q = q->n_left)
		narry[--i] = q->n_right;
	narry[--i] = q;

	/* free CM nodes */
	for (q = p; q->n_op == CM; ) {
		n = q->n_left;
		nfree(q);
		q = n;
	}

	/* count char args */
	r0l = r0h = r2 = a0 = 0;
	for (sz = nch = i = 0; i < nn && i < 6; i++) {
		TWORD t = narry[i]->n_type;
		if (sz >= 6)
			break;
		if (t == CHAR || t == UCHAR) {
			nch++;
			sz++;
		} else if ((t >= SHORT && t <= UNSIGNED) ||
		    t > BTMASK || t == FLOAT) {
			sz += 2;
		} else /* long, double */
			sz += 4;
			
	}

	/*
	 * Now the tricky part. The parameters that should be on stack
	 * must be found and pushed first, then the register parameters.
	 * For the latter, be sure that evaluating them do not use any
	 * registers where argument values already are inserted.
	 * XXX - function pointers?
	 * XXX foo(long a, char b) ???
	 */
	for (stk = 0; stk < 4; stk++) {
		TWORD t;

		if (stk == nn)
			break;
		t = narry[stk]->n_type;
		if (ISFTN(DECREF(t)))
			t = LONG;
		switch (t) {
		case CHAR: case UCHAR:
			if (r0l) {
				if (r0h)
					break;
				rary[stk] = R2; /* char talk for 'R0H' */
				r0h = 1;
			} else {
				rary[stk] = R0;
				r0l = 1;
			}
			continue;

		case INT: case UNSIGNED:
			if (r0l || nch) {
				if (r2) {
					if (a0)
						break;
					rary[stk] = A0;
					a0 = 1;
				} else {
					rary[stk] = R2;
					r2 = 1;
				}
			} else {
				rary[stk] = R0;
				r0l = r0h = 1;
			}
			continue;

		case LONG: case ULONG:
			if (r0l || r2)
				break;
			rary[stk] = R0;
			r0l = r0h = r2 = 1;
			continue;

		default:
			if (ISPTR(narry[stk]->n_type) &&
			    !ISFTN(DECREF(narry[stk]->n_type))) {
				if (a0) {
					if (r0l || nch) {
						if (r2)
							break;
						rary[stk] = R2;
						r2 = 1;
					} else {
						rary[stk] = R0;
						r0l = r0h = 1;
					}
				} else {
					rary[stk] = A0;
					a0 = 1;
				}
				continue;
			}
			break;
		}
		break;
	}

	/*
	 * The arguments that must be on stack are stk->nn args.
	 * Argument 0->stk-1 should be put in the rary[] register.
	 */
	for (sz = 0, i = nn-1; i >= stk; i--) { /* first stack args */
		NODE nod;
		pass2_compile(ipnode(mkunode(FUNARG,
		    narry[i], 0, narry[i]->n_type)));
		nod.n_type = narry[i]->n_type;
		sz += tlen(&nod);
	}
	/* if param cannot be addressed directly, evaluate and put on stack */
	for (i = 0; i < stk; i++) {

		if (canaddr(narry[i]))
			continue;
		t = narry[i]->n_type;
		k = BITOOR(freetemp(szty(t)));
		n = mklnode(OREG, k, FB, t);
		q = tcopy(n);
		pass2_compile(ipnode(mkbinode(ASSIGN, n, narry[i], t)));
		narry[i] = q;
	}
	/* move args to registers */
	for (i = 0; i < stk; i++) {
		t = narry[i]->n_type;
		pass2_compile(ipnode(mkbinode(ASSIGN, 
		    mklnode(REG, 0, rary[i], t), narry[i], t)));
	}
	return sz;
}

/*
 * Tell if a register can hold a specific datatype.
 */
#if 0
int
mayuse(int reg, TWORD type)
{
	return 1;  /* Everything is OK */
}
#endif

#ifdef TAILCALL
void
mktailopt(struct interpass *ip1, struct interpass *ip2)
{
	extern int earlylab;
	extern char *cftname;
	char *fn;
	NODE *p;

	p = ip1->ip_node->n_left->n_left;
	if (p->n_op == ICON) {
		fn = p->n_name;
		/* calling ourselves */
		p = ip1->ip_node->n_left;
		if (p->n_op == CALL) {
			if (storearg(p->n_right))
				comperr("too many args: fix mktailopt");
			p->n_op = UCALL;
		}
		tfree(ip1->ip_node);
		p = ip2->ip_node->n_left;
		if (strcmp(fn, cftname)) {
			/* Not us, must generate fake prologue */
			ip1->type = IP_ASM;
			ip1->ip_asm = "mov.w FB,SP\n\tpop.w FB";
			pass2_compile(ip1);
			p->n_lval = p->n_rval = 0;
			p->n_name = fn;
		} else
			p->n_lval = earlylab;
	} else {
		pass2_compile(ip1);
	}
	pass2_compile(ip2);
}
#endif
