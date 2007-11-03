/*	$OpenPackages$ */
/*	$OpenBSD: make.c,v 1.45 2007/11/03 11:45:52 espie Exp $	*/
/*	$NetBSD: make.c,v 1.10 1996/11/06 17:59:15 christos Exp $	*/

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
 * make.c --
 *	The functions which perform the examination of targets and
 *	their suitability for creation
 *
 * Interface:
 *	Make_Run		Initialize things for the module and recreate
 *				whatever needs recreating. Returns true if
 *				work was (or would have been) done and
 *				false
 *				otherwise.
 *
 *	Make_Update		Update all parents of a given child. Performs
 *				various bookkeeping chores like the updating
 *				of the cmtime field of the parent, filling
 *				of the IMPSRC context variable, etc. It will
 *				place the parent on the toBeMade queue if it
 *				should be.
 *
 */

#include <limits.h>
#include <stdio.h>
#include <signal.h>
#include "config.h"
#include "defines.h"
#include "dir.h"
#include "job.h"
#include "suff.h"
#include "var.h"
#include "error.h"
#include "make.h"
#include "gnode.h"
#include "extern.h"
#include "timestamp.h"
#include "engine.h"
#include "lst.h"
#include "targ.h"

static LIST	toBeMade;	/* The current fringe of the graph. These
				 * are nodes which await examination by
				 * MakeOODate. It is added to by
				 * Make_Update and subtracted from by
				 * MakeStartJobs */
static int	numNodes;	/* Number of nodes to be processed. If this
				 * is non-zero when Job_Empty() returns
				 * true, there's a cycle in the graph */

static void MakeAddChild(void *, void *);
static void MakeHandleUse(void *, void *);
static bool MakeStartJobs(void);
static void MakePrintStatus(void *, void *);

/*-
 *-----------------------------------------------------------------------
 * MakeAddChild  --
 *	Function used by Make_Run to add a child to the list l.
 *	It will only add the child if its make field is false.
 *
 * Side Effects:
 *	The given list is extended
 *-----------------------------------------------------------------------
 */
static void
MakeAddChild(void *to_addp, void *lp)
{
	GNode	   *to_add = (GNode *)to_addp;
	Lst 	   l = (Lst)lp;

	if (!to_add->make && !(to_add->type & OP_USE))
		Lst_EnQueue(l, to_add);
}

static void
MakeHandleUse(void *pgn, void *cgn)
{
    Make_HandleUse((GNode *)pgn, (GNode *)cgn);
}

/*-
 *-----------------------------------------------------------------------
 * Make_Update	--
 *	Perform update on the parents of a node. Used by JobFinish once
 *	a node has been dealt with and by MakeStartJobs if it finds an
 *	up-to-date node.
 *
 * Results:
 *	Always returns 0
 *
 * Side Effects:
 *	The unmade field of pgn is decremented and pgn may be placed on
 *	the toBeMade queue if this field becomes 0.
 *
 *	If the child was made, the parent's childMade field will be set true
 *	and its cmtime set to now.
 *
 *	If the child wasn't made, the cmtime field of the parent will be
 *	altered if the child's mtime is big enough.
 *
 *	Finally, if the child is the implied source for the parent, the
 *	parent's IMPSRC variable is set appropriately.
 *
 *-----------------------------------------------------------------------
 */
void
Make_Update(GNode *cgn)	/* the child node */
{
	GNode	*pgn;	/* the parent node */
	char	*cname; /* the child's name */
	LstNode	ln;	/* Element in parents and iParents lists */

	cname = Varq_Value(TARGET_INDEX, cgn);

	/*
	 * If the child was actually made, see what its modification time is
	 * now -- some rules won't actually update the file. If the file still
	 * doesn't exist, make its mtime now.
	 */
	if (cgn->made != UPTODATE) {
		/*
		 * This is what Make does and it's actually a good thing, as it
		 * allows rules like
		 *
		 *	cmp -s y.tab.h parse.h || cp y.tab.h parse.h
		 *
		 * to function as intended. Unfortunately, thanks to the
		 * stateless nature of NFS, there are times when the
		 * modification time of a file created on a remote machine
		 * will not be modified before the local stat() implied by
		 * the Dir_MTime occurs, thus leading us to believe that the
		 * file is unchanged, wreaking havoc with files that depend
		 * on this one.
		 */
		if (noExecute || is_out_of_date(Dir_MTime(cgn)))
			cgn->mtime = now;
		if (DEBUG(MAKE))
			printf("update time: %s\n", time_to_string(cgn->mtime));
	}

	for (ln = Lst_First(&cgn->parents); ln != NULL; ln = Lst_Adv(ln)) {
		pgn = (GNode *)Lst_Datum(ln);
		if (pgn->make) {
			pgn->unmade--;

			if ( ! (cgn->type & (OP_EXEC|OP_USE))) {
				if (cgn->made == MADE) {
					pgn->childMade = true;
					if (is_strictly_before(pgn->cmtime,
					    cgn->mtime))
						pgn->cmtime = cgn->mtime;
				} else {
					(void)Make_TimeStamp(pgn, cgn);
				}
			}
			if (pgn->unmade == 0) {
				/*
				 * Queue the node up -- any unmade
				 * predecessors will be dealt with in
				 * MakeStartJobs.
				 */
				Lst_EnQueue(&toBeMade, pgn);
			} else if (pgn->unmade < 0) {
				Error("Graph cycles through %s", pgn->name);
			}
		}
	}
	/* Deal with successor nodes. If any is marked for making and has an
	 * unmade count of 0, has not been made and isn't in the examination
	 * queue, it means we need to place it in the queue as it restrained
	 * itself before.	*/
	for (ln = Lst_First(&cgn->successors); ln != NULL; ln = Lst_Adv(ln)) {
		GNode	*succ = (GNode *)Lst_Datum(ln);

		if (succ->make && succ->unmade == 0 && succ->made == UNMADE)
			(void)Lst_QueueNew(&toBeMade, succ);
	}

	/* Set the .IMPSRC variables for all the implied parents
	 * of this node.  */
	for (ln = Lst_First(&cgn->iParents); ln != NULL; ln = Lst_Adv(ln)) {
		pgn = (GNode *)Lst_Datum(ln);
		if (pgn->make)
			Varq_Set(IMPSRC_INDEX, cname, pgn);
	}

}

/*
 *-----------------------------------------------------------------------
 * MakeStartJobs --
 *	Start as many jobs as possible.
 *
 * Results:
 *	If the query flag was given to pmake, no job will be started,
 *	but as soon as an out-of-date target is found, this function
 *	returns true. At all other times, this function returns false.
 *
 * Side Effects:
 *	Nodes are removed from the toBeMade queue and job table slots
 *	are filled.
 *-----------------------------------------------------------------------
 */
static bool
MakeStartJobs(void)
{
	GNode	*gn;

	while (!Job_Full() && (gn = (GNode *)Lst_DeQueue(&toBeMade)) != NULL) {
		if (DEBUG(MAKE))
			printf("Examining %s...", gn->name);
		/*
		 * Make sure any and all predecessors that are going to be made,
		 * have been.
		 */
		if (!Lst_IsEmpty(&gn->preds)) {
			LstNode ln;

			for (ln = Lst_First(&gn->preds); ln != NULL;
			    ln = Lst_Adv(ln)){
				GNode	*pgn = (GNode *)Lst_Datum(ln);

				if (pgn->make && pgn->made == UNMADE) {
					if (DEBUG(MAKE))
					    printf("predecessor %s not made yet.\n", pgn->name);
					break;
				}
			}
			/*
			 * If ln isn't NULL, there's a predecessor as yet
			 * unmade, so we just drop this node on the floor. When
			 * the node in question has been made, it will notice
			 * this node as being ready to make but as yet unmade
			 * and will place the node on the queue.
			 */
			if (ln != NULL)
				continue;
		}

		numNodes--;
		if (Make_OODate(gn)) {
			if (DEBUG(MAKE))
				printf("out-of-date\n");
			if (queryFlag)
				return true;
			Make_DoAllVar(gn);
			Job_Make(gn);
		} else {
			if (DEBUG(MAKE))
				printf("up-to-date\n");
			gn->made = UPTODATE;
			if (gn->type & OP_JOIN) {
				/*
				 * Even for an up-to-date .JOIN node, we need it
				 * to have its context variables so references
				 * to it get the correct value for .TARGET when
				 * building up the context variables of its
				 * parent(s)...
				 */
				Make_DoAllVar(gn);
			}

			Make_Update(gn);
		}
	}
	return false;
}

/*-
 *-----------------------------------------------------------------------
 * MakePrintStatus --
 *	Print the status of a top-level node, viz. it being up-to-date
 *	already or not created due to an error in a lower level.
 *	Callback function for Make_Run via Lst_ForEach.
 *
 * Side Effects:
 *	A message may be printed.
 *-----------------------------------------------------------------------
 */
static void
MakePrintStatus(
    void *gnp,		    /* Node to examine */
    void *cyclep)	    /* True if gn->unmade being non-zero implies
			     * a cycle in the graph, not an error in an
			     * inferior */
{
	GNode	*gn = (GNode *)gnp;
	bool	cycle = *(bool *)cyclep;
	if (gn->made == UPTODATE) {
		printf("`%s' is up to date.\n", gn->name);
	} else if (gn->unmade != 0) {
		if (cycle) {
			bool t = true;
			/*
			 * If printing cycles and came to one that has unmade
			 * children, print out the cycle by recursing on its
			 * children. Note a cycle like:
			 *	a : b
			 *	b : c
			 *	c : b
			 * will cause this to erroneously complain about a
			 * being in the cycle, but this is a good approximation.
			 */
			if (gn->made == CYCLE) {
				Error("Graph cycles through `%s'", gn->name);
				gn->made = ENDCYCLE;
				Lst_ForEach(&gn->children, MakePrintStatus, &t);
				gn->made = UNMADE;
			} else if (gn->made != ENDCYCLE) {
				gn->made = CYCLE;
				Lst_ForEach(&gn->children, MakePrintStatus, &t);
			}
		} else {
			printf("`%s' not remade because of errors.\n",
			    gn->name);
		}
	}
}


/*-
 *-----------------------------------------------------------------------
 * Make_Run --
 *	Initialize the nodes to remake and the list of nodes which are
 *	ready to be made by doing a breadth-first traversal of the graph
 *	starting from the nodes in the given list. Once this traversal
 *	is finished, all the 'leaves' of the graph are in the toBeMade
 *	queue.
 *	Using this queue and the Job module, work back up the graph,
 *	calling on MakeStartJobs to keep the job table as full as
 *	possible.
 *
 * Results:
 *	true if work was done. false otherwise.
 *
 * Side Effects:
 *	The make field of all nodes involved in the creation of the given
 *	targets is set to 1. The toBeMade list is set to contain all the
 *	'leaves' of these subgraphs.
 *-----------------------------------------------------------------------
 */
bool
Make_Run(Lst targs)		/* the initial list of targets */
{
	GNode	    *gn;	/* a temporary pointer */
	LIST	    examine;	/* List of targets to examine */
	int 	    errors;	/* Number of errors the Job module reports */

	Static_Lst_Init(&toBeMade);

	Lst_Clone(&examine, targs, NOCOPY);
	numNodes = 0;

	/*
	 * Make an initial downward pass over the graph, marking nodes to be
	 * made as we go down. We call Suff_FindDeps to find where a node is and
	 * to get some children for it if it has none and also has no commands.
	 * If the node is a leaf, we stick it on the toBeMade queue to
	 * be looked at in a minute, otherwise we add its children to our queue
	 * and go on about our business.
	 */
	while ((gn = (GNode *)Lst_DeQueue(&examine)) != NULL) {
		if (!gn->make) {
			gn->make = true;
			numNodes++;

			look_harder_for_target(gn);
			/*
			 * Apply any .USE rules before looking for implicit
			 * dependencies to make sure everything that should have
			 * commands has commands ...
			 */
			Lst_ForEach(&gn->children, MakeHandleUse, gn);
			Suff_FindDeps(gn);

			if (gn->unmade != 0)
				Lst_ForEach(&gn->children, MakeAddChild,
				    &examine);
			else
				Lst_EnQueue(&toBeMade, gn);
		}
	}

	if (queryFlag) {
		/*
		 * We wouldn't do any work unless we could start some jobs in
		 * the next loop... (we won't actually start any, of course,
		 * this is just to see if any of the targets was out of date)
		 */
		return MakeStartJobs();
	} else {
		/*
		 * Initialization. At the moment, no jobs are running and until
		 * some get started, nothing will happen since the remaining
		 * upward traversal of the graph is performed by the routines
		 * in job.c upon the finishing of a job. So we fill the Job
		 * table as much as we can before going into our loop.
		 */
		(void)MakeStartJobs();
	}

	/*
	 * Main Loop: The idea here is that the ending of jobs will take
	 * care of the maintenance of data structures and the waiting for output
	 * will cause us to be idle most of the time while our children run as
	 * much as possible. Because the job table is kept as full as possible,
	 * the only time when it will be empty is when all the jobs which need
	 * running have been run, so that is the end condition of this loop.
	 * Note that the Job module will exit if there were any errors unless
	 * the keepgoing flag was given.
	 */
	while (!Job_Empty()) {
		Job_CatchOutput();
		Job_CatchChildren();
		(void)MakeStartJobs();
	}

	errors = Job_Finish();

	/*
	 * Print the final status of each target. E.g. if it wasn't made
	 * because some inferior reported an error.
	 */
	errors = errors == 0 && numNodes != 0;
	Lst_ForEach(targs, MakePrintStatus, &errors);

	return true;
}
