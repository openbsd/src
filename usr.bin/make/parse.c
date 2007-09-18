/*	$OpenPackages$ */
/*	$OpenBSD: parse.c,v 1.94 2007/09/18 08:46:10 espie Exp $	*/
/*	$NetBSD: parse.c,v 1.29 1997/03/10 21:20:04 christos Exp $	*/

/*
 * Copyright (c) 1999 Marc Espie.
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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "dir.h"
#include "direxpand.h"
#include "job.h"
#include "buf.h"
#include "for.h"
#include "lowparse.h"
#include "arch.h"
#include "cond.h"
#include "suff.h"
#include "parse.h"
#include "var.h"
#include "targ.h"
#include "error.h"
#include "str.h"
#include "main.h"
#include "gnode.h"
#include "memory.h"
#include "extern.h"
#include "lst.h"
#include "parsevar.h"
#include "stats.h"
#include "garray.h"
#include "node_int.h"
#include "nodehashconsts.h"


/* gsources and gtargets should be local to some functions, but they're
 * set as persistent arrays for performance reasons.
 */
static struct growableArray gsources, gtargets;
#define SOURCES_SIZE	128
#define TARGETS_SIZE	32

static LIST	theUserIncPath;/* list of directories for "..." includes */
static LIST	theSysIncPath;	/* list of directories for <...> includes */
Lst systemIncludePath = &theSysIncPath;
Lst userIncludePath = &theUserIncPath;

#ifdef CLEANUP
static LIST	    targCmds;	/* command lines for targets */
#endif

static GNode	    *mainNode;	/* The main target to create. This is the
				 * first target on the first dependency
				 * line in the first makefile */
/*-
 * specType contains the special TYPE of the current target. It is
 * SPECIAL_NONE if the target is unspecial. If it *is* special, however,
 * the children are linked as children of the parent but not vice versa.
 * This variable is set in ParseDoDependency
 */

static int specType;
static int waiting;

/*
 * Predecessor node for handling .ORDER. Initialized to NULL when .ORDER
 * seen, then set to each successive source on the line.
 */
static GNode	*predecessor;

static void ParseLinkSrc(GNode *, GNode *);
static int ParseDoOp(GNode **, int);
static int ParseAddDep(GNode *, GNode *);
static void ParseDoSrc(struct growableArray *, struct growableArray *, int,
    const char *, const char *);
static int ParseFindMain(void *, void *);
static void ParseClearPath(void *);

static void add_target_node(const char *, const char *);
static void add_target_nodes(const char *, const char *);
static void apply_op(struct growableArray *, int, GNode *);
static void ParseDoDependency(const char *);
static void ParseAddCmd(void *, void *);
static void ParseHasCommands(void *);
static bool handle_poison(const char *);
static bool handle_for_loop(Buffer, const char *);
static bool handle_undef(const char *);
#define ParseReadLoopLine(linebuf) Parse_ReadUnparsedLine(linebuf, "for loop")
static bool handle_bsd_command(Buffer, Buffer, const char *);
static char *strip_comments(Buffer, const char *);
static char *resolve_include_filename(const char *, bool);
static void handle_include_file(const char *, const char *, bool, bool);
static bool lookup_bsd_include(const char *);
static void lookup_sysv_style_include(const char *, const char *, bool);
static void lookup_sysv_include(const char *, const char *);
static void lookup_conditional_include(const char *, const char *);
static bool parse_as_special_line(Buffer, Buffer, const char *);

static const char *parse_do_targets(Lst, int *, const char *);
static void parse_target_line(struct growableArray *, const char *,
    const char *);

static void finish_commands(struct growableArray *);
static void parse_commands(struct growableArray *, const char *);
static void create_special_nodes(void);
static bool found_delimiter(const char *);
static int handle_special_targets(Lst);
static void dump_targets(void);

#define	SPECIAL_EXEC		4
#define SPECIAL_IGNORE		5
#define SPECIAL_INCLUDES	6
#define	SPECIAL_INVISIBLE	8
#define SPECIAL_JOIN		9
#define SPECIAL_LIBS		10
#define SPECIAL_MADE		11
#define SPECIAL_MAIN		12
#define SPECIAL_MAKE		13
#define SPECIAL_MFLAGS		14
#define	SPECIAL_NOTMAIN		15
#define	SPECIAL_NOTPARALLEL	16
#define	SPECIAL_NULL		17
#define	SPECIAL_OPTIONAL	18
#define SPECIAL_ORDER		19
#define SPECIAL_PARALLEL	20
#define SPECIAL_PHONY		22
#define SPECIAL_PRECIOUS	23
#define SPECIAL_SILENT		25
#define SPECIAL_SINGLESHELL	26
#define SPECIAL_SUFFIXES	27
#define	SPECIAL_USE		28
#define SPECIAL_WAIT		29
#define SPECIAL_NOPATH		30
#define SPECIAL_ERROR		31


#define P(k) k, sizeof(k), K_##k

static struct {
	const char *keyword;
	size_t sz;
	uint32_t hv;
	int type;
	int special_op;
} specials[] = {
    { P(NODE_EXEC),	SPECIAL_EXEC | SPECIAL_TARGETSOURCE,	OP_EXEC, },
    { P(NODE_IGNORE),	SPECIAL_IGNORE | SPECIAL_TARGETSOURCE, 	OP_IGNORE, },
    { P(NODE_INCLUDES),	SPECIAL_INCLUDES | SPECIAL_TARGET,	0, },
    { P(NODE_INVISIBLE),SPECIAL_INVISIBLE | SPECIAL_TARGETSOURCE,OP_INVISIBLE, },
    { P(NODE_JOIN),	SPECIAL_JOIN | SPECIAL_TARGETSOURCE,	OP_JOIN, },
    { P(NODE_LIBS),	SPECIAL_LIBS | SPECIAL_TARGET,		0, },
    { P(NODE_MADE),	SPECIAL_MADE | SPECIAL_TARGETSOURCE,	OP_MADE, },
    { P(NODE_MAIN),	SPECIAL_MAIN | SPECIAL_TARGET,		0, },
    { P(NODE_MAKE),	SPECIAL_MAKE | SPECIAL_TARGETSOURCE,	OP_MAKE, },
    { P(NODE_MAKEFLAGS),	SPECIAL_MFLAGS | SPECIAL_TARGET,	0, },
    { P(NODE_MFLAGS),	SPECIAL_MFLAGS | SPECIAL_TARGET,	0, },
    { P(NODE_NOTMAIN),	SPECIAL_NOTMAIN | SPECIAL_TARGETSOURCE,	OP_NOTMAIN, },
    { P(NODE_NOTPARALLEL),SPECIAL_NOTPARALLEL | SPECIAL_TARGET,	0, },
    { P(NODE_NO_PARALLEL),SPECIAL_NOTPARALLEL | SPECIAL_TARGET,	0, },
    { P(NODE_NULL),	SPECIAL_NULL | SPECIAL_TARGET,		0, },
    { P(NODE_OPTIONAL),	SPECIAL_OPTIONAL | SPECIAL_TARGETSOURCE,OP_OPTIONAL, },
    { P(NODE_ORDER),	SPECIAL_ORDER | SPECIAL_TARGET,		0, },
    { P(NODE_PARALLEL),	SPECIAL_PARALLEL | SPECIAL_TARGET,	0, },
    { P(NODE_PATH),	SPECIAL_PATH | SPECIAL_TARGET,		0, },
    { P(NODE_PHONY),	SPECIAL_PHONY | SPECIAL_TARGETSOURCE,	OP_PHONY, },
    { P(NODE_PRECIOUS),	SPECIAL_PRECIOUS | SPECIAL_TARGETSOURCE,OP_PRECIOUS, },
    { P(NODE_RECURSIVE),SPECIAL_MAKE | SPECIAL_TARGETSOURCE,	OP_MAKE, },
    { P(NODE_SILENT),	SPECIAL_SILENT | SPECIAL_TARGETSOURCE,	OP_SILENT, },
    { P(NODE_SINGLESHELL),SPECIAL_SINGLESHELL | SPECIAL_TARGET,	0, },
    { P(NODE_SUFFIXES),	SPECIAL_SUFFIXES | SPECIAL_TARGET,	0, },
    { P(NODE_USE),	SPECIAL_USE | SPECIAL_TARGETSOURCE,	OP_USE, },
    { P(NODE_WAIT),	SPECIAL_WAIT | SPECIAL_TARGETSOURCE,	0 },
#if 0
	{ P(NODE_NOPATH),	SPECIAL_NOPATH, },
#endif
};

#undef P

static void
create_special_nodes()
{
	unsigned int i;

	for (i = 0; i < sizeof(specials)/sizeof(specials[0]); i++) {
		GNode *gn = Targ_FindNodeh(specials[i].keyword,
		    specials[i].sz, specials[i].hv, TARG_CREATE);
		gn->special = specials[i].type;
		gn->special_op = specials[i].special_op;
	}
}

/*-
 *---------------------------------------------------------------------
 * ParseLinkSrc  --
 *	Link the parent node to its new child. Used by
 *	ParseDoDependency. If the specType isn't 'Not', the parent
 *	isn't linked as a parent of the child.
 *
 * Side Effects:
 *	New elements are added to the parents list of cgn and the
 *	children list of cgn. the unmade field of pgn is updated
 *	to reflect the additional child.
 *---------------------------------------------------------------------
 */
static void
ParseLinkSrc(GNode *pgn, GNode *cgn)
{
	if (Lst_AddNew(&pgn->children, cgn)) {
		if (specType == SPECIAL_NONE)
			Lst_AtEnd(&cgn->parents, pgn);
		pgn->unmade++;
	}
}

/*-
 *---------------------------------------------------------------------
 * ParseDoOp  --
 *	Apply the parsed operator to the given target node. Used in a
 *	Array_Find call by ParseDoDependency once all targets have
 *	been found and their operator parsed. If the previous and new
 *	operators are incompatible, a major error is taken.
 *
 * Side Effects:
 *	The type field of the node is altered to reflect any new bits in
 *	the op.
 *---------------------------------------------------------------------
 */
static int
ParseDoOp(GNode **gnp, int op)
{
	GNode *gn = *gnp;
	/*
	 * If the dependency mask of the operator and the node don't match and
	 * the node has actually had an operator applied to it before, and the
	 * operator actually has some dependency information in it, complain.
	 */
	if (((op & OP_OPMASK) != (gn->type & OP_OPMASK)) &&
	    !OP_NOP(gn->type) && !OP_NOP(op)) {
		Parse_Error(PARSE_FATAL, "Inconsistent operator for %s", 
		    gn->name);
		return 0;
	}

	if (op == OP_DOUBLEDEP && ((gn->type & OP_OPMASK) == OP_DOUBLEDEP)) {
		/* If the node was the object of a :: operator, we need to
		 * create a new instance of it for the children and commands on
		 * this dependency line. The new instance is placed on the
		 * 'cohorts' list of the initial one (note the initial one is
		 * not on its own cohorts list) and the new instance is linked
		 * to all parents of the initial instance.  */
		GNode *cohort;
		LstNode ln;

		cohort = Targ_NewGN(gn->name);
		/* Duplicate links to parents so graph traversal is simple.
		 * Perhaps some type bits should be duplicated?
		 *
		 * Make the cohort invisible as well to avoid duplicating it
		 * into other variables. True, parents of this target won't
		 * tend to do anything with their local variables, but better
		 * safe than sorry.  */
		for (ln = Lst_First(&gn->parents); ln != NULL; ln = Lst_Adv(ln))
			ParseLinkSrc((GNode *)Lst_Datum(ln), cohort);
		cohort->type = OP_DOUBLEDEP|OP_INVISIBLE;
		Lst_AtEnd(&gn->cohorts, cohort);

		/* Replace the node in the targets list with the new copy */
		*gnp = cohort;
		gn = cohort;
	}
	/* We don't want to nuke any previous flags (whatever they were) so we
	 * just OR the new operator into the old.  */
	gn->type |= op;
	return 1;
}

/*-
 *---------------------------------------------------------------------
 * ParseAddDep	--
 *	Check if the pair of GNodes given needs to be synchronized.
 *	This has to be when two nodes are on different sides of a
 *	.WAIT directive.
 *
 * Results:
 *	Returns 0 if the two targets need to be ordered, 1 otherwise.
 *	If it returns 0, the search can stop.
 *
 * Side Effects:
 *	A dependency can be added between the two nodes.
 *
 *---------------------------------------------------------------------
 */
static int
ParseAddDep(GNode *p, GNode *s)
{
	if (p->order < s->order) {
		/* XXX: This can cause loops, and loops can cause unmade
		 * targets, but checking is tedious, and the debugging output
		 * can show the problem.  */
		Lst_AtEnd(&p->successors, s);
		Lst_AtEnd(&s->preds, p);
		return 1;
	} else
		return 0;
}

static void
apply_op(struct growableArray *targets, int op, GNode *gn)
{
	if (op)
		gn->type |= op;
	else
		Array_ForEach(targets, ParseLinkSrc, gn);
}

/*-
 *---------------------------------------------------------------------
 * ParseDoSrc  --
 *	Given the name of a source, figure out if it is an attribute
 *	and apply it to the targets if it is. Else decide if there is
 *	some attribute which should be applied *to* the source because
 *	of some special target and apply it if so. Otherwise, make the
 *	source be a child of the targets in the list 'targets'
 *
 * Side Effects:
 *	Operator bits may be added to the list of targets or to the source.
 *	The targets may have a new source added to their lists of children.
 *---------------------------------------------------------------------
 */
static void
ParseDoSrc(
    struct growableArray *targets,
    struct growableArray *sources,
    int 	tOp,	/* operator (if any) from special targets */
    const char	*src,	/* name of the source to handle */
    const char *esrc)
{
	GNode *gn = Targ_FindNodei(src, esrc, TARG_CREATE);
	if ((gn->special & SPECIAL_SOURCE) != 0) {
		if (gn->special_op) {
			Array_FindP(targets, ParseDoOp, gn->special_op);
			return;
		} else {
			assert((gn->special & SPECIAL_MASK) == SPECIAL_WAIT);
			waiting++;
			return;
		}
	}

	switch (specType) {
	case SPECIAL_MAIN:
		/*
		 * If we have noted the existence of a .MAIN, it means we need
		 * to add the sources of said target to the list of things
		 * to create.  Note that this will only be invoked if the user
		 * didn't specify a target on the command line. This is to
		 * allow #ifmake's to succeed, or something...
		 */
		Lst_AtEnd(create, gn->name);
		/*
		 * Add the name to the .TARGETS variable as well, so the user
		 * can employ that, if desired.
		 */
		Var_Append(".TARGETS", gn->name);
		return;

	case SPECIAL_ORDER:
		/*
		 * Create proper predecessor/successor links between the
		 * previous source and the current one.
		 */
		if (predecessor != NULL) {
			Lst_AtEnd(&predecessor->successors, gn);
			Lst_AtEnd(&gn->preds, predecessor);
		}
		predecessor = gn;
		break;

	default:
		/*
		 * In the case of a source that was the object of a :: operator,
		 * the attribute is applied to all of its instances (as kept in
		 * the 'cohorts' list of the node) or all the cohorts are linked
		 * to all the targets.
		 */
		apply_op(targets, tOp, gn);
		if ((gn->type & OP_OPMASK) == OP_DOUBLEDEP) {
			LstNode	ln;

			for (ln=Lst_First(&gn->cohorts); ln != NULL;
			    ln = Lst_Adv(ln)){
			    	apply_op(targets, tOp,
				    (GNode *)Lst_Datum(ln));
			}
		}
		break;
	}

	gn->order = waiting;
	Array_AtEnd(sources, gn);
	if (waiting)
		Array_Find(sources, ParseAddDep, gn);
}

/*-
 *-----------------------------------------------------------------------
 * ParseFindMain --
 *	Find a real target in the list and set it to be the main one.
 *	Called by ParseDoDependency when a main target hasn't been found
 *	yet.
 *
 * Results:
 *	1 if main not found yet, 0 if it is.
 *
 * Side Effects:
 *	mainNode is changed and.
 *-----------------------------------------------------------------------
 */
static int
ParseFindMain(void *gnp, void *dummy UNUSED)
{
	GNode *gn = (GNode *)gnp;
	if ((gn->type & OP_NOTARGET) == 0 && gn->special == SPECIAL_NONE) {
		mainNode = gn;
		return 0;
	} else {
		return 1;
	}
}

/*-
 *-----------------------------------------------------------------------
 * ParseClearPath --
 *	Reinit path to an empty path
 *-----------------------------------------------------------------------
 */
static void
ParseClearPath(void *p)
{
	Lst path = (Lst)p;

	Lst_Destroy(path, Dir_Destroy);
	Lst_Init(path);
}

static void
add_target_node(const char *line, const char *end)
{
	GNode *gn;

	gn = Suff_ParseAsTransform(line, end);

	if (gn == NULL) {
		gn = Targ_FindNodei(line, end, TARG_CREATE);
		gn->type &= ~OP_DUMMY;
	}

	if (gn != NULL)
		Array_AtEnd(&gtargets, gn);
}

static void
add_target_nodes(const char *line, const char *end)
{

	if (Dir_HasWildcardsi(line, end)) {
		/*
		 * Targets are to be sought only in the current directory,
		 * so create an empty path for the thing. Note we need to
		 * use Dir_Destroy in the destruction of the path as the
		 * Dir module could have added a directory to the path...
		 */
		char *targName;
		LIST emptyPath;
		LIST curTargs;

		Lst_Init(&emptyPath);
		Lst_Init(&curTargs);
		Dir_Expandi(line, end, &emptyPath, &curTargs);
		Lst_Destroy(&emptyPath, Dir_Destroy);
		while ((targName = (char *)Lst_DeQueue(&curTargs)) != NULL) {
			add_target_node(targName, targName + strlen(targName));
		}
		Lst_Destroy(&curTargs, NOFREE);
	} else {
		add_target_node(line, end);
	}
}

/* special target line check: a proper delimiter is a ':' or '!', but
 * we don't want to end a target on such a character if there is a better
 * match later on.
 * By "better" I mean one that is followed by whitespace. This allows the
 * user to have targets like:
 *    fie::fi:fo: fum
 * where "fie::fi:fo" is the target.  In real life this is used for perl5
 * library man pages where "::" separates an object from its class.  Ie:
 * "File::Spec::Unix".
 * This behaviour is also consistent with other versions of make.
 */
static bool
found_delimiter(const char *s)
{
	if (*s == '!' || *s == ':') {
		const char *p = s + 1;

		if (*s == ':' && *p == ':')
			p++;

		/* Found the best match already. */
		if (isspace(*p) || *p == '\0')
			return true;

		do {
			p += strcspn(p, "!:");
			if (*p == '\0')
			    break;
			p++;
		} while (!isspace(*p));

		/* No better match later on... */
		if (*p == '\0')
			return true;
	}
	return false;
}

static const char *
parse_do_targets(Lst paths, int *op, const char *line)
{
	const char *cp;

	do {
		for (cp = line; *cp && !isspace(*cp) && *cp != '(';) {
			if (*cp == '$')
				/* Must be a dynamic source (would have been
				 * expanded otherwise), so call the Var module
				 * to parse the puppy so we can safely advance
				 * beyond it...There should be no errors in
				 * this, as they would have been discovered in
				 * the initial Var_Subst and we wouldn't be
				 * here.  */
				Var_ParseSkip(&cp, NULL);
			else {
				if (found_delimiter(cp))
					break;
				cp++;
			}
		}

		if (*cp == '(') {
			LIST temp;
			Lst_Init(&temp);
			/* Archives must be handled specially to make sure the
			 * OP_ARCHV flag is set in their 'type' field, for one
			 * thing, and because things like "archive(file1.o
			 * file2.o file3.o)" are permissible.
			 * Arch_ParseArchive will set 'line' to be the first
			 * non-blank after the archive-spec. It creates/finds
			 * nodes for the members and places them on the given
			 * list, returning true if all went well and false if
			 * there was an error in the specification. On error,
			 * line should remain untouched.  */
			if (!Arch_ParseArchive(&line, &temp, NULL)) {
				Parse_Error(PARSE_FATAL,
				     "Error in archive specification: \"%s\"",
				     line);
				return NULL;
			} else {
				AppendList2Array(&temp, &gtargets);
				Lst_Destroy(&temp, NOFREE);
				cp = line;
				continue;
			}
		}
		if (*cp == '\0') {
			/* Ending a dependency line without an operator is a
			 * Bozo no-no */
			Parse_Error(PARSE_FATAL, "Need an operator");
			return NULL;
		}
		/*
		 * Have word in line. Get or create its nodes and stick it at
		 * the end of the targets list
		 */
	    	if (*line != '\0')
			add_target_nodes(line, cp);

		while (isspace(*cp))
			cp++;
		line = cp;
	} while (*line != '!' && *line != ':' && *line);
	*op = handle_special_targets(paths);
	return cp;
}

static void
dump_targets()
{
	size_t i;
	for (i = 0; i < gtargets.n; i++)
		fprintf(stderr, "%s", gtargets.a[i]->name);
	fprintf(stderr, "\n");
}

static int
handle_special_targets(Lst paths)
{
	size_t i;
	int seen_path = 0;
	int seen_special = 0;
	int seen_normal = 0;
	int type;

	for (i = 0; i < gtargets.n; i++) {
		type = gtargets.a[i]->special;
		if ((type & SPECIAL_MASK) == SPECIAL_PATH) {
			seen_path++;
			Lst_AtEnd(paths, find_suffix_path(gtargets.a[i]));
		} else if ((type & SPECIAL_TARGET) != 0)
			seen_special++;
		else
			seen_normal++;
	}
	if ((seen_path != 0) + (seen_special != 0) + (seen_normal != 0) > 1) {
		Parse_Error(PARSE_FATAL, "Wrong mix of special targets");
		dump_targets();
		specType = SPECIAL_ERROR;
		return 0;
	}
	if (seen_normal != 0) {
		specType = SPECIAL_NONE;
		return 0;
	}
	else if (seen_path != 0) {
		specType = SPECIAL_PATH;
		return 0;
	} else if (seen_special == 0) {
		specType = SPECIAL_NONE;
		return 0;
	} else if (seen_special != 1) {
		Parse_Error(PARSE_FATAL, 
		    "Mixing special targets is not allowed");
		dump_targets();
		return 0;
	} else if (seen_special == 1) {
		specType = gtargets.a[0]->special & SPECIAL_MASK;
		switch (specType) {
		case SPECIAL_MAIN:
			if (!Lst_IsEmpty(create)) {
				specType = SPECIAL_NONE;
			}
			break;
		case SPECIAL_NOTPARALLEL:
		{
			extern int  maxJobs;

			maxJobs = 1;
			break;
		}
		case SPECIAL_SINGLESHELL:
			compatMake = 1;
			break;
		case SPECIAL_ORDER:
			predecessor = NULL;
			break;
		default:
			break;
		}
		return gtargets.a[0]->special_op;
	} else {
		/* we're allowed to have 0 target */
		specType = SPECIAL_NONE;
		return 0;
	}
}

/*-
 *---------------------------------------------------------------------
 * ParseDoDependency  --
 *	Parse the dependency line in line.
 *
 * Side Effects:
 *	The nodes of the sources are linked as children to the nodes of the
 *	targets. Some nodes may be created.
 *
 *	We parse a dependency line by first extracting words from the line and
 * finding nodes in the list of all targets with that name. This is done
 * until a character is encountered which is an operator character. Currently
 * these are only ! and :. At this point the operator is parsed and the
 * pointer into the line advanced until the first source is encountered.
 *	The parsed operator is applied to each node in the 'targets' list,
 * which is where the nodes found for the targets are kept, by means of
 * the ParseDoOp function.
 *	The sources are read in much the same way as the targets were except
 * that now they are expanded using the wildcarding scheme of the C-Shell
 * and all instances of the resulting words in the list of all targets
 * are found. Each of the resulting nodes is then linked to each of the
 * targets as one of its children.
 *	Certain targets are handled specially. These are the ones detailed
 * by the specType variable.
 *	The storing of transformation rules is also taken care of here.
 * A target is recognized as a transformation rule by calling
 * Suff_IsTransform. If it is a transformation rule, its node is gotten
 * from the suffix module via Suff_AddTransform rather than the standard
 * Targ_FindNode in the target module.
 *---------------------------------------------------------------------
 */
static void
ParseDoDependency(const char *line)	/* the line to parse */
{
	const char *cp; 	/* our current position */
	int op; 		/* the operator on the line */
	LIST paths;		/* List of search paths to alter when parsing
			 	* a list of .PATH targets */
	int tOp;		/* operator from special target */


	waiting = 0;
	Lst_Init(&paths);

	Array_Reset(&gsources);

	cp = parse_do_targets(&paths, &tOp, line);
	if (cp == NULL || specType == SPECIAL_ERROR)
		return;

	/* Have now parsed all the target names. Must parse the operator next.
	 * The result is left in op .  */
	if (*cp == '!') {
		op = OP_FORCE;
	} else if (*cp == ':') {
		if (cp[1] == ':') {
			op = OP_DOUBLEDEP;
			cp++;
		} else {
			op = OP_DEPENDS;
		}
	} else {
		Parse_Error(PARSE_FATAL, "Missing dependency operator");
		return;
	}

	cp++;			/* Advance beyond operator */

	Array_FindP(&gtargets, ParseDoOp, op);

	/*
	 * Get to the first source
	 */
	while (isspace(*cp))
		cp++;
	line = cp;

	/*
	 * Several special targets take different actions if present with no
	 * sources:
	 *	a .SUFFIXES line with no sources clears out all old suffixes
	 *	a .PRECIOUS line makes all targets precious
	 *	a .IGNORE line ignores errors for all targets
	 *	a .SILENT line creates silence when making all targets
	 *	a .PATH removes all directories from the search path(s).
	 */
	if (!*line) {
		switch (specType) {
		case SPECIAL_SUFFIXES:
			Suff_ClearSuffixes();
			break;
		case SPECIAL_PRECIOUS:
			allPrecious = true;
			break;
		case SPECIAL_IGNORE:
			ignoreErrors = true;
			break;
		case SPECIAL_SILENT:
			beSilent = true;
			break;
		case SPECIAL_PATH:
			Lst_Every(&paths, ParseClearPath);
			break;
		default:
			break;
		}
	} else if (specType == SPECIAL_MFLAGS) {
		/*Call on functions in main.c to deal with these arguments */
		Main_ParseArgLine(line);
		return;
	} else if (specType == SPECIAL_NOTPARALLEL ||
	    specType == SPECIAL_SINGLESHELL) {
		return;
	}

	/*
	 * NOW GO FOR THE SOURCES
	 */
	if (specType == SPECIAL_SUFFIXES || specType == SPECIAL_PATH ||
	    specType == SPECIAL_INCLUDES || specType == SPECIAL_LIBS ||
	    specType == SPECIAL_NULL) {
		while (*line) {
		    /*
		     * If the target was one that doesn't take files as its
		     * sources but takes something like suffixes, we take each
		     * space-separated word on the line as a something and deal
		     * with it accordingly.
		     *
		     * If the target was .SUFFIXES, we take each source as a
		     * suffix and add it to the list of suffixes maintained by
		     * the Suff module.
		     *
		     * If the target was a .PATH, we add the source as a
		     * directory to search on the search path.
		     *
		     * If it was .INCLUDES, the source is taken to be the
		     * suffix of files which will be #included and whose search
		     * path should be present in the .INCLUDES variable.
		     *
		     * If it was .LIBS, the source is taken to be the suffix of
		     * files which are considered libraries and whose search
		     * path should be present in the .LIBS variable.
		     *
		     * If it was .NULL, the source is the suffix to use when a
		     * file has no valid suffix.
		     */
		    while (*cp && !isspace(*cp))
			    cp++;
		    switch (specType) {
		    case SPECIAL_SUFFIXES:
			    Suff_AddSuffixi(line, cp);
			    break;
		    case SPECIAL_PATH:
			    {
			    LstNode ln;

			    for (ln = Lst_First(&paths); ln != NULL;
			    	ln = Lst_Adv(ln))
				    Dir_AddDiri((Lst)Lst_Datum(ln), line, cp);
			    break;
			    }
		    case SPECIAL_INCLUDES:
			    Suff_AddIncludei(line, cp);
			    break;
		    case SPECIAL_LIBS:
			    Suff_AddLibi(line, cp);
			    break;
		    case SPECIAL_NULL:
			    Suff_SetNulli(line, cp);
			    break;
		    default:
			    break;
		    }
		    if (*cp != '\0')
			cp++;
		    while (isspace(*cp))
			cp++;
		    line = cp;
		}
		Lst_Destroy(&paths, NOFREE);
	} else {
		while (*line) {
			/*
			 * The targets take real sources, so we must beware of
			 * archive specifications (i.e. things with left
			 * parentheses in them) and handle them accordingly.
			 */
			while (*cp && !isspace(*cp)) {
				if (*cp == '(' && cp > line && cp[-1] != '$') {
					/*
					 * Only stop for a left parenthesis if
					 * it isn't at the start of a word
					 * (that'll be for variable changes
					 * later) and isn't preceded by a
					 * dollar sign (a dynamic source).
					 */
					break;
				} else {
					cp++;
				}
			}

			if (*cp == '(') {
				GNode *gn;
				LIST sources;	/* list of archive source
						 * names after expansion */

				Lst_Init(&sources);
				if (!Arch_ParseArchive(&line, &sources, NULL)) {
					Parse_Error(PARSE_FATAL,
					    "Error in source archive spec \"%s\"",
					    line);
					return;
				}

				while ((gn = (GNode *)Lst_DeQueue(&sources)) !=
				    NULL)
					ParseDoSrc(&gtargets, &gsources, tOp,
					    gn->name, NULL);
				cp = line;
			} else {
				const char *endSrc = cp;

				ParseDoSrc(&gtargets, &gsources, tOp, line,
				    endSrc);
				if (*cp)
					cp++;
			}
			while (isspace(*cp))
				cp++;
			line = cp;
		}
	}

	if (mainNode == NULL) {
		/* If we have yet to decide on a main target to make, in the
		 * absence of any user input, we want the first target on
		 * the first dependency line that is actually a real target
		 * (i.e. isn't a .USE or .EXEC rule) to be made.  */
		Array_Find(&gtargets, ParseFindMain, NULL);
	}
}

/*-
 * ParseAddCmd	--
 *	Lst_ForEach function to add a command line to all targets
 *
 * Side Effects:
 *	A new element is added to the commands list of the node.
 */
static void
ParseAddCmd(void *gnp, void *cmd)
{
	GNode *gn = (GNode *)gnp;
	/* if target already supplied, ignore commands */
	if (!(gn->type & OP_HAS_COMMANDS)) {
		Lst_AtEnd(&gn->commands, cmd);
		if (!gn->lineno) {
			gn->lineno = Parse_Getlineno();
			gn->fname = Parse_Getfilename();
		}
	}
}

/*-
 *-----------------------------------------------------------------------
 * ParseHasCommands --
 *	Callback procedure for Parse_File when destroying the list of
 *	targets on the last dependency line. Marks a target as already
 *	having commands if it does, to keep from having shell commands
 *	on multiple dependency lines.
 *
 * Side Effects:
 *	OP_HAS_COMMANDS may be set for the target.
 *-----------------------------------------------------------------------
 */
static void
ParseHasCommands(void *gnp)	    /* Node to examine */
{
	GNode *gn = (GNode *)gnp;
	if (!Lst_IsEmpty(&gn->commands))
		gn->type |= OP_HAS_COMMANDS;
}


/* Strip comments from line. Build a copy in buffer if necessary, */
static char *
strip_comments(Buffer copy, const char *line)
{
	const char *comment;
	const char *p;

	comment = strchr(line, '#');
	assert(comment != line);
	if (comment == NULL)
		return (char *)line;
	else {
		Buf_Reset(copy);

		for (p = line; *p != '\0'; p++) {
			if (*p == '\\') {
				if (p[1] == '#') {
					Buf_Addi(copy, line, p);
					Buf_AddChar(copy, '#');
					line = p+2;
				}
				if (p[1] != '\0')
					p++;
			} else if (*p == '#')
				break;
		}
		Buf_Addi(copy, line, p);
		Buf_KillTrailingSpaces(copy);
		return Buf_Retrieve(copy);
	}
}



/***
 *** Support for various include constructs
 ***/


void
Parse_AddIncludeDir(const char	*dir)
{
	Dir_AddDir(userIncludePath, dir);
}

static char *
resolve_include_filename(const char *file, bool isSystem)
{
	char *fullname;

	/* Look up system files on the system path first */
	if (isSystem) {
		fullname = Dir_FindFileNoDot(file, systemIncludePath);
		if (fullname)
			return fullname;
	}

	/* Handle non-system non-absolute files... */
	if (!isSystem && file[0] != '/') {
		/* ... by looking first under the same directory as the
		 * current file */
		char *slash;
		const char *fname;

		fname = Parse_Getfilename();

		slash = strrchr(fname, '/');
		if (slash != NULL) {
			char *newName;

			newName = Str_concati(fname, slash, file,
			    strchr(file, '\0'), '/');
			fullname = Dir_FindFile(newName, userIncludePath);
			if (fullname == NULL)
				fullname = Dir_FindFile(newName, defaultPath);
			free(newName);
			if (fullname)
				return fullname;
		}
	}

	/* Now look first on the -I search path, then on the .PATH
	 * search path, if not found in a -I directory.
	 * XXX: Suffix specific?  */
	fullname = Dir_FindFile(file, userIncludePath);
	if (fullname)
		return fullname;
	fullname = Dir_FindFile(file, defaultPath);
	if (fullname)
		return fullname;

	/* Still haven't found the makefile. Look for it on the system
	 * path as a last resort (if we haven't already). */
	if (isSystem)
		return NULL;
	else
		return Dir_FindFile(file, systemIncludePath);
}

static void
handle_include_file(const char *name, const char *ename, bool isSystem,
    bool errIfNotFound)
{
	char *file;
	char *fullname;

	/* Substitute for any variables in the file name before trying to
	 * find the thing. */
	file = Var_Substi(name, ename, NULL, false);

	fullname = resolve_include_filename(file, isSystem);
	if (fullname == NULL && errIfNotFound)
		Parse_Error(PARSE_FATAL, "Could not find %s", file);
	free(file);


	if (fullname != NULL) {
		FILE *f;

		f = fopen(fullname, "r");
		if (f == NULL && errIfNotFound)
			Parse_Error(PARSE_FATAL, "Cannot open %s", fullname);
		else
			Parse_FromFile(fullname, f);
	}
}

/* .include <file> (system) or .include "file" (normal) */
static bool
lookup_bsd_include(const char *file)
{
	char endc;
	const char *efile;
	bool isSystem;

	/* find starting delimiter */
	while (isspace(*file))
		file++;

	/* determine type of file */
	if (*file == '<') {
		isSystem = true;
		endc = '>';
	} else if (*file == '"') {
		isSystem = false;
		endc = '"';
	} else {
		Parse_Error(PARSE_WARNING,
		    ".include filename must be delimited by '\"' or '<'");
		return false;
	}

	/* delimit file name between file and efile */
	for (efile = ++file; *efile != endc; efile++) {
		if (*efile == '\0') {
			Parse_Error(PARSE_WARNING,
			     "Unclosed .include filename. '%c' expected", endc);
			return false;
		}
	}
	handle_include_file(file, efile, isSystem, true);
	return true;
}


static void
lookup_sysv_style_include(const char *file, const char *directive,
    bool errIfMissing)
{
	const char *efile;

	/* find beginning of name */
	while (isspace(*file))
		file++;
	if (*file == '\0') {
		Parse_Error(PARSE_FATAL, "Filename missing from \"%s\"",
		    directive);
		return;
	}
	/* sys5 delimits file up to next blank character or end of line */
	for (efile = file; *efile != '\0' && !isspace(*efile);)
		efile++;

	handle_include_file(file, efile, true, errIfMissing);
}


/* system V construct:  include file */
static void
lookup_sysv_include(const char *file, const char *directive)
{
	lookup_sysv_style_include(file, directive, true);
}


/* sinclude file and -include file */
static void
lookup_conditional_include(const char *file, const char *directive)
{
	lookup_sysv_style_include(file, directive, false);
}


/***
 ***   BSD-specific . constructs
 ***   They all follow the same pattern:
 ***    if the syntax matches BSD stuff, then we're committed to handle
 ***   them and report fatal errors (like, include file not existing)
 ***    otherwise, we return false, and hope somebody else will handle it.
 ***/

static bool
handle_poison(const char *line)
{
	const char *p = line;
	int type = POISON_NORMAL;
	bool not = false;
	bool paren_to_match = false;
	const char *name, *ename;

	while (isspace(*p))
		p++;
	if (*p == '!') {
		not = true;
		p++;
	}
	while (isspace(*p))
		p++;
	if (strncmp(p, "defined", 7) == 0) {
		type = POISON_DEFINED;
		p += 7;
	} else if (strncmp(p, "empty", 5) == 0) {
		type = POISON_EMPTY;
		p += 5;
	}
	while (isspace(*p))
		p++;
	if (*p == '(') {
		paren_to_match = true;
		p++;
	}
	while (isspace(*p))
		p++;
	name = ename = p;
	while (*p != '\0' && !isspace(*p)) {
		if (*p == ')' && paren_to_match) {
			paren_to_match = false;
			p++;
			break;
		}
		p++;
		ename = p;
	}
	while (isspace(*p))
		p++;
	switch(type) {
	case POISON_NORMAL:
	case POISON_EMPTY:
		if (not)
			type = POISON_INVALID;
		break;
	case POISON_DEFINED:
		if (not)
			type = POISON_NOT_DEFINED;
		else
			type = POISON_INVALID;
		break;
	}
	if ((*p != '\0' && *p != '#') || type == POISON_INVALID) {
		Parse_Error(PARSE_WARNING, "Invalid syntax for .poison: %s",
		    line);
		return false;
	} else {
		Var_MarkPoisoned(name, ename, type);
		return true;
	}
}


static bool
handle_for_loop(Buffer linebuf, const char *line)
{
	For *loop;

	loop = For_Eval(line);
	if (loop != NULL) {
		bool ok;
		do {
			/* Find the matching endfor.  */
			line = ParseReadLoopLine(linebuf);
			if (line == NULL) {
			    Parse_Error(PARSE_FATAL,
				 "Unexpected end of file in for loop.\n");
			    return false;
			}
			ok = For_Accumulate(loop, line);
		} while (ok);
		For_Run(loop);
		return true;
	} else
		return false;
}

static bool
handle_undef(const char *line)
{
	const char *eline;

	while (isspace(*line))
		line++;
	for (eline = line; !isspace(*eline) && *eline != '\0';)
		eline++;
	Var_Deletei(line, eline);
	return true;
}

/* global hub for the construct */
static bool
handle_bsd_command(Buffer linebuf, Buffer copy, const char *line)
{
	char *stripped;

	while (isspace(*line))
		line++;

	/* delegate basic classification to the conditional module */
	switch (Cond_Eval(line)) {
	case COND_SKIP:
		/* Skip to next conditional that evaluates to COND_PARSE.  */
		do {
			line = Parse_ReadNextConditionalLine(linebuf);
			if (line != NULL) {
				while (isspace(*line))
					line++;
					stripped = strip_comments(copy, line);
			}
		} while (line != NULL && Cond_Eval(stripped) != COND_PARSE);
		/* FALLTHROUGH */
	case COND_PARSE:
		return true;
	case COND_ISFOR:
		return handle_for_loop(linebuf, line + 3);
	case COND_ISINCLUDE:
		return lookup_bsd_include(line + 7);
	case COND_ISPOISON:
		return handle_poison(line + 6);
	case COND_ISUNDEF:
		return handle_undef(line + 5);
	default:
		break;
	}

	return false;
}

/***
 *** handle a group of commands
 ***/

static void
finish_commands(struct growableArray *targets)
{
	Array_Every(targets, ParseHasCommands);
	Array_Reset(targets);
}

static void
parse_commands(struct growableArray *targets, const char *line)
{
	/* add the command to the list of
	 * commands of all targets in the dependency spec */
	char *cmd = estrdup(line);

	Array_ForEach(targets, ParseAddCmd, cmd);
#ifdef CLEANUP
	Lst_AtEnd(&targCmds, cmd);
#endif
}

static bool
parse_as_special_line(Buffer buf, Buffer copy, const char *line)
{
	if (*line == '.' && handle_bsd_command(buf, copy, line+1))
		return true;
	if (FEATURES(FEATURE_SYSVINCLUDE) &&
	    strncmp(line, "include", 7) == 0 &&
	    isspace(line[7]) &&
	    strchr(line, ':') == NULL) {
	    /* It's an S3/S5-style "include".  */
		lookup_sysv_include(line + 7, "include");
		return true;
	}
	if (FEATURES(FEATURE_CONDINCLUDE) &&
	    strncmp(line, "sinclude", 8) == 0 &&
	    isspace(line[8]) &&
	    strchr(line, ':') == NULL) {
		lookup_conditional_include(line+8, "sinclude");
		return true;
	}
	if (FEATURES(FEATURE_CONDINCLUDE) &&
	    strncmp(line, "-include", 8) == 0 &&
	    isspace(line[8]) &&
	    strchr(line, ':') == NULL) {
		lookup_conditional_include(line+8, "-include");
		return true;
	}
	return false;
}

static void
parse_target_line(struct growableArray *targets, const char *line,
    const char *stripped)
{
	size_t pos;
	char *end;
	char *cp;
	char *dep;

	/* let's start a new set of commands */
	Array_Reset(targets);

	/* XXX this is a dirty heuristic to handle target: dep ; commands */
	dep = NULL;
	/* First we need to find eventual dependencies */
	pos = strcspn(stripped, ":!");
	/* go over :!, and find ;  */
	if (stripped[pos] != '\0' &&
	    (end = strchr(stripped+pos+1, ';')) != NULL) {
		if (line != stripped)
			/* find matching ; in original... The
			 * original might be slightly longer.  */
			dep = strchr(line+(end-stripped), ';');
		else
			dep = end;
		/* kill end of line. */
		*end = '\0';
	}
	/* We now know it's a dependency line so it needs to
	 * have all variables expanded before being parsed.
	 */
	cp = Var_Subst(stripped, NULL, false);
	ParseDoDependency(cp);
	free(cp);

	/* Parse dependency if it's not empty. */
	if (dep != NULL) {
		do {
			dep++;
		} while (isspace(*dep));
		if (*dep != '\0')
			parse_commands(targets, dep);
	}
}

void
Parse_File(const char *filename, FILE *stream)
{
	char *line;
	bool expectingCommands = false;

	/* somewhat permanent spaces to shave time */
	BUFFER buf;
	BUFFER copy;

	Buf_Init(&buf, MAKE_BSIZE);
	Buf_Init(&copy, MAKE_BSIZE);

	Parse_FromFile(filename, stream);
	do {
		while ((line = Parse_ReadNormalLine(&buf)) != NULL) {
			if (*line == '\t') {
				if (expectingCommands)
					parse_commands(&gtargets, line+1);
				else
					Parse_Error(PARSE_FATAL,
					    "Unassociated shell command \"%s\"",
					     line);
			} else {
				const char *stripped = strip_comments(&copy,
				    line);
				if (!parse_as_special_line(&buf, &copy,
				    stripped)) {
					if (expectingCommands)
						finish_commands(&gtargets);
					if (Parse_As_Var_Assignment(stripped))
						expectingCommands = false;
					else {
						parse_target_line(&gtargets,
						    line, stripped);
						expectingCommands = true;
					}
				}
			}
		}
	} while (Parse_NextFile());

	if (expectingCommands)
		finish_commands(&gtargets);
	/* Make sure conditionals are clean.  */
	Cond_End();

	Parse_ReportErrors();
	Buf_Destroy(&buf);
	Buf_Destroy(&copy);
}

void
Parse_Init(void)
{
	mainNode = NULL;
	Static_Lst_Init(userIncludePath);
	Static_Lst_Init(systemIncludePath);
	Array_Init(&gtargets, TARGETS_SIZE);
    	Array_Init(&gsources, SOURCES_SIZE);
	create_special_nodes();

	LowParse_Init();
#ifdef CLEANUP
	Static_Lst_Init(&targCmds);
#endif
}

#ifdef CLEANUP
void
Parse_End(void)
{
	Lst_Destroy(&targCmds, (SimpleProc)free);
	Lst_Destroy(systemIncludePath, Dir_Destroy);
	Lst_Destroy(userIncludePath, Dir_Destroy);
	LowParse_End();
}
#endif


void
Parse_MainName(Lst listmain)	/* result list */
{
	if (mainNode == NULL) {
		Punt("no target to make.");
		/*NOTREACHED*/
	} else if (mainNode->type & OP_DOUBLEDEP) {
		Lst_AtEnd(listmain, mainNode);
		Lst_Concat(listmain, &mainNode->cohorts);
	}
	else
		Lst_AtEnd(listmain, mainNode);
}
