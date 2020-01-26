/*	$OpenBSD: expandchildren.c,v 1.2 2020/01/26 12:41:21 espie Exp $ */
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
 * expandchildren.c --
 *	Dealing with final children expansion before building stuff
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "direxpand.h"
#include "engine.h"
#include "arch.h"
#include "expandchildren.h"
#include "var.h"
#include "targ.h"
#include "lst.h"
#include "gnode.h"
#include "suff.h"

static void ExpandChildren(LstNode, GNode *);
static void ExpandVarChildren(LstNode, GNode *, GNode *);
static void ExpandWildChildren(LstNode, GNode *, GNode *);

void
LinkParent(GNode *cgn, GNode *pgn)
{
	Lst_AtEnd(&cgn->parents, pgn);
	if (!has_been_built(cgn))
		pgn->children_left++;
	else if ( ! (cgn->type & OP_USE)) {
		if (cgn->built_status == REBUILT)
			pgn->child_rebuilt = true;
		(void)Make_TimeStamp(pgn, cgn);
	}
}

static void
ExpandVarChildren(LstNode after, GNode *cgn, GNode *pgn)
{
	GNode *gn;		/* New source 8) */
	char *cp;		/* Expanded value */
	LIST members;


	if (DEBUG(SUFF))
		printf("Expanding \"%s\"...", cgn->name);

	cp = Var_Subst(cgn->name, &pgn->localvars, true);
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

		(void)Arch_ParseArchive(&sacrifice, &members, &pgn->localvars);
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
				Var_ParseSkip(&cp2, &pgn->localvars);
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
			LinkParent(gn, pgn);
		}
	}
	/* Free the result.  */
	free(cp);
	if (DEBUG(SUFF))
		printf("\n");
}

static void
ExpandWildChildren(LstNode after, GNode *cgn, GNode *pgn)
{
	char *cp;	/* Expanded value */

	LIST exp;	/* List of expansions */
	Lst path;	/* Search path along which to expand */

	if (DEBUG(SUFF))
		printf("Wildcard expanding \"%s\"...", cgn->name);

	/* Find a path along which to expand the word: if
	 * the word has a known suffix, use the path for that suffix,
	 * otherwise use the default path. */
	path = find_best_path(cgn->name);

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
		 * up the parent's count of children to build.  */
		if (Lst_Member(&pgn->children, gn) == NULL) {
			Lst_Append(&pgn->children, after, gn);
			after = Lst_Adv(after);
			LinkParent(gn, pgn);
		}
	}

	if (DEBUG(SUFF))
		printf("\n");
}

/*-
 *-----------------------------------------------------------------------
 * ExpandChildren --
 *	Expand the names of any children of a given node that contain
 *	variable invocations or file wildcards into actual targets.
 *
 * Side Effects:
 *	The expanded node is removed from the parent's list of children,
 *	and the parent's children to build counter is decremented, 
 *      but other nodes may be added.
 *-----------------------------------------------------------------------
 */
static void
ExpandChildren(LstNode ln, /* LstNode of child, so we can replace it */
    GNode *pgn)
{
	GNode	*cgn = Lst_Datum(ln);

	/* First do variable expansion -- this takes precedence over wildcard
	 * expansion. If the result contains wildcards, they'll be gotten to
	 * later since the resulting words are tacked on to the end of the
	 * children list.  */
	if (strchr(cgn->name, '$') != NULL)
		ExpandVarChildren(ln, cgn, pgn);
	else if (Dir_HasWildcards(cgn->name))
		ExpandWildChildren(ln, cgn, pgn);
	else
	    /* Third case: nothing to expand.  */
		return;

	/* Since the source was expanded, remove it from the list of children to
	 * keep it from being processed.  */
	pgn->children_left--;
	Lst_Remove(&pgn->children, ln);
}

void
expand_children_from(GNode *parent, LstNode from)
{
	LstNode np, ln;

	for (ln = from; ln != NULL; ln = np) {
		np = Lst_Adv(ln);
		ExpandChildren(ln, parent);
	}
}
