/*	$OpenBSD: look.c,v 1.12 2003/06/30 21:42:50 espie Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)look.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * look.c
 * Facility: m4 macro processor
 * by: oz
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "mdef.h"
#include "stdd.h"
#include "extern.h"

struct ndblock {		/* hastable structure         */
	char		*name;	/* entry name..               */
	struct macro_definition d;
	unsigned int 	hv;	/* hash function value..      */
	ndptr		nxtptr;	/* link to next entry..       */
};
 
static void freent(ndptr);
static void	remhash(const char *, int);
static unsigned	hash(const char *);
static ndptr	addent(const char *);

static unsigned int
hash(const char *name)
{
	unsigned int h = 0;
	while (*name)
		h = (h << 5) + h + *name++;
	return (h);
}

/*
 * find name in the hash table
 */
ndptr 
lookup(const char *name)
{
	ndptr p;
	unsigned int h;

	h = hash(name);
	for (p = hashtab[h % HASHSIZE]; p != NULL; p = p->nxtptr)
		if (h == p->hv && STREQ(name, p->name))
			break;
	return (p);
}

/*
 * hash and create an entry in the hash table.
 * The new entry is added in front of a hash bucket.
 */
static ndptr 
addent(const char *name)
{
	unsigned int h;
	ndptr p;

	h = hash(name);
	p = (ndptr) xalloc(sizeof(struct ndblock));
	p->nxtptr = hashtab[h % HASHSIZE];
	hashtab[h % HASHSIZE] = p;
	p->name = xstrdup(name);
	p->hv = h;
	return p;
}

static void
freent(ndptr p)
{
	free((char *) p->name);
	if (p->d.defn != null)
		free((char *) p->d.defn);
	free((char *) p);
}

/*
 * remove an entry from the hashtable
 */
static void
remhash(const char *name, int all)
{
	unsigned int h;
	ndptr xp, tp, mp;

	h = hash(name);
	mp = hashtab[h % HASHSIZE];
	tp = NULL;
	while (mp != NULL) {
		if (mp->hv == h && STREQ(mp->name, name)) {
			mp = mp->nxtptr;
			if (tp == NULL) {
				freent(hashtab[h % HASHSIZE]);
				hashtab[h % HASHSIZE] = mp;
			}
			else {
				xp = tp->nxtptr;
				tp->nxtptr = mp;
				freent(xp);
			}
			if (!all)
				break;
		}
		else {
			tp = mp;
			mp = mp->nxtptr;
		}
	}
}

struct macro_definition *
lookup_macro_definition(const char *name)
{
	ndptr p;

	p = lookup(name);
	if (p)
		return &(p->d);
	else
		return NULL;
}

static void 
setup_definition(struct macro_definition *d, const char *defn)
{
	int n;

	if (strncmp(defn, BUILTIN_MARKER, sizeof(BUILTIN_MARKER)-1) == 0) {
		n = builtin_type(defn+sizeof(BUILTIN_MARKER)-1);
		if (n != -1) {
			d->type = n & TYPEMASK;
			if ((n & NOARGS) == 0)
				d->type |= NEEDARGS;
			d->defn = xstrdup(defn+sizeof(BUILTIN_MARKER)-1);
			return;
		}
	}
	if (!*defn)
		d->defn = null;
	else
		d->defn = xstrdup(defn);
	d->type = MACRTYPE;
}

void
macro_define(const char *name, const char *defn)
{
	ndptr p;

	if ((p = lookup(name)) == NULL)
		p = addent(name);
	else if (p->d.defn != null)
		free((char *) p->d.defn);
	setup_definition(&(p->d), defn);
	if (STREQ(name, defn))
		p->d.type |= RECDEF;
}

void
macro_pushdef(const char *name, const char *defn)
{
	ndptr p;

	p = addent(name);
	setup_definition(&(p->d), defn);
	if (STREQ(name, defn))
		p->d.type |= RECDEF;
}

void
macro_undefine(const char *name)
{
	remhash(name, ALL);
}

void
macro_popdef(const char *name)
{
	remhash(name, TOP);
}

void
macro_for_all(void (*f)(const char *, struct macro_definition *))
{
	int n;
	ndptr p;

	for (n = 0; n < HASHSIZE; n++)
		for (p = hashtab[n]; p != NULL; p = p->nxtptr)
			f(p->name, &(p->d));
}

void 
setup_builtin(const char *name, unsigned int type)
{
	unsigned int h;
	ndptr p;

	h = hash(name);
	p = (ndptr) xalloc(sizeof(struct ndblock));
	p->nxtptr = hashtab[h % HASHSIZE];
	hashtab[h % HASHSIZE] = p;
	p->name = xstrdup(name);
	p->d.defn = xstrdup(name);
	p->hv = h;
	p->d.type = type;
}

const char *
macro_name(ndptr p)
{
	return p->name;
}

struct macro_definition *
macro_getdef(ndptr p)
{
	return &(p->d);
}
