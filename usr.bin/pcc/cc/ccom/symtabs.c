/*	$Id: symtabs.c,v 1.1.1.1 2007/09/15 18:12:35 otto Exp $	*/
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


#include "pass1.h"

/*
 * These definitions are used in the patricia tree that stores
 * the strings.
 */
#define	LEFT_IS_LEAF		0x80000000
#define	RIGHT_IS_LEAF		0x40000000
#define	IS_LEFT_LEAF(x)		(((x) & LEFT_IS_LEAF) != 0)
#define	IS_RIGHT_LEAF(x)	(((x) & RIGHT_IS_LEAF) != 0)
#define	BITNO(x)		((x) & ~(LEFT_IS_LEAF|RIGHT_IS_LEAF))
#define	CHECKBITS		8

struct tree {
	int bitno;
	struct tree *lr[2];
};

static struct tree *firstname;
int nametabs, namestrlen;
static struct tree *firststr;
int strtabs, strstrlen;
static char *symtab_add(char *key, struct tree **, int *, int *);

#define	P_BIT(key, bit) (key[bit >> 3] >> (bit & 7)) & 1
#define	getree() permalloc(sizeof(struct tree))

char *
addname(char *key)      
{
	return symtab_add(key, &firstname, &nametabs, &namestrlen);
}

char *
addstring(char *key)
{
	return symtab_add(key, &firststr, &strtabs, &strstrlen);
}

/*
 * Add a name to the name stack (if its non-existing),
 * return its address.
 * This is a simple patricia implementation.
 */
char *
symtab_add(char *key, struct tree **first, int *tabs, int *stlen)
{
	struct tree *w, *new, *last;
	int cix, bit, fbit, svbit, ix, bitno, len;
	char *m, *k, *sm;

	/* Count full string length */
	for (k = key, len = 0; *k; k++, len++)
		;
	
	switch (*tabs) {
	case 0:
		*first = (struct tree *)newstring(key, len);
		*stlen += (len + 1);
		(*tabs)++;
		return (char *)*first;

	case 1:
		m = (char *)*first;
		svbit = 0; /* XXX why? */
		break;

	default:
		w = *first;
		bitno = len * CHECKBITS;
		for (;;) {
			bit = BITNO(w->bitno);
			fbit = bit > bitno ? 0 : P_BIT(key, bit);
			svbit = fbit ? IS_RIGHT_LEAF(w->bitno) :
			    IS_LEFT_LEAF(w->bitno);
			w = w->lr[fbit];
			if (svbit) {
				m = (char *)w;
				break;
			}
		}
	}

	sm = m;
	k = key;

	/* Check for correct string and return */
	for (cix = 0; *m && *k && *m == *k; m++, k++, cix += CHECKBITS)
		;
	if (*m == 0 && *k == 0)
		return sm;

	ix = *m ^ *k;
	while ((ix & 1) == 0)
		ix >>= 1, cix++;

	/* Create new node */
	new = getree();
	bit = P_BIT(key, cix);
	new->bitno = cix | (bit ? RIGHT_IS_LEAF : LEFT_IS_LEAF);
	new->lr[bit] = (struct tree *)newstring(key, len);
	*stlen += (len + 1);

	if ((*tabs)++ == 1) {
		new->lr[!bit] = *first;
		new->bitno |= (bit ? LEFT_IS_LEAF : RIGHT_IS_LEAF);
		*first = new;
		return (char *)new->lr[bit];
	}


	w = *first;
	last = NULL;
	for (;;) {
		fbit = w->bitno;
		bitno = BITNO(w->bitno);
		if (bitno == cix)
			cerror("bitno == cix");
		if (bitno > cix)
			break;
		svbit = P_BIT(key, bitno);
		last = w;
		w = w->lr[svbit];
		if (fbit & (svbit ? RIGHT_IS_LEAF : LEFT_IS_LEAF))
			break;
	}

	new->lr[!bit] = w;
	if (last == NULL) {
		*first = new;
	} else {
		last->lr[svbit] = new;
		last->bitno &= ~(svbit ? RIGHT_IS_LEAF : LEFT_IS_LEAF);
	}
	if (bitno < cix)
		new->bitno |= (bit ? LEFT_IS_LEAF : RIGHT_IS_LEAF);
	return (char *)new->lr[bit];
}

static struct tree *sympole[NSTYPES];
static struct symtab *tmpsyms[NSTYPES];
int numsyms[NSTYPES];

/*
 * Inserts a symbol into the symbol tree.
 * Returns a struct symtab.
 */
struct symtab *
lookup(char *key, int ttype)
{
	struct symtab *sym;
	struct tree *w, *new, *last;
	int cix, bit, fbit, svbit, ix, bitno, match;
	int type, uselvl;

	long code = (long)key;
	type = ttype & SMASK;
	uselvl = (blevel > 0 && type != SSTRING);

	/*
	 * The local symbols are kept in a simple linked list.
	 * Check this list first.
	 */
	if (blevel > 0)
		for (sym = tmpsyms[type]; sym; sym = sym->snext)
			if (sym->sname == key)
				return sym;

	switch (numsyms[type]) {
	case 0:
		if (ttype & SNOCREAT)
			return NULL;
		if (uselvl) {
			sym = getsymtab(key, ttype|STEMP);
			sym->snext = tmpsyms[type];
			tmpsyms[type] = sym;
			return sym;
		}
		sympole[type] = (struct tree *)getsymtab(key, ttype);
		numsyms[type]++;
		return (struct symtab *)sympole[type];

	case 1:
		w = (struct tree *)sympole[type];
		svbit = 0; /* XXX why? */
		break;

	default:
		w = sympole[type];
		for (;;) {
			bit = BITNO(w->bitno);
			fbit = (code >> bit) & 1;
			svbit = fbit ? IS_RIGHT_LEAF(w->bitno) :
			    IS_LEFT_LEAF(w->bitno);
			w = w->lr[fbit];
			if (svbit)
				break;
		}
	}

	sym = (struct symtab *)w;
	match = (long)sym->sname;

	ix = code ^ match;
	if (ix == 0)
		return sym;
	else if (ttype & SNOCREAT)
		return NULL;

#ifdef PCC_DEBUG
	if (ddebug)
		printf("	adding %s as %s at level %d\n",
		    key, uselvl ? "temp" : "perm", blevel);
#endif

	/*
	 * Insert into the linked list, if feasible.
	 */
	if (uselvl) {
		sym = getsymtab(key, ttype|STEMP);
		sym->snext = tmpsyms[type];
		tmpsyms[type] = sym;
		return sym;
	}

	/*
	 * Need a new node. If type is SNORMAL and inside a function
	 * the node must be allocated as permanent anyway.
	 * This could be optimized by adding a remove routine, but it
	 * may be more trouble than it is worth.
	 */
	if (ttype == (STEMP|SNORMAL))
		ttype = SNORMAL;

	for (cix = 0; (ix & 1) == 0; ix >>= 1, cix++)
		;

	new = ttype & STEMP ? tmpalloc(sizeof(struct tree)) :
	    permalloc(sizeof(struct tree));
	bit = (code >> cix) & 1;
	new->bitno = cix | (bit ? RIGHT_IS_LEAF : LEFT_IS_LEAF);
	new->lr[bit] = (struct tree *)getsymtab(key, ttype);
	if (numsyms[type]++ == 1) {
		new->lr[!bit] = sympole[type];
		new->bitno |= (bit ? LEFT_IS_LEAF : RIGHT_IS_LEAF);
		sympole[type] = new;
		return (struct symtab *)new->lr[bit];
	}


	w = sympole[type];
	last = NULL;
	for (;;) {
		fbit = w->bitno;
		bitno = BITNO(w->bitno);
		if (bitno == cix)
			cerror("bitno == cix");
		if (bitno > cix)
			break;
		svbit = (code >> bitno) & 1;
		last = w;
		w = w->lr[svbit];
		if (fbit & (svbit ? RIGHT_IS_LEAF : LEFT_IS_LEAF))
			break;
	}

	new->lr[!bit] = w;
	if (last == NULL) {
		sympole[type] = new;
	} else {
		last->lr[svbit] = new;
		last->bitno &= ~(svbit ? RIGHT_IS_LEAF : LEFT_IS_LEAF);
	}
	if (bitno < cix)
		new->bitno |= (bit ? LEFT_IS_LEAF : RIGHT_IS_LEAF);
	return (struct symtab *)new->lr[bit];
}

void
symclear(int level)
{
	struct symtab *s;
	int i;

#ifdef PCC_DEBUG
	if (ddebug)
		printf("symclear(%d)\n", level);
#endif
	if (level < 1) {
		for (i = 0; i < NSTYPES; i++) {
			s = tmpsyms[i];
			tmpsyms[i] = 0;
			if (i != SLBLNAME)
				continue;
			while (s != NULL) {
				if (s->soffset < 0)
					uerror("label '%s' undefined",s->sname);
				s = s->snext;
			}
		}
	} else {
		for (i = 0; i < NSTYPES; i++) {
			if (i == SLBLNAME)
				continue; /* function scope */
			while (tmpsyms[i] != NULL &&
			    tmpsyms[i]->slevel > level) {
				tmpsyms[i] = tmpsyms[i]->snext;
			}
		}
	}
}

struct symtab *
hide(struct symtab *sym)
{
	struct symtab *new;

	new = getsymtab(sym->sname, SNORMAL|STEMP);
	new->snext = tmpsyms[SNORMAL];
	tmpsyms[SNORMAL] = new;
#ifdef PCC_DEBUG
	if (ddebug)
		printf("\t%s hidden at level %d (%p -> %p)\n",
		    sym->sname, blevel, sym, new);
#endif
	return new;
}
