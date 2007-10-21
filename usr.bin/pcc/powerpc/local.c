/*	$OpenBSD: local.c,v 1.2 2007/10/21 17:41:06 otto Exp $	*/
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

#include "pass1.h"

extern int kflag;

static
void simmod(NODE *p);

/*	this file contains code which is dependent on the target machine */

/* don't permit CALLs to be arguments to other CALLs (nest CALLs) */

static NODE *
emitinnercall(NODE *r)
{
	NODE *tmp1, *tmp2;

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal::emitinnercall():\n");
		fwalk(r, eprint, 0);
	}
#endif

	tmp1 = tempnode(0, r->n_type, r->n_df, r->n_sue);
	tmp2 = tempnode(tmp1->n_lval, r->n_type, r->n_df, r->n_sue);
	ecode(buildtree(ASSIGN, tmp1, r));

	return tmp2;
}
 
static void
breaknestedcalls(NODE *p)
{
	NODE *r;

	for (r = p->n_right; r->n_op == CM; r = r->n_left) {
		if (r->n_right->n_op == CALL)
			r->n_right = emitinnercall(r->n_right);
		if (r->n_left->n_op == CALL)
			r->n_left = emitinnercall(r->n_left);
	}

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal::breaknestedcalls(): exiting with tree:\n");
		fwalk(p, eprint, 0);
	}
#endif
}

static void
fixupfuncargs(NODE *r, int *reg)
{
#ifdef PCC_DEBUG
	if (xdebug)
		printf("fixupfuncargs(): r=%p\n", r);
#endif

	if (r->n_op == CM) {
		/* recurse to the bottom of the tree */
		fixupfuncargs(r->n_left, reg);

		r->n_right = block(ASSIGN, NIL, r->n_right, 
			    r->n_right->n_type, r->n_right->n_df,
			    r->n_right->n_sue);
		r->n_right->n_left = block(REG, NIL, NIL, r->n_right->n_type, 0, MKSUE(INT));

		if (r->n_right->n_type == ULONGLONG || r->n_right->n_type == LONGLONG) {
			r->n_right->n_left->n_rval = R3R4 + (*reg) - R3;
			(*reg) += 2;
		} else {
			r->n_right->n_left->n_rval = (*reg);
			(*reg)++;
		}

	} else {
		NODE *l = talloc();
		*l = *r;
		r->n_op = ASSIGN;
		r->n_left = block(REG, NIL, NIL, l->n_type, 0, MKSUE(INT));
		r->n_right = l;
		if (r->n_right->n_type == ULONGLONG || r->n_right->n_type == LONGLONG) {
			r->n_left->n_rval = R3R4 + (*reg) - R3;
			(*reg) += 2;
		} else {
			r->n_left->n_rval = (*reg);
			(*reg)++;
		}
	}
}

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

	struct symtab *q;
	NODE *r, *l;
	int o;
	int m;
	TWORD t;

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal: %p\n", p);
		fwalk(p, eprint, 0);
	}
#endif
	switch (o = p->n_op) {

#if 1
	case ADDROF:
#ifdef PCC_DEBUG
		if (xdebug) {
			printf("clocal(): ADDROF\n");
			printf("type: 0x%x\n", p->n_type);
		}
#endif

		if (kflag && p->n_left->n_op == NAME) {

			TWORD t = DECREF(p->n_type);

			if (!ISFTN(t)) {
				r  = talloc();
				*r = *p;

				l = block(REG, NIL, NIL, INT, p->n_df, p->n_sue);
				l->n_lval = 0;
				l->n_rval = R31;

				p->n_op = PLUS;
				p->n_lval = 0;
				p->n_left = l;
				p->n_rval = 0;
				p->n_right = r;
			}

		}
		break;
#endif

	case NAME:
		if ((q = p->n_sp) == NULL)
			return p; /* Nothing to care about */

		switch (q->sclass) {

		case PARAM:
		case AUTO:
			/* fake up a structure reference */
			r = block(REG, NIL, NIL, PTR+STRTY, 0, 0);
			r->n_lval = 0;
			r->n_rval = FPREG;
			p = stref(block(STREF, r, p, 0, 0, 0));
			break;

		case STATIC:
			if (q->slevel == 0)
				break;
			p->n_lval = 0;
			p->n_sp = q;
			break;

		case REGISTER:
			p->n_op = REG;
			p->n_lval = 0;
			p->n_rval = q->soffset;
			break;

#if 1
		default:
			if (kflag && !ISFTN(p->n_type)) {

			TWORD t = p->n_type;
			l = block(REG, NIL, NIL, INCREF(t), p->n_df, p->n_sue);
			l->n_lval = 0;
			l->n_rval = R31;

			p->n_op = ICON;
			p->n_type = INCREF(p->n_type);

			p = block(PLUS, l, p, INCREF(t), p->n_df, p->n_sue);
			p = block(UMUL, p, NIL, t, p->n_df, p->n_sue);
			}
#endif

		}
		break;

	case STCALL:
	case CALL:
		{

		/* break nested CALLs */
		breaknestedcalls(p);

		/* Fix function call arguments. */
		/* move everything into the registers */
		/* XXX have to save the old values of R3-R10 on the stack
			(but only once per function */
		{
		int reg = R3;
		fixupfuncargs(p->n_right, &reg);
		}

#if 0
		for (r = p->n_right; r->n_op == CM; r = r->n_left) {
			if (r->n_right->n_op != STARG &&
			    r->n_right->n_op != FUNARG) {
				printf("HERE\n");
				r->n_right = block(FUNARG, r->n_right, NIL, 
				    r->n_right->n_type, r->n_right->n_df,
				    r->n_right->n_sue);
				}
		}
		if (r->n_op != STARG && r->n_op != FUNARG) {
			printf("HERE2\n");
			l = talloc();
			*l = *r;
			r->n_op = FUNARG; r->n_left = l; r->n_type = l->n_type;
		}
#endif
		break;
		}
		
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
			return l;
		}

		if (xdebug)
			printf("SCONV 1\n");

		if ((p->n_type & TMASK) == 0 && (l->n_type & TMASK) == 0 &&
		    btdims[p->n_type].suesize == btdims[l->n_type].suesize) {
			if (p->n_type != FLOAT && p->n_type != DOUBLE &&
			    l->n_type != FLOAT && l->n_type != DOUBLE &&
			    l->n_type != LDOUBLE && p->n_type != LDOUBLE) {
				if (l->n_op == NAME || l->n_op == UMUL ||
				    l->n_op == TEMP) {
					l->n_type = p->n_type;
					nfree(p);
					return l;
				}
			}
		}

		if (DEUNSIGN(p->n_type) == INT && DEUNSIGN(l->n_type) == INT &&
		    coptype(l->n_op) == BITYPE) {
			l->n_type = p->n_type;
			nfree(p);
			return l;
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
			case ENUMTY:
			case MOETY:
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
			return p;
		}
		break;

	case MOD:
		simmod(p);
		break;

	case DIV:
		if (o == DIV && p->n_type != CHAR && p->n_type != SHORT)
			break;
		/* make it an int division by inserting conversions */
		p->n_left = block(SCONV, p->n_left, NIL, INT, 0, MKSUE(INT));
		p->n_right = block(SCONV, p->n_right, NIL, INT, 0, MKSUE(INT));
		p = block(SCONV, p, NIL, p->n_type, 0, MKSUE(p->n_type));
		p->n_left->n_type = INT;
		break;

	case PMCONV:
	case PVCONV:
                
                nfree(p);
                return(buildtree(o==PMCONV?MUL:DIV, p->n_left, p->n_right));

	case FORCE:
		/* put return value in return reg */
		p->n_op = ASSIGN;
		p->n_right = p->n_left;
		p->n_left = block(REG, NIL, NIL, p->n_type, 0, MKSUE(INT));
		p->n_left->n_rval = p->n_left->n_type == BOOL ? 
		    RETREG(CHAR) : RETREG(p->n_type);
		break;

	case LS:
	case RS:
		/* unless longlong, where it must be int */
		if (p->n_right->n_op == ICON)
			break; /* do not do anything */
		if (p->n_type == LONGLONG || p->n_type == ULONGLONG) {
			if (p->n_right->n_type != INT)
				p->n_right = block(SCONV, p->n_right, NIL,
				    INT, 0, MKSUE(INT));
		}
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

void
myp2tree(NODE *p)
{
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
#ifdef PCC_DEBUG
	if (xdebug)
		printf("cendarg: autooff=%d (was %d)\n", AUTOINIT, autooff);
#endif
	autooff = AUTOINIT;
}

/*
 * Return 1 if a variable of type type is OK to put in register.
 */
int
cisreg(TWORD t)
{
	if (t == FLOAT || t == DOUBLE || t == LDOUBLE)
		return 0; /* not yet */
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

#ifdef PCC_DEBUG
	if (xdebug)
		printf("offcon: OFFSZ %lld type %x dim %p siz %d\n",
		    off, t, d, sue->suesize);
#endif

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
#if 0
	NODE *sp;
#endif

	p = buildtree(MUL, p, bcon(off/SZCHAR)); /* XXX word alignment? */

	assert(0);
#if 0
	/* sub the size from sp */
	sp = block(REG, NIL, NIL, p->n_type, 0, MKSUE(INT));
	sp->n_lval = 0;
	sp->n_rval = STKREG;
	ecomp(buildtree(MINUSEQ, sp, p));

	/* save the address of sp */
	sp = block(REG, NIL, NIL, PTR+INT, t->n_df, t->n_sue);
	sp->n_lval = 0;
	sp->n_rval = STKREG;
	t->n_type = sp->n_type;
	ecomp(buildtree(ASSIGN, t, sp)); /* Emit! */
#endif

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

#if 0
/*
 * Print out a string of characters.
 * Assume that the assembler understands C-style escape
 * sequences.  Location is already set.
 */
void
instring(char *str)
{
	char *s;

	/* be kind to assemblers and avoid long strings */
	printf("\t.ascii \"");
	for (s = str; *s != 0; ) {
		if (*s++ == '\\') {
			(void)esccon(&s);
		}
		if (s - str > 64) {
			fwrite(str, 1, s - str, stdout);
			printf("\"\n\t.ascii \"");
			str = s;
		}
	}
	fwrite(str, 1, s - str, stdout);
	printf("\\0\"\n");
}
#endif

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
				printf("+%s", exname(q->sname));
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
		printf("\t.byte %d\n", (int)p->n_lval & 0xff);
		break;
	case LDOUBLE:
		u.i[2] = 0;
		u.l = (long double)p->n_dcon;
		printf("\t.long\t0x%x,0x%x,0x%x\n", u.i[0], u.i[1], u.i[2]);
		break;
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

#if 0
/*
 * print out an integer.
 */
void
inval(CONSZ word)
{
	word &= 0xffffffff;
	printf("	.long 0x%llx\n", word);
}

/* output code to initialize a floating point value */
/* the proper alignment has been obtained */
void
finval(NODE *p)
{
	union { float f; double d; long double l; int i[3]; } u;

	switch (p->n_type) {
	case LDOUBLE:
		u.i[2] = 0;
		u.l = (long double)p->n_dcon;
		printf("\t.long\t0x%x,0x%x,0x%x\n", u.i[0], u.i[1], u.i[2]);
		break;
	case DOUBLE:
		u.d = (double)p->n_dcon;
		printf("\t.long\t0x%x,0x%x\n", u.i[0], u.i[1]);
		break;
	case FLOAT:
		u.f = (float)p->n_dcon;
		printf("\t.long\t0x%x\n", u.i[0]);
		break;
	}
}
#endif

/* make a name look like an external name in the local machine */
char *
exname(char *p)
{
#if 0
#define NCHNAM	256
        static char text[NCHNAM+1];
	int i;

	if (p == NULL)
		return "";

        text[0] = '_';
        for (i=1; *p && i<NCHNAM; ++i)
                text[i] = *p++;

        text[i] = '\0';
        text[NCHNAM] = '\0';  /* truncate */
#endif
        return (p);
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
calldec(NODE *p, NODE *q) 
{
#ifdef PCC_DEBUG
	if (xdebug)
		printf("calldec:\n");
#endif
}

void
extdec(struct symtab *q)
{
#ifdef PCC_DEBUG
	if (xdebug)
		printf("extdec:\n");
#endif
}

/* make a common declaration for id, if reasonable */
void
commdec(struct symtab *q)
{
	int off;

	off = tsize(q->stype, q->sdf, q->ssue);
	off = (off+(SZCHAR-1))/SZCHAR;
#ifdef GCC_COMPAT
	printf("	.comm %s,0%o\n", exname(gcc_findname(q)), off);
#else
	printf("	.comm %s,0%o\n", exname(q->sname), off);
#endif
}

/* make a local common declaration for id, if reasonable */
void
lcommdec(struct symtab *q)
{
	int off;

	off = tsize(q->stype, q->sdf, q->ssue);
	off = (off+(SZCHAR-1))/SZCHAR;
	if (q->slevel == 0)
#ifdef GCC_COMPAT
		printf("	.lcomm %s,0%o\n", exname(gcc_findname(q)), off);
#else
		printf("	.lcomm %s,0%o\n", exname(q->sname), off);
#endif
	else
		printf("	.lcomm " LABFMT ",0%o\n", q->soffset, off);
}

/*
 * print a (non-prog) label.
 */
void
deflab1(int label)
{
	printf(LABFMT ":\n", label);
}

static char *loctbl[] = { "text", "data", "section .rodata,", "cstring" };

void
setloc1(int locc)
{
#ifdef PCC_DEBUG
	if (xdebug)
		printf("setloc1: locc=%d, lastloc=%d\n", locc, lastloc);
#endif

	if (locc == lastloc)
		return;
	lastloc = locc;
	printf("	.%s\n", loctbl[locc]);
}

#if 0
int
ftoint(NODE *p, CONSZ **c)
{
	static CONSZ cc[3];
	union { float f; double d; long double l; int i[3]; } u;
	int n;

	switch (p->n_type) {
	case LDOUBLE:
		u.i[2] = 0;
		u.l = (long double)p->n_dcon;
		n = SZLDOUBLE;
		break;
	case DOUBLE:
		u.d = (double)p->n_dcon;
		n = SZDOUBLE;
		break;
	case FLOAT:
		u.f = (float)p->n_dcon;
		n = SZFLOAT;
		break;
	}
	cc[0] = u.i[0];
	cc[1] = u.i[1];
	cc[2] = u.i[2];
	*c = cc;
	return n;
}
#endif

/* simulate and optimise the MOD opcode */
static void
simmod(NODE *p)
{
	NODE *r = p->n_right;

	assert(p->n_op == MOD);

#define ISPOW2(n) ((n) && (((n)&((n)-1)) == 0))

	// if the right is a constant power of two, then replace with AND
	if (r->n_op == ICON && ISPOW2(r->n_lval)) {
		p->n_op = AND;
		r->n_lval--;
		return;
	}

#undef ISPOW2

	/* other optimizations can go here */
	
}
