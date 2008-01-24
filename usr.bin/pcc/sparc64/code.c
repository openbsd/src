/*
 * Copyright (c) 2008 David Crawshaw <david@zentus.com>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "pass1.h"

void
defloc(struct symtab *sp)
{
	static char *loctbl[] = { "text", "data", "rodata" };
	static int lastloc = -1;
	TWORD t;
	int s;

	t = sp->stype;
	s = ISFTN(t) ? PROG : ISCON(cqual(t, sp->squal)) ? RDATA : DATA;
	if (s != lastloc)
		printf("\n\t.section \".%s\"\n", loctbl[s]);
	lastloc = s;
	if (s == PROG)
		return;
	printf("\t.align 4\n");
	if (sp->sclass == EXTDEF)
		printf("\t.global %s\n", sp->soname);
	if (sp->slevel == 0) {
		printf("\t.type %s,#object\n", sp->soname);
		printf("\t.size %s," CONFMT "\n", sp->soname,
			tsize(sp->stype, sp->sdf, sp->ssue) / SZCHAR);
		printf("%s:\n", sp->soname);
	} else
		printf(LABFMT ":\n", sp->soffset);
}

void
efcode()
{
	/* XXX */
}

void
bfcode(struct symtab **sp, int cnt)
{
	int i;
	NODE *p, *q;
	struct symtab *sym;

	for (i=0; i < cnt && i < I7 - I0; i++) {
		sym = sp[i];
		q = block(REG, NIL, NIL, sym->stype, sym->sdf, sym->ssue);
		q->n_rval = i + I0;
		p = tempnode(0, sym->stype, sym->sdf, sym->ssue);
		sym->soffset = regno(p);
		sym->sflags |= STNODE;
		p = buildtree(ASSIGN, p, q);
		ecomp(p);
	}

	if (i < cnt)
		cerror("unprocessed arguments in bfcode"); /* TODO */
}

void
bccode()
{
	SETOFF(autooff, SZINT);
}

void
ejobcode(int flag)
{
}

void
bjobcode()
{
}

static NODE *
moveargs(NODE *p, int *regp)
{
	NODE *r, *q;

	if (p->n_op == CM) {
		p->n_left = moveargs(p->n_left, regp);
		r = p->n_right;
	} else {
		r = p;
	}

	if (*regp > I7 && r->n_op != STARG)
		cerror("reg > I7 in moveargs"); /* TODO */
	else if (r->n_op == STARG)
		cerror("op STARG in moveargs");
	else if (r->n_type == DOUBLE || r->n_type == LDOUBLE)
		cerror("FP in moveargs");
	else if (r->n_type == FLOAT)
		cerror("FP in moveargs");
	else {
		/* Argument can fit in O0...O7. */
		q = block(REG, NIL, NIL, r->n_type, r->n_df, r->n_sue);
		q->n_rval = (*regp)++;
		r = buildtree(ASSIGN, q, r);
	}

	if (p->n_op == CM) {
		p->n_right = r;
		return p;
	}

	return r;
}

NODE *
funcode(NODE *p)
{
	int reg = O0;
	p->n_right = moveargs(p->n_right, &reg);
	return p;
}

int
fldal(unsigned int t)
{
	uerror("illegal field type");
	return ALINT;
}

void
fldty(struct symtab *p)
{
}

int
mygenswitch(int num, TWORD type, struct swents **p, int n)
{
	return 0;
}
