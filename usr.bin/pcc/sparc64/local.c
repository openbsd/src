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
	case PCONV:
		if (p->n_type > BTMASK && l->n_type > BTMASK) {
			/* Remove unnecessary pointer conversions. */
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
				/* XXX: skip if FP? */
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
		case UNSIGNED:
		case ULONG:     l->n_lval = l->n_lval & 0xffffffff; break;
		case LONG:
		case INT:       l->n_lval = (int)l->n_lval; break;
		case ULONGLONG: l->n_lval = l->n_lval; break;
		case LONGLONG:	l->n_lval = (long long)l->n_lval; break;
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
		p->n_left->n_rval = I0; /* XXX adjust for float/double */
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
		if (*s == '\\')
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
	if (p->n_op != ICON && p->n_op != FCON)
		cerror("ninval: not a constant");
	if (p->n_op == ICON && p->n_sp != NULL && DEUNSIGN(p->n_type) != INT)
		cerror("ninval: not constant");

	switch (DEUNSIGN(p->n_type)) {
		case CHAR:
			printf("\t.align 1\n");
			printf("\t.byte %d\n", (int)p->n_lval & 0xff);
			break;
		case SHORT:
			printf("\t.align 2\n");
			printf("\t.half %d\n", (int)p->n_lval &0xffff);
			break;
		case BOOL:
			p->n_lval = (p->n_lval != 0); /* FALLTHROUGH */
		case INT:
			printf("\t.align 4\n\t.long " CONFMT "\n", p->n_lval);
			break;
		case LONGLONG:
			printf("\t.align 8\n\t.xword %lld\n", p->n_lval);
			break;
		/* TODO FP float and double */
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
