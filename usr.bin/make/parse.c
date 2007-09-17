/*	$OpenPackages$ */
/*	$OpenBSD: parse.c,v 1.89 2007/09/17 11:11:30 espie Exp $	*/
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
 * specType contains the SPECial TYPE of the current target. It is
 * Not if the target is unspecial. If it *is* special, however, the children
 * are linked as children of the parent but not vice versa. This variable is
 * set in ParseDoDependency
 */
typedef enum {
    Begin,	    /* .BEGIN */
    Default,	    /* .DEFAULT */
    End,	    /* .END */
    Ignore,	    /* .IGNORE */
    Includes,	    /* .INCLUDES */
    Interrupt,	    /* .INTERRUPT */
    Libs,	    /* .LIBS */
    MFlags,	    /* .MFLAGS or .MAKEFLAGS */
    Main,	    /* .MAIN and we don't have anything user-specified to
		     * make */
    NoExport,	    /* .NOEXPORT */
    NoPath,	    /* .NOPATH */
    Not,	    /* Not special */
    NotParallel,    /* .NOTPARALELL */
    Null,	    /* .NULL */
    Order,	    /* .ORDER */
    Parallel,	    /* .PARALLEL */
    ExPath,	    /* .PATH */
    Phony,	    /* .PHONY */
    Precious,	    /* .PRECIOUS */
    Silent,	    /* .SILENT */
    SingleShell,    /* .SINGLESHELL */
    Suffixes,	    /* .SUFFIXES */
    Wait,	    /* .WAIT */
    Attribute	    /* Generic attribute */
} ParseSpecial;

static ParseSpecial specType;
static int waiting;

/*
 * Predecessor node for handling .ORDER. Initialized to NULL when .ORDER
 * seen, then set to each successive source on the line.
 */
static GNode	*predecessor;

/*
 * The parseKeywords table is searched using binary search when deciding
 * if a target or source is special. The 'spec' field is the ParseSpecial
 * type of the keyword ("Not" if the keyword isn't special as a target) while
 * the 'op' field is the operator to apply to the list of targets if the
 * keyword is used as a source ("0" if the keyword isn't special as a source)
 */
static struct {
    char	  *name;	/* Name of keyword */
    ParseSpecial  spec; 	/* Type when used as a target */
    int 	  op;		/* Operator when used as a source */
} parseKeywords[] = {
{ ".BEGIN",	  Begin,	0 },
{ ".DEFAULT",	  Default,	0 },
{ ".END",	  End,		0 },
{ ".EXEC",	  Attribute,	OP_EXEC },
{ ".IGNORE",	  Ignore,	OP_IGNORE },
{ ".INCLUDES",	  Includes,	0 },
{ ".INTERRUPT",   Interrupt,	0 },
{ ".INVISIBLE",   Attribute,	OP_INVISIBLE },
{ ".JOIN",	  Attribute,	OP_JOIN },
{ ".LIBS",	  Libs, 	0 },
{ ".MADE",	  Attribute,	OP_MADE },
{ ".MAIN",	  Main, 	0 },
{ ".MAKE",	  Attribute,	OP_MAKE },
{ ".MAKEFLAGS",   MFlags,	0 },
{ ".MFLAGS",	  MFlags,	0 },
#if 0	/* basic scaffolding for NOPATH, not working yet */
{ ".NOPATH",	  NoPath,	OP_NOPATH },
#endif
{ ".NOTMAIN",	  Attribute,	OP_NOTMAIN },
{ ".NOTPARALLEL", NotParallel,	0 },
{ ".NO_PARALLEL", NotParallel,	0 },
{ ".NULL",	  Null, 	0 },
{ ".OPTIONAL",	  Attribute,	OP_OPTIONAL },
{ ".ORDER",	  Order,	0 },
{ ".PARALLEL",	  Parallel,	0 },
{ ".PATH",	  ExPath,	0 },
{ ".PHONY",	  Phony,	OP_PHONY },
{ ".PRECIOUS",	  Precious,	OP_PRECIOUS },
{ ".RECURSIVE",   Attribute,	OP_MAKE },
{ ".SILENT",	  Silent,	OP_SILENT },
{ ".SINGLESHELL", SingleShell,	0 },
{ ".SUFFIXES",	  Suffixes,	0 },
{ ".USE",	  Attribute,	OP_USE },
{ ".WAIT",	  Wait, 	0 },
};

static int ParseFindKeyword(const char *);
static void ParseLinkSrc(GNode *, GNode *);
static int ParseDoOp(GNode *, int);
static int ParseAddDep(GNode *, GNode *);
static void ParseDoSrc(int, const char *);
static int ParseFindMain(void *, void *);
static void ParseAddDir(void *, void *);
static void ParseClearPath(void *);

static void add_target_node(const char *);
static void add_target_nodes(const char *);
static void ParseDoDependency(char *);
static void ParseAddCmd(void *, void *);
static void ParseHasCommands(void *);
static bool handle_poison(const char *);
static bool handle_for_loop(Buffer, const char *);
static bool handle_undef(const char *);
#define ParseReadLoopLine(linebuf) Parse_ReadUnparsedLine(linebuf, "for loop")
static void ParseFinishDependency(void);
static bool handle_bsd_command(Buffer, Buffer, const char *);
static char *strip_comments(Buffer, const char *);
static char *resolve_include_filename(const char *, bool);
static void handle_include_file(const char *, const char *, bool, bool);
static bool lookup_bsd_include(const char *);
static void lookup_sysv_style_include(const char *, const char *, bool);
static void lookup_sysv_include(const char *, const char *);
static void lookup_conditional_include(const char *, const char *);

static void ParseDoCommands(const char *);

/*-
 *----------------------------------------------------------------------
 * ParseFindKeyword --
 *	Look in the table of keywords for one matching the given string.
 *
 * Results:
 *	The index of the keyword, or -1 if it isn't there.
 *----------------------------------------------------------------------
 */
static int
ParseFindKeyword(const char *str)	/* keyword to look up */
{
    int 	    start,
		    end,
		    cur;
    int 	    diff;

    start = 0;
    end = (sizeof(parseKeywords)/sizeof(parseKeywords[0])) - 1;

    do {
	cur = start + (end - start) / 2;
	diff = strcmp(str, parseKeywords[cur].name);

	if (diff == 0) {
	    return cur;
	} else if (diff < 0) {
	    end = cur - 1;
	} else {
	    start = cur + 1;
	}
    } while (start <= end);
    return -1;
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
ParseLinkSrc(
    GNode		*pgn,	/* The parent node */
    GNode		*cgn)	/* The child node */
{
	if (Lst_AddNew(&pgn->children, cgn)) {
		if (specType == Not)
			Lst_AtEnd(&cgn->parents, pgn);
		pgn->unmade++;
	}
}

/*-
 *---------------------------------------------------------------------
 * ParseDoOp  --
 *	Apply the parsed operator to the given target node. Used in a
 *	Lst_Find call by ParseDoDependency once all targets have
 *	been found and their operator parsed. If the previous and new
 *	operators are incompatible, a major error is taken.
 *
 * Side Effects:
 *	The type field of the node is altered to reflect any new bits in
 *	the op.
 *---------------------------------------------------------------------
 */
static int
ParseDoOp(
    GNode	   *gn,	/* The node to which the operator is to be applied */
    int	   	   op)	/* The operator to apply */
{
	/*
	 * If the dependency mask of the operator and the node don't match and
	 * the node has actually had an operator applied to it before, and
	 * the operator actually has some dependency information in it, complain.
	 */
	if (((op & OP_OPMASK) != (gn->type & OP_OPMASK)) &&
		!OP_NOP(gn->type) && !OP_NOP(op)) {
		Parse_Error(PARSE_FATAL, 
		    "Inconsistent operator for %s", gn->name);
		return 0;
	}

	if (op == OP_DOUBLEDEP && ((gn->type & OP_OPMASK) == OP_DOUBLEDEP)) {
		/* If the node was the object of a :: operator, we need to
		 * create a new instance of it for the children and commands on
		 * this dependency line. The new instance is placed on the
		 * 'cohorts' list of the initial one (note the initial one is
		 * not on its own cohorts list) and the new instance is linked
		 * to all parents of the initial instance.  */
		GNode		*cohort;
		LstNode 	ln;
		unsigned int i;

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
		for (i = 0; i < gtargets.n; i++)
			if (gtargets.a[i] == gn)
				break;
		gtargets.a[i] = cohort;
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
	}
	else
		return 0;
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
    int 	tOp,	/* operator (if any) from special targets */
    const char	*src)	/* name of the source to handle */
{
	GNode	*gn = NULL;

	if (*src == '.' && isupper(src[1])) {
		int keywd = ParseFindKeyword(src);
		if (keywd != -1) {
			int op = parseKeywords[keywd].op;
			if (op != 0) {
				Array_Find(&gtargets, ParseDoOp, op);
				return;
			}
			if (parseKeywords[keywd].spec == Wait) {
				waiting++;
				return;
			}
		}
	}

	switch (specType) {
	case Main:
		/*
		 * If we have noted the existence of a .MAIN, it means we need
		 * to add the sources of said target to the list of things
		 * to create. The string 'src' is likely to be freed, so we
		 * must make a new copy of it. Note that this will only be
		 * invoked if the user didn't specify a target on the command
		 * line. This is to allow #ifmake's to succeed, or something...
		 */
		Lst_AtEnd(create, estrdup(src));
		/*
		 * Add the name to the .TARGETS variable as well, so the user
		 * can employ that, if desired.
		 */
		Var_Append(".TARGETS", src);
		return;

	case Order:
		/*
		 * Create proper predecessor/successor links between the
		 * previous source and the current one.
		 */
		gn = Targ_FindNode(src, TARG_CREATE);
		if (predecessor != NULL) {
			Lst_AtEnd(&predecessor->successors, gn);
			Lst_AtEnd(&gn->preds, predecessor);
		}
		/*
		 * The current source now becomes the predecessor for the next
		 * one.
		 */
		predecessor = gn;
		break;

	default:
		/*
		 * If the source is not an attribute, we need to find/create
		 * a node for it. After that we can apply any operator to it
		 * from a special target or link it to its parents, as
		 * appropriate.
		 *
		 * In the case of a source that was the object of a :: operator,
		 * the attribute is applied to all of its instances (as kept in
		 * the 'cohorts' list of the node) or all the cohorts are linked
		 * to all the targets.
		 */
		gn = Targ_FindNode(src, TARG_CREATE);
		if (tOp) {
			gn->type |= tOp;
		} else {
			Array_ForEach(&gtargets, ParseLinkSrc, gn);
		}
		if ((gn->type & OP_OPMASK) == OP_DOUBLEDEP) {
			GNode	*cohort;
			LstNode	ln;

			for (ln=Lst_First(&gn->cohorts); ln != NULL; 
			    ln = Lst_Adv(ln)){
				cohort = (GNode *)Lst_Datum(ln);
				if (tOp) {
					cohort->type |= tOp;
				} else {
					Array_ForEach(&gtargets, ParseLinkSrc, 
					    cohort);
				}
			}
		}
		break;
	}

	gn->order = waiting;
	Array_AtEnd(&gsources, gn);
	if (waiting) {
		Array_Find(&gsources, ParseAddDep, gn);
	}
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
 *	mainNode is changed and Targ_SetMain is called.
 *-----------------------------------------------------------------------
 */
static int
ParseFindMain(
    void *gnp,	    /* Node to examine */
    void *dummy 	UNUSED)
{
	GNode	  *gn = (GNode *)gnp;
	if ((gn->type & OP_NOTARGET) == 0) {
		mainNode = gn;
		Targ_SetMain(gn);
		return 0;
	} else {
		return 1;
	}
}

/*-
 *-----------------------------------------------------------------------
 * ParseAddDir --
 *	Front-end for Dir_AddDir to make sure Lst_ForEach keeps going
 *
 * Side Effects:
 *	See Dir_AddDir.
 *-----------------------------------------------------------------------
 */
static void
ParseAddDir(void *path, void *name)
{
	Dir_AddDir((Lst)path, (char *)name);
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
	Lst 	path = (Lst)p;

	Lst_Destroy(path, Dir_Destroy);
	Lst_Init(path);
}

static void
add_target_node(const char *line)
{
	GNode *gn;

	if (!Suff_IsTransform(line))
		gn = Targ_FindNode(line, TARG_CREATE);
	else
		gn = Suff_AddTransform(line);

	if (gn != NULL)
		Array_AtEnd(&gtargets, gn);
}

static void
add_target_nodes(const char *line)
{

	if (Dir_HasWildcards(line)) {
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
		Dir_Expand(line, &emptyPath, &curTargs);
		Lst_Destroy(&emptyPath, Dir_Destroy);
		while ((targName = (char *)Lst_DeQueue(&curTargs)) != NULL) {
			add_target_node(targName);
		}
		Lst_Destroy(&curTargs, NOFREE);
	} else {
		add_target_node(line);
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
ParseDoDependency(char *line)	/* the line to parse */
{
    char	   *cp; 	/* our current position */
    GNode	   *gn; 	/* a general purpose temporary node */
    int 	    op; 	/* the operator on the line */
    char	    savec;	/* a place to save a character */
    LIST	    paths;	/* List of search paths to alter when parsing
				 * a list of .PATH targets */
    int 	    tOp;	/* operator from special target */
    tOp = 0;

    specType = Not;
    waiting = 0;
    Lst_Init(&paths);

    Array_Reset(&gsources);

    do {
	for (cp = line; *cp && !isspace(*cp) && *cp != '(';)
	    if (*cp == '$')
		/* Must be a dynamic source (would have been expanded
		 * otherwise), so call the Var module to parse the puppy
		 * so we can safely advance beyond it...There should be
		 * no errors in this, as they would have been discovered
		 * in the initial Var_Subst and we wouldn't be here.  */
		Var_ParseSkip(&cp, NULL);
	    else {
		/* We don't want to end a word on ':' or '!' if there is a
		 * better match later on in the string.  By "better" I mean
		 * one that is followed by whitespace.	This allows the user
		 * to have targets like:
		 *    fie::fi:fo: fum
		 * where "fie::fi:fo" is the target.  In real life this is used
		 * for perl5 library man pages where "::" separates an object
		 * from its class.  Ie: "File::Spec::Unix".  This behaviour
		 * is also consistent with other versions of make.  */
		if (*cp == '!' || *cp == ':') {
		    char *p = cp + 1;

		    if (*cp == ':' && *p == ':')
			p++;

		    /* Found the best match already. */
		    if (isspace(*p) || *p == '\0')
			break;

		    do {
			p += strcspn(p, "!:");
			if (*p == '\0')
			    break;
			p++;
		    } while (!isspace(*p));

		    /* No better match later on... */
		    if (*p == '\0')
			break;

		}
		cp++;
	    }
	if (*cp == '(') {
	    LIST temp;
	    Lst_Init(&temp);
	    /* Archives must be handled specially to make sure the OP_ARCHV
	     * flag is set in their 'type' field, for one thing, and because
	     * things like "archive(file1.o file2.o file3.o)" are permissible.
	     * Arch_ParseArchive will set 'line' to be the first non-blank
	     * after the archive-spec. It creates/finds nodes for the members
	     * and places them on the given list, returning true if all
	     * went well and false if there was an error in the
	     * specification. On error, line should remain untouched.  */
	    if (!Arch_ParseArchive(&line, &temp, NULL)) {
		Parse_Error(PARSE_FATAL,
			     "Error in archive specification: \"%s\"", line);
		return;
	    } else {
	    	AppendList2Array(&temp, &gtargets);
		Lst_Destroy(&temp, NOFREE);
		continue;
	    }
	}
	savec = *cp;

	if (*cp == '\0') {
	    /* Ending a dependency line without an operator is a Bozo no-no */
	    Parse_Error(PARSE_FATAL, "Need an operator");
	    return;
	}
	*cp = '\0';
	/* Have a word in line. See if it's a special target and set
	 * specType to match it.  */
	if (*line == '.' && isupper(line[1])) {
	    /* See if the target is a special target that must have it
	     * or its sources handled specially.  */
	    int keywd = ParseFindKeyword(line);
	    if (keywd != -1) {
		if (specType == ExPath && parseKeywords[keywd].spec != ExPath) {
		    Parse_Error(PARSE_FATAL, "Mismatched special targets");
		    return;
		}

		specType = parseKeywords[keywd].spec;
		tOp = parseKeywords[keywd].op;

		/*
		 * Certain special targets have special semantics:
		 *	.PATH		Have to set the defaultPath
		 *			variable too
		 *	.MAIN		Its sources are only used if
		 *			nothing has been specified to
		 *			create.
		 *	.DEFAULT	Need to create a node to hang
		 *			commands on, but we don't want
		 *			it in the graph, nor do we want
		 *			it to be the Main Target, so we
		 *			create it, set OP_NOTMAIN and
		 *			add it to the list, setting
		 *			DEFAULT to the new node for
		 *			later use. We claim the node is
		 *			A transformation rule to make
		 *			life easier later, when we'll
		 *			use Make_HandleUse to actually
		 *			apply the .DEFAULT commands.
		 *	.PHONY		The list of targets
		 *	.NOPATH 	Don't search for file in the path
		 *	.BEGIN
		 *	.END
		 *	.INTERRUPT	Are not to be considered the
		 *			main target.
		 *	.NOTPARALLEL	Make only one target at a time.
		 *	.SINGLESHELL	Create a shell for each command.
		 *	.ORDER		Must set initial predecessor to NULL
		 */
		switch (specType) {
		    case ExPath:
			Lst_AtEnd(&paths, defaultPath);
			break;
		    case Main:
			if (!Lst_IsEmpty(create)) {
			    specType = Not;
			}
			break;
		    case Begin:
		    case End:
		    case Interrupt:
			gn = Targ_FindNode(line, TARG_CREATE);
			gn->type |= OP_NOTMAIN;
			Array_AtEnd(&gtargets, gn);
			break;
		    case Default:
			gn = Targ_NewGN(".DEFAULT");
			gn->type |= OP_NOTMAIN|OP_TRANSFORM;
			Array_AtEnd(&gtargets, gn);
			DEFAULT = gn;
			break;
		    case NotParallel:
		    {
			extern int  maxJobs;

			maxJobs = 1;
			break;
		    }
		    case SingleShell:
			compatMake = 1;
			break;
		    case Order:
			predecessor = NULL;
			break;
		    default:
			break;
		}
	    } else if (strncmp(line, ".PATH", 5) == 0) {
		/*
		 * .PATH<suffix> has to be handled specially.
		 * Call on the suffix module to give us a path to
		 * modify.
		 */
		Lst	path;

		specType = ExPath;
		path = Suff_GetPath(&line[5]);
		if (path == NULL) {
		    Parse_Error(PARSE_FATAL,
				 "Suffix '%s' not defined (yet)",
				 &line[5]);
		    return;
		} else {
		    Lst_AtEnd(&paths, path);
		}
	    }
	}

	/*
	 * Have word in line. Get or create its node and stick it at
	 * the end of the targets list
	 */
	if (specType == Not && *line != '\0') {
	    add_target_nodes(line);
	} else if (specType == ExPath && *line != '.' && *line != '\0')
	    Parse_Error(PARSE_WARNING, "Extra target (%s) ignored", line);

	*cp = savec;
	/*
	 * If it is a special type and not .PATH, it's the only target we
	 * allow on this line...
	 */
	if (specType != Not && specType != ExPath) {
	    bool warn = false;

	    while (*cp != '!' && *cp != ':' && *cp) {
		if (*cp != ' ' && *cp != '\t') {
		    warn = true;
		}
		cp++;
	    }
	    if (warn) {
		Parse_Error(PARSE_WARNING, "Extra target ignored");
	    }
	} else {
	    while (isspace(*cp)) {
		cp++;
	    }
	}
	line = cp;
    } while (*line != '!' && *line != ':' && *line);

    if (!Array_IsEmpty(&gtargets)) {
	switch (specType) {
	    default:
		Parse_Error(PARSE_WARNING, "Special and mundane targets don't mix. Mundane ones ignored");
		break;
	    case Default:
	    case Begin:
	    case End:
	    case Interrupt:
		/* These four create nodes on which to hang commands, so
		 * targets shouldn't be empty...  */
	    case Not:
		/* Nothing special here -- targets can be empty if it wants.  */
		break;
	}
    }

    /* Have now parsed all the target names. Must parse the operator next. The
     * result is left in op .  */
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

    Array_Find(&gtargets, ParseDoOp, op);

    /*
     * Get to the first source
     */
    while (isspace(*cp)) {
	cp++;
    }
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
	    case Suffixes:
		Suff_ClearSuffixes();
		break;
	    case Precious:
		allPrecious = true;
		break;
	    case Ignore:
		ignoreErrors = true;
		break;
	    case Silent:
		beSilent = true;
		break;
	    case ExPath:
		Lst_Every(&paths, ParseClearPath);
		break;
	    default:
		break;
	}
    } else if (specType == MFlags) {
	/*
	 * Call on functions in main.c to deal with these arguments and
	 * set the initial character to a null-character so the loop to
	 * get sources won't get anything
	 */
	Main_ParseArgLine(line);
	*line = '\0';
    } else if (specType == NotParallel || specType == SingleShell) {
	*line = '\0';
    }

    /*
     * NOW GO FOR THE SOURCES
     */
    if (specType == Suffixes || specType == ExPath ||
	specType == Includes || specType == Libs ||
	specType == Null) {
	while (*line) {
	    /*
	     * If the target was one that doesn't take files as its sources
	     * but takes something like suffixes, we take each
	     * space-separated word on the line as a something and deal
	     * with it accordingly.
	     *
	     * If the target was .SUFFIXES, we take each source as a
	     * suffix and add it to the list of suffixes maintained by the
	     * Suff module.
	     *
	     * If the target was a .PATH, we add the source as a directory
	     * to search on the search path.
	     *
	     * If it was .INCLUDES, the source is taken to be the suffix of
	     * files which will be #included and whose search path should
	     * be present in the .INCLUDES variable.
	     *
	     * If it was .LIBS, the source is taken to be the suffix of
	     * files which are considered libraries and whose search path
	     * should be present in the .LIBS variable.
	     *
	     * If it was .NULL, the source is the suffix to use when a file
	     * has no valid suffix.
	     */
	    char  savec;
	    while (*cp && !isspace(*cp)) {
		cp++;
	    }
	    savec = *cp;
	    *cp = '\0';
	    switch (specType) {
		case Suffixes:
		    Suff_AddSuffix(line);
		    break;
		case ExPath:
		    Lst_ForEach(&paths, ParseAddDir, line);
		    break;
		case Includes:
		    Suff_AddInclude(line);
		    break;
		case Libs:
		    Suff_AddLib(line);
		    break;
		case Null:
		    Suff_SetNull(line);
		    break;
		default:
		    break;
	    }
	    *cp = savec;
	    if (savec != '\0') {
		cp++;
	    }
	    while (isspace(*cp)) {
		cp++;
	    }
	    line = cp;
	}
	Lst_Destroy(&paths, NOFREE);
    } else {
	while (*line) {
	    /*
	     * The targets take real sources, so we must beware of archive
	     * specifications (i.e. things with left parentheses in them)
	     * and handle them accordingly.
	     */
	    while (*cp && !isspace(*cp)) {
		if (*cp == '(' && cp > line && cp[-1] != '$') {
		    /*
		     * Only stop for a left parenthesis if it isn't at the
		     * start of a word (that'll be for variable changes
		     * later) and isn't preceded by a dollar sign (a dynamic
		     * source).
		     */
		    break;
		} else {
		    cp++;
		}
	    }

	    if (*cp == '(') {
		GNode	  *gn;
		LIST	  sources; /* list of archive source names after
				    * expansion */

		Lst_Init(&sources);
		if (!Arch_ParseArchive(&line, &sources, NULL)) {
		    Parse_Error(PARSE_FATAL,
				 "Error in source archive spec \"%s\"", line);
		    return;
		}

		while ((gn = (GNode *)Lst_DeQueue(&sources)) != NULL)
		    ParseDoSrc(tOp, gn->name);
		cp = line;
	    } else {
		if (*cp) {
		    *cp = '\0';
		    cp++;
		}

		ParseDoSrc(tOp, line);
	    }
	    while (isspace(*cp)) {
		cp++;
	    }
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

    /* Finally, destroy the list of sources.  */
}

/*-
 * ParseAddCmd	--
 *	Lst_ForEach function to add a command line to all targets
 *
 * Side Effects:
 *	A new element is added to the commands list of the node.
 */
static void
ParseAddCmd(
    void *gnp,	/* the node to which the command is to be added */
    void *cmd)	/* the command to add */
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
	if (!Lst_IsEmpty(&gn->commands)) {
		gn->type |= OP_HAS_COMMANDS;
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


/* Strip comments from the line. Build a copy in buffer if necessary, */
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

static bool
handle_for_loop(Buffer linebuf, const char *line)
{
	For *loop;

	loop = For_Eval(line+3);
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

static bool
handle_bsd_command(Buffer linebuf, Buffer copy, const char *line)
{
	char *stripped;

	while (isspace(*line))
		line++;

	/* The line might be a conditional. Ask the conditional module
	 * about it and act accordingly.  */
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
		return handle_for_loop(linebuf, line);
	case COND_ISINCLUDE:
		return lookup_bsd_include(line + 7);
	case COND_ISPOISON:
		return handle_poison(line+6);
	case COND_ISUNDEF:
		return handle_undef(line + 5);
	default:
		break;
	}

	return false;
}

/*-
 *-----------------------------------------------------------------------
 * ParseFinishDependency --
 *	Handle the end of a dependency group.
 *
 * Side Effects:
 *	'targets' list destroyed.
 *
 *-----------------------------------------------------------------------
 */
static void
ParseFinishDependency(void)
{
	Array_Every(&gtargets, Suff_EndTransform);
	Array_Every(&gtargets, ParseHasCommands);
	Array_Reset(&gtargets);
}

static void
ParseDoCommands(const char *line)
{
	/* add the command to the list of
	 * commands of all targets in the dependency spec */
	char *cmd = estrdup(line);

	Array_ForEach(&gtargets, ParseAddCmd, cmd);
#ifdef CLEANUP
	Lst_AtEnd(&targCmds, cmd);
#endif
}

void
Parse_File(
    const char	  *name,	/* the name of the file being read */
    FILE	  *stream)	/* Stream open to makefile to parse */
{
    char	  *cp,		/* pointer into the line */
		  *line;	/* the line we're working on */
    bool	  inDependency; /* true if currently in a dependency
				 * line or its commands */

    BUFFER	  buf;
    BUFFER	  copy;

    Buf_Init(&buf, MAKE_BSIZE);
    Buf_Init(&copy, MAKE_BSIZE);
    inDependency = false;
    Parse_FromFile(name, stream);

    do {
	while ((line = Parse_ReadNormalLine(&buf)) != NULL) {
	    if (*line == '\t') {
		if (inDependency)
		    ParseDoCommands(line+1);
		else
		    Parse_Error(PARSE_FATAL,
			"Unassociated shell command \"%s\"",
			 line);
	    } else {
		char *stripped;
		stripped = strip_comments(&copy, line);
		if (*stripped == '.' && handle_bsd_command(&buf, &copy,
		    stripped+1))
			;
		else if (FEATURES(FEATURE_SYSVINCLUDE) &&
		    strncmp(stripped, "include", 7) == 0 &&
		    isspace(stripped[7]) &&
		    strchr(stripped, ':') == NULL) {
		    /* It's an S3/S5-style "include".  */
			lookup_sysv_include(stripped + 7, "include");
		} else if (FEATURES(FEATURE_CONDINCLUDE) &&
		    strncmp(stripped, "sinclude", 8) == 0 &&
		    isspace(stripped[8]) &&
		    strchr(stripped, ':') == NULL) {
		    	lookup_conditional_include(stripped+8, "sinclude");
		} else if (FEATURES(FEATURE_CONDINCLUDE) &&
		    strncmp(stripped, "-include", 8) == 0 &&
		    isspace(stripped[8]) &&
		    strchr(stripped, ':') == NULL) {
		    	lookup_conditional_include(stripped+8, "-include");
		} else {
		    char *dep;

		    if (inDependency)
			ParseFinishDependency();
		    if (Parse_DoVar(stripped))
			inDependency = false;
		    else {
			size_t pos;
			char *end;

			/* Need a new list for the target nodes.  */
			Array_Reset(&gtargets);
			inDependency = true;

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
			 * Tell the variable module to complain if some
			 * variable is undefined... */
			cp = Var_Subst(stripped, NULL, true);
			ParseDoDependency(cp);
			free(cp);

			/* Parse dependency if it's not empty. */
			if (dep != NULL) {
			    do {
				dep++;
			    } while (isspace(*dep));
			    if (*dep != '\0')
				ParseDoCommands(dep);
			}
		    }
		}
	    }
	}
    } while (Parse_NextFile());

    if (inDependency)
	ParseFinishDependency();
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
	Array_Init(&gsources, SOURCES_SIZE);
	Array_Init(&gtargets, TARGETS_SIZE);

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

