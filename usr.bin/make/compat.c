/*	$OpenBSD: compat.c,v 1.94 2023/09/04 11:35:11 espie Exp $	*/
/*	$NetBSD: compat.c,v 1.14 1996/11/06 17:59:01 christos Exp $	*/

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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "defines.h"
#include "dir.h"
#include "engine.h"
#include "job.h"
#include "compat.h"
#include "suff.h"
#include "var.h"
#include "targ.h"
#include "targequiv.h"
#include "error.h"
#include "extern.h"
#include "gnode.h"
#include "timestamp.h"
#include "lst.h"

static void CompatMake(void *, void *);

/*-
 *-----------------------------------------------------------------------
 * CompatMake --
 *	Make a target.
 *
 * Side Effects:
 *	If an error is detected and not being ignored, the process exits.
 *-----------------------------------------------------------------------
 */
static void
CompatMake(void *gnp,	/* The node to make */
    void *pgnp)		/* Parent to abort if necessary */
{
	GNode *gn = gnp;
	GNode *pgn = pgnp;

	GNode *sib;
	bool cmdsOk;

	if (DEBUG(MAKE))
		printf("CompatMake(%s, %s)\n", pgn ? pgn->name : "NULL",
		    gn->name);

	/* XXX some loops are not loops, people write dependencies
	 * between siblings to make sure they get built.
	 * Also, we don't recognize direct loops.
	 */
	if (gn == pgn)
		return;
	/* handle .USE right away */
	if (gn->type & OP_USE) {
		Make_HandleUse(gn, pgn);
		return;
	}

	look_harder_for_target(gn);

	if (pgn != NULL && is_sibling(gn, pgn))
		return;

	if (pgn == NULL)
		pgn = gn;

	switch(gn->built_status) {
	case UNKNOWN: 
		/* First mark ourselves to be built, then apply whatever
		 * transformations the suffix module thinks are necessary.
		 * Once that's done, we can descend and make all our children.
		 * If any of them has an error but the -k flag was given,
		 * we will abort. */
		gn->must_make = true;
		gn->built_status = BUILDING;
		/* note that, in case we have siblings, we only check all
		 * children for all siblings, but we don't try to apply
		 * any other rule.
		 */
		sib = gn;
		do {
			Suff_FindDeps(sib);
			Lst_ForEach(&sib->children, CompatMake, gn);
			sib = sib->sibling;
		} while (sib != gn);

		if (gn->built_status == ABORTED) {
			Error("Build for %s aborted", gn->name);
			pgn->built_status = ABORTED;
			return;
		}

		/* All the children built ok. Now youngest points to
		 * the newest child, we need to find out
		 * if we exist and when we were modified last. The criteria
		 * for datedness are defined by the Make_OODate function.  */
		if (DEBUG(MAKE))
			printf("Examining %s...", gn->name);
		if (!Make_OODate(gn)) {
			gn->built_status = UPTODATE;
			if (DEBUG(MAKE))
				printf("up-to-date.\n");
			return;
		} else if (DEBUG(MAKE))
			printf("out-of-date.\n");

		/* If the user is just seeing if something is out-of-date,
		 * exit now to tell him/her "yes".  */
		if (queryFlag)
			exit(1);

		/* normally, we run the job, but if we can't find any
		 * commands, we defer to siblings instead.
		 */
		sib = gn;
		do {
			/* We need to be rebuilt. We also have to make sure
			 * we've got a $?  variable. To be nice, we also define
			 * the $> variable using Make_DoAllVar().
			 */
			Make_DoAllVar(sib);
			cmdsOk = node_find_valid_commands(sib);
			if (cmdsOk || (gn->type & OP_OPTIONAL))
				break;

			sib = sib->sibling;
		} while (sib != gn);

		if (cmdsOk) {
			/* Our commands are ok, but we still have to worry
			 * about the -t flag...	*/
			if (!touchFlag)
				run_gnode(sib);
			else {
				Job_Touch(sib);
				if (gn != sib)
					Job_Touch(gn);
			}
		} else {
			node_failure(gn);
			sib->built_status = ERROR;
		}

		/* copy over what we just did */
		gn->built_status = sib->built_status;

		if (gn->built_status == REBUILT) {
			/* If the node was built successfully, 
			 * update its modification time and timestamp all
			 * its parents.
			 * This is to keep its state from affecting that of
			 * its parent.  */
			/* This is what Make does and it's actually a good
			 * thing, as it allows rules like
			 *
			 *	cmp -s y.tab.h parse.h || cp y.tab.h parse.h
			 *
			 * to function as intended. Unfortunately, thanks to
			 * the stateless nature of NFS (and the speed of
			 * this program), there are times when the
			 * modification time of a file created on a remote
			 * machine will not be modified before the stat()
			 * implied by the Dir_MTime occurs, thus leading us
			 * to believe that the file is unchanged, wreaking
			 * havoc with files that depend on this one.
			 */
			if (noExecute || is_out_of_date(Dir_MTime(gn)))
				clock_gettime(CLOCK_REALTIME, &gn->mtime);
			if (is_strictly_before(gn->mtime, gn->youngest->mtime))
				gn->mtime = gn->youngest->mtime;
			if (sib != gn) {
				if (noExecute || is_out_of_date(Dir_MTime(sib)))
					clock_gettime(CLOCK_REALTIME, 
					    &sib->mtime);
				if (is_strictly_before(sib->mtime, 
				    sib->youngest->mtime))
					sib->mtime = sib->youngest->mtime;
			}
			if (DEBUG(MAKE))
				printf("update time: %s\n",
				    time_to_string(&gn->mtime));
			pgn->child_rebuilt = true;
			Make_TimeStamp(pgn, gn);
		} else if (keepgoing)
			pgn->built_status = ABORTED;
		else {
			print_errors();
			exit(1);
		}
		break;
	case ERROR:
		/* Already had an error when making this beastie. Tell the
		 * parent to abort.  */
		pgn->built_status = ABORTED;
		break;
	case BUILDING:
		Error("Graph cycles through %s", gn->name);
		gn->built_status = ERROR;
		pgn->built_status = ABORTED;
		break;
	case REBUILT:
		pgn->child_rebuilt = true;
		Make_TimeStamp(pgn, gn);
		break;
	case UPTODATE:
		Make_TimeStamp(pgn, gn);
		break;
	default:
		break;
	}
}

void
Compat_Init()
{
}

void
Compat_Update(GNode *gn)
{
}

void
Compat_Run(Lst targs, bool *has_errors, bool *out_of_date)
{
	GNode	  *gn = NULL;	/* Current root target */

	/* For each entry in the list of targets to create, call CompatMake on
	 * it to create the thing. CompatMake will leave the 'built_status'
	 * field of gn in one of several states:
	 *	    UPTODATE	    gn was already up-to-date
	 *	    REBUILT	    gn was recreated successfully
	 *	    ERROR	    An error occurred while gn was being
	 *                          created
	 *	    ABORTED	    gn was not built because one of its
	 *                          dependencies could not be built due 
	 *		      	    to errors.  */
	while ((gn = Lst_DeQueue(targs)) != NULL) {
		CompatMake(gn, NULL);

		if (gn->built_status == UPTODATE)
			printf("`%s' is up to date.\n", gn->name);
		else if (gn->built_status == ABORTED) {
			printf("`%s' not remade because of errors.\n",
			    gn->name);
			*out_of_date = true;
			*has_errors = true;
		} else {
			*out_of_date = true;
		}
	}

}
