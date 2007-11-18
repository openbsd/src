/*	$OpenBSD: local.c,v 1.3 2007/11/18 17:39:55 ragge Exp $	*/
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

#include "pass1.h"

static int inbits, inval;

#ifdef MIPS_BIGENDIAN
/*
 * If we're big endian, then all OREG loads of a type
 * larger than the destination, must have the
 * offset changed to point to the correct bytes in memory.
 */
static NODE *
offchg(NODE *p)
{
	NODE *l = p->n_left;

	if (p->n_op != SCONV)
		return;

	switch (l->n_type) {
	case SHORT:
	case USHORT:
		if (DEUNSIGN(p->n_type) == CHAR)
			l->n_lval += 1;
		break;
	case LONG:
	case ULONG:
	case INT:
	case UNSIGNED:
		if (DEUNSIGN(p->n_type) == CHAR)
			l->n_lval += 3;
		else if (DEUNSIGNED(p->n_type) == SHORT)
			l->n_lval += 2;
		break;
	case LONGLONG:
	case ULONGLONG:
		if (DEUNSIGN(p->n_type) == CHAR)
			l->n_lval += 7;
		else if (DEUNSIGNED(p->n_type) == SHORT)
			l->n_lval += 6;
		else if (DEUNSIGN(p->n_type) == INT ||
		    DEUNSIGN(p->n_type) == LONG)
			p->n_lval += 4;
	default:
		comperr("offchg: unknown type");
		break;
	}

	return p;
}
#endif

/* this is called to do local transformations on
 * an expression tree preparitory to its being
 * written out in intermediate code.
 */
NODE *
clocal(NODE *p)
{
	struct symtab *q;
	NODE *r, *l;
	int o;
	int m, ml;
	TWORD t;

//printf("in:\n");
//fwalk(p, eprint, 0);

	switch( o = p->n_op ){

	case NAME:
		if ((q = p->n_sp) == NULL)
			return p; /* Nothing to care about */

		switch (q->sclass) {

		case PARAM:
		case AUTO:
			/* fake up a structure reference */
			r = block(REG, NIL, NIL, PTR+STRTY, 0, 0);
			r->n_lval = 0;
			r->n_rval = FP;
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

			}
		break;

	case FUNARG:
		/* Args smaller than int are given as int */
		if (p->n_type != CHAR && p->n_type != UCHAR && 
		    p->n_type != SHORT && p->n_type != USHORT)
			break;
		p->n_left = block(SCONV, p->n_left, NIL, INT, 0, MKSUE(INT));
		p->n_type = INT;
		p->n_sue = MKSUE(INT);
		p->n_rval = SZINT;
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
#if 0
			  else if (l->n_right->n_op == SCONV &&
			    l->n_left->n_type == l->n_right->n_type) {
				r = l->n_left->n_left;
				nfree(l->n_left);
				l->n_left = r;
				r = l->n_right->n_left;
				nfree(l->n_right);
				l->n_right = r;
			}
#endif
		}
		break;

	case PCONV:
		ml = p->n_left->n_type;
		l = p->n_left;
		if ((ml == CHAR || ml == UCHAR || ml == SHORT || ml == USHORT)
		    && l->n_op != ICON)
			break;
		l->n_type = p->n_type;
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

#ifdef MIPS_BIGENDIAN
		/*
		 * If we're big endian, then all OREG loads of a type
		 * larger than the destination, must have the
		 * offset changed to point to the correct bytes in memory.
		 */
		if (l->n_type == OREG) 
			p = offchg(p);
#endif

		if ((p->n_type & TMASK) == 0 && (l->n_type & TMASK) == 0 &&
		    btdims[p->n_type].suesize == btdims[l->n_type].suesize) {
			if (p->n_type != FLOAT && p->n_type != DOUBLE &&
			    l->n_type != FLOAT && l->n_type != DOUBLE &&
			    l->n_type != LDOUBLE && p->n_type != LDOUBLE) {
				if (l->n_op == NAME || l->n_op == UMUL) {
					l->n_type = p->n_type;
					nfree(p);
					return l;
				}
			}
		}

		if ((p->n_type == INT || p->n_type == UNSIGNED) &&
		    ISPTR(l->n_type)) {
			nfree(p);
			return l;
		}

		o = l->n_op;
		m = p->n_type;

		if (o == ICON) {
			CONSZ val = l->n_lval;

			if (!ISPTR(m)) /* Pointers don't need to be conv'd */
			    switch (m) {
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
			nfree(p);
			return l;
		}
		if (DEUNSIGN(p->n_type) == SHORT &&
		    DEUNSIGN(l->n_type) == SHORT) {
			nfree(p);
			p = l;
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

	case FORCE:
		/* put return value in return reg */
		p->n_op = ASSIGN;
		p->n_right = p->n_left;
		p->n_left = block(REG, NIL, NIL, p->n_type, 0, MKSUE(INT));
		p->n_left->n_rval = RETREG(p->n_type);
		break;
	}

//printf("ut:\n");
//fwalk(p, eprint, 0);

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
	autooff = AUTOINIT;
}

/*
 * is an automatic variable of type t OK for a register variable
 */
int
cisreg(TWORD t)
{
	if (t == INT || t == UNSIGNED || t == LONG || t == ULONG)
		return(1);
	return 0; /* XXX - fix reg assignment in pftn.c */
}

/*
 * return a node, for structure references, which is suitable for
 * being added to a pointer of type t, in order to be off bits offset
 * into a structure
 * t, d, and s are the type, dimension offset, and sizeoffset
 * Be careful about only handling first-level pointers, the following
 * indirections must be fullword.
 */
NODE *
offcon(OFFSZ off, TWORD t, union dimfun *d, struct suedef *sue)
{
	NODE *p;

	if (xdebug)
		printf("offcon: OFFSZ %lld type %x dim %p siz %d\n",
		    off, t, d, sue->suesize);

	p = bcon(off/SZCHAR);
	return p;
}

/*
 * Allocate off bits on the stack.  p is a tree that when evaluated
 * is the multiply count for off, t is a NAME node where to write
 * the allocated address.
 */
void
spalloc(NODE *t, NODE *p, OFFSZ off)
{
	NODE *sp;

	if ((off % SZINT) == 0)
		p =  buildtree(MUL, p, bcon(off/SZINT));
	else if ((off % SZSHORT) == 0) {
		p = buildtree(MUL, p, bcon(off/SZSHORT));
		p = buildtree(PLUS, p, bcon(1));
		p = buildtree(RS, p, bcon(1));
	} else if ((off % SZCHAR) == 0) {
		p = buildtree(MUL, p, bcon(off/SZCHAR));
		p = buildtree(PLUS, p, bcon(3));
		p = buildtree(RS, p, bcon(2));
	} else
		cerror("roundsp");

	/* save the address of sp */
	sp = block(REG, NIL, NIL, PTR+INT, t->n_df, t->n_sue);
	sp->n_lval = 0;
	sp->n_rval = STKREG;
	t->n_type = sp->n_type;
	ecomp(buildtree(ASSIGN, t, sp)); /* Emit! */

	/* add the size to sp */
	sp = block(REG, NIL, NIL, p->n_type, 0, 0);
	sp->n_lval = 0;
	sp->n_rval = STKREG;
	ecomp(buildtree(PLUSEQ, sp, p));
}

/*
 * print out a constant node
 * mat be associated with a label
 */
void
ninval(CONSZ off, int fsz, NODE *p)
{
        union { float f; double d; long double l; int i[3]; } u;
        struct symtab *q;
        TWORD t;
#ifndef USE_GAS
        int i;
#endif

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
#ifdef USE_GAS
                printf("\t.dword 0x%llx\n", (long long)p->n_lval);
#else
                i = (p->n_lval >> 32);
                p->n_lval &= 0xffffffff;
                p->n_type = INT;
                ninval(off, 32, p);
                p->n_lval = i;
                ninval(off+32, 32, p);
#endif
                break;
        case BOOL:
                if (p->n_lval > 1)
                        p->n_lval = p->n_lval != 0;
                /* FALLTHROUGH */
        case INT:
        case UNSIGNED:
                printf("\t.word 0x%x", (int)p->n_lval);
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
                printf("\t.half 0x%x\n", (int)p->n_lval & 0xffff);
                break;
        case CHAR:
        case UCHAR:
                printf("\t.byte %d\n", (int)p->n_lval & 0xff);
                break;
        case LDOUBLE:
                u.i[2] = 0;
                u.l = (long double)p->n_dcon;
                printf("\t.word\t0x%x,0x%x,0x%x\n", u.i[0], u.i[1], u.i[2]);
                break;
        case DOUBLE:
                u.d = (double)p->n_dcon;
                printf("\t.word\t0x%x\n", u.i[0]);
                printf("\t.word\t0x%x\n", u.i[1]);
                break;
        case FLOAT:
                u.f = (float)p->n_dcon;
                printf("\t.word\t0x%x\n", u.i[0]);
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

/* curid is a variable which is defined but
 * is not initialized (and not a function );
 * This routine returns the storage class for an uninitialized declaration
 */
int
noinit()
{
	return(EXTERN);
}

void
calldec(NODE *p, NODE *q) 
{
}

void
extdec(struct symtab *q)
{
}

/* make a common declaration for id, if reasonable */
void
commdec(struct symtab *q)
{
	int off;

	off = tsize(q->stype, q->sdf, q->ssue);
	off = (off+(SZCHAR-1))/SZCHAR;

#ifdef GCC_COMPAT
	printf("	.comm %s,0%o\n", gcc_findname(q), off);
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
		printf("\t.lcomm %s,0%o\n", gcc_findname(q), off);
#else
		printf("\t.lcomm %s,0%o\n", exname(q->sname), off);
#endif
	else
		printf("\t.lcomm " LABFMT ",0%o\n", q->soffset, off);
}

/*
 * print a (non-prog) label.
 */
void
deflab1(int label)
{
	printf(LABFMT ":\n", label);
}

static char *loctbl[] = { "text", "data", "bss", "data" };

void
setloc1(int locc)
{
	//printf("setloc1(%d)\n", locc);
	if ((locc == lastloc) || (lastloc == DATA && locc == STRNG) || (locc == STRNG || lastloc == DATA))
		return;
	lastloc = locc;
	printf("\t.%s\n", loctbl[locc]);
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
