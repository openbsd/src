/*	$OpenBSD: regs.c,v 1.17 2008/08/17 18:40:13 ragge Exp $	*/
/*
 * Copyright (c) 2005 Anders Magnusson (ragge@ludd.luth.se).
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

#include "pass2.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define	MAXLOOP	20 /* Max number of allocation loops XXX 3 should be enough */

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
 
/*
 * New-style register allocator using graph coloring.
 * The design is based on the George and Appel paper
 * "Iterated Register Coalescing", ACM Transactions, No 3, May 1996.
 */

#define	BIT2BYTE(bits) ((((bits)+NUMBITS-1)/NUMBITS)*(NUMBITS/8))
#define	BITALLOC(ptr,all,sz) { \
	int sz__s = BIT2BYTE(sz); ptr = all(sz__s); memset(ptr, 0, sz__s); }

#undef COMPERR_PERM_MOVE
#define	RDEBUG(x)	if (rdebug) printf x
#define	RRDEBUG(x)	if (rdebug > 1) printf x
#define	RPRINTIP(x)	if (rdebug) printip(x)
#define	RDX(x)		x
#define UDEBUG(x)	if (udebug) printf x

/*
 * Data structure overview for this implementation of graph coloring:
 *
 * Each temporary (called "node") is described by the type REGW.  
 * Space for all nodes is allocated initially as an array, so 
 * the nodes can be can be referenced both by the node number and
 * by pointer.
 * 
 * All moves are represented by the type REGM, allocated when needed. 
 *
 * The "live" set used during graph building is represented by a bitset.
 *
 * Interference edges are represented by struct AdjSet, hashed and linked
 * from index into the edgehash array.
 *
 * A mapping from each node to the moves it is assiciated with is 
 * maintained by an array moveList which for each node number has a linked
 * list of MOVL types, each pointing to a REGM.
 *
 * Adjacency list is maintained by the adjList array, indexed by the
 * node number. Each adjList entry points to an ADJL type, and is a
 * single-linked list for all adjacent nodes.
 *
 * degree, alias and color are integer arrays indexed by node number.
 */

/*
 * linked list of adjacent nodes.
 */
typedef struct regw3 {
	struct regw3 *r_next;
	struct regw *a_temp;
} ADJL;

/*
 * Structure describing a move.
 */
typedef struct regm {
	DLIST_ENTRY(regm) link;
	struct regw *src, *dst;
	int queue;
} REGM;

typedef struct movlink {
	struct movlink *next;
	REGM *regm;
} MOVL;

/*
 * Structure describing a temporary.
 */
typedef struct regw {
	DLIST_ENTRY(regw) link;
	ADJL *r_adjList;	/* linked list of adjacent nodes */
	int r_class;		/* this nodes class */
	int r_nclass[NUMCLASS+1];	/* count of adjacent classes */
	struct regw *r_alias;		/* aliased temporary */
	int r_color;		/* final node color */
	struct regw *r_onlist;	/* which work list this node belongs to */
	MOVL *r_moveList;	/* moves associated with this node */
#ifdef PCC_DEBUG
	int nodnum;		/* Debug number */
#endif
} REGW;

/*
 * Worklists, a node is always on exactly one of these lists.
 */
static REGW precolored, simplifyWorklist, freezeWorklist, spillWorklist,
	spilledNodes, coalescedNodes, coloredNodes, selectStack;
static REGW initial, *nblock;
static void insnwalk(NODE *p);
#ifdef PCC_DEBUG
int use_regw;
int nodnum = 100;
#define	SETNUM(x)	(x)->nodnum = nodnum++
#define	ASGNUM(x)	(x)->nodnum
#else
#define SETNUM(x)
#define ASGNUM(x)
#endif

#define	ALLNEEDS (NACOUNT|NBCOUNT|NCCOUNT|NDCOUNT)

/* XXX */
REGW *ablock;

static int tempmin, tempmax, basetemp, xbits;
/*
 * nsavregs is an array that matches the permregs array.
 * Each entry in the array may have the values:
 * 0	: register coalesced, just ignore.
 * 1	: save register on stack
 * If the entry is 0 but the resulting color differs from the 
 * corresponding permregs index, add moves.
 * XXX - should be a bitfield!
 */
static int *nsavregs, *ndontregs;

/*
 * Return the REGW struct for a temporary.
 * If first time touched, enter into list for existing vars.
 * Only called from sucomp().
 */
static REGW *
newblock(NODE *p)
{
	REGW *nb = &nblock[regno(p)];
	if (nb->link.q_forw == 0) {
		DLIST_INSERT_AFTER(&initial, nb, link);
#ifdef PCC_DEBUG
		ASGNUM(nb) = regno(p);
		RDEBUG(("Adding longtime %d for tmp %d\n",
		    nb->nodnum, regno(p)));
#endif
	}
	if (nb->r_class == 0)
		nb->r_class = gclass(p->n_type);
#ifdef PCC_DEBUG
	RDEBUG(("newblock: p %p, node %d class %d\n",
	    p, nb->nodnum, nb->r_class));
#endif
	return nb;
}

/*
 * Count the number of registers needed to evaluate a tree.
 * This is only done to find the evaluation order of the tree.
 * While here, assign temp numbers to the registers that will
 * be needed when the tree is evaluated.
 *
 * While traversing the tree, assign REGW nodes to the registers
 * used by all instructions:
 *	- n_regw[0] is always set to the outgoing node. If the
 *	  instruction is 2-op (addl r0,r1) then an implicit move
 *	  is inserted just before the left (clobbered) operand.
 *	- if the instruction has needs then REGW nodes are
 *	  allocated as n_regw[1] etc.
 */
int
nsucomp(NODE *p)
{
	struct optab *q;
	int left, right;
	int nreg, need, i, nxreg, o;
	int nareg, nbreg, ncreg, ndreg;
	REGW *w;

	o = optype(p->n_op);

	UDEBUG(("entering nsucomp, node %p\n", p));

	if (TBLIDX(p->n_su) == 0) {
		int a = 0, b;

		p->n_regw = NULL;
		if (o == LTYPE ) {
			if (p->n_op == TEMP)
				p->n_regw = newblock(p);
			else if (p->n_op == REG)
				p->n_regw = &ablock[regno(p)];
		} else
			a = nsucomp(p->n_left);
		if (o == BITYPE) {
			b = nsucomp(p->n_right);
			if (b > a)
				p->n_su |= DORIGHT;
			a = MAX(a, b);
		}
		return a;
	}

	q = &table[TBLIDX(p->n_su)];
	nareg = (q->needs & NACOUNT);

	for (i = (q->needs & NBCOUNT), nbreg = 0; i; i -= NBREG)
		nbreg++;
	for (i = (q->needs & NCCOUNT), ncreg = 0; i; i -= NCREG)
		ncreg++;
	for (i = (q->needs & NDCOUNT), ndreg = 0; i; i -= NDREG)
		ndreg++;

	nxreg = nareg + nbreg + ncreg + ndreg;
	nreg = nxreg;
	if (callop(p->n_op))
		nreg = MAX(fregs, nreg);

	if (o == BITYPE) {
		right = nsucomp(p->n_right);
	} else
		right = 0;

	if (o != LTYPE)
		left = nsucomp(p->n_left);
	else
		left = 0;

	UDEBUG(("node %p left %d right %d\n", p, left, right));

	if (o == BITYPE) {
		/* Two children */
		if (right == left) {
			need = left + MAX(nreg, 1);
		} else {
			need = MAX(right, left);
			need = MAX(need, nreg);
		}
		if (setorder(p) == 0) {
			/* XXX - should take care of overlapping needs */
			if (right > left) {
				p->n_su |= DORIGHT;
			} else if (right == left) {
				/* A favor to 2-operand architectures */
				if ((q->rewrite & RRIGHT) == 0)
					p->n_su |= DORIGHT;
			}
		}
	} else if (o != LTYPE) {
		/* One child */
		need = MAX(right, left) + nreg;
	} else
		need = nreg;

	if (p->n_op == TEMP)
		(void)newblock(p);

	if (TCLASS(p->n_su) == 0 && nxreg == 0) {
		UDEBUG(("node %p no class\n", p));
		p->n_regw = NULL; /* may be set earlier */
		return need;
	}

#ifdef PCC_DEBUG
#define	ADCL(n, cl)	\
	for (i = 0; i < n; i++, w++) {	w->r_class = cl; \
		DLIST_INSERT_BEFORE(&initial, w, link);  SETNUM(w); \
		UDEBUG(("Adding " #n " %d\n", w->nodnum)); \
	}
#else
#define	ADCL(n, cl)	\
	for (i = 0; i < n; i++, w++) {	w->r_class = cl; \
		DLIST_INSERT_BEFORE(&initial, w, link);  SETNUM(w); \
	}
#endif

	UDEBUG(("node %p numregs %d\n", p, nxreg+1));
	w = p->n_regw = tmpalloc(sizeof(REGW) * (nxreg+1));
	memset(w, 0, sizeof(REGW) * (nxreg+1));

	w->r_class = TCLASS(p->n_su);
	if (w->r_class == 0)
		w->r_class = gclass(p->n_type);
	w->r_nclass[0] = o == LTYPE; /* XXX store leaf info here */
	SETNUM(w);
	if (w->r_class)
		DLIST_INSERT_BEFORE(&initial, w, link);
#ifdef PCC_DEBUG
	UDEBUG(("Adding short %d class %d\n", w->nodnum, w->r_class));
#endif
	w++;
	ADCL(nareg, CLASSA);
	ADCL(nbreg, CLASSB);
	ADCL(ncreg, CLASSC);
	ADCL(ndreg, CLASSD);

	if (q->rewrite & RESC1) {
		w = p->n_regw + 1;
		w->r_class = -1;
		DLIST_REMOVE(w,link);
	} else if (q->rewrite & RESC2) {
		w = p->n_regw + 2;
		w->r_class = -1;
		DLIST_REMOVE(w,link);
	} else if (q->rewrite & RESC3) {
		w = p->n_regw + 3;
		w->r_class = -1;
		DLIST_REMOVE(w,link);
	}

	UDEBUG(("node %p return regs %d\n", p, need));

	return need;
}

#define	CLASS(x)	(x)->r_class
#define	NCLASS(x,c)	(x)->r_nclass[c]
#define	ADJLIST(x)	(x)->r_adjList
#define	ALIAS(x)	(x)->r_alias
#define	ONLIST(x)	(x)->r_onlist
#define	MOVELIST(x)	(x)->r_moveList
#define	COLOR(x)	(x)->r_color

static bittype *live;

#define	PUSHWLIST(w, l)	DLIST_INSERT_AFTER(&l, w, link); w->r_onlist = &l
#define	POPWLIST(l)	popwlist(&l);
#define	DELWLIST(w)	DLIST_REMOVE(w, link)
#define WLISTEMPTY(h)	DLIST_ISEMPTY(&h,link)
#define	PUSHMLIST(w, l, q)	DLIST_INSERT_AFTER(&l, w, link); w->queue = q
#define	POPMLIST(l)	popmlist(&l);

#define	trivially_colorable(x) \
	trivially_colorable_p((x)->r_class, (x)->r_nclass)
/*
 * Determine if a node is trivially colorable ("degree < K").
 * This implementation is a dumb one, without considering speed.
 */
static int
trivially_colorable_p(int c, int *n)
{
	int r[NUMCLASS+1];
	int i;

	for (i = 1; i < NUMCLASS+1; i++)
		r[i] = n[i] < regK[i] ? n[i] : regK[i];

#if 0
	/* add the exclusion nodes. */
	/* XXX can we do someything smart here? */
	/* worst-case for exclusion nodes are better than the `worst-case' */
	for (; excl; excl >>= 1)
		if (excl & 1)
			r[c]++;
#endif

	i = COLORMAP(c, r);
	if (i < 0 || i > 1)
		comperr("trivially_colorable_p");
#ifdef PCC_DEBUG
	if (rdebug > 1)
		printf("trivially_colorable_p: n[1] %d n[2] %d n[3] %d n[4] "
		    "%d for class %d, triv %d\n", n[1], n[2], n[3], n[4], c, i);
#endif
	return i;
}

static int
ncnt(int needs)
{
	int i = 0;

	while (needs & NACOUNT)
		needs -= NAREG, i++;
	while (needs & NBCOUNT)
		needs -= NBREG, i++;
	while (needs & NCCOUNT)
		needs -= NCREG, i++;
	while (needs & NDCOUNT)
		needs -= NDREG, i++;
	return i;
}

static REGW *
popwlist(REGW *l)
{
	REGW *w = DLIST_NEXT(l, link);

	DLIST_REMOVE(w, link);
	w->r_onlist = NULL;
	return w;
}

/*
 * Move lists, a move node is always on only one list.
 */
static REGM coalescedMoves, constrainedMoves, frozenMoves, 
	worklistMoves, activeMoves;
enum { COAL, CONSTR, FROZEN, WLIST, ACTIVE };

static REGM *
popmlist(REGM *l)
{
	REGM *w = DLIST_NEXT(l, link);

	DLIST_REMOVE(w, link);
	return w;
}

/*
 * About data structures used in liveness analysis:
 *
 * The temporaries generated in pass1 are numbered between tempmin and
 * tempmax.  Temporaries generated in pass2 are numbered above tempmax,
 * so they are sequentially numbered.
 *
 * Bitfields are used for liveness.  Bit arrays are allocated on the
 * heap for the "live" variable and on the stack for the in, out, gen
 * and killed variables. Therefore, for a temp number, the bit number must
 * be biased with tempmin.
 *
 * There may be an idea to use a different data structure to store 
 * pass2 allocated temporaries, because they are very sparse.
 */

#ifdef PCC_DEBUG
static void
LIVEADD(int x)
{
	RDEBUG(("Liveadd: %d\n", x));
	if (x >= MAXREGS && (x < tempmin || x >= tempmax))
		comperr("LIVEADD: out of range");
	if (x < MAXREGS) {
		BITSET(live, x);
	} else
		BITSET(live, (x-tempmin+MAXREGS));
}

static void
LIVEDEL(int x)
{
	RDEBUG(("Livedel: %d\n", x));

	if (x >= MAXREGS && (x < tempmin || x >= tempmax))
		comperr("LIVEDEL: out of range");
	if (x < MAXREGS) {
		BITCLEAR(live, x);
	} else
		BITCLEAR(live, (x-tempmin+MAXREGS));
}
#else
#define LIVEADD(x) \
	(x >= MAXREGS ? BITSET(live, (x-tempmin+MAXREGS)) : BITSET(live, x))
#define LIVEDEL(x) \
	(x >= MAXREGS ? BITCLEAR(live, (x-tempmin+MAXREGS)) : BITCLEAR(live, x))
#endif

static struct lives {
	DLIST_ENTRY(lives) link;
	REGW *var;
} lused, lunused;

static void
LIVEADDR(REGW *x)
{
	struct lives *l;

#ifdef PCC_DEBUG
	RDEBUG(("LIVEADDR: %d\n", x->nodnum));
	DLIST_FOREACH(l, &lused, link)
		if (l->var == x)
			return;
//			comperr("LIVEADDR: multiple %d", ASGNUM(x));
#endif
	if (!DLIST_ISEMPTY(&lunused, link)) {
		l = DLIST_NEXT(&lunused, link);
		DLIST_REMOVE(l, link);
	} else
		l = tmpalloc(sizeof(struct lives));

	l->var = x;
	DLIST_INSERT_AFTER(&lused, l, link);
}

static void
LIVEDELR(REGW *x)
{
	struct lives *l;

#ifdef PCC_DEBUG
	RDEBUG(("LIVEDELR: %d\n", x->nodnum));
#endif
	DLIST_FOREACH(l, &lused, link) {
		if (l->var != x)
			continue;
		DLIST_REMOVE(l, link);
		DLIST_INSERT_AFTER(&lunused, l, link);
		return;
	}
//	comperr("LIVEDELR: %p not found", x);
}

#define	MOVELISTADD(t, p) movelistadd(t, p)
#define WORKLISTMOVEADD(s,d) worklistmoveadd(s,d)

static void
movelistadd(REGW *t, REGM *p)
{
	MOVL *w = tmpalloc(sizeof(MOVL));

	w->regm = p;
	w->next = t->r_moveList;
	t->r_moveList = w;
}

static REGM *
worklistmoveadd(REGW *src, REGW *dst)
{
	REGM *w = tmpalloc(sizeof(REGM));

	DLIST_INSERT_AFTER(&worklistMoves, w, link);
	w->src = src;
	w->dst = dst;
	w->queue = WLIST;
	return w;
}

struct AdjSet {
	struct AdjSet *next;
	REGW *u, *v;
} *edgehash[256];

/* Check if a node pair is adjacent */
static int
adjSet(REGW *u, REGW *v)
{
	struct AdjSet *w;
	REGW *t;

	if (ONLIST(u) == &precolored) {
		ADJL *a = ADJLIST(v);
		/*
		 * Check if any of the registers that have edges against v
		 * alias to u.
		 */
		for (; a; a = a->r_next) {
			if (ONLIST(a->a_temp) != &precolored)
				continue;
			t = a->a_temp;
			if (interferes(t - ablock, u - ablock))
				return 1;
		}
	}
	if (u > v)
		t = v, v = u, u = t;
	w = edgehash[((intptr_t)u+(intptr_t)v) & 255];
	for (; w; w = w->next) {
		if (u == w->u && v == w->v)
			return 1;
	}
	return 0;
}

/* Add a pair to adjset.  No check for dups */
static void
adjSetadd(REGW *u, REGW *v)
{
	struct AdjSet *w;
	int x;
	REGW *t;

	if (u > v)
		t = v, v = u, u = t;
	x = ((intptr_t)u+(intptr_t)v) & 255;
	w = tmpalloc(sizeof(struct AdjSet));
	w->u = u, w->v = v;
	w->next = edgehash[x];
	edgehash[x] = w;
}

/*
 * Add an interference edge between two nodes.
 */
static void
AddEdge(REGW *u, REGW *v)
{
	ADJL *x;

#ifdef PCC_DEBUG
	RRDEBUG(("AddEdge: u %d v %d\n", ASGNUM(u), ASGNUM(v)));

#if 0
	if (ASGNUM(u) == 0)
		comperr("AddEdge 0");
#endif
	if (CLASS(u) == 0 || CLASS(v) == 0)
		comperr("AddEdge class == 0 (%d=%d, %d=%d)",
		    CLASS(u), ASGNUM(u), CLASS(v), ASGNUM(v));
#endif

	if (u == v)
		return;
	if (adjSet(u, v))
		return;

	adjSetadd(u, v);

#if 0
	if (ONLIST(u) == &precolored || ONLIST(v) == &precolored)
		comperr("precolored node in AddEdge");
#endif

	if (ONLIST(u) != &precolored) {
		x = tmpalloc(sizeof(ADJL));
		x->a_temp = v;
		x->r_next = u->r_adjList;
		u->r_adjList = x;
		NCLASS(u, CLASS(v))++;
	}

	if (ONLIST(v) != &precolored) {
		x = tmpalloc(sizeof(ADJL));
		x->a_temp = u;
		x->r_next = v->r_adjList;
		v->r_adjList = x;
		NCLASS(v, CLASS(u))++;
	}

#if 0
	RDEBUG(("AddEdge: u %d(d %d) v %d(d %d)\n", u, DEGREE(u), v, DEGREE(v)));
#endif
}

static int
MoveRelated(REGW *n)
{
	MOVL *l;
	REGM *w;

	for (l = MOVELIST(n); l; l = l->next) {
		w = l->regm;
		if (w->queue == ACTIVE || w->queue == WLIST)
			return 1;
	}
	return 0;
}

static void
MkWorklist(void)
{
	REGW *w;

	RDX(int s=0);
	RDX(int f=0);
	RDX(int d=0);

	DLIST_INIT(&precolored, link);
	DLIST_INIT(&simplifyWorklist, link);
	DLIST_INIT(&freezeWorklist, link);
	DLIST_INIT(&spillWorklist, link);
	DLIST_INIT(&spilledNodes, link);
	DLIST_INIT(&coalescedNodes, link);
	DLIST_INIT(&coloredNodes, link);
	DLIST_INIT(&selectStack, link);

	/*
	 * Remove all nodes from the initial list and put them on 
	 * one of the worklists.
	 */
	while (!DLIST_ISEMPTY(&initial, link)) {
		w = DLIST_NEXT(&initial, link);
		DLIST_REMOVE(w, link);
		if (!trivially_colorable(w)) {
			PUSHWLIST(w, spillWorklist);
			RDX(s++);
		} else if (MoveRelated(w)) {
			PUSHWLIST(w, freezeWorklist);
			RDX(f++);
		} else {
			PUSHWLIST(w, simplifyWorklist);
			RDX(d++);
		}
	}
	RDEBUG(("MkWorklist: spill %d freeze %d simplify %d\n", s,f,d));
}

static void
addalledges(REGW *e)
{
	int i, j, k;
	struct lives *l;

#ifdef PCC_DEBUG
	RDEBUG(("addalledges for %d\n", e->nodnum));
#endif

	if (e->r_class == -1)
		return; /* unused */

	if (ONLIST(e) != &precolored) {
		for (i = 0; ndontregs[i] >= 0; i++)
			AddEdge(e, &ablock[ndontregs[i]]);
	}

	/* First add to long-lived temps and hard regs */
	RDEBUG(("addalledges longlived "));
	for (i = 0; i < xbits; i += NUMBITS) {
		if ((k = live[i/NUMBITS]) == 0)
			continue;
		while (k) {
			j = ffs(k)-1;
			if (i+j < MAXREGS)
				AddEdge(&ablock[i+j], e);
			else
				AddEdge(&nblock[i+j+tempmin-MAXREGS], e);
			RRDEBUG(("%d ", i+j+tempmin));
			k &= ~(1 << j);
		}
	}
	RDEBUG(("done\n"));
	/* short-lived temps */
	RDEBUG(("addalledges shortlived "));
	DLIST_FOREACH(l, &lused, link) {
#ifdef PCC_DEBUG
		RRDEBUG(("%d ", ASGNUM(l->var)));
#endif
		AddEdge(l->var, e);
	}
	RDEBUG(("done\n"));
}

/*
 * Add a move edge between def and use.
 */
static void
moveadd(REGW *def, REGW *use)
{
	REGM *r;

	if (def == use)
		return; /* no move to itself XXX - ``shouldn't happen'' */
#ifdef PCC_DEBUG
	RDEBUG(("moveadd: def %d use %d\n", ASGNUM(def), ASGNUM(use)));
#endif

	r = WORKLISTMOVEADD(use, def);
	MOVELISTADD(def, r);
	MOVELISTADD(use, r);
}

/*
 * Traverse arguments backwards.
 * XXX - can this be tricked in some other way?
 */
static void
argswalk(NODE *p)
{

	if (p->n_op == CM) {
		argswalk(p->n_left);
		insnwalk(p->n_right);
	} else
		insnwalk(p);
}

/*
 * Add to (or remove from) live set variables that must not
 * be clobbered when traversing down on the other leg for 
 * a BITYPE node.
 */
static void
setlive(NODE *p, int set, REGW *rv)
{
	if (rv != NULL) {
		set ? LIVEADDR(rv) : LIVEDELR(rv);
		return;
	}

	if (p->n_regw != NULL) {
		set ? LIVEADDR(p->n_regw) : LIVEDELR(p->n_regw);
		return;
	}

	switch (optype(p->n_op)) {
	case LTYPE:
		if (p->n_op == REG || p->n_op == TEMP)
			set ? LIVEADD(regno(p)) : LIVEDEL(regno(p));
		break;
	case BITYPE:
		setlive(p->n_right, set, rv);
		/* FALLTHROUGH */
	case UTYPE:
		setlive(p->n_left, set, rv);
		break;
	}
}

/*
 * Add edges for temporary w against all temporaries that may be
 * used simultaneously (like index registers).
 */
static void
addedge_r(NODE *p, REGW *w)
{
	RRDEBUG(("addedge_r: node %p regw %p\n", p, w));

	if (p->n_regw != NULL) {
		AddEdge(p->n_regw, w);
		return;
	}

	if (optype(p->n_op) == BITYPE)
		addedge_r(p->n_right, w);
	if (optype(p->n_op) != LTYPE)
		addedge_r(p->n_left, w);
}

/*
 * add/del parameter from live set.
 */
static void
setxarg(NODE *p)
{
	int i, ut = 0, in = 0;
	int cw;

	if (p->n_op == ICON && p->n_type == STRTY)
		return;

	RDEBUG(("setxarg %p %s\n", p, p->n_name));
	cw = xasmcode(p->n_name);
	if (XASMISINP(cw))
		in = 1;
	if (XASMISOUT(cw))
		ut = 1;

	switch (XASMVAL(cw)) {
	case 'g':
		if (p->n_left->n_op != REG && p->n_left->n_op != TEMP)
			break;
		/* FALLTHROUGH */
	case 'r':
		i = regno(p->n_left);
		if (ut) {
			REGW *rw = p->n_left->n_op == REG ? ablock : nblock;
			LIVEDEL(i);
			addalledges(&rw[i]);
		}
		if (in) {
			LIVEADD(i);
		}
		break;
	case 'i':
	case 'm':
	case 'n':
		break;
	default:
		comperr("bad ixarg %s", p->n_name);
	}
}

/*
 * Do the in-tree part of liveness analysis. (the difficult part)
 *
 * Walk down the tree in reversed-evaluation order (backwards).
 * The moves and edges inserted and evaluation order for
 * instructions when code is emitted is described here, hence
 * this code runs the same but backwards.
 *
 * 2-op reclaim LEFT: eval L, move to DEST, eval R.
 *	moveadd L,DEST; addedge DEST,R
 * 2-op reclaim LEFT DORIGHT: eval R, eval L, move to DEST.
 *	moveadd L,DEST; addedge DEST,R; addedge L,R
 * 2-op reclaim RIGHT; eval L, eval R, move to DEST.
 *	moveadd R,DEST; addedge DEST,L; addedge L,R
 * 2-op reclaim RIGHT DORIGHT: eval R, move to DEST, eval L.
 *	moveadd R,DEST; addedge DEST,L
 * 3-op: eval L, eval R
 *	addedge L,R
 * 3-op DORIGHT: eval R, eval L
 *	addedge L,R
 *
 * Instructions with special needs are handled just like these variants,
 * with the exception of extra added moves and edges.
 * Moves to special regs are scheduled after the evaluation of both legs.
 */

static void
insnwalk(NODE *p)
{
	int o = p->n_op;
	struct optab *q = &table[TBLIDX(p->n_su)];
	REGW *lr, *rr, *rv, *r, *rrv, *lrv;
	NODE *lp, *rp;
	int i, n;

	RDEBUG(("insnwalk %p\n", p));

	rv = p->n_regw;

	rrv = lrv = NULL;
	if (p->n_op == ASSIGN &&
	    (p->n_left->n_op == TEMP || p->n_left->n_op == REG)) {
		lr = p->n_left->n_op == TEMP ? nblock : ablock;
		i = regno(p->n_left);
		LIVEDEL(i);	/* remove assigned temp from live set */
		addalledges(&lr[i]);
	}

	/* Add edges for the result of this node */
	if (rv && (q->visit & INREGS || o == TEMP || o == REG))	
		addalledges(rv);

	/* special handling of CALL operators */
	if (callop(o)) {
		if (rv)
			moveadd(rv, &ablock[RETREG(p->n_type)]);
		for (i = 0; tempregs[i] >= 0; i++)
			addalledges(&ablock[tempregs[i]]);
	}

	/* for special return value registers add moves */
	if ((q->needs & NSPECIAL) && (n = rspecial(q, NRES)) >= 0) {
		rv = &ablock[n];
		moveadd(p->n_regw, rv);
	}

	/* Check leaves for results in registers */
	lr = optype(o) != LTYPE ? p->n_left->n_regw : NULL;
	lp = optype(o) != LTYPE ? p->n_left : NULL;
	rr = optype(o) == BITYPE ? p->n_right->n_regw : NULL;
	rp = optype(o) == BITYPE ? p->n_right : NULL;

	/* simple needs */
	n = ncnt(q->needs);
	for (i = 0; i < n; i++) {
#if 1
		static int ncl[] = { 0, NASL, NBSL, NCSL, NDSL };
		static int ncr[] = { 0, NASR, NBSR, NCSR, NDSR };
		int j;

		/* edges are already added */
		if ((r = &p->n_regw[1+i])->r_class == -1) {
			r = p->n_regw;
		} else {
			AddEdge(r, p->n_regw);
			addalledges(r);
		}
		if (optype(o) != LTYPE && (q->needs & ncl[CLASS(r)]) == 0)
			addedge_r(p->n_left, r);
		if (optype(o) == BITYPE && (q->needs & ncr[CLASS(r)]) == 0)
			addedge_r(p->n_right, r);
		for (j = i + 1; j < n; j++) {
			if (p->n_regw[j+1].r_class == -1)
				continue;
			AddEdge(r, &p->n_regw[j+1]);
		}
#else
		if ((r = &p->n_regw[1+i])->r_class == -1)
			continue;
		addalledges(r);
		if (optype(o) != LTYPE && (q->needs & NASL) == 0)
			addedge_r(p->n_left, r);
		if (optype(o) == BITYPE && (q->needs & NASR) == 0)
			addedge_r(p->n_right, r);
#endif
	}

	/* special needs */
	if (q->needs & NSPECIAL) {
		struct rspecial *rc;
		for (rc = nspecial(q); rc->op; rc++) {
			switch (rc->op) {
#define	ONLY(c,s) if (c) s(c, &ablock[rc->num])
			case NLEFT:
				addalledges(&ablock[rc->num]);
				ONLY(lr, moveadd);
				break;
			case NOLEFT:
				addedge_r(p->n_left, &ablock[rc->num]);
				break;
			case NRIGHT:
				addalledges(&ablock[rc->num]);
				ONLY(rr, moveadd);
				break;
			case NORIGHT:
				addedge_r(p->n_right, &ablock[rc->num]);
				break;
			case NEVER:
				addalledges(&ablock[rc->num]);
				break;
#undef ONLY
			}
		}
	}

	if (o == ASSIGN) {
		/* avoid use of unhandled registers */
		if (p->n_left->n_op == REG &&
		    !TESTBIT(validregs, regno(p->n_left)))
			lr = NULL;
		if (p->n_right->n_op == REG &&
		    !TESTBIT(validregs, regno(p->n_right)))
			rr = NULL;
		/* needs special treatment */
		if (lr && rr)
			moveadd(lr, rr);
		if (lr && rv)
			moveadd(lr, rv);
		if (rr && rv)
			moveadd(rr, rv);
	} else if (callop(o)) {
		int *c;

		for (c = livecall(p); *c != -1; c++) {
			addalledges(ablock + *c);
			LIVEADD(*c);
		}
	} else if (q->rewrite & (RESC1|RESC2|RESC3)) {
		if (lr && rr)
			AddEdge(lr, rr);
	} else if (q->rewrite & RLEFT) {
		if (lr && rv)
			moveadd(rv, lr), lrv = rv;
		if (rv && rp)
			addedge_r(rp, rv);
	} else if (q->rewrite & RRIGHT) {
		if (rr && rv)
			moveadd(rv, rr), rrv = rv;
		if (rv && lp)
			addedge_r(lp, rv);
	}

	switch (optype(o)) {
	case BITYPE:
		if (p->n_op == ASSIGN &&
		    (p->n_left->n_op == TEMP || p->n_left->n_op == REG)) {
			/* only go down right node */
			insnwalk(p->n_right);
		} else if (callop(o)) {
			insnwalk(p->n_left);
			/* Do liveness analysis on arguments (backwards) */
			argswalk(p->n_right);
		} else if ((p->n_su & DORIGHT) == 0) {
			setlive(p->n_left, 1, lrv);
			insnwalk(p->n_right);
			setlive(p->n_left, 0, lrv);
			insnwalk(p->n_left);
		} else {
			setlive(p->n_right, 1, rrv);
			insnwalk(p->n_left);
			setlive(p->n_right, 0, rrv);
			insnwalk(p->n_right);
		}
		break;

	case UTYPE:
		insnwalk(p->n_left);
		break;

	case LTYPE:
		switch (o) {
		case REG:
			if (!TESTBIT(validregs, regno(p)))
				break; /* never add moves */
			/* FALLTHROUGH */
		case TEMP:
			i = regno(p);
			rr = (o == TEMP ? &nblock[i] :  &ablock[i]);
			if (rv != rr) {
				addalledges(rr);
				moveadd(rv, rr);
			}
			LIVEADD(i);
			break;

		case OREG: /* XXX - not yet */
			break; 

		default:
			break;
		}
		break;
	}
}

static bittype **gen, **killed, **in, **out;

#define	MAXNSPILL	100
static int notspill[MAXNSPILL], nspill;

static int
innotspill(int n)
{
	int i;
	for (i = 0; i < nspill; i++)
		if (notspill[i] == n)
			return 1;
	return 0;
}

/*
 * Found an extended assembler node, so growel out gen/killed nodes.
 */
static void
xasmionize(NODE *p, void *arg)
{
	int bb = *(int *)arg;
	int cw, b;

	if (p->n_op == ICON && p->n_type == STRTY)
		return; /* dummy end marker */

	cw = xasmcode(p->n_name);
	if (XASMVAL(cw) == 'n' || XASMVAL(cw) == 'm')
		return; /* no flow analysis */
	p = p->n_left;

	if (XASMVAL(cw) == 'g' && p->n_op != TEMP && p->n_op != REG)
		return; /* no flow analysis */

	b = regno(p);
	if (XASMVAL(cw) == 'r' && p->n_op == TEMP) {
		if (!innotspill(b)) {
			if (nspill < MAXNSPILL)
				notspill[nspill++] = b;
			else
				werror("MAXNSPILL overbooked");
		}
	}
	if (XASMISOUT(cw)) {
		if (p->n_op == TEMP) {
			b -= tempmin+MAXREGS;
			BITCLEAR(gen[bb], b);
			BITSET(killed[bb], b);
		} else if (p->n_op == REG) {
			BITCLEAR(gen[bb], b);
			BITSET(killed[bb], b);
		} else
			uerror("bad xasm node type");
	}
	if (XASMISINP(cw)) {
		if (p->n_op == TEMP) {
			BITSET(gen[bb], (b - tempmin+MAXREGS));
		} else if (p->n_op == REG) {
			BITSET(gen[bb], b);
		} else if (optype(p->n_op) != LTYPE) {
			if (XASMVAL(cw) == 'r')
				uerror("couldn't find available register");
			else
				uerror("bad xasm node type2");
		}
	}
}

/*
 * Check that given constraints are valid.
 */
static void
xasmconstr(NODE *p, void *arg)
{
	int i;

	if (p->n_op == ICON && p->n_type == STRTY)
		return; /* no constraints */

	if (strcmp(p->n_name, "cc") == 0 || strcmp(p->n_name, "memory") == 0)
		return;

	for (i = 0; i < MAXREGS; i++)
		if (strcmp(rnames[i], p->n_name) == 0) {
			addalledges(&ablock[i]);
			return;
		}

	comperr("unsupported xasm constraint %s", p->n_name);
}

/*
 * Set/clear long term liveness for regs and temps.
 */
static void
unionize(NODE *p, int bb)
{
	int i, o, ty;

	if ((o = p->n_op) == TEMP) {
#ifdef notyet
		for (i = 0; i < szty(p->n_type); i++) {
			BITSET(gen[bb], (regno(p) - tempmin+i+MAXREGS));
		}
#else
		i = 0;
		BITSET(gen[bb], (regno(p) - tempmin+i+MAXREGS));
#endif
	} else if (o == REG) {
		BITSET(gen[bb], regno(p));
	}
	if (asgop(o)) {
		if (p->n_left->n_op == TEMP) {
			int b = regno(p->n_left) - tempmin+MAXREGS;
#ifdef notyet
			for (i = 0; i < szty(p->n_type); i++) {
				BITCLEAR(gen[bb], (b+i));
				BITSET(killed[bb], (b+i));
			}
#else
			i = 0;
			BITCLEAR(gen[bb], (b+i));
			BITSET(killed[bb], (b+i));
#endif
			unionize(p->n_right, bb);
			return;
		} else if (p->n_left->n_op == REG) {
			int b = regno(p->n_left);
			BITCLEAR(gen[bb], b);
			BITSET(killed[bb], b);
			unionize(p->n_right, bb);
			return;
		}
	}
	ty = optype(o);
	if (ty != LTYPE)
		unionize(p->n_left, bb);
	if (ty == BITYPE)
		unionize(p->n_right, bb);
}

/*
 * Do variable liveness analysis.  Only analyze the long-lived 
 * variables, and save the live-on-exit temporaries in a bit-field
 * at the end of each basic block. This bit-field is later used
 * when doing short-range liveness analysis in Build().
 */
static void
LivenessAnalysis(void)
{
	extern struct basicblock bblocks;
	struct basicblock *bb;
	struct interpass *ip;
	int i, bbnum;

	/*
	 * generate the gen-killed sets for all basic blocks.
	 */
	DLIST_FOREACH(bb, &bblocks, bbelem) {
		bbnum = bb->bbnum;
		for (ip = bb->last; ; ip = DLIST_PREV(ip, qelem)) {
			/* gen/killed is 'p', this node is 'n' */
			if (ip->type == IP_NODE) {
				if (ip->ip_node->n_op == XASM)
					flist(ip->ip_node->n_left,
					    xasmionize, &bbnum);
				else
					unionize(ip->ip_node, bbnum);
			}
			if (ip == bb->first)
				break;
		}
		memcpy(in[bbnum], gen[bbnum], BIT2BYTE(xbits));
#ifdef PCC_DEBUG
#define	PRTRG(x) printf("%d ", i < MAXREGS ? i : i + tempmin-MAXREGS)
		if (rdebug) {
			printf("basic block %d\ngen: ", bbnum);
			for (i = 0; i < xbits; i++)
				if (TESTBIT(gen[bbnum], i))
					PRTRG(i);
			printf("\nkilled: ");
			for (i = 0; i < xbits; i++)
				if (TESTBIT(killed[bbnum], i))
					PRTRG(i);
			printf("\n");
		}
#endif
	}
}

#define	RUP(x) (((x)+NUMBITS-1)/NUMBITS)
#define	SETCOPY(t,f,i,n) for (i = 0; i < RUP(n); i++) t[i] = f[i]
#define	SETSET(t,f,i,n) for (i = 0; i < RUP(n); i++) t[i] |= f[i]
#define	SETCLEAR(t,f,i,n) for (i = 0; i < RUP(n); i++) t[i] &= ~f[i]
#define	SETCMP(v,t,f,i,n) for (i = 0; i < RUP(n); i++) \
	if (t[i] != f[i]) v = 1

/*
 * Build the set of interference edges and adjacency list.
 */
static void
Build(struct interpass *ipole)
{
	extern struct basicblock bblocks;
	struct basicblock bbfake;
	struct interpass *ip;
	struct basicblock *bb;
	struct cfgnode *cn;
	extern int nbblocks;
	bittype *saved;
	int i, j, again;

	if (xtemps == 0) {
		/*
		 * No basic block splitup is done if not optimizing,
		 * so fake one basic block to keep the liveness analysis 
		 * happy.
		 */
		nbblocks = 1;
		bbfake.bbnum = 0;
		bbfake.last = DLIST_PREV(ipole, qelem);
		bbfake.first = DLIST_NEXT(ipole, qelem);
		DLIST_INIT(&bblocks, bbelem);
		DLIST_INSERT_AFTER(&bblocks, &bbfake, bbelem);
		SLIST_INIT(&bbfake.children);
	}

	/* Just fetch space for the temporaries from stack */
	gen = alloca(nbblocks*sizeof(bittype*));
	killed = alloca(nbblocks*sizeof(bittype*));
	in = alloca(nbblocks*sizeof(bittype*));
	out = alloca(nbblocks*sizeof(bittype*));
	for (i = 0; i < nbblocks; i++) {
		BITALLOC(gen[i],alloca,xbits);
		BITALLOC(killed[i],alloca,xbits);
		BITALLOC(in[i],alloca,xbits);
		BITALLOC(out[i],alloca,xbits);
	}
	BITALLOC(saved,alloca,xbits);

	nspill = 0;
	LivenessAnalysis();

	/* register variable temporaries are live */
	for (i = 0; i < NPERMREG-1; i++) {
		if (nsavregs[i])
			continue;
		BITSET(out[nbblocks-1], (i+MAXREGS));
		for (j = i+1; j < NPERMREG-1; j++) {
			if (nsavregs[j])
				continue;
			AddEdge(&nblock[i+tempmin], &nblock[j+tempmin]);
		}
	}

	/* do liveness analysis on basic block level */
	do {
		again = 0;
		/* XXX - loop should be in reversed execution-order */
		DLIST_FOREACH_REVERSE(bb, &bblocks, bbelem) {
			i = bb->bbnum;
			SETCOPY(saved, out[i], j, xbits);
			SLIST_FOREACH(cn, &bb->children, cfgelem) {
				SETSET(out[i], in[cn->bblock->bbnum], j, xbits);
			}
			SETCMP(again, saved, out[i], j, xbits);
			SETCOPY(saved, in[i], j, xbits);
			SETCOPY(in[i], out[i], j, xbits);
			SETCLEAR(in[i], killed[i], j, xbits);
			SETSET(in[i], gen[i], j, xbits);
			SETCMP(again, saved, in[i], j, xbits);
		}
	} while (again);

#ifdef PCC_DEBUG
	if (rdebug) {
		DLIST_FOREACH(bb, &bblocks, bbelem) {
			printf("basic block %d\nin: ", bb->bbnum);
			for (i = 0; i < xbits; i++)
				if (TESTBIT(in[bb->bbnum], i))
					PRTRG(i);
			printf("\nout: ");
			for (i = 0; i < xbits; i++)
				if (TESTBIT(out[bb->bbnum], i))
					PRTRG(i);
			printf("\n");
		}
	}
#endif

	DLIST_FOREACH(bb, &bblocks, bbelem) {
		RDEBUG(("liveadd bb %d\n", bb->bbnum));
		i = bb->bbnum;
		for (j = 0; j < xbits; j += NUMBITS)
			live[j/NUMBITS] = 0;
		SETCOPY(live, out[i], j, xbits);
		for (ip = bb->last; ; ip = DLIST_PREV(ip, qelem)) {
			if (ip->type == IP_NODE) {
				if (ip->ip_node->n_op == XASM) {
					flist(ip->ip_node->n_right,
					    xasmconstr, 0);
					listf(ip->ip_node->n_left, setxarg);
				} else
					insnwalk(ip->ip_node);
			}
			if (ip == bb->first)
				break;
		}
	}

#ifdef PCC_DEBUG
	if (rdebug) {
		struct AdjSet *w;
		ADJL *x;
		REGW *y;
		MOVL *m;

		printf("Interference edges\n");
		for (i = 0; i < 256; i++) {
			if ((w = edgehash[i]) == NULL)
				continue;
			for (; w; w = w->next)
				printf("%d <-> %d\n", ASGNUM(w->u), ASGNUM(w->v));
		}
		printf("Degrees\n");
		DLIST_FOREACH(y, &initial, link) {
			printf("%d (%c): trivial [%d] ", ASGNUM(y),
			    CLASS(y)+'@', trivially_colorable(y));
			for (x = ADJLIST(y); x; x = x->r_next) {
				if (ONLIST(x->a_temp) != &selectStack &&
				    ONLIST(x->a_temp) != &coalescedNodes)
					printf("%d ", ASGNUM(x->a_temp));
				else
					printf("(%d) ", ASGNUM(x->a_temp));
			}
			printf("\n");
		}
		printf("Move nodes\n");
		DLIST_FOREACH(y, &initial, link) {
			if (MOVELIST(y) == NULL)
				continue;
			printf("%d: ", ASGNUM(y));
			for (m = MOVELIST(y); m; m = m->next) {
				REGW *yy = m->regm->src == y ?
				    m->regm->dst : m->regm->src;
				printf("%d ", ASGNUM(yy));
			}
			printf("\n");
		}
	}
#endif

}

static void
EnableMoves(REGW *n)
{
	MOVL *l;
	REGM *m;

	for (l = MOVELIST(n); l; l = l->next) {
		m = l->regm;
		if (m->queue != ACTIVE)
			continue;
		DLIST_REMOVE(m, link);
		PUSHMLIST(m, worklistMoves, WLIST);
	}
}

static void
EnableAdjMoves(REGW *nodes)
{
	ADJL *w;
	REGW *n;

	EnableMoves(nodes);
	for (w = ADJLIST(nodes); w; w = w->r_next) {
		n = w->a_temp;
		if (ONLIST(n) == &selectStack || ONLIST(n) == &coalescedNodes)
			continue;
		EnableMoves(w->a_temp);
	}
}

/*
 * Decrement the degree of node w for class c.
 */
static void
DecrementDegree(REGW *w, int c)
{
	int wast;

#ifdef PCC_DEBUG
	RRDEBUG(("DecrementDegree: w %d, c %d\n", ASGNUM(w), c));
#endif

	wast = trivially_colorable(w);
	NCLASS(w, c)--;
	if (wast == trivially_colorable(w))
		return;

	EnableAdjMoves(w);
	DELWLIST(w);
	ONLIST(w) = 0;
	if (MoveRelated(w)) {
		PUSHWLIST(w, freezeWorklist);
	} else {
		PUSHWLIST(w, simplifyWorklist);
	}
}

static void
Simplify(void)
{
	REGW *w;
	ADJL *l;

	w = POPWLIST(simplifyWorklist);
	PUSHWLIST(w, selectStack);
#ifdef PCC_DEBUG
	RDEBUG(("Simplify: node %d class %d\n", ASGNUM(w), w->r_class));
#endif

	l = w->r_adjList;
	for (; l; l = l->r_next) {
		if (ONLIST(l->a_temp) == &selectStack ||
		    ONLIST(l->a_temp) == &coalescedNodes)
			continue;
		DecrementDegree(l->a_temp, w->r_class);
	}
}

static REGW *
GetAlias(REGW *n)
{
	if (ONLIST(n) == &coalescedNodes)
		return GetAlias(ALIAS(n));
	return n;
}

static int
OK(REGW *t, REGW *r)
{
#ifdef PCC_DEBUG
	RDEBUG(("OK: t %d CLASS(t) %d adjSet(%d,%d)=%d\n",
	    ASGNUM(t), CLASS(t), ASGNUM(t), ASGNUM(r), adjSet(t, r)));

	if (rdebug > 1) {
		ADJL *w;
		int ndeg = 0;
		printf("OK degree: ");
		for (w = ADJLIST(t); w; w = w->r_next) {
			if (ONLIST(w->a_temp) != &selectStack &&
			    ONLIST(w->a_temp) != &coalescedNodes)
				printf("%c%d ", CLASS(w->a_temp)+'@',
				    ASGNUM(w->a_temp)), ndeg++;
			else
				printf("(%d) ", ASGNUM(w->a_temp));
		}
		printf("\n");
#if 0
		if (ndeg != DEGREE(t) && DEGREE(t) >= 0)
			printf("!!!ndeg %d != DEGREE(t) %d\n", ndeg, DEGREE(t));
#endif
	}
#endif

	if (trivially_colorable(t) || ONLIST(t) == &precolored || 
	    (adjSet(t, r) || !aliasmap(CLASS(t), COLOR(r))))/* XXX - check aliasmap */
		return 1;
	return 0;
}

static int
adjok(REGW *v, REGW *u)
{
	ADJL *w;
	REGW *t;

	RDEBUG(("adjok\n"));
	for (w = ADJLIST(v); w; w = w->r_next) {
		t = w->a_temp;
		if (ONLIST(t) == &selectStack || ONLIST(t) == &coalescedNodes)
			continue;
		if (OK(t, u) == 0)
			return 0;
	}
	RDEBUG(("adjok returns OK\n"));
	return 1;
}

#define oldcons /* check some more */
/*
 * Do a conservative estimation of whether two temporaries can 
 * be coalesced.  This is "Briggs-style" check.
 * Neither u nor v is precolored when called.
 */
static int
Conservative(REGW *u, REGW *v)
{
	ADJL *w, *ww;
	REGW *n;
#ifdef oldcons
	int i, ncl[NUMCLASS+1];

#ifdef PCC_DEBUG
	if (CLASS(u) != CLASS(v))
		comperr("Conservative: u(%d = %d), v(%d = %d)",
		    ASGNUM(u), CLASS(u), ASGNUM(v), CLASS(v));
#endif

	for (i = 0; i < NUMCLASS+1; i++)
		ncl[i] = 0;

#ifdef PCC_DEBUG
	RDEBUG(("Conservative (%d,%d)\n", ASGNUM(u), ASGNUM(v)));
#endif

	for (w = ADJLIST(u); w; w = w->r_next) {
		n = w->a_temp;
		if (ONLIST(n) == &selectStack || ONLIST(n) == &coalescedNodes)
			continue;
		for (ww = ADJLIST(v); ww; ww = ww->r_next)
			if (ww->a_temp == n)
				break;
		if (ww)
			continue;
		if (!trivially_colorable(n))
			ncl[CLASS(n)]++;
	}
	for (w = ADJLIST(v); w; w = w->r_next) {
		n = w->a_temp;
		if (ONLIST(n) == &selectStack || ONLIST(n) == &coalescedNodes)
			continue;
		if (!trivially_colorable(n))
			ncl[CLASS(n)]++;
	}
	i = trivially_colorable_p(CLASS(u), ncl);
#endif
{
	int xncl[NUMCLASS+1], mcl = 0, j;
	for (j = 0; j < NUMCLASS+1; j++)
		xncl[j] = 0;
	/*
	 * Increment xncl[class] up to K for each class.
	 * If all classes has reached K then check colorability and return.
	 */
	for (w = ADJLIST(u); w; w = w->r_next) {
		n = w->a_temp;
		if (ONLIST(n) == &selectStack || ONLIST(n) == &coalescedNodes)
			continue;
		if (xncl[CLASS(n)] == regK[CLASS(n)])
			continue;
		if (!trivially_colorable(n))
			xncl[CLASS(n)]++;
		if (xncl[CLASS(n)] < regK[CLASS(n)])
			continue;
		if (++mcl == NUMCLASS)
			goto out; /* cannot get more out of it */
	}
	for (w = ADJLIST(v); w; w = w->r_next) {
		n = w->a_temp;
		if (ONLIST(n) == &selectStack || ONLIST(n) == &coalescedNodes)
			continue;
		if (xncl[CLASS(n)] == regK[CLASS(n)])
			continue;
		/* ugly: have we been here already? */
		for (ww = ADJLIST(u); ww; ww = ww->r_next)
			if (ww->a_temp == n)
				break;
		if (ww)
			continue;
		if (!trivially_colorable(n))
			xncl[CLASS(n)]++;
		if (xncl[CLASS(n)] < regK[CLASS(n)])
			continue;
		if (++mcl == NUMCLASS)
			break;
	}
out:	j = trivially_colorable_p(CLASS(u), xncl);
#ifdef oldcons
	if (j != i)
		comperr("Conservative: j %d i %d", j, i);
#else
	return j;
#endif
}
#ifdef oldcons
	RDEBUG(("Conservative i=%d\n", i));
	return i;
#endif
}

static void
AddWorkList(REGW *w)
{

	if (ONLIST(w) != &precolored && !MoveRelated(w) &&
	    trivially_colorable(w)) {
		DELWLIST(w);
		PUSHWLIST(w, simplifyWorklist);
	}
}

static void
Combine(REGW *u, REGW *v)
{
	MOVL *m;
	ADJL *l;
	REGW *t;

#ifdef PCC_DEBUG
	RDEBUG(("Combine (%d,%d)\n", ASGNUM(u), ASGNUM(v)));
#endif

	if (ONLIST(v) == &freezeWorklist) {
		DELWLIST(v);
	} else {
		DELWLIST(v);
	}
	PUSHWLIST(v, coalescedNodes);
	ALIAS(v) = u;
#ifdef PCC_DEBUG
	if (rdebug) { 
		printf("adjlist(%d): ", ASGNUM(v));
		for (l = ADJLIST(v); l; l = l->r_next)
			printf("%d ", l->a_temp->nodnum);
		printf("\n");
	}
#endif
#if 1
{
	MOVL *m0 = MOVELIST(v);

	for (m0 = MOVELIST(v); m0; m0 = m0->next) {
		for (m = MOVELIST(u); m; m = m->next)
			if (m->regm == m0->regm)
				break; /* Already on list */
		if (m)
			continue; /* already on list */
		MOVELISTADD(u, m0->regm);
	}
}
#else

	if ((m = MOVELIST(u))) {
		while (m->next)
			m = m->next;
		m->next = MOVELIST(v);
	} else
		MOVELIST(u) = MOVELIST(v);
#endif
	EnableMoves(v);
	for (l = ADJLIST(v); l; l = l->r_next) {
		t = l->a_temp;
		if (ONLIST(t) == &selectStack || ONLIST(t) == &coalescedNodes)
			continue;
		/* Do not add edge if u cannot affect the colorability of t */
		/* XXX - check aliasmap */
		if (ONLIST(u) != &precolored || aliasmap(CLASS(t), COLOR(u)))
			AddEdge(t, u);
		DecrementDegree(t, CLASS(v));
	}
	if (!trivially_colorable(u) && ONLIST(u) == &freezeWorklist) {
		DELWLIST(u);
		PUSHWLIST(u, spillWorklist);
	}
#ifdef PCC_DEBUG
if (rdebug) {
	ADJL *w;
	printf("Combine %d class (%d): ", ASGNUM(u), CLASS(u));
	for (w = ADJLIST(u); w; w = w->r_next) {
		if (ONLIST(w->a_temp) != &selectStack &&
		    ONLIST(w->a_temp) != &coalescedNodes)
			printf("%d ", ASGNUM(w->a_temp));
		else
			printf("(%d) ", ASGNUM(w->a_temp));
	}
	printf("\n");
}
#endif
}

static void
Coalesce(void)
{
	REGM *m;
	REGW *x, *y, *u, *v;

	m = POPMLIST(worklistMoves);
	x = GetAlias(m->src);
	y = GetAlias(m->dst);

	if (ONLIST(y) == &precolored)
		u = y, v = x;
	else
		u = x, v = y;

#ifdef PCC_DEBUG
	RDEBUG(("Coalesce: src %d dst %d u %d v %d x %d y %d\n",
	    ASGNUM(m->src), ASGNUM(m->dst), ASGNUM(u), ASGNUM(v),
	    ASGNUM(x), ASGNUM(y)));
#endif

	if (CLASS(m->src) != CLASS(m->dst))
		comperr("Coalesce: src class %d, dst class %d",
		    CLASS(m->src), CLASS(m->dst));

	if (u == v) {
		RDEBUG(("Coalesce: u == v\n"));
		PUSHMLIST(m, coalescedMoves, COAL);
		AddWorkList(u);
	} else if (ONLIST(v) == &precolored || adjSet(u, v)) {
		RDEBUG(("Coalesce: constrainedMoves\n"));
		PUSHMLIST(m, constrainedMoves, CONSTR);
		AddWorkList(u);
		AddWorkList(v);
	} else if ((ONLIST(u) == &precolored && adjok(v, u)) ||
	    (ONLIST(u) != &precolored && Conservative(u, v))) {
		RDEBUG(("Coalesce: Conservative\n"));
		PUSHMLIST(m, coalescedMoves, COAL);
		Combine(u, v);
		AddWorkList(u);
	} else {
		RDEBUG(("Coalesce: activeMoves\n"));
		PUSHMLIST(m, activeMoves, ACTIVE);
	}
}

static void
FreezeMoves(REGW *u)
{
	MOVL *w, *o;
	REGM *m;
	REGW *z;
	REGW *x, *y, *v;

	for (w = MOVELIST(u); w; w = w->next) {
		m = w->regm;
		if (m->queue != WLIST && m->queue != ACTIVE)
			continue;
		x = m->src;
		y = m->dst;
		if (GetAlias(y) == GetAlias(u))
			v = GetAlias(x);
		else
			v = GetAlias(y);
#ifdef PCC_DEBUG
		RDEBUG(("FreezeMoves: u %d (%d,%d) v %d\n",
		    ASGNUM(u),ASGNUM(x),ASGNUM(y),ASGNUM(v)));
#endif
		DLIST_REMOVE(m, link);
		PUSHMLIST(m, frozenMoves, FROZEN);
		if (ONLIST(v) != &freezeWorklist)
			continue;
		for (o = MOVELIST(v); o; o = o->next)
			if (o->regm->queue == WLIST || o->regm->queue == ACTIVE)
				break;
		if (o == NULL) {
			z = v;
			DELWLIST(z);
			PUSHWLIST(z, simplifyWorklist);
		}
	}
}

static void
Freeze(void)
{
	REGW *u;

	/* XXX
	 * Should check if the moves to freeze have exactly the same 
	 * interference edges.  If they do, coalesce them instead, it
	 * may free up other nodes that they interfere with.
	 */
	u = POPWLIST(freezeWorklist);
	PUSHWLIST(u, simplifyWorklist);
#ifdef PCC_DEBUG
	RDEBUG(("Freeze %d\n", ASGNUM(u)));
#endif
	FreezeMoves(u);
}

static void
SelectSpill(void)
{
	REGW *w;

	RDEBUG(("SelectSpill\n"));
#ifdef PCC_DEBUG
	if (rdebug)
		DLIST_FOREACH(w, &spillWorklist, link)
			printf("SelectSpill: %d\n", ASGNUM(w));
#endif

	/* First check if we can spill register variables */
	DLIST_FOREACH(w, &spillWorklist, link) {
		if (w >= &nblock[tempmin] && w < &nblock[basetemp])
			break;
	}

	if (w == &spillWorklist) {
		/* try to find another long-range variable */
		DLIST_FOREACH(w, &spillWorklist, link) {
			if (innotspill(w - nblock))
				continue;
			if (w >= &nblock[tempmin] && w < &nblock[tempmax])
				break;
		}
	}

	if (w == &spillWorklist) {
		/* no heuristics, just fetch first element */
		/* but not if leaf */
		DLIST_FOREACH(w, &spillWorklist, link) {
			if (w->r_nclass[0] == 0)
				break;
		}
	}

	if (w == &spillWorklist) {
		/* Eh, only leaves :-/ Try anyway */
		/* May not be useable */
		w = DLIST_NEXT(&spillWorklist, link);
	}
 
        DLIST_REMOVE(w, link);

	PUSHWLIST(w, simplifyWorklist);
#ifdef PCC_DEBUG
	RDEBUG(("Freezing node %d\n", ASGNUM(w)));
#endif
	FreezeMoves(w);
}

/*
 * Set class on long-lived temporaries based on its type.
 */
static void
traclass(NODE *p)
{
	REGW *nb;

	if (p->n_op != TEMP)
		return;

	nb = &nblock[regno(p)];
	if (CLASS(nb) == 0)
		CLASS(nb) = gclass(p->n_type);
}

static void
paint(NODE *p)
{
	struct optab *q;
	REGW *w, *ww;
	int i;

	if (p->n_regw != NULL) {
		/* Must color all allocated regs also */
		ww = w = p->n_regw;
		q = &table[TBLIDX(p->n_su)];
		p->n_reg = COLOR(w);
		w++;
		if (q->needs & ALLNEEDS)
			for (i = 0; i < ncnt(q->needs); i++) {
				if (w->r_class == -1)
					p->n_reg |= ENCRA(COLOR(ww), i);
				else
					p->n_reg |= ENCRA(COLOR(w), i);
				w++;
			}
	} else
		p->n_reg = -1;
	if (p->n_op == TEMP) {
		REGW *nb = &nblock[regno(p)];
		regno(p) = COLOR(nb);
		if (TCLASS(p->n_su) == 0)
			SCLASS(p->n_su, CLASS(nb));
		p->n_op = REG;
		p->n_lval = 0;
	}
}

static void
AssignColors(struct interpass *ip)
{
	int okColors, c;
	REGW *o, *w;
	ADJL *x;

	RDEBUG(("AssignColors\n"));
	while (!WLISTEMPTY(selectStack)) {
		w = POPWLIST(selectStack);
		okColors = classmask(CLASS(w));
#ifdef PCC_DEBUG
		RDEBUG(("classmask av %d, class %d: %x\n",
		    w->nodnum, CLASS(w), okColors));
#endif

		for (x = ADJLIST(w); x; x = x->r_next) {
			o = GetAlias(x->a_temp);
#ifdef PCC_DEBUG
			RRDEBUG(("Adj(%d): %d (%d)\n",
			    ASGNUM(w), ASGNUM(o), ASGNUM(x->a_temp)));
#endif

			if (ONLIST(o) == &coloredNodes ||
			    ONLIST(o) == &precolored) {
				c = aliasmap(CLASS(w), COLOR(o));
				RRDEBUG(("aliasmap in class %d by color %d: "
				    "%x, okColors %x\n",
				    CLASS(w), COLOR(o), c, okColors));

				okColors &= ~c;
			}
		}
		if (okColors == 0) {
			PUSHWLIST(w, spilledNodes);
#ifdef PCC_DEBUG
			RDEBUG(("Spilling node %d\n", ASGNUM(w)));
#endif
		} else {
			PUSHWLIST(w, coloredNodes);
			c = ffs(okColors)-1;
			COLOR(w) = color2reg(c, CLASS(w));
#ifdef PCC_DEBUG
			RDEBUG(("Coloring %d with %s, free %x\n",
			    ASGNUM(w), rnames[COLOR(w)], okColors));
#endif
		}
	}
	DLIST_FOREACH(w, &coalescedNodes, link) {
		REGW *ww = GetAlias(w);
		COLOR(w) = COLOR(ww);
		if (ONLIST(ww) == &spilledNodes) {
#ifdef PCC_DEBUG
			RDEBUG(("coalesced node %d spilled\n", w->nodnum));
#endif
			ww = DLIST_PREV(w, link);
			DLIST_REMOVE(w, link);
			PUSHWLIST(w, spilledNodes);
			w = ww;
		} else {
#ifdef PCC_DEBUG
			RDEBUG(("Giving coalesced node %d color %s\n",
			    w->nodnum, rnames[COLOR(w)]));
#endif
		}
	}

#ifdef PCC_DEBUG
	if (rdebug)
		DLIST_FOREACH(w, &coloredNodes, link)
			printf("%d: color %s\n", ASGNUM(w), rnames[COLOR(w)]);
#endif
	if (DLIST_ISEMPTY(&spilledNodes, link)) {
		struct interpass *ip2;
		DLIST_FOREACH(ip2, ip, qelem)
			if (ip2->type == IP_NODE)
				walkf(ip2->ip_node, paint);
	}
}

static REGW *spole;
/*
 * Store all spilled nodes in memory by fetching a temporary on the stack.
 * Will never end up here if not optimizing.
 */
static void
longtemp(NODE *p)
{
	NODE *l, *r;
	REGW *w;

	if (p->n_op != TEMP)
		return;
	/* XXX - should have a bitmask to find temps to convert */
	DLIST_FOREACH(w, spole, link) {
		if (w != &nblock[regno(p)])
			continue;
		if (w->r_class == 0) {
			w->r_color = BITOOR(freetemp(szty(p->n_type)));
			w->r_class = 1;
		}
		l = mklnode(REG, 0, FPREG, INCREF(p->n_type));
		r = mklnode(ICON, w->r_color, 0, INT);
		p->n_left = mkbinode(PLUS, l, r, INCREF(p->n_type));
		p->n_op = UMUL;
		p->n_regw = NULL;
		break;
	}
}

static struct interpass *cip;
/*
 * Rewrite a tree by storing a variable in memory.
 * XXX - must check if basic block structure is destroyed!
 */
static void
shorttemp(NODE *p)
{
	struct interpass *nip;
	struct optab *q;
	REGW *w;
	NODE *l, *r;
	int off;

	/* XXX - optimize this somewhat */
	DLIST_FOREACH(w, spole, link) {
		if (w != p->n_regw)
			continue;
		/* XXX - use canaddr() */
		if (p->n_op == OREG || p->n_op == NAME) {
			DLIST_REMOVE(w, link);
#ifdef PCC_DEBUG
			RDEBUG(("Node %d already in memory\n", ASGNUM(w)));
#endif
			break;
		}
#ifdef PCC_DEBUG
		RDEBUG(("rewriting node %d\n", ASGNUM(w)));
#endif

		off = BITOOR(freetemp(szty(p->n_type)));
		l = mklnode(OREG, off, FPREG, p->n_type);
		r = talloc();
		/*
		 * If this is a binode which reclaim a leg, and it had
		 * to walk down the other leg first, then it must be
		 * split below this node instead.
		 */
		q = &table[TBLIDX(p->n_su)];
		if (optype(p->n_op) == BITYPE &&
		    (q->rewrite & RLEFT && (p->n_su & DORIGHT) == 0) &&
		    (TBLIDX(p->n_right->n_su) != 0)) {
			*r = *l;
			nip = ipnode(mkbinode(ASSIGN, l,
			    p->n_left, p->n_type));
			p->n_left = r;
		} else if (optype(p->n_op) == BITYPE &&
		    (q->rewrite & RRIGHT && (p->n_su & DORIGHT) != 0) &&
		    (TBLIDX(p->n_left->n_su) != 0)) {
			*r = *l;
			nip = ipnode(mkbinode(ASSIGN, l,
			    p->n_right, p->n_type));
			p->n_right = r;
		} else {
			*r = *p;
			nip = ipnode(mkbinode(ASSIGN, l, r, p->n_type));
			*p = *l;
		}
		DLIST_INSERT_BEFORE(cip, nip, qelem);
		DLIST_REMOVE(w, link);
		break;
	}
}

/*
 * Change the TEMPs in the ipole list to stack variables.
 */
static void
treerewrite(struct interpass *ipole, REGW *rpole)
{
	struct interpass *ip;

	spole = rpole;

	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		cip = ip;
		walkf(ip->ip_node, shorttemp);	/* convert temps to oregs */
	}
	if (!DLIST_ISEMPTY(spole, link))
		comperr("treerewrite not empty");
}

/*
 * Change the TEMPs in the ipole list to stack variables.
 */
static void
leafrewrite(struct interpass *ipole, REGW *rpole)
{
	extern NODE *nodepole;
	extern int thisline;
	struct interpass *ip;

	spole = rpole;
	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type != IP_NODE)
			continue;
		nodepole = ip->ip_node;
		thisline = ip->lineno;
		walkf(ip->ip_node, longtemp);	/* convert temps to oregs */
	}
	nodepole = NIL;
}

/*
 * Avoid copying spilled argument to new position on stack.
 */
static int
temparg(struct interpass *ipole, REGW *w)
{
	struct interpass *ip;
	NODE *p;

	ip = DLIST_NEXT(ipole, qelem); /* PROLOG */
	ip = DLIST_NEXT(ip, qelem); /* first DEFLAB */
	ip = DLIST_NEXT(ip, qelem); /* first NODE */
	for (; ip->type != IP_DEFLAB; ip = DLIST_NEXT(ip, qelem)) {
		if (ip->type == IP_ASM)
			continue;
		p = ip->ip_node;
#ifdef notdef
		/* register args may already have been put on stack */
		if (p->n_op != ASSIGN || p->n_left->n_op != TEMP)
			comperr("temparg");
#endif
		if (p->n_right->n_op != OREG)
			continue; /* arg in register */
		if (w != &nblock[regno(p->n_left)])
			continue;
		w->r_color = p->n_right->n_lval;
		tfree(p);
		/* Cannot DLIST_REMOVE here, would break basic blocks */
		/* Make it a nothing instead */
		ip->type = IP_ASM;
		ip->ip_asm="";
		return 1;
	}
	return 0;
}

#define	ONLYPERM 1
#define	LEAVES	 2
#define	SMALL	 3

/*
 * Scan the whole function and search for temporaries to be stored
 * on-stack.
 *
 * Be careful to not destroy the basic block structure in the first scan.
 */
static int
RewriteProgram(struct interpass *ip)
{
	REGW shortregs, longregs, saveregs, *q;
	REGW *w;
	int rwtyp;

	RDEBUG(("RewriteProgram\n"));
	DLIST_INIT(&shortregs, link);
	DLIST_INIT(&longregs, link);
	DLIST_INIT(&saveregs, link);

	/* sort the temporaries in three queues, short, long and perm */
	while (!DLIST_ISEMPTY(&spilledNodes, link)) {
		w = DLIST_NEXT(&spilledNodes, link);
		DLIST_REMOVE(w, link);

		if (w >= &nblock[tempmin] && w < &nblock[basetemp]) {
			q = &saveregs;
		} else if (w >= &nblock[basetemp] && w < &nblock[tempmax]) {
			q = &longregs;
		} else
			q = &shortregs;
		DLIST_INSERT_AFTER(q, w, link);
	}
#ifdef PCC_DEBUG
	if (rdebug) {
		printf("permanent: ");
		DLIST_FOREACH(w, &saveregs, link)
			printf("%d ", ASGNUM(w));
		printf("\nlong-lived: ");
		DLIST_FOREACH(w, &longregs, link)
			printf("%d ", ASGNUM(w));
		printf("\nshort-lived: ");
		DLIST_FOREACH(w, &shortregs, link)
			printf("%d ", ASGNUM(w));
		printf("\n");
	}
#endif
	rwtyp = 0;

	if (!DLIST_ISEMPTY(&saveregs, link)) {
		rwtyp = ONLYPERM;
		DLIST_FOREACH(w, &saveregs, link) {
			int num = w - nblock - tempmin;
			nsavregs[num] = 1;
		}
	}
	if (!DLIST_ISEMPTY(&longregs, link)) {
		rwtyp = LEAVES;
		DLIST_FOREACH(w, &longregs, link) {
			w->r_class = xtemps ? temparg(ip, w) : 0;
		}
	}

	if (rwtyp == LEAVES) {
		leafrewrite(ip, &longregs);
		rwtyp = ONLYPERM;
	}

	if (rwtyp == 0 && !DLIST_ISEMPTY(&shortregs, link)) {
		/* Must rewrite the trees */
		treerewrite(ip, &shortregs);
//		if (xtemps)
//			comperr("treerewrite");
		rwtyp = SMALL;
	}

	RDEBUG(("savregs %x rwtyp %d\n", 0, rwtyp));

	return rwtyp;
}

#ifdef PCC_DEBUG
/*
 * Print TEMP/REG contents in a node.
 */
void
prtreg(FILE *fp, NODE *p)
{
	int i, n = p->n_su == -1 ? 0 : ncnt(table[TBLIDX(p->n_su)].needs);

	if (use_regw || p->n_reg > 0x40000000 || p->n_reg < 0) {
		fprintf(fp, "TEMP ");
		if (p->n_regw != NULL) {
			for (i = 0; i < n+1; i++)
				fprintf(fp, "%d ", p->n_regw[i].nodnum);
		} else
			fprintf(fp, "<undef>");
	} else {
		fprintf(fp, "REG ");
		if (p->n_reg != -1) {
			for (i = 0; i < n+1; i++)
				fprintf(fp, "%s ", rnames[DECRA(p->n_reg, i)]);
		} else
			fprintf(fp, "<undef>");
	}
}
#endif

#ifdef notyet
/*
 * Assign instructions, calculate evaluation order and
 * set temporary register numbers.
 */
static void
insgen()
{
	geninsn(); /* instruction assignment */
	sucomp();  /* set evaluation order */
	slong();   /* set long temp types */
	sshort();  /* set short temp numbers */
}
#endif

/*
 * Do register allocation for trees by graph-coloring.
 */
void
ngenregs(struct interpass *ipole)
{
	extern NODE *nodepole;
	struct interpass_prolog *ipp, *epp;
	struct interpass *ip;
	int i, j, tbits;
	int uu[NPERMREG] = { -1 };
	int xnsavregs[NPERMREG];
	int beenhere = 0;
	TWORD type;

	DLIST_INIT(&lunused, link);
	DLIST_INIT(&lused, link);

	/*
	 * Do some setup before doing the real thing.
	 */
	ipp = (struct interpass_prolog *)DLIST_NEXT(ipole, qelem);
	epp = (struct interpass_prolog *)DLIST_PREV(ipole, qelem);

	tempmin = ipp->ip_tmpnum;
	tempmax = epp->ip_tmpnum;

	/*
	 * Allocate space for the permanent registers in the
	 * same block as the long-lived temporaries.
	 * These temporaries will be handled the same way as 
	 * all other variables.
	 */
	basetemp = tempmin;
	nsavregs = xnsavregs;
	for (i = 0; i < NPERMREG; i++)
		xnsavregs[i] = 0;
	ndontregs = uu; /* currently never avoid any regs */

	tempmin -= (NPERMREG-1);
#ifdef notyet
	if (xavoidfp)
		dontregs |= REGBIT(FPREG);
#endif

#ifdef PCC_DEBUG
	nodnum = tempmax;
#endif
	tbits = tempmax - tempmin;	/* # of temporaries */
	xbits = tbits + MAXREGS;	/* total size of live array */
	if (tbits) {
		nblock = tmpalloc(tbits * sizeof(REGW));

		nblock -= tempmin;
		RDEBUG(("nblock %p num %d size %zu\n",
		    nblock, tbits, (size_t)(tbits * sizeof(REGW))));
	}
	live = tmpalloc(BIT2BYTE(xbits));

	/* Block for precolored nodes */
	ablock = tmpalloc(sizeof(REGW)*MAXREGS);
	memset(ablock, 0, sizeof(REGW)*MAXREGS);
	for (i = 0; i < MAXREGS; i++) {
		ablock[i].r_onlist = &precolored;
		ablock[i].r_class = GCLASS(i); /* XXX */
		ablock[i].r_color = i;
#ifdef PCC_DEBUG
		ablock[i].nodnum = i;
#endif
	}
#ifdef notyet
	TMPMARK();
#endif


recalc:
onlyperm: /* XXX - should not have to redo all */
	memset(edgehash, 0, sizeof(edgehash));

	if (tbits) {
		memset(nblock+tempmin, 0, tbits * sizeof(REGW));
#ifdef PCC_DEBUG
		for (i = tempmin; i < tempmax; i++)
			nblock[i].nodnum = i;
#endif
	}
	memset(live, 0, BIT2BYTE(xbits));
	RPRINTIP(ipole);
	DLIST_INIT(&initial, link);
	DLIST_FOREACH(ip, ipole, qelem) {
		extern int thisline;
		if (ip->type != IP_NODE)
			continue;
		nodepole = ip->ip_node;
		thisline = ip->lineno;
		if (ip->ip_node->n_op != XASM)
			geninsn(ip->ip_node, FOREFF);
		nsucomp(ip->ip_node);
		walkf(ip->ip_node, traclass);
	}
	nodepole = NIL;
	RDEBUG(("nsucomp allocated %d temps (%d,%d)\n", 
	    tempmax-tempmin, tempmin, tempmax));

#ifdef PCC_DEBUG
	use_regw = 1;
#endif
	RPRINTIP(ipole);
#ifdef PCC_DEBUG
	use_regw = 0;
#endif
	RDEBUG(("ngenregs: numtemps %d (%d, %d)\n", tempmax-tempmin,
		    tempmin, tempmax));

	DLIST_INIT(&coalescedMoves, link);
	DLIST_INIT(&constrainedMoves, link);
	DLIST_INIT(&frozenMoves, link);
	DLIST_INIT(&worklistMoves, link);
	DLIST_INIT(&activeMoves, link);

	/* Set class and move-related for perm regs */
	for (i = 0; i < (NPERMREG-1); i++) {
		if (nsavregs[i])
			continue;
		nblock[i+tempmin].r_class = GCLASS(permregs[i]);
		DLIST_INSERT_AFTER(&initial, &nblock[i+tempmin], link);
		moveadd(&nblock[i+tempmin], &ablock[permregs[i]]);
		addalledges(&nblock[i+tempmin]);
	}

	Build(ipole);
	RDEBUG(("Build done\n"));
	MkWorklist();
	RDEBUG(("MkWorklist done\n"));
	do {
		if (!WLISTEMPTY(simplifyWorklist))
			Simplify();
		else if (!WLISTEMPTY(worklistMoves))
			Coalesce();
		else if (!WLISTEMPTY(freezeWorklist))
			Freeze();
		else if (!WLISTEMPTY(spillWorklist))
			SelectSpill();
	} while (!WLISTEMPTY(simplifyWorklist) || !WLISTEMPTY(worklistMoves) ||
	    !WLISTEMPTY(freezeWorklist) || !WLISTEMPTY(spillWorklist));
	AssignColors(ipole);

	RDEBUG(("After AssignColors\n"));
	RPRINTIP(ipole);

	if (!WLISTEMPTY(spilledNodes)) {
		switch (RewriteProgram(ipole)) {
		case ONLYPERM:
			goto onlyperm;
		case SMALL:
			optimize(ipole);
			if (beenhere++ == MAXLOOP)
				comperr("beenhere");
			goto recalc;
		}
	}

	/* fill in regs to save */
	ipp->ipp_regs = 0;
	for (i = 0; i < NPERMREG-1; i++) {
		NODE *p;

		if (nsavregs[i]) {
			ipp->ipp_regs |= (1 << permregs[i]);
			continue; /* Spilled */
		}
		if (nblock[i+tempmin].r_color == permregs[i])
			continue; /* Coalesced */
		/*
		 * If the original color of this permreg is used for
		 * coloring another register, swap them to avoid
		 * unnecessary moves.
		 */
		for (j = i+1; j < NPERMREG-1; j++) {
			if (nblock[j+tempmin].r_color != permregs[i])
				continue;
			nblock[j+tempmin].r_color = nblock[i+tempmin].r_color;
			break;
		}
		if (j != NPERMREG-1)
			continue;

		/* Generate reg-reg move nodes for save */
		type = PERMTYPE(permregs[i]);
#ifdef PCC_DEBUG
		if (PERMTYPE(nblock[i+tempmin].r_color) != type)
			comperr("permreg botch");
#endif
		p = mkbinode(ASSIGN,
		    mklnode(REG, 0, nblock[i+tempmin].r_color, type),
		    mklnode(REG, 0, permregs[i], type), type);
		p->n_reg = p->n_left->n_reg = p->n_right->n_reg = -1;
		p->n_left->n_su = p->n_right->n_su = 0;
		geninsn(p, FOREFF);
		ip = ipnode(p);
		DLIST_INSERT_AFTER(ipole->qelem.q_forw, ip, qelem);
		p = mkbinode(ASSIGN, mklnode(REG, 0, permregs[i], type),
		    mklnode(REG, 0, nblock[i+tempmin].r_color, type), type);
		p->n_reg = p->n_left->n_reg = p->n_right->n_reg = -1;
		p->n_left->n_su = p->n_right->n_su = 0;
		geninsn(p, FOREFF);
		ip = ipnode(p);
		DLIST_INSERT_BEFORE(ipole->qelem.q_back, ip, qelem);
	}
	epp->ipp_regs = ipp->ipp_regs;
	/* Done! */
}
