/*	$OpenBSD: code.c,v 1.6 2008/04/11 20:45:52 stefan Exp $	*/
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

/*
 * Define everything needed to print out some data (or text).
 * This means segment, alignment, visibility, etc.
 */
void
defloc(struct symtab *sp)
{
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
	lastloc = s;
	if (s == PROG)
		return; /* text is written in prologue() */
	if (s != lastloc)
		printf("	.%s\n", loctbl[s]);
	printf("	.p2align %d\n", ispow2(talign(t, sp->ssue)));
	if (sp->sclass == EXTDEF)
		printf("	.globl %s\n", sp->soname);
	if (sp->slevel == 0) {
#ifdef USE_GAS
		printf("\t.type %s,@object\n", sp->soname);
		printf("\t.size %s," CONFMT "\n", sp->soname,
		    tsize(sp->stype, sp->sdf, sp->ssue));
#endif
		printf("%s:\n", sp->soname);
	} else
		printf(LABFMT ":\n", sp->soffset);
}


#ifdef notdef
/*
 * cause the alignment to become a multiple of n
 * never called for text segment.
 */
void
defalign(int n)
{
	n = ispow2(n / SZCHAR);
	if (n == -1)
		cerror("defalign: n != 2^i");
	printf("\t.p2align %d\n", n);
}

/*
 * define the current location as the name p->sname
 * never called for text segment.
 */
void
defnam(struct symtab *p)
{
	char *c = p->soname;

	if (p->sclass == EXTDEF)
		printf("\t.globl %s\n", c);
#ifdef USE_GAS
	printf("\t.type %s,@object\n", c);
	printf("\t.size %s," CONFMT "\n", c, tsize(p->stype, p->sdf, p->ssue));
#endif
	printf("%s:\n", c);
}
#endif

static int rvnr;

/*
 * code for the end of a function
 * deals with struct return here
 */
void
efcode()
{
	NODE *p, *q;
	int tempnr;
	int ty;

	if (cftnsp->stype != STRTY+FTN && cftnsp->stype != UNIONTY+FTN)
		return;

	ty = cftnsp->stype - FTN;

	q = block(REG, NIL, NIL, INCREF(ty), 0, cftnsp->ssue);
	q->n_rval = V0;
	p = tempnode(0, INCREF(ty), 0, cftnsp->ssue);
	tempnr = regno(p);
	p = buildtree(ASSIGN, p, q);
	ecomp(p);

	q = tempnode(tempnr, INCREF(ty), 0, cftnsp->ssue);
	q = buildtree(UMUL, q, NIL);

	p = tempnode(rvnr, INCREF(ty), 0, cftnsp->ssue);
	p = buildtree(UMUL, p, NIL);

	p = buildtree(ASSIGN, p, q);
	ecomp(p);

	q = tempnode(rvnr, INCREF(ty), 0, cftnsp->ssue);
	p = block(REG, NIL, NIL, INCREF(ty), 0, cftnsp->ssue);
	p->n_rval = V0;
	p = buildtree(ASSIGN, p, q);
	ecomp(p);
}

/* Put a symbol in a temporary
 * used by bfcode() and its helpers */
static void
putintemp(struct symtab *sym)
{
	NODE *p;
	spname = sym;
	p = tempnode(0, sym->stype, sym->sdf, sym->ssue);
	p = buildtree(ASSIGN, p, buildtree(NAME, 0, 0));
	sym->soffset = regno(p->n_left);
	sym->sflags |= STNODE;
	ecomp(p);
}

/* setup the hidden pointer to struct return parameter
 * used by bfcode() */
static void
param_retptr(void)
{
	NODE *p, *q;

	p = tempnode(0, PTR+STRTY, 0, cftnsp->ssue);
	rvnr = regno(p);
	q = block(REG, NIL, NIL, PTR+STRTY, 0, cftnsp->ssue);
	q->n_rval = A0;
	p = buildtree(ASSIGN, p, q);
	ecomp(p);
}

/* setup struct parameter
 * push the registers out to memory
 * used by bfcode() */
static void
param_struct(struct symtab *sym, int *regp)
{
	int reg = *regp;
	NODE *p, *q;
	int navail;
	int sz;
	int off;
	int num;
	int i;

	navail = nargregs - (reg - A0);
	sz = tsize(sym->stype, sym->sdf, sym->ssue) / SZINT;
	off = ARGINIT/SZINT + (reg - A0);
	num = sz > navail ? navail : sz;
	for (i = 0; i < num; i++) {
		q = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
		q->n_rval = reg++;
		p = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
		p->n_rval = FP;
		p = block(PLUS, p, bcon(4*off++), INT, 0, MKSUE(INT));
		p = block(UMUL, p, NIL, INT, 0, MKSUE(INT));
		p = buildtree(ASSIGN, p, q);
		ecomp(p);
	}

	*regp = reg;
}

/* setup a 64-bit parameter (double/ldouble/longlong)
 * used by bfcode() */
static void
param_64bit(struct symtab *sym, int *regp, int dotemps)
{
	int reg = *regp;
	NODE *p, *q;
	int navail;

	/* alignment */
	++reg;
	reg &= ~1;

	navail = nargregs - (reg - A0);

	if (navail < 2) {
		/* would have appeared half in registers/half
		 * on the stack, but alignment ensures it
		 * appears on the stack */
		if (dotemps)
			putintemp(sym);
		*regp = reg;
		return;
	}

	q = block(REG, NIL, NIL, sym->stype, sym->sdf, sym->ssue);
	q->n_rval = A0A1 + (reg - A0);
	if (dotemps) {
		p = tempnode(0, sym->stype, sym->sdf, sym->ssue);
		sym->soffset = regno(p);
		sym->sflags |= STNODE;
	} else {
		spname = sym;
		p = buildtree(NAME, 0, 0);
	}
	p = buildtree(ASSIGN, p, q);
	ecomp(p);
	*regp = reg + 2;
}

/* setup a 32-bit param on the stack
 * used by bfcode() */
static void
param_32bit(struct symtab *sym, int *regp, int dotemps)
{
	NODE *p, *q;

	q = block(REG, NIL, NIL, sym->stype, sym->sdf, sym->ssue);
	q->n_rval = (*regp)++;
	if (dotemps) {
		p = tempnode(0, sym->stype, sym->sdf, sym->ssue);
		sym->soffset = regno(p);
		sym->sflags |= STNODE;
	} else {
		spname = sym;
		p = buildtree(NAME, 0, 0);
	}
	p = buildtree(ASSIGN, p, q);
	ecomp(p);
}

/*
 * XXX This is a hack.  We cannot have (l)doubles in more than one
 * register class.  So we bounce them in and out of temps to
 * move them in and out of the right registers.
 */
static void
param_double(struct symtab *sym, int *regp, int dotemps)
{
	int reg = *regp;
	NODE *p, *q, *t;
	int navail;
	int tmpnr;

	/* alignment */
	++reg;
	reg &= ~1;

	navail = nargregs - (reg - A0);

	if (navail < 2) {
		/* would have appeared half in registers/half
		 * on the stack, but alignment ensures it
		 * appears on the stack */
		if (dotemps)
			putintemp(sym);
		*regp = reg;
		return;
	}

	t = tempnode(0, LONGLONG, 0, MKSUE(LONGLONG));
	tmpnr = regno(t);
	q = block(REG, NIL, NIL, LONGLONG, 0, MKSUE(LONGLONG));
	q->n_rval = A0A1 + (reg - A0);
	p = buildtree(ASSIGN, t, q);
	ecomp(p);

	if (dotemps) {
		sym->soffset = tmpnr;
		sym->sflags |= STNODE;
	} else {
		q = tempnode(tmpnr, sym->stype, sym->sdf, sym->ssue);
		spname = sym;
		p = buildtree(NAME, 0, 0);
		p = buildtree(ASSIGN, p, q);
		ecomp(p);
	}
	*regp = reg + 2;
}

/*
 * XXX This is a hack.  We cannot have floats in more than one
 * register class.  So we bounce them in and out of temps to
 * move them in and out of the right registers.
 */
static void
param_float(struct symtab *sym, int *regp, int dotemps)
{
	NODE *p, *q, *t;
	int tmpnr;

	t = tempnode(0, INT, 0, MKSUE(INT));
	tmpnr = regno(t);
	q = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
	q->n_rval = (*regp)++;
	p = buildtree(ASSIGN, t, q);
	ecomp(p);

	if (dotemps) {
		sym->soffset = tmpnr;
		sym->sflags |= STNODE;
	} else {
		q = tempnode(tmpnr, sym->stype, sym->sdf, sym->ssue);
		spname = sym;
		p = buildtree(NAME, 0, 0);
		p = buildtree(ASSIGN, p, q);
		ecomp(p);
	}
}

/*
 * code for the beginning of a function; a is an array of
 * indices in symtab for the arguments; n is the number
 */
void
bfcode(struct symtab **sp, int cnt)
{
	union arglist *usym;
	int lastreg = A0 + nargregs - 1;
	int saveallargs = 0;
	int i, reg;

	/*
	 * Detect if this function has ellipses and save all
	 * argument register onto stack.
	 */
	usym = cftnsp->sdf->dfun;
	while (usym && usym->type != TNULL) {
		if (usym->type == TELLIPSIS) {
			saveallargs = 1;
			break;
		}
		++usym;
	}

	reg = A0;

	/* assign hidden return structure to temporary */
	if (cftnsp->stype == STRTY+FTN || cftnsp->stype == UNIONTY+FTN) {
		param_retptr();
		++reg;
	}

        /* recalculate the arg offset and create TEMP moves */
        for (i = 0; i < cnt; i++) {

		if ((reg > lastreg) && !xtemps)
			break;
		else if (reg > lastreg) 
			putintemp(sp[i]);
		else if (sp[i]->stype == STRTY || sp[i]->stype == UNIONTY)
			param_struct(sp[i], &reg);
		else if (DEUNSIGN(sp[i]->stype) == LONGLONG)
			param_64bit(sp[i], &reg, xtemps && !saveallargs);
		else if (sp[i]->stype == DOUBLE || sp[i]->stype == LDOUBLE)
			param_double(sp[i], &reg, xtemps && !saveallargs);
		else if (sp[i]->stype == FLOAT)
			param_float(sp[i], &reg, xtemps && !saveallargs);
		else
			param_32bit(sp[i], &reg, xtemps && !saveallargs);
	}

	/* if saveallargs, save the rest of the args onto the stack */
	if (!saveallargs)
		return;
	while (reg <= lastreg) {
		NODE *p, *q;
		int off = ARGINIT/SZINT + (reg - A0);
		q = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
		q->n_rval = reg++;
		p = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
		p->n_rval = FP;
		p = block(PLUS, p, bcon(4*off), INT, 0, MKSUE(INT));
		p = block(UMUL, p, NIL, INT, 0, MKSUE(INT));
		p = buildtree(ASSIGN, p, q);
		ecomp(p);
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
ejobcode(int flag )
{
}

void
bjobcode()
{
	printf("\t.section .mdebug.abi32\n");
	printf("\t.previous\n");
	printf("\t.abicalls\n");
}

#ifdef notdef
/*
 * Print character t at position i in one string, until t == -1.
 * Locctr & label is already defined.
 */
void
bycode(int t, int i)
{
	static int lastoctal = 0;

	/* put byte i+1 in a string */

	if (t < 0) {
		if (i != 0)
			puts("\\000\"");
	} else {
		if (i == 0)
			printf("\t.ascii \"");
		if (t == 0)
			return;
		else if (t == '\\' || t == '"') {
			lastoctal = 0;
			putchar('\\');
			putchar(t);
		} else if (t == 011) {
			printf("\\t");
		} else if (t == 012) {
			printf("\\n");
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
#endif

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


/* setup call stack with a structure */
/* called from moveargs() */
static NODE *
movearg_struct(NODE *p, NODE *parent, int *regp)
{
	int reg = *regp;
	NODE *l, *q, *t, *r;
	int tmpnr;
	int navail;
	int off;
	int num;
        int sz;
	int ty;
	int i;

	navail = nargregs - (reg - A0);
	sz = tsize(p->n_type, p->n_df, p->n_sue) / SZINT;
	num = sz > navail ? navail : sz;

	l = p->n_left;
	nfree(p);
	ty = l->n_type;
	t = tempnode(0, l->n_type, l->n_df, l->n_sue);
	tmpnr = regno(t);
	l = buildtree(ASSIGN, t, l);

	if (p != parent) {
		q = parent->n_left;
	} else
		q = NULL;

	/* copy structure into registers */
	for (i = 0; i < num; i++) {
		t = tempnode(tmpnr, ty, 0, MKSUE(PTR+ty));
		t = block(SCONV, t, NIL, PTR+INT, 0, MKSUE(PTR+INT));
		t = block(PLUS, t, bcon(4*i), PTR+INT, 0, MKSUE(PTR+INT));
		t = buildtree(UMUL, t, NIL);

		r = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
		r->n_rval = reg++;

               	r = buildtree(ASSIGN, r, t);
		if (q == NULL)
			q = r;
		else 
			q = block(CM, q, r, INT, 0, MKSUE(INT));
	}
	off = ARGINIT/SZINT + nargregs;
	for (i = num; i < sz; i++) {
		t = tempnode(tmpnr, ty, 0, MKSUE(PTR+ty));
		t = block(SCONV, t, NIL, PTR+INT, 0, MKSUE(PTR+INT));
		t = block(PLUS, t, bcon(4*i), PTR+INT, 0, MKSUE(PTR+INT));
		t = buildtree(UMUL, t, NIL);

		r = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
		r->n_rval = FP;
		r = block(PLUS, r, bcon(4*off++), INT, 0, MKSUE(INT));
		r = block(UMUL, r, NIL, INT, 0, MKSUE(INT));

               	r = buildtree(ASSIGN, r, t);
		if (q == NULL)
			q = r;
		else
			q = block(CM, q, r, INT, 0, MKSUE(INT));
	}

	if (parent->n_op == CM) {
		parent->n_left = q;
		q = l;
	} else {
		q = block(CM, q, l, INT, 0, MKSUE(INT));
	}

	*regp = reg;
	return q;
}

/* setup call stack with 64-bit argument */
/* called from moveargs() */
static NODE *
movearg_64bit(NODE *p, int *regp)
{
	int reg = *regp;
	NODE *q;
	int lastarg;

	/* alignment */
	++reg;
	reg &= ~1;

	lastarg = A0 + nargregs - 1;
	if (reg > lastarg) {
		*regp = reg;
		return block(FUNARG, p, NIL, p->n_type, p->n_df, p->n_sue);
	}

	q = block(REG, NIL, NIL, p->n_type, p->n_df, p->n_sue);
	q->n_rval = A0A1 + (reg - A0);
	q = buildtree(ASSIGN, q, p);

	*regp = reg + 2;
	return q;
}

/* setup call stack with 32-bit argument */
/* called from moveargs() */
static NODE *
movearg_32bit(NODE *p, int *regp)
{
	int reg = *regp;
	NODE *q;

	q = block(REG, NIL, NIL, p->n_type, p->n_df, p->n_sue);
	q->n_rval = reg++;
	q = buildtree(ASSIGN, q, p);

	*regp = reg;
	return q;
}

static NODE *
moveargs(NODE *p, int *regp)
{
        NODE *r, **rp;
	int lastreg;
	int reg;

        if (p->n_op == CM) {
                p->n_left = moveargs(p->n_left, regp);
                r = p->n_right;
		rp = &p->n_right;
        } else {
		r = p;
		rp = &p;
	}

 	lastreg = A0 + nargregs - 1;
        reg = *regp;

	if (reg > lastreg && r->n_op != STARG)
		*rp = block(FUNARG, r, NIL, r->n_type, r->n_df, r->n_sue);
	else if (r->n_op == STARG) {
		*rp = movearg_struct(r, p, regp);
	} else if (DEUNSIGN(r->n_type) == LONGLONG) {
		*rp = movearg_64bit(r, regp);
	} else if (r->n_type == DOUBLE || r->n_type == LDOUBLE) {
		/* XXX bounce in and out of temporary to change to longlong */
		NODE *t1 = tempnode(0, LONGLONG, 0, MKSUE(LONGLONG));
		int tmpnr = regno(t1);
		NODE *t2 = tempnode(tmpnr, r->n_type, r->n_df, r->n_sue);
		t1 =  movearg_64bit(t1, regp);
		r = block(ASSIGN, t2, r, r->n_type, r->n_df, r->n_sue);
		if (p->n_op == CM) {
			p->n_left = buildtree(CM, p->n_left, t1);
			p->n_right = r;
		} else {
			p = buildtree(CM, t1, r);
		}
	} else if (r->n_type == FLOAT) {
		/* XXX bounce in and out of temporary to change to int */
		NODE *t1 = tempnode(0, INT, 0, MKSUE(INT));
		int tmpnr = regno(t1);
		NODE *t2 = tempnode(tmpnr, r->n_type, r->n_df, r->n_sue);
		t1 =  movearg_32bit(t1, regp);
		r = block(ASSIGN, t2, r, r->n_type, r->n_df, r->n_sue);
		if (p->n_op == CM) {
			p->n_left = buildtree(CM, p->n_left, t1);
			p->n_right = r;
		} else {
			p = buildtree(CM, t1, r);
		}
	} else {
		*rp = movearg_32bit(r, regp);
	}

	return p;
}

/*
 * Called with a function call with arguments as argument.
 * This is done early in buildtree() and only done once.
 */
NODE *
funcode(NODE *p)
{
	int regnum = A0;
	NODE *l, *r, *t, *q;
	int ty;

	l = p->n_left;
	r = p->n_right;

	/*
	 * if returning a structure, make the first argument
	 * a hidden pointer to return structure.
	 */
	ty = DECREF(l->n_type);
	if (ty == STRTY+FTN || ty == UNIONTY+FTN) {
		ty = DECREF(l->n_type) - FTN;
		q = tempnode(0, ty, l->n_df, l->n_sue);
		q = buildtree(ADDROF, q, NIL);
		if (r->n_op != CM) {
			p->n_right = block(CM, q, r, INCREF(ty),
			    l->n_df, l->n_sue);
		} else {
			for (t = r; t->n_left->n_op == CM; t = t->n_left)
				;
			t->n_left = block(CM, q, t->n_left, INCREF(ty),
			    l->n_df, l->n_sue);
		}
	}

	p->n_right = moveargs(p->n_right, &regnum);

	return p;
}
