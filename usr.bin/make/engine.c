/*	$OpenBSD: engine.c,v 1.2 2007/09/16 12:09:36 espie Exp $ */
/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
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

#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "defines.h"
#include "dir.h"
#include "engine.h"
#include "arch.h"
#include "gnode.h"
#include "targ.h"
#include "var.h"
#include "extern.h"
#include "lst.h"
#include "timestamp.h"
#include "make.h"
#include "main.h"

static void MakeTimeStamp(void *, void *);
static void MakeAddAllSrc(void *, void *);

/*-
 *-----------------------------------------------------------------------
 * Job_CheckCommands --
 *	Make sure the given node has all the commands it needs.
 *
 * Results:
 *	true if the commands list is/was ok.
 *
 * Side Effects:
 *	The node will have commands from the .DEFAULT rule added to it
 *	if it needs them.
 *-----------------------------------------------------------------------
 */
bool
Job_CheckCommands(GNode *gn, 		/* The target whose commands need
				     	 * verifying */
    void (*abortProc)(char *, ...)) 	/* Function to abort with message */
{
    if (OP_NOP(gn->type) && Lst_IsEmpty(&gn->commands) &&
	(gn->type & OP_LIB) == 0) {
	/*
	 * No commands. Look for .DEFAULT rule from which we might infer
	 * commands
	 */
	if (DEFAULT != NULL && !Lst_IsEmpty(&DEFAULT->commands)) {
	    /*
	     * Make only looks for a .DEFAULT if the node was never the
	     * target of an operator, so that's what we do too. If
	     * a .DEFAULT was given, we substitute its commands for gn's
	     * commands and set the IMPSRC variable to be the target's name
	     * The DEFAULT node acts like a transformation rule, in that
	     * gn also inherits any attributes or sources attached to
	     * .DEFAULT itself.
	     */
	    Make_HandleUse(DEFAULT, gn);
	    Varq_Set(IMPSRC_INDEX, Varq_Value(TARGET_INDEX, gn), gn);
	} else if (is_out_of_date(Dir_MTime(gn))) {
	    /*
	     * The node wasn't the target of an operator we have no .DEFAULT
	     * rule to go on and the target doesn't already exist. There's
	     * nothing more we can do for this branch. If the -k flag wasn't
	     * given, we stop in our tracks, otherwise we just don't update
	     * this node's parents so they never get examined.
	     */
	    static const char msg[] = "make: don't know how to make";

	    if (gn->type & OP_OPTIONAL) {
		(void)fprintf(stdout, "%s %s(ignored)\n", msg, gn->name);
		(void)fflush(stdout);
	    } else if (keepgoing) {
		(void)fprintf(stdout, "%s %s(continuing)\n", msg, gn->name);
		(void)fflush(stdout);
		return false;
	    } else {
		(*abortProc)("%s %s. Stop in %s.", msg, gn->name,
			Var_Value(".CURDIR"));
		return false;
	    }
	}
    }
    return true;
}

/*-
 *-----------------------------------------------------------------------
 * Job_Touch --
 *	Touch the given target. Called by JobStart when the -t flag was
 *	given
 *
 * Side Effects:
 *	The data modification of the file is changed. In addition, if the
 *	file did not exist, it is created.
 *-----------------------------------------------------------------------
 */
void
Job_Touch(GNode *gn,		/* the node of the file to touch */
    bool silent)		/* true if should not print messages */
{
    int 	  streamID;	/* ID of stream opened to do the touch */

    if (gn->type & (OP_JOIN|OP_USE|OP_EXEC|OP_OPTIONAL)) {
	/*
	 * .JOIN, .USE, .ZEROTIME and .OPTIONAL targets are "virtual" targets
	 * and, as such, shouldn't really be created.
	 */
	return;
    }

    if (!silent) {
	(void)fprintf(stdout, "touch %s\n", gn->name);
	(void)fflush(stdout);
    }

    if (noExecute) {
	return;
    }

    if (gn->type & OP_ARCHV) {
	Arch_Touch(gn);
    } else if (gn->type & OP_LIB) {
	Arch_TouchLib(gn);
    } else {
	const char *file = gn->path != NULL ? gn->path : gn->name;

	if (set_times(file) == -1){
	    streamID = open(file, O_RDWR | O_CREAT, 0666);

	    if (streamID >= 0) {
		char	c;

		/*
		 * Read and write a byte to the file to change the
		 * modification time, then close the file.
		 */
		if (read(streamID, &c, 1) == 1) {
		    (void)lseek(streamID, 0, SEEK_SET);
		    (void)write(streamID, &c, 1);
		}

		(void)close(streamID);
	    } else {
		(void)fprintf(stdout, "*** couldn't touch %s: %s",
			       file, strerror(errno));
		(void)fflush(stdout);
	    }
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * Make_TimeStamp --
 *	Set the cmtime field of a parent node based on the mtime stamp in its
 *	child.
 *
 * Side Effects:
 *	The cmtime of the parent node will be changed if the mtime
 *	field of the child is greater than it.
 *-----------------------------------------------------------------------
 */
void
Make_TimeStamp(
    GNode *pgn, /* the current parent */
    GNode *cgn) /* the child we've just examined */
{
    if (is_strictly_before(pgn->cmtime, cgn->mtime))
	pgn->cmtime = cgn->mtime;
}

/*-
 *-----------------------------------------------------------------------
 * Make_HandleUse --
 *	Function called by Make_Run and SuffApplyTransform on the downward
 *	pass to handle .USE and transformation nodes. A callback function
 *	for Lst_ForEach, it implements the .USE and transformation
 *	functionality by copying the node's commands, type flags
 *	and children to the parent node. Should be called before the
 *	children are enqueued to be looked at by MakeAddChild.
 *
 *	A .USE node is much like an explicit transformation rule, except
 *	its commands are always added to the target node, even if the
 *	target already has commands.
 *
 * Side Effects:
 *	Children and commands may be added to the parent and the parent's
 *	type may be changed.
 *
 *-----------------------------------------------------------------------
 */
void
Make_HandleUse(
    GNode	*cgn,	/* The .USE node */
    GNode	*pgn)	/* The target of the .USE node */
{
    GNode	*gn;	/* A child of the .USE node */
    LstNode	ln;	/* An element in the children list */

    if (cgn->type & (OP_USE|OP_TRANSFORM)) {
	if ((cgn->type & OP_USE) || Lst_IsEmpty(&pgn->commands)) {
	    /* .USE or transformation and target has no commands -- append
	     * the child's commands to the parent.  */
	    Lst_Concat(&pgn->commands, &cgn->commands);
	}

	for (ln = Lst_First(&cgn->children); ln != NULL; ln = Lst_Adv(ln)) {
	    gn = (GNode *)Lst_Datum(ln);

	    if (Lst_AddNew(&pgn->children, gn)) {
		Lst_AtEnd(&gn->parents, pgn);
		pgn->unmade += 1;
	    }
	}

	pgn->type |= cgn->type & ~(OP_OPMASK|OP_USE|OP_TRANSFORM);

	/*
	 * This child node is now "made", so we decrement the count of
	 * unmade children in the parent... We also remove the child
	 * from the parent's list to accurately reflect the number of decent
	 * children the parent has. This is used by Make_Run to decide
	 * whether to queue the parent or examine its children...
	 */
	if (cgn->type & OP_USE) {
	    pgn->unmade--;
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * MakeAddAllSrc --
 *	Add a child's name to the ALLSRC and OODATE variables of the given
 *	node. Called from Make_DoAllVar via Lst_ForEach. A child is added only
 *	if it has not been given the .EXEC, .USE or .INVISIBLE attributes.
 *	.EXEC and .USE children are very rarely going to be files, so...
 *	A child is added to the OODATE variable if its modification time is
 *	later than that of its parent, as defined by Make, except if the
 *	parent is a .JOIN node. In that case, it is only added to the OODATE
 *	variable if it was actually made (since .JOIN nodes don't have
 *	modification times, the comparison is rather unfair...)..
 *
 * Side Effects:
 *	The ALLSRC variable for the given node is extended.
 *-----------------------------------------------------------------------
 */
static void
MakeAddAllSrc(
    void *cgnp, /* The child to add */
    void *pgnp) /* The parent to whose ALLSRC variable it should be */
			/* added */
{
    GNode	*cgn = (GNode *)cgnp;
    GNode	*pgn = (GNode *)pgnp;
    if ((cgn->type & (OP_EXEC|OP_USE|OP_INVISIBLE)) == 0) {
	const char *child;

	if (OP_NOP(cgn->type) ||
	    (child = Varq_Value(TARGET_INDEX, cgn)) == NULL) {
	    /*
	     * this node is only source; use the specific pathname for it
	     */
	    child = cgn->path != NULL ? cgn->path : cgn->name;
	}

	Varq_Append(ALLSRC_INDEX, child, pgn);
	if (pgn->type & OP_JOIN) {
	    if (cgn->made == MADE) {
		Varq_Append(OODATE_INDEX, child, pgn);
	    }
	} else if (is_strictly_before(pgn->mtime, cgn->mtime) ||
		   (!is_strictly_before(cgn->mtime, now) && cgn->made == MADE))
	{
	    /*
	     * It goes in the OODATE variable if the parent is younger than the
	     * child or if the child has been modified more recently than
	     * the start of the make. This is to keep pmake from getting
	     * confused if something else updates the parent after the
	     * make starts (shouldn't happen, I know, but sometimes it
	     * does). In such a case, if we've updated the kid, the parent
	     * is likely to have a modification time later than that of
	     * the kid and anything that relies on the OODATE variable will
	     * be hosed.
	     *
	     */
	    Varq_Append(OODATE_INDEX, child, pgn);
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * Make_DoAllVar --
 *	Set up the ALLSRC and OODATE variables. Sad to say, it must be
 *	done separately, rather than while traversing the graph. This is
 *	because Make defined OODATE to contain all sources whose modification
 *	times were later than that of the target, *not* those sources that
 *	were out-of-date. Since in both compatibility and native modes,
 *	the modification time of the parent isn't found until the child
 *	has been dealt with, we have to wait until now to fill in the
 *	variable. As for ALLSRC, the ordering is important and not
 *	guaranteed when in native mode, so it must be set here, too.
 *
 * Side Effects:
 *	The ALLSRC and OODATE variables of the given node is filled in.
 *	If the node is a .JOIN node, its TARGET variable will be set to
 *	match its ALLSRC variable.
 *-----------------------------------------------------------------------
 */
void
Make_DoAllVar(GNode *gn)
{
    Lst_ForEach(&gn->children, MakeAddAllSrc, gn);

    if (Varq_Value(OODATE_INDEX, gn) == NULL)
	Varq_Set(OODATE_INDEX, "", gn);
    if (Varq_Value(ALLSRC_INDEX, gn) == NULL)
	Varq_Set(ALLSRC_INDEX, "", gn);

    if (gn->type & OP_JOIN)
	Varq_Set(TARGET_INDEX, Varq_Value(ALLSRC_INDEX, gn), gn);
}

/* Wrapper to call Make_TimeStamp from a forEach loop.	*/
static void
MakeTimeStamp(
    void *pgn,	/* the current parent */
    void *cgn)	/* the child we've just examined */
{
    Make_TimeStamp((GNode *)pgn, (GNode *)cgn);
}

/*-
 *-----------------------------------------------------------------------
 * Make_OODate --
 *	See if a given node is out of date with respect to its sources.
 *	Used by Make_Run when deciding which nodes to place on the
 *	toBeMade queue initially and by Make_Update to screen out USE and
 *	EXEC nodes. In the latter case, however, any other sort of node
 *	must be considered out-of-date since at least one of its children
 *	will have been recreated.
 *
 * Results:
 *	true if the node is out of date. false otherwise.
 *
 * Side Effects:
 *	The mtime field of the node and the cmtime field of its parents
 *	will/may be changed.
 *-----------------------------------------------------------------------
 */
bool
Make_OODate(GNode *gn)	/* the node to check */
{
    bool	    oodate;

    /*
     * Certain types of targets needn't even be sought as their datedness
     * doesn't depend on their modification time...
     */
    if ((gn->type & (OP_JOIN|OP_USE|OP_EXEC)) == 0) {
	(void)Dir_MTime(gn);
	if (DEBUG(MAKE)) {
	    if (!is_out_of_date(gn->mtime)) {
		printf("modified %s...", time_to_string(gn->mtime));
	    } else {
		printf("non-existent...");
	    }
	}
    }

    /*
     * A target is remade in one of the following circumstances:
     *	its modification time is smaller than that of its youngest child
     *	    and it would actually be run (has commands or type OP_NOP)
     *	it's the object of a force operator
     *	it has no children, was on the lhs of an operator and doesn't exist
     *	    already.
     *
     * Libraries are only considered out-of-date if the archive module says
     * they are.
     *
     * These weird rules are brought to you by Backward-Compatibility and
     * the strange people who wrote 'Make'.
     */
    if (gn->type & OP_USE) {
	/*
	 * If the node is a USE node it is *never* out of date
	 * no matter *what*.
	 */
	if (DEBUG(MAKE)) {
	    printf(".USE node...");
	}
	oodate = false;
    } else if ((gn->type & OP_LIB) && Arch_IsLib(gn)) {
	if (DEBUG(MAKE)) {
	    printf("library...");
	}

	/*
	 * always out of date if no children and :: target
	 */

	oodate = Arch_LibOODate(gn) ||
	    (is_out_of_date(gn->cmtime) && (gn->type & OP_DOUBLEDEP));
    } else if (gn->type & OP_JOIN) {
	/*
	 * A target with the .JOIN attribute is only considered
	 * out-of-date if any of its children was out-of-date.
	 */
	if (DEBUG(MAKE)) {
	    printf(".JOIN node...");
	}
	oodate = gn->childMade;
    } else if (gn->type & (OP_FORCE|OP_EXEC|OP_PHONY)) {
	/*
	 * A node which is the object of the force (!) operator or which has
	 * the .EXEC attribute is always considered out-of-date.
	 */
	if (DEBUG(MAKE)) {
	    if (gn->type & OP_FORCE) {
		printf("! operator...");
	    } else if (gn->type & OP_PHONY) {
		printf(".PHONY node...");
	    } else {
		printf(".EXEC node...");
	    }
	}
	oodate = true;
    } else if (is_strictly_before(gn->mtime, gn->cmtime) ||
	       (is_out_of_date(gn->cmtime) &&
		(is_out_of_date(gn->mtime) || (gn->type & OP_DOUBLEDEP))))
    {
	/*
	 * A node whose modification time is less than that of its
	 * youngest child or that has no children (cmtime == OUT_OF_DATE) and
	 * either doesn't exist (mtime == OUT_OF_DATE) or was the object of a
	 * :: operator is out-of-date. Why? Because that's the way Make does
	 * it.
	 */
	if (DEBUG(MAKE)) {
	    if (is_strictly_before(gn->mtime, gn->cmtime)) {
		printf("modified before source...");
	    } else if (is_out_of_date(gn->mtime)) {
		printf("non-existent and no sources...");
	    } else {
		printf(":: operator and no sources...");
	    }
	}
	oodate = true;
    } else {
#if 0
	/* WHY? */
	if (DEBUG(MAKE)) {
	    printf("source %smade...", gn->childMade ? "" : "not ");
	}
	oodate = gn->childMade;
#else
	oodate = false;
#endif /* 0 */
    }

    /*
     * If the target isn't out-of-date, the parents need to know its
     * modification time. Note that targets that appear to be out-of-date
     * but aren't, because they have no commands and aren't of type OP_NOP,
     * have their mtime stay below their children's mtime to keep parents from
     * thinking they're out-of-date.
     */
    if (!oodate)
	Lst_ForEach(&gn->parents, MakeTimeStamp, gn);

    return oodate;
}

