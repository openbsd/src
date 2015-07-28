/*	$OpenBSD: job.c,v 1.136 2015/07/28 14:22:26 espie Exp $	*/
/*	$NetBSD: job.c,v 1.16 1996/11/06 17:59:08 christos Exp $	*/

/*
 * Copyright (c) 2012 Marc Espie.
 *
 * Extensive code modifications for the OpenBSD project.
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

/*-
 * job.c --
 *	handle the creation etc. of our child processes.
 *
 * Interface:
 *	Job_Make		Start the creation of the given target.
 *
 *	Job_Init		Called to initialize this module. 
 *
 *	Job_Begin		execute commands attached to the .BEGIN target
 *				if any.
 *
 *	can_start_job		Return true if we can start job
 *
 *	Job_Empty		Return true if the job table is completely
 *				empty.
 *
 *	Job_Finish		Perform any final processing which needs doing.
 *				This includes the execution of any commands
 *				which have been/were attached to the .END
 *				target. 
 *
 *	Job_AbortAll		Abort all current jobs. It doesn't
 *				handle output or do anything for the jobs,
 *				just kills them. 
 *
 *	Job_Wait		Wait for all running jobs to finish.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "defines.h"
#include "job.h"
#include "engine.h"
#include "pathnames.h"
#include "var.h"
#include "targ.h"
#include "error.h"
#include "extern.h"
#include "lst.h"
#include "gnode.h"
#include "memory.h"
#include "make.h"
#include "buf.h"

static int	aborting = 0;	    /* why is the make aborting? */
#define ABORT_ERROR	1	    /* Because of an error */
#define ABORT_INTERRUPT 2	    /* Because it was interrupted */
#define ABORT_WAIT	3	    /* Waiting for jobs to finish */

static int	maxJobs;	/* The most children we can run at once */
static int	nJobs;		/* Number of jobs already allocated */
static bool	no_new_jobs;	/* Mark recursive shit so we shouldn't start
				 * something else at the same time
				 */
Job *runningJobs;		/* Jobs currently running a process */
Job *errorJobs;			/* Jobs in error at end */
static Job *heldJobs;		/* Jobs not running yet because of expensive */
static pid_t mypid;		/* Used for printing debugging messages */

static volatile sig_atomic_t got_fatal;

static volatile sig_atomic_t got_SIGINT, got_SIGHUP, got_SIGQUIT, got_SIGTERM, 
    got_SIGINFO;

static sigset_t sigset, emptyset;

static void handle_fatal_signal(int);
static void handle_siginfo(void);
static void postprocess_job(Job *, bool);
static Job *prepare_job(GNode *);
static void determine_job_next_step(Job *);
static void remove_job(Job *, bool);
static void may_continue_job(Job *);
static void continue_job(Job *);
static Job *reap_finished_job(pid_t);
static bool reap_jobs(void);

static void loop_handle_running_jobs(void);
static bool expensive_job(Job *);
static bool expensive_command(const char *);
static void setup_signal(int);
static void notice_signal(int);
static void setup_all_signals(void);
static const char *really_kill(Job *, int);
static void kill_with_doas_maybe(pid_t, int, const char *);
static void debug_kill_printf(const char *, ...);
static void debug_vprintf(const char *, va_list);
static void may_remove_target(Job *);
static const char *really_kill(Job *, int);
static void print_error(Job *);
static void internal_print_errors(void);

static int dying_signal = 0;

const char *	basedirectory = NULL;

static void 
kill_with_doas_maybe(pid_t pid, int signo, const char *p)
{
	char buf[32]; /* largely enough */
	int sudo;

	for (;*p != '\0'; p++) {
		if (*p == 's')
			sudo = 1;
		else if (*p == 'd')
			sudo = 0;
		else
			continue;
		if (sudo && p[1] != 'u' || !sudo && p[1] != 'o')
			continue;
		p++;
		if (sudo && p[1] != 'd' || !sudo && p[1] != 'a')
			continue;
		p++;
		if (sudo && p[1] != 'o' || !sudo && p[1] != 's')
			continue;
		snprintf(buf, sizeof buf, "%s -n /bin/kill -%d %ld", 
		    sudo ? "sudo" : "doas",
		    signo, (long)pid);
		debug_kill_printf("trying to kill with %s", buf);
		system(buf);
		return;
	}

}

static const char *
really_kill(Job *job, int signo)
{
	pid_t pid = job->pid;
	if (getpgid(pid) != getpgrp()) {
		if (killpg(pid, signo) == 0)
			return "group got signal";
		pid = -pid;
	} else {
		if (kill(pid, signo) == 0)
			return "process got signal";
	}
	if (errno == ESRCH) {
		job->flags |= JOB_LOST;
		return "not found";
	} else if (errno == EPERM) {
		kill_with_doas_maybe(pid, signo, job->cmd);
		return "";
	} else
		return "should not happen";
}

static void
may_remove_target(Job *j)
{
	int dying = check_dying_signal();

	if (dying && !noExecute && !Targ_Precious(j->node)) {
		const char *file = Var(TARGET_INDEX, j->node);
		int r = eunlink(file);

		if (DEBUG(JOB) && r == -1)
			fprintf(stderr, " *** would unlink %s\n", file);
		if (r != -1)
			fprintf(stderr, " *** %s removed\n", file);
	}
}

static void
buf_addcurdir(BUFFER *buf)
{
	const char *v = Var_Value(".CURDIR");
	if (basedirectory != NULL) {
		size_t len = strlen(basedirectory);
		if (strncmp(basedirectory, v, len) == 0 &&
		    v[len] == '/') {
			v += len+1;
		} else if (strcmp(basedirectory, v) == 0) {
			Buf_AddString(buf, ".");
			return;
		}
	}
	Buf_AddString(buf, v);
}

static const char *
shortened_curdir(void)
{
	static BUFFER buf;
	bool first = true;
	if (first) {
		Buf_Init(&buf, 0);
		buf_addcurdir(&buf);
		first = false;
	}
	return Buf_Retrieve(&buf);
}

static void 
quick_error(Job *j, int signo, bool first)
{
	if (first) {
		fprintf(stderr, "*** Signal SIG%s", sys_signame[signo]);
		fprintf(stderr, " in %s (", shortened_curdir());
	} else
		fprintf(stderr, " ");

	fprintf(stderr, "%s", j->node->name);
	free(j->cmd);
}

static void 
print_error(Job *j)
{
	static bool first = true;
	BUFFER buf;

	Buf_Init(&buf, 0);

	if (j->exit_type == JOB_EXIT_BAD)
		Buf_printf(&buf, "*** Error %d", j->code);
	else if (j->exit_type == JOB_SIGNALED) {
		if (j->code < NSIG)
			Buf_printf(&buf, "*** Signal SIG%s", 
			    sys_signame[j->code]);
		else
			Buf_printf(&buf, "*** unknown signal %d", j->code);
	} else
		Buf_printf(&buf, "*** Should not happen %d/%d", 
		    j->exit_type, j->code);
	if (DEBUG(KILL) && (j->flags & JOB_LOST))
		Buf_AddChar(&buf, '!');
	if (first) {
		Buf_AddString(&buf, " in ");
		buf_addcurdir(&buf);
		first = false;
	}
	Buf_printf(&buf, " (%s:%lu", j->location->fname, j->location->lineno);
	Buf_printf(&buf, " '%s'", j->node->name);
	if ((j->flags & (JOB_SILENT | JOB_IS_EXPENSIVE)) == JOB_SILENT
	    && Buf_Size(&buf) < 140-2) {
		size_t len = strlen(j->cmd);
		Buf_AddString(&buf, ": ");
		if (len + Buf_Size(&buf) < 140)
			Buf_AddString(&buf, j->cmd);
		else {
			Buf_AddChars(&buf, 140 - Buf_Size(&buf), j->cmd);
			Buf_AddString(&buf, "...");
		}
	}
	fprintf(stderr, "%s)\n", Buf_Retrieve(&buf));
	Buf_Destroy(&buf);
	free(j->cmd);
}
static void
quick_summary(int signo)
{
	Job *j, *k, *jnext;
	bool first = true;

	k = errorJobs;
	errorJobs = NULL;
	for (j = k; j != NULL; j = jnext) {
		jnext = j->next;
		if ((j->exit_type == JOB_EXIT_BAD && j->code == signo+128) ||
		    (j->exit_type == JOB_SIGNALED && j->code == signo)) {
			quick_error(j, signo, first);
			first = false;
		} else {
			j->next = errorJobs;
			errorJobs = j;
		}
	}
	if (!first)
		fprintf(stderr, ")\n");
}

static void
internal_print_errors()
{
	Job *j, *k, *jnext;
	int dying;

	if (!errorJobs)
		fprintf(stderr, "Stop in %s\n", shortened_curdir());

	for (j = errorJobs; j != NULL; j = j->next)
		may_remove_target(j);
	dying = check_dying_signal();
	if (dying)
		quick_summary(dying);
	while (errorJobs != NULL) {
		k = errorJobs;
		errorJobs = NULL;
		for (j = k; j != NULL; j = jnext) {
			jnext = j->next;
			if (j->location->fname == k->location->fname)
				print_error(j);
			else {
				j->next = errorJobs;
				errorJobs = j;
			}
		}
	}
}

void
print_errors(void)
{
	handle_all_signals();
	internal_print_errors();
}

static void
setup_signal(int sig)
{
	if (signal(sig, SIG_IGN) != SIG_IGN) {
		(void)signal(sig, notice_signal);
		sigaddset(&sigset, sig);
	}
}

static void
notice_signal(int sig)
{

	switch(sig) {
	case SIGINT:
		got_SIGINT++;
		got_fatal = 1;
		break;
	case SIGHUP:
		got_SIGHUP++;
		got_fatal = 1;
		break;
	case SIGQUIT:
		got_SIGQUIT++;
		got_fatal = 1;
		break;
	case SIGTERM:
		got_SIGTERM++;
		got_fatal = 1;
		break;
	case SIGINFO:
		got_SIGINFO++;
		break;
	case SIGCHLD:
		break;
	}
}

static void
setup_all_signals(void)
{
	sigemptyset(&sigset);
	sigemptyset(&emptyset);
	/*
	 * Catch the four signals that POSIX specifies if they aren't ignored.
	 * handle_signal will take care of calling JobInterrupt if appropriate.
	 */
	setup_signal(SIGINT);
	setup_signal(SIGHUP);
	setup_signal(SIGQUIT);
	setup_signal(SIGTERM);
	/* Display running jobs on SIGINFO */
	setup_signal(SIGINFO);
	/* Have to see SIGCHLD */
	setup_signal(SIGCHLD);
	got_fatal = 0;
}

static void 
handle_siginfo(void)
{
	static BUFFER buf;
	static size_t length = 0;

	Job *job;
	bool first = true;

	got_SIGINFO = 0;
	/* we have to store the info in a buffer, because status from all
	 * makes running would get intermixed otherwise
	 */

	if (length == 0) {
		Buf_Init(&buf, 0);
		Buf_printf(&buf, "%s in ", Var_Value("MAKE"));
		buf_addcurdir(&buf);
		Buf_AddString(&buf, ": ");
		length = Buf_Size(&buf);
	} else
		Buf_Truncate(&buf, length);

	for (job = runningJobs; job != NULL ; job = job->next) {
		if (!first)
			Buf_puts(&buf, ", ");
		first = false;
		Buf_puts(&buf, job->node->name);
	}
	Buf_puts(&buf, first ? "nothing running\n" : "\n");

	fputs(Buf_Retrieve(&buf), stderr);
}

int
check_dying_signal(void)
{
	sigset_t set;
	if (dying_signal)
		return dying_signal;
	sigpending(&set);
	if (got_SIGINT || sigismember(&set, SIGINT))
		return dying_signal = SIGINT;
	if (got_SIGHUP || sigismember(&set, SIGHUP))
		return dying_signal = SIGHUP;
	if (got_SIGQUIT || sigismember(&set, SIGQUIT))
		return dying_signal = SIGQUIT;
	if (got_SIGTERM || sigismember(&set, SIGTERM))
		return dying_signal = SIGTERM;
	return 0;
}

void
handle_all_signals(void)
{
	if (got_SIGINFO)
		handle_siginfo();
	while (got_fatal) {
		got_fatal = 0;
		aborting = ABORT_INTERRUPT;

		if (got_SIGINT) {
			got_SIGINT=0;
			handle_fatal_signal(SIGINT);
		}
		if (got_SIGHUP) {
			got_SIGHUP=0;
			handle_fatal_signal(SIGHUP);
		}
		if (got_SIGQUIT) {
			got_SIGQUIT=0;
			handle_fatal_signal(SIGQUIT);
		}
		if (got_SIGTERM) {
			got_SIGTERM=0;
			handle_fatal_signal(SIGTERM);
		}
	}
}

static void
debug_vprintf(const char *fmt, va_list va)
{
	(void)printf("[%ld] ", (long)mypid);
	(void)vprintf(fmt, va);
	fflush(stdout);
}

void
debug_job_printf(const char *fmt, ...)
{
	if (DEBUG(JOB)) {
		va_list va;
		va_start(va, fmt);
		debug_vprintf(fmt, va);
		va_end(va);
	}
}

static void
debug_kill_printf(const char *fmt, ...)
{
	if (DEBUG(KILL)) {
		va_list va;
		va_start(va, fmt);
		debug_vprintf(fmt, va);
		va_end(va);
	}
}

/*-
 *-----------------------------------------------------------------------
 * postprocess_job  --
 *	Do final processing for the given job including updating
 *	parents and starting new jobs as available/necessary.
 *
 * Side Effects:
 *	If we got an error and are aborting (aborting == ABORT_ERROR) and
 *	the job list is now empty, we are done for the day.
 *	If we recognized an error we set the aborting flag
 *	to ABORT_ERROR so no more jobs will be started.
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/

static void
postprocess_job(Job *job, bool okay)
{
	if (okay &&
	    aborting != ABORT_ERROR &&
	    aborting != ABORT_INTERRUPT) {
		/* As long as we aren't aborting and the job didn't return a
		 * non-zero status that we shouldn't ignore, we call
		 * Make_Update to update the parents. */
		job->node->built_status = MADE;
		Make_Update(job->node);
		free(job);
	}

	if (errorJobs != NULL && !keepgoing &&
	    aborting != ABORT_INTERRUPT)
		aborting = ABORT_ERROR;

	if (aborting == ABORT_ERROR && DEBUG(QUICKDEATH))
		handle_fatal_signal(SIGINT);
	if (aborting == ABORT_ERROR && Job_Empty())
		Finish();
}

/* expensive jobs handling: in order to avoid forking an exponential number
 * of jobs, make tries to figure out "recursive make" configurations.
 * It may err on the side of caution.
 * Basically, a command is "expensive" if it's likely to fork an extra
 * level of make: either by looking at the command proper, or if it has
 * some specific qualities ('+cmd' are likely to be recursive, as are
 * .MAKE: commands).  It's possible to explicitly say some targets are
 * expensive or cheap with .EXPENSIVE or .CHEAP.
 *
 * While an expensive command is running, no_new_jobs
 * is set, so jobs that would fork new processes are accumulated in the
 * heldJobs list instead.
 *
 * This heuristics is also used on error exit: we display silent commands
 * that failed, unless those ARE expensive commands: expensive commands
 * are likely to not be failing by themselves, but to be the result of
 * a cascade of failures in descendant makes.
 */
void
determine_expensive_job(Job *job)
{ 
	if (expensive_job(job)) {
		job->flags |= JOB_IS_EXPENSIVE;
		no_new_jobs = true;
	} else
		job->flags &= ~JOB_IS_EXPENSIVE;
	if (DEBUG(EXPENSIVE))
		fprintf(stderr, "[%ld] Target %s running %.50s: %s\n",
		    (long)mypid, job->node->name, job->cmd,
		    job->flags & JOB_IS_EXPENSIVE ? "expensive" : "cheap");
}

static bool
expensive_job(Job *job)
{
	if (job->node->type & OP_CHEAP)
		return false;
	if (job->node->type & (OP_EXPENSIVE | OP_MAKE))
		return true;
	return expensive_command(job->cmd);
}

static bool
expensive_command(const char *s)
{
	const char *p;
	bool include = false;
	bool expensive = false;

	/* okay, comments are cheap, always */
	if (*s == '#')
		return false;
	/* and commands we always execute are expensive */
	if (*s == '+')
		return true;

	for (p = s; *p != '\0'; p++) {
		if (*p == ' ' || *p == '\t') {
			include = false;
			if (p[1] == '-' && p[2] == 'I')
				include = true;
		}
		if (include)
			continue;
		/* KMP variant, avoid looking twice at the same
		 * letter.
		 */
		if (*p != 'm')
			continue;
		if (p[1] != 'a')
			continue;
		p++;
		if (p[1] != 'k')
			continue;
		p++;
		if (p[1] != 'e')
			continue;
		p++;
		expensive = true;
		while (p[1] != '\0' && p[1] != ' ' && p[1] != '\t') {
			if (p[1] == '.' || p[1] == '/') {
				expensive = false;
				break;
			}
		    	p++;
		}
		if (expensive)
			return true;
	}
	return false;
}

static Job *
prepare_job(GNode *gn)
{
	/* a new job is prepared unless its commands are bogus (we don't
	 * have anything for it), or if we're in touch mode.
	 *
	 * Note that even in noexec mode, some commands may still run
	 * thanks to the +cmd construct.
	 */
	if (node_find_valid_commands(gn)) {
		if (touchFlag) {
			Job_Touch(gn);
			return NULL;
		} else {
			Job *job;       	

			job = emalloc(sizeof(Job));
			if (job == NULL)
				Punt("can't create job: out of memory");

			job_attach_node(job, gn);
			return job;
		}
	} else {
		node_failure(gn);
		return NULL;
	}
}

static void
may_continue_job(Job *job)
{
	if (no_new_jobs) {
		if (DEBUG(EXPENSIVE))
			fprintf(stderr, "[%ld] expensive -> hold %s\n",
			    (long)mypid, job->node->name);
		job->next = heldJobs;
		heldJobs = job;
	} else
		continue_job(job);
}

static void
continue_job(Job *job)
{
	bool finished = job_run_next(job);
	if (finished)
		remove_job(job, true);
	else
		determine_expensive_job(job);
}

/*-
 *-----------------------------------------------------------------------
 * Job_Make  --
 *	Start a target-creation process going for the target described
 *	by the graph node gn.
 *
 * Side Effects:
 *	A new Job node is created and  its commands continued, which
 *	may fork the first command of that job.
 *-----------------------------------------------------------------------
 */
void
Job_Make(GNode *gn)
{
	Job *job;

	job = prepare_job(gn);
	if (!job)
		return;
	nJobs++;
	may_continue_job(job);
}

static void
determine_job_next_step(Job *job)
{
	bool okay;
	if (job->flags & JOB_IS_EXPENSIVE) {
		no_new_jobs = false;
		if (DEBUG(EXPENSIVE))
			fprintf(stderr, "[%ld] "
			    "Returning from expensive target %s, "
			    "allowing new jobs\n", (long)mypid, 
			    job->node->name);
	}

	okay = job->exit_type == JOB_EXIT_OKAY;
	if (!okay || job->next_cmd == NULL)
		remove_job(job, okay);
	else
		may_continue_job(job);
}

static void
remove_job(Job *job, bool okay)
{
	nJobs--;
	postprocess_job(job, okay);
	while (!no_new_jobs) {
		if (heldJobs != NULL) {
			job = heldJobs;
			heldJobs = heldJobs->next;
			if (DEBUG(EXPENSIVE))
				fprintf(stderr, "[%ld] cheap -> release %s\n",
				    (long)mypid, job->node->name);
			continue_job(job);
		} else
			break;
	}
}

/*
 * job = reap_finished_job(pid):
 * 	retrieve and remove a job from runningJobs, based on its pid
 *
 *	Note that we remove it right away, so that handle_signals()
 *	is accurate.
 */
static Job *
reap_finished_job(pid_t pid)
{
	Job **j, *job;

	for (j = &runningJobs; *j != NULL; j = &((*j)->next))
		if ((*j)->pid == pid) {
			job = *j;
			*j = job->next;
			return job;
		}

	return NULL;
}

/*
 * classic waitpid handler: retrieve as many dead children as possible.
 * returns true if succesful
 */
static bool
reap_jobs(void)
{
 	pid_t pid;	/* pid of dead child */
 	int status;	/* Exit/termination status */
	bool reaped = false;
	Job *job;

	while ((pid = waitpid(WAIT_ANY, &status, WNOHANG)) > 0) {
		reaped = true;
		job = reap_finished_job(pid);

		if (job == NULL) {
			Punt("Child (%ld) not in table?", (long)pid);
		} else {
			job_handle_status(job, status);
			determine_job_next_step(job);
		}
	}
	/* sanity check, should not happen */
	if (pid == -1 && errno == ECHILD && runningJobs != NULL)
		Punt("Process has no children, but runningJobs is not empty ?");
	return reaped;
}

void
handle_running_jobs(void)
{
	sigset_t old;
	/* reaping children in the presence of caught signals */

	/* first, we make sure to hold on new signals, to synchronize
	 * reception of new stuff on sigsuspend
	 */
	sigprocmask(SIG_BLOCK, &sigset, &old);
	/* note this will NOT loop until runningJobs == NULL.
	 * It's merely an optimisation, namely that we don't need to go 
	 * through the logic if no job is present. As soon as a job 
	 * gets reaped, we WILL exit the loop through the break.
	 */
	while (runningJobs != NULL) {
		/* did we already have pending stuff that advances things ?
		 * then handle_all_signals() will not return
		 * or reap_jobs() will reap_jobs()
		 */
		handle_all_signals();
		if (reap_jobs())
			break;
		/* okay, so it's safe to suspend, we have nothing to do but
		 * wait...
		 */
		sigsuspend(&emptyset);
	}
	sigprocmask(SIG_SETMASK, &old, NULL);
}

void
handle_one_job(Job *job)
{
	int stat;
	int status;
	sigset_t old;

	sigprocmask(SIG_BLOCK, &sigset, &old);
	while (1) {
		handle_all_signals();
		stat = waitpid(job->pid, &status, WNOHANG);
		if (stat == job->pid)
			break;
		sigsuspend(&emptyset);
	}
	runningJobs = NULL;
	job_handle_status(job, status);
	sigprocmask(SIG_SETMASK, &old, NULL);
}

static void
loop_handle_running_jobs()
{
	while (runningJobs != NULL)
		handle_running_jobs();
}

void
Job_Init(int maxproc)
{
	runningJobs = NULL;
	heldJobs = NULL;
	errorJobs = NULL;
	maxJobs = maxproc;
	mypid = getpid();

	nJobs = 0;

	aborting = 0;
	setup_all_signals();
}

bool
can_start_job(void)
{
	if (aborting || nJobs >= maxJobs)
		return false;
	else
		return true;
}

bool
Job_Empty(void)
{
	return runningJobs == NULL;
}

/*-
 *-----------------------------------------------------------------------
 * handle_fatal_signal --
 *	Handle the receipt of a fatal interrupt
 *
 * Side Effects:
 *	All children are killed. Another job may be started if there
 *	is an interrupt target and the signal was SIGINT.
 *-----------------------------------------------------------------------
 */
static void
handle_fatal_signal(int signo)
{
	Job *job;

	debug_kill_printf("handle_fatal_signal(%d) called.\n", signo);

	dying_signal = signo;
	for (job = runningJobs; job != NULL; job = job->next) {
		debug_kill_printf("passing to "
		    "child %ld running %s: %s\n", (long)job->pid,
		    job->node->name, really_kill(job, signo));
		may_remove_target(job);
	}

	if (signo == SIGINT && !touchFlag) {
		if ((interrupt_node->type & OP_DUMMY) == 0) {
			ignoreErrors = false;

			Job_Make(interrupt_node);
		}
	}
	loop_handle_running_jobs();
	internal_print_errors();

	/* die by that signal */
	sigprocmask(SIG_BLOCK, &sigset, NULL);
	signal(signo, SIG_DFL);
	kill(getpid(), signo);
	sigprocmask(SIG_SETMASK, &emptyset, NULL);
	/*NOTREACHED*/
	fprintf(stderr, "This should never happen\n");
	exit(1);
}

/*
 *-----------------------------------------------------------------------
 * Job_Finish --
 *	Do final processing such as the running of the commands
 *	attached to the .END target.
 *
 *	return true if fatal errors have happened.
 *-----------------------------------------------------------------------
 */
bool
Job_Finish(void)
{
	bool problem = errorJobs != NULL;

	if ((end_node->type & OP_DUMMY) == 0) {
		if (problem) {
			Error("Errors reported so .END ignored");
		} else {
			Job_Make(end_node);
			loop_handle_running_jobs();
		}
	}
	return problem;
}

void
Job_Begin(void)
{
	if ((begin_node->type & OP_DUMMY) == 0) {
		Job_Make(begin_node);
		loop_handle_running_jobs();
	}
}

/*-
 *-----------------------------------------------------------------------
 * Job_Wait --
 *	Waits for all running jobs to finish and returns. Sets 'aborting'
 *	to ABORT_WAIT to prevent other jobs from starting.
 *
 * Side Effects:
 *	Currently running jobs finish.
 *
 *-----------------------------------------------------------------------
 */
void
Job_Wait(void)
{
	aborting = ABORT_WAIT;
	loop_handle_running_jobs();
	aborting = 0;
}

/*-
 *-----------------------------------------------------------------------
 * Job_AbortAll --
 *	Abort all currently running jobs without handling output or anything.
 *	This function is to be called only in the event of a major
 *	error.
 *
 * Side Effects:
 *	All children are killed
 *-----------------------------------------------------------------------
 */
void
Job_AbortAll(void)
{
	Job *job;	/* the job descriptor in that element */
	int foo;

	aborting = ABORT_ERROR;

	for (job = runningJobs; job != NULL; job = job->next) {
		killpg(job->pid, SIGINT);
		killpg(job->pid, SIGKILL);
	}

	/*
	 * Catch as many children as want to report in at first, then give up
	 */
	while (waitpid(WAIT_ANY, &foo, WNOHANG) > 0)
		continue;
}
