/*	$OpenBSD: local.c,v 1.6 2008/04/11 20:45:52 stefan Exp $	*/
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

#include <assert.h>
#include "pass1.h"

static int inbits, inval;

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
	int m;
	TWORD ty;
	int tmpnr, isptrvoid = 0;

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal in: %p\n", p);
		fwalk(p, eprint, 0);
	}
#endif

	switch (o = p->n_op) {

	case UCALL:
	case CALL:
	case STCALL:
	case USTCALL:
		if (p->n_type == VOID)
			break;
		/*
		 * if the function returns void*, ecode() invokes
		 * delvoid() to convert it to uchar*.
		 * We just let this happen on the ASSIGN to the temp,
		 * and cast the pointer back to void* on access
		 * from the temp.
		 */
		if (p->n_type == PTR+VOID)
			isptrvoid = 1;
		r = tempnode(0, p->n_type, p->n_df, p->n_sue);
		tmpnr = regno(r);
		r = block(ASSIGN, r, p, p->n_type, p->n_df, p->n_sue);

		p = tempnode(tmpnr, r->n_type, r->n_df, r->n_sue);
		if (isptrvoid) {
			p = block(PCONV, p, NIL, PTR+VOID,
			    p->n_df, MKSUE(PTR+VOID));
		}
		p = buildtree(COMOP, r, p);
		break;

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
				ty = r->n_type;
				nfree(l->n_left);
				l->n_left = r;
				l->n_type = ty;
				l->n_right->n_type = ty;
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
		/* Remove redundant PCONV's. Be careful */
		l = p->n_left;
		if (l->n_op == ICON) {
			l->n_lval = (unsigned)l->n_lval;
			goto delp;
		}
		if (l->n_type < INT || DEUNSIGN(l->n_type) == LONGLONG) {
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
				if (l->n_op == NAME || l->n_op == UMUL ||
				    l->n_op == TEMP) {
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
		}

		if (DEUNSIGN(p->n_type) == SHORT &&
		    DEUNSIGN(l->n_type) == SHORT) {
			nfree(p);
			p = l;
		}

		/* convert float/double to int before to (u)char/(u)short */
		if ((DEUNSIGN(p->n_type) == CHAR ||
		    DEUNSIGN(p->n_type) == SHORT) &&
                    (l->n_type == FLOAT || l->n_type == DOUBLE ||
		    l->n_type == LDOUBLE)) {
			p = block(SCONV, p, NIL, p->n_type, p->n_df, p->n_sue);
			p->n_left->n_type = INT;
			break;
                }

		/* convert (u)char/(u)short to int before float/double */
		if  ((p->n_type == FLOAT || p->n_type == DOUBLE ||
		    p->n_type == LDOUBLE) && (DEUNSIGN(l->n_type) == CHAR ||
		    DEUNSIGN(l->n_type) == SHORT)) {
			p = block(SCONV, p, NIL, p->n_type, p->n_df, p->n_sue);
			p->n_left->n_type = INT;
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
                p = buildtree(o==PMCONV?MUL:DIV, p->n_left, p->n_right);
		break;

	case FORCE:
		/* put return value in return reg */
		p->n_op = ASSIGN;
		p->n_right = p->n_left;
		p->n_left = block(REG, NIL, NIL, p->n_type, 0, MKSUE(INT));
		p->n_left->n_rval = RETREG(p->n_type);
		break;
	}

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal out: %p\n", p);
		fwalk(p, eprint, 0);
	}
#endif

	return(p);
}

void
myp2tree(NODE *p)
{
	struct symtab *sp;

	if (p->n_op != FCON) 
		return;

	/* Write float constants to memory */
 
	sp = tmpalloc(sizeof(struct symtab));
	sp->sclass = STATIC;
	sp->slevel = 1; /* fake numeric label */
	sp->soffset = getlab();
	sp->sflags = 0;
	sp->stype = p->n_type;
	sp->squal = (CON >> TSHIFT);

	defloc(sp);
	ninval(0, btdims[p->n_type].suesize, p);

	p->n_op = NAME;
	p->n_lval = 0;
	p->n_sp = sp;

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
	int nbytes = off / SZCHAR;

	p = buildtree(MUL, p, bcon(nbytes));
	p = buildtree(PLUS, p, bcon(7));
	p = buildtree(AND, p, bcon(~7));

	/* subtract the size from sp */
	sp = block(REG, NIL, NIL, p->n_type, 0, 0);
	sp->n_lval = 0;
	sp->n_rval = SP;
	ecomp(buildtree(MINUSEQ, sp, p));

	/* save the address of sp */
	sp = block(REG, NIL, NIL, PTR+INT, t->n_df, t->n_sue);
	sp->n_rval = SP;
	t->n_type = sp->n_type;
	ecomp(buildtree(ASSIGN, t, sp)); /* Emit! */
}

/*
 * print out a constant node
 * mat be associated with a label
 */
void
ninval(CONSZ off, int fsz, NODE *p)
{
        union { float f; double d; int i[2]; } u;
        struct symtab *q;
        TWORD t;
#ifndef USE_GAS
        int i, j;
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
                printf("\t.dword %lld\n", (long long)p->n_lval);
#else
                i = p->n_lval >> 32;
                j = p->n_lval & 0xffffffff;
                p->n_type = INT;
		if (bigendian) {
			p->n_lval = j;
	                ninval(off, 32, p);
			p->n_lval = i;
			ninval(off+32, 32, p);
		} else {
			p->n_lval = i;
	                ninval(off, 32, p);
			p->n_lval = j;
			ninval(off+32, 32, p);
		}
#endif
                break;
        case BOOL:
                if (p->n_lval > 1)
                        p->n_lval = p->n_lval != 0;
                /* FALLTHROUGH */
        case INT:
        case UNSIGNED:
                printf("\t.word " CONFMT, (CONSZ)p->n_lval);
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
                printf("\t.half %d\n", (int)p->n_lval & 0xffff);
                break;
        case CHAR:
        case UCHAR:
                printf("\t.byte %d\n", (int)p->n_lval & 0xff);
                break;
        case LDOUBLE:
        case DOUBLE:
                u.d = (double)p->n_dcon;
		if (bigendian) {
	                printf("\t.word\t%d\n", u.i[0]);
			printf("\t.word\t%d\n", u.i[1]);
		} else {
			printf("\t.word\t%d\n", u.i[1]);
	                printf("\t.word\t%d\n", u.i[0]);
		}
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

/*
 * Print out a string of characters.
 * Assume that the assembler understands C-style escape
 * sequences.
 */
void
instring(struct symtab *sp)
{
	char *s, *str;

	defloc(sp);
	str = sp->sname;

	/* be kind to assemblers and avoid long strings */
	printf("\t.ascii \"");
	for (s = str; *s != 0; ) {
		if (*s++ == '\\') {
			(void)esccon(&s);
		}
		if (s - str > 60) {
			fwrite(str, 1, s - str, stdout);
			printf("\"\n\t.ascii \"");
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

/* make a common declaration for id, if reasonable */
void
defzero(struct symtab *sp)
{
	int off;

	off = tsize(sp->stype, sp->sdf, sp->ssue);
	off = (off+(SZCHAR-1))/SZCHAR;
	printf("	.%scomm ", sp->sclass == STATIC ? "l" : "");
	if (sp->slevel == 0)
		printf("%s,0%o\n", exname(sp->soname), off);
	else
		printf(LABFMT ",0%o\n", sp->soffset, off);
}


#ifdef notdef
/* make a common declaration for id, if reasonable */
void
commdec(struct symtab *q)
{
	int off;

	off = tsize(q->stype, q->sdf, q->ssue);
	off = (off+(SZCHAR-1))/SZCHAR;

	printf("	.comm %s,%d\n", exname(q->soname), off);
}

/* make a local common declaration for id, if reasonable */
void
lcommdec(struct symtab *q)
{
	int off;

	off = tsize(q->stype, q->sdf, q->ssue);
	off = (off+(SZCHAR-1))/SZCHAR;
	if (q->slevel == 0)
		printf("\t.lcomm %s,%d\n", exname(q->soname), off);
	else
		printf("\t.lcomm " LABFMT ",%d\n", q->soffset, off);
}

/*
 * print a (non-prog) label.
 */
void
deflab1(int label)
{
	printf(LABFMT ":\n", label);
}

/* ro-text, rw-data, ro-data, ro-strings */
static char *loctbl[] = { "text", "data", "rdata", "rdata" };

void
setloc1(int locc)
{
	if (locc == lastloc && locc != STRNG)
		return;
	if (locc == DATA && lastloc == STRNG)
		return;

	if (locc != lastloc) {
		lastloc = locc;
		printf("\t.%s\n", loctbl[locc]);
	}

	if (locc == STRNG)
		printf("\t.align 2\n");
}
#endif

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

/*
 * va_start(ap, last) implementation.
 *
 * f is the NAME node for this builtin function.
 * a is the argument list containing:
 *	   CM
 *	ap   last
 *
 * It turns out that this is easy on MIPS.  Just write the
 * argument registers to the stack in va_arg_start() and
 * use the traditional method of walking the stackframe.
 */
NODE *
mips_builtin_stdarg_start(NODE *f, NODE *a)
{
	NODE *p, *q;
	int sz = 1;
	int i;

	/* check num args and type */
	if (a == NULL || a->n_op != CM || a->n_left->n_op == CM ||
	    !ISPTR(a->n_left->n_type))
		goto bad;

	/*
	 * look at the offset of the last org to calculate the
	 * number of remain registers that need to be written
	 * to the stack.
	 */
	if (xtemps) {
		for (i = 0; i < nargregs; i++) {
			q = block(REG, NIL, NIL, PTR+INT, 0, MKSUE(INT));
			q->n_rval = A0 + i;
			p = block(REG, NIL, NIL, PTR+INT, 0, MKSUE(INT));
			p->n_rval = SP;
			p = block(PLUS, p, bcon(ARGINIT+i), PTR+INT, 0, MKSUE(INT));
			p = buildtree(UMUL, p, NIL);
			p = buildtree(ASSIGN, p, q);
			ecomp(p);
		}
	}

	/* must first deal with argument size; use int size */
	p = a->n_right;
	if (p->n_type < INT) {
		/* round up to word */
		sz = SZINT / tsize(p->n_type, p->n_df, p->n_sue);
	}

	/*
	 * Once again, if xtemps, the register is written to a
	 * temp.  We cannot take the address of the temp and
	 * walk from there.
	 *
	 * No solution at the moment...
	 */
	assert(!xtemps);

	p = buildtree(ADDROF, p, NIL);	/* address of last arg */
	p = optim(buildtree(PLUS, p, bcon(sz)));
	q = block(NAME, NIL, NIL, PTR+VOID, 0, 0);
	q = buildtree(CAST, q, p);
	p = q->n_right;
	nfree(q->n_left);
	nfree(q);
	p = buildtree(ASSIGN, a->n_left, p);
	tfree(f);
	nfree(a);

	return p;

bad:
	uerror("bad argument to __builtin_stdarg_start");
	return bcon(0);
}

NODE *
mips_builtin_va_arg(NODE *f, NODE *a)
{
	NODE *p, *q, *r;
	int sz, tmpnr;

	/* check num args and type */
	if (a == NULL || a->n_op != CM || a->n_left->n_op == CM ||
	    !ISPTR(a->n_left->n_type) || a->n_right->n_op != TYPE)
		goto bad;

	/* create a copy to a temp node */
	p = tcopy(a->n_left);
	q = tempnode(0, p->n_type, p->n_df, p->n_sue);
	tmpnr = regno(q);
	p = buildtree(ASSIGN, q, p);

	r = a->n_right;
	sz = tsize(r->n_type, r->n_df, r->n_sue) / SZCHAR;
	if (sz < SZINT/SZCHAR) {
		werror("%s%s promoted to int when passed through ...",
			r->n_type & 1 ? "unsigned " : "",
			DEUNSIGN(r->n_type) == SHORT ? "short" : "char");
		sz = SZINT/SZCHAR;
	}
	q = buildtree(PLUSEQ, a->n_left, bcon(sz));
	q = buildtree(COMOP, p, q);

	nfree(a->n_right);
	nfree(a);
	nfree(f); 

	p = tempnode(tmpnr, INCREF(r->n_type), r->n_df, r->n_sue);
	p = buildtree(UMUL, p, NIL);
	p = buildtree(COMOP, q, p);

	return p;

bad:
	uerror("bad argument to __builtin_va_arg");
	return bcon(0);
}

NODE *
mips_builtin_va_end(NODE *f, NODE *a)
{
	tfree(f);
	tfree(a);
	return bcon(0);
}

NODE *
mips_builtin_va_copy(NODE *f, NODE *a)
{
	if (a == NULL || a->n_op != CM || a->n_left->n_op == CM)
		goto bad;
	tfree(f);
	f = buildtree(ASSIGN, a->n_left, a->n_right);
	nfree(a);
	return f;

bad:
	uerror("bad argument to __buildtin_va_copy");
	return bcon(0);
}
/*
 * Give target the opportunity of handling pragmas.
 */
int
mypragma(char **ary)
{
	return 0; }

/*
 * Called when a identifier has been declared, to give target last word.
 */
void
fixdef(struct symtab *sp)
{
}

