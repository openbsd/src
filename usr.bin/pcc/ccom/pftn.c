/*	$OpenBSD: pftn.c,v 1.14 2008/08/17 18:40:13 ragge Exp $	*/
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
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code and documentation must retain the above
 * copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditionsand the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 * 	This product includes software developed or owned by Caldera
 *	International, Inc.
 * Neither the name of Caldera International, Inc. nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OFLIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Many changes from the 32V sources, among them:
 * - New symbol table manager (moved to another file).
 * - Prototype saving/checks.
 */

# include "pass1.h"

#include <string.h> /* XXX - for strcmp */

#include "cgram.h"

struct symtab *cftnsp;
int arglistcnt, dimfuncnt;	/* statistics */
int symtabcnt, suedefcnt;	/* statistics */
int autooff,		/* the next unused automatic offset */
    maxautooff,		/* highest used automatic offset in function */
    argoff;		/* the next unused argument offset */
int retlab = NOLAB;	/* return label for subroutine */
int brklab;
int contlab;
int flostat;
int blevel;
int reached, prolab;

struct params;

#define ISSTR(ty) (ty == STRTY || ty == UNIONTY)
#define ISSOU(ty) (ty == STRTY || ty == UNIONTY)
#define MKTY(p, t, d, s) r = talloc(); *r = *p; \
	r = argcast(r, t, d, s); *p = *r; nfree(r);

/*
 * Linked list stack while reading in structs.
 */
struct rstack {
	struct	rstack *rnext;
	int	rsou;
	int	rstr;
	struct	symtab *rsym;
	struct	symtab *rb;
	int	flags;
#define	LASTELM	1
} *rpole;

/*
 * Linked list for parameter (and struct elements) declaration.
 */
static struct params {
	struct params *next, *prev;
	struct symtab *sym;
} *lpole, *lparam;
static int nparams;

/* defines used for getting things off of the initialization stack */

static NODE *arrstk[10];
static int arrstkp;
static int intcompare;
static NODE *parlink;

void fixtype(NODE *p, int class);
int fixclass(int class, TWORD type);
int falloc(struct symtab *p, int w, int new, NODE *pty);
static void dynalloc(struct symtab *p, int *poff);
void inforce(OFFSZ n);
void vfdalign(int n);
static void ssave(struct symtab *);
static void alprint(union arglist *al, int in);
static void lcommadd(struct symtab *sp);

int ddebug = 0;

/*
 * Declaration of an identifier.  Handles redeclarations, hiding,
 * incomplete types and forward declarations.
 */

void
defid(NODE *q, int class)
{
	extern int fun_inline;
	struct symtab *p;
	TWORD type, qual;
	TWORD stp, stq;
	int scl;
	union dimfun *dsym, *ddef;
	int slev, temp, changed;

	if (q == NIL)
		return;  /* an error was detected */

	p = q->n_sp;

	if (p->sname == NULL)
		cerror("defining null identifier");

#ifdef PCC_DEBUG
	if (ddebug) {
		printf("defid(%s (%p), ", p->sname, p);
		tprint(stdout, q->n_type, q->n_qual);
		printf(", %s, (%p,%p)), level %d\n", scnames(class),
		    q->n_df, q->n_sue, blevel);
	}
#endif

	fixtype(q, class);

	type = q->n_type;
	qual = q->n_qual;
	class = fixclass(class, type);

	stp = p->stype;
	stq = p->squal;
	slev = p->slevel;

#ifdef PCC_DEBUG
	if (ddebug) {
		printf("	modified to ");
		tprint(stdout, type, qual);
		printf(", %s\n", scnames(class));
		printf("	previous def'n: ");
		tprint(stdout, stp, stq);
		printf(", %s, (%p,%p)), level %d\n",
		    scnames(p->sclass), p->sdf, p->ssue, slev);
	}
#endif

	if (blevel == 1) {
		switch (class) {
		default:
			if (!(class&FIELD) && !ISFTN(type))
				uerror("declared argument %s missing",
				    p->sname );
		case MOS:
		case MOU:
		case TYPEDEF:
		case PARAM:
			;
		}
	}

	if (stp == UNDEF)
		goto enter; /* New symbol */

	if (type != stp)
		goto mismatch;

	if (blevel > slev && (class == AUTO || class == REGISTER))
		/* new scope */
		goto mismatch;

	/*
	 * test (and possibly adjust) dimensions.
	 * also check that prototypes are correct.
	 */
	dsym = p->sdf;
	ddef = q->n_df;
	changed = 0;
	for (temp = type; temp & TMASK; temp = DECREF(temp)) {
		if (ISARY(temp)) {
			if (dsym->ddim == NOOFFSET) {
				dsym->ddim = ddef->ddim;
				changed = 1;
			} else if (ddef->ddim != NOOFFSET &&
			    dsym->ddim!=ddef->ddim) {
				goto mismatch;
			}
			++dsym;
			++ddef;
		} else if (ISFTN(temp)) {
			/* add a late-defined prototype here */
			if (cftnsp == NULL && dsym->dfun == NULL)
				dsym->dfun = ddef->dfun;
			if (!oldstyle && ddef->dfun != NULL &&
			    chkftn(dsym->dfun, ddef->dfun))
				uerror("declaration doesn't match prototype");
			dsym++, ddef++;
		}
	}
#ifdef STABS
	if (changed && gflag)
		stabs_chgsym(p); /* symbol changed */
#endif

	/* check that redeclarations are to the same structure */
	if ((temp == STRTY || temp == UNIONTY) && p->ssue != q->n_sue) {
		goto mismatch;
	}

	scl = p->sclass;

#ifdef PCC_DEBUG
	if (ddebug)
		printf("	previous class: %s\n", scnames(scl));
#endif

	if (class & FIELD)
		return;
	switch(class) {

	case EXTERN:
		switch( scl ){
		case STATIC:
		case USTATIC:
			if( slev==0 )
				goto done;
			break;
		case EXTDEF:
		case EXTERN:
		case FORTRAN:
		case UFORTRAN:
			goto done;
			}
		break;

	case STATIC:
		if (scl==USTATIC || (scl==EXTERN && blevel==0)) {
			p->sclass = STATIC;
			goto done;
		}
		if (changed || (scl == STATIC && blevel == slev))
			goto done; /* identical redeclaration */
		break;

	case USTATIC:
		if (scl==STATIC || scl==USTATIC)
			goto done;
		break;

	case TYPEDEF:
		if (scl == class)
			goto done;
		break;

	case UFORTRAN:
		if (scl == UFORTRAN || scl == FORTRAN)
			goto done;
		break;

	case FORTRAN:
		if (scl == UFORTRAN) {
			p->sclass = FORTRAN;
			goto done;
		}
		break;

	case MOU:
	case MOS:
		goto done;

	case EXTDEF:
		switch (scl) {
		case EXTERN:
			p->sclass = EXTDEF;
			goto done;
		case USTATIC:
			p->sclass = STATIC;
			goto done;
		}
		break;

	case AUTO:
	case REGISTER:
		if (blevel == slev)
			goto redec;
		break;  /* mismatch.. */
	case SNULL:
		if (fun_inline && ISFTN(type))
			goto done;
		break;
	}

	mismatch:

	/*
	 * Only allowed for automatic variables.
	 */
	if (blevel == slev || class == EXTERN || class == FORTRAN ||
	    class == UFORTRAN) {
		if (ISSTR(class) && !ISSTR(p->sclass)) {
redec:			uerror("redeclaration of %s", p->sname);
			return;
		}
	}
	if (blevel == 0)
		goto redec;
	q->n_sp = p = hide(p);

	enter:  /* make a new entry */

#ifdef PCC_DEBUG
	if(ddebug)
		printf("	new entry made\n");
#endif
	p->stype = type;
	p->squal = qual;
	p->sclass = class;
	p->slevel = blevel;
	p->soffset = NOOFFSET;
	if (q->n_sue == NULL)
		cerror("q->n_sue == NULL");
	p->ssue = q->n_sue;

	/* copy dimensions */
	p->sdf = q->n_df;
	/* Do not save param info for old-style functions */
	if (ISFTN(type) && oldstyle)
		p->sdf->dfun = NULL;

	/* allocate offsets */
	if (class&FIELD) {
		(void) falloc(p, class&FLDSIZ, 0, NIL);  /* new entry */
	} else switch (class) {

	case REGISTER:
		cerror("register var");

	case AUTO:
		if (arrstkp)
			dynalloc(p, &autooff);
		else
			oalloc(p, &autooff);
		break;
	case PARAM:
		if (ISARY(p->stype)) {
			/* remove array type on parameters before oalloc */
			p->stype += (PTR-ARY);
			p->sdf++;
		}
		if (arrstkp)
			dynalloc(p, &argoff);
		else
			oalloc(p, &argoff);
		break;
		
	case STATIC:
	case EXTDEF:
	case EXTERN:
	case UFORTRAN:
	case FORTRAN:
		p->soffset = getlab();
		if (pragma_renamed)
			p->soname = pragma_renamed;
		pragma_renamed = NULL;
		break;

	case MOU:
		rpole->rstr = 0;
		/* FALLTHROUGH */
	case MOS:
		oalloc(p, &rpole->rstr);
		if (class == MOU)
			rpole->rstr = 0;
		break;
	case SNULL:
		if (fun_inline) {
			p->slevel = 1;
			p->soffset = getlab();
		}
	}

#ifdef STABS
	if (gflag)
		stabs_newsym(p);
#endif

done:
	fixdef(p);	/* Leave last word to target */
#ifdef PCC_DEBUG
	if (ddebug)
		printf( "	sdf, ssue, offset: %p, %p, %d\n",
		    p->sdf, p->ssue, p->soffset);
#endif
}

void
ssave(struct symtab *sym)
{
	struct params *p;

	p = tmpalloc(sizeof(struct params));
	p->next = NULL;
	p->sym = sym;

	if ((p->prev = lparam) == NULL)
		lpole = p;
	else
		lparam->next = p;
	lparam = p;
}

/*
 * end of function
 */
void
ftnend()
{
	extern struct savbc *savbc;
	extern struct swdef *swpole;
	extern int tvaloff;
	char *c;

	if (retlab != NOLAB && nerrors == 0) { /* inside a real function */
		plabel(retlab);
		efcode(); /* struct return handled here */
		c = cftnsp->soname;
		SETOFF(maxautooff, ALCHAR);
		send_passt(IP_EPILOG, 0, maxautooff/SZCHAR, c,
		    cftnsp->stype, cftnsp->sclass == EXTDEF, retlab, tvaloff);
	}

	tcheck();
	brklab = contlab = retlab = NOLAB;
	flostat = 0;
	if (nerrors == 0) {
		if (savbc != NULL)
			cerror("bcsave error");
		if (lparam != NULL)
			cerror("parameter reset error");
		if (swpole != NULL)
			cerror("switch error");
	}
	savbc = NULL;
	lparam = NULL;
	maxautooff = autooff = AUTOINIT;
	reached = 1;

	if (isinlining)
		inline_end();
	inline_prtout();

	tmpfree(); /* Release memory resources */
}

static struct symtab nulsym = {
	{ NULL, 0, 0, 0, 0 }, "null", "null", INT, 0, NULL, NULL
};

void
dclargs()
{
	union dimfun *df;
	union arglist *al, *al2, *alb;
	struct params *a;
	struct symtab *p, **parr = NULL; /* XXX gcc */
	int i;

	/*
	 * Deal with fun(void) properly.
	 */
	if (nparams == 1 && lparam->sym && lparam->sym->stype == VOID)
		goto done;

	/*
	 * Generate a list for bfcode().
	 * Parameters were pushed in reverse order.
	 */
	if (nparams != 0)
		parr = tmpalloc(sizeof(struct symtab *) * nparams);

	if (nparams)
	    for (a = lparam, i = 0; a != NULL; a = a->prev) {
		p = a->sym;
		parr[i++] = p;
		if (p == NULL) {
			uerror("parameter %d name missing", i);
			p = &nulsym; /* empty symtab */
		}
		if (p->stype == FARG) {
			p->stype = INT;
			p->ssue = MKSUE(INT);
		}
		if (ISARY(p->stype)) {
			p->stype += (PTR-ARY);
			p->sdf++;
		} else if (ISFTN(p->stype)) {
			werror("function declared as argument");
			p->stype = INCREF(p->stype);
		}
#ifdef STABS
		if (gflag)
			stabs_newsym(p);
#endif
	}
	if (oldstyle && (df = cftnsp->sdf) && (al = df->dfun)) {
		/*
		 * Check against prototype of oldstyle function.
		 */
		alb = al2 = tmpalloc(sizeof(union arglist) * nparams * 3 + 1);
		for (i = 0; i < nparams; i++) {
			TWORD type = parr[i]->stype;
			(al2++)->type = type;
			if (ISSTR(BTYPE(type)))
				(al2++)->sue = parr[i]->ssue;
			while (!ISFTN(type) && !ISARY(type) && type > BTMASK)
				type = DECREF(type);
			if (type > BTMASK)
				(al2++)->df = parr[i]->sdf;
		}
		al2->type = TNULL;
		intcompare = 1;
		if (chkftn(al, alb))
			uerror("function doesn't match prototype");
		intcompare = 0;

	}

	if (oldstyle && nparams) {
		/* Must recalculate offset for oldstyle args here */
		argoff = ARGINIT;
		for (i = 0; i < nparams; i++) {
			parr[i]->soffset = NOOFFSET;
			oalloc(parr[i], &argoff);
		}
	}

done:	cendarg();

	plabel(prolab); /* after prolog, used in optimization */
	retlab = getlab();
	bfcode(parr, nparams);
	plabel(getlab()); /* used when spilling */
	if (parlink)
		ecomp(parlink);
	parlink = NIL;
	lparam = NULL;
	nparams = 0;
	symclear(1);	/* In case of function pointer args */
}

/*
 * Struct/union/enum symtab construction.
 */
static void
defstr(struct symtab *sp, int class)
{
	sp->ssue = permalloc(sizeof(struct suedef));
	sp->ssue->suesize = 0;
	sp->ssue->sylnk = NULL; 
	sp->ssue->suealign = 0;
	sp->sclass = class;
	if (class == STNAME)
		sp->stype = STRTY;
	else if (class == UNAME)
		sp->stype = UNIONTY;
}

/*
 * Declare a struct/union/enum tag.
 * If not found, create a new tag with UNDEF type.
 */
static struct symtab *
deftag(char *name, int class)
{
	struct symtab *sp;

	if ((sp = lookup(name, STAGNAME))->ssue == NULL) {
		/* New tag */
		defstr(sp, class);
	} else if (sp->sclass != class)
		uerror("tag %s redeclared", name);
	return sp;
}

/*
 * reference to a structure or union, with no definition
 */
NODE *
rstruct(char *tag, int soru)
{
	struct symtab *sp;

	sp = deftag(tag, soru);
	return mkty(sp->stype, 0, sp->ssue);
}

static int enumlow, enumhigh;
int enummer;

/*
 * Declare a member of enum.
 */
void
moedef(char *name)
{
	struct symtab *sp;

	sp = lookup(name, SNORMAL);
	if (sp->stype == UNDEF || (sp->slevel < blevel)) {
		if (sp->stype != UNDEF)
			sp = hide(sp);
		sp->stype = INT; /* always */
		sp->ssue = MKSUE(INT);
		sp->sclass = MOE;
		sp->soffset = enummer;
	} else
		uerror("%s redeclared", name);
	if (enummer < enumlow)
		enumlow = enummer;
	if (enummer > enumhigh)
		enumhigh = enummer;
	enummer++;
}

/*
 * Declare an enum tag.  Complain if already defined.
 */
struct symtab *
enumhd(char *name)
{
	struct symtab *sp;

	enummer = enumlow = enumhigh = 0;
	if (name == NULL)
		return NULL;

	sp = deftag(name, ENAME);
	if (sp->stype != UNDEF) {
		if (sp->slevel == blevel)
			uerror("%s redeclared", name);
		sp = hide(sp);
		defstr(sp, ENAME);
	}
	return sp;
}

/*
 * finish declaration of an enum
 */
NODE *
enumdcl(struct symtab *sp)
{
	NODE *p;
	TWORD t;

#ifdef ENUMSIZE
	t = ENUMSIZE(enumhigh, enumlow);
#else
	if (enumhigh <= MAX_CHAR && enumlow >= MIN_CHAR)
		t = ctype(CHAR);
	else if (enumhigh <= MAX_SHORT && enumlow >= MIN_SHORT)
		t = ctype(SHORT);
	else
		t = ctype(INT);
#endif
	if (sp) {
		sp->stype = t;
		sp->ssue = MKSUE(t);
	}
	p = mkty(t, 0, MKSUE(t));
	p->n_sp = sp;
	return p;
}

/*
 * Handle reference to an enum
 */
NODE *
enumref(char *name)
{
	struct symtab *sp;
	NODE *p;

	sp = lookup(name, STAGNAME);
	/*
	 * 6.7.2.3 Clause 2:
	 * "A type specifier of the form 'enum identifier' without an
	 *  enumerator list shall only appear after the type it specifies
	 *  is complete."
	 */
	if (sp->sclass != ENAME)
		uerror("enum %s undeclared", name);

	p = mkty(sp->stype, 0, sp->ssue);
	p->n_sp = sp;
	return p;
}

/*
 * begining of structure or union declaration
 */
struct rstack *
bstruct(char *name, int soru)
{
	struct rstack *r;
	struct symtab *sp;

	if (name != NULL) {
		sp = deftag(name, soru);
		if (sp->ssue->suealign != 0) {
			if (sp->slevel < blevel) {
				sp = hide(sp);
				defstr(sp, soru);
			} else
				uerror("%s redeclared", name);
		}
		sp->ssue->suealign = ALSTRUCT;
	} else
		sp = NULL;

	r = tmpcalloc(sizeof(struct rstack));
	r->rsou = soru;
	r->rsym = sp;
	r->rb = NULL;
	r->rnext = rpole;
	rpole = r;

	return r;
}

/*
 * Called after a struct is declared to restore the environment.
 */
NODE *
dclstruct(struct rstack *r)
{
	NODE *n;
	struct suedef *sue;
	struct symtab *sp;
	int al, sa, sz, coff;
	TWORD temp;

	if (pragma_allpacked && !pragma_packed)
		pragma_packed = pragma_allpacked;

	if (r->rsym == NULL) {
		sue = permalloc(sizeof(struct suedef));
		suedefcnt++;
		sue->suesize = 0;
		sue->suealign = ALSTRUCT;
	} else
		sue = r->rsym->ssue;
	if (sue->suealign == 0)  /* suealign == 0 is undeclared struct */
		sue->suealign = ALSTRUCT;

	temp = r->rsou == STNAME ? STRTY : UNIONTY;
	al = ALSTRUCT;

	coff = 0;
	if (pragma_packed || pragma_aligned)
		rpole->rstr = 0; /* must recount it */

	sue->sylnk = r->rb;
	for (sp = r->rb; sp; sp = sp->snext) {
		sa = talign(sp->stype, sp->ssue);
		if (sp->sclass & FIELD)
			sz = sp->sclass&FLDSIZ;
		else
			sz = tsize(sp->stype, sp->sdf, sp->ssue);

		if (pragma_packed || pragma_aligned) {
			/* XXX check pack/align sizes */
			sp->soffset = coff;
			if (pragma_aligned)
				coff += ALLDOUBLE;
			else
				coff += sz;
			rpole->rstr = coff;
		}

		if (sz > rpole->rstr)
			rpole->rstr = sz;  /* for use with unions */
		/*
		 * set al, the alignment, to the lcm of the alignments
		 * of the members.
		 */
		SETOFF(al, sa);
	}

	if (!pragma_packed && !pragma_aligned)
		SETOFF(rpole->rstr, al);

	sue->suesize = rpole->rstr;
	sue->suealign = al;

#ifdef PCC_DEBUG
	if (ddebug) {
		printf("dclstruct(%s): size=%d, align=%d\n",
		    r->rsym ? r->rsym->sname : "??",
		    sue->suesize, sue->suealign);
	}
#endif

	pragma_packed = pragma_aligned = 0;

#ifdef STABS
	if (gflag)
		stabs_struct(r->rsym, sue);
#endif

#ifdef PCC_DEBUG
	if (ddebug>1) {
		printf("\tsize %d align %d link %p\n",
		    sue->suesize, sue->suealign, sue->sylnk);
		for (sp = sue->sylnk; sp != NULL; sp = sp->snext) {
			printf("\tmember %s(%p)\n", sp->sname, sp);
		}
	}
#endif

	rpole = r->rnext;
	n = mkty(temp, 0, sue);
	return n;
}

/*
 * Add a new member to the current struct or union being declared.
 */
void
soumemb(NODE *n, char *name, int class)
{
	struct symtab *sp, *lsp;
	int incomp;
 
	if (rpole == NULL)
		cerror("soumemb");
 
	lsp = NULL;
	for (sp = rpole->rb; sp != NULL; lsp = sp, sp = sp->snext)
		if (sp->sname == name)
			uerror("redeclaration of %s", name);

	sp = getsymtab(name, SMOSNAME);
	if (rpole->rb == NULL)
		rpole->rb = sp;
	else
		lsp->snext = sp;
	n->n_sp = sp;
	if ((class & FIELD) == 0)
		class = rpole->rsou == STNAME ? MOS : MOU;
	defid(n, class);

	/*
	 * 6.7.2.1 clause 16:
	 * "...the last member of a structure with more than one
	 *  named member may have incomplete array type;"
	 */
	if (ISARY(sp->stype) && sp->sdf->ddim == NOOFFSET)
		incomp = 1;
	else
		incomp = 0;
	if ((rpole->flags & LASTELM) || (rpole->rb == sp && incomp == 1))
		uerror("incomplete array in struct");
	if (incomp == 1)
		rpole->flags |= LASTELM;

	/*
	 * 6.7.2.1 clause 2:
	 * "...such a structure shall not be a member of a structure
	 *  or an element of an array."
	 */
	if (sp->stype == STRTY && sp->ssue->sylnk) {
		struct symtab *lnk;

		for (lnk = sp->ssue->sylnk; lnk->snext; lnk = lnk->snext)
			;
		if (ISARY(lnk->stype) && lnk->sdf->ddim == NOOFFSET)
			uerror("incomplete struct in struct");
	}
}

/*
 * error printing routine in parser
 */
void yyerror(char *s);
void
yyerror(char *s)
{
	uerror(s);
}

void yyaccpt(void);
void
yyaccpt(void)
{
	ftnend();
}

/*
 * p is top of type list given to tymerge later.
 * Find correct CALL node and declare parameters from there.
 */
void
ftnarg(NODE *p)
{
	NODE *q;

#ifdef PCC_DEBUG
	if (ddebug > 2)
		printf("ftnarg(%p)\n", p);
#endif
	/*
	 * Push argument symtab entries onto param stack in reverse order,
	 * due to the nature of the stack it will be reclaimed correct.
	 */
	for (; p->n_op != NAME; p = p->n_left) {
		if (p->n_op == UCALL && p->n_left->n_op == NAME)
			return;	/* Nothing to enter */
		if (p->n_op == CALL && p->n_left->n_op == NAME)
			break;
	}

	p = p->n_right;
	while (p->n_op == CM) {
		q = p->n_right;
		if (q->n_op != ELLIPSIS) {
			ssave(q->n_sp);
			nparams++;
#ifdef PCC_DEBUG
			if (ddebug > 2)
				printf("	saving sym %s (%p) from (%p)\n",
				    q->n_sp->sname, q->n_sp, q);
#endif
		}
		p = p->n_left;
	}
	ssave(p->n_sp);
	if (p->n_type != VOID)
		nparams++;

#ifdef PCC_DEBUG
	if (ddebug > 2)
		printf("	saving sym %s (%p) from (%p)\n",
		    nparams ? p->n_sp->sname : "<noname>", p->n_sp, p);
#endif
}

/*
 * compute the alignment of an object with type ty, sizeoff index s
 */
int
talign(unsigned int ty, struct suedef *sue)
{
	int i;

	if (ISPTR(ty))
		return(ALPOINT); /* shortcut */

	if(sue == NULL && ty!=INT && ty!=CHAR && ty!=SHORT &&
	    ty!=UNSIGNED && ty!=UCHAR && ty!=USHORT) {
		return(fldal(ty));
	}

	for( i=0; i<=(SZINT-BTSHIFT-1); i+=TSHIFT ){
		switch( (ty>>i)&TMASK ){

		case FTN:
			cerror("compiler takes alignment of function");
		case PTR:
			return(ALPOINT);
		case ARY:
			continue;
		case 0:
			break;
			}
		}

	switch( BTYPE(ty) ){

	case UNIONTY:
	case STRTY:
		return((unsigned int)sue->suealign);
	case BOOL:
		return (ALBOOL);
	case CHAR:
	case UCHAR:
		return (ALCHAR);
	case FLOAT:
		return (ALFLOAT);
	case LDOUBLE:
		return (ALLDOUBLE);
	case DOUBLE:
		return (ALDOUBLE);
	case LONGLONG:
	case ULONGLONG:
		return (ALLONGLONG);
	case LONG:
	case ULONG:
		return (ALLONG);
	case SHORT:
	case USHORT:
		return (ALSHORT);
	default:
		return (ALINT);
	}
}

/* compute the size associated with type ty,
 *  dimoff d, and sizoff s */
/* BETTER NOT BE CALLED WHEN t, d, and s REFER TO A BIT FIELD... */
OFFSZ
tsize(TWORD ty, union dimfun *d, struct suedef *sue)
{

	int i;
	OFFSZ mult, sz;

	mult = 1;

	for( i=0; i<=(SZINT-BTSHIFT-1); i+=TSHIFT ){
		switch( (ty>>i)&TMASK ){

		case FTN:
			uerror( "cannot take size of function");
		case PTR:
			return( SZPOINT(ty) * mult );
		case ARY:
			if (d->ddim == NOOFFSET)
				return 0;
			mult *= d->ddim;
			d++;
			continue;
		case 0:
			break;

			}
		}

	if (sue == NULL)
		cerror("bad tsize sue");
	sz = sue->suesize;
#ifdef GCC_COMPAT
	if (ty == VOID)
		sz = SZCHAR;
#endif
	if (ty != STRTY && ty != UNIONTY) {
		if (sz == 0) {
			uerror("unknown size");
			return(SZINT);
		}
	} else {
		if (sue->suealign == 0)
			uerror("unknown structure/union/enum");
	}

	return((unsigned int)sz * mult);
}

/*
 * Save string (and print it out).  If wide then wide string.
 */
NODE *
strend(int wide, char *str)
{
	struct symtab *sp;
	NODE *p;

	/* If an identical string is already emitted, just forget this one */
	if (wide) {
		/* Do not save wide strings, at least not now */
		sp = getsymtab(str, SSTRING|STEMP);
	} else {
		str = addstring(str);	/* enter string in string table */
		sp = lookup(str, SSTRING);	/* check for existance */
	}

	if (sp->soffset == 0) { /* No string */
		char *wr;
		int i;

		sp->sclass = STATIC;
		sp->slevel = 1;
		sp->soffset = getlab();
		sp->squal = (CON >> TSHIFT);
		sp->sdf = permalloc(sizeof(union dimfun));
		if (wide) {
			sp->stype = WCHAR_TYPE+ARY;
			sp->ssue = MKSUE(WCHAR_TYPE);
		} else {
			if (funsigned_char) {
				sp->stype = UCHAR+ARY;
				sp->ssue = MKSUE(UCHAR);
			} else {
				sp->stype = CHAR+ARY;
				sp->ssue = MKSUE(CHAR);
			}
		}
		for (wr = sp->sname, i = 1; *wr; i++)
			if (*wr++ == '\\')
				(void)esccon(&wr);

		sp->sdf->ddim = i;
		if (wide)
			inwstring(sp);
		else
			instring(sp);
	}

	p = block(NAME, NIL, NIL, sp->stype, sp->sdf, sp->ssue);
	p->n_sp = sp;
	return(clocal(p));
}

/*
 * update the offset pointed to by poff; return the
 * offset of a value of size `size', alignment `alignment',
 * given that off is increasing
 */
int
upoff(int size, int alignment, int *poff)
{
	int off;

	off = *poff;
	SETOFF(off, alignment);
	if (off < 0)
		cerror("structure or stack overgrown"); /* wrapped */
	*poff = off+size;
	return (off);
}

/*
 * allocate p with offset *poff, and update *poff
 */
int
oalloc(struct symtab *p, int *poff )
{
	int al, off, tsz;
	int noff;

	/*
	 * Only generate tempnodes if we are optimizing,
	 * and only for integers, floats or pointers,
	 * and not if the type on this level is volatile.
	 */
	if (xtemps && ((p->sclass == AUTO) || (p->sclass == REGISTER)) &&
	    (p->stype < STRTY || ISPTR(p->stype)) &&
	    !(cqual(p->stype, p->squal) & VOL) && cisreg(p->stype)) {
		NODE *tn = tempnode(0, p->stype, p->sdf, p->ssue);
		p->soffset = regno(tn);
		p->sflags |= STNODE;
		nfree(tn);
		return 0;
	}

	al = talign(p->stype, p->ssue);
	noff = off = *poff;
	tsz = tsize(p->stype, p->sdf, p->ssue);
#ifdef BACKAUTO
	if (p->sclass == AUTO) {
		noff = off + tsz;
		if (noff < 0)
			cerror("stack overflow");
		SETOFF(noff, al);
		off = -noff;
	} else
#endif
	if (p->sclass == PARAM && (p->stype == CHAR || p->stype == UCHAR ||
	    p->stype == SHORT || p->stype == USHORT || p->stype == BOOL)) {
		off = upoff(SZINT, ALINT, &noff);
#ifndef RTOLBYTES
		off = noff - tsz;
#endif
	} else {
		off = upoff(tsz, al, &noff);
	}

	if (p->sclass != REGISTER) {
	/* in case we are allocating stack space for register arguments */
		if (p->soffset == NOOFFSET)
			p->soffset = off;
		else if(off != p->soffset)
			return(1);
	}

	*poff = noff;
	return(0);
}

/*
 * Delay emission of code generated in argument headers.
 */
static void
edelay(NODE *p)
{
	if (blevel == 1) {
		/* Delay until after declarations */
		if (parlink == NULL)
			parlink = p;
		else
			parlink = block(COMOP, parlink, p, 0, 0, 0);
	} else
		ecomp(p);
}

/*
 * Allocate space on the stack for dynamic arrays (or at least keep track
 * of the index).
 * Strategy is as follows:
 * - first entry is a pointer to the dynamic datatype.
 * - if it's a one-dimensional array this will be the only entry used.
 * - if it's a multi-dimensional array the following (numdim-1) integers
 *   will contain the sizes to multiply the indexes with.
 * - code to write the dimension sizes this will be generated here.
 * - code to allocate space on the stack will be generated here.
 */
static void
dynalloc(struct symtab *p, int *poff)
{
	union dimfun *df;
	NODE *n, *nn, *tn, *pol;
	TWORD t;
	int astkp, no;

	/*
	 * The pointer to the array is not necessarily stored in a
	 * TEMP node, but if it is, its number is in the soffset field;
	 */
	t = p->stype;
	astkp = 0;
	if (ISARY(t) && blevel == 1) {
		/* must take care of side effects of dynamic arg arrays */
		if (p->sdf->ddim < 0 && p->sdf->ddim != NOOFFSET) {
			/* first-level array will be indexed correct */
			edelay(arrstk[astkp++]);
		}
		p->sdf++;
		p->stype += (PTR-ARY);
		t = p->stype;
	}
	if (ISARY(t)) {
		p->sflags |= (STNODE|SDYNARRAY);
		p->stype = INCREF(p->stype); /* Make this an indirect pointer */
		tn = tempnode(0, p->stype, p->sdf, p->ssue);
		p->soffset = regno(tn);
	} else {
		oalloc(p, poff);
		tn = NIL;
	}

	df = p->sdf;

	pol = NIL;
	for (; t > BTMASK; t = DECREF(t)) {
		if (!ISARY(t))
			continue;
		if (df->ddim < 0) {
			n = arrstk[astkp++];
			do {
				nn = tempnode(0, INT, 0, MKSUE(INT));
				no = regno(nn);
			} while (no == -NOOFFSET);
			edelay(buildtree(ASSIGN, nn, n));

			df->ddim = -no;
			n = tempnode(no, INT, 0, MKSUE(INT));
		} else
			n = bcon(df->ddim);

		pol = (pol == NIL ? n : buildtree(MUL, pol, n));
		df++;
	}
	/* Create stack gap */
	if (blevel == 1) {
		if (tn)
			tfree(tn);
		if (pol)
			tfree(pol);
	} else {
		if (pol == NIL)
			uerror("aggregate dynamic array not allowed");
		if (tn)
			spalloc(tn, pol, tsize(t, 0, p->ssue));
	}
	arrstkp = 0;
}

/*
 * allocate a field of width w
 * new is 0 if new entry, 1 if redefinition, -1 if alignment
 */
int
falloc(struct symtab *p, int w, int new, NODE *pty)
{
	int al,sz,type;

	type = (new<0)? pty->n_type : p->stype;

	/* this must be fixed to use the current type in alignments */
	switch( new<0?pty->n_type:p->stype ){

	case CHAR:
	case UCHAR:
		al = ALCHAR;
		sz = SZCHAR;
		break;

	case SHORT:
	case USHORT:
		al = ALSHORT;
		sz = SZSHORT;
		break;

	case INT:
	case UNSIGNED:
		al = ALINT;
		sz = SZINT;
		break;

	default:
		if( new < 0 ) {
			uerror( "illegal field type" );
			al = ALINT;
		} else
			al = fldal( p->stype );
		sz =SZINT;
	}

	if( w > sz ) {
		uerror( "field too big");
		w = sz;
		}

	if( w == 0 ){ /* align only */
		SETOFF( rpole->rstr, al );
		if( new >= 0 ) uerror( "zero size field");
		return(0);
		}

	if( rpole->rstr%al + w > sz ) SETOFF( rpole->rstr, al );
	if( new < 0 ) {
		rpole->rstr += w;  /* we know it will fit */
		return(0);
		}

	/* establish the field */

	if( new == 1 ) { /* previous definition */
		if( p->soffset != rpole->rstr || p->sclass != (FIELD|w) ) return(1);
		}
	p->soffset = rpole->rstr;
	rpole->rstr += w;
	p->stype = type;
	fldty( p );
	return(0);
}

/*
 * handle unitialized declarations assumed to be not functions:
 * int a;
 * extern int a;
 * static int a;
 */
void
nidcl(NODE *p, int class)
{
	struct symtab *sp;
	int commflag = 0;

	/* compute class */
	if (class == SNULL) {
		if (blevel > 1)
			class = AUTO;
		else if (blevel != 0 || rpole)
			cerror( "nidcl error" );
		else /* blevel = 0 */
			commflag = 1, class = EXTERN;
	}

	defid(p, class);

	sp = p->n_sp;
	/* check if forward decl */
	if (ISARY(sp->stype) && sp->sdf->ddim == NOOFFSET)
		return;

	if (sp->sflags & SASG)
		return; /* already initialized */

	switch (class) {
	case EXTDEF:
		/* simulate initialization by 0 */
		simpleinit(p->n_sp, bcon(0));
		break;
	case EXTERN:
		if (commflag)
			lcommadd(p->n_sp);
		else
			extdec(p->n_sp);
		break;
	case STATIC:
		if (blevel == 0)
			lcommadd(p->n_sp);
		else
			defzero(p->n_sp);
		break;
	}
}

struct lcd {
	SLIST_ENTRY(lcd) next;
	struct symtab *sp;
};

static SLIST_HEAD(, lcd) lhead = { NULL, &lhead.q_forw};

/*
 * Add a local common statement to the printout list.
 */
void
lcommadd(struct symtab *sp)
{
	struct lcd *lc, *lcp;

	lcp = NULL;
	SLIST_FOREACH(lc, &lhead, next) {
		if (lc->sp == sp)
			return; /* already exists */
		if (lc->sp == NULL && lcp == NULL)
			lcp = lc;
	}
	if (lcp == NULL) {
		lc = permalloc(sizeof(struct lcd));
		lc->sp = sp;
		SLIST_INSERT_LAST(&lhead, lc, next);
	} else
		lcp->sp = sp;
}

/*
 * Delete a local common statement.
 */
void
lcommdel(struct symtab *sp)
{
	struct lcd *lc;

	SLIST_FOREACH(lc, &lhead, next) {
		if (lc->sp == sp) {
			lc->sp = NULL;
			return;
		}
	}
}

/*
 * Print out the remaining common statements.
 */
void
lcommprint(void)
{
	struct lcd *lc;

	SLIST_FOREACH(lc, &lhead, next) {
		if (lc->sp != NULL)
			defzero(lc->sp);
	}
}

/*
 * Merge given types to a single node.
 * Any type can end up here.
 * p is the old node, q is the old (if any).
 * CLASS is AUTO, EXTERN, REGISTER, STATIC or TYPEDEF.
 * QUALIFIER is VOL or CON
 * TYPE is CHAR, SHORT, INT, LONG, SIGNED, UNSIGNED, VOID, BOOL, FLOAT,
 * 	DOUBLE, STRTY, UNIONTY.
 */
NODE *
typenode(NODE *p)
{
	NODE *q, *saved;
	TWORD type;
	int class, qual;
	int sig, uns, cmplx;

	cmplx = type = class = qual = sig = uns = 0;
	saved = NIL;

	for (q = p; p; p = p->n_left) {
		switch (p->n_op) {
		case CLASS:
			if (class)
				goto bad; /* max 1 class */
			class = p->n_type;
			break;

		case QUALIFIER:
			qual |= p->n_type >> TSHIFT;
			break;

		case TYPE:
			if (p->n_sp != NULL || ISSOU(p->n_type)) {
				/* typedef, enum or struct/union */
				if (saved || type)
					goto bad;
				saved = p;
				break;
			} else if ((p->n_type == SIGNED && uns) ||
			    (p->n_type == UNSIGNED && sig))
				goto bad;

			switch (p->n_type) {
			case BOOL:
			case CHAR:
			case FLOAT:
			case VOID:
				if (type)
					goto bad;
				type = p->n_type;
				break;
			case DOUBLE:
				if (type == 0)
					type = DOUBLE;
				else if (type == LONG)
					type = LDOUBLE;
				else
					goto bad;
				break;
			case SHORT:
				if (type == 0 || type == INT)
					type = SHORT;
				else
					goto bad;
				break;
			case INT:
				if (type == SHORT || type == LONG ||
				    type == LONGLONG)
					break;
				else if (type == 0)
					type = INT;
				else
					goto bad;
				break;
			case LONG:
				if (type == 0)
					type = LONG;
				else if (type == INT)
					break;
				else if (type == LONG)
					type = LONGLONG;
				else if (type == DOUBLE)
					type = LDOUBLE;
				else
					goto bad;
				break;
			case SIGNED:
				if (sig || uns)
					goto bad;
				sig = 1;
				break;
			case UNSIGNED:
				if (sig || uns)
					goto bad;
				uns = 1;
				break;
			case COMPLEX:
				cmplx = 1;
				break;
			default:
				cerror("typenode");
			}
		}
	}
	if (cmplx) {
		if (sig || uns)
			goto bad;
		switch (type) {
		case FLOAT:
			type = FCOMPLEX;
			break;
		case DOUBLE:
			type = COMPLEX;
			break;
		case LDOUBLE:
			type = LCOMPLEX;
			break;
		default:
			goto bad;
		}
	}

	if (saved && type)
		goto bad;
	if (sig || uns) {
		if (type == 0)
			type = sig ? INT : UNSIGNED;
		if (type > ULONGLONG)
			goto bad;
		if (uns)
			type = ENUNSIGN(type);
	}

	if (funsigned_char && type == CHAR && sig == 0)
		type = UCHAR;

	/* free the chain */
	while (q) {
		p = q->n_left;
		if (q != saved)
			nfree(q);
		q = p;
	}

	p = (saved ? saved : block(TYPE, NIL, NIL, type, 0, 0));
	p->n_qual = qual;
	p->n_lval = class;
	if (BTYPE(p->n_type) == UNDEF)
		MODTYPE(p->n_type, INT);
	return p;

bad:	uerror("illegal type combination");
	return mkty(INT, 0, 0);
}

struct tylnk {
	struct tylnk *next;
	union dimfun df;
};

static void tyreduce(NODE *p, struct tylnk **, int *);

static void
tylkadd(union dimfun dim, struct tylnk **tylkp, int *ntdim)
{
	(*tylkp)->next = tmpalloc(sizeof(struct tylnk));
	*tylkp = (*tylkp)->next;
	(*tylkp)->next = NULL;
	(*tylkp)->df = dim;
	(*ntdim)++;
}

/* merge type typ with identifier idp  */
NODE *
tymerge(NODE *typ, NODE *idp)
{
	NODE *p;
	union dimfun *j;
	struct tylnk *base, tylnk, *tylkp;
	unsigned int t;
	int ntdim, i;

	if (typ->n_op != TYPE)
		cerror("tymerge: arg 1");

#ifdef PCC_DEBUG
	if (ddebug > 2) {
		printf("tymerge(%p,%p)\n", typ, idp);
		fwalk(typ, eprint, 0);
		fwalk(idp, eprint, 0);
	}
#endif

	idp->n_type = typ->n_type;
	idp->n_qual |= typ->n_qual;

	tylkp = &tylnk;
	tylkp->next = NULL;
	ntdim = 0;

	tyreduce(idp, &tylkp, &ntdim);
	idp->n_sue = typ->n_sue;

	for (t = typ->n_type, j = typ->n_df; t&TMASK; t = DECREF(t))
		if (ISARY(t) || ISFTN(t))
			tylkadd(*j++, &tylkp, &ntdim);

	if (ntdim) {
		union dimfun *a = permalloc(sizeof(union dimfun) * ntdim);
		dimfuncnt += ntdim;
		for (i = 0, base = tylnk.next; base; base = base->next, i++)
			a[i] = base->df;
		idp->n_df = a;
	} else
		idp->n_df = NULL;

	/* now idp is a single node: fix up type */

	idp->n_type = ctype(idp->n_type);

	/* in case ctype has rewritten things */
	if ((t = BTYPE(idp->n_type)) != STRTY && t != UNIONTY)
		idp->n_sue = MKSUE(t);

	if (idp->n_op != NAME) {
		for (p = idp->n_left; p->n_op != NAME; p = p->n_left)
			nfree(p);
		nfree(p);
		idp->n_op = NAME;
	}

	return(idp);
}

/*
 * Retrieve all CM-separated argument types, sizes and dimensions and
 * put them in an array.
 * XXX - can only check first type level, side effects?
 */
static union arglist *
arglist(NODE *n)
{
	union arglist *al;
	NODE *w = n, **ap;
	int num, cnt, i, j, k;
	TWORD ty;

#ifdef PCC_DEBUG
	if (pdebug) {
		printf("arglist %p\n", n);
		fwalk(n, eprint, 0);
	}
#endif
	/* First: how much to allocate */
	for (num = cnt = 0, w = n; w->n_op == CM; w = w->n_left) {
		cnt++;	/* Number of levels */
		num++;	/* At least one per step */
		if (w->n_right->n_op == ELLIPSIS)
			continue;
		ty = w->n_right->n_type;
		if (BTYPE(ty) == STRTY || BTYPE(ty) == UNIONTY)
			num++;
		while (ISFTN(ty) == 0 && ISARY(ty) == 0 && ty > BTMASK)
			ty = DECREF(ty);
		if (ty > BTMASK)
			num++;
	}
	cnt++;
	ty = w->n_type;
	if (BTYPE(ty) == STRTY || BTYPE(ty) == UNIONTY)
		num++;
	while (ISFTN(ty) == 0 && ISARY(ty) == 0 && ty > BTMASK)
		ty = DECREF(ty);
	if (ty > BTMASK)
		num++;
	num += 2; /* TEND + last arg type */

	/* Second: Create list to work on */
	ap = tmpalloc(sizeof(NODE *) * cnt);
	al = permalloc(sizeof(union arglist) * num);
	arglistcnt += num;

	for (w = n, i = 0; w->n_op == CM; w = w->n_left)
		ap[i++] = w->n_right;
	ap[i] = w;

	/* Third: Create actual arg list */
	for (k = 0, j = i; j >= 0; j--) {
		if (ap[j]->n_op == ELLIPSIS) {
			al[k++].type = TELLIPSIS;
			ap[j]->n_op = ICON; /* for tfree() */
			continue;
		}
		/* Convert arrays to pointers */
		if (ISARY(ap[j]->n_type)) {
			ap[j]->n_type += (PTR-ARY);
			ap[j]->n_df++;
		}
		/* Convert (silently) functions to pointers */
		if (ISFTN(ap[j]->n_type))
			ap[j]->n_type = INCREF(ap[j]->n_type);
		ty = ap[j]->n_type;
		al[k++].type = ty;
		if (BTYPE(ty) == STRTY || BTYPE(ty) == UNIONTY)
			al[k++].sue = ap[j]->n_sue;
		while (ISFTN(ty) == 0 && ISARY(ty) == 0 && ty > BTMASK)
			ty = DECREF(ty);
		if (ty > BTMASK)
			al[k++].df = ap[j]->n_df;
	}
	al[k++].type = TNULL;
	if (k > num)
		cerror("arglist: k%d > num%d", k, num);
	tfree(n);
#ifdef PCC_DEBUG
	if (pdebug)
		alprint(al, 0);
#endif
	return al;
}

/*
 * build a type, and stash away dimensions,
 * from a parse tree of the declaration
 * the type is build top down, the dimensions bottom up
 */
void
tyreduce(NODE *p, struct tylnk **tylkp, int *ntdim)
{
	union dimfun dim;
	NODE *r = NULL;
	int o;
	TWORD t, q;

	o = p->n_op;
	if (o == NAME) {
		p->n_qual = DECQAL(p->n_qual);
		return;
	}

	t = INCREF(p->n_type);
	q = p->n_qual;
	switch (o) {
	case CALL:
		t += (FTN-PTR);
		dim.dfun = arglist(p->n_right);
		break;
	case UCALL:
		t += (FTN-PTR);
		dim.dfun = NULL;
		break;
	case LB:
		t += (ARY-PTR);
		if (p->n_right->n_op != ICON) {
			r = p->n_right;
			o = RB;
		} else {
			dim.ddim = p->n_right->n_lval;
			nfree(p->n_right);
#ifdef notdef
	/* XXX - check dimensions at usage time */
			if (dim.ddim == NOOFFSET && p->n_left->n_op == LB)
				uerror("null dimension");
#endif
		}
		break;
	}

	p->n_left->n_type = t;
	p->n_left->n_qual = INCQAL(q) | p->n_left->n_qual;
	tyreduce(p->n_left, tylkp, ntdim);

	if (o == LB || o == (UCALL) || o == CALL)
		tylkadd(dim, tylkp, ntdim);
	if (o == RB) {
		dim.ddim = -1;
		tylkadd(dim, tylkp, ntdim);
		arrstk[arrstkp++] = r;
	}

	p->n_sp = p->n_left->n_sp;
	p->n_type = p->n_left->n_type;
	p->n_qual = p->n_left->n_qual;
}

static NODE *
argcast(NODE *p, TWORD t, union dimfun *d, struct suedef *sue)
{
	NODE *u, *r = talloc();

	r->n_op = NAME;
	r->n_type = t;
	r->n_qual = 0; /* XXX */
	r->n_df = d;
	r->n_sue = sue;

	u = buildtree(CAST, r, p);
	nfree(u->n_left);
	r = u->n_right;
	nfree(u);
	return r;
}

#ifndef NO_C_BUILTINS
/*
 * replace an alloca function with direct allocation on stack.
 * return a destination temp node.
 */
static NODE *
builtin_alloca(NODE *f, NODE *a)
{
	struct symtab *sp;
	NODE *t, *u;

#ifdef notyet
	if (xnobuiltins)
		return NULL;
#endif
	sp = f->n_sp;

	if (a == NULL || a->n_op == CM) {
		uerror("wrong arg count for alloca");
		return bcon(0);
	}
	t = tempnode(0, VOID|PTR, 0, MKSUE(INT) /* XXX */);
	u = tempnode(regno(t), VOID|PTR, 0, MKSUE(INT) /* XXX */);
	spalloc(t, a, SZCHAR);
	tfree(f);
	return u;
}

/*
 * Determine if a value is known to be constant at compile-time and
 * hence that PCC can perform constant-folding on expressions involving
 * that value.
 */
static NODE *
builtin_constant_p(NODE *f, NODE *a)
{
	int isconst = (a != NULL && a->n_op == ICON);

	tfree(f);
	tfree(a);

	return bcon(isconst);
}

#ifndef TARGET_STDARGS
static NODE *
builtin_stdarg_start(NODE *f, NODE *a)
{
	NODE *p, *q;
	int sz;

	/* check num args and type */
	if (a == NULL || a->n_op != CM || a->n_left->n_op == CM ||
	    !ISPTR(a->n_left->n_type))
		goto bad;

	/* must first deal with argument size; use int size */
	p = a->n_right;
	if (p->n_type < INT) {
		sz = SZINT/tsize(p->n_type, p->n_df, p->n_sue);
	} else
		sz = 1;

	/* do the real job */
	p = buildtree(ADDROF, p, NIL); /* address of last arg */
#ifdef BACKAUTO
	p = optim(buildtree(PLUS, p, bcon(sz))); /* add one to it (next arg) */
#else
	p = optim(buildtree(MINUS, p, bcon(sz))); /* add one to it (next arg) */
#endif
	q = block(NAME, NIL, NIL, PTR+VOID, 0, 0); /* create cast node */
	q = buildtree(CAST, q, p); /* cast to void * (for assignment) */
	p = q->n_right;
	nfree(q->n_left);
	nfree(q);
	p = buildtree(ASSIGN, a->n_left, p); /* assign to ap */
	tfree(f);
	nfree(a);
	return p;
bad:
	uerror("bad argument to __builtin_stdarg_start");
	return bcon(0);
}

static NODE *
builtin_va_arg(NODE *f, NODE *a)
{
	NODE *p, *q, *r, *rv;
	int sz, nodnum;

	/* check num args and type */
	if (a == NULL || a->n_op != CM || a->n_left->n_op == CM ||
	    !ISPTR(a->n_left->n_type) || a->n_right->n_op != TYPE)
		goto bad;

	/* create a copy to a temp node of current ap */
	p = tcopy(a->n_left);
	q = tempnode(0, p->n_type, p->n_df, p->n_sue);
	nodnum = regno(q);
	rv = buildtree(ASSIGN, q, p);

	r = a->n_right;
	sz = tsize(r->n_type, r->n_df, r->n_sue)/SZCHAR;
	/* add one to ap */
#ifdef BACKAUTO
	rv = buildtree(COMOP, rv , buildtree(PLUSEQ, a->n_left, bcon(sz)));
#else
#error fix wrong eval order in builtin_va_arg
	ecomp(buildtree(MINUSEQ, a->n_left, bcon(sz)));
#endif

	nfree(a->n_right);
	nfree(a);
	nfree(f);
	r = tempnode(nodnum, INCREF(r->n_type), r->n_df, r->n_sue);
	return buildtree(COMOP, rv, buildtree(UMUL, r, NIL));
bad:
	uerror("bad argument to __builtin_va_arg");
	return bcon(0);

}

static NODE *
builtin_va_end(NODE *f, NODE *a)
{
	tfree(f);
	tfree(a);
	return bcon(0); /* nothing */
}

static NODE *
builtin_va_copy(NODE *f, NODE *a)
{
	if (a == NULL || a->n_op != CM || a->n_left->n_op == CM)
		goto bad;
	tfree(f);
	f = buildtree(ASSIGN, a->n_left, a->n_right);
	nfree(a);
	return f;

bad:
	uerror("bad argument to __builtin_va_copy");
	return bcon(0);
}
#endif /* TARGET_STDARGS */

static struct bitable {
	char *name;
	NODE *(*fun)(NODE *f, NODE *a);
} bitable[] = {
	{ "__builtin_alloca", builtin_alloca },
	{ "__builtin_constant_p", builtin_constant_p },
#ifndef TARGET_STDARGS
	{ "__builtin_stdarg_start", builtin_stdarg_start },
	{ "__builtin_va_arg", builtin_va_arg },
	{ "__builtin_va_end", builtin_va_end },
	{ "__builtin_va_copy", builtin_va_copy },
#endif
#ifdef TARGET_BUILTINS
	TARGET_BUILTINS
#endif
};
#endif

#ifdef PCC_DEBUG
/*
 * Print a prototype.
 */
static void
alprint(union arglist *al, int in)
{
	int i = 0, j;

	for (; al->type != TNULL; al++) {
		for (j = in; j > 0; j--)
			printf("  ");
		printf("arg %d: ", i++);
		tprint(stdout, al->type, 0);
		if (ISARY(al->type)) {
			printf(" dim %d\n", al->df->ddim);
		} else if (BTYPE(al->type) == STRTY ||
		    BTYPE(al->type) == UNIONTY) {
			al++;
			printf(" (size %d align %d)", al->sue->suesize,
			    al->sue->suealign);
		} else if (ISFTN(DECREF(al->type))) {
			al++;
			alprint(al->df->dfun, in+1);
		}
		printf("\n");
	}
	if (in == 0)
		printf("end arglist\n");
}
#endif
/*
 * Do prototype checking and add conversions before calling a function.
 * Argument f is function and a is a CM-separated list of arguments.
 * Returns a merged node (via buildtree() of function and arguments.
 */
NODE *
doacall(NODE *f, NODE *a)
{
	NODE *w, *r;
	union arglist *al;
	struct ap {
		struct ap *next;
		NODE *node;
	} *at, *apole = NULL;
	int argidx/* , hasarray = 0*/;
	TWORD type, arrt;

#ifdef PCC_DEBUG
	if (ddebug) {
		printf("doacall.\n");
		fwalk(f, eprint, 0);
		if (a)
			fwalk(a, eprint, 0);
	}
#endif

	/* First let MD code do something */
	calldec(f, a);
/* XXX XXX hack */
	if ((f->n_op == CALL) &&
	    f->n_left->n_op == ADDROF &&
	    f->n_left->n_left->n_op == NAME &&
	    (f->n_left->n_left->n_type & 0x7e0) == 0x4c0)
		goto build;
/* XXX XXX hack */

#ifndef NO_C_BUILTINS
	/* check for builtins. function pointers are not allowed */
	if (f->n_op == NAME &&
	    f->n_sp->sname[0] == '_' && f->n_sp->sname[1] == '_') {
		int i;

		for (i = 0; i < (int)(sizeof(bitable)/sizeof(bitable[0])); i++) {
			if (strcmp(bitable[i].name, f->n_sp->sname) == 0)
				return (*bitable[i].fun)(f, a);
		}
	}
#endif
	/*
	 * Do some basic checks.
	 */
	if (f->n_df == NULL || (al = f->n_df[0].dfun) == NULL) {
		if (Wimplicit_function_declaration) {
			if (f->n_sp != NULL) {
				if (strncmp(f->n_sp->sname,
				    "__builtin", 9) != 0)
					werror("no prototype for function "
					    "'%s()'", f->n_sp->sname);
			} else {
				werror("no prototype for function pointer");
			}
		}
		/* floats must be cast to double */
		if (a == NULL)
			goto build;
		for (w = a; w->n_op == CM; w = w->n_left) {
			if (w->n_right->n_op == TYPE)
				uerror("type is not an argument");
			if (w->n_right->n_type != FLOAT)
				continue;
			w->n_right = argcast(w->n_right, DOUBLE,
			    NULL, MKSUE(DOUBLE));
		}
		if (a->n_op == TYPE)
			uerror("type is not an argument");
		if (a->n_type == FLOAT) {
			MKTY(a, DOUBLE, 0, 0);
		}
		goto build;
	}
	if (al->type == VOID) {
		if (a != NULL)
			uerror("function takes no arguments");
		goto build; /* void function */
	} else {
		if (a == NULL) {
			uerror("function needs arguments");
			goto build;
		}
	}
#ifdef PCC_DEBUG
	if (pdebug) {
		printf("arglist for %p\n",
		    f->n_sp != NULL ? f->n_sp->sname : "function pointer");
		alprint(al, 0);
	}
#endif

	/*
	 * Create a list of pointers to the nodes given as arg.
	 */
	for (w = a; w->n_op == CM; w = w->n_left) {
		at = tmpalloc(sizeof(struct ap));
		at->node = w->n_right;
		at->next = apole;
		apole = at;
	}
	at = tmpalloc(sizeof(struct ap));
	at->node = w;
	at->next = apole;
	apole = at;

	/*
	 * Do the typechecking by walking up the list.
	 */
	argidx = 1;
	while (al->type != TNULL) {
		if (al->type == TELLIPSIS) {
			/* convert the rest of float to double */
			for (; apole; apole = apole->next) {
				if (apole->node->n_type != FLOAT)
					continue;
				MKTY(apole->node, DOUBLE, 0, 0);
			}
			goto build;
		}
		if (apole == NULL) {
			uerror("too few arguments to function");
			goto build;
		}
/* al = prototyp, apole = argument till ftn */
/* type = argumentets typ, arrt = prototypens typ */
		type = apole->node->n_type;
		arrt = al->type;
#if 0
		if ((hasarray = ISARY(arrt)))
			arrt += (PTR-ARY);
#endif
		/* Taking addresses of arrays are meaningless in expressions */
		/* but people tend to do that and also use in prototypes */
		/* this is mostly a problem with typedefs */
		if (ISARY(type)) {
			if (ISPTR(arrt) && ISARY(DECREF(arrt)))
				type = INCREF(type);
			else
				type += (PTR-ARY);
		} else if (ISPTR(type) && !ISARY(DECREF(type)) &&
		    ISPTR(arrt) && ISARY(DECREF(arrt))) {
			type += (ARY-PTR);
			type = INCREF(type);
		}

		/* Check structs */
		if (type <= BTMASK && arrt <= BTMASK) {
			if (type != arrt) {
				if (ISSOU(BTYPE(type)) || ISSOU(BTYPE(arrt))) {
incomp:					uerror("incompatible types for arg %d",
					    argidx);
				} else {
					MKTY(apole->node, arrt, 0, 0)
				}
			} else if (ISSOU(BTYPE(type))) {
				if (apole->node->n_sue != al[1].sue)
					goto incomp;
			}
			goto out;
		}

		/* XXX should (recusively) check return type and arg list of
		   func ptr arg XXX */
		if (ISFTN(DECREF(arrt)) && ISFTN(type))
			type = INCREF(type);

		/* Hereafter its only pointers (or arrays) left */
		/* Check for struct/union intermixing with other types */
		if (((type <= BTMASK) && ISSOU(BTYPE(type))) ||
		    ((arrt <= BTMASK) && ISSOU(BTYPE(arrt))))
			goto incomp;

		/* Check for struct/union compatibility */
		if (type == arrt) {
			if (ISSOU(BTYPE(type))) {
				if (apole->node->n_sue == al[1].sue)
					goto out;
			} else
				goto out;
		}
		if (BTYPE(arrt) == VOID && type > BTMASK)
			goto skip; /* void *f = some pointer */
		if (arrt > BTMASK && BTYPE(type) == VOID)
			goto skip; /* some *f = void pointer */
		if (apole->node->n_op == ICON && apole->node->n_lval == 0)
			goto skip; /* Anything assigned a zero */

		if ((type & ~BTMASK) == (arrt & ~BTMASK)) {
			/* do not complain for pointers with signedness */
			if (!Wpointer_sign &&
			    DEUNSIGN(BTYPE(type)) == DEUNSIGN(BTYPE(arrt)))
				goto skip;
		}

		werror("implicit conversion of argument %d due to prototype",
		    argidx);

skip:		if (ISSTR(BTYPE(arrt))) {
			MKTY(apole->node, arrt, 0, al[1].sue)
		} else {
			MKTY(apole->node, arrt, 0, 0)
		}

out:		al++;
		if (ISSTR(BTYPE(arrt)))
			al++;
#if 0
		while (arrt > BTMASK && !ISFTN(arrt))
			arrt = DECREF(arrt);
		if (ISFTN(arrt) || hasarray)
			al++;
#else
		while (arrt > BTMASK) {
			if (ISARY(arrt) || ISFTN(arrt)) {
				al++;
				break;
			}
			arrt = DECREF(arrt);
		}
#endif
		apole = apole->next;
		argidx++;
	}
	if (apole != NULL)
		uerror("too many arguments to function");

build:	return buildtree(a == NIL ? UCALL : CALL, f, a);
}

static int
chk2(TWORD type, union dimfun *dsym, union dimfun *ddef)
{
	while (type > BTMASK) {
		switch (type & TMASK) {
		case ARY:
			/* may be declared without dimension */
			if (dsym->ddim == NOOFFSET)
				dsym->ddim = ddef->ddim;
			if (ddef->ddim != NOOFFSET && dsym->ddim != ddef->ddim)
				return 1;
			dsym++, ddef++;
			break;
		case FTN:
			/* old-style function headers with function pointers
			 * will most likely not have a prototype.
			 * This is not considered an error.  */
			if (ddef->dfun == NULL) {
#ifdef notyet
				werror("declaration not a prototype");
#endif
			} else if (chkftn(dsym->dfun, ddef->dfun))
				return 1;
			dsym++, ddef++;
			break;
		}
		type = DECREF(type);
	}
	return 0;
}

/*
 * Compare two function argument lists to see if they match.
 */
int
chkftn(union arglist *usym, union arglist *udef)
{
	TWORD t2;
	int ty, tyn;

	if (usym == NULL)
		return 0;
	if (cftnsp != NULL && udef == NULL && usym->type == VOID)
		return 0; /* foo() { function with foo(void); prototype */
	if (udef == NULL && usym->type != TNULL)
		return 1;
	while (usym->type != TNULL) {
		if (usym->type == udef->type)
			goto done;
		/*
		 * If an old-style declaration, then all types smaller than
		 * int are given as int parameters.
		 */
		if (intcompare) {
			ty = BTYPE(usym->type);
			tyn = BTYPE(udef->type);
			if (ty == tyn || ty != INT)
				return 1;
			if (tyn == CHAR || tyn == UCHAR ||
			    tyn == SHORT || tyn == USHORT)
				goto done;
			return 1;
		} else
			return 1;

done:		ty = BTYPE(usym->type);
		t2 = usym->type;
		if (ISSTR(ty)) {
			usym++, udef++;
			if (usym->sue != udef->sue)
				return 1;
		}

		while (ISFTN(t2) == 0 && ISARY(t2) == 0 && t2 > BTMASK)
			t2 = DECREF(t2);
		if (t2 > BTMASK) {
			usym++, udef++;
			if (chk2(t2, usym->df, udef->df))
				return 1;
		}
		usym++, udef++;
	}
	if (usym->type != udef->type)
		return 1;
	return 0;
}

void
fixtype(NODE *p, int class)
{
	unsigned int t, type;
	int mod1, mod2;
	/* fix up the types, and check for legality */

	if( (type = p->n_type) == UNDEF ) return;
	if ((mod2 = (type&TMASK))) {
		t = DECREF(type);
		while( mod1=mod2, mod2 = (t&TMASK) ){
			if( mod1 == ARY && mod2 == FTN ){
				uerror( "array of functions is illegal" );
				type = 0;
				}
			else if( mod1 == FTN && ( mod2 == ARY || mod2 == FTN ) ){
				uerror( "function returns illegal type" );
				type = 0;
				}
			t = DECREF(t);
			}
		}

	/* detect function arguments, watching out for structure declarations */
	if (rpole && ISFTN(type)) {
		uerror("function illegal in structure or union");
		type = INCREF(type);
	}
	p->n_type = type;
}

/*
 * give undefined version of class
 */
int
uclass(int class)
{
	if (class == SNULL)
		return(EXTERN);
	else if (class == STATIC)
		return(USTATIC);
	else if (class == FORTRAN)
		return(UFORTRAN);
	else
		return(class);
}

int
fixclass(int class, TWORD type)
{
	extern int fun_inline;

	/* first, fix null class */
	if (class == SNULL) {
		if (fun_inline && ISFTN(type))
			return SNULL;
		if (rpole)
			class = rpole->rsou == STNAME ? MOS : MOU;
		else if (blevel == 0)
			class = EXTDEF;
		else
			class = AUTO;
	}

	/* now, do general checking */

	if( ISFTN( type ) ){
		switch( class ) {
		default:
			uerror( "function has illegal storage class" );
		case AUTO:
			class = EXTERN;
		case EXTERN:
		case EXTDEF:
		case FORTRAN:
		case TYPEDEF:
		case STATIC:
		case UFORTRAN:
		case USTATIC:
			;
			}
		}

	if (class & FIELD) {
		if (rpole && rpole->rsou != STNAME && rpole->rsou != UNAME)
			uerror("illegal use of field");
		return(class);
	}

	switch (class) {

	case MOS:
	case MOU:
		if (rpole == NULL)
			uerror("illegal member class");
		return(class);

	case REGISTER:
		if (blevel == 0)
			uerror("illegal register declaration");
		if (blevel == 1)
			return(PARAM);
		else
			return(AUTO);

	case AUTO:
		if( blevel < 2 ) uerror( "illegal ULABEL class" );
		return( class );

	case UFORTRAN:
	case FORTRAN:
# ifdef NOFORTRAN
		NOFORTRAN;    /* a condition which can regulate the FORTRAN usage */
# endif
		if( !ISFTN(type) ) uerror( "fortran declaration must apply to function" );
		else {
			type = DECREF(type);
			if( ISFTN(type) || ISARY(type) || ISPTR(type) ) {
				uerror( "fortran function has wrong type" );
				}
			}
	case EXTERN:
	case STATIC:
	case EXTDEF:
	case TYPEDEF:
	case USTATIC:
	case PARAM:
		return( class );

	default:
		cerror( "illegal class: %d", class );
		/* NOTREACHED */

	}
	return 0; /* XXX */
}

/*
 * Generates a goto statement; sets up label number etc.
 */
void
gotolabel(char *name)
{
	struct symtab *s = lookup(name, SLBLNAME);

	if (s->soffset == 0)
		s->soffset = -getlab();
	branch(s->soffset < 0 ? -s->soffset : s->soffset);
}

/*
 * Sets a label for gotos.
 */
void
deflabel(char *name)
{
	struct symtab *s = lookup(name, SLBLNAME);

	if (s->soffset > 0)
		uerror("label '%s' redefined", name);
	if (s->soffset == 0)
		s->soffset = getlab();
	if (s->soffset < 0)
		s->soffset = -s->soffset;
	plabel( s->soffset);
}

struct symtab *
getsymtab(char *name, int flags)
{
	struct symtab *s;

	if (flags & STEMP) {
		s = tmpalloc(sizeof(struct symtab));
	} else {
		s = permalloc(sizeof(struct symtab));
		symtabcnt++;
	}
	s->sname = s->soname = name;
	s->snext = NULL;
	s->stype = UNDEF;
	s->squal = 0;
	s->sclass = SNULL;
	s->sflags = flags & SMASK;
	s->soffset = 0;
	s->slevel = blevel;
	s->sdf = NULL;
	s->ssue = NULL;
	return s;
}

int
fldchk(int sz)
{
	if (rpole->rsou != STNAME && rpole->rsou != UNAME)
		uerror("field outside of structure");
	if (sz < 0 || sz >= FIELD) {
		uerror("illegal field size");
		return 1;
	}
	return 0;
}

#ifdef PCC_DEBUG
static char *
ccnames[] = { /* names of storage classes */
	"SNULL",
	"AUTO",
	"EXTERN",
	"STATIC",
	"REGISTER",
	"EXTDEF",
	"LABEL",
	"ULABEL",
	"MOS",
	"PARAM",
	"STNAME",
	"MOU",
	"UNAME",
	"TYPEDEF",
	"FORTRAN",
	"ENAME",
	"MOE",
	"UFORTRAN",
	"USTATIC",
	};

char *
scnames(int c)
{
	/* return the name for storage class c */
	static char buf[12];
	if( c&FIELD ){
		snprintf( buf, sizeof(buf), "FIELD[%d]", c&FLDSIZ );
		return( buf );
		}
	return( ccnames[c] );
	}
#endif

void
sspinit()
{
	NODE *p;

	p = block(NAME, NIL, NIL, FTN+VOID, 0, MKSUE(VOID));
	p->n_sp = lookup("__stack_chk_fail", SNORMAL);
	defid(p, EXTERN);
	nfree(p);

	p = block(NAME, NIL, NIL, INT, 0, MKSUE(INT));
	p->n_sp = lookup("__stack_chk_guard", SNORMAL);
	defid(p, EXTERN);
	nfree(p);
}

void
sspstart()
{
	NODE *p, *q;

	q = block(NAME, NIL, NIL, INT, 0, MKSUE(INT));
 	q->n_sp = lookup("__stack_chk_guard", SNORMAL);
	q = clocal(q);

	p = block(REG, NIL, NIL, INT, 0, 0);
	p->n_lval = 0;
	p->n_rval = FPREG;
	q = block(ER, p, q, INT, 0, MKSUE(INT));
	q = clocal(q);

	p = block(NAME, NIL, NIL, INT, 0, MKSUE(INT));
	p->n_sp = lookup("__stack_chk_canary", SNORMAL);
	defid(p, AUTO);
	p = clocal(p);

	ecomp(buildtree(ASSIGN, p, q));
}

void
sspend()
{
	NODE *p, *q;
	TWORD t;
	int tmpnr = 0;
	int lab;

	if (retlab != NOLAB) {
		plabel(retlab);
		retlab = getlab();
	}

	t = DECREF(cftnsp->stype);
	if (t == BOOL)
		t = BOOL_TYPE;

	if (t != VOID && !ISSOU(t)) {
		p = tempnode(0, t, cftnsp->sdf, cftnsp->ssue);
		tmpnr = regno(p);
		q = block(REG, NIL, NIL, t, cftnsp->sdf, cftnsp->ssue);
		q->n_rval = RETREG(t);
		ecomp(buildtree(ASSIGN, p, q));
	}

	p = block(NAME, NIL, NIL, INT, 0, MKSUE(INT));
	p->n_sp = lookup("__stack_chk_canary", SNORMAL);
	p = clocal(p);

	q = block(REG, NIL, NIL, INT, 0, 0);
	q->n_lval = 0;
	q->n_rval = FPREG;
	q = block(ER, p, q, INT, 0, MKSUE(INT));

	p = block(NAME, NIL, NIL, INT, 0, MKSUE(INT));
	p->n_sp = lookup("__stack_chk_guard", SNORMAL);
	p = clocal(p);

	lab = getlab();
	cbranch(buildtree(EQ, p, q), bcon(lab));

	p = block(NAME, NIL, NIL, FTN+VOID, 0, MKSUE(VOID));
	p->n_sp = lookup("__stack_chk_fail", SNORMAL);
	p = clocal(p);

	ecomp(buildtree(UCALL, p, NIL));

	plabel(lab);

	if (t != VOID && !ISSOU(t)) {
		p = tempnode(tmpnr, t, cftnsp->sdf, cftnsp->ssue);
		q = block(REG, NIL, NIL, t, cftnsp->sdf, cftnsp->ssue);
		q->n_rval = RETREG(t);
		ecomp(buildtree(ASSIGN, q, p));
	}
}

/*
 * Allocate on the permanent heap for inlines, otherwise temporary heap.
 */
void *
inlalloc(int size)
{
	return isinlining ?  permalloc(size) : tmpalloc(size);
}
