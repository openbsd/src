/*	$OpenBSD: code.c,v 1.4 2007/12/22 13:13:06 stefan Exp $	*/
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


# include "pass1.h"

/*
 * define the current location as the name p->sname
 * never called for text segment.
 */
void
defnam(struct symtab *p)
{
	char *c = p->sname;

#ifdef GCC_COMPAT
	c = gcc_findname(p);
#endif
	if (p->sclass == EXTDEF)
		printf("        .globl %s\n", c);
	printf("%s:\n", c);
}

/*
 * cause the alignment to become a multiple of n
 * Nothing to do on PDP10.
 */
void
defalign(int n)
{
}

/*
 * code for the end of a function
 */
void
efcode()
{
}

/*
 * code for the beginning of a function; a is an array of
 * indices in stab for the arguments; n is the number
 */
void
bfcode(struct symtab **sp, int cnt)
{
	NODE *p, *q;
	int i, n;

	if (cftnsp->stype == STRTY+FTN || cftnsp->stype == UNIONTY+FTN) {
		uerror("no struct return yet");
	}
	/* recalculate the arg offset and create TEMP moves */
	for (n = 1, i = 0; i < cnt; i++) {
		if (n < 8) {
			p = tempnode(0, sp[i]->stype, sp[i]->sdf, sp[i]->ssue);
			spname = sp[i];
			q = block(REG, NIL, NIL,
			    sp[i]->stype, sp[i]->sdf, sp[i]->ssue);
			q->n_rval = n;
			p = buildtree(ASSIGN, p, q);
			sp[i]->soffset = p->n_left->n_lval;
			sp[i]->sflags |= STNODE;
			ecomp(p);
		} else {
			sp[i]->soffset += SZINT * n;
			if (xtemps) {
				/* put stack args in temps if optimizing */
				spname = sp[i];
				p = tempnode(0, sp[i]->stype,
				    sp[i]->sdf, sp[i]->ssue);
				p = buildtree(ASSIGN, p, buildtree(NAME, 0, 0));
				sp[i]->soffset = p->n_left->n_lval;
				sp[i]->sflags |= STNODE;
				ecomp(p);
			}
		}
		n += szty(sp[i]->stype);
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

void
bjobcode()
{
}

/* called just before final exit */
/* flag is 1 if errors, 0 if none */
void
ejobcode(int flag )
{
}

/*
 * Make a register node, helper for funcode.
 */
static NODE *
mkreg(NODE *p, int n)
{
	NODE *r;

	r = block(REG, NIL, NIL, p->n_type, p->n_df, p->n_sue);
	if (szty(p->n_type) == 2)
		n += 16;
	r->n_rval = n;
	return r;
}

static int regnum;
/*
 * Move args to registers and emit expressions bottom-up.
 */
static void
fixargs(NODE *p)
{
	NODE *r;

	if (p->n_op == CM) {
		fixargs(p->n_left);
		r = p->n_right;
		if (r->n_op == STARG)
			regnum = 9; /* end of register list */
		else if (regnum + szty(r->n_type) > 8)
			p->n_right = block(FUNARG, r, NIL, r->n_type,
			    r->n_df, r->n_sue);
		else
			p->n_right = buildtree(ASSIGN, mkreg(r, regnum), r);
	} else {
		if (p->n_op == STARG) {
			regnum = 9; /* end of register list */
		} else {
			r = talloc();
			*r = *p;
			r = buildtree(ASSIGN, mkreg(r, regnum), r);
			*p = *r;
			nfree(r);
		}
		r = p;
	}
	regnum += szty(r->n_type);
}


/*
 * Called with a function call with arguments as argument.
 * This is done early in buildtree() and only done once.
 */
NODE *
funcode(NODE *p)
{

	regnum = 1;

	fixargs(p->n_right);
	return p;
}

/*
 * Print character t at position i in one string, until t == -1.
 * Locctr & label is already defined.
 */
void
bycode(int t, int i)
{
	static	int	lastoctal = 0;

	/* put byte i+1 in a string */

	if (t < 0) {
		if (i != 0)
			puts("\"");
	} else {
		if (i == 0)
			printf("\t.ascii \"");
		if (t == '\\' || t == '"') {
			lastoctal = 0;
			putchar('\\');
			putchar(t);
		} else if (t < 040 || t >= 0177) {
			lastoctal++;
			printf("\\%o",t);
		} else if (lastoctal && '0' <= t && t <= '9') {
			lastoctal = 0;
			printf("\"\n\t.ascii \"%c", t);
		} else {	
			lastoctal = 0;
			putchar(t);
		}
	}
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
