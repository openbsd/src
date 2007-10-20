/*	$OpenBSD: code.c,v 1.1 2007/10/20 10:01:38 otto Exp $	*/
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

# include "pass1.h"
#include "pass2.h"

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
	printf("	.align %d\n", n);
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
		printf("	.globl %s\n", exname(c));
	printf("%s:\n", exname(c));
}


/*
 * code for the end of a function
 * deals with struct return here
 */
void
efcode()
{
#if 0
	NODE *p, *q;
	int sz;
#endif

#if 0
	printf("EFCODE:\n");
#endif

	if (cftnsp->stype != STRTY+FTN && cftnsp->stype != UNIONTY+FTN)
		return;
	assert(0);
#if 0
	/* address of return struct is in eax */
	/* create a call to memcpy() */
	/* will get the result in eax */
	p = block(REG, NIL, NIL, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	p->n_rval = EAX;
	q = block(OREG, NIL, NIL, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	q->n_rval = EBP;
	q->n_lval = 8; /* return buffer offset */
	p = block(CM, q, p, INT, 0, MKSUE(INT));
	sz = (tsize(STRTY, cftnsp->sdf, cftnsp->ssue)+SZCHAR-1)/SZCHAR;
	p = block(CM, p, bcon(sz), INT, 0, MKSUE(INT));
	p->n_right->n_name = "";
	p = block(CALL, bcon(0), p, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	p->n_left->n_name = "memcpy";
	p = clocal(p);
	send_passt(IP_NODE, p);
#endif
}

/*
 * code for the beginning of a function; a is an array of
 * indices in symtab for the arguments; n is the number
 */
void
bfcode(struct symtab **a, int n)
{
	int i, m;

#if 0
	printf("BFCODE start with %d arguments\n", n);
#endif

	if (cftnsp->stype == STRTY+FTN && cftnsp->stype == UNIONTY+FTN) {
		/* Function returns struct, adjust arg offset */
		for (i = 0; i < n; i++)
			a[i]->soffset += SZPOINT(INT);
	}

	m = n <= 8 ? n : 8;

#if 0
	/* if optimised, assign parameters to registers */
	/* XXX consider the size of the types */
	for (i=0; i < m; i++) {
		a[i]->hdr.h_sclass = REGISTER;
		a[i]->hdr.h_offset = R3 + i;
	}
#endif

	/* if not optimised, */
	/* save the register arguments (R3-R10) onto the stack */
	int passedargoff = ARGINIT + FIXEDSTACKSIZE*8;  // XXX must add the size of the stack frame
	int reg = R3;
	for (i=0; i < m; i++) {
		NODE *r, *p;
		a[i]->hdr.h_sclass = PARAM;
		a[i]->hdr.h_offset = NOOFFSET;
		oalloc(a[i], &passedargoff);
		spname = a[i];
		p = buildtree(NAME, NIL, NIL);
		r = bcon(0);
		r->n_op = REG;
		if (BTYPE(p->n_type) == LONGLONG || BTYPE(p->n_type) == ULONGLONG) {
			r->n_rval = R3R4+(reg-R3);
			reg += 2;
		} else {
			r->n_rval = reg++;
		}
		r->n_type = p->n_type;
		r->n_sue = p->n_sue;
		r->n_df = p->n_df;
		ecode(buildtree(ASSIGN, p, r));
	}

#if 1
	/* XXXHACK save the rest of the registers too, for varargs */
	for (; reg < R11; reg++) {
		NODE *r, *l;
		l = bcon(0);
		l->n_op = OREG;
		l->n_lval = (passedargoff/SZCHAR);
		l->n_rval = FPREG;
		l->n_type = INT;
		passedargoff += SZINT;
		r = bcon(0);
		r->n_op = REG;
		r->n_rval = reg;
		r->n_type = INT;
		ecode(buildtree(ASSIGN, l, r));
	}
#endif
	
#if 0
	printf("BFCODE end\n");
#endif
}


/*
 * by now, the automatics and register variables are allocated
 */
void
bccode()
{
#if 0
	printf("BCCODE: autooff=%d, SZINT=%d\n", autooff, SZINT);
#endif
	SETOFF(autooff, SZINT);
}

struct stub stublist;
struct stub nlplist;

/* called just before final exit */
/* flag is 1 if errors, 0 if none */
void
ejobcode(int flag )
{
#if 0
	printf("EJOBCODE:\n");
#endif

	if (kflag) {
		// iterate over the stublist and output the PIC stubs
		struct stub *p;

		DLIST_FOREACH(p, &stublist, link) {
			printf("\t.section __TEXT, __picsymbolstub1,symbol_stubs,pure_instructions,32\n");
			printf("\t.align 5\n");
			printf("%s$stub:\n", p->name);
			printf("\t.indirect_symbol %s\n", p->name);
			printf("\tmflr r0\n");
			printf("\tbcl 20,31,L%s$stub$spb\n", p->name);
			printf("L%s$stub$spb:\n", p->name);
			printf("\tmflr r11\n");
			printf("\taddis r11,r11,ha16(L%s$lazy_ptr-L%s$stub$spb)\n", p->name, p->name);
			printf("\tmtlr r0\n");
			printf("\tlwzu r12,lo16(L%s$lazy_ptr-L%s$stub$spb)(r11)\n", p->name, p->name);
			printf("\tmtctr r12\n");
			printf("\tbctr\n");
			printf("\t.lazy_symbol_pointer\n");
			printf("L%s$lazy_ptr:\n", p->name);
			printf("\t.indirect_symbol %s\n", p->name);
			printf("\t.long	dyld_stub_binding_helper\n");
			printf("\t.subsections_via_symbols\n");

		}

		printf("\t.non_lazy_symbol_pointer\n");
		DLIST_FOREACH(p, &nlplist, link) {
			printf("L%s$non_lazy_ptr:\n", p->name);
			printf("\t.indirect_symbol %s\n", p->name);
			printf("\t.long 0\n");
	        }

		// memory leak here
	}
}

void
bjobcode()
{
#if 0
	printf("BJOBCODE:\n");
#endif

	DLIST_INIT(&stublist, link);
	DLIST_INIT(&nlplist, link);
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
 * n integer words of zeros
 */
void
zecode(int n)
{
	printf("	.zero %d\n", n * (SZINT/SZCHAR));
//	inoff += n * SZINT;
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

/* p points to an array of structures, each consisting
 * of a constant value and a label.
 * The first is >=0 if there is a default label;
 * its value is the label number
 * The entries p[1] to p[n] are the nontrivial cases
 * XXX - fix genswitch.
 * n is the number of case statemens (length of list)
 */
void
genswitch(int num, struct swents **p, int n)
{
	NODE *r;
	int i;

	/* simple switch code */
	for (i = 1; i <= n; ++i) {
		/* already in 1 */
		r = tempnode(num, INT, 0, MKSUE(INT));
		r = buildtree(NE, r, bcon(p[i]->sval));
		cbranch(buildtree(NOT, r, NIL), bcon(p[i]->slab));
	}
	if (p[0]->slab > 0)
		branch(p[0]->slab);
}
