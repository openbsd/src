/*	$OpenBSD: local.c,v 1.2 2008/04/11 20:45:52 stefan Exp $	*/
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

NODE *
clocal(NODE *p)
{
	struct symtab *sp;
	int op;
	NODE *r, *l;

	op = p->n_op;
	sp = p->n_sp;
	l  = p->n_left;
	r  = p->n_right;

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal in: %p, %s\n", p, copst(op));
		fwalk(p, eprint, 0);
	}
#endif

	switch (op) {

	case NAME:
		if (sp->sclass == PARAM || sp->sclass == AUTO) {
			/*
			 * Use a fake structure reference to
			 * write out frame pointer offsets.
			 */
			l = block(REG, NIL, NIL, PTR+STRTY, 0, 0);
			l->n_lval = 0;
			l->n_rval = FP;
			r = p;
			p = stref(block(STREF, l, r, 0, 0, 0));
		}
		break;
	case PCONV: /* Remove what PCONVs we can. */
		if (l->n_op == SCONV)
			break;

		if (l->n_op == ICON || (ISPTR(p->n_type) && ISPTR(l->n_type))) {
			l->n_type = p->n_type;
			l->n_qual = p->n_qual;
			l->n_df = p->n_df;
			l->n_sue = p->n_sue;
			nfree(p);
			p = l;
		}
		break;

	case SCONV:
		if (l->n_op == NAME || l->n_op == UMUL || l->n_op == TEMP) {
			if ((p->n_type & TMASK) == 0 &&
			    (l->n_type & TMASK) == 0 &&
			    btdims[p->n_type].suesize ==
			    btdims[l->n_type].suesize) {
				if (p->n_type == FLOAT || p->n_type == DOUBLE)
					break;
				l->n_type = p->n_type;
				nfree(p);
				p = l;
			}
			break;
		}

		if (l->n_op != ICON)
			break;

		if (ISPTR(p->n_type)) {
			l->n_type = p->n_type;
			nfree(p);
			p = l;
			break;
		}

		switch (p->n_type) {
		case BOOL:      l->n_lval = (l->n_lval != 0); break;
		case CHAR:      l->n_lval = (char)l->n_lval; break;
		case UCHAR:     l->n_lval = l->n_lval & 0377; break;
		case SHORT:     l->n_lval = (short)l->n_lval; break;
		case USHORT:    l->n_lval = l->n_lval & 0177777; break;
		case UNSIGNED:  l->n_lval = l->n_lval & 0xffffffff; break;
		case INT:       l->n_lval = (int)l->n_lval; break;
		case ULONG:
		case ULONGLONG: l->n_lval = l->n_lval; break;
		case LONG:
		case LONGLONG:	l->n_lval = (long long)l->n_lval; break;
		case FLOAT:
		case DOUBLE:
		case LDOUBLE:
			l->n_op = FCON;
			l->n_dcon = l->n_lval;
			break;
		case VOID:
			break;
		default:
			cerror("sconv type unknown %d", p->n_type);
		}

		l->n_type = p->n_type;
		nfree(p);
		p = l;
		break;

	case PMCONV:
	case PVCONV:
		if (r->n_op != ICON)
			cerror("converting bad type");
		nfree(p);
		p = buildtree(op == PMCONV ? MUL : DIV, l, r);
		break;

	case FORCE:
		/* Put attached value into the return register. */
		p->n_op = ASSIGN;
		p->n_right = p->n_left;
		p->n_left = block(REG, NIL, NIL, p->n_type, 0, MKSUE(INT));
		p->n_left->n_rval = RETREG_PRE(p->n_type);
		break;
	}

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("clocal out: %p, %s\n", p, copst(op));
		fwalk(p, eprint, 0);
	}
#endif

	return p;
}

void
myp2tree(NODE *p)
{
	struct symtab *sp;

	if (p->n_op != FCON)
		return;

	sp = tmpalloc(sizeof(struct symtab));
	sp->sclass = STATIC;
	sp->slevel = 1;
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

int
andable(NODE *p)
{
	return 1;
}

void
cendarg()
{
	autooff = AUTOINIT;
}

int
cisreg(TWORD t)
{
	/* SPARCv9 registers are all 64-bits wide. */
	return 1;
}

NODE *
offcon(OFFSZ off, TWORD t, union dimfun *d, struct suedef *sue)
{
	return bcon(off / SZCHAR);
}

void
spalloc(NODE *t, NODE *p, OFFSZ off)
{
}

void
inwstring(struct symtab *sp)
{
}

void
instring(struct symtab *sp)
{
	char *s, *str;

	defloc(sp);
	str = sp->sname;

	printf("\t.ascii \"");
	for (s = str; *s != 0; *s++) {
		if (*s++ == '\\')
			esccon(&s);
		if (s - str > 60) {
			fwrite(str, 1, s - str, stdout);
			printf("\"\n\t.ascii \"");
			str = s;
		}
	}
	fwrite(str, 1, s - str, stdout);
	printf("\\0\"\n");
}

void
zbits(OFFSZ off, int fsz)
{
}

void
infld(CONSZ off, int fsz, CONSZ val)
{
}

void
ninval(CONSZ off, int fsz, NODE *p)
{
	TWORD t;
	struct symtab *sp;
	union { float f; double d; int i; long long l; } u;

	t = p->n_type;
	sp = p->n_sp;

	if (ISPTR(t))
		t = LONGLONG;

	if (p->n_op != ICON && p->n_op != FCON)
		cerror("ninval: not a constant");
	if (p->n_op == ICON && sp != NULL && DEUNSIGN(t) != LONGLONG)
		cerror("ninval: not constant");

	switch (t) {
		case CHAR:
		case UCHAR:
			printf("\t.byte %d\n", (int)p->n_lval & 0xff);
			break;
		case SHORT:
		case USHORT:
			printf("\t.half %d\n", (int)p->n_lval &0xffff);
			break;
		case BOOL:
			p->n_lval = (p->n_lval != 0); /* FALLTHROUGH */
		case INT:
		case UNSIGNED:
			printf("\t.long " CONFMT "\n", p->n_lval);
			break;
		case LONG:
		case ULONG:
		case LONGLONG:
		case ULONGLONG:
			printf("\t.xword %lld", p->n_lval);
			if (sp != 0) {
				if ((sp->sclass == STATIC && sp->slevel > 0)
				    || sp->sclass == ILABEL)
					printf("+" LABFMT, sp->soffset);
				else
					printf("+%s", exname(sp->soname));
			}
			printf("\n");
			break;
		case FLOAT:
			u.f = (float)p->n_dcon;
			printf("\t.long %d\n", u.i);
			break;
		case DOUBLE:
			u.d = (double)p->n_dcon;
			printf("\t.xword %lld\n", u.l);
			break;
	}
}

char *
exname(char *p)
{
	return p ? p : "";
}

TWORD
ctype(TWORD type)
{
	return type;
}

void
calldec(NODE *p, NODE *q) 
{
}

void
extdec(struct symtab *q)
{
}

void
defzero(struct symtab *sp)
{
	int off = (tsize(sp->stype, sp->sdf, sp->ssue) + SZCHAR - 1) / SZCHAR;
	printf("\t.comm ");
	if (sp->slevel == 0)
		printf("%s,%d\n", exname(sp->soname), off);
	else
		printf(LABFMT ",%d\n", sp->soffset, off);
}

int
mypragma(char **ary)
{
	return 0;
}

void
fixdef(struct symtab *sp)
{
}
