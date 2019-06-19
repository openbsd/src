/*	$OpenBSD: suff.c,v 1.95 2018/11/13 14:51:35 espie Exp $ */
/*	$NetBSD: suff.c,v 1.13 1996/11/06 17:59:25 christos Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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

/*-
 * suff.c --
 *	Functions to maintain suffix lists and find implicit dependents
 *	using suffix transformation rules
 *
 * Interface:
 *	Suff_Init		Initialize all things to do with suffixes.
 *
 *	Suff_ClearSuffixes	Clear out all the suffixes.
 *
 *	Suff_AddSuffix		Add the passed string as another known suffix.
 *
 *	Suff_ParseAsTransform	Line might be a suffix line, check it.
 *				If it's not, return NULL. Otherwise, add
 *				another transformation to the suffix graph.
 *				Returns	GNode suitable for framing, I mean,
 *				tacking commands, attributes, etc. on.
 *
 *	Suff_FindDeps		Find implicit sources for and the location of
 *				a target based on its suffix. Returns the
 *				bottom-most node added to the graph or NULL
 *				if the target had no implicit sources.
 */

#include <ctype.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ohash.h>
#include "config.h"
#include "defines.h"
#include "dir.h"
#include "direxpand.h"
#include "engine.h"
#include "arch.h"
#include "suff.h"
#include "var.h"
#include "targ.h"
#include "error.h"
#include "str.h"
#include "lst.h"
#include "memory.h"
#include "gnode.h"
#include "make.h"
#include "stats.h"
#include "dump.h"

/* XXX the suffixes hash is stored using a specific hash function, suitable
 * for looking up suffixes in reverse.
 */
static struct ohash suffixes;

/* We remember the longest suffix, so we don't need to look beyond that.  */
size_t maxLen;
static LIST srclist;

/* Transforms (.c.o) are stored in another hash, independently from suffixes.
 * When make sees a target, it checks whether it's currently parsable as a
 * transform (according to the active suffixes). If yes, it's stored as a
 * new transform.
 *
 * XXX
 * But transforms DO NOT have a canonical decomposition as a set of suffixes,
 * and will be used as necessary later, when looking up implicit rules for
 * actual targets.
 *
 * For instance, a transform .a.b.c  can be parsed as .a -> .b.c if suffixes
 * .a and .b.c are active, and then LATER, reused as .a.b -> .c if suffixes
 * .a.b and .c are active.
 */
static struct ohash transforms;

/* conflicts between suffixes are solved by suffix declaration order. */
static int order = 0;

/*
 * Structure describing an individual suffix.
 */
struct Suff_ {
	size_t nameLen;		/* optimisation: strlen(name) */
	short flags;
#define SUFF_ACTIVE	  0x08	/* We never destroy suffixes and rules, */
				/* we just deactivate them. */
#define SUFF_PATH	  0x10	/* False suffix: actually, the path keyword */
	LIST searchPath;	/* The path along which files of this suffix
			     	 * may be found */
	int order;		/* order of declaration for conflict
				 * resolution. */
	LIST parents;		/* List of Suff we have a transformation to */
	LIST children;		/* List of Suff we have a transformation from */
	char name[1];
};

static struct ohash_info suff_info = {
	offsetof(struct Suff_, name), NULL,
	hash_calloc, hash_free, element_alloc
};

/*
 * Structure used in the search for implied sources.
 */
typedef struct Src_ {
	char *file;		/* The file to look for */
	char *prefix;		/* Prefix from which file was formed */
	Suff *suff;		/* The suffix on the file */
	struct Src_ *parent;	/* The Src for which this is a source */
	GNode *node;		/* The node describing the file */
	int children;		/* Count of existing children (so we don't free
				 * this thing too early or never nuke it) */
#ifdef DEBUG_SRC
	LIST	    cp; 	/* Debug; children list */
#endif
} Src;

/*
 * A structure for passing more than one argument to the Lst-library-invoked
 * function...
 */
typedef struct {
	Lst l;
	Src *s;
} LstSrc;

static Suff *emptySuff; /* The empty suffix required for POSIX
			 * single-suffix transformation rules */


#define parse_transform(s, p, q) parse_transformi(s, s + strlen(s), p, q)
static bool parse_transformi(const char *, const char *, Suff **, Suff **);
#define new_suffix(s)	new_suffixi(s, NULL)
static Suff *new_suffixi(const char *, const char *);
static void reverse_hash_add_char(uint32_t *, const char *);
static uint32_t reverse_hashi(const char *, const char **);
static unsigned int reverse_slot(struct ohash *, const char *, const char **);
static void clear_suffixes(void);
static void record_possible_suffix(Suff *, GNode *, char *, Lst, Lst);
static void record_possible_suffixes(GNode *, Lst, Lst);
static Suff *find_suffix_as_suffix(Lst, const char *, const char *);
static Suff *add_suffixi(const char *, const char *);

static void SuffInsert(Lst, Suff *);
static void SuffAddSrc(void *, void *);
static bool SuffRemoveSrc(Lst);
static void SuffAddLevel(Lst, Src *);
static Src *SuffFindThem(Lst, Lst);
static Src *SuffFindCmds(Src *, Lst);
static void SuffExpandChildren(LstNode, GNode *);
static void SuffExpandVarChildren(LstNode, GNode *, GNode *);
static void SuffExpandWildChildren(LstNode, GNode *, GNode *);
static bool SuffApplyTransform(GNode *, GNode *, Suff *, Suff *);
static void SuffFindDeps(GNode *, Lst);
static void SuffFindArchiveDeps(GNode *, Lst);
static void SuffFindNormalDeps(GNode *, Lst);
static void SuffPrintName(void *);
static void SuffPrintSuff(void *);
static void SuffPrintTrans(GNode *);

#define find_suff(name)	find_suffi(name, NULL)
static Suff *find_suffi(const char *, const char *);
static Suff *find_best_suffix(const char *, const char *);
static GNode *find_transform(const char *);
static GNode *find_or_create_transformi(const char *, const char *);
static void setup_paths(void);
static void build_suffixes_graph(void);
static void special_path_hack(void);

#ifdef DEBUG_SRC
static void PrintAddr(void *);
#endif

/* hash functions for the suffixes hash */
/* add one char to the hash */
static void
reverse_hash_add_char(uint32_t *pk, const char *s)
{
	*pk =  ((*pk << 2) | (*pk >> 30)) ^ *s;
}

/* build a full hash from end to start */
static uint32_t
reverse_hashi(const char *s, const char **e)
{
	const char *p;
	uint32_t k;

	if (*e == NULL)
		*e = s + strlen(s);
	p = *e;
	if (p == s)
		k = 0;
	else
		k = *--p;
	while (p != s) {
		reverse_hash_add_char(&k, --p);
	}
	return k;
}

static unsigned int
reverse_slot(struct ohash *h, const char *s, const char **e)
{
	uint32_t hv;

	hv = reverse_hashi(s, e);
	return ohash_lookup_interval(h, s, *e, hv);
}


static char *
suffix_is_suffix(Suff *s, const char *str, const char *estr)
{
	const char *p1; 	/* Pointer into suffix name */
	const char *p2; 	/* Pointer into string being examined */

	if (estr - str < (ptrdiff_t) s->nameLen)
		return NULL;
	p1 = s->name + s->nameLen;
	p2 = estr;

	while (p1 != s->name) {
		p1--;
		p2--;
		if (*p1 != *p2)
			return NULL;
	}

	return (char *)p2;
}

static Suff *
find_suffi(const char *name, const char *ename)
{
	unsigned int slot;
#ifdef STATS_SUFF
	STAT_SUFF_LOOKUP_NAME++;
#endif
	slot = reverse_slot(&suffixes, name, &ename);
	return ohash_find(&suffixes, slot);
}

static GNode *
find_transform(const char *name)
{
	unsigned int slot;

#ifdef STATS_SUFF
	STAT_TRANSFORM_LOOKUP_NAME++;
#endif
	slot = ohash_qlookup(&transforms, name);

	return ohash_find(&transforms, slot);
}

static GNode *
find_or_create_transformi(const char *name, const char *end)
{
	GNode *r;
	unsigned int slot;

#ifdef STATS_SUFF
	STAT_TRANSFORM_LOOKUP_NAME++;
#endif
	slot = ohash_qlookupi(&transforms, name, &end);

	r = ohash_find(&transforms, slot);

	if (r == NULL) {
		r = Targ_NewGNi(name, end);
		ohash_insert(&transforms, slot, r);
	}
	return r;
}

/*-
 *-----------------------------------------------------------------------
 * SuffInsert  --
 *	Insert the suffix into the list keeping the list ordered by suffix
 *	numbers.
 *
 * Side Effects:
 *	The reference count of the suffix is incremented
 *-----------------------------------------------------------------------
 */
static void
SuffInsert(Lst l, Suff *s)
{
	LstNode ln;		/* current element in l we're examining */
	Suff *s2 = NULL;	/* the suffix descriptor in this element */

	for (ln = Lst_First(l); ln != NULL; ln = Lst_Adv(ln)) {
		s2 = Lst_Datum(ln);
		if (s2->order >= s->order)
			break;
	}

	if (DEBUG(SUFF))
		printf("inserting %s(%d)...", s->name, s->order);
	if (ln == NULL) {
		if (DEBUG(SUFF))
			printf("at end of list\n");
		Lst_AtEnd(l, s);
	} else if (s2->order != s->order) {
		if (DEBUG(SUFF))
			printf("before %s(%d)\n", s2->name, s2->order);
		Lst_Insert(l, ln, s);
	} else if (DEBUG(SUFF)) {
		printf("already there\n");
	}
}

/*-
 *-----------------------------------------------------------------------
 * Suff_ClearSuffixes --
 *	Nuke the list of suffixes but keep all transformation
 *	rules around.
 *
 * Side Effects:
 *	Current suffixes are reset
 *-----------------------------------------------------------------------
 */
static void
clear_suffixes(void)
{
	unsigned int i;
	Suff *s;

	for (s = ohash_first(&suffixes, &i); s != NULL;
	    s = ohash_next(&suffixes, &i))
		s->flags &= ~SUFF_ACTIVE;

	order = 0;
	maxLen = 0;
}

void
Suff_ClearSuffixes(void)
{
	clear_suffixes();
}


/* okay = parse_transform(str, &src, &targ);
 * 	try parsing a string as a transformation rule, returns true if
 *	successful. Fills &src, &targ with the constituent suffixes.
 * Special hack: source suffixes must exist OR be the special SUFF_PATH
 * pseudo suffix (.PATH)
 */
static bool
parse_transformi(const char *str, const char *e, Suff **srcPtr, Suff **targPtr)
{
	Suff *src, *target, *best_src, *best_target;
	const char *p;

	size_t len;
	uint32_t hv;
	unsigned int slot;

	/* empty string -> no suffix */
	if (e == str)
		return false;

	len = e - str;

	if (len > 2 * maxLen)
		return false;

	p = e;
	best_src = best_target = NULL;

	hv = *--p;
	while (p != str) {
		slot = ohash_lookup_interval(&suffixes, p, e, hv);
		/* no double suffix in there */
		if (p - str <= (ptrdiff_t)maxLen) {
			target = ohash_find(&suffixes, slot);
			if (target != NULL && (target->flags & SUFF_ACTIVE)) {
				src = find_suffi(str, p);
				if (src != NULL &&
				    (src->flags & (SUFF_ACTIVE | SUFF_PATH))) {
				/* XXX even if we find a set of suffixes, we
				 * have to keep going to find the best one,
				 * namely, the one whose src appears first in
				 * .SUFFIXES
				 */
					if (best_src == NULL ||
					    src->order < best_src->order) {
						best_src = src;
						best_target = target;
					}
				}
			}
		}
		/* can't be a suffix anyways */
		if (e - p >= (ptrdiff_t)maxLen)
			break;
		reverse_hash_add_char(&hv, --p);
	}

	if (p == str && best_src == NULL) {
		/* no double suffix transformation, resort to single suffix if
		 * we find one.  */
		slot = ohash_lookup_interval(&suffixes, p, e, hv);
		src = ohash_find(&suffixes, slot);
		if (src != NULL && (src->flags & (SUFF_ACTIVE | SUFF_PATH))) {
			best_src = src;
			best_target = emptySuff;
		}
	}
	if (best_src != NULL) {
		*srcPtr = best_src;
		*targPtr = best_target;
		return true;
	} else {
		return false;
	}
}

static void
special_path_hack(void)
{
	Suff *path = add_suffixi(".PATH", NULL);
	path->flags |= SUFF_PATH;
}

static Suff *
find_best_suffix(const char *s, const char *e)
{
	const char *p;
	uint32_t hv;
	unsigned int slot;
	Suff *best = NULL;
	Suff *suff;

	if (e == s)
		return NULL;
	p = e;
	hv = *--p;
	while (p != s) {
		slot = ohash_lookup_interval(&suffixes, p, e, hv);
		suff = ohash_find(&suffixes, slot);
		if (suff != NULL)
			if (best == NULL || suff->order < best->order)
				best = suff;
		if (e - p >= (ptrdiff_t)maxLen)
			break;
		reverse_hash_add_char(&hv, --p);
	}
	return best;
}

/*-
 *-----------------------------------------------------------------------
 * Suff_ParseAsTransform --
 *	Try parsing a target line as a transformation rule, depending on
 *	existing suffixes.
 *
 *	Possibly create a new transform, or reset an existing one.
 *-----------------------------------------------------------------------
 */
GNode *
Suff_ParseAsTransform(const char *line, const char *end)
{
	GNode *gn;	/* GNode of transformation rule */
	Suff *s;	/* source suffix */
	Suff *t;	/* target suffix */

	if (!parse_transformi(line, end, &s, &t))
		return NULL;

	gn = find_or_create_transformi(line, end);
	/* In case the transform already exists, nuke old commands and children.
	 * Note we can't free them, since there might be stuff that references
	 * them elsewhere
	 */
	if (!Lst_IsEmpty(&gn->commands)) {
		Lst_Destroy(&gn->commands, NOFREE);
		Lst_Init(&gn->commands);
	}
	if (!Lst_IsEmpty(&gn->children)) {
		Lst_Destroy(&gn->children, NOFREE);
		Lst_Init(&gn->children);
	}

	gn->type = OP_TRANSFORM;
	if (s->flags & SUFF_PATH) {
		gn->special = SPECIAL_PATH | SPECIAL_TARGET;
		gn->suffix = t;
	}

	if (DEBUG(SUFF))
		printf("defining transformation from `%s' to `%s'\n",
		    s->name, t->name);
	return gn;
}

static void
make_suffix_known(Suff *s)
{
	if ((s->flags & SUFF_ACTIVE) == 0) {
		s->order = order++;
		s->flags |= SUFF_ACTIVE;
		if (s->nameLen > maxLen)
			maxLen = s->nameLen;
	}
}

static Suff *
new_suffixi(const char *str, const char *eptr)
{
	Suff *s;

	s = ohash_create_entry(&suff_info, str, &eptr);
	s->nameLen = eptr - str;
	Lst_Init(&s->searchPath);
	Lst_Init(&s->children);
	Lst_Init(&s->parents);
	s->flags = 0;
	return s;
}

/*-
 *-----------------------------------------------------------------------
 * Suff_AddSuffix --
 *	Add the suffix in string to the end of the list of known suffixes.
 *	Should we restructure the suffix graph? Make doesn't...
 *
 * Side Effects:
 *	A GNode is created for the suffix and a Suff structure is created and
 *	added to the known suffixes, unless it was already known.
 *-----------------------------------------------------------------------
 */
void
Suff_AddSuffixi(const char *str, const char *end)
{
	(void)add_suffixi(str, end);
}

static Suff *
add_suffixi(const char *str, const char *end)
{
	Suff *s;	    /* new suffix descriptor */

	unsigned int slot;

	slot = reverse_slot(&suffixes, str, &end);
	s = ohash_find(&suffixes, slot);
	if (s == NULL) {
		s = new_suffixi(str, end);
		ohash_insert(&suffixes, slot, s);
	}
	make_suffix_known(s);
	return s;
}

Lst
find_suffix_path(GNode *gn)
{
	if (gn->suffix != NULL && gn->suffix != emptySuff)
		return &(gn->suffix->searchPath);
	else
		return defaultPath;
}

static void
build_suffixes_graph(void)
{
	Suff *s, *s2;
	GNode *gn;
	unsigned int i;

	for (gn = ohash_first(&transforms, &i); gn != NULL;
	    gn = ohash_next(&transforms, &i)) {
	    	if (Lst_IsEmpty(&gn->commands) && Lst_IsEmpty(&gn->children))
			continue;
		if ((gn->special & SPECIAL_MASK) == SPECIAL_PATH)
			continue;
	    	if (parse_transform(gn->name, &s, &s2)) {
			SuffInsert(&s2->children, s);
			SuffInsert(&s->parents, s2);
		}
	}
}

/*-
 *-----------------------------------------------------------------------
 * setup_paths
 *	Extend the search paths for all suffixes to include the default
 *	search path.
 *
 * Side Effects:
 *	The searchPath field of all the suffixes is extended by the
 *	directories in defaultPath. If paths were specified for the
 *	".h" suffix, the directories are stuffed into a global variable
 *	called ".INCLUDES" with each directory preceded by a -I. The same
 *	is done for the ".a" suffix, except the variable is called
 *	".LIBS" and the flag is -L.
 *-----------------------------------------------------------------------
 */
static void
setup_paths(void)
{
	unsigned int i;
	Suff *s;

	for (s = ohash_first(&suffixes, &i); s != NULL;
	    s = ohash_next(&suffixes, &i)) {
		if (!Lst_IsEmpty(&s->searchPath))
			Dir_Concat(&s->searchPath, defaultPath);
		else
			Lst_Clone(&s->searchPath, defaultPath, Dir_CopyDir);
	}
}

void
process_suffixes_after_makefile_is_read(void)
{
	/* once the Makefile is finish reading, we can set up the default PATH
	 * stuff, and build the final suffixes graph
	 */
	setup_paths();
	/* and we link all transforms to active suffixes at this point. */
	build_suffixes_graph();
}
	  /********** Implicit Source Search Functions *********/

/*-
 *-----------------------------------------------------------------------
 * SuffAddSrc  --
 *	Add a suffix as a Src structure to the given list with its parent
 *	being the given Src structure. If the suffix is the null suffix,
 *	the prefix is used unaltered as the file name in the Src structure.
 *
 * Side Effects:
 *	A Src structure is created and tacked onto the end of the list
 *-----------------------------------------------------------------------
 */
static void
SuffAddSrc(
    void *sp,		/* suffix for which to create a Src structure */
    void *lsp)		/* list and parent for the new Src */
{
	Suff *s = sp;
	LstSrc *ls = lsp;
	Src *s2;	/* new Src structure */
	Src *targ;	/* Target structure */

	targ = ls->s;

	s2 = emalloc(sizeof(Src));
	s2->file = Str_concat(targ->prefix, s->name, 0);
	s2->prefix = targ->prefix;
	s2->parent = targ;
	s2->node = NULL;
	s2->suff = s;
	s2->children = 0;
	targ->children++;
	Lst_AtEnd(ls->l, s2);
#ifdef DEBUG_SRC
	Lst_Init(&s2->cp);
	Lst_AtEnd(&targ->cp, s2);
	printf("2 add %x %x to %x:", targ, s2, ls->l);
	Lst_Every(ls->l, PrintAddr);
	printf("\n");
#endif

}

/*-
 *-----------------------------------------------------------------------
 * SuffAddLevel  --
 *	Add all the children of targ as Src structures to the given list
 *
 * Side Effects:
 *	Lots of structures are created and added to the list
 *-----------------------------------------------------------------------
 */
static void
SuffAddLevel(
    Lst l,	/* list to which to add the new level */
    Src *targ)	/* Src structure to use as the parent */
{
	LstSrc	   ls;

	ls.s = targ;
	ls.l = l;

	Lst_ForEach(&targ->suff->children, SuffAddSrc, &ls);
}

/*-
 *----------------------------------------------------------------------
 * SuffRemoveSrc --
 *	Free Src structure with a zero reference count in a list
 *
 *	returns true if a src was removed
 *----------------------------------------------------------------------
 */
static bool
SuffRemoveSrc(Lst l)
{
	LstNode ln;
	Src *s;

#ifdef DEBUG_SRC
	printf("cleaning %lx: ", (unsigned long)l);
	Lst_Every(l, PrintAddr);
	printf("\n");
#endif


	for (ln = Lst_First(l); ln != NULL; ln = Lst_Adv(ln)) {
		s = Lst_Datum(ln);
		if (s->children == 0) {
			free(s->file);
			if (!s->parent)
				free(s->prefix);
			else {
#ifdef DEBUG_SRC
				LstNode ln2 = Lst_Member(&s->parent->cp, s);
				if (ln2 != NULL)
				    Lst_Remove(&s->parent->cp, ln2);
#endif
				--s->parent->children;
			}
#ifdef DEBUG_SRC
			printf("free: [l=%x] p=%x %d\n", l, s, s->children);
			Lst_Destroy(&s->cp, NOFREE);
#endif
			Lst_Remove(l, ln);
			free(s);
			return true;
		}
#ifdef DEBUG_SRC
		else {
			printf("keep: [l=%x] p=%x %d: ", l, s, s->children);
			Lst_Every(&s->cp, PrintAddr);
			printf("\n");
		}
#endif
	}

	return false;
}

/*-
 *-----------------------------------------------------------------------
 * SuffFindThem --
 *	Find the first existing file/target in the list srcs
 *
 * Results:
 *	The lowest structure in the chain of transformations
 *-----------------------------------------------------------------------
 */
static Src *
SuffFindThem(
    Lst srcs,	/* list of Src structures to search through */
    Lst slst)
{
	Src *s;		/* current Src */
	Src *rs; 	/* returned Src */
	char *ptr;

	rs = NULL;

	while ((s = Lst_DeQueue(srcs)) != NULL) {
		if (DEBUG(SUFF))
			printf("\ttrying %s...", s->file);

		/*
		 * A file is considered to exist if either a node exists in the
		 * graph for it or the file actually exists.
		 */
		if (Targ_FindNode(s->file, TARG_NOCREATE) != NULL) {
#ifdef DEBUG_SRC
			printf("remove %x from %x\n", s, srcs);
#endif
			rs = s;
			break;
		}

		if ((ptr = Dir_FindFile(s->file, &s->suff->searchPath))
		    != NULL) {
			rs = s;
#ifdef DEBUG_SRC
			printf("remove %x from %x\n", s, srcs);
#endif
			free(ptr);
			break;
		}

		if (DEBUG(SUFF))
		    printf("not there\n");

		SuffAddLevel(srcs, s);
		Lst_AtEnd(slst, s);
	}

	if (DEBUG(SUFF) && rs)
	    printf("got it\n");
	return rs;
}

/*-
 *-----------------------------------------------------------------------
 * SuffFindCmds --
 *	See if any of the children of the target in the Src structure is
 *	one from which the target can be transformed. If there is one,
 *	a Src structure is put together for it and returned.
 *-----------------------------------------------------------------------
 */
static Src *
SuffFindCmds(Src *targ, Lst slst)
{
	LstNode ln;	/* General-purpose list node */
	GNode *t;	/* Target GNode */
	GNode *s;	/* Source GNode */
	int prefixLen;	/* The length of the defined prefix */
	Suff *suff;	/* Suffix on matching beastie */
	Src *ret;	/* Return value */
	const char *cp;

	t = targ->node;
	prefixLen = strlen(targ->prefix);

	for (ln = Lst_First(&t->children); ln != NULL; ln = Lst_Adv(ln)) {
		s = Lst_Datum(ln);

		cp = strrchr(s->name, '/');
		if (cp == NULL)
			cp = s->name;
		else
			cp++;
		if (strncmp(cp, targ->prefix, prefixLen) != 0)
			continue;
		/* The node matches the prefix ok, see if it has a known
		 * suffix.	*/
		suff = find_suff(&cp[prefixLen]);
		if (suff == NULL)
			continue;
		/*
		 * It even has a known suffix, see if there's a transformation
		 * defined between the node's suffix and the target's suffix.
		 *
		 * XXX: Handle multi-stage transformations here, too.
		 */
		if (Lst_Member(&suff->parents, targ->suff) == NULL)
			continue;
		/*
		 * Create a new Src structure to describe this transformation
		 * (making sure to duplicate the source node's name so
		 * Suff_FindDeps can free it again (ick)), and return the new
		 * structure.
		 */
		ret = emalloc(sizeof(Src));
		ret->file = estrdup(s->name);
		ret->prefix = targ->prefix;
		ret->suff = suff;
		ret->parent = targ;
		ret->node = s;
		ret->children = 0;
		targ->children++;
#ifdef DEBUG_SRC
		Lst_Init(&ret->cp);
		printf("3 add %x %x\n", targ, ret);
		Lst_AtEnd(&targ->cp, ret);
#endif
		Lst_AtEnd(slst, ret);
		if (DEBUG(SUFF))
			printf("\tusing existing source %s\n", s->name);
		return ret;
	}
	return NULL;
}

static void
SuffLinkParent(GNode *cgn, GNode *pgn)
{
	Lst_AtEnd(&cgn->parents, pgn);
	if (!has_been_built(cgn))
		pgn->unmade++;
	else if ( ! (cgn->type & (OP_EXEC|OP_USE))) {
		if (cgn->built_status == MADE)
			pgn->childMade = true;
		(void)Make_TimeStamp(pgn, cgn);
	}
}

static void
SuffExpandVarChildren(LstNode after, GNode *cgn, GNode *pgn)
{
	GNode *gn;		/* New source 8) */
	char *cp;		/* Expanded value */
	LIST members;


	if (DEBUG(SUFF))
		printf("Expanding \"%s\"...", cgn->name);

	cp = Var_Subst(cgn->name, &pgn->context, true);
	if (cp == NULL) {
		printf("Problem substituting in %s", cgn->name);
		printf("\n");
		return;
	}

	Lst_Init(&members);

	if (cgn->type & OP_ARCHV) {
		/*
		 * Node was an archive(member) target, so we want to call
		 * on the Arch module to find the nodes for us, expanding
		 * variables in the parent's context.
		 */
		const char *sacrifice = (const char *)cp;

		(void)Arch_ParseArchive(&sacrifice, &members, &pgn->context);
	} else {
		/* Break the result into a vector of strings whose nodes
		 * we can find, then add those nodes to the members list.
		 * Unfortunately, we can't use brk_string because it
		 * doesn't understand about variable specifications with
		 * spaces in them...  */
		const char *start, *cp2;

		for (start = cp; *start == ' ' || *start == '\t'; start++)
			continue;
		for (cp2 = start; *cp2 != '\0';) {
			if (ISSPACE(*cp2)) {
				/* White-space -- terminate element, find the
				 * node, add it, skip any further spaces.  */
				gn = Targ_FindNodei(start, cp2, TARG_CREATE);
				cp2++;
				Lst_AtEnd(&members, gn);
				while (ISSPACE(*cp2))
					cp2++;
				/* Adjust cp2 for increment at start of loop,
				 * but set start to first non-space.  */
				start = cp2;
			} else if (*cp2 == '$')
				/* Start of a variable spec -- contact variable
				 * module to find the end so we can skip over
				 * it.  */
				Var_ParseSkip(&cp2, &pgn->context);
			else if (*cp2 == '\\' && cp2[1] != '\0')
				/* Escaped something -- skip over it.  */
				cp2+=2;
			else
				cp2++;
	    }

	    if (cp2 != start) {
		    /* Stuff left over -- add it to the list too.  */
		    gn = Targ_FindNodei(start, cp2, TARG_CREATE);
		    Lst_AtEnd(&members, gn);
	    }
	}
	/* Add all elements of the members list to the parent node.  */
	while ((gn = Lst_DeQueue(&members)) != NULL) {
		if (DEBUG(SUFF))
			printf("%s...", gn->name);
		if (Lst_Member(&pgn->children, gn) == NULL) {
			Lst_Append(&pgn->children, after, gn);
			after = Lst_Adv(after);
			SuffLinkParent(gn, pgn);
		}
	}
	/* Free the result.  */
	free(cp);
	if (DEBUG(SUFF))
		printf("\n");
}

static void
SuffExpandWildChildren(LstNode after, GNode *cgn, GNode *pgn)
{
	Suff *s;
	char *cp;	/* Expanded value */

	LIST exp;	/* List of expansions */
	Lst path;	/* Search path along which to expand */

	if (DEBUG(SUFF))
		printf("Wildcard expanding \"%s\"...", cgn->name);

	/* Find a path along which to expand the word.
	 *
	 * If the word has a known suffix, use that path.
	 * If it has no known suffix and we're allowed to use the null
	 *	 suffix, use its path.
	 * Else use the default system search path.  */
	s = find_best_suffix(cgn->name, cgn->name + strlen(cgn->name));

	if (s != NULL) {
		if (DEBUG(SUFF))
			printf("suffix is \"%s\"...", s->name);
		path = &s->searchPath;
	} else
		/* Use default search path.  */
		path = defaultPath;

	/* Expand the word along the chosen path. */
	Lst_Init(&exp);
	Dir_Expand(cgn->name, path, &exp);

	/* Fetch next expansion off the list and find its GNode.  */
	while ((cp = Lst_DeQueue(&exp)) != NULL) {
		GNode *gn;		/* New source 8) */
		if (DEBUG(SUFF))
			printf("%s...", cp);
		gn = Targ_FindNode(cp, TARG_CREATE);

		/* If gn isn't already a child of the parent, make it so and
		 * up the parent's count of unmade children.  */
		if (Lst_Member(&pgn->children, gn) == NULL) {
			Lst_Append(&pgn->children, after, gn);
			after = Lst_Adv(after);
			SuffLinkParent(gn, pgn);
		}
	}

	if (DEBUG(SUFF))
		printf("\n");
}

/*-
 *-----------------------------------------------------------------------
 * SuffExpandChildren --
 *	Expand the names of any children of a given node that contain
 *	variable invocations or file wildcards into actual targets.
 *
 * Side Effects:
 *	The expanded node is removed from the parent's list of children,
 *	and the parent's unmade counter is decremented, but other nodes
 *	may be added.
 *-----------------------------------------------------------------------
 */
static void
SuffExpandChildren(LstNode ln, /* LstNode of child, so we can replace it */
    GNode *pgn)
{
	GNode	*cgn = Lst_Datum(ln);

	/* First do variable expansion -- this takes precedence over wildcard
	 * expansion. If the result contains wildcards, they'll be gotten to
	 * later since the resulting words are tacked on to the end of the
	 * children list.  */
	if (strchr(cgn->name, '$') != NULL)
		SuffExpandVarChildren(ln, cgn, pgn);
	else if (Dir_HasWildcards(cgn->name))
		SuffExpandWildChildren(ln, cgn, pgn);
	else
	    /* Third case: nothing to expand.  */
		return;

	/* Since the source was expanded, remove it from the list of children to
	 * keep it from being processed.  */
	pgn->unmade--;
	Lst_Remove(&pgn->children, ln);
}

void
expand_children_from(GNode *parent, LstNode from)
{
	LstNode np, ln;

	for (ln = from; ln != NULL; ln = np) {
		np = Lst_Adv(ln);
		SuffExpandChildren(ln, parent);
	}
}

/*-
 *-----------------------------------------------------------------------
 * SuffApplyTransform --
 *	Apply a transformation rule, given the source and target nodes
 *	and suffixes.
 *
 * Results:
 *	true if successful, false if not.
 *
 * Side Effects:
 *	The source and target are linked and the commands from the
 *	transformation are added to the target node's commands list.
 *	All attributes but OP_DEPMASK and OP_TRANSFORM are applied
 *	to the target. The target also inherits all the sources for
 *	the transformation rule.
 *-----------------------------------------------------------------------
 */
static bool
SuffApplyTransform(
    GNode	*tGn,	/* Target node */
    GNode	*sGn,	/* Source node */
    Suff	*t,	/* Target suffix */
    Suff	*s)	/* Source suffix */
{
	LstNode	ln;	/* General node */
	char	*tname; /* Name of transformation rule */
	GNode	*gn;	/* Node for same */

	if (Lst_AddNew(&tGn->children, sGn)) {
		/* Not already linked, so form the proper links between the
		 * target and source.  */
		SuffLinkParent(sGn, tGn);
	}

	if ((sGn->type & OP_OPMASK) == OP_DOUBLEDEP) {
		/* When a :: node is used as the implied source of a node, we
		 * have to link all its cohorts in as sources as well. There's
		 * only one implied src, as that will be sufficient to get
		 * the .IMPSRC variable set for tGn.	*/
		for (ln=Lst_First(&sGn->cohorts); ln != NULL; ln=Lst_Adv(ln)) {
			gn = Lst_Datum(ln);

			if (Lst_AddNew(&tGn->children, gn)) {
				/* Not already linked, so form the proper links
				 * between the target and source.  */
				SuffLinkParent(gn, tGn);
			}
		}
	}
	/* Locate the transformation rule itself.  */
	tname = Str_concat(s->name, t->name, 0);
	gn = find_transform(tname);
	free(tname);

	if (gn == NULL)
		/*
		 * Not really such a transformation rule (can happen when we're
		 * called to link an OP_MEMBER and OP_ARCHV node), so return
		 * false.
		 */
		return false;

	if (DEBUG(SUFF))
		printf("\tapplying %s -> %s to \"%s\"\n", s->name, t->name,
		    tGn->name);

	/* Record last child for expansion purposes.  */
	ln = Lst_Last(&tGn->children);

	/* Pass the buck to Make_HandleUse to apply the rule.  */
	Make_HandleUse(gn, tGn);

	/* Deal with wildcards and variables in any acquired sources.  */
	expand_children_from(tGn, Lst_Succ(ln));

	/* Keep track of another parent to which this beast is transformed so
	 * the .IMPSRC variable can be set correctly for the parent.  */
	tGn->impliedsrc = sGn;

	return true;
}

static Suff *
find_suffix_as_suffix(Lst l, const char *b, const char *e)
{
	LstNode ln;
	Suff *s;

	for (ln = Lst_First(l); ln != NULL; ln = Lst_Adv(ln)) {
		s = Lst_Datum(ln);
		if (suffix_is_suffix(s, b, e))
			return s;
	}
	return NULL;
}

/*-
 *-----------------------------------------------------------------------
 * SuffFindArchiveDeps --
 *	Locate dependencies for an OP_ARCHV node.
 *
 * Side Effects:
 *	Same as Suff_FindDeps
 *-----------------------------------------------------------------------
 */
static void
SuffFindArchiveDeps(
    GNode	*gn,    /* Node for which to locate dependencies */
    Lst 	slst)
{
	char *eoarch;	/* End of archive portion */
	char *eoname;	/* End of member portion */
	GNode *mem;	/* Node for member */
	Suff *ms;	/* Suffix descriptor for member */
	char *name;	/* Start of member's name */

	/* The node is an archive(member) pair. so we must find a suffix
	 * for both of them.  */
	eoarch = strchr(gn->name, '(');
	if (eoarch == NULL)
		return;

	name = eoarch + 1;

	eoname = strchr(name, ')');
	if (eoname == NULL)
		return;

	/* To simplify things, call Suff_FindDeps recursively on the member now,
	 * so we can simply compare the member's .PREFIX and .TARGET variables
	 * to locate its suffix. This allows us to figure out the suffix to
	 * use for the archive without having to do a quadratic search over the
	 * suffix list, backtracking for each one...  */
	mem = Targ_FindNodei(name, eoname, TARG_CREATE);
	SuffFindDeps(mem, slst);

	/* Create the link between the two nodes right off. */
	if (Lst_AddNew(&gn->children, mem))
		SuffLinkParent(mem, gn);

	/* Copy variables from member node to this one.  */
	Var(TARGET_INDEX, gn) = Var(TARGET_INDEX, mem);
	Var(PREFIX_INDEX, gn) = Var(PREFIX_INDEX, mem);

	ms = mem->suffix;
	if (ms == NULL) {
		/* Didn't know what it was -- use .NULL suffix if not in make
		 * mode.  */
		if (DEBUG(SUFF))
			printf("using empty suffix\n");
		ms = emptySuff;
	}


	/* Set the other two local variables required for this target.  */
	Var(MEMBER_INDEX, gn) = mem->name;
	Var(ARCHIVE_INDEX, gn) = gn->name;

	if (ms != NULL) {
		/*
		 * Member has a known suffix, so look for a transformation rule
		 * from it to a possible suffix of the archive. Rather than
		 * searching through the entire list, we just look at suffixes
		 * to which the member's suffix may be transformed...
		 */

		Suff *suff;

		suff = find_suffix_as_suffix(&ms->parents, gn->name, eoarch);

		if (suff != NULL) {
			/* Got one -- apply it.  */
			if (!SuffApplyTransform(gn, mem, suff, ms) &&
			    DEBUG(SUFF))
				printf("\tNo transformation from %s -> %s\n",
				   ms->name, suff->name);
		}
	}

	/* Pretend gn appeared to the left of a dependency operator so
	 * the user needn't provide a transformation from the member to the
	 * archive.  */
	if (OP_NOP(gn->type))
		gn->type |= OP_DEPENDS;

	/* Flag the member as such so we remember to look in the archive for
	 * its modification time.  */
	mem->type |= OP_MEMBER;
}

static void
record_possible_suffix(Suff *s, GNode *gn, char *eoname, Lst srcs, Lst targs)
{
	int prefixLen;
	Src *targ;

	targ = emalloc(sizeof(Src));
	targ->file = estrdup(gn->name);
	targ->suff = s;
	targ->node = gn;
	targ->parent = NULL;
	targ->children = 0;
#ifdef DEBUG_SRC
	Lst_Init(&targ->cp);
#endif

	/* Allocate room for the prefix, whose end is found by
	 * subtracting the length of the suffix from the end of
	 * the name.  */
	prefixLen = (eoname - targ->suff->nameLen) - gn->name;
	targ->prefix = emalloc(prefixLen + 1);
	memcpy(targ->prefix, gn->name, prefixLen);
	targ->prefix[prefixLen] = '\0';

	/* Add nodes from which the target can be made.  */
	SuffAddLevel(srcs, targ);

	/* Record the target so we can nuke it.  */
	Lst_AtEnd(targs, targ);
}

static void
record_possible_suffixes(GNode *gn, Lst srcs, Lst targs)
{
	char *s = gn->name;
	char *e =  s + strlen(s);
	const char *p;
	uint32_t hv;
	unsigned int slot;
	Suff *suff;

	if (e == s)
		return;

	p = e;
	hv = *--p;

	while (p != s) {
		slot = ohash_lookup_interval(&suffixes, p, e, hv);
		suff = ohash_find(&suffixes, slot);
		if (suff != NULL && (suff->flags & SUFF_ACTIVE))
			record_possible_suffix(suff, gn, e, srcs, targs);
		if (e - p >= (ptrdiff_t)maxLen)
			break;
		reverse_hash_add_char(&hv, --p);
	}
}

/*-
 *-----------------------------------------------------------------------
 * SuffFindNormalDeps --
 *	Locate implicit dependencies for regular targets.
 *
 * Side Effects:
 *	Same as Suff_FindDeps...
 *-----------------------------------------------------------------------
 */
static void
SuffFindNormalDeps(
    GNode	*gn,	    /* Node for which to find sources */
    Lst 	slst)
{
	LIST srcs;	/* List of sources at which to look */
	LIST targs;	/* List of targets to which things can be
			 * transformed. They all have the same file,
			 * but different suff and prefix fields */
	Src *bottom;    /* Start of found transformation path */
	Src *src;	/* General Src pointer */
	char *prefix;	/* Prefix to use */
	Src *targ;	/* General Src target pointer */


	Lst_Init(&srcs);
	Lst_Init(&targs);

	/* We're caught in a catch-22 here. On the one hand, we want to use any
	 * transformation implied by the target's sources, but we can't examine
	 * the sources until we've expanded any variables/wildcards they may
	 * hold, and we can't do that until we've set up the target's local
	 * variables and we can't do that until we know what the proper suffix
	 * for the target is (in case there are two suffixes one of which is a
	 * suffix of the other) and we can't know that until we've found its
	 * implied source, which we may not want to use if there's an existing
	 * source that implies a different transformation.
	 *
	 * In an attempt to get around this, which may not work all the time,
	 * but should work most of the time, we look for implied sources first,
	 * checking transformations to all possible suffixes of the target, use
	 * what we find to set the target's local variables, expand the
	 * children, then look for any overriding transformations they imply.
	 * Should we find one, we discard the one we found before.	*/


	record_possible_suffixes(gn, &srcs, &targs);
	/* Handle target of unknown suffix...  */
	if (Lst_IsEmpty(&srcs)) {
		if (DEBUG(SUFF))
			printf("\tNo known suffix on %s. Using empty suffix\n",
			    gn->name);

		targ = emalloc(sizeof(Src));
		targ->file = estrdup(gn->name);
		targ->suff = emptySuff;
		targ->node = gn;
		targ->parent = NULL;
		targ->children = 0;
		targ->prefix = estrdup(gn->name);
#ifdef DEBUG_SRC
		Lst_Init(&targ->cp);
#endif

		/* Only use the default suffix rules if we don't have commands
		 * or dependencies defined for this gnode.  */
		if (Lst_IsEmpty(&gn->commands) && Lst_IsEmpty(&gn->children))
			SuffAddLevel(&srcs, targ);
		else {
			if (DEBUG(SUFF))
				printf("not ");
		}

		if (DEBUG(SUFF))
			printf("adding suffix rules\n");

		Lst_AtEnd(&targs, targ);
	}

	/* Using the list of possible sources built up from the target
	 * suffix(es), try and find an existing file/target that matches.  */
	bottom = SuffFindThem(&srcs, slst);

	if (bottom == NULL) {
		/* No known transformations -- use the first suffix found for
		 * setting the local variables.  */
		if (!Lst_IsEmpty(&targs))
			targ = Lst_Datum(Lst_First(&targs));
		else
			targ = NULL;
	} else {
		/* Work up the transformation path to find the suffix of the
		 * target to which the transformation was made.  */
		for (targ = bottom; targ->parent != NULL; targ = targ->parent)
			continue;
	}

	/* The .TARGET variable we always set to be the name at this point,
	 * since it's only set to the path if the thing is only a source and
	 * if it's only a source, it doesn't matter what we put here as far
	 * as expanding sources is concerned, since it has none...	*/
	Var(TARGET_INDEX, gn) = gn->name;

	prefix = targ != NULL ? estrdup(targ->prefix) : gn->name;
	Var(PREFIX_INDEX, gn) = prefix;

	/* Now we've got the important local variables set, expand any sources
	 * that still contain variables or wildcards in their names.  */
	expand_all_children(gn);

	if (targ == NULL) {
		if (DEBUG(SUFF))
			printf("\tNo valid suffix on %s\n", gn->name);

sfnd_abort:
		/* Deal with finding the thing on the default search path if the
		 * node is only a source (not on the lhs of a dependency operator
		 * or [XXX] it has neither children or commands).  */
		if (OP_NOP(gn->type) ||
		    (Lst_IsEmpty(&gn->children) &&
		    Lst_IsEmpty(&gn->commands))) {
			gn->path = Dir_FindFile(gn->name,
			    (targ == NULL ? defaultPath :
			    &targ->suff->searchPath));
			if (gn->path != NULL) {
				char *ptr;
				Var(TARGET_INDEX, gn) = estrdup(gn->path);

				if (targ != NULL) {
					/* Suffix known for the thing -- trim
					 * the suffix off the path to form the
					 * proper .PREFIX variable.  */
					int savep = strlen(gn->path) -
					    targ->suff->nameLen;
					char savec;

					gn->suffix = targ->suff;

					savec = gn->path[savep];
					gn->path[savep] = '\0';

					if ((ptr = strrchr(gn->path, '/')) !=
					    NULL)
						ptr++;
					else
						ptr = gn->path;

					Var(PREFIX_INDEX, gn) = estrdup(ptr);

					gn->path[savep] = savec;
				} else {
					/* The .PREFIX gets the full path if the
					 * target has no known suffix.  */
					gn->suffix = NULL;

					if ((ptr = strrchr(gn->path, '/')) !=
					    NULL)
						ptr++;
					else
						ptr = gn->path;

					Var(PREFIX_INDEX, gn) = estrdup(ptr);
				}
			}
		} else {
			/* Not appropriate to search for the thing -- set the
			 * path to be the name so Dir_MTime won't go grovelling
			 * for it.  */
			gn->suffix = targ == NULL ? NULL : targ->suff;
			free(gn->path);
			gn->path = estrdup(gn->name);
		}

		goto sfnd_return;
	}

	/* Check for overriding transformation rule implied by sources.  */
	if (!Lst_IsEmpty(&gn->children)) {
		src = SuffFindCmds(targ, slst);

		if (src != NULL) {
			/* Free up all the Src structures in the transformation
			 * path up to, but not including, the parent node.  */
			while (bottom && bottom->parent != NULL) {
				(void)Lst_AddNew(slst, bottom);
				bottom = bottom->parent;
			}
			bottom = src;
		}
	}

	if (bottom == NULL) {
		/* No idea from where it can come -- return now.  */
		goto sfnd_abort;
	}

	/* We now have a list of Src structures headed by 'bottom' and linked
	 * via their 'parent' pointers. What we do next is create links between
	 * source and target nodes (which may or may not have been created) and
	 * set the necessary local variables in each target. The commands for
	 * each target are set from the commands of the transformation rule
	 * used to get from the src suffix to the targ suffix. Note that this
	 * causes the commands list of the original node, gn, to be replaced by
	 * the commands of the final transformation rule. Also, the unmade
	 * field of gn is incremented.  Etc.  */
	if (bottom->node == NULL) {
		bottom->node = Targ_FindNode(bottom->file, TARG_CREATE);
	}

	for (src = bottom; src->parent != NULL; src = src->parent) {
		targ = src->parent;

		src->node->suffix = src->suff;

		if (targ->node == NULL) {
			targ->node = Targ_FindNode(targ->file, TARG_CREATE);
		}

		SuffApplyTransform(targ->node, src->node,
				   targ->suff, src->suff);

		if (targ->node != gn) {
			/* Finish off the dependency-search process for any
			 * nodes between bottom and gn (no point in questing
			 * around the filesystem for their implicit source when
			 * it's already known). Note that the node can't have
			 * any sources that need expanding, since SuffFindThem
			 * will stop on an existing node, so all we need to do
			 * is set the standard and System V variables.  */
			targ->node->type |= OP_DEPS_FOUND;

			Var(PREFIX_INDEX, targ->node) = estrdup(targ->prefix);

			Var(TARGET_INDEX, targ->node) = targ->node->name;
		}
	}

	gn->suffix = src->suff;

	/* So Dir_MTime doesn't go questing for it...  */
	free(gn->path);
	gn->path = estrdup(gn->name);

	/* Nuke the transformation path and the Src structures left over in the
	 * two lists.  */
sfnd_return:
	if (bottom)
		(void)Lst_AddNew(slst, bottom);

	while (SuffRemoveSrc(&srcs) || SuffRemoveSrc(&targs))
		continue;

	Lst_ConcatDestroy(slst, &srcs);
	Lst_ConcatDestroy(slst, &targs);
}


/*-
 *-----------------------------------------------------------------------
 * Suff_FindDeps  --
 *	Find implicit sources for the target described by the graph node
 *	gn
 *
 * Side Effects:
 *	Nodes are added to the graph below the passed-in node. The nodes
 *	are marked to have their IMPSRC variable filled in. The
 *	PREFIX variable is set for the given node and all its
 *	implied children.
 *
 * Notes:
 *	The path found by this target is the shortest path in the
 *	transformation graph, which may pass through non-existent targets,
 *	to an existing target. The search continues on all paths from the
 *	root suffix until a file is found. I.e. if there's a path
 *	.o -> .c -> .l -> .l,v from the root and the .l,v file exists but
 *	the .c and .l files don't, the search will branch out in
 *	all directions from .o and again from all the nodes on the
 *	next level until the .l,v node is encountered.
 *-----------------------------------------------------------------------
 */

void
Suff_FindDeps(GNode *gn)
{

	SuffFindDeps(gn, &srclist);
	while (SuffRemoveSrc(&srclist))
		continue;
}


static void
SuffFindDeps(GNode *gn, Lst slst)
{
	if (gn->type & OP_DEPS_FOUND) {
		/*
		 * If dependencies already found, no need to do it again...
		 */
		return;
	} else {
		gn->type |= OP_DEPS_FOUND;
	}

	if (DEBUG(SUFF))
		printf("SuffFindDeps (%s)\n", gn->name);

	current_node = gn;
	if (gn->type & OP_ARCHV)
		SuffFindArchiveDeps(gn, slst);
	else
		SuffFindNormalDeps(gn, slst);
	current_node = NULL;
}

/*-
 *-----------------------------------------------------------------------
 * Suff_Init --
 *	Initialize suffixes module
 *
 * Side Effects:
 *	Many
 *-----------------------------------------------------------------------
 */
void
Suff_Init(void)
{
	Static_Lst_Init(&srclist);
	ohash_init(&transforms, 4, &gnode_info);

	/*
	 * Create null suffix for single-suffix rules (POSIX). The thing doesn't
	 * actually go on the suffix list or everyone will think that's its
	 * suffix.
	 */
	emptySuff = new_suffix("");
	make_suffix_known(emptySuff);
	Dir_Concat(&emptySuff->searchPath, defaultPath);
	ohash_init(&suffixes, 4, &suff_info);
	order = 0;
	clear_suffixes();
	special_path_hack();

}


/********************* DEBUGGING FUNCTIONS **********************/

static void
SuffPrintName(void *p)
{
	const Suff *s = p;
	printf("%s ", s == emptySuff ? "<empty>" : s->name);
}

static void
SuffPrintSuff(void *sp)
{
	Suff    *s = sp;

	printf("# %-5s ", s->name);

	if (!Lst_IsEmpty(&s->parents)) {
		printf(" ->");
		Lst_Every(&s->parents, SuffPrintName);
	}
	if (!Lst_IsEmpty(&s->children)) {
		printf(" <-");
		Lst_Every(&s->children, SuffPrintName);
	}
	fputc('\n', stdout);
}

static void
SuffPrintTrans(GNode *t)
{
	printf("%-16s: ", t->name);
	Targ_PrintType(t->type);
	fputc('\n', stdout);
	Lst_Every(&t->commands, Targ_PrintCmd);
	fputc('\n', stdout);
}

static int 
compare_order(const void *a, const void *b)
{
	const Suff **pa = (const Suff **)a;
	const Suff **pb = (const Suff **)b;
	return (*pb)->order - (*pa)->order;
}

static void
print_path(Suff *s)
{
	/* do we need to print it ? compare against defaultPath */
	LstNode ln1, ln2;
	bool first = true;

	for (ln1 = Lst_First(&s->searchPath), ln2 = Lst_First(defaultPath);
	    ln1 != NULL && ln2 != NULL; 
	    ln1 = Lst_Adv(ln1)) {
		if (Lst_Datum(ln1) == Lst_Datum(ln2)) {
			ln2 = Lst_Adv(ln2);
			continue;
		}
		if (first) {
			printf(".PATH%s:", s->name);
			first = false;
		}
		printf(" %s", PathEntry_name(Lst_Datum(ln1)));
	}
	if (!first)
		printf("\n\n");
}

void
Suff_PrintAll(void)
{
	Suff **t;
	GNode **u;
	unsigned int i;
	bool reprint;


	printf("# Suffixes graph\n");
	t = sort_ohash_by_name(&suffixes);
	for (i = 0; t[i] != NULL; i++)
		if (!(t[i]->flags & SUFF_PATH))
			SuffPrintSuff(t[i]);

	printf("\n.PATH: ");
	Dir_PrintPath(defaultPath);
	printf("\n\n");
	for (i = 0; t[i] != NULL; i++)
		if (!(t[i]->flags & SUFF_PATH))
			print_path(t[i]);
	free(t);

	reprint = false;
	t = sort_ohash(&suffixes, compare_order);
	printf(".SUFFIXES:");
	for (i = 0; t[i] != NULL; i++) {
		if (t[i]->flags & SUFF_PATH)
			continue;
		printf(" %s", t[i]->name);
		if (!(t[i]->flags & SUFF_ACTIVE))
			reprint = true;
	}
	printf("\n\n");
	u = sort_ohash_by_name(&transforms);
	for (i = 0; u[i] != NULL; i++)
	    	SuffPrintTrans(u[i]);
	free(u);

	if (reprint) {
		printf(".SUFFIXES:");
		for (i = 0; t[i] != NULL; i++)
			if (t[i]->flags & SUFF_ACTIVE)
				printf(" %s", t[i]->name);
		printf("\n");
	}
	free(t);
}

#ifdef DEBUG_SRC
static void
PrintAddr(void *a)
{
	printf("%lx ", (unsigned long)a);
}
#endif
