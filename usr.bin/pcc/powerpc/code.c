/*	$OpenBSD: code.c,v 1.2 2007/11/01 10:52:58 otto Exp $	*/
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
#include <stdlib.h>

#include "pass1.h"
#include "pass2.h"

static void genswitch_simple(int num, struct swents **p, int n);
static void genswitch_bintree(int num, struct swents **p, int n);
static void genswitch_table(int num, struct swents **p, int n);
static void genswitch_mrst(int num, struct swents **p, int n);

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
	if (n == 0) {
		if (p[0]->sval != 0)
			branch(p[0]->sval);
		return;
	}

#ifdef PCC_DEBUG
	if (xdebug) {
		int i;
		for (i = 1; i <= n; i++)
			printf("%d: %llu\n", i, p[i]->sval);
	}
#endif

	if (0)
	genswitch_table(num, p, n);
	if (0)
	genswitch_bintree(num, p, n);
	genswitch_mrst(num, p, n);
}

static void
genswitch_simple(int num, struct swents **p, int n)
{
	NODE *r;
	int i;

	for (i = 1; i <= n; ++i) {
		r = tempnode(num, INT, 0, MKSUE(INT));
		r = buildtree(NE, r, bcon(p[i]->sval));
		cbranch(buildtree(NOT, r, NIL), bcon(p[i]->slab));
	}
	if (p[0]->slab > 0)
		branch(p[0]->slab);
}

static void bintree_rec(int num, struct swents **p, int n, int s, int e);

static void
genswitch_bintree(int num, struct swents **p, int n)
{
	int lab = getlab();

	if (p[0]->slab == 0)
		p[0]->slab = lab;

	bintree_rec(num, p, n, 1, n);

	plabel(lab);
}

static void
bintree_rec(int num, struct swents **p, int n, int s, int e)
{
	NODE *r;
	int rlabel;
	int h;

	if (s == e) {
		r = tempnode(num, INT, 0, MKSUE(INT));
		r = buildtree(NE, r, bcon(p[s]->sval));
		cbranch(buildtree(NOT, r, NIL), bcon(p[s]->slab));
		branch(p[0]->slab);
		return;
	}

	rlabel = getlab();

	h = s + (e - s) / 2;

	r = tempnode(num, INT, 0, MKSUE(INT));
	r = buildtree(GT, r, bcon(p[h]->sval));
	cbranch(r, bcon(rlabel));
	bintree_rec(num, p, n, s, h);
	plabel(rlabel);
	bintree_rec(num, p, n, h+1, e);
}



static void
genswitch_table(int num, struct swents **p, int n)
{
	NODE *r, *t;
	int tval;
	int minval, maxval, range;
	int deflabel, tbllabel;
	int i, j;

	minval = p[1]->sval;
	maxval = p[n]->sval;

	range = maxval - minval + 1;

	if (n < 10 || range > 3 * n) {
		/* too small or too sparse for jump table */
		genswitch_simple(num, p, n);
		return;
	}

	r = tempnode(num, UNSIGNED, 0, MKSUE(UNSIGNED));
	r = buildtree(MINUS, r, bcon(minval));
	t = tempnode(0, UNSIGNED, 0, MKSUE(UNSIGNED));
	tval = t->n_lval;
	r = buildtree(ASSIGN, t, r);
	ecomp(r);

	deflabel = p[0]->slab;
	if (deflabel == 0)
		deflabel = getlab();

	t = tempnode(tval, UNSIGNED, 0, MKSUE(UNSIGNED));
	cbranch(buildtree(GT, t, bcon(maxval-minval)), bcon(deflabel));

	tbllabel = getlab();
	struct symtab *strtbl = lookup("__switch_table", SLBLNAME|STEMP);
	strtbl->soffset = tbllabel;
	strtbl->sclass = ILABEL;
	strtbl->stype = INCREF(UCHAR);

	t = block(NAME, NIL, NIL, UNSIGNED, 0, MKSUE(UNSIGNED));
	t->n_sp = strtbl;
	t = buildtree(ADDROF, t, NIL);
	r = tempnode(tval, UNSIGNED, 0, MKSUE(INT));
	r = buildtree(PLUS, t, r);
	t = tempnode(0, INCREF(UNSIGNED), 0, MKSUE(UNSIGNED));
	r = buildtree(ASSIGN, t, r);
	ecomp(r);

	r = tempnode(t->n_lval, INCREF(UNSIGNED), 0, MKSUE(UNSIGNED));
	r = buildtree(UMUL, r, NIL);
	t = block(NAME, NIL, NIL, UCHAR, 0, MKSUE(UCHAR));
	t->n_sp = strtbl;
	t = buildtree(ADDROF, t, NIL);
	r = buildtree(PLUS, t, r);
	r = block(GOTO, r, NIL, 0, 0, 0);
	ecomp(r);

	plabel(tbllabel);
	for (i = minval, j=1; i <= maxval; i++) {
		char *entry = tmpalloc(20);
		int lab = deflabel;
		//printf("; minval=%d, maxval=%d, i=%d, j=%d p[j]=%lld\n", minval, maxval, i, j, p[j]->sval);
		if (p[j]->sval == i) {
			lab = p[j]->slab;
			j++;
		}
		snprintf(entry, 20, ".long " LABFMT "-" LABFMT, lab, tbllabel);
		send_passt(IP_ASM, entry);
	}

	if (p[0]->slab <= 0)
		plabel(deflabel);
}

#define DPRINTF(x)	if (xdebug) printf x
//#define DPRINTF(x)	do { } while(0)

#define MIN_TABLE_SIZE	8

/*
 *  Multi-way Radix Search Tree (MRST)
 */

static void mrst_rec(int num, struct swents **p, int n, int *state, int lab);
static unsigned long mrst_find_window(struct swents **p, int n, int *state, int lab, int *len, int *lowbit);
void mrst_put_entry_and_recurse(int num, struct swents **p, int n, int *state, int tbllabel, int lab, unsigned long j, unsigned long tblsize, unsigned long Wmax, int lowbit);

static void
genswitch_mrst(int num, struct swents **p, int n)
{
	int *state;
	int i;
	int putlabel = 0;

	if (n < 10) {
		/* too small for MRST */
		genswitch_simple(num, p, n);
		return;
	}

	state = tmpalloc((n+1)*sizeof(int));
	for (i = 0; i <= n; i++)
		state[i] = 0;

	if (p[0]->slab == 0) {
		p[0]->slab = getlab();
		putlabel = 1;
	}

	mrst_rec(num, p, n, state, 0);

	if (putlabel)
		plabel(p[0]->slab);
}


/*
 *  Look through the cases and generate a table or
 *  list of simple comparisons.  If generating a table,
 *  invoke mrst_put_entry_and_recurse() to put
 *  an entry in the table and recurse.
 */
static void
mrst_rec(int num, struct swents **p, int n, int *state, int lab)
{
	int len, lowbit;
	unsigned long Wmax;
	unsigned int tblsize;
	NODE *t;
	NODE *r;
	int tval;
	int i;

	DPRINTF(("mrst_rec: num=%d, n=%d, lab=%d\n", num, n, lab));

	/* find best window to cover set*/
	Wmax = mrst_find_window(p, n, state, lab, &len, &lowbit);
	tblsize = (1 << len);
	assert(len > 0 && tblsize > 0);

	DPRINTF(("mrst_rec: Wmax=%lu, lowbit=%d, tblsize=%u\n",
		Wmax, lowbit, tblsize));

	if (lab)
		plabel(lab);

	if (tblsize <= MIN_TABLE_SIZE) {
		DPRINTF(("msrt_rec: break the recursion\n"));
		for (i = 1; i <= n; i++) {
			if (state[i] == lab) {
				t = tempnode(num, UNSIGNED, 0, MKSUE(UNSIGNED));
				cbranch(buildtree(EQ, t, bcon(p[i]->sval)),
				    bcon(p[i]->slab));
			}
		}
		branch(p[0]->slab);
		return;
	}

	DPRINTF(("generating table with %d elements\n", tblsize));

	// AND with Wmax
	t = tempnode(num, UNSIGNED, 0, MKSUE(UNSIGNED));
	r = buildtree(AND, t, bcon(Wmax));

	// RS lowbits
	r = buildtree(RS, r, bcon(lowbit));

	t = tempnode(0, UNSIGNED, 0, MKSUE(UNSIGNED));
	tval = t->n_lval;
	r = buildtree(ASSIGN, t, r);
	ecomp(r);

	int tbllabel = getlab();
	struct symtab *strtbl = lookup("__switch_table", SLBLNAME|STEMP);
	strtbl->soffset = tbllabel;
	strtbl->sclass = ILABEL;
	strtbl->stype = INCREF(UCHAR);

	t = block(NAME, NIL, NIL, UNSIGNED, 0, MKSUE(UNSIGNED));
	t->n_sp = strtbl;
	t = buildtree(ADDROF, t, NIL);
	r = tempnode(tval, UNSIGNED, 0, MKSUE(INT));
	r = buildtree(PLUS, t, r);
	t = tempnode(0, INCREF(UNSIGNED), 0, MKSUE(UNSIGNED));
	r = buildtree(ASSIGN, t, r);
	ecomp(r);

	r = tempnode(t->n_lval, INCREF(UNSIGNED), 0, MKSUE(UNSIGNED));
	r = buildtree(UMUL, r, NIL);
	t = block(NAME, NIL, NIL, UCHAR, 0, MKSUE(UCHAR));
	t->n_sp = strtbl;
	t = buildtree(ADDROF, t, NIL);
	r = buildtree(PLUS, t, r);
	r = block(GOTO, r, NIL, 0, 0, 0);
	ecomp(r);

	plabel(tbllabel);
	
	mrst_put_entry_and_recurse(num, p, n, state, tbllabel, lab,
		0, tblsize, Wmax, lowbit);
}


/*
 * Put an entry into the table and recurse to the next entry
 * in the table.  On the way back through the recursion, invoke
 * mrst_rec() to check to see if we should generate another
 * table.
 */
void
mrst_put_entry_and_recurse(int num, struct swents **p, int n, int *state,
	int tbllabel, int labval,
	unsigned long j, unsigned long tblsize, unsigned long Wmax, int lowbit)
{
	int i;
	int found = 0;
	int lab = getlab();

	/*
	 *  Look for labels which map to this table entry.
	 *  Mark each one in "state" that they fall inside this table.
	 */
	for (i = 1; i <= n; i++) {
		unsigned int val = (p[i]->sval & Wmax) >> lowbit;
		if (val == j && state[i] == labval) {
			found = 1;
			state[i] = lab;
		}
	}

	/* couldn't find any labels?  goto the default label */
	if (!found)
		lab = p[0]->slab;

	/* generate the table entry */
	char *entry = tmpalloc(20);
	snprintf(entry, 20, ".long " LABFMT "-" LABFMT, lab, tbllabel);
	send_passt(IP_ASM, entry);

	DPRINTF(("mrst_put_entry: table=%d, pos=%lu/%lu, label=%d\n",
	    tbllabel, j, tblsize, lab));

	/* go to the next table entry */
	if (j+1 < tblsize) {
		mrst_put_entry_and_recurse(num, p, n, state, tbllabel, labval,
			j+1, tblsize, Wmax, lowbit);
	}

	/* if we are going to the default label, bail now */
	if (!found)
		return;

#ifdef PCC_DEBUG
	if (xdebug) {
		printf("state: ");
		for (i = 1; i <= n; i++)
			printf("%d ", state[i]);
		printf("\n");
	}
#endif

	/* build another table */
	mrst_rec(num, p, n, state, lab);
}

/*
 * counts the number of entries in a table of size (1 << L) which would
 * be used given the cases and the mask (W, lowbit).
 */
static unsigned int
mrst_cardinality(struct swents **p, int n, int *state, int step, unsigned long W, int L, int lowbit)
{
	unsigned int count = 0;
	int i;

	if (W == 0)
		return 0;

	int *vals = (int *)calloc(1 << L, sizeof(int));
	assert(vals);

	DPRINTF(("mrst_cardinality: "));
	for (i = 1; i <= n; i++) {
		int idx;
		if (state[i] != step)
			continue;
		idx = (p[i]->sval & W) >> lowbit;
		DPRINTF(("%llu->%d, ", p[i]->sval, idx));
		if (!vals[idx]) {
			count++;
		}
		vals[idx] = 1;
	}
	DPRINTF((": found %d entries\n", count));
	free(vals);

	return count;
}

/*
 *  Find the maximum window (table size) which would best cover
 *  the set of labels.  Algorithm explained in:
 *
 *  Ulfar Erlingsson, Mukkai Krishnamoorthy and T.V. Raman.
 *  Efficient Multiway Radix Search Trees.
 *  Information Processing Letters 60:3 115-120 (November 1996)
 */

static unsigned long
mrst_find_window(struct swents **p, int n, int *state, int lab, int *len, int *lowbit)
{
	unsigned int tblsize;
	unsigned long W = 0;
	unsigned long Wmax = 0;
	unsigned long Wleft = (1 << (SZLONG-1));
	unsigned int C = 0;
	unsigned int Cmax = 0;
	int L = 0;
	int Lmax = 0;
	int lowmax = 0;
	int no_b = SZLONG-1;
	unsigned long b = (1 << (SZLONG-1));

	DPRINTF(("mrst_find_window: n=%d, lab=%d\n", n, lab));

	for (; b > 0; b >>= 1, no_b--) {

		// select the next bit
		W |= b;
		L += 1;

		tblsize = 1 << L;
		assert(tblsize > 0);

		DPRINTF(("no_b=%d, b=0x%lx, Wleft=0x%lx, W=0x%lx, Wmax=0x%lx, L=%d, Lmax=%d, Cmax=%u, lowmax=%d, tblsize=%u\n", no_b, b, Wleft, W, Wmax, L, Lmax, Cmax, lowmax, tblsize));

		C = mrst_cardinality(p, n, state, lab, W, L, no_b);
		DPRINTF((" -> cardinality is %d\n", C));

		if (2*C >= tblsize) {
			DPRINTF(("(found good match, keep adding to table)\n"));
			Wmax = W;
			Lmax = L;
			lowmax = no_b;
			Cmax = C;
		} else {
			DPRINTF(("(too sparse)\n"));
			assert((W & Wleft) != 0);

			/* flip the MSB and see if we get a better match */
			W ^= Wleft;
			Wleft >>= 1;
			L -= 1;

			DPRINTF((" --> trying W=0x%lx and L=%d and Cmax=%u\n", W, L, Cmax));
			C = mrst_cardinality(p, n, state, lab, W, L, no_b);
			DPRINTF((" --> C=%u\n", C));
			if (C > Cmax) {
				Wmax = W;
				Lmax = L;
				lowmax = no_b;
				Cmax = C;
				DPRINTF((" --> better!\n"));
			} else {
				DPRINTF((" --> no better\n"));
			}
		}

	}

#ifdef PCC_DEBUG
	if (xdebug) {
		int i;
		int hibit = lowmax + Lmax;
		printf("msrt_find_window: Wmax=0x%lx, lowbit=%d, result=", Wmax, lowmax);
		for (i = 31; i >= 0; i--) {
			int mask = (1 << i);
			if (i == hibit)
				printf("[");
			if (Wmax & mask)
				printf("1");
			else
				printf("0");
			if (i == lowmax)
				printf("]");
		}
		printf("\n");
	}
#endif

	assert(Lmax > 0);
	*len = Lmax;
	*lowbit = lowmax;

	DPRINTF(("msrt_find_window: returning Wmax=%lu, len=%d, lowbit=%d [tblsize=%u, entries=%u]\n", Wmax, Lmax, lowmax, tblsize, C));

	return Wmax;
}
