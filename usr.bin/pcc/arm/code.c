/*      $OpenBSD: code.c,v 1.3 2008/04/11 20:45:52 stefan Exp $    */
/*
 * Copyright (c) 2007 Gregory McGarry (g.mcgarry@ieee.org).
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
 *  Stuff for pass1.
 */

#include <assert.h>

#include "pass1.h"
#include "pass2.h"

int lastloc = -1;
/*
 * Define everything needed to print out some data (or text).
 * This means segment, alignment, visibility, etc.
 */
void
defloc(struct symtab *sp)
{
	extern char *nextsect;
	static char *loctbl[] = { "text", "data", "section .rodata" };
	TWORD t;
	int s;

	if (sp == NULL) {
		lastloc = -1;
		return;
	}
	t = sp->stype;
	s = ISFTN(t) ? PROG : ISCON(cqual(t, sp->squal)) ? PROG : DATA;
	if (nextsect) {
		printf("\t.section %s\n", nextsect);
		nextsect = NULL;
		s = -1;
	} else if (s != lastloc)
		printf("\t.%s\n", loctbl[s]);
	lastloc = s;
	while (ISARY(t))
		t = DECREF(t);
	if (t > UCHAR)
		printf("\t.align %d\n", t > USHORT ? 4 : 2);
#ifdef USE_GAS
	if (ISFTN(t))
		printf("\t.type %s,%%function\n", exname(sp->soname));
#endif
	if (sp->sclass == EXTDEF)
		printf("\t.global %s\n", exname(sp->soname));
	if (ISFTN(t))
		return;
	if (sp->slevel == 0)
		printf("%s:\n", exname(sp->soname));
	else
		printf(LABFMT ":\n", sp->soffset);
}

/* Put a symbol in a temporary
 * used by bfcode() and its helpers
 */
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

/* setup a 64-bit parameter (double/ldouble/longlong)
 * used by bfcode() */
static void
param_64bit(struct symtab *sym, int *argoffp, int dotemps)
{
        int argoff = *argoffp;
        NODE *p, *q;
        int navail;

#if ALLONGLONG == 64
	/* alignment */
	++argoff;
	argoff &= ~1;
	*argoffp = argoff;
#endif

        navail = NARGREGS - argoff;

        if (navail < 2) {
		/* half in and half out of the registers */
		if (features(FEATURE_BIGENDIAN)) {
			cerror("movearg_64bit");
			p = q = NULL;
		} else {
        		q = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
		        regno(q) = R0 + argoff;
			if (dotemps) {
				q = block(SCONV, q, NIL,
				    ULONGLONG, 0, MKSUE(ULONGLONG));
				spname = sym;
		                p = buildtree(NAME, 0, 0);
				p->n_type = ULONGLONG;
				p->n_df = 0;
				p->n_sue = MKSUE(ULONGLONG);
				p = block(LS, p, bcon(32), ULONGLONG,
				    0, MKSUE(ULONGLONG));
				q = block(PLUS, p, q, ULONGLONG,
				    0, MKSUE(ULONGLONG));
				p = tempnode(0, ULONGLONG,
				    0, MKSUE(ULONGLONG));
				sym->soffset = regno(p);
				sym->sflags |= STNODE;
			} else {
                		spname = sym;
		                p = buildtree(NAME, 0, 0);
				regno(p) = sym->soffset;
				p->n_type = INT;
				p->n_df = 0;
				p->n_sue = MKSUE(INT);
			}
		}
	        p = buildtree(ASSIGN, p, q);
		ecomp(p);
	        *argoffp = argoff + 2;
		return;
        }

        q = block(REG, NIL, NIL, sym->stype, sym->sdf, sym->ssue);
        regno(q) = R0R1 + argoff;
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
        *argoffp = argoff + 2;
}

/* setup a 32-bit param on the stack
 * used by bfcode() */
static void
param_32bit(struct symtab *sym, int *argoffp, int dotemps)
{
        NODE *p, *q;

        q = block(REG, NIL, NIL, sym->stype, sym->sdf, sym->ssue);
        regno(q) = R0 + (*argoffp)++;
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

/* setup a double param on the stack
 * used by bfcode() */
static void
param_double(struct symtab *sym, int *argoffp, int dotemps)
{
        NODE *p, *q, *t;
        int tmpnr;

	/*
	 * we have to dump the float from the general register
	 * into a temp, since the register allocator doesn't like
	 * floats to be in CLASSA.  This may not work for -xtemps.
	 */

        t = tempnode(0, ULONGLONG, 0, MKSUE(ULONGLONG));
        tmpnr = regno(t);
        q = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
        q->n_rval = R0R1 + (*argoffp)++;
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

/* setup a float param on the stack
 * used by bfcode() */
static void
param_float(struct symtab *sym, int *argoffp, int dotemps)
{
        NODE *p, *q, *t;
        int tmpnr;

	/*
	 * we have to dump the float from the general register
	 * into a temp, since the register allocator doesn't like
	 * floats to be in CLASSA.  This may not work for -xtemps.
	 */

        t = tempnode(0, INT, 0, MKSUE(INT));
        tmpnr = regno(t);
        q = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
        q->n_rval = R0 + (*argoffp)++;
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

int rvnr;

/*
 * Beginning-of-function code:
 *
 * 'sp' is an array of indices in symtab for the arguments
 * 'cnt' is the number of arguments
 */
void
bfcode(struct symtab **sp, int cnt)
{
	NODE *p, *q;
	union arglist *usym;
	int saveallargs = 0;
	int i, argoff = 0;

        /*
         * Detect if this function has ellipses and save all
         * argument registers onto stack.
         */
        usym = cftnsp->sdf->dfun;
        while (usym && usym->type != TNULL) {
                if (usym->type == TELLIPSIS) {
                        saveallargs = 1;
                        break;
                }
                ++usym;
        }

	/* if returning a structure, more the hidden argument into a TEMP */
        if (cftnsp->stype == STRTY+FTN || cftnsp->stype == UNIONTY+FTN) {
		p = tempnode(0, PTR+STRTY, 0, cftnsp->ssue);
		rvnr = regno(p);
		q = block(REG, NIL, NIL, PTR+STRTY, 0, cftnsp->ssue);
		q->n_rval = R0 + argoff++;
		p = buildtree(ASSIGN, p, q);
		ecomp(p);
	}

        /* recalculate the arg offset and create TEMP moves */
        for (i = 0; i < cnt; i++) {

                if ((argoff >= NARGREGS) && !xtemps) {
                        break;
                } else if (argoff > NARGREGS) {
                        putintemp(sp[i]);
                } else if (sp[i]->stype == STRTY || sp[i]->stype == UNIONTY) {
			cerror("unimplemented structure arguments");
                } else if (DEUNSIGN(sp[i]->stype) == LONGLONG) {
                        param_64bit(sp[i], &argoff, xtemps && !saveallargs);
                } else if (sp[i]->stype == DOUBLE || sp[i]->stype == LDOUBLE) {
			if (features(FEATURE_HARDFLOAT))
	                        param_double(sp[i], &argoff,
				    xtemps && !saveallargs);
			else
	                        param_64bit(sp[i], &argoff,
				    xtemps && !saveallargs);
                } else if (sp[i]->stype == FLOAT) {
			if (features(FEATURE_HARDFLOAT))
                        	param_float(sp[i], &argoff,
				    xtemps && !saveallargs);
			else
                        	param_32bit(sp[i], &argoff,
				    xtemps && !saveallargs);
                } else {
                        param_32bit(sp[i], &argoff, xtemps && !saveallargs);
		}
        }

        /* if saveallargs, save the rest of the args onto the stack */
        while (saveallargs && argoff < NARGREGS) {
      		NODE *p, *q;
		int off = ARGINIT/SZINT + argoff;
		q = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
		regno(q) = R0 + argoff++;
		p = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
		regno(p) = FPREG;
		p = block(PLUS, p, bcon(4*off), INT, 0, MKSUE(INT));
		p = block(UMUL, p, NIL, INT, 0, MKSUE(INT));
		p = buildtree(ASSIGN, p, q);
		ecomp(p);
	}

}

/*
 * End-of-Function code:
 */
void
efcode()
{
	NODE *p, *q;
	int tempnr;

	if (cftnsp->stype != STRTY+FTN && cftnsp->stype != UNIONTY+FTN)
		return;

	/*
	 * At this point, the address of the return structure on
	 * has been FORCEd to RETREG, which is R0.
	 * We want to copy the contents from there to the address
	 * we placed into the tempnode "rvnr".
	 */

	/* move the pointer out of R0 to a tempnode */
	q = block(REG, NIL, NIL, PTR+STRTY, 0, cftnsp->ssue);
	q->n_rval = R0;
	p = tempnode(0, PTR+STRTY, 0, cftnsp->ssue);
	tempnr = regno(p);
	p = buildtree(ASSIGN, p, q);
	ecomp(p);

	/* get the address from the tempnode */
	q = tempnode(tempnr, PTR+STRTY, 0, cftnsp->ssue);
	q = buildtree(UMUL, q, NIL);
	
	/* now, get the structure destination */
	p = tempnode(rvnr, PTR+STRTY, 0, cftnsp->ssue);
	p = buildtree(UMUL, p, NIL);

	/* struct assignment */
	p = buildtree(ASSIGN, p, q);
	ecomp(p);
}

/*
 * Beginning-of-code: finished generating function prologue
 *
 * by now, the automatics and register variables are allocated
 */
void
bccode()
{
	SETOFF(autooff, SZINT);
}

/*
 * End-of-job: called just before final exit.
 */
void
ejobcode(int flag )
{
#define OSB(x) __STRING(x)
#define OS OSB(TARGOS)
	printf("\t.ident \"%s (%s)\"\n", VERSSTR, OS);
}

/*
 * Beginning-of-job: called before compilation starts
 *
 * Initialise data structures specific for the local machine.
 */
void
bjobcode()
{
}

/*
 * Compute the alignment of object with type 't'.
 */
int
fldal(unsigned int t)
{
	uerror("illegal field type");
	return(ALINT);
}

/*
 * fix up type of field p
 */
void
fldty(struct symtab *p)
{
}

/*
 * Build target-dependent switch tree/table.
 *
 * Return 1 if successfull, otherwise return 0 and the
 * target-independent tree will be used.
 */
int
mygenswitch(int num, TWORD type, struct swents **p, int n)
{
	return 0;
}


/*
 * Straighten a chain of CM ops so that the CM nodes
 * only appear on the left node.
 *
 *          CM               CM
 *        CM  CM           CM  b
 *       x y  a b        CM  a
 *                      x  y
 */
static NODE *
straighten(NODE *p)
{
	NODE *r = p->n_right;

	if (p->n_op != CM || r->n_op != CM)
		return p;

	p->n_right = r->n_left;
	r->n_left = p;

	return r;
}


/* push arg onto the stack */
/* called by moveargs() */
static NODE *
pusharg(NODE *p, int *regp)
{
        NODE *q;
        int sz;

        /* convert to register size, if smaller */
        sz = tsize(p->n_type, p->n_df, p->n_sue);
        if (sz < SZINT)
                p = block(SCONV, p, NIL, INT, 0, MKSUE(INT));

        q = block(REG, NIL, NIL, INT, 0, MKSUE(INT));
        regno(q) = SP;

	if (szty(p->n_type) == 1) {
		++(*regp);
		q = block(MINUSEQ, q, bcon(4), INT, 0, MKSUE(INT));
	} else {
		(*regp) += 2;
		q = block(MINUSEQ, q, bcon(8), INT, 0, MKSUE(INT));
	}

	q = block(UMUL, q, NIL, p->n_type, p->n_df, p->n_sue);

	return buildtree(ASSIGN, q, p);
}

/* setup call stack with 32-bit argument */
/* called from moveargs() */
static NODE *
movearg_32bit(NODE *p, int *regp)
{
	int reg = *regp;
	NODE *q;

	q = block(REG, NIL, NIL, p->n_type, p->n_df, p->n_sue);
	regno(q) = reg++;
	q = buildtree(ASSIGN, q, p);

	*regp = reg;
	return q;
}

/* setup call stack with 64-bit argument */
/* called from moveargs() */
static NODE *
movearg_64bit(NODE *p, int *regp)
{
        int reg = *regp;
        NODE *q, *r;

#if ALLONGLONG == 64
        /* alignment */
        ++reg;
        reg &= ~1;
	*regp = reg;
#endif

        if (reg > R3) {
                q = pusharg(p, regp);
	} else if (reg == R3) {
		/* half in and half out of the registers */
		r = tcopy(p);
		if (features(FEATURE_BIGENDIAN)) {
			q = buildtree(RS, p, bcon(32));
			q = block(SCONV, q, NIL, INT, 0, MKSUE(INT));
		} else {
			q = block(SCONV, p, NIL, INT, 0, MKSUE(INT));
		}
		q = movearg_32bit(q, regp);
		if (features(FEATURE_BIGENDIAN)) {
			r = block(SCONV, r, NIL, INT, 0, MKSUE(INT));
		} else {
			r = buildtree(RS, r, bcon(32));
			r = block(SCONV, r, NIL, INT, 0, MKSUE(INT));
		}
		r = pusharg(r, regp);
		q = straighten(block(CM, q, r, p->n_type, p->n_df, p->n_sue));
        } else {
                q = block(REG, NIL, NIL, p->n_type, p->n_df, p->n_sue);
                regno(q) = R0R1 + (reg - R0);
                q = buildtree(ASSIGN, q, p);
                *regp = reg + 2;
        }

        return q;
}

/* setup call stack with float/double argument */
/* called from moveargs() */
static NODE *
movearg_float(NODE *p, int *regp)
{
	NODE *r, *l;
	int tmpnr;

        /*
         * Floats are passed in the general registers for
	 * compatibily with libraries compiled to handle soft-float.
         */

        l = tempnode(0, p->n_type, p->n_df, p->n_sue);
	tmpnr = regno(l);
        l = buildtree(ASSIGN, l, p);
        ecomp(l);

	if (p->n_type == FLOAT) {
	        r = tempnode(tmpnr, INT, 0, MKSUE(INT));
		r = movearg_32bit(r, regp);
	} else {
	        r = tempnode(tmpnr, ULONGLONG, 0, MKSUE(ULONGLONG));
		r = movearg_64bit(r, regp);
	}

	return r;
}

static NODE *
moveargs(NODE *p, int *regp)
{
        NODE *r, **rp;
        int reg;

        if (p->n_op == CM) {
                p->n_left = moveargs(p->n_left, regp);
                r = p->n_right;
                rp = &p->n_right;
        } else {
                r = p;
                rp = &p;
        }

        reg = *regp;

        if (reg > R3) {
                *rp = pusharg(r, regp);
        } else if (DEUNSIGN(r->n_type) == LONGLONG) {
                *rp = movearg_64bit(r, regp);
	} else if (r->n_type == DOUBLE || r->n_type == LDOUBLE) {
		*rp = movearg_float(r, regp);
	} else if (r->n_type == FLOAT) {
		*rp = movearg_float(r, regp);
        } else {
                *rp = movearg_32bit(r, regp);
        }

	if ((*rp)->n_op == CM && r != p)
		p = straighten(p);

        return p;
}

/*
 * Called with a function call with arguments as argument.
 * This is done early in buildtree() and only done once.
 */
NODE *
funcode(NODE *p)
{
	int reg = R0;
	int ty;

	ty = DECREF(p->n_left->n_type);
	if (ty == STRTY+FTN || ty == UNIONTY+FTN)
		reg = R1;

	p->n_right = moveargs(p->n_right, &reg);

	return p;
}
