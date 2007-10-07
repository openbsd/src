/*	$OpenBSD: code.c,v 1.1 2007/10/07 17:58:51 otto Exp $	*/
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
 * cause the alignment to become a multiple of n
 */
void
defalign(int n)
{
#if 0
	char *s;

	n /= SZCHAR;
	if (lastloc == PROG || n == 1)
		return;
	s = (isinlining ? permalloc(40) : tmpalloc(40));
	sprintf(s, ".align %d", n);
	send_passt(IP_ASM, s);
#endif
}

/*
 * define the current location as the name p->sname
 */
void
defnam(struct symtab *p)
{
	char *c = p->sname;

#ifdef GCC_COMPAT
	c = gcc_findname(p);
#endif
	if (p->sclass == EXTDEF)
		printf("	PUBLIC %s\n", c);
	printf("%s:\n", c);
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
	/* will get the result in eax */
	p = block(REG, NIL, NIL, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	p->n_rval = R0;
	q = block(OREG, NIL, NIL, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	q->n_rval = FB;
	q->n_lval = 8; /* return buffer offset */
	p = block(CM, q, p, INT, 0, MKSUE(INT));
	sz = (tsize(STRTY, cftnsp->sdf, cftnsp->ssue)+SZCHAR-1)/SZCHAR;
	p = block(CM, p, bcon(sz), INT, 0, MKSUE(INT));
	p->n_right->n_name = "";
	p = block(CALL, bcon(0), p, CHAR+PTR, 0, MKSUE(CHAR+PTR));
	p->n_left->n_name = "memcpy";
	send_passt(IP_NODE, p);
}

/*
 * helper for bfcode() to put register arguments on stack.
 */
static void
argmove(struct symtab *s, int regno)
{
	NODE *p, *r;

	s->sclass = AUTO;
	s->soffset = NOOFFSET;
	oalloc(s, &autooff);
	spname = s;
	p = buildtree(NAME, NIL, NIL);
	r = bcon(0);
	r->n_op = REG;
	r->n_rval = regno;
	r->n_type = p->n_type;
	r->n_sue = p->n_sue;
	r->n_df = p->n_df;
	ecode(buildtree(ASSIGN, p, r));
}

/*
 * code for the beginning of a function; a is an array of
 * indices in symtab for the arguments; n is the number
 * On m16k, space is allocated on stack for register arguments,
 * arguments are moved to the stack and symtab is updated accordingly.
 */
void
bfcode(struct symtab **a, int n)
{
	struct symtab *s;
	int i, r0l, r0h, a0, r2, sz, hasch, stk;
	int argoff = ARGINIT;

	if (cftnsp->stype == STRTY+FTN || cftnsp->stype == UNIONTY+FTN) {
		/* Function returns struct, adjust arg offset */
		for (i = 0; i < n; i++)
			a[i]->soffset += SZPOINT(INT);
	}
	/* first check if there are 1-byte parameters */
	for (hasch = i = 0; i < n && i < 6; i++)
		if (DEUNSIGN(a[i]->stype) == CHAR)
			hasch = 1;

	stk = r0l = r0h = a0 = r2 = 0;
	for (i = 0; i < n; i++) {
		s = a[i];
		sz = tsize(s->stype, s->sdf, s->ssue);
		if (ISPTR(s->stype) && ISFTN(DECREF(s->stype)))
			sz = SZLONG; /* function pointers are always 32 */
		if (stk == 0)
		    switch (sz) {
		case SZCHAR:
			if (r0l) {
				if (r0h)
					break;
				argmove(s, 1);
				r0h = 1;
			} else {
				argmove(s, 0);
				r0l = 1;
			}
			continue;

		case SZINT:
			if (s->stype > BTMASK) {
				/* is a pointer */
				if (a0) {
					if (r0l || hasch) {
						if (r2)
							break;
						argmove(s, R2);
						r2 = 1;
					} else {
						argmove(s, R0);
						r0l = r0h = 1;
					}
				} else {
					argmove(s, A0);
					a0 = 1;
				}
			} else if (r0l || hasch) {
				if (r2) {
					if (a0)
						break;
					argmove(s, A0);
					a0 = 1;
				} else {
					argmove(s, R2);
					r2 = 1;
				}
			} else {
				argmove(s, R0);
				r0l = r0h = 1;
			}
			continue;
		case SZLONG:
			if (r0l||r0h||r2)
				break;
			argmove(s, R0);
			r0l = r0h = r2 = 1;
			continue;

		default:
			break;
		}
		stk = 1;
		s->soffset = argoff;
		argoff += sz;
	}
}

/*
 * Add a symbol to an internal list printed out at the end.
 */
void addsym(struct symtab *);
static struct symlst {
	struct symlst *next;
	struct symtab *sp;
} *sympole;

void
addsym(struct symtab *q)
{
	struct symlst *w = sympole;

	if (q == NULL)
		return;

	while (w) {
		if (q == w->sp)
			return; /* exists */
		w = w->next;
	}
	w = permalloc(sizeof(struct symlst));
	w->sp = q;
	w->next = sympole;
	sympole = w;
}

/*
 * by now, the automatics and register variables are allocated
 */
void
bccode()
{
}

struct caps {
	char *cap, *stat;
} caps[] = {
	{ "__64bit_doubles", "Disabled" },
	{ "__calling_convention", "Normal" },
	{ "__constant_data", "near" },
	{ "__data_alignment", "2" },
	{ "__data_model", "near" },
	{ "__processor", "M16C" },
	{ "__rt_version", "1" },
	{ "__variable_data", "near" },
	{ NULL, NULL },
};
/*
 * Called before parsing begins.
 */
void
bjobcode()
{
	struct caps *c;

	printf("	NAME gurka.c\n"); /* Don't have the name */
	for (c = caps; c->cap; c++)
		printf("	RTMODEL \"%s\", \"%s\"\n", c->cap, c->stat);
	//printf("	RSEG CODE:CODE:REORDER:NOROOT(0)\n");
}

/* called just before final exit */
/* flag is 1 if errors, 0 if none */
void
ejobcode(int flag )
{
	struct symlst *w = sympole;

	for (w = sympole; w; w = w->next) {
		if (w->sp->sclass != EXTERN)
			continue;
		printf("	EXTERN %s\n", w->sp->sname);
	}
	
	printf("	END\n");
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
	inoff += n * SZINT;
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
 */
void
genswitch(struct swents **p, int n)
{
    uerror("switch() statements unsopported");
#if 0
	int i;
	char *s;

	/* simple switch code */
	for (i = 1; i <= n; ++i) {
		/* already in 1 */
		s = (isinlining ? permalloc(40) : tmpalloc(40));
		sprintf(s, "	cmpl $%lld,%%eax", p[i]->sval);
		send_passt(IP_ASM, s);
		s = (isinlining ? permalloc(40) : tmpalloc(40));
		sprintf(s, "	je " LABFMT, p[i]->slab);
		send_passt(IP_ASM, s);
	}
	if (p[0]->slab > 0)
		branch(p[0]->slab);
#endif
}
