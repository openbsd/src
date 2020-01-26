#ifndef GNODE_H
#define GNODE_H
/*	$OpenBSD: gnode.h,v 1.39 2020/01/26 12:41:21 espie Exp $ */

/*
 * Copyright (c) 2001 Marc Espie.
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

#include <sys/time.h>
#ifndef LIST_TYPE
#include "lst_t.h"
#endif
#ifndef LOCATION_TYPE
#include "location.h"
#endif
#ifndef SYMTABLE_H
#include "symtable.h"
#endif
#include <assert.h>

/*-
 * The structure for an individual graph node. Each node has a lot of
 * of data associated with it.
 *	1) the *name*of the target it describes (at end because ohash)
 *	2) the *path* to the target file
 *	3) the *type* of operator used to define its sources 
 *		(cf parse.c, mostly : :: !  but...)
 *	4) *must_make*: whether it is involved in this invocation of make
 *	5) *built_status*: has the target been rebuilt/is up-to-date...
 *	6) *child_rebuild*: at least one of its children has been rebuilt
 *	7) *children_left*: number of children still to consider
 *	8) *mtime*: node's modification time
 *	9) *youngest*: youngest child (cf make.c)
 *	10) *parents*: list of nodes for which this is a dependency
 *	11) *children*: list of nodes on which this depends
 *	12) *cohorts*: list of nodes of the same name created by the :: operator
 *	13) *predecessors*: list of nodes, result of .ORDER:
 *	    if considered for building, they should be built before this node.
 *	14) *successors*: list of nodes, result of .ORDER:
 *	    if considered for building, they should be built after this node.
 *	15) *localvars*: ``local'' variables specific to this target
 *	   and this target only (cf var.c [$@ $< $?, etc.])
 *	16) *commands*: the actual LIST of strings to pass to the shell
 *	   to create this target.
 */

/* constants for specials
 * Most of these values are only handled by parse.c.
 * In many cases, there is a corresponding OP_* flag
 */
#define SPECIAL_NONE		0U
#define SPECIAL_PATH		62U	/* handled by parse.c and suff.c */

#define SPECIAL_EXEC		4U
#define SPECIAL_IGNORE		5U
#define SPECIAL_NOTHING 	6U	/* this is used for things we
					 * recognize for compatibility but
					 * don't do anything with... */
#define SPECIAL_DEPRECATED	7U	/* this is an old keyword and it will
					 * trigger a fatal error. */
#define SPECIAL_INVISIBLE	8U
#define SPECIAL_JOIN		9U
#define SPECIAL_MADE		11U
#define SPECIAL_MAIN		12U
#define SPECIAL_MAKE		13U
#define SPECIAL_MFLAGS		14U
#define SPECIAL_NOTMAIN		15U
#define SPECIAL_NOTPARALLEL	16U
#define SPECIAL_OPTIONAL	18U
#define SPECIAL_ORDER		19U
#define SPECIAL_PARALLEL	20U
#define SPECIAL_PHONY		22U
#define SPECIAL_PRECIOUS	23U
#define SPECIAL_SILENT		25U
#define SPECIAL_SUFFIXES	27U
#define SPECIAL_USE		28U
#define SPECIAL_WAIT		29U
#define SPECIAL_NOPATH		30U
#define SPECIAL_ERROR		31U
#define SPECIAL_CHEAP		32U
#define SPECIAL_EXPENSIVE	33U

struct GNode_ {
    unsigned int type;		/* node type (see the OP flags, below) */
    unsigned int special_op;	/* special op to apply (only used in parse.c) */
    unsigned char special;	/* type of special node or SPECIAL_NONE */
    bool must_make;		/* true if this target needs building */
    bool child_rebuilt;		/* true if at least one child was rebuilt,
    			 	 * thus triggering timestamps changes */

    char built_status;	
#define UNKNOWN		0	/* Not examined yet */
#define BUILDING	1	/* In the process of building */
#define REBUILT		2	/* Was out of date and got rebuilt */
#define UPTODATE	3	/* Was already up-to-date */
#define ERROR		4	/* Error occurred while building
				 * (used only in compat mode) */
#define ABORTED		5	/* Was aborted due to an error 
				 * making an inferior */
#define NOSUCHNODE	6	/* error from run_gnode */
#define HELDBACK	7	/* Another target in the same group is
				 * currently building, avoid race conditions
				 * Only used in the parallel engine make.c */

    char *path;		/* full pathname of the file */
    int order;		/* wait weight (see .ORDER/predecessors/successors) */

    int children_left;	/* number of children left to build */

    struct timespec mtime;	/* Node's modification time */
    GNode *youngest;		/* Node's youngest child */

    GNode *impliedsrc;	/* found by suff, to help with localvars */
    LIST cohorts;	/* Other nodes for the :: operator */
    LIST parents;	/* Nodes that depend on this one */
    LIST children;	/* Nodes on which this one depends */
    LIST predecessors;
    LIST successors; 	

    SymTable localvars;
    LIST commands;	/* Creation commands */
    Suff *suffix;	/* Suffix for the node (determined by
			 * Suff_FindDeps and opaque to everyone
			 * but the Suff module) */
    GNode *groupling;	/* target lists, for HELDBACK: do not build two
    			 * at the same time */
    GNode *watched;	/* the node currently building for HELDBACK */

			/* stuff for target name equivalence: */
    GNode *sibling;	/* equivalent targets (not complete yet) */
    char *basename;	/* pointer to name stripped of path */
    GNode *next;

    bool in_cycle;	/* cycle detection */
    char name[1];	/* The target's name */
};

struct command
{
	Location location;
	char string[1];
};

#define has_been_built(gn) \
	((gn)->built_status == REBUILT || (gn)->built_status == UPTODATE)
#define should_have_file(gn) \
	((gn)->special == SPECIAL_NONE && \
	((gn)->type & (OP_PHONY | OP_DUMMY)) == 0)
/*
 * The OP_ constants are used when parsing a dependency line as a way of
 * communicating to other parts of the program the way in which a target
 * should be made. These constants are bitwise-OR'ed together and
 * placed in the 'type' field of each node. Any node that has
 * a 'type' field which satisfies the OP_NOP function was never never on
 * the lefthand side of an operator, though it may have been on the
 * righthand side...
 */
#define OP_ZERO		0x00000000  /* No dependency operator seen so far */
#define OP_DEPENDS	0x00000001  /* Execution of commands depends on
				     * kids (:) */
#define OP_FORCE	0x00000002  /* Always execute commands (!) */
#define OP_DOUBLEDEP	0x00000004  /* Execution of commands depends on kids
				     * per line (::) */
#define OP_ERROR	0x00000007
#define OP_OPMASK	(OP_DEPENDS|OP_FORCE|OP_DOUBLEDEP)

#define OP_OPTIONAL	0x00000008  /* Don't care if the target doesn't
				     * exist and can't be created */
#define OP_USE		0x00000010  /* Use associated commands for parents */
#define OP_IGNORE	0x00000040  /* Ignore errors when creating the node */
#define OP_PRECIOUS	0x00000080  /* Don't remove the target when
				     * interrupted */
#define OP_SILENT	0x00000100  /* Don't echo commands when executed */
#define OP_MAKE 	0x00000200  /* Target is a recursive make so its
				     * commands should always be executed when
				     * it is out of date, regardless of the
				     * state of the -n or -t flags */
#define OP_INVISIBLE	0x00001000  /* The node is invisible to its parents.
				     * I.e. it doesn't show up in the parents's
				     * local variables. Used by :: for
				     * supplementary nodes (cohorts). */
#define OP_NOTMAIN	0x00002000  /* The node is exempt from normal 'main
				     * target' processing in parse.c */
#define OP_PHONY	0x00004000  /* Not a file target; run always */
#define OP_NOPATH	0x00008000  /* Don't search for file in the path */
#define OP_NODEFAULT	0x00010000  /* Special node that never needs */
				    /* DEFAULT commands applied */
#define OP_DUMMY	0x00020000  /* node was created by default, but it */
				    /* does not really exist. */
/* Attributes applied by PMake */
#define OP_TRANSFORM	0x00040000  /* The node is a transformation rule */
#define OP_MEMBER	0x00080000  /* Target is a member of an archive */
#define OP_DOUBLE	0x00100000  /* normal op with double commands */
#define OP_ARCHV	0x00200000  /* Target is an archive construct */
#define OP_HAS_COMMANDS 0x00400000  /* Target has all the commands it should.
				     * Used when parsing to catch multiple
				     * commands for a target */
#define OP_DEPS_FOUND	0x00800000  /* Already processed by Suff_FindDeps */
#define OP_RESOLVED	0x01000000  /* We looked harder already */
#define OP_CHEAP	0x02000000  /* Assume job is not recursive */
#define OP_EXPENSIVE	0x04000000  /* Recursive job, don't run in parallel */

/*
 * OP_NOP will return true if the node with the given type was not the
 * object of a dependency operator
 */
#define OP_NOP(t)	(((t) & OP_OPMASK) == OP_ZERO)

#define OP_NOTARGET (OP_NOTMAIN|OP_USE|OP_TRANSFORM)


#endif
