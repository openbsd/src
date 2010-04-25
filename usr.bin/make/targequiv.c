/*	$OpenBSD: targequiv.c,v 1.2 2010/04/25 13:59:53 espie Exp $ */
/*
 * Copyright (c) 2007-2008 Marc Espie.
 *
 * Extensive code changes for the OpenBSD project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Compute equivalence tables of targets, helpful for VPATH and parallel
 * make.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/param.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "memory.h"
#include "ohash.h"
#include "gnode.h"
#include "lst.h"
#include "suff.h"
#include "dir.h"
#include "targ.h"
#include "targequiv.h"

struct equiv_list {
	GNode *first, *last;
	char name[1];
};

static struct ohash_info equiv_info = {
	offsetof(struct equiv_list, name), NULL, hash_alloc, hash_free,
	element_alloc
};

static void attach_node(GNode *, GNode *);
static void build_equivalence(void);
static void add_to_equiv_list(struct ohash *, GNode *);
static char *names_match(GNode *, GNode *);
static char *names_match_with_dir(const char *, const char *, char *,
    char *,  const char *);
static char *names_match_with_dirs(const char *, const char *, char *,
    char *,  const char *, const char *);
static char *relative_reduce(const char *, const char *);
static char *relative_reduce2(const char *, const char *, const char *);
static char *absolute_reduce(const char *);
static size_t parse_reduce(size_t, const char *);
static void find_siblings(GNode *);

/* These functions build `equivalence lists' of targets with the same
 * basename, as circular lists. They use an intermediate ohash as scaffold,
 * to insert same-basename targets in a simply linked list. Then they make
 * those lists circular, to the exception of lone elements, since they can't
 * alias to anything.
 *
 * This structure is used to simplify alias-lookup to a great extent: two
 * nodes can only alias each other if they're part of the same equivalence
 * set. Most nodes either don't belong to an alias set, or to a very simple
 * alias set, thus removing most possibilities.
 */
static void
add_to_equiv_list(struct ohash *equiv, GNode *gn)
{
	const char *end = NULL;
	struct equiv_list *e;
	unsigned int slot;

	if (!should_have_file(gn))
		return;

	gn->basename = strrchr(gn->name, '/');
	if (gn->basename == NULL)
		gn->basename = gn->name;
	else
		gn->basename++;
	slot = ohash_qlookupi(equiv, gn->basename, &end);
	e = ohash_find(equiv, slot);
	if (e == NULL) {
		e = ohash_create_entry(&equiv_info, gn->basename, &end);
		e->first = NULL;
		e->last = gn;
		ohash_insert(equiv, slot, e);
	}
	gn->next = e->first;
	e->first = gn;
}

static void
build_equivalence()
{
	unsigned int i;
	GNode *gn;
	struct equiv_list *e;
	struct ohash equiv;
	struct ohash *t = targets_hash();


	ohash_init(&equiv, 10, &equiv_info);

	for (gn = ohash_first(t, &i); gn != NULL; gn = ohash_next(t, &i))
		add_to_equiv_list(&equiv, gn);

	/* finish making the lists circular */
	for (e = ohash_first(&equiv, &i); e != NULL;
	    e = ohash_next(&equiv, &i)) {
	    	if (e->last != e->first)
			e->last->next = e->first;
#ifdef CLEANUP
		free(e);
#endif
	}
#ifdef CLEANUP
	ohash_delete(&equiv);
#endif
}

static const char *curdir, *objdir;
static char *kobjdir;
static size_t objdir_len;

void
Targ_setdirs(const char *c, const char *o)
{
	curdir = c;
	objdir = o;

	objdir_len = strlen(o);
	kobjdir = emalloc(objdir_len+2);
	memcpy(kobjdir, o, objdir_len);
	kobjdir[objdir_len++] = '/';
	kobjdir[objdir_len] = 0;
}


void
kludge_look_harder_for_target(GNode *gn)
{
	GNode *extra, *cgn;
	LstNode ln;

	if (strncmp(gn->name, kobjdir, objdir_len) == 0) {
		extra = Targ_FindNode(gn->name + objdir_len, TARG_NOCREATE);
		if (extra != NULL) {
			if (Lst_IsEmpty(&gn->commands))
				Lst_Concat(&gn->commands, &extra->commands);
			for (ln = Lst_First(&extra->children); ln != NULL;
			    ln = Lst_Adv(ln)) {
				cgn = (GNode *)Lst_Datum(ln);

				if (Lst_AddNew(&gn->children, cgn)) {
					Lst_AtEnd(&cgn->parents, gn);
					gn->unmade++;
				}
			}
		}
	}
}

static void
attach_node(GNode *gn, GNode *extra)
{
	/* XXX normally extra->sibling == extra, but this is not
	 * always the case yet, so we merge the two lists
	 */
	GNode *tmp;

	tmp = gn->sibling;
	gn->sibling = extra->sibling;
	extra->sibling = tmp;
}

static char *buffer = NULL;
static size_t bufsize = MAXPATHLEN;

static size_t
parse_reduce(size_t i, const char *src)
{
	while (src[0] != 0) {
		while (src[0] == '/')
			src++;
		/* special cases */
		if (src[0] == '.') {
			if (src[1] == '/') {
				src += 2;
				continue;
			}
			if (src[1] == '.' && src[2] == '/') {
				src += 3;
				i--;
				while (i > 0 && buffer[i-1] != '/')
					i--;
				if (i == 0)
					i = 1;
				continue;
			}
		}
		while (src[0] != '/' && src[0] != 0) {
			if (i > bufsize - 2) {
				bufsize *= 2;
				buffer = erealloc(buffer, bufsize);
			}
			buffer[i++] = *src++;
		}
		buffer[i++] = *src;
	}
	return i;
}

static char *
absolute_reduce(const char *src)
{
	size_t i = 0;

	if (buffer == NULL)
		buffer = emalloc(bufsize);

	buffer[i++] = '/';
	i = parse_reduce(i, src);
	return estrdup(buffer);
}

static char *
relative_reduce(const char *dir, const char *src)
{
	size_t i = 0;

	if (buffer == NULL)
		buffer = emalloc(bufsize);

	buffer[i++] = '/';
	i = parse_reduce(i, dir);
	i--;

	if (buffer[i-1] != '/')
		buffer[i++] = '/';
	i = parse_reduce(i, src);
	return estrdup(buffer);
}

static char *
relative_reduce2(const char *dir1, const char *dir2, const char *src)
{
	size_t i = 0;

	if (buffer == NULL)
		buffer = emalloc(bufsize);

	buffer[i++] = '/';
	i = parse_reduce(i, dir1);
	i--;
	if (buffer[i-1] != '/')
		buffer[i++] = '/';

	i = parse_reduce(i, dir2);
	i--;
	if (buffer[i-1] != '/')
		buffer[i++] = '/';

	i = parse_reduce(i, src);
	return estrdup(buffer);
}

static char *
names_match_with_dir(const char *a, const char *b, char *ra,
    char *rb,  const char *dir)
{
	bool r;
	bool free_a, free_b;

	if (ra == NULL) {
		ra = relative_reduce(dir, a);
		free_a = true;
	} else {
		free_a = false;
	}

	if (rb == NULL) {
		rb = relative_reduce(dir, b);
		free_b = true;
	} else {
		free_b = false;
	}
	r = strcmp(ra, rb) == 0;
	if (free_a)
		free(ra);
	if (r)
		return rb;
	else {
		if (free_b)
			free(rb);
		return NULL;
	}
}

static char *
names_match_with_dirs(const char *a, const char *b, char *ra,
    char *rb,  const char *dir1, const char *dir2)
{
	bool r;
	bool free_a, free_b;

	if (ra == NULL) {
		ra = relative_reduce2(dir1, dir2, a);
		free_a = true;
	} else {
		free_a = false;
	}

	if (rb == NULL) {
		rb = relative_reduce2(dir1, dir2, b);
		free_b = true;
	} else {
		free_b = false;
	}
	r = strcmp(ra, rb) == 0;
	if (free_a)
		free(ra);
	if (r)
		return rb;
	else {
		if (free_b)
			free(rb);
		return NULL;
	}
}

static char *
names_match(GNode *a, GNode *b)
{
	char *ra = NULL , *rb = NULL;
	char *r;

	if (a->name[0] == '/')
		ra = absolute_reduce(a->name);
	if (b->name[0] == '/')
		rb = absolute_reduce(b->name);
	if (ra && rb) {
		if (strcmp(ra, rb) == 0)
			r = rb;
		else
			r = NULL;
	} else {
		r = names_match_with_dir(a->name, b->name, ra, rb, objdir);
		if (!r)
			r = names_match_with_dir(a->name, b->name, ra, rb,
			    curdir);
		if (!r) {
			/* b has necessarily the same one */
			Lst l = find_suffix_path(a);
			LstNode ln;

			for (ln = Lst_First(l); ln != NULL; ln = Lst_Adv(ln)) {
				const char *p = PathEntry_name(Lst_Datum(ln));
				if (p[0] == '/') {
					r = names_match_with_dir(a->name,
					    b->name, ra, rb, p);
					if (r)
						break;
				} else {
					r = names_match_with_dirs(a->name,
					    b->name, ra, rb, p, objdir);
					if (r)
						break;
					r = names_match_with_dirs(a->name,
					    b->name, ra, rb, p, curdir);
					if (r)
						break;
				}
			}
		}
	}
	free(ra);
	if (r != rb)
		free(rb);
	return r;
}

static void
find_siblings(GNode *gn)
{
	GNode *gn2;
	char *fullpath;

	/* not part of an equivalence class: can't alias */
	if (gn->next == NULL)
		return;
	/* already resolved, actually */
	if (gn->sibling != gn)
		return;
	if (DEBUG(NAME_MATCHING))
		fprintf(stderr, "Matching for %s:", gn->name);
	/* look through the aliases */
	for (gn2 = gn->next; gn2 != gn; gn2 = gn2->next) {
		fullpath = names_match(gn, gn2);
		if (fullpath) {
			attach_node(gn, gn2);
		} else {
			if (DEBUG(NAME_MATCHING))
				fputc('!', stderr);
		}
		if (DEBUG(NAME_MATCHING))
			fprintf(stderr, "%s ", gn2->name);
	}
	if (DEBUG(NAME_MATCHING))
		fputc('\n', stderr);
}

void
look_harder_for_target(GNode *gn)
{
	static bool equiv_was_built = false;

	if (!equiv_was_built) {
		build_equivalence();
		equiv_was_built = true;
	}

	if (gn->type & (OP_RESOLVED | OP_PHONY))
		return;
	gn->type |= OP_RESOLVED;
	find_siblings(gn);
}

bool
is_sibling(GNode *gn, GNode *gn2)
{
	GNode *sibling;

	sibling = gn;
	do {
		if (sibling == gn2)
			return true;
		sibling = sibling->sibling;
	} while (sibling != gn);

	return false;
}
