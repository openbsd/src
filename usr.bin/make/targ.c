/*	$OpenBSD: targ.c,v 1.29 2000/11/24 14:36:35 espie Exp $	*/
/*	$NetBSD: targ.c,v 1.11 1997/02/20 16:51:50 christos Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * targ.c --
 *		Target nodes are kept into a hash table.
 *
 * Interface:
 *	Targ_Init 	    	Initialization procedure.
 *
 *	Targ_End 	    	Cleanup the module
 *
 *	Targ_NewGN	    	Create a new GNode for the passed target
 *	    	  	    	(string). The node is *not* placed in the
 *	    	  	    	hash table, though all its fields are
 *	    	  	    	initialized.
 *
 *	Targ_FindNode	    	Find the node for a given target, creating
 *	    	  	    	and storing it if it doesn't exist and the
 *	    	  	    	flags are right (TARG_CREATE)
 *
 *	Targ_FindList	    	Given a list of names, find nodes for all
 *	    	  	    	of them, creating nodes if needed.
 *
 *	Targ_Ignore	    	Return TRUE if errors should be ignored when
 *	    	  	    	creating the given target.
 *
 *	Targ_Silent	    	Return TRUE if we should be silent when
 *	    	  	    	creating the given target.
 *
 *	Targ_Precious	    	Return TRUE if the target is precious and
 *	    	  	    	should not be removed if we are interrupted.
 *
 * Debugging:
 *	Targ_PrintGraph	    	Print out the entire graphm all variables
 *	    	  	    	and statistics for the directory cache. Should
 *	    	  	    	print something for suffixes, too, but...
 */

#include	  <stddef.h>
#include	  <stdio.h>
#include	  <time.h>
#include	  "make.h"
#include	  "ohash.h"
#include	  "dir.h"

#ifndef lint
#if 0
static char sccsid[] = "@(#)targ.c	8.2 (Berkeley) 3/19/94";
#else
UNUSED
static char *rcsid = "$OpenBSD: targ.c,v 1.29 2000/11/24 14:36:35 espie Exp $";
#endif
#endif /* not lint */

#ifdef CLEANUP
static LIST	  allGNs;	/* List of all the GNodes */
#endif
static struct hash targets; 	/* a hash table of same */
static struct hash_info gnode_info = { 
	offsetof(GNode, name), 
    NULL, hash_alloc, hash_free, element_alloc };

static void TargPrintOnlySrc __P((GNode *));
static void TargPrintName __P((void *));
static void TargPrintNode __P((GNode *, int));
#ifdef CLEANUP
static void TargFreeGN __P((void *));
#endif

/*-
 *-----------------------------------------------------------------------
 * Targ_Init --
 *	Initialize this module
 *
* Side Effects:
 *	The targets hash table is initialized
 *-----------------------------------------------------------------------
 */
void
Targ_Init()
{
#ifdef CLEANUP
    Lst_Init(&allGNs);
#endif
    /* A small make file already creates 200 targets.  */
    hash_init(&targets, 10, &gnode_info);
}

/*-
 *-----------------------------------------------------------------------
 * Targ_End --
 *	Finalize this module
 *
 * Side Effects:
 *	All lists and gnodes are cleared
 *-----------------------------------------------------------------------
 */
void
Targ_End ()
{
#ifdef CLEANUP
    Lst_Destroy(&allGNs, TargFreeGN);
    hash_delete(&targets);
#endif
}

/*-
 *-----------------------------------------------------------------------
 * Targ_NewGN  --
 *	Create and initialize a new graph node
 *
 * Results:
 *	An initialized graph node with the name field filled with a copy
 *	of the passed name
 *
 * Side Effects:
 *	The gnode is added to the set of all gnodes.
 *-----------------------------------------------------------------------
 */
GNode *
Targ_NewGN(name, end)
    const char		*name;	/* the name to stick in the new node */
    const char 		*end;
{
    GNode 		*gn;

    gn = hash_create_entry(&gnode_info, name, &end);
    gn->path = NULL;
    if (name[0] == '-' && name[1] == 'l') {
	gn->type = OP_LIB;
    } else {
	gn->type = 0;
    }
    gn->unmade =    	0;
    gn->make = 	    	FALSE;
    gn->made = 	    	UNMADE;
    gn->childMade = 	FALSE;
    gn->order =		0;
    set_out_of_date(gn->mtime);
    set_out_of_date(gn->cmtime);
    Lst_Init(&gn->iParents);
    Lst_Init(&gn->cohorts);
    Lst_Init(&gn->parents);
    Lst_Init(&gn->children);
    Lst_Init(&gn->successors);
    Lst_Init(&gn->preds);
    SymTable_Init(&gn->context);
    gn->lineno = 0;
    gn->fname = NULL;
    Lst_Init(&gn->commands);
    gn->suffix =	NULL;

#ifdef CLEANUP
    Lst_AtEnd(&allGNs, gn);
#endif

    return gn;
}

#ifdef CLEANUP
/*-
 *-----------------------------------------------------------------------
 * TargFreeGN  --
 *	Destroy a GNode
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	None.
 *-----------------------------------------------------------------------
 */
static void
TargFreeGN(gnp)
    void *gnp;
{
    GNode *gn = (GNode *) gnp;

    efree(gn->path);
    Lst_Destroy(&gn->iParents, NOFREE);
    Lst_Destroy(&gn->cohorts, NOFREE);
    Lst_Destroy(&gn->parents, NOFREE);
    Lst_Destroy(&gn->children, NOFREE);
    Lst_Destroy(&gn->successors, NOFREE);
    Lst_Destroy(&gn->preds, NOFREE);
    SymTable_Destroy(&gn->context);
    Lst_Destroy(&gn->commands, NOFREE);
    free(gn);
}
#endif


/*-
 *-----------------------------------------------------------------------
 * Targ_FindNode  --
 *	Find a node in the list using the given name for matching
 *
 * Results:
 *	The node in the list if it was. If it wasn't, return NULL of
 *	flags was TARG_NOCREATE or the newly created and initialized node
 *	if it was TARG_CREATE
 *
 * Side Effects:
 *	Sometimes a node is created and added to the list
 *-----------------------------------------------------------------------
 */
GNode *
Targ_FindNode(name, flags)
    const char		*name;	/* the name to find */
    int			flags;	/* flags governing events when target not
				 * found */
{
    const char		*end = NULL;
    GNode         	*gn;	/* node in that element */
    unsigned int	slot;

    slot = hash_qlookupi(&targets, name, &end);

    gn = hash_find(&targets, slot);
    
    if (gn == NULL && (flags & TARG_CREATE)) {
    	gn = Targ_NewGN(name, end);
	hash_insert(&targets, slot, gn);
    }

    return gn;
}

/*-
 *-----------------------------------------------------------------------
 * Targ_FindList --
 *	Make a complete list of GNodes from the given list of names
 *
 * Side Effects:
 *	Nodes will be created for all names which do not yet have graph
 *	nodes.
 *
 *	A complete list of graph nodes corresponding to all instances of
 *	all names is added to nodes.
 * -----------------------------------------------------------------------
 */
void
Targ_FindList(nodes, names)
    Lst		nodes;		/* result list */
    Lst		names;		/* list of names to find */
{
    LstNode  	ln;		/* name list element */
    GNode	*gn;		/* node in tLn */
    char	*name;

    for (ln = Lst_First(names); ln != NULL; ln = Lst_Adv(ln)) {
	name = (char *)Lst_Datum(ln);
	gn = Targ_FindNode(name, TARG_CREATE);
	/*
	 * Note: Lst_AtEnd must come before the Lst_Concat so the nodes
	 * are added to the list in the order in which they were
	 * encountered in the makefile.
	 */
	Lst_AtEnd(nodes, gn);
	if (gn->type & OP_DOUBLEDEP)
	    Lst_Concat(nodes, &gn->cohorts);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Targ_Ignore  --
 *	Return true if should ignore errors when creating gn
 *-----------------------------------------------------------------------
 */
Boolean
Targ_Ignore(gn)
    GNode          *gn;		/* node to check for */
{
    if (ignoreErrors || gn->type & OP_IGNORE)
	return TRUE;
    else
	return FALSE;
}

/*-
 *-----------------------------------------------------------------------
 * Targ_Silent  --
 *	Return true if be silent when creating gn
 *-----------------------------------------------------------------------
 */
Boolean
Targ_Silent(gn)
    GNode          *gn;		/* node to check for */
{
    if (beSilent || gn->type & OP_SILENT)
	return TRUE;
    else
	return FALSE;
}

/*-
 *-----------------------------------------------------------------------
 * Targ_Precious --
 *	See if the given target is precious
 *-----------------------------------------------------------------------
 */
Boolean
Targ_Precious (gn)
    GNode          *gn;		/* the node to check */
{
    if (allPrecious || (gn->type & (OP_PRECIOUS|OP_DOUBLEDEP)))
	return TRUE;
    else
	return FALSE;
}

/******************* DEBUG INFO PRINTING ****************/

static GNode	  *mainTarg;	/* the main target, as set by Targ_SetMain */
/*-
 *-----------------------------------------------------------------------
 * Targ_SetMain --
 *	Set our idea of the main target we'll be creating. Used for
 *	debugging output.
 *
 * Side Effects:
 *	"mainTarg" is set to the main target's node.
 *-----------------------------------------------------------------------
 */
void
Targ_SetMain(gn)
    GNode   *gn;  	/* The main target we'll create */
{
    mainTarg = gn;
}

static void
TargPrintName(gnp)
    void *gnp;
{
    GNode *gn = (GNode *)gnp;

    printf("%s ", gn->name);
}


void
Targ_PrintCmd(cmd)
    void *cmd;
{
    printf("\t%s\n", (char *)cmd);
}

/*-
 *-----------------------------------------------------------------------
 * Targ_FmtTime --
 *	Format a modification time in some reasonable way and return it.
 *
 * Results:
 *	The time reformatted.
 *
 * Side Effects:
 *	The time is placed in a static area, so it is overwritten
 *	with each call.
 *
 *-----------------------------------------------------------------------
 */
char *
Targ_FmtTime(time)
    TIMESTAMP		time;
{
    struct tm	  	*parts;
    static char		buf[128];
    time_t t;

    t = timestamp2time_t(time);

    parts = localtime(&t);
    strftime(buf, sizeof buf, "%k:%M:%S %b %d, %Y", parts);
    buf[sizeof(buf) - 1] = '\0';
    return(buf);
}

/*-
 *-----------------------------------------------------------------------
 * Targ_PrintType --
 *	Print out a type field giving only those attributes the user can
 *	set.
 *-----------------------------------------------------------------------
 */
void
Targ_PrintType(type)
    int    type;
{
    int    tbit;

#ifdef __STDC__
#define PRINTBIT(attr)	case CONCAT(OP_,attr): printf("." #attr " "); break
#define PRINTDBIT(attr) case CONCAT(OP_,attr): if (DEBUG(TARG)) printf("." #attr " "); break
#else
#define PRINTBIT(attr) 	case CONCAT(OP_,attr): printf(".attr "); break
#define PRINTDBIT(attr)	case CONCAT(OP_,attr): if (DEBUG(TARG)) printf(".attr "); break
#endif /* __STDC__ */

    type &= ~OP_OPMASK;

    while (type) {
	tbit = 1 << (ffs(type) - 1);
	type &= ~tbit;

	switch(tbit) {
	    PRINTBIT(OPTIONAL);
	    PRINTBIT(USE);
	    PRINTBIT(EXEC);
	    PRINTBIT(IGNORE);
	    PRINTBIT(PRECIOUS);
	    PRINTBIT(SILENT);
	    PRINTBIT(MAKE);
	    PRINTBIT(JOIN);
	    PRINTBIT(INVISIBLE);
	    PRINTBIT(NOTMAIN);
	    PRINTDBIT(LIB);
	    /*XXX: MEMBER is defined, so CONCAT(OP_,MEMBER) gives OP_"%" */
	    case OP_MEMBER: if (DEBUG(TARG)) printf(".MEMBER "); break;
	    PRINTDBIT(ARCHV);
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * TargPrintNode --
 *	print the contents of a node
 *-----------------------------------------------------------------------
 */
static void
TargPrintNode(gn, pass)
    GNode 	*gn;
    int  	pass;
{
    if (!OP_NOP(gn->type)) {
	printf("#\n");
	if (gn == mainTarg)
	    printf("# *** MAIN TARGET ***\n");
	if (pass == 2) {
	    if (gn->unmade)
		printf("# %d unmade children\n", gn->unmade);
	    else
		printf("# No unmade children\n");
	    if (! (gn->type & (OP_JOIN|OP_USE|OP_EXEC))) {
		if (!is_out_of_date(gn->mtime))
		    printf("# last modified %s: %s\n",
			      Targ_FmtTime(gn->mtime),
			      (gn->made == UNMADE ? "unmade" :
			       (gn->made == MADE ? "made" :
				(gn->made == UPTODATE ? "up-to-date" :
				 "error when made"))));
		else if (gn->made != UNMADE)
		    printf("# non-existent (maybe): %s\n",
			      (gn->made == MADE ? "made" :
			       (gn->made == UPTODATE ? "up-to-date" :
				(gn->made == ERROR ? "error when made" :
				 "aborted"))));
		else
		    printf("# unmade\n");
	    }
	    if (!Lst_IsEmpty(&gn->iParents)) {
		printf("# implicit parents: ");
		Lst_Every(&gn->iParents, TargPrintName);
		fputc('\n', stdout);
	    }
	}
	if (!Lst_IsEmpty(&gn->parents)) {
	    printf("# parents: ");
	    Lst_Every(&gn->parents, TargPrintName);
	    fputc ('\n', stdout);
	}

	printf("%-16s", gn->name);
	switch (gn->type & OP_OPMASK) {
	    case OP_DEPENDS:
		printf(": "); break;
	    case OP_FORCE:
		printf("! "); break;
	    case OP_DOUBLEDEP:
		printf(":: "); break;
	}
	Targ_PrintType(gn->type);
	Lst_Every(&gn->children, TargPrintName);
	fputc('\n', stdout);
	Lst_Every(&gn->commands, Targ_PrintCmd);
	printf("\n\n");
	if (gn->type & OP_DOUBLEDEP) {
	    LstNode ln;
	    
	    for (ln = Lst_First(&gn->cohorts); ln != NULL; ln = Lst_Adv(ln)) 
		    TargPrintNode((GNode *)Lst_Datum(ln), pass);
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * TargPrintOnlySrc --
 *	Print only those targets that are just a source.
 *-----------------------------------------------------------------------
 */
static void
TargPrintOnlySrc(gn)
    GNode 	*gn;
{
    if (OP_NOP(gn->type))
	printf("#\t%s [%s]\n", gn->name, gn->path ? gn->path : gn->name);
}

/*-
 *-----------------------------------------------------------------------
 * Targ_PrintGraph --
 *	print the entire graph.
 *-----------------------------------------------------------------------
 */
void
Targ_PrintGraph(pass)
    int	    		pass; 	/* Which pass this is. 1 => no processing
			 	 * 2 => processing done */
{
    GNode		*gn;
    unsigned int	i;

    printf("#*** Input graph:\n");
    for (gn = hash_first(&targets, &i); gn != NULL; 
	gn = hash_next(&targets, &i))
	    TargPrintNode(gn, pass);
    printf("\n\n");
    printf("#\n#   Files that are only sources:\n");
    for (gn = hash_first(&targets, &i); gn != NULL; 
	gn = hash_next(&targets, &i))
		TargPrintOnlySrc(gn);
    printf("#*** Global Variables:\n");
    Var_Dump(VAR_GLOBAL);
    printf("#*** Command-line Variables:\n");
    Var_Dump(VAR_CMD);
    printf("\n");
    Dir_PrintDirectories();
    printf("\n");
    Suff_PrintAll();
}
