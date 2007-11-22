/*	$OpenBSD: code.c,v 1.3 2007/11/22 15:06:43 stefan Exp $	*/
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

/*
 * code for the end of a function
 * deals with struct return here
 */
void
efcode()
{
	if (cftnsp->stype != STRTY+FTN && cftnsp->stype != UNIONTY+FTN)
		return;
}

/*
 * code for the beginning of a function; a is an array of
 * indices in symtab for the arguments; n is the number
 */
void
bfcode(struct symtab **sp, int cnt)
{
	NODE *p, *q;
	int i, n;

	if (cftnsp->stype == STRTY+FTN || cftnsp->stype == UNIONTY+FTN) {
		/* Function returns struct, adjust arg offset */
		for (i = 0; i < cnt; i++)
			sp[i]->soffset += SZPOINT(INT);
	}

        /* recalculate the arg offset and create TEMP moves */
        for (n = A0, i = 0; i < cnt; i++) {
                if (n + szty(sp[i]->stype) <= A0 + MIPS_NARGREGS) {
			if (xtemps) {
	                        p = tempnode(0, sp[i]->stype,
				    sp[i]->sdf, sp[i]->ssue);
	                        spname = sp[i];
	                        q = block(REG, NIL, NIL,
	                            sp[i]->stype, sp[i]->sdf, sp[i]->ssue);
	                        q->n_rval = n;
	                        p = buildtree(ASSIGN, p, q);
	                        sp[i]->soffset = p->n_left->n_lval;
	                        sp[i]->sflags |= STNODE;
			} else {
				// sp[i]->soffset += ARGINIT;
				spname = sp[i];
				q = block(REG, NIL, NIL,
				    sp[i]->stype, sp[i]->sdf, sp[i]->ssue);
				q->n_rval = n;
                                p = buildtree(ASSIGN, buildtree(NAME, 0, 0), q);
			}
                        ecomp(p);
                } else {
                        // sp[i]->soffset += ARGINIT;
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

static void
moveargs(NODE **n, int *regp)
{
        NODE *r = *n;
        NODE *t;
        int sz;
        int regnum;

        if (r->n_op == CM) {
                moveargs(&r->n_left, regp);
                n = &r->n_right;
                r = r->n_right;
        }

        regnum = *regp;
        sz = szty(r->n_type);

        if (regnum + sz <= A0 + MIPS_NARGREGS) {
                t = block(REG, NIL, NIL, r->n_type, r->n_df, r->n_sue);
                t->n_rval = regnum;
                t = buildtree(ASSIGN, t, r);
        } else {
                t = block(FUNARG, r, NIL, r->n_type, r->n_df, r->n_sue);
        }

        *n = t;
        *regp += sz;
}

/*
 * Called with a function call with arguments as argument.
 * This is done early in buildtree() and only done once.
 */
NODE *
funcode(NODE *p)
{
	int regnum = A0;
	moveargs(&p->n_right, &regnum);
	return p;
}
