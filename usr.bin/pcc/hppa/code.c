/*	$OpenBSD: code.c,v 1.4 2008/04/11 20:45:52 stefan Exp $	*/

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


# include "pass1.h"

NODE *funarg(NODE *, int *);
int argreg(TWORD, int *);

/*
 * Define everything needed to print out some data (or text).
 * This means segment, alignment, visibility, etc.
 */
void
defloc(struct symtab *sp)
{
	extern char *nextsect;
	static char *loctbl[] = { "text", "data", "section .rodata" };
	static int lastloc = -1;
	TWORD t;
	int s;

	if (sp == NULL) {
		lastloc = -1;
		return;
	}
	t = sp->stype;
	s = ISFTN(t) ? PROG : ISCON(cqual(t, sp->squal)) ? RDATA : DATA;
	if (nextsect) {
		printf("\t.section %s\n", nextsect);
		nextsect = NULL;
		s = -1;
	} else if (s != lastloc)
		printf("\t.%s\n", loctbl[s]);
	lastloc = s;
	while (ISARY(t))
		t = DECREF(t);
	if (t > UCHAR)
		printf("\t.align\t%d\n", ISFTN(t)? 4 : talign(t, sp->ssue));
	if (sp->sclass == EXTDEF)
		printf("\t.export %s, data\n", sp->soname);
	if (sp->slevel == 0)
		printf("\t.label %s\n", sp->soname);
	else
		printf("\t.label\t" LABFMT "\n", sp->soffset);
}

/*
 * code for the end of a function
 * deals with struct return here
 */
void
efcode()
{
	NODE *p, *q;
	int sz;

	if (cftnsp->stype != STRTY+FTN && cftnsp->stype != UNIONTY+FTN)
		return;
	/* address of return struct is in eax */
	/* create a call to memcpy() */
	/* will get the result in %ret0 */
	p = block(REG, NIL, NIL, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	p->n_rval = RET0;
	q = block(OREG, NIL, NIL, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	q->n_rval = FP;
	q->n_lval = 8; /* return buffer offset */
	p = block(CM, q, p, INT, 0, MKSUE(INT));
	sz = (tsize(STRTY, cftnsp->sdf, cftnsp->ssue)+SZCHAR-1)/SZCHAR;
	p = block(CM, p, bcon(sz), INT, 0, MKSUE(INT));
	p->n_right->n_name = "";
	p = block(CALL, bcon(0), p, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	p->n_left->n_name = "memcpy";
	p = clocal(p);
	send_passt(IP_NODE, p);
}

int
argreg(TWORD t, int *n)
{
	switch (t) {
	case FLOAT:
		return FR7L - 2 * (*n)++;
	case DOUBLE:
	case LDOUBLE:
		*n += 2;
		return FR6 - *n - 2;
	case LONGLONG:
	case ULONGLONG:
		*n += 2;
		return AD1 - (*n - 2) / 2;
	default:
		return ARG0 - (*n)++;
	}
}

/*
 * code for the beginning of a function; 'a' is an array of
 * indices in symtab for the arguments; n is the number
 */
void
bfcode(struct symtab **a, int cnt)
{
	struct symtab *sp;
	NODE *p, *q;
	int i, n, sz;

	if (cftnsp->stype == STRTY+FTN || cftnsp->stype == UNIONTY+FTN) {
	/* Function returns struct, adjust arg offset */
	for (i = 0; i < n; i++)
		a[i]->soffset += SZPOINT(INT);
	}

	/* recalculate the arg offset and create TEMP moves */
	for (n = 0, i = 0; i < cnt; i++) {
		sp = a[i];

		sz = szty(sp->stype);
		if (n % sz)
			n++;	/* XXX LDOUBLE */

		if (n < 4) {
			p = tempnode(0, sp->stype, sp->sdf, sp->ssue);
			/* TODO p->n_left->n_lval = -(32 + n * 4); */
			q = block(REG, NIL, NIL, sp->stype, sp->sdf, sp->ssue);
			q->n_rval = argreg(sp->stype, &n);
			p = buildtree(ASSIGN, p, q);
			sp->soffset = regno(p->n_left);
			sp->sflags |= STNODE;
			ecomp(p);
		} else {
			sp->soffset += SZINT * n;
			if (xtemps) {
				/* put stack args in temps if optimizing */
				p = tempnode(0, sp->stype, sp->sdf, sp->ssue);
				p = buildtree(ASSIGN, p, buildtree(NAME, 0, 0));
				sp->soffset = regno(p->n_left);
				sp->sflags |= STNODE;
				ecomp(p);
			}
		}
	}
}


/*
 * by now, the automatics and register variables are allocated
 */
void
bccode()
{
	SETOFF(autooff, SZINT);
}

/* called just before final exit */
/* flag is 1 if errors, 0 if none */
void
ejobcode(int errors)
{
	if (errors)
		return;

	printf("\t.end\n");
}

void
bjobcode(void)
{
	printf("\t.import $global$, data\n");
}

/*
 * return the alignment of field of type t
 */
int
fldal(unsigned int t)
{
	uerror("illegal field type");
	return(ALINT);
}

/* fix up type of field p */
void
fldty(struct symtab *p)
{
}

/*
 * XXX - fix genswitch.
 */
int
mygenswitch(int num, TWORD type, struct swents **p, int n)
{
	return 0;
}

NODE *
funarg(NODE *p, int *n)
{
	NODE *r;
	int sz;

	if (p->n_op == CM) {
		p->n_left = funarg(p->n_left, n);
		p->n_right = funarg(p->n_right, n);
		return p;
	}

	sz = szty(p->n_type);
	if (*n % sz)
		(*n)++;	/* XXX LDOUBLE */

	if (*n >= 4) {
		*n += sz;
		r = block(OREG, NIL, NIL, p->n_type|PTR, 0,
		    MKSUE(p->n_type|PTR));
		r->n_rval = SP;
		r->n_lval = -(32 + *n * 4);
	} else {
		r = block(REG, NIL, NIL, p->n_type, 0, 0);
		r->n_lval = 0;
		r->n_rval = argreg(p->n_type, n);
	}
	p = block(ASSIGN, r, p, p->n_type, 0, 0);
	clocal(p);

	return p;
}

/*
 * Called with a function call with arguments as argument.
 * This is done early in buildtree() and only done once.
 */
NODE *
funcode(NODE *p)
{
	int n = 0;

	p->n_right = funarg(p->n_right, &n);
	return p;
}
