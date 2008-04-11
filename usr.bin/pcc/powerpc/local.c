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

#include <assert.h>
#include "pass1.h"

#define IALLOC(sz) (isinlining ? permalloc(sz) : tmpalloc(sz))

#define SNOUNDERSCORE SLOCAL1

extern int kflag;

static
void simmod(NODE *p);

/*	this file contains code which is dependent on the target machine */

#if defined(MACHOABI)

/*
 *  Keep track of PIC stubs.
 */

void
addstub(struct stub *list, char *name)
{
        struct stub *s;

        DLIST_FOREACH(s, list, link) {
                if (strcmp(s->name, name) == 0)
                        return;
        }

        if ((s = permalloc(sizeof(struct stub))) == NULL)
                cerror("addstub: malloc");
        if ((s->name = strdup(name)) == NULL)
                cerror("addstub: strdup");
        DLIST_INSERT_BEFORE(list, s, link);
}

#endif


/*
 * Make a symtab entry for PIC use.
 */
static struct symtab *
picsymtab(char *p, char *s, char *s2)
{
	struct symtab *sp = IALLOC(sizeof(struct symtab));
	size_t len = strlen(p) + strlen(s) + strlen(s2) + 1;

	sp->sname = sp->soname = IALLOC(len);
	strlcpy(sp->soname, p, len);
	strlcat(sp->soname, s, len);
	strlcat(sp->soname, s2, len);
	sp->sclass = EXTERN;
	sp->sflags = sp->slevel = 0;

	return sp;
}

int gotnr;  /* tempnum for GOT register */

/*
 * Create a reference for an extern variable.
 */
static NODE *
picext(NODE *p)
{
	NODE *q;
	struct symtab *sp;

	if (strncmp(p->n_sp->soname, "__builtin", 9) == 0)
		return p;

#if defined(ELFABI)

	sp = picsymtab("", p->n_sp->soname, "@got(31)");
	q = xbcon(0, sp, PTR+VOID);
	q = block(UMUL, q, 0, PTR+VOID, 0, MKSUE(VOID));

#elif defined(MACHOABI)

	char buf2[64];
	NODE *r;

	if (p->n_sp->sclass == EXTDEF) {
		snprintf(buf2, 64, "-L%s$pb", cftnsp->soname);
		sp = picsymtab("", exname(p->n_sp->soname), buf2);
	} else {
		snprintf(buf2, 64, "$non_lazy_ptr-L%s$pb", cftnsp->soname);
		sp = picsymtab("L", p->n_sp->soname, buf2);
		addstub(&nlplist, p->n_sp->soname);
	}
#if USE_GOTNR
	q = tempnode(gotnr, PTR+VOID, 0, MKSUE(VOID));
#else
	q = block(REG, NIL, NIL, PTR+VOID, 0, MKSUE(VOID));
	regno(q) = GOTREG;
#endif
	r = xbcon(0, sp, INT);
	q = buildtree(PLUS, q, r);

	if (p->n_sp->sclass != EXTDEF)
		q = block(UMUL, q, 0, PTR+VOID, 0, MKSUE(VOID));

#endif

	q = block(UMUL, q, 0, p->n_type, p->n_df, p->n_sue);
	q->n_sp = p->n_sp; /* for init */
	nfree(p);

	return q;
}

/*
 * Create a reference for a static variable
 */

static NODE *
picstatic(NODE *p)
{
	NODE *q;
	struct symtab *sp;

#if defined(ELFABI)

	if (p->n_sp->slevel > 0 || p->n_sp->sclass == ILABEL) {
		char buf[64];
		snprintf(buf, 64, LABFMT, (int)p->n_sp->soffset);
		sp = picsymtab("", buf, "@got(31)");
		sp->sflags |= SNOUNDERSCORE;
	} else  {
		sp = picsymtab("", p->n_sp->soname, "@got(31)");
	}
	sp->sclass = STATIC;
	sp->stype = p->n_sp->stype;
	q = xbcon(0, sp, PTR+VOID);
	q = block(UMUL, q, 0, p->n_type, p->n_df, p->n_sue);
	q->n_sp = p->n_sp;
	nfree(p);

#elif defined(MACHOABI)

	char buf2[64];
	NODE *r;

	snprintf(buf2, 64, "-L%s$pb", cftnsp->soname);

	if (p->n_sp->slevel > 0 || p->n_sp->sclass == ILABEL) {
		char buf1[64];
		snprintf(buf1, 64, LABFMT, (int)p->n_sp->soffset);
		sp = picsymtab("", buf1, buf2);
		sp->sflags |= SNOUNDERSCORE;
	} else  {
		sp = picsymtab("", exname(p->n_sp->soname), buf2);
	}
	sp->sclass = STATIC;
	sp->stype = p->n_sp->stype;
#if USE_GOTNR
	q = tempnode(gotnr, PTR+VOID, 0, MKSUE(VOID));
#else
	q = block(REG, NIL, NIL, PTR+VOID, 0, MKSUE(VOID));
	regno(q) = GOTREG;
#endif
	r = xbcon(0, sp, INT);
	q = buildtree(PLUS, q, r);
	q = block(UMUL, q, 0, p->n_type, p->n_df, p->n_sue);
	q->n_sp = p->n_sp;
	nfree(p);

#endif

	return q;
}

static NODE *
convert_ulltof(NODE *p)
{
	NODE *q, *r, *l, *t;
	int ty;
	int tmpnr;

	ty = p->n_type;
	l = p->n_left;
	nfree(p);

	q = tempnode(0, ULONGLONG, 0, MKSUE(ULONGLONG));
	tmpnr = regno(q);
	t = buildtree(ASSIGN, q, l);
	ecomp(t);

	//q = tempnode(tmpnr, ULONGLONG, 0, MKSUE(ULONGLONG));
	//q = block(SCONV, q, NIL, LONGLONG, 0, MKSUE(LONGLONG));
	q = tempnode(tmpnr, LONGLONG, 0, MKSUE(LONGLONG));
	r = block(SCONV, q, NIL, ty, 0, MKSUE(ty));

	q = tempnode(tmpnr, ULONGLONG, 0, MKSUE(ULONGLONG));
	q = block(RS, q, bcon(1), ULONGLONG, 0, MKSUE(ULONGLONG));
	q = block(SCONV, q, NIL, LONGLONG, 0, MKSUE(LONGLONG));
	q = block(SCONV, q, NIL, ty, 0, MKSUE(ty));
	t = block(FCON, NIL, NIL, ty, 0, MKSUE(ty));
	t->n_dcon = 2;
	l = block(MUL, q, t, ty, 0, MKSUE(ty));

	r = buildtree(COLON, l, r);

	q = tempnode(tmpnr, ULONGLONG, 0, MKSUE(ULONGLONG));
	q = block(SCONV, q, NIL, LONGLONG, 0, MKSUE(LONGLONG));
	l = block(LE, q, xbcon(0, NULL, LONGLONG), INT, 0, MKSUE(INT));

	return clocal(buildtree(QUEST, l, r));

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
	int isptrvoid = 0;
	int tmpnr;

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal: %p\n", p);
		fwalk(p, eprint, 0);
	}
#endif
	switch (o = p->n_op) {

	case ADDROF:
#ifdef PCC_DEBUG
		if (xdebug) {
			printf("clocal(): ADDROF\n");
			printf("type: 0x%x\n", p->n_type);
		}
#endif

		if (kflag == 0 || blevel == 0)
			break;
		/* char arrays may end up here */
		l = p->n_left;
		if (l->n_op != NAME ||
		    (l->n_type != ARY+CHAR && l->n_type != ARY+WCHAR_TYPE))
			break;
		l = p;
		p = picstatic(p->n_left);
		nfree(l);
		if (p->n_op != UMUL)
			cerror("ADDROF error");
		l = p;
		p = p->n_left;
		nfree(l);
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
			r->n_rval = FPREG;
			p = stref(block(STREF, r, p, 0, 0, 0));
			break;

		case USTATIC:
			if (kflag == 0)
				break;
			/* FALLTHROUGH */

		case STATIC:
			if (kflag == 0) {
				if (q->slevel == 0)
					break;
				p->n_lval = 0;
			} else if (blevel > 0) {
				p = picstatic(p);
			}
			break;

		case REGISTER:
			p->n_op = REG;
			p->n_lval = 0;
			p->n_rval = q->soffset;
			break;

		case EXTERN: 
		case EXTDEF:
			if (kflag == 0)
				break;
			if (blevel > 0)
				p = picext(p);
			break;

		case ILABEL:
			if (kflag && blevel > 0)
				p = picstatic(p);
			break;
		}
		break;

	case UCALL:
	case CALL:
	case USTCALL:
	case STCALL:
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
		r = buildtree(ASSIGN, r, p);

                p = tempnode(tmpnr, r->n_type, r->n_df, r->n_sue);
                if (isptrvoid) {
                        p = block(PCONV, p, NIL, PTR+VOID,
                            p->n_df, MKSUE(PTR+VOID));
                }
#if 1
                p = buildtree(COMOP, r, p);
#else
		/* XXX this doesn't work if the call is already in a COMOP */
		r = clocal(r);
		ecomp(r);
#endif
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
			return l;
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

		/*
		 * if converting ULONGLONG to FLOAT/(L)DOUBLE,
		 * replace ___floatunsdidf() with ___floatdidf()
		 */
		if (l->n_type == ULONGLONG && p->n_type >= FLOAT &&
		    p->n_type <= LDOUBLE) {
			return convert_ulltof(p);
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
		} else if (o == FCON) {
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
		if ((DEUNSIGN(p->n_type) == CHAR ||
		    DEUNSIGN(p->n_type) == SHORT) &&
		    (l->n_type == FLOAT || l->n_type == DOUBLE ||
		    l->n_type == LDOUBLE)) {
			p = block(SCONV, p, NIL, p->n_type, p->n_df, p->n_sue);
			p->n_left->n_type = INT;
			return p;
		}
		if ((DEUNSIGN(l->n_type) == CHAR ||
		    DEUNSIGN(l->n_type) == SHORT) &&
		    (p->n_type == FLOAT || p->n_type == DOUBLE ||
		    p->n_type == LDOUBLE)) {
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

	case STNAME:
		if ((q = p->n_sp) == NULL)
			return p;
		if (q->sclass != STNAME)
			return p;
		t = p->n_type;
		p = block(ADDROF, p, NIL, INCREF(t), p->n_df, p->n_sue);
		p = block(UMUL, p, NIL, t, p->n_df, p->n_sue);
		break;

	case FORCE:
		/* put return value in return reg */
		p->n_op = ASSIGN;
		p->n_right = p->n_left;
		p->n_left = block(REG, NIL, NIL, p->n_type, 0, MKSUE(INT));
		p->n_left->n_rval = p->n_left->n_type == BOOL ? 
		    RETREG(BOOL_TYPE) : RETREG(p->n_type);
		break;

	case LS:
	case RS:
		if (p->n_right->n_op == ICON)
			break; /* do not do anything */
		if (DEUNSIGN(p->n_right->n_type) == INT)
			break;
		p->n_right = block(SCONV, p->n_right, NIL, INT, 0, MKSUE(INT));
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

/*
 * Change CALL references to either direct (static) or PLT.
 */
static void
fixnames(NODE *p)
{
        struct symtab *sp;
        struct suedef *sue;
        NODE *q;
        char *c;
        int isu;

        if ((cdope(p->n_op) & CALLFLG) == 0)
                return;

        isu = 0;
        q = p->n_left;
        sue = q->n_sue;
        if (q->n_op == UMUL)
                q = q->n_left, isu = 1;

#if defined(ELFABI)

        if (q->n_op == ICON) {
                sp = q->n_sp;

#elif defined(MACHOABI)

#ifdef USE_GOTNR
	if (q->n_op == PLUS && q->n_left->n_op == TEMP &&
#else
	if (q->n_op == PLUS && q->n_left->n_op == REG &&
#endif
	    q->n_right->n_op == ICON) {
                sp = q->n_right->n_sp;
#endif

                if (sp == NULL)
                        return; /* nothing to do */
                if (sp->sclass == STATIC && !ISFTN(sp->stype))
                        return; /* function pointer */

                if (sp->sclass != STATIC && sp->sclass != EXTERN &&
                    sp->sclass != EXTDEF)
                        cerror("fixnames");

#if defined(ELFABI)

                if ((c = strstr(sp->soname, "@got(31)")) == NULL)
                        cerror("fixnames2");
                if (isu) {
                        strcpy(c, "@plt");
                } else
                        *c = 0;

#elif defined(MACHOABI)

		if ((c = strstr(sp->soname, "$non_lazy_ptr")) == NULL &&
		    (c = strstr(sp->soname, "-L")) == NULL)
				cerror("fixnames2");
		if (isu) {
			*c = 0;
			addstub(&stublist, sp->soname+1);
			strcpy(c, "$stub");
		} else 
			*c = 0;

		nfree(q->n_left);
		q = q->n_right;
		if (isu)
			nfree(p->n_left->n_left);
		nfree(p->n_left);
		p->n_left = q;
		q->n_sue = sue;

#endif
        }
}

void
myp2tree(NODE *p)
{
	int o = p->n_op;
	struct symtab *sp;

	if (kflag)
		walkf(p, fixnames);
	if (o != FCON) 
		return;

	/* Write float constants to memory */
	/* Should be voluntary per architecture */
 
	sp = IALLOC(sizeof(struct symtab));
	sp->sclass = STATIC;
	sp->ssue = MKSUE(p->n_type);
	sp->slevel = 1; /* fake numeric label */
	sp->soffset = getlab();
	sp->sflags = 0;
	sp->stype = p->n_type;
	sp->squal = (CON >> TSHIFT);

	defloc(sp);
	ninval(0, sp->ssue->suesize, p);

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
	return 1;
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
 * Allocate bits on the stack.
 * 'off' is the number of bits to allocate
 * 'p' is a tree that when evaluated is the multiply count for 'off'
 * 't' is a storeable node where to write the allocated address
 */
void
spalloc(NODE *t, NODE *p, OFFSZ off)
{
	NODE *q, *r;
	int nbytes = off / SZCHAR;
	int stacksize = 24+40; /* this should be p2stacksize */

	/*
	 * After we subtract the requisite bytes
	 * off the stack, we need to step back over
	 * the 40 bytes for the arguments registers
	 * *and* any other parameters which will get
	 * saved to the stack.  Unfortunately, we
	 * don't have that information in pass1 and
	 * the parameters will stomp on the allocated
	 * space for alloca().
	 *
	 * No fix yet.
	 */
	werror("parameters may stomp on alloca()");

	/* compute size */
	p = buildtree(MUL, p, bcon(nbytes));
	p = buildtree(PLUS, p, bcon(ALSTACK/SZCHAR));

	/* load the top-of-stack */
	q = block(REG, NIL, NIL, PTR+INT, 0, MKSUE(INT));
	regno(q) = SPREG;
	q = block(UMUL, q, NIL, INT, 0, MKSUE(INT));

	/* save old top-of-stack value to new top-of-stack position */
	r = block(REG, NIL, NIL, PTR+INT, 0, MKSUE(INT));
	regno(r) = SPREG;
	r = block(MINUSEQ, r, p, INT, 0, MKSUE(INT));
	r = block(UMUL, r, NIL, INT, 0, MKSUE(INT));
	ecomp(buildtree(ASSIGN, r, q));

	r = block(REG, NIL, NIL, PTR+INT, 0, MKSUE(INT));
	regno(r) = SPREG;

	/* skip over the arguments space and align to 16 bytes */
	r = block(PLUS, r, bcon(stacksize + 15), INT, 0, MKSUE(INT));
	r = block(RS, r, bcon(4), INT, 0, MKSUE(INT));
	r = block(LS, r, bcon(4), INT, 0, MKSUE(INT));

	t->n_type = p->n_type;
	ecomp(buildtree(ASSIGN, t, r));
}

/*
 * Print out a string of characters.
 * Unfortunately, this code assumes that the assembler understands
 * C-style escape sequences. (which it doesn't!)
 * Location is already set.
 */
void
instring(struct symtab *sp)
{
	char *s, *str = sp->sname;

#if defined(ELFABI)

	defloc(sp);

#elif defined(MACHOABI)

	extern int lastloc;
	if (lastloc != STRNG)
		printf("	.cstring\n");
	lastloc = STRNG;
	printf("	.p2align 1\n");
	printf(LABFMT ":\n", sp->soffset);

#endif

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

#if 0
	/* little-endian */
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
#endif
	/* big-endian */
	if (inbits) {
		m = SZCHAR - inbits;
		if (fsz < m) {
			inbits += fsz;
			inval <<= fsz;
		} else {
			printf("\t.byte %d\n", inval << m);
			fsz -= m;
			inval = inbits = 0;
		}
	}

	if (fsz >= SZCHAR) {
		printf("\t.space %d\n", fsz/SZCHAR);
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

#if 0
	/* little-endian */
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
#endif

	/* big-endian */
	inval <<= fsz;
	inval |= val;
	inbits += fsz;
	while (inbits >= SZCHAR) {
		int pval = inval >> (inbits - SZCHAR);
		printf("\t.byte %d\n", pval & 255);
		inbits -= SZCHAR;
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
	char *c;
	TWORD t;
	int i;

	t = p->n_type;
	if (t > BTMASK)
		t = INT; /* pointer */

	while (p->n_op == SCONV || p->n_op == PCONV) {
		NODE *l = p->n_left;
		l->n_type = p->n_type;
		p = l;
	}

	if (kflag && (p->n_op == PLUS || p->n_op == UMUL)) {
		if (p->n_op == UMUL)
			p = p->n_left;
		p = p->n_right;
		q = p->n_sp;

#if defined(ELFABI)

		if ((c = strstr(q->soname, "@got(31)")) != NULL)
			*c = 0; /* ignore GOT ref here */

#elif defined(MACHOABI)

		if  ((c = strstr(q->soname, "$non_lazy_ptr")) != NULL) {
			q->soname++;	/* skip "L" */
			*c = 0; /* ignore GOT ref here */
		}
		else if ((c = strstr(q->soname, "-L")) != NULL)
			*c = 0; /* ignore GOT ref here */

#endif

	}

	if (p->n_op != ICON && p->n_op != FCON)
		cerror("ninval: init node not constant: node %p [%s]",
		     p, cftnsp->soname);

	if (p->n_op == ICON && p->n_sp != NULL && DEUNSIGN(t) != INT)
		uerror("element not constant");

	switch (t) {
	case LONGLONG:
	case ULONGLONG:
#if 0
		/* little-endian */
		i = (p->n_lval >> 32);
		p->n_lval &= 0xffffffff;
		p->n_type = INT;
		ninval(off, 32, p);
		p->n_lval = i;
		ninval(off+32, 32, p);
#endif
		/* big-endian */
		i = (p->n_lval & 0xffffffff);
		p->n_lval >>= 32;
		p->n_type = INT;
		ninval(off, 32, p);
		p->n_lval = i;
		ninval(off+32, 32, p);

		break;
	case INT:
	case UNSIGNED:
		printf("\t.long %d", (int)p->n_lval);
		if ((q = p->n_sp) != NULL) {
			if ((q->sclass == STATIC && q->slevel > 0) ||
			    q->sclass == ILABEL) {
				printf("+" LABFMT, q->soffset);
			} else {

#if defined(MACHOABI)
				if ((q->sflags & SNOUNDERSCORE) != 0)
					printf("+%s", q->soname);
				else
#endif
					printf("+%s", exname(q->soname));
			}
		}
		printf("\n");
		break;
	case SHORT:
	case USHORT:
		printf("\t.short %d\n", (int)p->n_lval & 0xffff);
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
#if 0
		/* little-endian */
		printf("\t.long 0x%x,0x%x,0x%x\n", u.i[0], u.i[1], u.i[2]);
#endif
		/* big-endian */
		printf("\t.long 0x%x,0x%x,0x%x\n", u.i[0], u.i[1], u.i[2]);
		break;
	case DOUBLE:
		u.d = (double)p->n_dcon;
		printf("\t.long 0x%x\n\t.long 0x%x\n", u.i[0], u.i[1]);
		break;
	case FLOAT:
		u.f = (float)p->n_dcon;
		printf("\t.long 0x%x\n", u.i[0]);
		break;
	default:
		cerror("ninval");
	}
}

/* make a name look like an external name in the local machine */
char *
exname(char *p)
{
#if defined(ELFABI)

	return (p == NULL ? "" : p);

#elif defined(MACHOABI)

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

        return (text);

#endif
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
defzero(struct symtab *sp)
{
	int off;

	off = tsize(sp->stype, sp->sdf, sp->ssue);
	off = (off+(SZCHAR-1))/SZCHAR;
	printf("\t.%scomm ", sp->sclass == STATIC ? "l" : "");
	if (sp->slevel == 0)
		printf("%s,%d\n", exname(sp->soname), off);
	else
		printf(LABFMT ",%d\n", sp->soffset, off);
}


#ifdef notdef
/* make a common declaration for id, if reasonable */
void
commdec(struct symtab *q)
{
	int off;

	off = tsize(q->stype, q->sdf, q->ssue);
	off = (off+(SZCHAR-1))/SZCHAR;
	printf("\t.comm %s,0%o\n", exname(q->soname), off);
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

#if defined(ELFABI)

static char *loctbl[] = { "text", "data", "section .rodata,",
    "section .rodata" };

#elif defined(MACHOABI)

static char *loctbl[] = { "text", "data", "section .rodata,", "cstring" };

#endif

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

/*
 * Give target the opportunity of handling pragmas.
 */
int
mypragma(char **ary)
{
	return 0;
}

/*
 * Called when a identifier has been declared, to give target last word.
 */
void
fixdef(struct symtab *sp)
{
}

/*
 * There is very little different here to the standard builtins.
 * It basically handles promotion of types smaller than INT.
 */

NODE *
powerpc_builtin_stdarg_start(NODE *f, NODE *a)
{
        NODE *p, *q;
        int sz = 1;

        /* check num args and type */
        if (a == NULL || a->n_op != CM || a->n_left->n_op == CM ||
            !ISPTR(a->n_left->n_type))
                goto bad;

        /* must first deal with argument size; use int size */
        p = a->n_right;
        if (p->n_type < INT) {
                /* round up to word */
                sz = SZINT / tsize(p->n_type, p->n_df, p->n_sue);
        }

        p = buildtree(ADDROF, p, NIL);  /* address of last arg */
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
powerpc_builtin_va_arg(NODE *f, NODE *a)
{
        NODE *p, *q, *r;
        int sz, tmpnr;

        /* check num args and type */
        if (a == NULL || a->n_op != CM || a->n_left->n_op == CM ||
            !ISPTR(a->n_left->n_type) || a->n_right->n_op != TYPE)
                goto bad;

        r = a->n_right;

        /* get type size */
        sz = tsize(r->n_type, r->n_df, r->n_sue) / SZCHAR;
        if (sz < SZINT/SZCHAR) {
                werror("%s%s promoted to int when passed through ...",
                        ISUNSIGNED(r->n_type) ? "unsigned " : "",
                        DEUNSIGN(r->n_type) == SHORT ? "short" : "char");
                sz = SZINT/SZCHAR;
		r->n_type = INT;
		r->n_sue = MKSUE(INT);
        }

        p = tcopy(a->n_left);

#if defined(ELFABI)

        /* alignment */
        if (SZINT/SZCHAR && r->n_type != UNIONTY && r->n_type != STRTY) {
                p = buildtree(PLUS, p, bcon(ALSTACK/8 - 1));
                p = block(AND, p, bcon(-ALSTACK/8), p->n_type, p->n_df, p->n_sue);
        }

#endif

        /* create a copy to a temp node */
        q = tempnode(0, p->n_type, p->n_df, p->n_sue);
        tmpnr = regno(q);
        p = buildtree(ASSIGN, q, p);

        q = tempnode(tmpnr, p->n_type, p->n_df, p->n_sue);
        q = buildtree(PLUS, q, bcon(sz));
        q = buildtree(ASSIGN, a->n_left, q);

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
powerpc_builtin_va_end(NODE *f, NODE *a)
{
        tfree(f);
        tfree(a);
 
        return bcon(0);
}

NODE *
powerpc_builtin_va_copy(NODE *f, NODE *a)
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

NODE *
powerpc_builtin_return_address(NODE *f, NODE *a)
{
	int nframes;
	int i = 0;

	if (a == NULL || a->n_op != ICON)
		goto bad;

	nframes = a->n_lval;

	tfree(f);
	tfree(a);

	f = block(REG, NIL, NIL, PTR+VOID, 0, MKSUE(PTR+VOID));
	regno(f) = SPREG;

	do {
		f = buildtree(UMUL, f, NIL);
	} while (i++ < nframes);

	f = block(PLUS, f, bcon(8), INCREF(PTR+VOID), 0, MKSUE(VOID));
	f = buildtree(UMUL, f, NIL);

	return f;
bad:
        uerror("bad argument to __builtin_return_address");
        return bcon(0);
}
