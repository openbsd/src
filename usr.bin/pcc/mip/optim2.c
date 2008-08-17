/*	$OpenBSD: optim2.c,v 1.8 2008/08/17 18:40:13 ragge Exp $	*/
/*
 * Copyright (c) 2004 Anders Magnusson (ragge@ludd.luth.se).
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
#include <stdlib.h>

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define	BDEBUG(x)	if (b2debug) printf x

#define	mktemp(n, t)	mklnode(TEMP, 0, n, t)

static int dfsnum;

void saveip(struct interpass *ip);
void deljumps(struct interpass *);
void optdump(struct interpass *ip);
void printip(struct interpass *pole);

static struct varinfo defsites;
struct interpass *storesave;
static struct interpass_prolog *ipp, *epp; /* prolog/epilog */

void bblocks_build(struct interpass *, struct labelinfo *, struct bblockinfo *);
void cfg_build(struct labelinfo *labinfo);
void cfg_dfs(struct basicblock *bb, unsigned int parent, 
	     struct bblockinfo *bbinfo);
void dominators(struct bblockinfo *bbinfo);
struct basicblock *
ancestorwithlowestsemi(struct basicblock *bblock, struct bblockinfo *bbinfo);
void link(struct basicblock *parent, struct basicblock *child);
void computeDF(struct basicblock *bblock, struct bblockinfo *bbinfo);
void findTemps(struct interpass *ip);
void placePhiFunctions(struct bblockinfo *bbinfo);
void remunreach(void);

struct basicblock bblocks;
int nbblocks;
static struct interpass *cvpole;

struct addrof {
	struct addrof *next;
	int tempnum;
	int oregoff;
} *otlink;

static int
getoff(int num)
{
	struct addrof *w;

	for (w = otlink; w; w = w->next)
		if (w->tempnum == num)
			return w->oregoff;
	return 0;
}

/*
 * Use stack argument addresses instead of copying if & is used on a var.
 */
static int
setargs(int tval, struct addrof *w)
{
	struct interpass *ip;
	NODE *p;

	ip = DLIST_NEXT(cvpole, qelem); /* PROLOG */
	ip = DLIST_NEXT(ip, qelem); /* first DEFLAB */
	ip = DLIST_NEXT(ip, qelem); /* first NODE */
	for (; ip->type != IP_DEFLAB; ip = DLIST_NEXT(ip, qelem)) {
		p = ip->ip_node;
#ifdef PCC_DEBUG
		if (p->n_op != ASSIGN || p->n_left->n_op != TEMP)
			comperr("temparg");
#endif
		if (p->n_right->n_op != OREG)
			continue; /* arg in register */
		if (tval != regno(p->n_left))
			continue; /* wrong assign */
		w->oregoff = p->n_right->n_lval;
		tfree(p);
		DLIST_REMOVE(ip, qelem);
		return 1;
        }
	return 0;
}

/*
 * Search for ADDROF elements and, if found, record them.
 */
static void
findaddrof(NODE *p)
{
	struct addrof *w;
	int tnr;

	if (p->n_op != ADDROF)
		return;
	tnr = regno(p->n_left);
	if (getoff(tnr))
		return;
	w = tmpalloc(sizeof(struct addrof));
	w->tempnum = tnr;
	if (setargs(tnr, w) == 0)
		w->oregoff = BITOOR(freetemp(szty(p->n_left->n_type)));
	w->next = otlink;
	otlink = w;
}


/*
 * Convert address-taken temps to OREGs.
 */
static void
cvtaddrof(NODE *p)
{
	NODE *l;
	int n;

	if (p->n_op != ADDROF && p->n_op != TEMP)
		return;
	if (p->n_op == TEMP) {
		n = getoff(regno(p));
		if (n == 0)
			return;
		p->n_op = OREG;
		p->n_lval = n;
		regno(p) = FPREG;
	} else {
		l = p->n_left;
		l->n_type = p->n_type;
		p->n_right = mklnode(ICON, l->n_lval, 0, l->n_type);
		p->n_op = PLUS;
		l->n_op = REG;
		l->n_lval = 0;
		l->n_rval = FPREG;
	}
}

void
optimize(struct interpass *ipole)
{
	struct interpass *ip;
	struct labelinfo labinfo;
	struct bblockinfo bbinfo;

	ipp = (struct interpass_prolog *)DLIST_NEXT(ipole, qelem);
	epp = (struct interpass_prolog *)DLIST_PREV(ipole, qelem);

	if (b2debug) {
		printf("initial links\n");
		printip(ipole);
	}

	/*
	 * Convert ADDROF TEMP to OREGs.
	 */
	if (xtemps) {
		otlink = NULL;
		cvpole = ipole;
		DLIST_FOREACH(ip, ipole, qelem) {
			if (ip->type != IP_NODE)
				continue;
			walkf(ip->ip_node, findaddrof);
		}
		if (otlink) {
			DLIST_FOREACH(ip, ipole, qelem) {
				if (ip->type != IP_NODE)
					continue;
				walkf(ip->ip_node, cvtaddrof);
			}
		}
	}
		
	if (xdeljumps)
		deljumps(ipole); /* Delete redundant jumps and dead code */

#ifdef PCC_DEBUG
	if (b2debug) {
		printf("links after deljumps\n");
		printip(ipole);
	}
#endif
	if (xssaflag || xtemps) {
		DLIST_INIT(&bblocks, bbelem);
		bblocks_build(ipole, &labinfo, &bbinfo);
		BDEBUG(("Calling cfg_build\n"));
		cfg_build(&labinfo);
	}
	if (xssaflag) {
		BDEBUG(("Calling dominators\n"));
		dominators(&bbinfo);
		BDEBUG(("Calling computeDF\n"));
		computeDF(DLIST_NEXT(&bblocks, bbelem), &bbinfo);
		BDEBUG(("Calling remunreach\n"));
		remunreach();
#if 0
		dfg = dfg_build(cfg);
		ssa = ssa_build(cfg, dfg);
#endif
	}

#ifdef PCC_DEBUG
	if (epp->ipp_regs != 0)
		comperr("register error");
#endif

	myoptim(ipole);
}

/*
 * Delete unused labels, excess of labels, gotos to gotos.
 * This routine can be made much more efficient.
 */
void
deljumps(struct interpass *ipole)
{
	struct interpass *ip, *n, *ip2, *start;
	int gotone,low, high;
	int *lblary, *jmpary, sz, o, i, j, lab1, lab2;
	int del;
	extern int negrel[];
	extern size_t negrelsize;

	low = ipp->ip_lblnum;
	high = epp->ip_lblnum;

#ifdef notyet
	mark = tmpmark(); /* temporary used memory */
#endif

	sz = (high-low) * sizeof(int);
	lblary = tmpalloc(sz);
	jmpary = tmpalloc(sz);

	/*
	 * XXX: Find the first two labels. They may not be deleted,
	 * because the register allocator expects them to be there.
	 * These will not be coalesced with any other label.
	 */
	lab1 = lab2 = -1;
	start = NULL;
	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type != IP_DEFLAB)
			continue;
		if (lab1 < 0)
			lab1 = ip->ip_lbl;
		else if (lab2 < 0) {
			lab2 = ip->ip_lbl;
			start = ip;
		} else	/* lab1 >= 0 && lab2 >= 0, we're done. */
			break;
	}
	if (lab1 < 0 || lab2 < 0)
		comperr("deljumps");

again:	gotone = 0;
	memset(lblary, 0, sz);
	lblary[lab1 - low] = lblary[lab2 - low] = 1;
	memset(jmpary, 0, sz);

	/* refcount and coalesce all labels */
	DLIST_FOREACH(ip, ipole, qelem) {
		if (ip->type == IP_DEFLAB && ip->ip_lbl != lab1 &&
		    ip->ip_lbl != lab2) {
			n = DLIST_NEXT(ip, qelem);

			/*
			 * Find unconditional jumps directly following a
			 * label. Jumps jumping to themselves are not
			 * taken into account.
			 */
			if (n->type == IP_NODE && n->ip_node->n_op == GOTO) {
				i = n->ip_node->n_left->n_lval;
				if (i != ip->ip_lbl)
					jmpary[ip->ip_lbl - low] = i;
			}

			while (n->type == IP_DEFLAB) {
				if (n->ip_lbl != lab1 && n->ip_lbl != lab2 &&
				    lblary[n->ip_lbl-low] >= 0) {
					/*
					 * If the label is used, mark the
					 * label to be coalesced with as
					 * used, too.
					 */
					if (lblary[n->ip_lbl - low] > 0 &&
					    lblary[ip->ip_lbl - low] == 0)
						lblary[ip->ip_lbl - low] = 1;
					lblary[n->ip_lbl - low] = -ip->ip_lbl;
				}
				n = DLIST_NEXT(n, qelem);
			}
		}
		if (ip->type != IP_NODE)
			continue;
		o = ip->ip_node->n_op;
		if (o == GOTO)
			i = ip->ip_node->n_left->n_lval;
		else if (o == CBRANCH)
			i = ip->ip_node->n_right->n_lval;
		else
			continue;

		/*
		 * Mark destination label i as used, if it is not already.
		 * If i is to be merged with label j, mark j as used, too.
		 */
		if (lblary[i - low] == 0)
			lblary[i-low] = 1;
		else if ((j = lblary[i - low]) < 0 && lblary[-j - low] == 0)
			lblary[-j - low] = 1;
	}

	/* delete coalesced/unused labels and rename gotos */
	DLIST_FOREACH(ip, ipole, qelem) {
		n = DLIST_NEXT(ip, qelem);
		if (n->type == IP_DEFLAB) {
			if (lblary[n->ip_lbl-low] <= 0) {
				DLIST_REMOVE(n, qelem);
				gotone = 1;
			}
		}
		if (ip->type != IP_NODE)
			continue;
		o = ip->ip_node->n_op;
		if (o == GOTO)
			i = ip->ip_node->n_left->n_lval;
		else if (o == CBRANCH)
			i = ip->ip_node->n_right->n_lval;
		else
			continue;

		/* Simplify (un-)conditional jumps to unconditional jumps. */
		if (jmpary[i - low] > 0) {
			gotone = 1;
			i = jmpary[i - low];
			if (o == GOTO)
				ip->ip_node->n_left->n_lval = i;
			else
				ip->ip_node->n_right->n_lval = i;
		}

		/* Fixup for coalesced labels. */
		if (lblary[i-low] < 0) {
			if (o == GOTO)
				ip->ip_node->n_left->n_lval = -lblary[i-low];
			else
				ip->ip_node->n_right->n_lval = -lblary[i-low];
		}
	}

	/* Delete gotos to the next statement */
	DLIST_FOREACH(ip, ipole, qelem) {
		n = DLIST_NEXT(ip, qelem);
		if (n->type != IP_NODE)
			continue;
		o = n->ip_node->n_op;
		if (o == GOTO)
			i = n->ip_node->n_left->n_lval;
		else if (o == CBRANCH)
			i = n->ip_node->n_right->n_lval;
		else
			continue;

		ip2 = n;
		ip2 = DLIST_NEXT(ip2, qelem);

		if (ip2->type != IP_DEFLAB)
			continue;
		if (ip2->ip_lbl == i && i != lab1 && i != lab2) {
			tfree(n->ip_node);
			DLIST_REMOVE(n, qelem);
			gotone = 1;
		}
	}

	/*
	 * Transform cbranch cond, 1; goto 2; 1: ... into
	 * cbranch !cond, 2; 1: ...
	 */
	DLIST_FOREACH(ip, ipole, qelem) {
		n = DLIST_NEXT(ip, qelem);
		ip2 = DLIST_NEXT(n, qelem);
		if (ip->type != IP_NODE || ip->ip_node->n_op != CBRANCH)
			continue;
		if (n->type != IP_NODE || n->ip_node->n_op != GOTO)
			continue;
		if (ip2->type != IP_DEFLAB)
			continue;
		i = ip->ip_node->n_right->n_lval;
		j = n->ip_node->n_left->n_lval;
		if (j == lab1 || j == lab2)
			continue;
		if (i != ip2->ip_lbl || i == lab1 || i == lab2)
			continue;
		ip->ip_node->n_right->n_lval = j;
		i = ip->ip_node->n_left->n_op;
		if (i < EQ || i - EQ >= (int)negrelsize)
			comperr("deljumps: unexpected op");
		ip->ip_node->n_left->n_op = negrel[i - EQ];
		tfree(n->ip_node);
		DLIST_REMOVE(n, qelem);
		gotone = 1;
	}

	/* Delete everything after a goto up to the next label. */
	for (ip = start, del = 0; ip != DLIST_ENDMARK(ipole);
	     ip = DLIST_NEXT(ip, qelem)) {
loop:
		if ((n = DLIST_NEXT(ip, qelem)) == DLIST_ENDMARK(ipole))
			break;
		if (n->type != IP_NODE) {
			del = 0;
			continue;
		}
		if (del) {
			tfree(n->ip_node);
			DLIST_REMOVE(n, qelem);
			gotone = 1;
			goto loop;
		} else if (n->ip_node->n_op == GOTO)
			del = 1;
	}

	if (gotone)
		goto again;

#ifdef notyet
	tmpfree(mark);
#endif
}

void
optdump(struct interpass *ip)
{
	static char *nm[] = { "node", "prolog", "newblk", "epilog", "locctr",
		"deflab", "defnam", "asm" };
	printf("type %s\n", nm[ip->type-1]);
	switch (ip->type) {
	case IP_NODE:
#ifdef PCC_DEBUG
		fwalk(ip->ip_node, e2print, 0);
#endif
		break;
	case IP_DEFLAB:
		printf("label " LABFMT "\n", ip->ip_lbl);
		break;
	case IP_ASM:
		printf(": %s\n", ip->ip_asm);
		break;
	}
}

/*
 * Build the basic blocks, algorithm 9.1, pp 529 in Compilers.
 *
 * Also fills the labelinfo struct with information about which bblocks
 * that contain which label.
 */

void
bblocks_build(struct interpass *ipole, struct labelinfo *labinfo,
    struct bblockinfo *bbinfo)
{
	struct interpass *ip;
	struct basicblock *bb = NULL;
	int low, high;
	int count = 0;
	int i;

	BDEBUG(("bblocks_build (%p, %p)\n", labinfo, bbinfo));
	low = ipp->ip_lblnum;
	high = epp->ip_lblnum;

	/* 
	 * First statement is a leader.
	 * Any statement that is target of a jump is a leader.
	 * Any statement that immediately follows a jump is a leader.
	 */
	DLIST_FOREACH(ip, ipole, qelem) {
		if (bb == NULL || (ip->type == IP_EPILOG) ||
		    (ip->type == IP_DEFLAB) || (ip->type == IP_DEFNAM)) {
			bb = tmpalloc(sizeof(struct basicblock));
			bb->first = ip;
			SLIST_INIT(&bb->children);
			SLIST_INIT(&bb->parents);
			bb->dfnum = 0;
			bb->dfparent = 0;
			bb->semi = 0;
			bb->ancestor = 0;
			bb->idom = 0;
			bb->samedom = 0;
			bb->bucket = NULL;
			bb->df = NULL;
			bb->dfchildren = NULL;
			bb->Aorig = NULL;
			bb->Aphi = NULL;
			bb->bbnum = count;
			DLIST_INSERT_BEFORE(&bblocks, bb, bbelem);
			count++;
		}
		bb->last = ip;
		if ((ip->type == IP_NODE) && (ip->ip_node->n_op == GOTO || 
		    ip->ip_node->n_op == CBRANCH))
			bb = NULL;
		if (ip->type == IP_PROLOG)
			bb = NULL;
	}
	nbblocks = count;

	if (b2debug) {
		printf("Basic blocks in func: %d, low %d, high %d\n",
		    count, low, high);
		DLIST_FOREACH(bb, &bblocks, bbelem) {
			printf("bb %p: first %p last %p\n", bb,
			    bb->first, bb->last);
		}
	}

	labinfo->low = low;
	labinfo->size = high - low + 1;
	labinfo->arr = tmpalloc(labinfo->size * sizeof(struct basicblock *));
	for (i = 0; i < labinfo->size; i++) {
		labinfo->arr[i] = NULL;
	}
	
	bbinfo->size = count + 1;
	bbinfo->arr = tmpalloc(bbinfo->size * sizeof(struct basicblock *));
	for (i = 0; i < bbinfo->size; i++) {
		bbinfo->arr[i] = NULL;
	}

	/* Build the label table */
	DLIST_FOREACH(bb, &bblocks, bbelem) {
		if (bb->first->type == IP_DEFLAB)
			labinfo->arr[bb->first->ip_lbl - low] = bb;
	}

	if (b2debug) {
		printf("Label table:\n");
		for (i = 0; i < labinfo->size; i++)
			if (labinfo->arr[i])
				printf("Label %d bblock %p\n", i+low,
				    labinfo->arr[i]);
	}
}

/*
 * Build the control flow graph.
 */

void
cfg_build(struct labelinfo *labinfo)
{
	/* Child and parent nodes */
	struct cfgnode *cnode; 
	struct cfgnode *pnode;
	struct basicblock *bb;
	
	DLIST_FOREACH(bb, &bblocks, bbelem) {

		if (bb->first->type == IP_EPILOG) {
			break;
		}

		cnode = tmpalloc(sizeof(struct cfgnode));
		pnode = tmpalloc(sizeof(struct cfgnode));
		pnode->bblock = bb;

		if ((bb->last->type == IP_NODE) && 
		    (bb->last->ip_node->n_op == GOTO) &&
		    (bb->last->ip_node->n_left->n_op == ICON))  {
			if (bb->last->ip_node->n_left->n_lval - labinfo->low > 
			    labinfo->size) {
				comperr("Label out of range: %d, base %d", 
					bb->last->ip_node->n_left->n_lval, 
					labinfo->low);
			}
			cnode->bblock = labinfo->arr[bb->last->ip_node->n_left->n_lval - labinfo->low];
			SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
			SLIST_INSERT_LAST(&bb->children, cnode, cfgelem);
			continue;
		}
		if ((bb->last->type == IP_NODE) && 
		    (bb->last->ip_node->n_op == CBRANCH)) {
			if (bb->last->ip_node->n_right->n_lval - labinfo->low > 
			    labinfo->size) 
				comperr("Label out of range: %d", 
					bb->last->ip_node->n_left->n_lval);
			
			cnode->bblock = labinfo->arr[bb->last->ip_node->n_right->n_lval - labinfo->low];
			SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
			SLIST_INSERT_LAST(&bb->children, cnode, cfgelem);
			cnode = tmpalloc(sizeof(struct cfgnode));
			pnode = tmpalloc(sizeof(struct cfgnode));
			pnode->bblock = bb;
		}

		cnode->bblock = DLIST_NEXT(bb, bbelem);
		SLIST_INSERT_LAST(&cnode->bblock->parents, pnode, cfgelem);
		SLIST_INSERT_LAST(&bb->children, cnode, cfgelem);
	}
}

void
cfg_dfs(struct basicblock *bb, unsigned int parent, struct bblockinfo *bbinfo)
{
	struct cfgnode *cnode;
	
	if (bb->dfnum != 0)
		return;

	bb->dfnum = ++dfsnum;
	bb->dfparent = parent;
	bbinfo->arr[bb->dfnum] = bb;
	SLIST_FOREACH(cnode, &bb->children, cfgelem) {
		cfg_dfs(cnode->bblock, bb->dfnum, bbinfo);
	}
	/* Don't bring in unreachable nodes in the future */
	bbinfo->size = dfsnum + 1;
}

static bittype *
setalloc(int nelem)
{
	bittype *b;
	int sz = (nelem+NUMBITS-1)/NUMBITS;

	b = tmpalloc(sz * sizeof(bittype));
	memset(b, 0, sz * sizeof(bittype));
	return b;
}

/*
 * Algorithm 19.9, pp 414 from Appel.
 */

void
dominators(struct bblockinfo *bbinfo)
{
	struct cfgnode *cnode;
	struct basicblock *bb, *y, *v;
	struct basicblock *s, *sprime, *p;
	int h, i;

	DLIST_FOREACH(bb, &bblocks, bbelem) {
		bb->bucket = setalloc(bbinfo->size);
		bb->df = setalloc(bbinfo->size);
		bb->dfchildren = setalloc(bbinfo->size);
	}

	dfsnum = 0;
	cfg_dfs(DLIST_NEXT(&bblocks, bbelem), 0, bbinfo);

	if (b2debug) {
		struct basicblock *bbb;
		struct cfgnode *ccnode;

		DLIST_FOREACH(bbb, &bblocks, bbelem) {
			printf("Basic block %d, parents: ", bbb->dfnum);
			SLIST_FOREACH(ccnode, &bbb->parents, cfgelem) {
				printf("%d, ", ccnode->bblock->dfnum);
			}
			printf("\nChildren: ");
			SLIST_FOREACH(ccnode, &bbb->children, cfgelem) {
				printf("%d, ", ccnode->bblock->dfnum);
			}
			printf("\n");
		}
	}

	for(h = bbinfo->size - 1; h > 1; h--) {
		bb = bbinfo->arr[h];
		p = s = bbinfo->arr[bb->dfparent];
		SLIST_FOREACH(cnode, &bb->parents, cfgelem) {
			if (cnode->bblock->dfnum <= bb->dfnum) 
				sprime = cnode->bblock;
			else 
				sprime = bbinfo->arr[ancestorwithlowestsemi
					      (cnode->bblock, bbinfo)->semi];
			if (sprime->dfnum < s->dfnum)
				s = sprime;
		}
		bb->semi = s->dfnum;
		BITSET(s->bucket, bb->dfnum);
		link(p, bb);
		for (i = 1; i < bbinfo->size; i++) {
			if(TESTBIT(p->bucket, i)) {
				v = bbinfo->arr[i];
				y = ancestorwithlowestsemi(v, bbinfo);
				if (y->semi == v->semi) 
					v->idom = p->dfnum;
				else
					v->samedom = y->dfnum;
			}
		}
		memset(p->bucket, 0, (bbinfo->size + 7)/8);
	}

	if (b2debug) {
		printf("Num\tSemi\tAncest\tidom\n");
		DLIST_FOREACH(bb, &bblocks, bbelem) {
			printf("%d\t%d\t%d\t%d\n", bb->dfnum, bb->semi,
			    bb->ancestor, bb->idom);
		}
	}

	for(h = 2; h < bbinfo->size; h++) {
		bb = bbinfo->arr[h];
		if (bb->samedom != 0) {
			bb->idom = bbinfo->arr[bb->samedom]->idom;
		}
	}
	DLIST_FOREACH(bb, &bblocks, bbelem) {
		if (bb->idom != 0 && bb->idom != bb->dfnum) {
			BDEBUG(("Setting child %d of %d\n",
			    bb->dfnum, bbinfo->arr[bb->idom]->dfnum));
			BITSET(bbinfo->arr[bb->idom]->dfchildren, bb->dfnum);
		}
	}
}


struct basicblock *
ancestorwithlowestsemi(struct basicblock *bblock, struct bblockinfo *bbinfo)
{
	struct basicblock *u = bblock;
	struct basicblock *v = bblock;

	while (v->ancestor != 0) {
		if (bbinfo->arr[v->semi]->dfnum < 
		    bbinfo->arr[u->semi]->dfnum) 
			u = v;
		v = bbinfo->arr[v->ancestor];
	}
	return u;
}

void
link(struct basicblock *parent, struct basicblock *child)
{
	child->ancestor = parent->dfnum;
}

void
computeDF(struct basicblock *bblock, struct bblockinfo *bbinfo)
{
	struct cfgnode *cnode;
	int h, i;
	
	SLIST_FOREACH(cnode, &bblock->children, cfgelem) {
		if (cnode->bblock->idom != bblock->dfnum)
			BITSET(bblock->df, cnode->bblock->dfnum);
	}
	for (h = 1; h < bbinfo->size; h++) {
		if (!TESTBIT(bblock->dfchildren, h))
			continue;
		computeDF(bbinfo->arr[h], bbinfo);
		for (i = 1; i < bbinfo->size; i++) {
			if (TESTBIT(bbinfo->arr[h]->df, i) && 
			    (bbinfo->arr[h] == bblock ||
			     (bblock->idom != bbinfo->arr[h]->dfnum))) 
			    BITSET(bblock->df, i);
		}
	}
}

static struct basicblock *currbb;
static struct interpass *currip;

/* Helper function for findTemps, Find assignment nodes. */
static void
searchasg(NODE *p)
{
	struct pvarinfo *pv;

	if (p->n_op != ASSIGN)
		return;

	if (p->n_left->n_op != TEMP)
		return;

	pv = tmpcalloc(sizeof(struct pvarinfo));
	pv->next = defsites.arr[p->n_left->n_lval];
	pv->bb = currbb;
	pv->top = currip->ip_node;
	pv->n = p->n_left;
	BITSET(currbb->Aorig, p->n_left->n_lval);

	defsites.arr[p->n_left->n_lval] = pv;
}

/* Walk the interpass looking for assignment nodes. */
void findTemps(struct interpass *ip)
{
	if (ip->type != IP_NODE)
		return;

	currip = ip;

	walkf(ip->ip_node, searchasg);
}

/*
 * Algorithm 19.6 from Appel.
 */

void
placePhiFunctions(struct bblockinfo *bbinfo)
{
	struct basicblock *bb;
	struct interpass *ip;
	int maxtmp, i, j, k, l;
	struct pvarinfo *n;
	struct cfgnode *cnode;
	TWORD ntype;
	NODE *p;
	struct pvarinfo *pv;

	bb = DLIST_NEXT(&bblocks, bbelem);
	defsites.low = ((struct interpass_prolog *)bb->first)->ip_tmpnum;
	bb = DLIST_PREV(&bblocks, bbelem);
	maxtmp = ((struct interpass_prolog *)bb->first)->ip_tmpnum;
	defsites.size = maxtmp - defsites.low + 1;
	defsites.arr = tmpcalloc(defsites.size*sizeof(struct pvarinfo *));

	/* Find all defsites */
	DLIST_FOREACH(bb, &bblocks, bbelem) {
		currbb = bb;
		ip = bb->first;
		bb->Aorig = setalloc(defsites.size);
		bb->Aphi = setalloc(defsites.size);
		

		while (ip != bb->last) {
			findTemps(ip);
			ip = DLIST_NEXT(ip, qelem);
		}
		/* Make sure we get the last statement in the bblock */
		findTemps(ip);
	}
	/* For each variable */
	for (i = defsites.low; i < defsites.size; i++) {
		/* While W not empty */
		while (defsites.arr[i] != NULL) {
			/* Remove some node n from W */
			n = defsites.arr[i];
			defsites.arr[i] = n->next;
			/* For each y in n->bb->df */
			for (j = 0; j < bbinfo->size; j++) {
				if (!TESTBIT(n->bb->df, j))
					continue;
				
				if (TESTBIT(bbinfo->arr[j]->Aphi, i))
					continue;

				ntype = n->n->n_type;
				k = 0;
				/* Amount of predecessors for y */
				SLIST_FOREACH(cnode, &n->bb->parents, cfgelem) 
					k++;
				/* Construct phi(...) */
				p = mktemp(i, ntype);
				for (l = 0; l < k-1; l++)
					p = mkbinode(PHI, p,
					    mktemp(i, ntype), ntype);
				ip = ipnode(mkbinode(ASSIGN,
				    mktemp(i, ntype), p, ntype));
				/* Insert phi at top of basic block */
				DLIST_INSERT_BEFORE(((struct interpass*)&n->bb->first), ip, qelem);
				n->bb->first = ip;
				BITSET(bbinfo->arr[j]->Aphi, i);
				if (!TESTBIT(bbinfo->arr[j]->Aorig, i)) {
					pv = tmpalloc(sizeof(struct pvarinfo));
					// XXXpj Ej fullständig information.
					pv->bb = bbinfo->arr[j];
					pv->next = defsites.arr[i]->next;
					defsites.arr[i] = pv;
				}
					

			}
		}
	}
}

/*
 * Remove unreachable nodes in the CFG.
 */ 

void
remunreach(void)
{
	struct basicblock *bb, *nbb;
	struct interpass *next, *ctree;

	bb = DLIST_NEXT(&bblocks, bbelem);
	while (bb != &bblocks) {
		nbb = DLIST_NEXT(bb, bbelem);

		/* Code with dfnum 0 is unreachable */
		if (bb->dfnum != 0) {
			bb = nbb;
			continue;
		}

		/* Need the epilogue node for other parts of the
		   compiler, set its label to 0 and backend will
		   handle it. */ 
		if (bb->first->type == IP_EPILOG) {
			bb->first->ip_lbl = 0;
			bb = nbb;
			continue;
		}

		next = bb->first;
		do {
			ctree = next;
			next = DLIST_NEXT(ctree, qelem);
			
			if (ctree->type == IP_NODE)
				tfree(ctree->ip_node);
			DLIST_REMOVE(ctree, qelem);
		} while (ctree != bb->last);
			
		DLIST_REMOVE(bb, bbelem);
		bb = nbb;
	}
}

void
printip(struct interpass *pole)
{
	static char *foo[] = {
	   0, "NODE", "PROLOG", "STKOFF", "EPILOG", "DEFLAB", "DEFNAM", "ASM" };
	struct interpass *ip;
	struct interpass_prolog *ipplg, *epplg;

	DLIST_FOREACH(ip, pole, qelem) {
		if (ip->type > MAXIP)
			printf("IP(%d) (%p): ", ip->type, ip);
		else
			printf("%s (%p): ", foo[ip->type], ip);
		switch (ip->type) {
		case IP_NODE: printf("\n");
#ifdef PCC_DEBUG
			fwalk(ip->ip_node, e2print, 0); break;
#endif
		case IP_PROLOG:
			ipplg = (struct interpass_prolog *)ip;
			printf("%s %s regs %x autos %d mintemp %d minlbl %d\n",
			    ipplg->ipp_name, ipplg->ipp_vis ? "(local)" : "",
			    ipplg->ipp_regs, ipplg->ipp_autos, ipplg->ip_tmpnum,
			    ipplg->ip_lblnum);
			break;
		case IP_EPILOG:
			epplg = (struct interpass_prolog *)ip;
			printf("%s %s regs %x autos %d mintemp %d minlbl %d\n",
			    epplg->ipp_name, epplg->ipp_vis ? "(local)" : "",
			    epplg->ipp_regs, epplg->ipp_autos, epplg->ip_tmpnum,
			    epplg->ip_lblnum);
			break;
		case IP_DEFLAB: printf(LABFMT "\n", ip->ip_lbl); break;
		case IP_DEFNAM: printf("\n"); break;
		case IP_ASM: printf("%s\n", ip->ip_asm); break;
		default:
			break;
		}
	}
}
