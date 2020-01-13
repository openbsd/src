#ifndef ENGINE_H
#define ENGINE_H
/*	$OpenBSD: engine.h,v 1.17 2020/01/13 15:12:58 espie Exp $	*/

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
 *
 *	from: @(#)job.h 8.1 (Berkeley) 6/6/93
 */

/* ok = node_find_valid_commands(node);
 *	verify the integrity of a node's commands, pulling stuff off
 * 	.DEFAULT and other places if necessary.
 */
extern bool node_find_valid_commands(GNode *);

/* node_failure(gn); 
 *	indicate we don't know how to make gn.
 *	may continue with -k or if node was optional.
 */
extern void node_failure(GNode *);

/* Job_Touch(node);
 *	touch the path corresponding to a node or update the corresponding
 *	archive object.
 */
extern void Job_Touch(GNode *);

/* Make_TimeStamp(parent, child);
 *	ensure parent is at least as recent as child.
 */
extern void Make_TimeStamp(GNode *, GNode *);

/* Make_HandleUse(user_node, usee_node);
 *	let user_node inherit the commands from usee_node
 */
extern void Make_HandleUse(GNode *, GNode *);

/* old = Make_OODate(node);
 *	check if a given node is out-of-date.
 */
extern bool Make_OODate(GNode *);

/* Make_DoAllVar(node);
 *	fill all dynamic variables for a node.
 */
extern void Make_DoAllVar(GNode *);

/* status = run_gnode(gn):
 *	fully run all commands of a node for compat mode.
 */
extern int run_gnode(GNode *);

/*-
 * Job Table definitions.
 *
 * Each job has several things associated with it:
 *	1) The process id of the child shell
 *	2) The graph node describing the target of this job
 *	3) State associated to latest command run
 *	5) A word of flags which determine how the module handles errors,
 *	   echoing, etc. for the job
 *
 * The job "table" is kept as a linked Lst in 'jobs', with the number of
 * active jobs maintained in the 'nJobs' variable. At no time will this
 * exceed the value of 'maxJobs', initialized by the Job_Init function.
 *
 * When a job is finished, the Make_Update function is called on each of the
 * parents of the node which was just rebuilt. This takes care of the upward
 * traversal of the dependency graph.
 */
struct Job_ {
	struct Job_ 	*next;		/* singly linked list */
	pid_t		pid;		/* Current command process id */
	Location	*location;
	int 		code;		/* exit status or signal code */
	unsigned short	exit_type;	/* last child exit or signal */
#define JOB_EXIT_OKAY 	0
#define JOB_EXIT_BAD 	1
#define JOB_SIGNALED 	2
	unsigned short	flags;
#define JOB_SILENT		0x001	/* Command was silent */
#define JOB_IS_EXPENSIVE 	0x002
#define JOB_LOST		0x004	/* sent signal to non-existing pid ? */
#define JOB_ERRCHECK		0x008	/* command wants errcheck */
#define JOB_KEEPERROR		0x010	/* should place job on error list */
	LstNode		next_cmd;	/* Next command to run */
	char		*cmd;		/* Last command run */
	GNode		*node;	    	/* Target of this job */
};

/* Continuation-style running commands for the parallel engine */

/* job_attach_node(job, node):
 *	attach a job to an allocated node, to be able to run commands
 */
extern void job_attach_node(Job *, GNode *);

/* finished = job_run_next(job):
 *	run next command for a job attached to a node.
 *	return true when job is finished.
 */
extern bool job_run_next(Job *);

/* handle_job_status(job, waitstatus):
 *	process a wait return value corresponding to a job, display
 *	messages and set job status accordingly.
 */
extern void handle_job_status(Job *, int);

#endif
