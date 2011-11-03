/*	$OpenBSD: make.c,v 1.62 2011/11/03 20:55:22 schwarze Exp $	*/
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ohash.h>
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
#include "targequiv.h"
#include "garray.h"
#include "memory.h"

/* what gets added each time. Kept as one static array so that it doesn't
 * get resized every time.
 */
static struct growableArray examine;
/* The current fringe of the graph. These are nodes which await examination by
 * MakeOODate. It is added to by Make_Update and subtracted from by
 * MakeStartJobs */
static struct growableArray toBeMade;	

static struct ohash targets;	/* stuff we must build */

static void MakeAddChild(void *, void *);
static void MakeHandleUse(void *, void *);
static bool MakeStartJobs(void);
static void MakePrintStatus(void *, void *);
static bool try_to_make_node(GNode *);
static void add_targets_to_make(Lst);

static bool has_unmade_predecessor(GNode *);
static void requeue_successors(GNode *);
static void random_setup(void);

static bool randomize_queue;
long random_delay = 0;

bool 
no_jobs_left()
{
	return Array_IsEmpty(&toBeMade);
}

static void
random_setup()
{
	randomize_queue = Var_Definedi("RANDOM_ORDER", NULL);

	if (Var_Definedi("RANDOM_DELAY", NULL))
		random_delay = strtonum(Var_Value("RANDOM_DELAY"), 0, 1000, 
		    NULL) * 1000000;

	if (randomize_queue || random_delay) {
		unsigned int random_seed;
		char *t;
		
		t = Var_Value("RANDOM_SEED");
		if (t != NULL)
			random_seed = strtonum(t, 0, UINT_MAX, NULL);
		else
			random_seed = time(NULL);
		fprintf(stderr, "RANDOM_SEED=%u\n", random_seed);
		srandom(random_seed);
	}
}

static void
randomize_garray(struct growableArray *g)
{
	/* This is a fairly standard algorithm to randomize an array. */
	unsigned int i, v;
	GNode *e;

	for (i = g->n; i > 0; i--) {
		v = random() % i;
		if (v == i-1)
			continue;
		else {
			e = g->a[i-1];
			g->a[i-1] = g->a[v];
			g->a[v] = e;
		}
	}
}

static bool
has_unmade_predecessor(GNode *gn)
{
	LstNode ln;

	if (Lst_IsEmpty(&gn->preds))
		return false;


	for (ln = Lst_First(&gn->preds); ln != NULL; ln = Lst_Adv(ln)) {
		GNode	*pgn = (GNode *)Lst_Datum(ln);

		if (pgn->must_make && pgn->built_status == UNKNOWN) {
			if (DEBUG(MAKE))
				printf("predecessor %s not made yet.\n", 
				    pgn->name);
			return true;
		}
	}
	return false;
}

static void
requeue_successors(GNode *gn)
{
	LstNode ln;
	/* Deal with successor nodes. If any is marked for making and has an
	 * unmade count of 0, has not been made and isn't in the examination
	 * queue, it means we need to place it in the queue as it restrained
	 * itself before.	*/
	for (ln = Lst_First(&gn->successors); ln != NULL; ln = Lst_Adv(ln)) {
		GNode	*succ = (GNode *)Lst_Datum(ln);

		if (succ->must_make && succ->unmade == 0 
		    && succ->built_status == UNKNOWN)
			Array_PushNew(&toBeMade, succ);
	}
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
 *-----------------------------------------------------------------------
 */
void
Make_Update(GNode *cgn)	/* the child node */
{
	GNode	*pgn;	/* the parent node */
	LstNode	ln;	/* Element in parents list */

	/*
	 * If the child was actually made, see what its modification time is
	 * now -- some rules won't actually update the file. If the file still
	 * doesn't exist, make its mtime now.
	 */
	if (cgn->built_status != UPTODATE) {
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
			ts_set_from_now(cgn->mtime);
		if (DEBUG(MAKE))
			printf("update time: %s\n", time_to_string(cgn->mtime));
	}

	/* SIB: this is where I should mark the build as finished */
	cgn->build_lock = false;
	for (ln = Lst_First(&cgn->parents); ln != NULL; ln = Lst_Adv(ln)) {
		pgn = (GNode *)Lst_Datum(ln);
		/* SIB: there should be a siblings loop there */
		pgn->unmade--;
		if (pgn->must_make) {
			if (DEBUG(MAKE))
				printf("%s--=%d ", 
				    pgn->name, pgn->unmade);

			if ( ! (cgn->type & (OP_EXEC|OP_USE))) {
				if (cgn->built_status == MADE) {
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
				if (DEBUG(MAKE))
					printf("QUEUING ");
				Array_Push(&toBeMade, pgn);
			} else if (pgn->unmade < 0) {
				Error("Child %s discovered graph cycles through %s", cgn->name, pgn->name);
			}
		}
	}
	if (DEBUG(MAKE))
		printf("\n");
	requeue_successors(cgn);
}

static bool
try_to_make_node(GNode *gn)
{
	if (DEBUG(MAKE))
		printf("Examining %s...", gn->name);
		
	if (gn->unmade != 0) {
		if (DEBUG(MAKE))
			printf(" Requeuing (%d)\n", gn->unmade);
		add_targets_to_make(&gn->children);
		Array_Push(&toBeMade, gn);
		return false;
	}
	if (has_been_built(gn)) {
		if (DEBUG(MAKE))
			printf(" already made\n");
			return false;
	}
	if (has_unmade_predecessor(gn)) {
		if (DEBUG(MAKE))
			printf(" Dropping for now\n");
		return false;
	}

	/* SIB: this is where there should be a siblings loop */
	Suff_FindDeps(gn);
	if (gn->unmade != 0) {
		if (DEBUG(MAKE))
			printf(" Requeuing (after deps: %d)\n", gn->unmade);
		add_targets_to_make(&gn->children);
		return false;
	}
	if (Make_OODate(gn)) {
		/* SIB: if a sibling is getting built, I don't build it right now */
		if (DEBUG(MAKE))
			printf("out-of-date\n");
		if (queryFlag)
			return true;
		/* SIB: this is where commands should get prepared */
		Make_DoAllVar(gn);
		/* SIB: this is where I should make the gn as `being built */
		gn->build_lock = true;
		Job_Make(gn);
	} else {
		if (DEBUG(MAKE))
			printf("up-to-date\n");
		gn->built_status = UPTODATE;
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
	return false;
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

	while (can_start_job() && (gn = Array_Pop(&toBeMade)) != NULL) {
		if (try_to_make_node(gn))
			return true;
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
	if (gn->built_status == UPTODATE) {
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
			if (gn->built_status == CYCLE) {
				Error("Graph cycles through `%s'", gn->name);
				gn->built_status = ENDCYCLE;
				Lst_ForEach(&gn->children, MakePrintStatus, &t);
				gn->built_status = UNKNOWN;
			} else if (gn->built_status != ENDCYCLE) {
				gn->built_status = CYCLE;
				Lst_ForEach(&gn->children, MakePrintStatus, &t);
			}
		} else {
			printf("`%s' not remade because of errors.\n",
			    gn->name);
		}
	}
}


static void
MakeAddChild(void *to_addp, void *ap)
{
	GNode *gn = (GNode *)to_addp;

	if (!gn->must_make && !(gn->type & OP_USE))
		Array_Push((struct growableArray *)ap, gn);
}

static void
MakeHandleUse(void *cgnp, void *pgnp)
{
	GNode *cgn = (GNode *)cgnp;
	GNode *pgn = (GNode *)pgnp;

	if (cgn->type & OP_USE)
		Make_HandleUse(cgn, pgn);
}

/* Add stuff to the toBeMade queue. we try to sort things so that stuff 
 * that can be done directly is done right away.  This won't be perfect,
 * since some dependencies are only discovered later (e.g., SuffFindDeps).
 */
static void
add_targets_to_make(Lst todo)
{
	GNode *gn;

	unsigned int slot;

	AppendList2Array(todo, &examine);

	while ((gn = Array_Pop(&examine)) != NULL) {
		if (gn->must_make) 	/* already known */
			continue;
		gn->must_make = true;

		slot = ohash_qlookup(&targets, gn->name);
		if (!ohash_find(&targets, slot))
			ohash_insert(&targets, slot, gn);


		look_harder_for_target(gn);
		kludge_look_harder_for_target(gn);
		/*
		 * Apply any .USE rules before looking for implicit
		 * dependencies to make sure everything that should have
		 * commands has commands ...
		 */
		Lst_ForEach(&gn->children, MakeHandleUse, gn);
		expand_all_children(gn);

		if (gn->unmade != 0) {
			if (DEBUG(MAKE))
				printf("%s: not queuing (%d unmade children)\n",
				    gn->name, gn->unmade);
			Lst_ForEach(&gn->children, MakeAddChild,
			    &examine);
		} else {
			if (DEBUG(MAKE))
				printf("%s: queuing\n", gn->name);
			Array_Push(&toBeMade, gn);
		}
	}
	if (randomize_queue)
		randomize_garray(&toBeMade);
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
 *	The must_make field of all nodes involved in the creation of the given
 *	targets is set to 1. The toBeMade list is set to contain all the
 *	'leaves' of these subgraphs.
 *-----------------------------------------------------------------------
 */
bool
Make_Run(Lst targs)		/* the initial list of targets */
{
	int errors;	/* Number of errors the Job module reports */
	GNode *gn;
	unsigned int i;
	bool cycle;

	/* wild guess at initial sizes */
	Array_Init(&toBeMade, 500);
	Array_Init(&examine, 150);
	ohash_init(&targets, 10, &gnode_info);
	if (DEBUG(PARALLEL))
		random_setup();

	add_targets_to_make(targs);
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
		handle_running_jobs();
		(void)MakeStartJobs();
	}

	errors = Job_Finish();
	cycle = false;

	for (gn = ohash_first(&targets, &i); gn != NULL; 
	    gn = ohash_next(&targets, &i)) {
	    	if (has_been_built(gn))
			continue;
		cycle = true;
		errors++;
	    	printf("Error: target %s unaccounted for (%s)\n", 
		    gn->name, status_to_string(gn));
	}
	/*
	 * Print the final status of each target. E.g. if it wasn't made
	 * because some inferior reported an error.
	 */
	Lst_ForEach(targs, MakePrintStatus, &cycle);
	if (errors)
		Fatal("Errors while building");

	return true;
}
