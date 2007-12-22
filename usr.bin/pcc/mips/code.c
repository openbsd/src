/*	$OpenBSD: code.c,v 1.4 2007/12/22 14:12:26 stefan Exp $	*/
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
 * cause the alignment to become a multiple of n
 * never called for text segment.
 */
void
defalign(int n)
{
	n /= SZCHAR;
	if (n == 1)
		return;
	printf("\t.align %d\n", n);
}

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
		printf("\t.globl %s\n", c);
#ifdef USE_GAS
	printf("\t.type %s,@object\n", c);
	printf("\t.size %s," CONFMT "\n", c, tsize(p->stype, p->sdf, p->ssue));
#endif
	printf("%s:\n", c);
}

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
	tempnr = p->n_lval;
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
	sym->soffset = p->n_left->n_lval;
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
	rvnr = p->n_lval;
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
	int sz;

	/* alignment */
	++reg;
	reg &= ~1;

	navail = nargregs - (reg - A0);
	sz = szty(sym->stype);

	if (sz > navail) {
		/* would have appeared half in registers/half
		 * on the stack, but alignment ensures it
		 * appears on the stack */
		if (dotemps)
			putintemp(sym);
	} else {
		q = block(REG, NIL, NIL,
		    sym->stype, sym->sdf, sym->ssue);
		q->n_rval = A0A1 + (reg - A0);
		if (dotemps) {
			p = tempnode(0, sym->stype, sym->sdf, sym->ssue);
			sym->soffset = p->n_lval;
			sym->sflags |= STNODE;
		} else {
			spname = sym;
			p = buildtree(NAME, 0, 0);
		}
		p = buildtree(ASSIGN, p, q);
		ecomp(p);
		reg += 2;
	}

	*regp = reg;
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
		sym->soffset = p->n_lval;
		sym->sflags |= STNODE;
	} else {
		spname = sym;
		p = buildtree(NAME, 0, 0);
	}
	p = buildtree(ASSIGN, p, q);
	ecomp(p);
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
		else if (sp[i]->stype == DOUBLE || sp[i]->stype == LDOUBLE ||
		     DEUNSIGN(sp[i]->stype) == LONGLONG)
			param_64bit(sp[i], &reg, xtemps && !saveallargs);
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
			puts("\"");
	} else {
		if (i == 0)
			printf("\t.asciiz \"");
		if (t == 0)
			return;
		else if (t == '\\' || t == '"') {
			lastoctal = 0;
			putchar('\\');
			putchar(t);
		} else if (t == 012) {
			printf("\\n");
		} else if (t < 040 || t >= 0177) {
			lastoctal++;
			printf("\\%o",t);
		} else if (lastoctal && '0' <= t && t <= '9') {
			lastoctal = 0;
			printf("\"\n\t.asciiz \"%c", t);
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


/* setup call stack with a structure */
/* called from moveargs() */
static NODE *
movearg_struct(NODE *p, NODE *parent, int *regp)
{
	int reg = *regp;
	NODE *q, *t, *r;
	int navail;
	int off;
	int num;
        int sz;
	int i;

	navail = nargregs - (reg - A0);
	sz = tsize(p->n_type, p->n_df, p->n_sue) / SZINT;
	num = sz > navail ? navail : sz;

	if (p != parent)
		q = parent->n_left;
	else
		q = NULL;

	/* copy structure into registers */
	for (i = 0; i < num; i++) {
		t = tcopy(p->n_left);
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
		t = tcopy(p->n_left);
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
	tfree(p);

	if (parent->n_op == CM) {
		parent->n_left = q->n_left;
		t = q;
		q = q->n_right;
		nfree(t);
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

	/* alignment */
	++reg;
	reg &= ~1;

	q = block(REG, NIL, NIL, p->n_type, p->n_df, p->n_sue);
	q->n_rval = A0A1 + (reg++ - A0);
	q = buildtree(ASSIGN, q, p);

	*regp = reg;
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
	} else if (DEUNSIGN(r->n_type) == LONGLONG || r->n_type == DOUBLE ||
	    r->n_type == LDOUBLE)
		*rp = movearg_64bit(r, regp);
	else
		*rp = movearg_32bit(r, regp);

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
