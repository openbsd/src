/*	$OpenPackages$ */
/*	$OpenBSD: suff.c,v 1.52 2004/05/05 09:10:47 espie Exp $ */
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
 *	Suff_End		Cleanup the module
 *
 *	Suff_DoPaths		This function is used to make life easier
 *				when searching for a file according to its
 *				suffix. It takes the global search path,
 *				as defined using the .PATH: target, and appends
 *				its directories to the path of each of the
 *				defined suffixes, as specified using
 *				.PATH<suffix>: targets. In addition, all
 *				directories given for suffixes labeled as
 *				include files or libraries, using the .INCLUDES
 *				or .LIBS targets, are played with using
 *				Dir_MakeFlags to create the .INCLUDES and
 *				.LIBS global variables.
 *
 *	Suff_ClearSuffixes	Clear out all the suffixes and defined
 *				transformations.
 *
 *	Suff_IsTransform	Return true if the passed string is the lhs
 *				of a transformation rule.
 *
 *	Suff_AddSuffix		Add the passed string as another known suffix.
 *
 *	Suff_GetPath		Return the search path for the given suffix.
 *
 *	Suff_AddInclude 	Mark the given suffix as denoting an include
 *				file.
 *
 *	Suff_AddLib		Mark the given suffix as denoting a library.
 *
 *	Suff_AddTransform	Add another transformation to the suffix
 *				graph. Returns	GNode suitable for framing, I
 *				mean, tacking commands, attributes, etc. on.
 *
 *	Suff_SetNull		Define the suffix to consider the suffix of
 *				any file that doesn't have a known one.
 *
 *	Suff_FindDeps		Find implicit sources for and the location of
 *				a target based on its suffix. Returns the
 *				bottom-most node added to the graph or NULL
 *				if the target had no implicit sources.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "dir.h"
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

static LIST	 sufflist;	/* Lst of suffixes */
#ifdef CLEANUP
static LIST	 suffClean;	/* Lst of suffixes to be cleaned */
#endif
static LIST	 srclist;	/* Lst of sources */
static LIST	 transforms;	/* Lst of transformation rules */

static int	  sNum = 0;	/* Counter for assigning suffix numbers */

/*
 * Structure describing an individual suffix.
 */
typedef struct Suff_ {
    char	 *name; 	/* The suffix itself */
    int 	 nameLen;	/* Length of the suffix */
    short	 flags; 	/* Type of suffix */
#define SUFF_INCLUDE	  0x01	    /* One which is #include'd */
#define SUFF_LIBRARY	  0x02	    /* One which contains a library */
#define SUFF_NULL	  0x04	    /* The empty suffix */
    LIST	 searchPath;	/* The path along which files of this suffix
				 * may be found */
    int 	 sNum;		/* The suffix number */
    LIST	 parents;	/* Suffixes we have a transformation to */
    LIST	 children;	/* Suffixes we have a transformation from */
    LIST	 ref;		/* List of lists this suffix is referenced */
} Suff;

/*
 * Structure used in the search for implied sources.
 */
typedef struct Src_ {
    char	    *file;	/* The file to look for */
    char	    *pref;	/* Prefix from which file was formed */
    Suff	    *suff;	/* The suffix on the file */
    struct Src_     *parent;	/* The Src for which this is a source */
    GNode	    *node;	/* The node describing the file */
    int 	    children;	/* Count of existing children (so we don't free
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
    Lst 	   l;
    Src 	   *s;
} LstSrc;

static Suff	    *suffNull;	/* The NULL suffix for this run */
static Suff	    *emptySuff; /* The empty suffix required for POSIX
				 * single-suffix transformation rules */


static char *SuffStrIsPrefix(const char *, const char *);
static char *SuffSuffIsSuffix(Suff *, const char *);
static int SuffSuffIsSuffixP(void *, const void *);
static int SuffSuffIsPrefix(void *, const void *);
static int SuffHasNameP(void *, const void *);
static int GNodeHasNameP(void *, const void *);
static void SuffUnRef(Lst, Suff *);
#ifdef CLEANUP
static void SuffFree(void *);
#endif
static void SuffInsert(Lst, Suff *);
static bool SuffParseTransform(const char *, Suff **, Suff **);
static void SuffRebuildGraph(void *, void *);
static void SuffAddSrc(void *, void *);
static int SuffRemoveSrc(Lst);
static void SuffAddLevel(Lst, Src *);
static Src *SuffFindThem(Lst, Lst);
static Src *SuffFindCmds(Src *, Lst);
static void SuffExpandChildren(void *, void *);
static void SuffExpandVarChildren(LstNode, GNode *, GNode *);
static void SuffExpandWildChildren(LstNode, GNode *, GNode *);
static bool SuffApplyTransform(GNode *, GNode *, Suff *, Suff *);
static void SuffFindDeps(GNode *, Lst);
static void SuffFindArchiveDeps(GNode *, Lst);
static void SuffFindNormalDeps(GNode *, Lst);
static void SuffPrintName(void *);
static void SuffPrintSuff(void *);
static void SuffPrintTrans(void *);

static LstNode suff_find_by_name(const char *);
static LstNode transform_find_by_name(const char *);

#ifdef DEBUG_SRC
static void PrintAddr(void *);
#endif
	/*************** Lst Predicates ****************/
/*-
 *-----------------------------------------------------------------------
 * SuffStrIsPrefix  --
 *	See if prefix is a prefix of str.
 *
 * Results:
 *	NULL if it ain't, pointer to character in str after prefix if so
 *-----------------------------------------------------------------------
 */
static char    *
SuffStrIsPrefix(const char *prefix, const char *str)
{
    while (*str && *prefix == *str) {
	prefix++;
	str++;
    }

    return *prefix ? NULL : (char *)str;
}

/*-
 *-----------------------------------------------------------------------
 * SuffSuffIsSuffix  --
 *	See if suff is a suffix of str. str should point to the end of the
 *	string to check.
 *
 * Results:
 *	NULL if it ain't, pointer to first character of suffix in str if
 *	it is.
 *-----------------------------------------------------------------------
 */
static char *
SuffSuffIsSuffix(Suff *s, const char *str)
{
    const char	   *p1; 	/* Pointer into suffix name */
    const char	   *p2; 	/* Pointer into string being examined */

    p1 = s->name + s->nameLen;
    p2 = str;

    while (p1 != s->name) {
	p1--;
	p2--;
	if (*p1 != *p2)
		return NULL;
    }

    return (char *)p2;
}

/*-
 *-----------------------------------------------------------------------
 * SuffSuffIsSuffixP --
 *	Predicate form of SuffSuffIsSuffix. Passed as the callback function
 *	to Lst_Find.
 *
 * Results:
 *	0 if the suffix is the one desired, non-zero if not.
 *-----------------------------------------------------------------------
 */
static int
SuffSuffIsSuffixP(void *s, const void *str)
{
    return !SuffSuffIsSuffix((Suff *)s, (const char *)str);
}

static int
SuffHasNameP(void *s, const void *sname)
{
    return strcmp((const char *)sname, ((Suff *)s)->name);
}

static LstNode
suff_find_by_name(const char *name)
{
#ifdef STATS_SUFF
    STAT_SUFF_LOOKUP_NAME++;
#endif
    return Lst_FindConst(&sufflist, SuffHasNameP, name);
}

static int
GNodeHasNameP(void *gn, const void *name)
{
    return strcmp((const char *)name, ((GNode *)gn)->name);
}

static LstNode
transform_find_by_name(const char *name)
{
#ifdef STATS_SUFF
    STAT_TRANSFORM_LOOKUP_NAME++;
#endif
    return Lst_FindConst(&transforms, GNodeHasNameP, name);
}
/*-
 *-----------------------------------------------------------------------
 * SuffSuffIsPrefix  --
 *	See if the suffix described by s is a prefix of the string. Care
 *	must be taken when using this to search for transformations and
 *	what-not, since there could well be two suffixes, one of which
 *	is a prefix of the other...
 *
 * Results:
 *	0 if s is a prefix of str. non-zero otherwise
 *-----------------------------------------------------------------------
 */
static int
SuffSuffIsPrefix(void *s, const void *str)
{
    return SuffStrIsPrefix(((Suff *)s)->name, (const char *)str) == NULL ? 1 : 0;
}

	    /*********** Maintenance Functions ************/

static void
SuffUnRef(Lst l, Suff *sp)
{
    LstNode ln = Lst_Member(l, sp);
    if (ln != NULL)
	Lst_Remove(l, ln);
}

#ifdef CLEANUP
/*-
 *-----------------------------------------------------------------------
 * SuffFree  --
 *	Free up all memory associated with the given suffix structure.
 *
 * Side Effects:
 *	the suffix entry is detroyed
 *-----------------------------------------------------------------------
 */
static void
SuffFree(void *sp)
{
    Suff	*s = (Suff *)sp;

    if (s == suffNull)
	suffNull = NULL;

    if (s == emptySuff)
	emptySuff = NULL;

    Lst_Destroy(&s->ref, NOFREE);
    Lst_Destroy(&s->children, NOFREE);
    Lst_Destroy(&s->parents, NOFREE);
    Lst_Destroy(&s->searchPath, Dir_Destroy);

    free(s->name);
    free(s);
}
#endif


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
    LstNode	  ln;		/* current element in l we're examining */
    Suff	  *s2 = NULL;	/* the suffix descriptor in this element */

    for (ln = Lst_First(l); ln != NULL; ln = Lst_Adv(ln)) {
	s2 = (Suff *)Lst_Datum(ln);
	if (s2->sNum >= s->sNum)
	    break;
    }

    if (DEBUG(SUFF)) {
	printf("inserting %s(%d)...", s->name, s->sNum);
    }
    if (ln == NULL) {
	if (DEBUG(SUFF)) {
	    printf("at end of list\n");
	}
	Lst_AtEnd(l, s);
	Lst_AtEnd(&s->ref, l);
    } else if (s2->sNum != s->sNum) {
	if (DEBUG(SUFF)) {
	    printf("before %s(%d)\n", s2->name, s2->sNum);
	}
	Lst_Insert(l, ln, s);
	Lst_AtEnd(&s->ref, l);
    } else if (DEBUG(SUFF)) {
	printf("already there\n");
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_ClearSuffixes --
 *	This is gross. Nuke the list of suffixes but keep all transformation
 *	rules around. The transformation graph is destroyed in this process,
 *	but we leave the list of rules so when a new graph is formed the rules
 *	will remain.
 *	This function is called from the parse module when a
 *	.SUFFIXES:\n line is encountered.
 *
 * Side Effects:
 *	the sufflist and its graph nodes are destroyed
 *-----------------------------------------------------------------------
 */
void
Suff_ClearSuffixes(void)
{
#ifdef CLEANUP
    Lst_ConcatDestroy(&suffClean, &sufflist);
#endif
    Lst_Init(&sufflist);
    sNum = 0;
    suffNull = emptySuff;
}

/*-
 *-----------------------------------------------------------------------
 * SuffParseTransform --
 *	Parse a transformation string to find its two component suffixes.
 *
 * Results:
 *	true if the string is a valid transformation and false otherwise.
 *
 * Side Effects:
 *	The passed pointers are overwritten.
 *-----------------------------------------------------------------------
 */
static bool
SuffParseTransform(
    const char		*str,		/* String being parsed */
    Suff		**srcPtr,	/* Place to store source of trans. */
    Suff		**targPtr)	/* Place to store target of trans. */
{
    LstNode		srcLn;	    /* element in suffix list of trans source*/
    Suff		*src;	    /* Source of transformation */
    LstNode		targLn;     /* element in suffix list of trans target*/
    const char		*str2;	    /* Extra pointer (maybe target suffix) */
    LstNode		singleLn;   /* element in suffix list of any suffix
				     * that exactly matches str */
    Suff		*single = NULL;/* Source of possible transformation to
				     * null suffix */

    srcLn = NULL;
    singleLn = NULL;

    /*
     * Loop looking first for a suffix that matches the start of the
     * string and then for one that exactly matches the rest of it. If
     * we can find two that meet these criteria, we've successfully
     * parsed the string.
     */
    for (;;) {
	if (srcLn == NULL)
	    srcLn = Lst_FindConst(&sufflist, SuffSuffIsPrefix, str);
	else
	    srcLn = Lst_FindFromConst(Lst_Succ(srcLn), SuffSuffIsPrefix, str);
	if (srcLn == NULL) {
	    /*
	     * Ran out of source suffixes -- no such rule
	     */
	    if (singleLn != NULL) {
		/*
		 * Not so fast Mr. Smith! There was a suffix that encompassed
		 * the entire string, so we assume it was a transformation
		 * to the null suffix (thank you POSIX). We still prefer to
		 * find a double rule over a singleton, hence we leave this
		 * check until the end.
		 *
		 * XXX: Use emptySuff over suffNull?
		 */
		*srcPtr = single;
		*targPtr = suffNull;
		return true;
	    }
	    return false;
	}
	src = (Suff *)Lst_Datum(srcLn);
	str2 = str + src->nameLen;
	if (*str2 == '\0') {
	    single = src;
	    singleLn = srcLn;
	} else {
	    targLn = suff_find_by_name(str2);
	    if (targLn != NULL) {
		*srcPtr = src;
		*targPtr = (Suff *)Lst_Datum(targLn);
		return true;
	    }
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_IsTransform  --
 *	Return true if the given string is a transformation rule
 *
 * Results:
 *	true if the string is a concatenation of two known suffixes.
 *	false otherwise
 *-----------------------------------------------------------------------
 */
bool
Suff_IsTransform(const char *str)
{
    Suff	  *src, *targ;

    return SuffParseTransform(str, &src, &targ);
}

/*-
 *-----------------------------------------------------------------------
 * Suff_AddTransform --
 *	Add the transformation rule described by the line to the
 *	list of rules and place the transformation itself in the graph
 *
 * Results:
 *	The node created for the transformation in the transforms list
 *
 * Side Effects:
 *	The node is placed on the end of the transforms Lst and links are
 *	made between the two suffixes mentioned in the target name
 *-----------------------------------------------------------------------
 */
GNode *
Suff_AddTransform(const char *line)
{
    GNode	  *gn;		/* GNode of transformation rule */
    Suff	  *s,		/* source suffix */
		  *t;		/* target suffix */
    LstNode	  ln;		/* Node for existing transformation */

    ln = transform_find_by_name(line);
    if (ln == NULL) {
	/*
	 * Make a new graph node for the transformation. It will be filled in
	 * by the Parse module.
	 */
	gn = Targ_NewGN(line);
	Lst_AtEnd(&transforms, gn);
    } else {
	/*
	 * New specification for transformation rule. Just nuke the old list
	 * of commands so they can be filled in again... We don't actually
	 * free the commands themselves, because a given command can be
	 * attached to several different transformations.
	 */
	gn = (GNode *)Lst_Datum(ln);
	Lst_Destroy(&gn->commands, NOFREE);
	Lst_Init(&gn->commands);
	Lst_Destroy(&gn->children, NOFREE);
	Lst_Init(&gn->children);
    }

    gn->type = OP_TRANSFORM;

    (void)SuffParseTransform(line, &s, &t);

    /*
     * link the two together in the proper relationship and order
     */
    if (DEBUG(SUFF)) {
	printf("defining transformation from `%s' to `%s'\n",
		s->name, t->name);
    }
    SuffInsert(&t->children, s);
    SuffInsert(&s->parents, t);

    return gn;
}

/*-
 *-----------------------------------------------------------------------
 * Suff_EndTransform --
 *	Handle the finish of a transformation definition, removing the
 *	transformation from the graph if it has neither commands nor
 *	sources. This is a callback procedure for the Parse module via
 *	Lst_ForEach
 *
 * Side Effects:
 *	If the node has no commands or children, the children and parents
 *	lists of the affected suffices are altered.
 *-----------------------------------------------------------------------
 */
void
Suff_EndTransform(void *gnp)
{
    GNode *gn = (GNode *)gnp;

    if ((gn->type & OP_TRANSFORM) && Lst_IsEmpty(&gn->commands) &&
	Lst_IsEmpty(&gn->children))
    {
	Suff	*s, *t;

	(void)SuffParseTransform(gn->name, &s, &t);

	if (DEBUG(SUFF)) {
	    printf("deleting transformation from `%s' to `%s'\n",
		    s->name, t->name);
	}

	/*
	 * Remove the source from the target's children list.
	 *
	 * We'll be called twice when the next target is seen, but .c and .o
	 * are only linked once...
	 */
	SuffUnRef(&t->children, s);

	/*
	 * Remove the target from the source's parents list
	 */
	if (s != NULL)
	    SuffUnRef(&s->parents, t);
    } else if ((gn->type & OP_TRANSFORM) && DEBUG(SUFF)) {
	printf("transformation %s complete\n", gn->name);
    }
}

/*-
 *-----------------------------------------------------------------------
 * SuffRebuildGraph --
 *	Called from Suff_AddSuffix via Lst_ForEach to search through the
 *	list of existing transformation rules and rebuild the transformation
 *	graph when it has been destroyed by Suff_ClearSuffixes. If the
 *	given rule is a transformation involving this suffix and another,
 *	existing suffix, the proper relationship is established between
 *	the two.
 *
 * Side Effects:
 *	The appropriate links will be made between this suffix and
 *	others if transformation rules exist for it.
 *-----------------------------------------------------------------------
 */
static void
SuffRebuildGraph(
    void	*transformp,	/* Transformation to test */
    void	*sp)		/* Suffix to rebuild */
{
    GNode	*transform = (GNode *)transformp;
    Suff	*s = (Suff *)sp;
    char	*cp;
    LstNode	ln;
    Suff	*s2;

    /* First see if it is a transformation from this suffix.  */
    cp = SuffStrIsPrefix(s->name, transform->name);
    if (cp != NULL) {
	ln = suff_find_by_name(cp);
	if (ln != NULL) {
	    /* Found target. Link in and return, since it can't be anything
	     * else.  */
	    s2 = (Suff *)Lst_Datum(ln);
	    SuffInsert(&s2->children, s);
	    SuffInsert(&s->parents, s2);
	    return;
	}
    }

    /* Not from, maybe to?  */
    cp = SuffSuffIsSuffix(s, transform->name + strlen(transform->name));
    if (cp != NULL) {
	/* Null-terminate the source suffix in order to find it.  */
	*cp = '\0';
	ln = suff_find_by_name(transform->name);
	/* Replace the start of the target suffix.  */
	*cp = s->name[0];
	if (ln != NULL) {
	    /* Found it -- establish the proper relationship.  */
	    s2 = (Suff *)Lst_Datum(ln);
	    SuffInsert(&s->children, s2);
	    SuffInsert(&s2->parents, s);
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_AddSuffix --
 *	Add the suffix in string to the end of the list of known suffixes.
 *	Should we restructure the suffix graph? Make doesn't...
 *
 * Side Effects:
 *	A GNode is created for the suffix and a Suff structure is created and
 *	added to the suffixes list unless the suffix was already known.
 *-----------------------------------------------------------------------
 */
void
Suff_AddSuffix(const char *str)
{
    Suff	  *s;	    /* new suffix descriptor */
    LstNode	  ln;

    ln = suff_find_by_name(str);
    if (ln == NULL) {
	s = emalloc(sizeof(Suff));

	s->name =	estrdup(str);
	s->nameLen =	strlen(s->name);
	Lst_Init(&s->searchPath);
	Lst_Init(&s->children);
	Lst_Init(&s->parents);
	Lst_Init(&s->ref);
	s->sNum =	sNum++;
	s->flags =	0;

	Lst_AtEnd(&sufflist, s);
	/*
	 * Look for any existing transformations from or to this suffix.
	 * XXX: Only do this after a Suff_ClearSuffixes?
	 */
	Lst_ForEach(&transforms, SuffRebuildGraph, s);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_GetPath --
 *	Return the search path for the given suffix, if it's defined.
 *
 * Results:
 *	The searchPath for the desired suffix or NULL if the suffix isn't
 *	defined.
 *-----------------------------------------------------------------------
 */
Lst
Suff_GetPath(const char *sname)
{
    LstNode	  ln;
    Suff	  *s;

    ln = suff_find_by_name(sname);
    if (ln == NULL) {
	return NULL;
    } else {
	s = (Suff *)Lst_Datum(ln);
	return &s->searchPath;
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_DoPaths --
 *	Extend the search paths for all suffixes to include the default
 *	search path.
 *
 * Side Effects:
 *	The searchPath field of all the suffixes is extended by the
 *	directories in dirSearchPath. If paths were specified for the
 *	".h" suffix, the directories are stuffed into a global variable
 *	called ".INCLUDES" with each directory preceeded by a -I. The same
 *	is done for the ".a" suffix, except the variable is called
 *	".LIBS" and the flag is -L.
 *-----------------------------------------------------------------------
 */
void
Suff_DoPaths(void)
{
    Suff		*s;
    LstNode		ln;
    char		*ptr;
    LIST		inIncludes; /* Cumulative .INCLUDES path */
    LIST		inLibs;     /* Cumulative .LIBS path */

    Lst_Init(&inIncludes);
    Lst_Init(&inLibs);

    for (ln = Lst_First(&sufflist); ln != NULL; ln = Lst_Adv(ln)) {
	s = (Suff *)Lst_Datum(ln);
	if (!Lst_IsEmpty(&s->searchPath)) {
#ifdef INCLUDES
	    if (s->flags & SUFF_INCLUDE) {
		Dir_Concat(&inIncludes, &s->searchPath);
	    }
#endif /* INCLUDES */
#ifdef LIBRARIES
	    if (s->flags & SUFF_LIBRARY) {
		Dir_Concat(&inLibs, &s->searchPath);
	    }
#endif /* LIBRARIES */
	    Dir_Concat(&s->searchPath, dirSearchPath);
	} else
	    Lst_Clone(&s->searchPath, dirSearchPath, Dir_CopyDir);
    }

    Var_Set(".INCLUDES", ptr = Dir_MakeFlags("-I", &inIncludes), VAR_GLOBAL);
    free(ptr);
    Var_Set(".LIBS", ptr = Dir_MakeFlags("-L", &inLibs), VAR_GLOBAL);
    free(ptr);

    Lst_Destroy(&inIncludes, Dir_Destroy);
    Lst_Destroy(&inLibs, Dir_Destroy);
}

/*-
 *-----------------------------------------------------------------------
 * Suff_AddInclude --
 *	Add the given suffix as a type of file which gets included.
 *	Called from the parse module when a .INCLUDES line is parsed.
 *	The suffix must have already been defined.
 *
 * Side Effects:
 *	The SUFF_INCLUDE bit is set in the suffix's flags field
 *-----------------------------------------------------------------------
 */
void
Suff_AddInclude(const char *sname)	/* Name of suffix to mark */
{
    LstNode	  ln;
    Suff	  *s;

    ln = suff_find_by_name(sname);
    if (ln != NULL) {
	s = (Suff *)Lst_Datum(ln);
	s->flags |= SUFF_INCLUDE;
    }
}

/*-
 *-----------------------------------------------------------------------
 * Suff_AddLib --
 *	Add the given suffix as a type of file which is a library.
 *	Called from the parse module when parsing a .LIBS line. The
 *	suffix must have been defined via .SUFFIXES before this is
 *	called.
 *
 * Side Effects:
 *	The SUFF_LIBRARY bit is set in the suffix's flags field
 *-----------------------------------------------------------------------
 */
void
Suff_AddLib(const char *sname)	/* Name of suffix to mark */
{
    LstNode	  ln;
    Suff	  *s;

    ln = suff_find_by_name(sname);
    if (ln != NULL) {
	s = (Suff *)Lst_Datum(ln);
	s->flags |= SUFF_LIBRARY;
    }
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
    void *sp,	    /* suffix for which to create a Src structure */
    void *lsp)	    /* list and parent for the new Src */
{
    Suff	*s = (Suff *)sp;
    LstSrc	*ls = (LstSrc *)lsp;
    Src 	*s2;	    /* new Src structure */
    Src 	*targ;	    /* Target structure */

    targ = ls->s;

    if ((s->flags & SUFF_NULL) && *s->name != '\0') {
	/*
	 * If the suffix has been marked as the NULL suffix, also create a Src
	 * structure for a file with no suffix attached. Two birds, and all
	 * that...
	 */
	s2 = emalloc(sizeof(Src));
	s2->file =	estrdup(targ->pref);
	s2->pref =	targ->pref;
	s2->parent =	targ;
	s2->node =	NULL;
	s2->suff =	s;
	s2->children =	0;
	targ->children += 1;
	Lst_AtEnd(ls->l, s2);
#ifdef DEBUG_SRC
	Lst_Init(&s2->cp);
	Lst_AtEnd(&targ->cp, s2);
	printf("1 add %x %x to %x:", targ, s2, ls->l);
	Lst_Every(ls->l, PrintAddr);
	printf("\n");
#endif
    }
    s2 = emalloc(sizeof(Src));
    s2->file =	    Str_concat(targ->pref, s->name, 0);
    s2->pref =	    targ->pref;
    s2->parent =    targ;
    s2->node =	    NULL;
    s2->suff =	    s;
    s2->children =  0;
    targ->children += 1;
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
    Lst 	   l,		/* list to which to add the new level */
    Src 	   *targ)	/* Src structure to use as the parent */
{
    LstSrc	   ls;

    ls.s = targ;
    ls.l = l;

    Lst_ForEach(&targ->suff->children, SuffAddSrc, &ls);
}

/*-
 *----------------------------------------------------------------------
 * SuffRemoveSrc --
 *	Free all src structures in list that don't have a reference count
 *
 * Results:
 *	Ture if an src was removed
 *
 * Side Effects:
 *	The memory is free'd.
 *----------------------------------------------------------------------
 */
static int
SuffRemoveSrc(Lst l)
{
    LstNode ln;
    Src *s;
    int t = 0;

#ifdef DEBUG_SRC
    printf("cleaning %lx: ", (unsigned long)l);
    Lst_Every(l, PrintAddr);
    printf("\n");
#endif


    for (ln = Lst_First(l); ln != NULL; ln = Lst_Adv(ln)) {
	s = (Src *)Lst_Datum(ln);
	if (s->children == 0) {
	    free(s->file);
	    if (!s->parent)
		free(s->pref);
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
	    t |= 1;
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

    return t;
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
    Lst 	   srcs,	/* list of Src structures to search through */
    Lst 	   slst)
{
    Src 	   *s;		/* current Src */
    Src 	   *rs; 	/* returned Src */
    char	   *ptr;

    rs = NULL;

    while ((s = (Src *)Lst_DeQueue(srcs)) != NULL) {
	if (DEBUG(SUFF)) {
	    printf("\ttrying %s...", s->file);
	}

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

	if ((ptr = Dir_FindFile(s->file, &s->suff->searchPath)) != NULL) {
	    rs = s;
#ifdef DEBUG_SRC
	    printf("remove %x from %x\n", s, srcs);
#endif
	    free(ptr);
	    break;
	}

	if (DEBUG(SUFF)) {
	    printf("not there\n");
	}

	SuffAddLevel(srcs, s);
	Lst_AtEnd(slst, s);
    }

    if (DEBUG(SUFF) && rs) {
	printf("got it\n");
    }
    return rs;
}

/*-
 *-----------------------------------------------------------------------
 * SuffFindCmds --
 *	See if any of the children of the target in the Src structure is
 *	one from which the target can be transformed. If there is one,
 *	a Src structure is put together for it and returned.
 *
 * Results:
 *	The Src structure of the "winning" child, or NULL if no such beast.
 *
 * Side Effects:
 *	A Src structure may be allocated.
 *-----------------------------------------------------------------------
 */
static Src *
SuffFindCmds(
    Src 	*targ,	/* Src structure to play with */
    Lst 	slst)
{
    LstNode		ln;	/* General-purpose list node */
    GNode		*t,	/* Target GNode */
			*s;	/* Source GNode */
    int 		prefLen;/* The length of the defined prefix */
    Suff		*suff;	/* Suffix on matching beastie */
    Src 		*ret;	/* Return value */
    const char		*cp;

    t = targ->node;
    prefLen = strlen(targ->pref);

    for (ln = Lst_First(&t->children); ln != NULL; ln = Lst_Adv(ln)) {
	s = (GNode *)Lst_Datum(ln);

	cp = strrchr(s->name, '/');
	if (cp == NULL) {
	    cp = s->name;
	} else {
	    cp++;
	}
	if (strncmp(cp, targ->pref, prefLen) == 0) {
	    /* The node matches the prefix ok, see if it has a known
	     * suffix.	*/
	    LstNode ln2;
	    ln2 = suff_find_by_name(&cp[prefLen]);
	    if (ln2 != NULL) {
		/*
		 * It even has a known suffix, see if there's a transformation
		 * defined between the node's suffix and the target's suffix.
		 *
		 * XXX: Handle multi-stage transformations here, too.
		 */
		suff = (Suff *)Lst_Datum(ln2);

		if (Lst_Member(&suff->parents, targ->suff) != NULL) {
		    /*
		     * Hot Damn! Create a new Src structure to describe
		     * this transformation (making sure to duplicate the
		     * source node's name so Suff_FindDeps can free it
		     * again (ick)), and return the new structure.
		     */
		    ret = emalloc(sizeof(Src));
		    ret->file = estrdup(s->name);
		    ret->pref = targ->pref;
		    ret->suff = suff;
		    ret->parent = targ;
		    ret->node = s;
		    ret->children = 0;
		    targ->children += 1;
#ifdef DEBUG_SRC
		    Lst_Init(&ret->cp);
		    printf("3 add %x %x\n", targ, ret);
		    Lst_AtEnd(&targ->cp, ret);
#endif
		    Lst_AtEnd(slst, ret);
		    if (DEBUG(SUFF)) {
			printf ("\tusing existing source %s\n", s->name);
		    }
		    return ret;
		}
	    }
	}
    }
    return NULL;
}

static void
SuffExpandVarChildren(LstNode after, GNode *cgn, GNode *pgn)
{
    GNode	*gn;		/* New source 8) */
    char	*cp;		/* Expanded value */
    LIST	members;


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
	char	*sacrifice = cp;

	(void)Arch_ParseArchive(&sacrifice, &members, &pgn->context);
    } else {
	/* Break the result into a vector of strings whose nodes
	 * we can find, then add those nodes to the members list.
	 * Unfortunately, we can't use brk_string because it
	 * doesn't understand about variable specifications with
	 * spaces in them...  */
	char	    *start, *cp2;

	for (start = cp; *start == ' ' || *start == '\t'; start++)
	    continue;
	for (cp2 = start; *cp2 != '\0';) {
	    if (isspace(*cp2)) {
		/* White-space -- terminate element, find the node,
		 * add it, skip any further spaces.  */
		gn = Targ_FindNodei(start, cp2, TARG_CREATE);
		cp2++;
		Lst_AtEnd(&members, gn);
		while (isspace(*cp2))
		    cp2++;
		/* Adjust cp2 for increment at start of loop, but
		 * set start to first non-space.  */
		start = cp2;
	    } else if (*cp2 == '$')
		/* Start of a variable spec -- contact variable module
		 * to find the end so we can skip over it.  */
		cp2 += Var_ParseSkip(cp2, &pgn->context, NULL);
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
    while ((gn = (GNode *)Lst_DeQueue(&members)) != NULL) {
	if (DEBUG(SUFF))
	    printf("%s...", gn->name);
	if (Lst_Member(&pgn->children, gn) == NULL) {
	    Lst_Append(&pgn->children, after, gn);
	    after = Lst_Adv(after);
	    Lst_AtEnd(&gn->parents, pgn);
	    pgn->unmade++;
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
    LstNode	ln;		/* List element for old source */
    char	*cp;		/* Expanded value */

    LIST	exp;	    /* List of expansions */
    Lst 	path;	    /* Search path along which to expand */

    if (DEBUG(SUFF))
	printf("Wildcard expanding \"%s\"...", cgn->name);

    /* Find a path along which to expand the word.
     *
     * If the word has a known suffix, use that path.
     * If it has no known suffix and we're allowed to use the null
     *	 suffix, use its path.
     * Else use the default system search path.  */
    cp = cgn->name + strlen(cgn->name);
    ln = Lst_FindConst(&sufflist, SuffSuffIsSuffixP, cp);

    if (ln != NULL) {
	Suff	*s = (Suff *)Lst_Datum(ln);

	if (DEBUG(SUFF))
	    printf("suffix is \"%s\"...", s->name);
	path = &s->searchPath;
    } else
	/* Use default search path.  */
	path = dirSearchPath;

    /* Expand the word along the chosen path. */
    Lst_Init(&exp);
    Dir_Expand(cgn->name, path, &exp);

    /* Fetch next expansion off the list and find its GNode.  */
    while ((cp = (char *)Lst_DeQueue(&exp)) != NULL) {
	GNode		*gn;		/* New source 8) */
	if (DEBUG(SUFF))
	    printf("%s...", cp);
	gn = Targ_FindNode(cp, TARG_CREATE);

	/* If gn isn't already a child of the parent, make it so and
	 * up the parent's count of unmade children.  */
	if (Lst_Member(&pgn->children, gn) == NULL) {
	    Lst_Append(&pgn->children, after, gn);
	    after = Lst_Adv(after);
	    Lst_AtEnd(&gn->parents, pgn);
	    pgn->unmade++;
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
SuffExpandChildren(
    void	*cgnp,		/* Child to examine */
    void	*pgnp)		/* Parent node being processed */
{
    GNode	*cgn = (GNode *)cgnp;
    GNode	*pgn = (GNode *)pgnp;
    LstNode	ln;
    /* New nodes effectively take the place of the child, so we place them
     * after the child.  */
    ln = Lst_Member(&pgn->children, cgn);

    /* First do variable expansion -- this takes precedence over
     * wildcard expansion. If the result contains wildcards, they'll be gotten
     * to later since the resulting words are tacked on to the end of
     * the children list.  */
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
    LstNode	np;	    /* Next node for loop */
    char	*tname; /* Name of transformation rule */
    GNode	*gn;	/* Node for same */

    if (Lst_AddNew(&tGn->children, sGn)) {
	/* Not already linked, so form the proper links between the
	 * target and source.  */
	Lst_AtEnd(&sGn->parents, tGn);
	tGn->unmade++;
    }

    if ((sGn->type & OP_OPMASK) == OP_DOUBLEDEP) {
	/* When a :: node is used as the implied source of a node, we have
	 * to link all its cohorts in as sources as well. Only the initial
	 * sGn gets the target in its iParents list, however, as that
	 * will be sufficient to get the .IMPSRC variable set for tGn.	*/
	for (ln=Lst_First(&sGn->cohorts); ln != NULL; ln=Lst_Adv(ln)) {
	    gn = (GNode *)Lst_Datum(ln);

	    if (Lst_AddNew(&tGn->children, gn)) {
		/* Not already linked, so form the proper links between the
		 * target and source.  */
		Lst_AtEnd(&gn->parents, tGn);
		tGn->unmade++;
	    }
	}
    }
    /* Locate the transformation rule itself.  */
    tname = Str_concat(s->name, t->name, 0);
    ln = transform_find_by_name(tname);
    free(tname);

    if (ln == NULL)
	/*
	 * Not really such a transformation rule (can happen when we're
	 * called to link an OP_MEMBER and OP_ARCHV node), so return
	 * false.
	 */
	return false;

    gn = (GNode *)Lst_Datum(ln);

    if (DEBUG(SUFF))
	printf("\tapplying %s -> %s to \"%s\"\n", s->name, t->name, tGn->name);

    /* Record last child for expansion purposes.  */
    ln = Lst_Last(&tGn->children);

    /* Pass the buck to Make_HandleUse to apply the rule.  */
    Make_HandleUse(gn, tGn);

    /* Deal with wildcards and variables in any acquired sources.  */
    for (ln = Lst_Succ(ln); ln != NULL; ln = np) {
    	np = Lst_Adv(ln);
	SuffExpandChildren(Lst_Datum(ln), tGn);
    }

    /* Keep track of another parent to which this beast is transformed so
     * the .IMPSRC variable can be set correctly for the parent.  */
    Lst_AtEnd(&sGn->iParents, tGn);

    return true;
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
    GNode	*gn,	    /* Node for which to locate dependencies */
    Lst 	slst)
{
    char	*eoarch;    /* End of archive portion */
    char	*eoname;    /* End of member portion */
    GNode	*mem;	    /* Node for member */
    Suff	*ms;	    /* Suffix descriptor for member */
    char	*name;	    /* Start of member's name */

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
    if (Lst_AddNew(&gn->children, mem)) {
	Lst_AtEnd(&mem->parents, gn);
	gn->unmade++;
    }

    /* Copy variables from member node to this one.  */
    Varq_Set(TARGET_INDEX, Varq_Value(TARGET_INDEX, mem), gn);
    Varq_Set(PREFIX_INDEX, Varq_Value(PREFIX_INDEX, mem), gn);

    ms = mem->suffix;
    if (ms == NULL) {
	/* Didn't know what it was -- use .NULL suffix if not in make mode.  */
	if (DEBUG(SUFF))
	    printf("using null suffix\n");
	ms = suffNull;
    }


    /* Set the other two local variables required for this target.  */
    Varq_Set(MEMBER_INDEX, mem->name, gn);
    Varq_Set(ARCHIVE_INDEX, gn->name, gn);

    if (ms != NULL) {
	/*
	 * Member has a known suffix, so look for a transformation rule from
	 * it to a possible suffix of the archive. Rather than searching
	 * through the entire list, we just look at suffixes to which the
	 * member's suffix may be transformed...
	 */
	LstNode     ln;

	/* Use first matching suffix...  */
	ln = Lst_FindConst(&ms->parents, SuffSuffIsSuffixP, eoarch);

	if (ln != NULL) {
	    /* Got one -- apply it.  */
	    if (!SuffApplyTransform(gn, mem, (Suff *)Lst_Datum(ln), ms) &&
		DEBUG(SUFF))
		printf("\tNo transformation from %s -> %s\n",
		       ms->name, ((Suff *)Lst_Datum(ln))->name);
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
    char	*eoname;    /* End of name */
    char	*sopref;    /* Start of prefix */
    LstNode	ln;	    /* Next suffix node to check */
    LstNode	np;
    LIST	srcs;	    /* List of sources at which to look */
    LIST	targs;	    /* List of targets to which things can be
			     * transformed. They all have the same file,
			     * but different suff and pref fields */
    Src 	*bottom;    /* Start of found transformation path */
    Src 	*src;	    /* General Src pointer */
    char	*pref;	    /* Prefix to use */
    Src 	*targ;	    /* General Src target pointer */


    eoname = gn->name + strlen(gn->name);

    sopref = gn->name;

    /* Begin at the beginning...  */
    ln = Lst_First(&sufflist);
    Lst_Init(&srcs);
    Lst_Init(&targs);

    /* We're caught in a catch-22 here. On the one hand, we want to use any
     * transformation implied by the target's sources, but we can't examine
     * the sources until we've expanded any variables/wildcards they may hold,
     * and we can't do that until we've set up the target's local variables
     * and we can't do that until we know what the proper suffix for the
     * target is (in case there are two suffixes one of which is a suffix of
     * the other) and we can't know that until we've found its implied
     * source, which we may not want to use if there's an existing source
     * that implies a different transformation.
     *
     * In an attempt to get around this, which may not work all the time,
     * but should work most of the time, we look for implied sources first,
     * checking transformations to all possible suffixes of the target,
     * use what we find to set the target's local variables, expand the
     * children, then look for any overriding transformations they imply.
     * Should we find one, we discard the one we found before.	*/

    while (ln != NULL) {
	/* Look for next possible suffix...  */
	ln = Lst_FindFromConst(ln, SuffSuffIsSuffixP, eoname);

	if (ln != NULL) {
	    int     prefLen;	    /* Length of the prefix */
	    Src     *targ;

	    /* Allocate a Src structure to which things can be transformed.  */
	    targ = emalloc(sizeof(Src));
	    targ->file = estrdup(gn->name);
	    targ->suff = (Suff *)Lst_Datum(ln);
	    targ->node = gn;
	    targ->parent = NULL;
	    targ->children = 0;
#ifdef DEBUG_SRC
	    Lst_Init(&targ->cp);
#endif

	    /* Allocate room for the prefix, whose end is found by subtracting
	     * the length of the suffix from the end of the name.  */
	    prefLen = (eoname - targ->suff->nameLen) - sopref;
	    targ->pref = emalloc(prefLen + 1);
	    memcpy(targ->pref, sopref, prefLen);
	    targ->pref[prefLen] = '\0';

	    /* Add nodes from which the target can be made.  */
	    SuffAddLevel(&srcs, targ);

	    /* Record the target so we can nuke it.  */
	    Lst_AtEnd(&targs, targ);

	    /* Search from this suffix's successor...  */
	    ln = Lst_Succ(ln);
	}
    }

    /* Handle target of unknown suffix...  */
    if (Lst_IsEmpty(&targs) && suffNull != NULL) {
	if (DEBUG(SUFF)) {
	    printf("\tNo known suffix on %s. Using .NULL suffix\n", gn->name);
	}

	targ = emalloc(sizeof(Src));
	targ->file = estrdup(gn->name);
	targ->suff = suffNull;
	targ->node = gn;
	targ->parent = NULL;
	targ->children = 0;
	targ->pref = estrdup(sopref);
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

    /* Using the list of possible sources built up from the target suffix(es),
     * try and find an existing file/target that matches.  */
    bottom = SuffFindThem(&srcs, slst);

    if (bottom == NULL) {
	/* No known transformations -- use the first suffix found for setting
	 * the local variables.  */
	if (!Lst_IsEmpty(&targs))
	    targ = (Src *)Lst_Datum(Lst_First(&targs));
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
    Varq_Set(TARGET_INDEX, gn->name, gn);

    pref = targ != NULL ? targ->pref : gn->name;
    Varq_Set(PREFIX_INDEX, pref, gn);

    /* Now we've got the important local variables set, expand any sources
     * that still contain variables or wildcards in their names.  */
    for (ln = Lst_First(&gn->children); ln != NULL; ln = np) {
    	np = Lst_Adv(ln);
	SuffExpandChildren(Lst_Datum(ln), gn);
    }

    if (targ == NULL) {
	if (DEBUG(SUFF))
	    printf("\tNo valid suffix on %s\n", gn->name);

sfnd_abort:
	/* Deal with finding the thing on the default search path if the
	 * node is only a source (not on the lhs of a dependency operator
	 * or [XXX] it has neither children or commands).  */
	if (OP_NOP(gn->type) ||
	    (Lst_IsEmpty(&gn->children) && Lst_IsEmpty(&gn->commands)))
	{
	    gn->path = Dir_FindFile(gn->name,
				    (targ == NULL ? dirSearchPath :
				     &targ->suff->searchPath));
	    if (gn->path != NULL) {
		char *ptr;
		Varq_Set(TARGET_INDEX, gn->path, gn);

		if (targ != NULL) {
		    /* Suffix known for the thing -- trim the suffix off
		     * the path to form the proper .PREFIX variable.  */
		    int 	savep = strlen(gn->path) - targ->suff->nameLen;
		    char	savec;

		    gn->suffix = targ->suff;

		    savec = gn->path[savep];
		    gn->path[savep] = '\0';

		    if ((ptr = strrchr(gn->path, '/')) != NULL)
			ptr++;
		    else
			ptr = gn->path;

		    Varq_Set(PREFIX_INDEX, ptr, gn);

		    gn->path[savep] = savec;
		} else {
		    /* The .PREFIX gets the full path if the target has
		     * no known suffix.  */
		    gn->suffix = NULL;

		    if ((ptr = strrchr(gn->path, '/')) != NULL)
			ptr++;
		    else
			ptr = gn->path;

		    Varq_Set(PREFIX_INDEX, ptr, gn);
		}
	    }
	} else {
	    /* Not appropriate to search for the thing -- set the
	     * path to be the name so Dir_MTime won't go grovelling for
	     * it.  */
	    gn->suffix = targ == NULL ? NULL : targ->suff;
	    efree(gn->path);
	    gn->path = estrdup(gn->name);
	}

	goto sfnd_return;
    }

    /* If the suffix indicates that the target is a library, mark that in
     * the node's type field.  */
    if (targ->suff->flags & SUFF_LIBRARY) {
	gn->type |= OP_LIB;
    }

    /* Check for overriding transformation rule implied by sources.  */
    if (!Lst_IsEmpty(&gn->children)) {
	src = SuffFindCmds(targ, slst);

	if (src != NULL) {
	    /* Free up all the Src structures in the transformation path
	     * up to, but not including, the parent node.  */
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

    /* We now have a list of Src structures headed by 'bottom' and linked via
     * their 'parent' pointers. What we do next is create links between
     * source and target nodes (which may or may not have been created)
     * and set the necessary local variables in each target. The
     * commands for each target are set from the commands of the
     * transformation rule used to get from the src suffix to the targ
     * suffix. Note that this causes the commands list of the original
     * node, gn, to be replaced by the commands of the final
     * transformation rule. Also, the unmade field of gn is incremented.
     * Etc.  */
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
	    /* Finish off the dependency-search process for any nodes
	     * between bottom and gn (no point in questing around the
	     * filesystem for their implicit source when it's already
	     * known). Note that the node can't have any sources that
	     * need expanding, since SuffFindThem will stop on an existing
	     * node, so all we need to do is set the standard and System V
	     * variables.  */
	    targ->node->type |= OP_DEPS_FOUND;

	    Varq_Set(PREFIX_INDEX, targ->pref, targ->node);

	    Varq_Set(TARGET_INDEX, targ->node->name, targ->node);
	}
    }

    gn->suffix = src->suff;

    /* So Dir_MTime doesn't go questing for it...  */
    efree(gn->path);
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

    if (DEBUG(SUFF)) {
	printf("SuffFindDeps (%s)\n", gn->name);
    }

    if (gn->type & OP_ARCHV) {
	SuffFindArchiveDeps(gn, slst);
    } else if (gn->type & OP_LIB) {
	/*
	 * If the node is a library, it is the arch module's job to find it
	 * and set the TARGET variable accordingly. We merely provide the
	 * search path, assuming all libraries end in ".a" (if the suffix
	 * hasn't been defined, there's nothing we can do for it, so we just
	 * set the TARGET variable to the node's name in order to give it a
	 * value).
	 */
	LstNode ln;
	Suff	*s;

	ln = suff_find_by_name(LIBSUFF);
	if (ln != NULL) {
	    gn->suffix = s = (Suff *)Lst_Datum(ln);
	    Arch_FindLib(gn, &s->searchPath);
	} else {
	    gn->suffix = NULL;
	    Varq_Set(TARGET_INDEX, gn->name, gn);
	}
	/*
	 * Because a library (-lfoo) target doesn't follow the standard
	 * filesystem conventions, we don't set the regular variables for
	 * the thing. .PREFIX is simply made empty...
	 */
	Varq_Set(PREFIX_INDEX, "", gn);
    } else
	SuffFindNormalDeps(gn, slst);
}

/*-
 *-----------------------------------------------------------------------
 * Suff_SetNull --
 *	Define which suffix is the null suffix.
 *
 * Side Effects:
 *	'suffNull' is altered.
 *
 * Notes:
 *	Need to handle the changing of the null suffix gracefully so the
 *	old transformation rules don't just go away.
 *-----------------------------------------------------------------------
 */
void
Suff_SetNull(const char *name)
{
    Suff    *s;
    LstNode ln;

    ln = suff_find_by_name(name);
    if (ln != NULL) {
	s = (Suff *)Lst_Datum(ln);
	if (suffNull != NULL) {
	    suffNull->flags &= ~SUFF_NULL;
	}
	s->flags |= SUFF_NULL;
	/*
	 * XXX: Here's where the transformation mangling would take place
	 */
	suffNull = s;
    } else {
	Parse_Error(PARSE_WARNING, "Desired null suffix %s not defined.",
		     name);
    }
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
    Static_Lst_Init(&sufflist);
#ifdef CLEANUP
    Static_Lst_Init(&suffClean);
#endif
    Static_Lst_Init(&srclist);
    Static_Lst_Init(&transforms);

    sNum = 0;
    /*
     * Create null suffix for single-suffix rules (POSIX). The thing doesn't
     * actually go on the suffix list or everyone will think that's its
     * suffix.
     */
    emptySuff = suffNull = emalloc(sizeof(Suff));

    suffNull->name =	    estrdup("");
    suffNull->nameLen =     0;
    Lst_Init(&suffNull->searchPath);
    Dir_Concat(&suffNull->searchPath, dirSearchPath);
    Lst_Init(&suffNull->children);
    Lst_Init(&suffNull->parents);
    Lst_Init(&suffNull->ref);
    suffNull->sNum =	    sNum++;
    suffNull->flags =	    SUFF_NULL;

}


/*-
 *----------------------------------------------------------------------
 * Suff_End --
 *	Cleanup the this module
 *
 * Side Effects:
 *	The memory is free'd.
 *----------------------------------------------------------------------
 */

#ifdef CLEANUP
void
Suff_End(void)
{
    Lst_Destroy(&sufflist, SuffFree);
    Lst_Destroy(&suffClean, SuffFree);
    if (suffNull)
	SuffFree(suffNull);
    Lst_Destroy(&srclist, NOFREE);
    Lst_Destroy(&transforms, NOFREE);
}
#endif


/********************* DEBUGGING FUNCTIONS **********************/

static void SuffPrintName(void *s)
{
    printf("%s ", ((Suff *)s)->name);
}

static void
SuffPrintSuff(void *sp)
{
    Suff    *s = (Suff *)sp;
    int     flags;
    int     flag;

    printf("# `%s' ", s->name);

    flags = s->flags;
    if (flags) {
	fputs(" (", stdout);
	while (flags) {
	    flag = 1 << (ffs(flags) - 1);
	    flags &= ~flag;
	    switch (flag) {
		case SUFF_NULL:
		    printf("NULL");
		    break;
		case SUFF_INCLUDE:
		    printf("INCLUDE");
		    break;
		case SUFF_LIBRARY:
		    printf("LIBRARY");
		    break;
	    }
	    fputc(flags ? '|' : ')', stdout);
	}
    }
    fputc('\n', stdout);
    printf("#\tTo: ");
    Lst_Every(&s->parents, SuffPrintName);
    fputc('\n', stdout);
    printf("#\tFrom: ");
    Lst_Every(&s->children, SuffPrintName);
    fputc('\n', stdout);
    printf("#\tSearch Path: ");
    Dir_PrintPath(&s->searchPath);
    fputc('\n', stdout);
}

static void
SuffPrintTrans(void *tp)
{
    GNode   *t = (GNode *)tp;

    printf("%-16s: ", t->name);
    Targ_PrintType(t->type);
    fputc('\n', stdout);
    Lst_Every(&t->commands, Targ_PrintCmd);
    fputc('\n', stdout);
}

void
Suff_PrintAll(void)
{
    printf("#*** Suffixes:\n");
    Lst_Every(&sufflist, SuffPrintSuff);

    printf("#*** Transformations:\n");
    Lst_Every(&transforms, SuffPrintTrans);
}

#ifdef DEBUG_SRC
static void
PrintAddr(void *a)
{
    printf("%lx ", (unsigned long)a);
}
#endif
