/*      $OpenBSD: match.c,v 1.10 2008/08/17 18:40:13 ragge Exp $   */
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
 * Copyright(C) Caldera International Inc. 2001-2002. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code and documentation must retain the above
 * copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditionsand the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 * 	This product includes software developed or owned by Caldera
 *	International, Inc.
 * Neither the name of Caldera International, Inc. nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OFLIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "pass2.h"

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

void setclass(int tmp, int class);
int getclass(int tmp);

int s2debug = 0;

extern char *ltyp[], *rtyp[];

static char *srtyp[] = { "SRNOPE", "SRDIR", "SROREG", "SRREG" };

/*
 * return true if shape is appropriate for the node p
 * side effect for SFLD is to set up fldsz, etc
 *
 * Return values:
 * SRNOPE  Cannot match this shape.
 * SRDIR   Direct match, may or may not traverse down.
 * SRREG   Will match if put in a regster XXX - kill this?
 */
int
tshape(NODE *p, int shape)
{
	int o, mask;

	o = p->n_op;

#ifdef PCC_DEBUG
	if (s2debug)
		printf("tshape(%p, %s) op = %s\n", p, prcook(shape), opst[o]);
#endif

	if (shape & SPECIAL) {

		switch (shape) {
		case SZERO:
		case SONE:
		case SMONE:
		case SSCON:
		case SCCON:
			if (o != ICON || p->n_name[0])
				return SRNOPE;
			if (p->n_lval == 0 && shape == SZERO)
				return SRDIR;
			if (p->n_lval == 1 && shape == SONE)
				return SRDIR;
			if (p->n_lval == -1 && shape == SMONE)
				return SRDIR;
			if (p->n_lval > -257 && p->n_lval < 256 &&
			    shape == SCCON)
				return SRDIR;
			if (p->n_lval > -32769 && p->n_lval < 32768 &&
			    shape == SSCON)
				return SRDIR;
			return SRNOPE;

		case SSOREG:	/* non-indexed OREG */
			if (o == OREG && !R2TEST(p->n_rval))
				return SRDIR;
			return SRNOPE;

		default:
			return (special(p, shape));
		}
	}

	if (shape & SANY)
		return SRDIR;

	if ((shape&INTEMP) && shtemp(p)) /* XXX remove? */
		return SRDIR;

	if ((shape&SWADD) && (o==NAME||o==OREG))
		if (BYTEOFF(p->n_lval))
			return SRNOPE;

	switch (o) {

	case NAME:
		if (shape & SNAME)
			return SRDIR;
		break;

	case ICON:
	case FCON:
		if (shape & SCON)
			return SRDIR;
		break;

	case FLD:
		if (shape & SFLD)
			return flshape(p->n_left);
		break;

	case CCODES:
		if (shape & SCC)
			return SRDIR;
		break;

	case REG:
	case TEMP:
		mask = PCLASS(p);
		if (shape & mask)
			return SRDIR;
		break;

	case OREG:
		if (shape & SOREG)
			return SRDIR;
		break;

	case UMUL:
		if (shumul(p->n_left) & shape)
			return SROREG;	/* Calls offstar to traverse down */
		break;

	}
	return SRNOPE;
}

/*
 * does the type t match tword
 */
int
ttype(TWORD t, int tword)
{
	if (tword & TANY)
		return(1);

#ifdef PCC_DEBUG
	if (t2debug)
		printf("ttype(%o, %o)\n", t, tword);
#endif
	if (ISPTR(t) && ISFTN(DECREF(t)) && (tword & TFTN)) {
		/* For funny function pointers */
		return 1;
	}
	if (ISPTR(t) && (tword&TPTRTO)) {
		do {
			t = DECREF(t);
		} while (ISARY(t));
			/* arrays that are left are usually only
			 * in structure references...
			 */
		return (ttype(t, tword&(~TPTRTO)));
	}
	if (t != BTYPE(t))
		return (tword & TPOINT); /* TPOINT means not simple! */
	if (tword & TPTRTO)
		return(0);

	switch (t) {
	case CHAR:
		return( tword & TCHAR );
	case SHORT:
		return( tword & TSHORT );
	case STRTY:
	case UNIONTY:
		return( tword & TSTRUCT );
	case INT:
		return( tword & TINT );
	case UNSIGNED:
		return( tword & TUNSIGNED );
	case USHORT:
		return( tword & TUSHORT );
	case UCHAR:
		return( tword & TUCHAR );
	case ULONG:
		return( tword & TULONG );
	case LONG:
		return( tword & TLONG );
	case LONGLONG:
		return( tword & TLONGLONG );
	case ULONGLONG:
		return( tword & TULONGLONG );
	case FLOAT:
		return( tword & TFLOAT );
	case DOUBLE:
		return( tword & TDOUBLE );
	case LDOUBLE:
		return( tword & TLDOUBLE );
	}

	return(0);
}

#define	FLDSZ(x)	UPKFSZ(x)
#ifdef RTOLBYTES
#define	FLDSHF(x)	UPKFOFF(x)
#else
#define	FLDSHF(x)	(SZINT - FLDSZ(x) - UPKFOFF(x))
#endif

/*
 * generate code by interpreting table entry
 */
void
expand(NODE *p, int cookie, char *cp)
{
	CONSZ val;

	for( ; *cp; ++cp ){
		switch( *cp ){

		default:
			PUTCHAR( *cp );
			continue;  /* this is the usual case... */

		case 'Z':  /* special machine dependent operations */
			zzzcode( p, *++cp );
			continue;

		case 'F':  /* this line deleted if FOREFF is active */
			if (cookie & FOREFF) {
				while (*cp && *cp != '\n')
					cp++;
				if (*cp == 0)
					return;
			}
			continue;

		case 'S':  /* field size */
			if (fldexpand(p, cookie, &cp))
				continue;
			printf("%d", FLDSZ(p->n_rval));
			continue;

		case 'H':  /* field shift */
			if (fldexpand(p, cookie, &cp))
				continue;
			printf("%d", FLDSHF(p->n_rval));
			continue;

		case 'M':  /* field mask */
		case 'N':  /* complement of field mask */
			if (fldexpand(p, cookie, &cp))
				continue;
			val = 1;
			val <<= FLDSZ(p->n_rval);
			--val;
			val <<= FLDSHF(p->n_rval);
			adrcon( *cp=='M' ? val : ~val );
			continue;

		case 'L':  /* output special label field */
			if (*++cp == 'C')
				printf(LABFMT, p->n_label);
			else
				printf(LABFMT, (int)getlr(p,*cp)->n_lval);
			continue;

		case 'O':  /* opcode string */
			hopcode( *++cp, p->n_op );
			continue;

		case 'B':  /* byte offset in word */
			val = getlr(p,*++cp)->n_lval;
			val = BYTEOFF(val);
			printf( CONFMT, val );
			continue;

		case 'C': /* for constant value only */
			conput(stdout, getlr( p, *++cp ) );
			continue;

		case 'I': /* in instruction */
			insput( getlr( p, *++cp ) );
			continue;

		case 'A': /* address of */
			adrput(stdout, getlr( p, *++cp ) );
			continue;

		case 'U': /* for upper half of address, only */
			upput(getlr(p, *++cp), SZLONG);
			continue;

			}

		}

	}

NODE resc[4];

NODE *
getlr(NODE *p, int c)
{
	NODE *q;

	/* return the pointer to the left or right side of p, or p itself,
	   depending on the optype of p */

	switch (c) {

	case '1':
	case '2':
	case '3':
	case 'D':
		if (c == 'D')
			c = 0;
		else
			c -= '0';
		q = &resc[c];
		q->n_op = REG;
		q->n_type = p->n_type; /* XXX should be correct type */
		q->n_rval = DECRA(p->n_reg, c);
		q->n_su = p->n_su;
		return q;

	case 'L':
		return( optype( p->n_op ) == LTYPE ? p : p->n_left );

	case 'R':
		return( optype( p->n_op ) != BITYPE ? p : p->n_right );

	}
	cerror( "bad getlr: %c", c );
	/* NOTREACHED */
	return NULL;
}

#ifdef PCC_DEBUG
#define	F2DEBUG(x)	if (f2debug) printf x
#define	F2WALK(x)	if (f2debug) fwalk(x, e2print, 0)
#else
#define	F2DEBUG(x)
#define	F2WALK(x)
#endif

/*
 * Convert a node to REG or OREG.
 * Shape is register class where we want the result.
 * Returns register class if register nodes.
 * If w is: (should be shapes)
 *	- LREG - result in register, call geninsn().
 *	- LOREG - create OREG; call offstar().
 *	- 0 - clear su, walk down.
 */
static int
swmatch(NODE *p, int shape, int w)
{
	int rv = 0;

	F2DEBUG(("swmatch: p=%p, shape=%s, w=%s\n", p, prcook(shape), srtyp[w]));

	switch (w) {
	case LREG:
		rv = geninsn(p, shape);
		break;

	case LOREG:
		/* should be here only if op == UMUL */
		if (p->n_op != UMUL && p->n_op != FLD)
			comperr("swmatch %p", p);
		if (p->n_op == FLD) {
			offstar(p->n_left->n_left, shape);
			p->n_left->n_su = 0;
		} else
			offstar(p->n_left, shape);
		p->n_su = 0;
		rv = ffs(shape)-1;
		break;

	case 0:
		if (optype(p->n_op) == BITYPE)
			swmatch(p->n_right, 0, 0);
		if (optype(p->n_op) != LTYPE)
			swmatch(p->n_left, 0, 0);
		p->n_su = 0;
	}
	return rv;

}

/*
 * Help routines for find*() functions.
 * If the node will be a REG node and it will be rewritten in the
 * instruction, ask for it to be put in a register.
 */
static int
chcheck(NODE *p, int shape, int rew)
{
	int sh, sha;

	sha = shape;
	if (shape & SPECIAL)
		shape = 0;

	switch ((sh = tshape(p, sha))) {
	case SRNOPE:
		if (shape & INREGS)
			sh = SRREG;
		break;

	case SROREG:
	case SRDIR:
		if (rew == 0)
			break;
		if (shape & INREGS)
			sh = SRREG;
		else
			sh = SRNOPE;
		break;
	}
	return sh;
}

/*
 * Check how to walk further down.
 * Merge with swmatch()?
 * 	sh - shape for return value (register class).
 *	p - node (for this leg)
 *	shape - given shape for this leg
 *	cookie - cookie given for parent node
 *	rew - 
 *	go - switch key for traversing down
 *	returns register class.
 */
static int
shswitch(int sh, NODE *p, int shape, int cookie, int rew, int go)
{
	int lsh;

	F2DEBUG(("shswitch: p=%p, shape=%s, cookie=%s, rew=0x%x, go=%s\n", p, prcook(shape), prcook(cookie), rew, srtyp[go]));

	switch (go) {
	case SRDIR: /* direct match, just clear su */
		(void)swmatch(p, 0, 0);
		break;

	case SROREG: /* call offstar to prepare for OREG conversion */
		(void)swmatch(p, shape, LOREG);
		break;

	case SRREG: /* call geninsn() to get value into register */
		lsh = shape & (FORCC | INREGS);
		if (rew && cookie != FOREFF)
			lsh &= (cookie & (FORCC | INREGS));
		lsh = swmatch(p, lsh, LREG);
		if (rew)
			sh = lsh;
		break;
	}
	return sh;
}

/*
 * Find the best instruction to evaluate the given tree.
 * Best is to match both subnodes directly, second-best is if
 * subnodes must be evaluated into OREGs, thereafter if nodes 
 * must be put into registers.
 * Whether 2-op instructions or 3-op is preferred is depending on in
 * which order they are found in the table.
 * mtchno is set to the count of regs needed for its legs.
 */
int
findops(NODE *p, int cookie)
{
	extern int *qtable[];
	struct optab *q, *qq = NULL;
	int i, shl, shr, *ixp, sh;
	int lvl = 10, idx = 0, gol = 0, gor = 0;
	NODE *l, *r;

	F2DEBUG(("findops node %p (%s)\n", p, prcook(cookie)));
	F2WALK(p);

	ixp = qtable[p->n_op];
	l = getlr(p, 'L');
	r = getlr(p, 'R');
	for (i = 0; ixp[i] >= 0; i++) {
		q = &table[ixp[i]];

		F2DEBUG(("findop: ixp %d\n", ixp[i]));
		if (!acceptable(q))		/* target-dependent filter */
			continue;

		if (ttype(l->n_type, q->ltype) == 0 ||
		    ttype(r->n_type, q->rtype) == 0)
			continue; /* Types must be correct */

		if ((cookie & q->visit) == 0)
			continue; /* must get a result */

		F2DEBUG(("findop got types\n"));

		if ((shl = chcheck(l, q->lshape, q->rewrite & RLEFT)) == SRNOPE)
			continue;

		F2DEBUG(("findop lshape %d\n", shl));
		F2WALK(l);

		if ((shr = chcheck(r, q->rshape, q->rewrite & RRIGHT)) == SRNOPE)
			continue;

		F2DEBUG(("findop rshape %d\n", shr));
		F2WALK(r);

		if (q->needs & REWRITE)
			break;  /* Done here */

		if (lvl <= (shl + shr))
			continue;
		lvl = shl + shr;
		qq = q;
		idx = ixp[i];
		gol = shl;
		gor = shr;
	}
	if (lvl == 10) {
		F2DEBUG(("findops failed\n"));
		if (setbin(p))
			return FRETRY;
		return FFAIL;
	}

	F2DEBUG(("findops entry %d(%s,%s)\n", idx, srtyp[gol], srtyp[gor]));

	sh = -1;

	sh = shswitch(sh, p->n_left, qq->lshape, cookie,
	    qq->rewrite & RLEFT, gol);
	sh = shswitch(sh, p->n_right, qq->rshape, cookie,
	    qq->rewrite & RRIGHT, gor);

	if (sh == -1) {
		if (cookie == FOREFF || cookie == FORCC)
			sh = 0;
		else
			sh = ffs(cookie & qq->visit & INREGS)-1;
	}
	F2DEBUG(("findops: node %p (%s)\n", p, prcook(1 << sh)));
	p->n_su = MKIDX(idx, 0);
	SCLASS(p->n_su, sh);
	return sh;
}

/*
 * Find the best relation op for matching the two trees it has.
 * This is a sub-version of the function findops() above.
 * The instruction with the lowest grading is emitted.
 *
 * Level assignment for priority:
 *	left	right	prio
 *	-	-	-
 *	direct	direct	1
 *	direct	OREG	2	# make oreg
 *	OREG	direct	2	# make oreg
 *	OREG	OREG	2	# make both oreg
 *	direct	REG	3	# put in reg
 *	OREG	REG	3	# put in reg, make oreg
 *	REG	direct	3	# put in reg
 *	REG	OREG	3	# put in reg, make oreg
 *	REG	REG	4	# put both in reg
 */
int
relops(NODE *p)
{
	extern int *qtable[];
	struct optab *q;
	int i, shl = 0, shr = 0;
	NODE *l, *r;
	int *ixp, idx = 0;
	int lvl = 10, gol = 0, gor = 0;

	F2DEBUG(("relops tree:\n"));
	F2WALK(p);

	l = getlr(p, 'L');
	r = getlr(p, 'R');
	ixp = qtable[p->n_op];
	for (i = 0; ixp[i] >= 0; i++) {
		q = &table[ixp[i]];

		F2DEBUG(("relops: ixp %d\n", ixp[i]));
		if (!acceptable(q))		/* target-dependent filter */
			continue;

		if (ttype(l->n_type, q->ltype) == 0 ||
		    ttype(r->n_type, q->rtype) == 0)
			continue; /* Types must be correct */

		F2DEBUG(("relops got types\n"));
		if ((shl = chcheck(l, q->lshape, 0)) == SRNOPE)
			continue;
		F2DEBUG(("relops lshape %d\n", shl));
		F2WALK(p);
		if ((shr = chcheck(r, q->rshape, 0)) == SRNOPE)
			continue;
		F2DEBUG(("relops rshape %d\n", shr));
		F2WALK(p);
		if (q->needs & REWRITE)
			break;	/* Done here */

		if (lvl <= (shl + shr))
			continue;
		lvl = shl + shr;
		idx = ixp[i];
		gol = shl;
		gor = shr;
	}
	if (lvl == 10) {
		F2DEBUG(("relops failed\n"));
		if (setbin(p))
			return FRETRY;
		return FFAIL;
	}
	F2DEBUG(("relops entry %d(%s %s)\n", idx, srtyp[gol], srtyp[gor]));

	q = &table[idx];

	(void)shswitch(-1, p->n_left, q->lshape, FORCC,
	    q->rewrite & RLEFT, gol);

	(void)shswitch(-1, p->n_right, q->rshape, FORCC,
	    q->rewrite & RRIGHT, gor);
	
	F2DEBUG(("relops: node %p\n", p));
	p->n_su = MKIDX(idx, 0);
	SCLASS(p->n_su, CLASSA); /* XXX */
	return 0;
}

/*
 * Find a matching assign op.
 *
 * Level assignment for priority:
 *	left	right	prio
 *	-	-	-
 *	direct	direct	1
 *	direct	REG	2
 *	direct	OREG	3
 *	OREG	direct	4
 *	OREG	REG	5
 *	OREG	OREG	6
 */
int
findasg(NODE *p, int cookie)
{
	extern int *qtable[];
	struct optab *q;
	int i, sh, shl, shr, lvl = 10;
	NODE *l, *r;
	int *ixp;
	struct optab *qq = NULL; /* XXX gcc */
	int idx = 0, gol = 0, gor = 0;

	shl = shr = 0;

	F2DEBUG(("findasg tree: %s\n", prcook(cookie)));
	F2WALK(p);

	ixp = qtable[p->n_op];
	l = getlr(p, 'L');
	r = getlr(p, 'R');
	for (i = 0; ixp[i] >= 0; i++) {
		q = &table[ixp[i]];

		F2DEBUG(("findasg: ixp %d\n", ixp[i]));
		if (!acceptable(q))		/* target-dependent filter */
			continue;

		if (ttype(l->n_type, q->ltype) == 0 ||
		    ttype(r->n_type, q->rtype) == 0)
			continue; /* Types must be correct */

		if ((cookie & q->visit) == 0)
			continue; /* must get a result */

		F2DEBUG(("findasg got types\n"));
		if ((shl = tshape(l, q->lshape)) == SRNOPE)
			continue;

		if (shl == SRREG)
			continue;

		F2DEBUG(("findasg lshape %d\n", shl));
		F2WALK(l);

		if ((shr = chcheck(r, q->rshape, q->rewrite & RRIGHT)) == SRNOPE)
			continue;

		F2DEBUG(("findasg rshape %d\n", shr));
		F2WALK(r);
		if (q->needs & REWRITE)
			break;	/* Done here */

		if (lvl <= (shl + shr))
			continue;

		lvl = shl + shr;
		qq = q;
		idx = ixp[i];
		gol = shl;
		gor = shr;
	}

	if (lvl == 10) {
		F2DEBUG(("findasg failed\n"));
		if (setasg(p, cookie))
			return FRETRY;
		return FFAIL;
	}
	F2DEBUG(("findasg entry %d(%s,%s)\n", idx, srtyp[gol], srtyp[gor]));

	sh = -1;
	sh = shswitch(sh, p->n_left, qq->lshape, cookie,
	    qq->rewrite & RLEFT, gol);

	sh = shswitch(sh, p->n_right, qq->rshape, cookie,
	    qq->rewrite & RRIGHT, gor);

	if (sh == -1) {
		if (cookie == FOREFF)
			sh = 0;
		else
			sh = ffs(cookie & qq->visit & INREGS)-1;
	}
	F2DEBUG(("findasg: node %p class %d\n", p, sh));

	p->n_su = MKIDX(idx, 0);
	SCLASS(p->n_su, sh);

	return sh;
}

/*
 * Search for an UMUL table entry that can turn an indirect node into
 * a move from an OREG.
 */
int
findumul(NODE *p, int cookie)
{
	extern int *qtable[];
	struct optab *q = NULL; /* XXX gcc */
	int i, shl = 0, shr = 0, sh;
	int *ixp;

	F2DEBUG(("findumul p %p (%s)\n", p, prcook(cookie)));
	F2WALK(p);

	ixp = qtable[p->n_op];
	for (i = 0; ixp[i] >= 0; i++) {
		q = &table[ixp[i]];

		F2DEBUG(("findumul: ixp %d\n", ixp[i]));
		if (!acceptable(q))		/* target-dependent filter */
			continue;

		if ((q->visit & cookie) == 0)
			continue; /* wrong registers */

		if (ttype(p->n_type, q->rtype) == 0)
			continue; /* Types must be correct */
		

		F2DEBUG(("findumul got types, rshape %s\n", prcook(q->rshape)));
		/*
		 * Try to create an OREG of the node.
		 * Fake left even though it's right node,
		 * to be sure of conversion if going down left.
		 */
		if ((shl = chcheck(p, q->rshape, 0)) == SRNOPE)
			continue;
		
		shr = 0;

		if (q->needs & REWRITE)
			break;	/* Done here */

		F2DEBUG(("findumul got shape %s\n", srtyp[shl]));

		break; /* XXX search better matches */
	}
	if (ixp[i] < 0) {
		F2DEBUG(("findumul failed\n"));
		if (setuni(p, cookie))
			return FRETRY;
		return FFAIL;
	}
	F2DEBUG(("findumul entry %d(%s %s)\n", ixp[i], srtyp[shl], srtyp[shr]));

	sh = shswitch(-1, p, q->rshape, cookie, q->rewrite & RLEFT, shl);
	if (sh == -1)
		sh = ffs(cookie & q->visit & INREGS)-1;

	F2DEBUG(("findumul: node %p (%s)\n", p, prcook(1 << sh)));
	p->n_su = MKIDX(ixp[i], 0);
	SCLASS(p->n_su, sh);
	return sh;
}

/*
 * Find a leaf type node that puts the value into a register.
 */
int
findleaf(NODE *p, int cookie)
{
	extern int *qtable[];
	struct optab *q = NULL; /* XXX gcc */
	int i, sh;
	int *ixp;

	F2DEBUG(("findleaf p %p (%s)\n", p, prcook(cookie)));
	F2WALK(p);

	ixp = qtable[p->n_op];
	for (i = 0; ixp[i] >= 0; i++) {
		q = &table[ixp[i]];

		F2DEBUG(("findleaf: ixp %d\n", ixp[i]));
		if (!acceptable(q))		/* target-dependent filter */
			continue;
		if ((q->visit & cookie) == 0)
			continue; /* wrong registers */

		if (ttype(p->n_type, q->rtype) == 0 ||
		    ttype(p->n_type, q->ltype) == 0)
			continue; /* Types must be correct */

		F2DEBUG(("findleaf got types, rshape %s\n", prcook(q->rshape)));

		if (chcheck(p, q->rshape, 0) != SRDIR)
			continue;

		if (q->needs & REWRITE)
			break;	/* Done here */

		break;
	}
	if (ixp[i] < 0) {
		F2DEBUG(("findleaf failed\n"));
		if (setuni(p, cookie))
			return FRETRY;
		return FFAIL;
	}
	F2DEBUG(("findleaf entry %d\n", ixp[i]));

	sh = ffs(cookie & q->visit & INREGS)-1;
	F2DEBUG(("findleaf: node %p (%s)\n", p, prcook(1 << sh)));
	p->n_su = MKIDX(ixp[i], 0);
	SCLASS(p->n_su, sh);
	return sh;
}

/*
 * Find a UNARY op that satisfy the needs.
 * For now, the destination is always a register.
 * Both source and dest types must match, but only source (left)
 * shape is of interest.
 */
int
finduni(NODE *p, int cookie)
{
	extern int *qtable[];
	struct optab *q;
	NODE *l, *r;
	int i, shl = 0, num = 4;
	int *ixp, idx = 0;
	int sh;

	F2DEBUG(("finduni tree: %s\n", prcook(cookie)));
	F2WALK(p);

	l = getlr(p, 'L');
	if (p->n_op == CALL || p->n_op == FORTCALL || p->n_op == STCALL)
		r = p;
	else
		r = getlr(p, 'R');
	ixp = qtable[p->n_op];
	for (i = 0; ixp[i] >= 0; i++) {
		q = &table[ixp[i]];

		F2DEBUG(("finduni: ixp %d\n", ixp[i]));
		if (!acceptable(q))		/* target-dependent filter */
			continue;

		if (ttype(l->n_type, q->ltype) == 0)
			continue; /* Type must be correct */

		F2DEBUG(("finduni got left type\n"));
		if (ttype(r->n_type, q->rtype) == 0)
			continue; /* Type must be correct */

		F2DEBUG(("finduni got types\n"));
		if ((shl = chcheck(l, q->lshape, q->rewrite & RLEFT)) == SRNOPE)
			continue;

		F2DEBUG(("finduni got shapes %d\n", shl));

		if ((cookie & q->visit) == 0)	/* check correct return value */
			continue;		/* XXX - should check needs */

		/* avoid clobbering of longlived regs */
		/* let register allocator coalesce */
		if ((q->rewrite & RLEFT) && (shl == SRDIR) /* && isreg(l) */)
			shl = SRREG;

		F2DEBUG(("finduni got cookie\n"));
		if (q->needs & REWRITE)
			break;	/* Done here */

		if (shl >= num)
			continue;
		num = shl;
		idx = ixp[i];

		if (shl == SRDIR)
			break;
	}

	if (num == 4) {
		F2DEBUG(("finduni failed\n"));
	} else
		F2DEBUG(("finduni entry %d(%s)\n", idx, srtyp[num]));

	if (num == 4) {
		if (setuni(p, cookie))
			return FRETRY;
		return FFAIL;
	}
	q = &table[idx];

	sh = shswitch(-1, p->n_left, q->lshape, cookie,
	    q->rewrite & RLEFT, num);
	if (sh == -1)
		sh = ffs(cookie & q->visit & INREGS)-1;
	if (sh == -1)
		sh = 0;

	F2DEBUG(("finduni: node %p (%s)\n", p, prcook(1 << sh)));
	p->n_su = MKIDX(idx, 0);
	SCLASS(p->n_su, sh);
	return sh;
}
