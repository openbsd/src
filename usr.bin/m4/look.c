/*	$OpenBSD: look.c,v 1.13 2003/06/30 22:10:21 espie Exp $	*/

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
#include <ohash.h>
#include "mdef.h"
#include "stdd.h"
#include "extern.h"

struct ndblock {			/* hashtable structure         */
	struct macro_definition *d;
	char		name[1];	/* entry name..               */
};
 
extern void *hash_alloc(size_t, void *);
extern void hash_free(void *, size_t, void *);
extern void *element_alloc(size_t, void *);
static void setup_definition(struct macro_definition *, const char *, 
    const char *);

static struct ohash_info macro_info = {
	offsetof(struct ndblock, name),
	NULL, hash_alloc, hash_free, element_alloc };

static struct ohash macros;

void
init_macros()
{
	ohash_init(&macros, 7, &macro_info);
}

/*
 * find name in the hash table
 */
ndptr 
lookup(const char *name)
{
	return ohash_find(&macros, ohash_qlookup(&macros, name));
}

struct macro_definition *
lookup_macro_definition(const char *name)
{
	ndptr p;

	p = lookup(name);
	if (p)
		return p->d;
	else
		return NULL;
}

static void 
setup_definition(struct macro_definition *d, const char *defn, const char *name)
{
	int n;

	if (strncmp(defn, BUILTIN_MARKER, sizeof(BUILTIN_MARKER)-1) == 0 &&
	    (n = builtin_type(defn+sizeof(BUILTIN_MARKER)-1)) != -1) {
		d->type = n & TYPEMASK;
		if ((n & NOARGS) == 0)
			d->type |= NEEDARGS;
		d->defn = xstrdup(defn+sizeof(BUILTIN_MARKER)-1);
	} else {
		if (!*defn)
			d->defn = null;
		else
			d->defn = xstrdup(defn);
		d->type = MACRTYPE;
	}
	if (STREQ(name, defn))
		d->type |= RECDEF;
}

static ndptr
create_entry(const char *name)
{
	const char *end = NULL;
	unsigned int i;
	ndptr n;

	i = ohash_qlookupi(&macros, name, &end);
	n = ohash_find(&macros, i);
	if (n == NULL) {
		n = ohash_create_entry(&macro_info, name, &end);
		ohash_insert(&macros, i, n);
		n->d = NULL;
	}
	return n;
}

void
macro_define(const char *name, const char *defn)
{
	ndptr n = create_entry(name);
	if (n->d != NULL) {
		if (n->d->defn != null)
			free(n->d->defn);
	} else {
		n->d = xalloc(sizeof(struct macro_definition));
		n->d->next = NULL;
	}
	setup_definition(n->d, defn, name);
}

void
macro_pushdef(const char *name, const char *defn)
{
	ndptr n;
	struct macro_definition *d;
	
	n = create_entry(name);
	d = xalloc(sizeof(struct macro_definition));
	d->next = n->d;
	n->d = d;
	setup_definition(n->d, defn, name);
}

void
macro_undefine(const char *name)
{
	ndptr n = lookup(name);
	if (n != NULL) {
		struct macro_definition *r, *r2;

		for (r = n->d; r != NULL; r = r2) {
			r2 = r->next;
			if (r->defn != null)
				free(r->defn);
			free(r);
		}
		n->d = NULL;
	}
}

void
macro_popdef(const char *name)
{
	ndptr n = lookup(name);

	if (n != NULL) {
		struct macro_definition *r = n->d;
		if (r != NULL) {
			n->d = r->next;
			if (r->defn != null)
				free(r->defn);
			free(r);
		}
	}
}

void
macro_for_all(void (*f)(const char *, struct macro_definition *))
{
	ndptr n;
	unsigned int i;

	for (n = ohash_first(&macros, &i); n != NULL; 
	    n = ohash_next(&macros, &i))
		f(n->name, n->d);
}

void 
setup_builtin(const char *name, unsigned int type)
{
	ndptr n;

	n = create_entry(name);
	n->d = xalloc(sizeof(struct macro_definition));
	n->d->defn = xstrdup(name);
	n->d->type = type;
	n->d->next = NULL;
}

const char *
macro_name(ndptr p)
{
	return p->name;
}

struct macro_definition *
macro_getdef(ndptr p)
{
	return p->d;
}
