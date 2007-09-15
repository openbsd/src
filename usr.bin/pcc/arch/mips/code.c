/*	$Id: code.c,v 1.1.1.1 2007/09/15 18:12:27 otto Exp $	*/
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

# include "pass1.h"
# include "manifest.h"

/* Offset to arguments passed to a function. */
int passedargoff;

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
		printf("	.globl %s\n", c);
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
}

/*
 * helper for bfcode() to put register arguments on stack.
 */
static void
argmove(struct symtab *s, int regno)
{
	NODE *p, *r;

	s->sclass = PARAM;
	s->soffset = NOOFFSET;

	oalloc(s, &passedargoff);

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
 */
void
bfcode(struct symtab **a, int n)
{
	int i, m;

	/* Passed arguments start 64 bits above the framepointer. */
	passedargoff = 64;
	
	if (cftnsp->stype == STRTY+FTN || cftnsp->stype == UNIONTY+FTN) {
		/* Function returns struct, adjust arg offset */
		for (i = 0; i < n; i++)
			a[i]->soffset += SZPOINT(INT);
	}

	m = n <= 4 ? n : 4;
	
	for(i = 0; i < m; i++) {
	    /*
	    if(a[i]->stype == LONGLONG || a[i]->stype == ULONGLONG) {
		printf("longlong\n");
		argmove(a[i], A0+i);

		if(i+1 < 4) {
		    argmove(a[i], A0+i+1);
		}
		
		i++;		
		} else*/
	    argmove(a[i], A0+i);

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
}
