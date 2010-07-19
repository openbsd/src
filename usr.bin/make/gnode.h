#ifndef GNODE_H
#define GNODE_H
/*	$OpenBSD: gnode.h,v 1.17 2010/07/19 19:46:44 espie Exp $ */

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

#ifndef TIMESTAMP_TYPE
#include "timestamp_t.h"
#endif
#ifndef LIST_TYPE
#include "lst_t.h"
#endif
#ifndef SYMTABLE_H
#include "symtable.h"
#endif

struct Suff_;
/*-
 * The structure for an individual graph node. Each node has several
 * pieces of data associated with it.
 *	1) the name of the target it describes
 *	2) the location of the target file in the file system.
 *	3) the type of operator used to define its sources (qv. parse.c)
 *	4) whether it is involved in this invocation of make
 *	5) whether the target has been remade
 *	6) whether any of its children has been remade
 *	7) the number of its children that are, as yet, unmade
 *	8) its modification time
 *	9) the modification time of its youngest child (qv. make.c)
 *	10) a list of nodes for which this is a source
 *	11) a list of nodes on which this depends
 *	12) a list of nodes that depend on this, as gleaned from the
 *	    transformation rules.
 *	13) a list of nodes of the same name created by the :: operator
 *	14) a list of nodes that must be made (if they're made) before
 *	    this node can be, but that do no enter into the datedness of
 *	    this node.
 *	15) a list of nodes that must be made (if they're made) after
 *	    this node is, but that do not depend on this node, in the
 *	    normal sense.
 *	16) a Lst of ``local'' variables that are specific to this target
 *	   and this target only (qv. var.c [$@ $< $?, etc.])
 *	17) a Lst of strings that are commands to be given to a shell
 *	   to create this target.
 */

#define UNKNOWN		0
#define BEINGMADE	1
#define MADE		2
#define UPTODATE	3
#define ERROR		4
#define ABORTED		5
#define CYCLE		6
#define ENDCYCLE	7
#define NOSUCHNODE	8

#define SPECIAL_NONE	0U
#define SPECIAL_PATH		21U
#define SPECIAL_MASK		63U
#define SPECIAL_TARGET		64U
#define SPECIAL_SOURCE		128U
#define SPECIAL_TARGETSOURCE	(SPECIAL_TARGET|SPECIAL_SOURCE)

struct GNode_ {
    unsigned int special_op;	/* special op to apply */
    unsigned char special;/* type of special node */
    char must_make;	/* true if this target needs to be remade */
    char childMade;	/* true if one of this target's children was
			 * made */
    char built_status;	/* Set to reflect the state of processing
			 * on this node:
			 *  UNKNOWN - Not examined yet
			 *  BEINGMADE - Target is currently being made.
			 *  MADE - Was out-of-date and has been made
			 *  UPTODATE - Was already up-to-date
			 *  ERROR - An error occurred while it was being
			 *	made (used only in compat mode)
			 *  ABORTED - The target was aborted due to
			 *	an error making an inferior.
			 *  CYCLE - Marked as potentially being part of
			 *	a graph cycle. If we come back to a
			 *	node marked this way, it is printed
			 *	and 'built_status' is changed to ENDCYCLE.
			 *  ENDCYCLE - the cycle has been completely
			 *	printed. Go back and unmark all its
			 *	members.
			 */
    char build_lock;	/* for parallel build in siblings */
    char *path;		/* The full pathname of the file */
    unsigned int type;	/* Its type (see the OP flags, below) */
    int order;		/* Its wait weight */

    int unmade;		/* The number of unmade children */

    TIMESTAMP mtime;	/* Its modification time */
    TIMESTAMP cmtime;	/* The modification time of its youngest
			 * child */

    GNode *impliedsrc;
    LIST cohorts;	/* Other nodes for the :: operator */
    LIST parents;	/* Nodes that depend on this one */
    LIST children;	/* Nodes on which this one depends */
    LIST successors; 	/* Nodes that must be made after this one */
    LIST preds;		/* Nodes that must be made before this one */

    SymTable context;	/* The local variables */
    unsigned long lineno;/* First line number of commands.  */
    const char *fname;	/* File name of commands.  */
    LIST commands;	/* Creation commands */
    LIST expanded;	/* Expanded commands */
    struct Suff_ *suffix;/* Suffix for the node (determined by
			 * Suff_FindDeps and opaque to everyone
			 * but the Suff module) */
    struct GNode_ *sibling;	/* equivalent targets */
    /* stuff for target name equivalence */
    char *basename;	/* pointer to name stripped of path */
    struct GNode_ *next;
    char name[1];	/* The target's name */
};

#define has_been_built(gn) \
	((gn)->built_status == MADE || (gn)->built_status == UPTODATE)
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
#define OP_DEPENDS	0x00000001  /* Execution of commands depends on
				     * kids (:) */
#define OP_FORCE	0x00000002  /* Always execute commands (!) */
#define OP_DOUBLEDEP	0x00000004  /* Execution of commands depends on kids
				     * per line (::) */
#define OP_ERROR	0x00000000
#define OP_OPMASK	(OP_DEPENDS|OP_FORCE|OP_DOUBLEDEP)

#define OP_OPTIONAL	0x00000008  /* Don't care if the target doesn't
				     * exist and can't be created */
#define OP_USE		0x00000010  /* Use associated commands for parents */
#define OP_EXEC 	0x00000020  /* Target is never out of date, but always
				     * execute commands anyway. Its time
				     * doesn't matter, so it has none...sort
				     * of */
#define OP_IGNORE	0x00000040  /* Ignore errors when creating the node */
#define OP_PRECIOUS	0x00000080  /* Don't remove the target when
				     * interrupted */
#define OP_SILENT	0x00000100  /* Don't echo commands when executed */
#define OP_MAKE 	0x00000200  /* Target is a recursive make so its
				     * commands should always be executed when
				     * it is out of date, regardless of the
				     * state of the -n or -t flags */
#define OP_JOIN 	0x00000400  /* Target is out-of-date only if any of its
				     * children was out-of-date */
#define OP_MADE 	0x00000800  /* Assume the node is already made; even if
				     * it really is out of date */
#define OP_INVISIBLE	0x00001000  /* The node is invisible to its parents.
				     * I.e. it doesn't show up in the parents's
				     * local variables. */
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
#define OP_LIB		0x00100000  /* Target is a library */
#define OP_ARCHV	0x00200000  /* Target is an archive construct */
#define OP_HAS_COMMANDS 0x00400000  /* Target has all the commands it should.
				     * Used when parsing to catch multiple
				     * commands for a target */
#define OP_DEPS_FOUND	0x00800000  /* Already processed by Suff_FindDeps */
#define OP_RESOLVED	0x01000000  /* We looked harder already */

/*
 * OP_NOP will return true if the node with the given type was not the
 * object of a dependency operator
 */
#define OP_NOP(t)	(((t) & OP_OPMASK) == 0x00000000)

#define OP_NOTARGET (OP_NOTMAIN|OP_USE|OP_EXEC|OP_TRANSFORM)


#endif
