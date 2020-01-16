#ifndef _JOB_H_
#define _JOB_H_

/*	$OpenBSD: job.h,v 1.37 2020/01/16 16:07:18 espie Exp $	*/
/*	$NetBSD: job.h,v 1.5 1996/11/06 17:59:10 christos Exp $ */

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

/*-
 * job.h --
 *	Definitions pertaining to the running of jobs.
 */

/* Job_Make(gn);
 *	register a new job running commands associated with building gn.
 */
extern void Job_Make(GNode *);
/* Job_Init(maxproc);
 *	setup job handling framework
 */
extern void Job_Init(int);

/* save signal mask at start */
extern void Sigset_Init();

/* interface with the normal build in make.c */
/* okay = can_start_job();
 *	can we run new jobs right now ?
 */
extern bool can_start_job(void);

/* finished = Job_Empty();
 *	wait until all jobs are finished after we build everything.
 */
extern bool Job_Empty(void);

extern void Job_Wait(void);
extern void Job_AbortAll(void);
extern void print_errors(void);

/* handle_running_jobs();
 *	wait until something happens, like a job finishing running a command
 *	or a signal coming in.
 */
extern void handle_running_jobs(void);
/* loop_handle_running_jobs();
 *	handle running jobs until they're finished.
 */
extern void loop_handle_running_jobs(void);
extern void reset_signal_mask(void);

/* handle_all_signals();
 *	if a signal was received, react accordingly.
 *	By displaying STATUS info, or by aborting running jobs for a fatal
 *	signals. Relies on Job_Init() for setting up handlers.
 */
extern void handle_all_signals(void);

extern void determine_expensive_job(Job *);
extern Job *runningJobs, *errorJobs, *availableJobs;
extern void debug_job_printf(const char *, ...);
extern void handle_one_job(Job *);
extern int check_dying_signal(void);

extern const char *basedirectory;

extern bool	sequential;	/* True if we are running one single-job */

#endif /* _JOB_H_ */
