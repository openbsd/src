/*	$OpenBSD: local.c,v 1.4 2008/04/11 20:45:52 stefan Exp $	*/

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


#include "pass1.h"
#include "pass2.h"

/*	this file contains code which is dependent on the target machine */

struct symtab *makememcpy(void);

/* clocal() is called to do local transformations on
 * an expression tree preparitory to its being
 * written out in intermediate code.
 *
 * the major essential job is rewriting the
 * automatic variables and arguments in terms of
 * REG and OREG nodes
 * conversion ops which are not necessary are also clobbered here
 * in addition, any special features (such as rewriting
 * exclusive or) are easily handled here as well
 */
NODE *
clocal(NODE *p)
{
	register struct symtab *q, *sp;
	register NODE *r, *l, *s;
	register int o, m, rn;
	char *ch, name[16];
	TWORD t;

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal: %p\n", p);
		fwalk(p, eprint, 0);
	}
#endif
	switch (o = p->n_op) {

	case NAME:
		if ((q = p->n_sp) == NULL)
			break;	/* Nothing to care about */

		switch (q->sclass) {

		case PARAM:
			/* first four integral args are in regs */
			rn = (q->soffset >> 5) - 8;
			if (rn < 4) {
				r = block(REG, NIL, NIL, p->n_type, 0, 0);
				r->n_lval = 0;
				switch (p->n_type) {
				case FLOAT:
					r->n_rval = FR7L - rn;
					break;
				case DOUBLE:
				case LDOUBLE:
					r->n_rval = FR6 - rn;
					break;
				case LONGLONG:
				case ULONGLONG:
					r->n_rval = AD1 - rn / 2;
					break;
				default:
					r->n_rval = ARG0 - rn;
				}
				r->n_sue = p->n_sue;
				p->n_sue = NULL;
				nfree(p);
				p = r;
				break;
			}
			/* FALLTHROUGH */

		case AUTO:
			/* fake up a structure reference */
			r = block(REG, NIL, NIL, PTR+STRTY, 0, 0);
			r->n_lval = 0;
			r->n_rval = FPREG;
			p = stref(block(STREF, r, p, 0, 0, 0));
			break;

		case REGISTER:
			p->n_op = REG;
			p->n_lval = 0;
			p->n_rval = q->soffset;
			break;

		case STATIC:
		case EXTERN:
			if (strncmp(p->n_sp->soname, "__builtin", 9) == 0)
				break;

			l = block(REG, NIL, NIL, INT, 0, 0);
			l->n_lval = 0;
			l->n_rval = R1;
			l = block(ASSIGN, l, p, INT, 0, 0);
			r = xbcon(0, p->n_sp, INT);
			p = block(UMUL,
			    block(PLUS, l, r, INT, 0, 0),
			    NIL, p->n_type, p->n_df, p->n_sue);
			break;
		}
		break;

	case ADDROF:
		if (!ISFTN(p->n_left->n_type))
			break;

		l = block(REG, NIL, NIL, INT, 0, 0);
		l->n_lval = 0;
		l->n_rval = R1;
		l = block(ASSIGN, l, p->n_left, INT, 0, 0);
		r = xbcon(0, p->n_left->n_sp, INT);
		l = block(PLUS, l, r, p->n_type, p->n_df, p->n_sue);
		nfree(p);
		p = l;
		break;

	case CBRANCH:
		l = p->n_left;

		/*
		 * Remove unneccessary conversion ops.
		 */
		if (clogop(l->n_op) && l->n_left->n_op == SCONV) {
			if (coptype(l->n_op) != BITYPE)
				break;
			if (l->n_right->n_op == ICON) {
				r = l->n_left->n_left;
				if (r->n_type >= FLOAT && r->n_type <= LDOUBLE)
					break;
				/* Type must be correct */
				t = r->n_type;
				nfree(l->n_left);
				l->n_left = r;
				l->n_type = t;
				l->n_right->n_type = t;
			}
		}
		break;

	case PCONV:
		/* Remove redundant PCONV's. Be careful */
		l = p->n_left;
		if (l->n_op == ICON) {
			l->n_lval = (unsigned)l->n_lval;
			goto delp;
		}
		if (l->n_type < INT || l->n_type == LONGLONG || 
		    l->n_type == ULONGLONG) {
			/* float etc? */
			p->n_left = block(SCONV, l, NIL,
			    UNSIGNED, 0, MKSUE(UNSIGNED));
			break;
		}
		/* if left is SCONV, cannot remove */
		if (l->n_op == SCONV)
			break;

		/* avoid ADDROF TEMP */
		if (l->n_op == ADDROF && l->n_left->n_op == TEMP)
			break;

		/* if conversion to another pointer type, just remove */
		if (p->n_type > BTMASK && l->n_type > BTMASK)
			goto delp;
		break;

	delp:	l->n_type = p->n_type;
		l->n_qual = p->n_qual;
		l->n_df = p->n_df;
		l->n_sue = p->n_sue;
		nfree(p);
		p = l;
		break;
		
	case SCONV:
		l = p->n_left;

		if (p->n_type == l->n_type) {
			nfree(p);
			p = l;
			break;
		}

		if ((p->n_type & TMASK) == 0 && (l->n_type & TMASK) == 0 &&
		    btdims[p->n_type].suesize == btdims[l->n_type].suesize) {
			if (p->n_type != FLOAT && p->n_type != DOUBLE &&
			    l->n_type != FLOAT && l->n_type != DOUBLE &&
			    l->n_type != LDOUBLE && p->n_type != LDOUBLE) {
				if (l->n_op == UMUL || l->n_op == TEMP ||
				    l->n_op == NAME) {
					l->n_type = p->n_type;
					nfree(p);
					p = l;
					break;
				}
			}
		}

		if (DEUNSIGN(p->n_type) == INT && DEUNSIGN(l->n_type) == INT &&
		    coptype(l->n_op) == BITYPE) {
			l->n_type = p->n_type;
			nfree(p);
			p = l;
			break;
		}

		o = l->n_op;
		m = p->n_type;

		if (o == ICON) {
			CONSZ val = l->n_lval;

			if (!ISPTR(m)) /* Pointers don't need to be conv'd */
			    switch (m) {
			case BOOL:
				l->n_lval = l->n_lval != 0;
				break;
			case CHAR:
				l->n_lval = (char)val;
				break;
			case UCHAR:
				l->n_lval = val & 0377;
				break;
			case SHORT:
				l->n_lval = (short)val;
				break;
			case USHORT:
				l->n_lval = val & 0177777;
				break;
			case ULONG:
			case UNSIGNED:
				l->n_lval = val & 0xffffffff;
				break;
			case LONG:
			case INT:
				l->n_lval = (int)val;
				break;
			case LONGLONG:
				l->n_lval = (long long)val;
				break;
			case ULONGLONG:
				l->n_lval = val;
				break;
			case VOID:
				break;
			case LDOUBLE:
			case DOUBLE:
			case FLOAT:
				l->n_op = FCON;
				l->n_dcon = val;
				break;
			default:
				cerror("unknown type %d", m);
			}
			l->n_type = m;
			l->n_sue = MKSUE(m);
			nfree(p);
			return l;
                } else if (l->n_op == FCON) {
			l->n_lval = l->n_dcon;
			l->n_sp = NULL;
			l->n_op = ICON;
			l->n_type = m;
			l->n_sue = MKSUE(m);
			nfree(p);
			return clocal(l);
		}

		if (DEUNSIGN(p->n_type) == SHORT &&
		    DEUNSIGN(l->n_type) == SHORT) {
			nfree(p);
			p = l;
		}
		if ((p->n_type == CHAR || p->n_type == UCHAR ||
		    p->n_type == SHORT || p->n_type == USHORT) &&
		    (l->n_type == FLOAT || l->n_type == DOUBLE ||
		    l->n_type == LDOUBLE)) {
			p = block(SCONV, p, NIL, p->n_type, p->n_df, p->n_sue);
			p->n_left->n_type = INT;
			break;
		}
		break;

	case MOD:
	case DIV:
		if (o == DIV && p->n_type != CHAR && p->n_type != SHORT)
			break;
		if (o == MOD && p->n_type != CHAR && p->n_type != SHORT)
			break;
		/* make it an int division by inserting conversions */
		p->n_left = block(SCONV, p->n_left, NIL, INT, 0, MKSUE(INT));
		p->n_right = block(SCONV, p->n_right, NIL, INT, 0, MKSUE(INT));
		p = block(SCONV, p, NIL, p->n_type, 0, MKSUE(p->n_type));
		p->n_left->n_type = INT;
		break;

	case PMCONV:
	case PVCONV:
                if( p->n_right->n_op != ICON ) cerror( "bad conversion", 0);
                nfree(p);
                return(buildtree(o==PMCONV?MUL:DIV, p->n_left, p->n_right));

	case LS:
	case RS:
		/* shift count must be in an int */
		if (p->n_right->n_op == ICON || p->n_right->n_lval <= 32)
			break;	/* do not do anything */
		if (p->n_right->n_type != INT || p->n_right->n_lval > 32)
			p->n_right = block(SCONV, p->n_right, NIL,
			    INT, 0, MKSUE(INT));
		break;

#if 0
	case FLD:
		/* already rewritten (in ASSIGN) */
		if (p->n_left->n_op == TEMP)
			break;

		r = tempnode(0, p->n_type, p->n_df, p->n_sue);
		l = block(ASSIGN, r, p->n_left, p->n_type, p->n_df, p->n_sue);
		p->n_left = tcopy(r);
		p = block(COMOP, l, p, p->n_type, p->n_df, p->n_sue);
		break;
#endif

	case FORCE:
		/* put return value in return reg */
		p->n_op = ASSIGN;
		p->n_right = p->n_left;
		p->n_left = block(REG, NIL, NIL, p->n_type, p->n_df, p->n_sue);
		p->n_left->n_rval = p->n_left->n_type == BOOL ? 
		    RETREG(CHAR) : RETREG(p->n_type);
		if (p->n_right->n_op != FLD)
			break;
		break;

	case ASSIGN:
		r = p->n_right;
		l = p->n_left;

		/* rewrite ICON#0 into %r0 */
		if (r->n_op == ICON && r->n_lval == 0 &&
		    (l->n_op == REG || l->n_op == OREG)) {
			r->n_op = REG;
			r->n_rval = R0;
		}

		/* rewrite FCON#0 into %fr0 */
		if (r->n_op == FCON && r->n_lval == 0 && l->n_op == REG) {
			r->n_op = REG;
			r->n_rval = r->n_type == FLOAT? FR0L : FR0;
		}

		if (p->n_left->n_op != FLD)
			break;

		r = tempnode(0, l->n_type, l->n_df, l->n_sue);
		p = block(COMOP,
		    block(ASSIGN, r, l->n_left, l->n_type, l->n_df, l->n_sue),
		    p, p->n_type, p->n_df, p->n_sue);
		s = tcopy(l->n_left);
		p = block(COMOP, p,
		    block(ASSIGN, s, tcopy(r), l->n_type, l->n_df, l->n_sue),
		    p->n_type, p->n_df, p->n_sue);
		l->n_left = tcopy(r);
		break;

	case STASG:
		/* memcpy(left, right, size) */
		sp = makememcpy();
		l = p->n_left;
		/* guess struct return */
		if (l->n_op == NAME && ISFTN(l->n_sp->stype)) {
			l = block(REG, NIL, NIL, VOID|PTR, 0, MKSUE(LONG));
			l->n_lval = 0;
			l->n_rval = RET0;
		} else if (l->n_op == UMUL)
			l = tcopy(l->n_left);
		else if (l->n_op == NAME)
			l = block(ADDROF,tcopy(l),NIL,PTR|STRTY,0,MKSUE(LONG));
		l = block(CALL, block(ADDROF,
		    (s = block(NAME, NIL, NIL, FTN, 0, MKSUE(LONG))),
		    NIL, PTR|FTN, 0, MKSUE(LONG)),
		    block(CM, block(CM, l, tcopy(p->n_right),
		    STRTY|PTR, 0, MKSUE(LONG)),
		    (r = block(ICON, NIL, NIL, INT, 0, MKSUE(LONG))), 0, 0, 0),
		    INT, 0, MKSUE(LONG));
		r->n_lval = p->n_sue->suesize/SZCHAR;
		s->n_sp = sp;
		s->n_df = s->n_sp->sdf;
		defid(s, EXTERN);
		tfree(p);
		p = l;
		p->n_left = clocal(p->n_left);
		p->n_right = clocal(p->n_right);
		calldec(p->n_left, p->n_right);
		funcode(p);
		break;

	case STARG:
		/* arg = memcpy(argN-size, src, size) */
		sp = makememcpy();
		l = block(CALL, block(ADDROF,
		    (s = block(NAME, NIL, NIL, FTN, 0, MKSUE(LONG))),NIL,0,0,0),
		    block(CM, block(CM, tcopy(p), tcopy(p->n_left), 0, 0, 0),
		    (r = block(ICON, NIL, NIL, INT, 0, MKSUE(LONG))), 0, 0, 0),
		    INT, 0, MKSUE(LONG));
		r->n_lval = p->n_sue->suesize/SZCHAR;
		s->n_sp = sp;
		s->n_df = s->n_sp->sdf;
		defid(s, EXTERN);
		tfree(p);
		p = l;
		p->n_left = clocal(p->n_left);
		calldec(p->n_left, p->n_right);
		funcode(p);
		break;

	case STCALL:
	case CALL:
		for (r = p->n_right; r->n_op == CM; r = r->n_left) {
			if (r->n_right->n_op == ASSIGN &&
			    r->n_right->n_right->n_op == CALL) {
				s = r->n_right->n_right;
				l = tempnode(0, s->n_type, s->n_df, s->n_sue);
				ecode(buildtree(ASSIGN, l, s));
				r->n_right->n_right = tcopy(l);
			}
			if (r->n_left->n_op == ASSIGN &&
			    r->n_left->n_right->n_op == CALL) {
				s = r->n_left->n_right;
				l = tempnode(0, s->n_type, s->n_df, s->n_sue);
				ecode(buildtree(ASSIGN, l, s));
				r->n_left->n_right = tcopy(l);
			}
		}
		break;
	}

	/* second pass - rewrite long ops */
	switch (o) {
	case DIV:
	case MOD:
	case MUL:
	case RS:
	case LS:
		if (!(p->n_type == LONGLONG || p->n_type == ULONGLONG) ||
		    !((o == DIV || o == MOD || o == MUL) &&
		      p->n_type < FLOAT))
			break;
		if (o == DIV && p->n_type == ULONGLONG) ch = "udiv";
		else if (o == DIV) ch = "div";
		else if (o == MUL) ch = "mul";
		else if (o == MOD && p->n_type == ULONGLONG) ch = "umod";
		else if (o == MOD) ch = "mod";
		else if (o == RS && p->n_type == ULONGLONG) ch = "lshr";
		else if (o == RS) ch = "ashr";
		else if (o == LS) ch = "ashl";
		else break;
		snprintf(name, sizeof(name), "__%sdi3", ch);
		p->n_right = block(CM, p->n_left, p->n_right, 0, 0, 0);
		p->n_left = block(ADDROF,
		    block(NAME, NIL, NIL, FTN, 0, MKSUE(INT)), NIL,
		    PTR|FTN, 0, MKSUE(INT));
		p->n_left->n_left->n_sp = lookup(addname(name), 0);
		defid(p->n_left->n_left, EXTERN);
		p->n_left = clocal(p->n_left);
		calldec(p->n_left, p->n_right);
		p->n_op = CALL;
		funcode(p);
		break;
	}

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal end: %p\n", p);
		fwalk(p, eprint, 0);
	}
#endif
	return(p);
}

struct symtab *
makememcpy()
{
	NODE *memcpy, *args, *t, *u;
	struct symtab *sp;

	/* TODO check that it's a func proto */
	if ((sp = lookup(addname("memcpy"), SNORMAL)))
		return sp;

	memcpy = block(NAME, NIL, NIL, 0, 0, MKSUE(LONG));
	memcpy->n_sp = sp = lookup(addname("memcpy"), SNORMAL);
	defid(memcpy, EXTERN);

	args = block(CM, block(CM,
	    block(NAME, NIL, NIL, VOID|PTR, 0, MKSUE(LONG)),
	    block(NAME, NIL, NIL, VOID|PTR, 0, MKSUE(LONG)), 0, 0, 0),
	    block(NAME, NIL, NIL, LONG, 0, MKSUE(LONG)), 0, 0, 0);

	tymerge(t = block(TYPE, NIL, NIL, VOID|PTR, 0, 0),
	    (u = block(UMUL, block(CALL, memcpy, args, LONG, 0, 0),
	    NIL, LONG, 0, 0)));
	tfree(t);
	tfree(u);

	return sp;
}

void
myp2tree(NODE *p)
{
	int o = p->n_op;

	if (o != FCON) 
		return;

	/* Write float constants to memory */
	/* Should be volontary per architecture */
 
#if 0
	setloc1(RDATA);
	defalign(p->n_type == FLOAT ? ALFLOAT : p->n_type == DOUBLE ?
	    ALDOUBLE : ALLDOUBLE );
	deflab1(i = getlab()); 
#endif

	ninval(0, btdims[p->n_type].suesize, p);
	p->n_op = NAME;
	p->n_lval = 0;	
	p->n_sp = tmpalloc(sizeof(struct symtab_hdr));
	p->n_sp->sclass = ILABEL;
	p->n_sp->soffset = getlab();
	p->n_sp->sflags = 0;

}

/*ARGSUSED*/
int
andable(NODE *p)
{
	return(1);  /* all names can have & taken on them */
}

/*
 * at the end of the arguments of a ftn, set the automatic offset
 */
void
cendarg()
{
	/* TODO can use to generate sp/rp tree ops? */
	autooff = AUTOINIT;
}

/*
 * Return 1 if a variable of type "t" is OK to put in register.
 */
int
cisreg(TWORD t)
{
	return 1;
}

/*
 * return a node, for structure references, which is suitable for
 * being added to a pointer of type t, in order to be off bits offset
 * into a structure
 * t, d, and s are the type, dimension offset, and sizeoffset
 * For pdp10, return the type-specific index number which calculation
 * is based on its size. For example, short a[3] would return 3.
 * Be careful about only handling first-level pointers, the following
 * indirections must be fullword.
 */
NODE *
offcon(OFFSZ off, TWORD t, union dimfun *d, struct suedef *sue)
{
	register NODE *p;

	if (xdebug)
		printf("offcon: OFFSZ %lld type %x dim %p siz %d\n",
		    off, t, d, sue->suesize);

	p = bcon(0);
	p->n_lval = off/SZCHAR;	/* Default */
	return(p);
}

/*
 * Allocate off bits on the stack.  p is a tree that when evaluated
 * is the multiply count for off, t is a storeable node where to write
 * the allocated address.
 */
void
spalloc(NODE *t, NODE *p, OFFSZ off)
{
	NODE *sp;

	p = buildtree(MUL, p, bcon(off/SZCHAR)); /* XXX word alignment? */

	/* sub the size from sp */
	sp = block(REG, NIL, NIL, p->n_type, 0, MKSUE(INT));
	sp->n_lval = 0;
	sp->n_rval = STKREG;
	ecomp(buildtree(PLUSEQ, sp, p));

	/* save the address of sp */
	sp = block(REG, NIL, NIL, PTR+INT, t->n_df, t->n_sue);
	sp->n_lval = 0;
	sp->n_rval = STKREG;
	t->n_type = sp->n_type;
	ecomp(buildtree(ASSIGN, t, sp)); /* Emit! */

}

#if 0
/*
 * Print out an integer constant of size size.
 * can only be sizes <= SZINT.
 */
void
indata(CONSZ val, int size)
{
	switch (size) {
	case SZCHAR:
		printf("\t.byte %d\n", (int)val & 0xff);
		break;
	case SZSHORT:
		printf("\t.word %d\n", (int)val & 0xffff);
		break;
	case SZINT:
		printf("\t.long %d\n", (int)val & 0xffffffff);
		break;
	default:
		cerror("indata");
	}
}
#endif

/*
 * Print out a string of characters.
 * Assume that the assembler understands C-style escape
 * sequences.  Location is already set.
 */
void
instring(struct symtab *sp)
{
	char *s, *str;

	defloc(sp);
	str = sp->sname;

	/* be kind to assemblers and avoid long strings */
	printf("\t.ascii\t\"");
	for (s = str; *s != 0; ) {
		if (*s++ == '\\') {
			(void)esccon(&s);
		}
		if (s - str > 64) {
			fwrite(str, 1, s - str, stdout);
			printf("\"\n\t.ascii\t\"");
			str = s;
		}
	}
	fwrite(str, 1, s - str, stdout);
	printf("\\0\"\n");
}

/*
 * Print out a wide string by calling ninval().
 */
void
inwstring(struct symtab *sp)
{
	char *s = sp->sname;
	NODE *p;

	defloc(sp);
	p = bcon(0);
	do { 
		if (*s++ == '\\')
			p->n_lval = esccon(&s);
		else
			p->n_lval = (unsigned char)s[-1];
		ninval(0, (MKSUE(WCHAR_TYPE))->suesize, p);
	} while (s[-1] != 0);
	nfree(p);
}

static int inbits, inval;

/*
 * set fsz bits in sequence to zero.
 */
void
zbits(OFFSZ off, int fsz)
{
	int m;

	if (idebug)
		printf("zbits off %lld, fsz %d inbits %d\n", off, fsz, inbits);
	if ((m = (inbits % SZCHAR))) {
		m = SZCHAR - m;
		if (fsz < m) {
			inbits += fsz;
			return;
		} else {
			fsz -= m;
			printf("\t.byte %d\n", inval);
			inval = inbits = 0;
		}
	}
	if (fsz >= SZCHAR) {
		printf("\t.zero %d\n", fsz/SZCHAR);
		fsz -= (fsz/SZCHAR) * SZCHAR;
	}
	if (fsz) {
		inval = 0;
		inbits = fsz;
	}
}

/*
 * Initialize a bitfield.
 */
void
infld(CONSZ off, int fsz, CONSZ val)
{
	if (idebug)
		printf("infld off %lld, fsz %d, val %lld inbits %d\n",
		    off, fsz, val, inbits);
	val &= (1 << fsz)-1;
	while (fsz + inbits >= SZCHAR) {
		inval |= (val << inbits);
		printf("\t.byte %d\n", inval & 255);
		fsz -= (SZCHAR - inbits);
		val >>= (SZCHAR - inbits);
		inval = inbits = 0;
	}
	if (fsz) {
		inval |= (val << inbits);
		inbits += fsz;
	}
}

/*
 * print out a constant node, may be associated with a label.
 * Do not free the node after use.
 * off is bit offset from the beginning of the aggregate
 * fsz is the number of bits this is referring to
 *
 * XXX this relies on the host fp numbers representation
 */
void
ninval(CONSZ off, int fsz, NODE *p)
{
	union { float f; double d; long double l; int i[3]; } u;
	struct symtab *q;
	TWORD t;
	int i;

	t = p->n_type;
	if (t > BTMASK)
		t = INT; /* pointer */

	if (p->n_op != ICON && p->n_op != FCON)
		cerror("ninval: init node not constant");

	if (p->n_op == ICON && p->n_sp != NULL && DEUNSIGN(t) != INT)
		uerror("element not constant");

	switch (t) {
	case LONGLONG:
	case ULONGLONG:
		i = (p->n_lval >> 32);
		p->n_lval &= 0xffffffff;
		p->n_type = INT;
		ninval(off, 32, p);
		p->n_lval = i;
		ninval(off+32, 32, p);
		break;
	case INT:
	case UNSIGNED:
		printf("\t.long 0x%x", (int)p->n_lval);
		if ((q = p->n_sp) != NULL) {
			if ((q->sclass == STATIC && q->slevel > 0) ||
			    q->sclass == ILABEL) {
				printf("+" LABFMT, q->soffset);
			} else
				printf("+%s", exname(q->soname));
		}
		printf("\n");
		break;
	case SHORT:
	case USHORT:
		printf("\t.short 0x%x\n", (int)p->n_lval & 0xffff);
		break;
	case BOOL:
		if (p->n_lval > 1)
			p->n_lval = p->n_lval != 0;
		/* FALLTHROUGH */
	case CHAR:
	case UCHAR:
		/* TODO make the upper layer give an .asciz */
		printf("\t.byte %d\n", (int)p->n_lval & 0xff);
		break;
	case LDOUBLE:
	case DOUBLE:
		u.d = (double)p->n_dcon;
		printf("\t.long\t0x%x,0x%x\n", u.i[0], u.i[1]);
		break;
	case FLOAT:
		u.f = (float)p->n_dcon;
		printf("\t.long\t0x%x\n", u.i[0]);
		break;
	default:
		cerror("ninval");
	}
}

/* make a name look like an external name in the local machine */
char *
exname(char *p)
{
	if (p == NULL)
		return "";
	return p;
}

/*
 * map types which are not defined on the local machine
 */
TWORD
ctype(TWORD type)
{
	switch (BTYPE(type)) {
	case LONG:
		MODTYPE(type,INT);
		break;

	case ULONG:
		MODTYPE(type,UNSIGNED);
	}

	return (type);
}

void
calldec(NODE *f, NODE *a) 
{
	struct symtab *q;
	if (f->n_op == UMUL && f->n_left->n_op == PLUS &&
	    f->n_left->n_right->n_op == ICON)
		q = f->n_left->n_right->n_sp;
	else if (f->n_op == PLUS && f->n_right->n_op == ICON)
		q = f->n_right->n_sp;
	else {
		fwalk(f, eprint, 0);
		cerror("unknown function");
		return;
	}

	printf("\t.import\t%s,code\n", exname(q->soname));
}

void
extdec(struct symtab *q)
{
	printf("\t.import\t%s,data\n", exname(q->soname));
}

/* make a common declaration for id, if reasonable */
void
defzero(struct symtab *sp)
{
	int off;

	off = tsize(sp->stype, sp->sdf, sp->ssue);
	off = (off + (SZCHAR - 1)) / SZCHAR;
	printf("\t.%scomm\t", sp->sclass == STATIC ? "l" : "");
	if (sp->slevel == 0)
		printf("%s,0%o\n", exname(sp->soname), off);
	else
		printf(LABFMT ",0%o\n", sp->soffset, off);
}

char *nextsect;

#define	SSECTION	010000

/*
 * Give target the opportunity of handling pragmas.
 */
int
mypragma(char **ary)
{
	if (strcmp(ary[1], "section") || ary[2] == NULL)
		return 0;
	nextsect = newstring(ary[2], strlen(ary[2]));
	return 1;
}

/*
 * Called when a identifier has been declared, to give target last word.
 */
void
fixdef(struct symtab *sp)
{
}

