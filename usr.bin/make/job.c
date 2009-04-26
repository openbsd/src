/*	$OpenPackages$ */
/*	$OpenBSD: job.c,v 1.116 2009/04/26 09:25:49 espie Exp $	*/
/*	$NetBSD: job.c,v 1.16 1996/11/06 17:59:08 christos Exp $	*/

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
 *	Job_Init		Called to initialize this module. in addition,
 *				any commands attached to the .BEGIN target
 *				are executed before this function returns.
 *				Hence, the makefile must have been parsed
 *				before this function is called.
 *
 *	Job_End 		Cleanup any memory used.
 *
 *	Job_Full		Return true if the job table is filled.
 *
 *	Job_Empty		Return true if the job table is completely
 *				empty.
 *
 *	Job_Finish		Perform any final processing which needs doing.
 *				This includes the execution of any commands
 *				which have been/were attached to the .END
 *				target. It should only be called when the
 *				job table is empty.
 *
 *	Job_AbortAll		Abort all currently running jobs. It doesn't
 *				handle output or do anything for the jobs,
 *				just kills them. It should only be called in
 *				an emergency, as it were.
 *
 *	Job_Wait		Wait for all currently-running jobs to finish.
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
#include "lst.h"
#include "extern.h"
#include "gnode.h"
#include "memory.h"
#include "make.h"

/*
 * The SEL_ constants determine the maximum amount of time spent in select
 * before coming out to see if a child has finished. SEL_SEC is the number of
 * seconds and SEL_USEC is the number of micro-seconds
 */
#define SEL_SEC 	0
#define SEL_USEC	500000


/*-
 * Job Table definitions.
 *
 * Each job has several things associated with it:
 *	1) The process id of the child shell
 *	2) The graph node describing the target being made by this job
 *	3) An FILE* for writing out the commands. This is only
 *	   used before the job is actually started.
 *	4) Things used for handling the shell's output.
 *	   the output is being caught via a pipe and
 *	   the descriptors of our pipe, an array in which output is line
 *	   buffered and the current position in that buffer are all
 *	   maintained for each job.
 *	5) A word of flags which determine how the module handles errors,
 *	   echoing, etc. for the job
 *
 * The job "table" is kept as a linked Lst in 'jobs', with the number of
 * active jobs maintained in the 'nJobs' variable. At no time will this
 * exceed the value of 'maxJobs', initialized by the Job_Init function.
 *
 * When a job is finished, the Make_Update function is called on each of the
 * parents of the node which was just remade. This takes care of the upward
 * traversal of the dependency graph.
 */
#define JOB_BUFSIZE	1024
struct job_pipe {
	int fd;
	char buffer[JOB_BUFSIZE];
	size_t pos;
};
	
typedef struct Job_ {
    pid_t 	pid;	    /* The child's process ID */
    GNode	*node;	    /* The target the child is making */
    short	flags;	    /* Flags to control treatment of job */
#define JOB_SPECIAL	0x004	/* Target is a special one. */
#define JOB_RESTART	0x080	/* Job needs to be completely restarted */
#define JOB_RESUME	0x100	/* Job needs to be resumed b/c it stopped,
				 * for some reason */
#define JOB_CONTINUING	0x200	/* We are in the process of resuming this job.
				 * Used to avoid infinite recursion between
				 * JobFinish and JobRestart */
#define JOB_DIDOUTPUT	0x001
    struct job_pipe in[2];
} Job;


static int	aborting = 0;	    /* why is the make aborting? */
#define ABORT_ERROR	1	    /* Because of an error */
#define ABORT_INTERRUPT 2	    /* Because it was interrupted */
#define ABORT_WAIT	3	    /* Waiting for jobs to finish */

static int	maxJobs;	/* The most children we can run at once */
static int	nJobs;		/* The number of children currently running */
static LIST	runningJobs;	/* The structures that describe them */
static bool	jobFull;	/* Flag to tell when the job table is full. It
				 * is set true when nJobs equals maxJobs */
static GNode	*lastNode;	/* The node for which output was most recently
				 * produced. */

/* data structure linked to job handling through select */
static fd_set *output_mask = NULL;	/* File descriptors to look for */
				 
static fd_set *actual_mask = NULL;	/* actual select argument */
static int largest_fd = -1;
static size_t mask_size = 0;

static LIST	stoppedJobs;	

/* wait possibilities */
#define JOB_EXITED 0
#define JOB_SIGNALED 1
#define JOB_CONTINUED 2
#define JOB_STOPPED 3
#define JOB_UNKNOWN 4

static LIST	errorsList;
static int	errors;
struct error_info {
	int reason;
	int code;
	GNode *n;
};


#if defined(USE_PGRP) && defined(SYSV)
# define KILL(pid, sig) 	killpg(-(pid), (sig))
#else
# if defined(USE_PGRP)
#  define KILL(pid, sig)	killpg((pid), (sig))
# else
#  define KILL(pid, sig)	kill((pid), (sig))
# endif
#endif

static void signal_running_jobs(int);
static void handle_all_signals(void);
static void handle_signal(int);
static int JobCmpPid(void *, void *);
static void JobFinish(Job *, int);
static void finish_job(Job *, int, int);
static void JobExec(Job *);
static void JobRestart(Job *);
static void JobStart(GNode *, int);
static void JobInterrupt(int, int);
static void JobRestartJobs(void);
static void debug_printf(const char *, ...);
static Job *prepare_job(GNode *, int);
static void start_queued_job(Job *);
static void banner(Job *, FILE *);

/***
 ***  Input/output from jobs
 ***/

/* prepare_pipe(jp, &fd):
 *	set up pipe data structure (buffer and pos) corresponding to
 *	pointed fd, and prepare to watch for it.
 */
static void prepare_pipe(struct job_pipe *, int *);

/* close_job_pipes(j):
 *	handle final output from job, and close pipes properly
 */
static void close_job_pipes(Job *);


static void handle_all_jobs_output(void);

/* handle_job_output(job, n, finish): 
 *	n = 0 or 1 (stdout/stderr), set finish to retrieve everything.
 */
static void handle_job_output(Job *, int, bool);

static void print_partial_buffer(struct job_pipe *, Job *, FILE *, size_t);
static void print_partial_buffer_and_shift(struct job_pipe *, Job *, FILE *, 
    size_t);
static bool print_complete_lines(struct job_pipe *, Job *, FILE *, size_t);


static void register_error(int, int, Job *);
static void loop_handle_running_jobs(void);
static void Job_CatchChildren(void);

static void
register_error(int reason, int code, Job *job)
{
	struct error_info *p;

	errors++;
	p = emalloc(sizeof(struct error_info));
	p->reason = reason;
	p->code = code;
	p->n = job->node;
	Lst_AtEnd(&errorsList, p);
}

void
print_errors()
{
	LstNode ln;
	struct error_info *p;
	const char *type;

	for (ln = Lst_First(&errorsList); ln != NULL; ln = Lst_Adv(ln)) {
		p = (struct error_info *)Lst_Datum(ln);
		switch(p->reason) {
		case JOB_EXITED:
			type = "Exit status";
			break;
		case JOB_SIGNALED:
			type = "Received signal";
			break;
		case JOB_STOPPED:
			type = "Stopped";
			break;
		case JOB_CONTINUED:
			type = "Continued";
			break;
		default:
			type = "Should not happen";
			break;
		}
	if (p->n->lineno)
		Error(" %s %d (%s, line %lu of %s)",
		    type, p->code, p->n->name, p->n->lineno, p->n->fname);
	else 
		Error(" %s %d (%s)", type, p->code, p->n->name);
	}
}

static void
banner(Job *job, FILE *out)
{
	if (job->node != lastNode) {
		if (DEBUG(JOBBANNER)) 
			(void)fprintf(out, "--- %s ---\n", job->node->name);
		lastNode = job->node;
	}
}

static void
handle_all_signals()
{
	if (got_signal)
		got_signal = 0;
	else
		return;

	if (got_SIGINT) {
		got_SIGINT=0;
		handle_signal(SIGINT);
	}
	if (got_SIGHUP) {
		got_SIGHUP=0;
		handle_signal(SIGHUP);
	}
	if (got_SIGQUIT) {
		got_SIGQUIT=0;
		handle_signal(SIGQUIT);
	}
	if (got_SIGTERM) {
		got_SIGTERM=0;
		handle_signal(SIGTERM);
	}
	if (got_SIGTSTP) {
		got_SIGTSTP=0;
		handle_signal(SIGTSTP);
	}
	if (got_SIGTTOU) {
		got_SIGTTOU=0;
		handle_signal(SIGTTOU);
	}
	if (got_SIGTTIN) {
		got_SIGTTIN=0;
		handle_signal(SIGTTIN);
	}
	if (got_SIGWINCH) {
		got_SIGWINCH=0;
		handle_signal(SIGWINCH);
	}
}

static void
signal_running_jobs(int signo)
{
	LstNode ln;
	for (ln = Lst_First(&runningJobs); ln != NULL; ln = Lst_Adv(ln)) {
	    	Job *job = Lst_Datum(ln);
		if (DEBUG(JOB)) {
			(void)fprintf(stdout,
			    "signal %d to child %ld.\n",
			    signo, (long)job->pid);
			(void)fflush(stdout);
		}
		KILL(job->pid, signo);
	}
}

/*-
 *-----------------------------------------------------------------------
 * handle_signal --
 *	Pass a signal to all local jobs if USE_PGRP is defined,
 *	then die ourselves.
 *
 * Side Effects:
 *	We die by the same signal.
 *-----------------------------------------------------------------------
 */
static void
handle_signal(int signo) /* The signal number we've received */
{
	sigset_t nmask, omask;
	struct sigaction act;

	if (DEBUG(JOB)) {
		(void)fprintf(stdout, "handle_signal(%d) called.\n", signo);
		(void)fflush(stdout);
	}
	signal_running_jobs(signo);

	/*
	 * Deal with proper cleanup based on the signal received. We only run
	 * the .INTERRUPT target if the signal was in fact an interrupt. The
	 * other three termination signals are more of a "get out *now*"
	 * command.
	 */
	if (signo == SIGINT) {
		JobInterrupt(true, signo);
	} else if (signo == SIGHUP || signo == SIGTERM || signo == SIGQUIT) {
		JobInterrupt(false, signo);
	}

	/*
	 * Leave gracefully if SIGQUIT, rather than core dumping.
	 */
	if (signo == SIGQUIT) {
		Finish(0);
	}

	/*
	 * Send ourselves the signal now we've given the message to everyone
	 * else.  Note we block everything else possible while we're getting
	 * the signal.  This ensures that all our jobs get continued when we
	 * wake up before we take any other signal.
	 */
	sigemptyset(&nmask);
	sigaddset(&nmask, signo);
	sigprocmask(SIG_SETMASK, &nmask, &omask);
	memset(&act, 0, sizeof act);
	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(signo, &act, NULL);

	if (DEBUG(JOB)) {
		(void)fprintf(stdout,
		    "handle_signal passing signal to self, mask = %x.\n",
		    ~0 & ~(1 << (signo-1)));
		(void)fflush(stdout);
	}
	(void)signal(signo, SIG_DFL);

	(void)KILL(getpid(), signo);

	signal_running_jobs(SIGCONT);

	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
	sigprocmask(SIG_SETMASK, &omask, NULL);
	act.sa_handler = SigHandler;
	sigaction(signo, &act, NULL);
}

/*-
 *-----------------------------------------------------------------------
 * JobCmpPid  --
 *	Compare the pid of the job with the given pid and return 0 if they
 *	are equal. This function is called from Job_CatchChildren via
 *	Lst_Find to find the job descriptor of the finished job.
 *
 * Results:
 *	0 if the pid's match
 *-----------------------------------------------------------------------
 */
static int
JobCmpPid(void *job,	/* job to examine */
    void *pid)		/* process id desired */
{
	return *(pid_t *)pid - ((Job *)job)->pid;
}

static void
debug_printf(const char *fmt, ...)
{
	if (DEBUG(JOB)) {
		va_list va;

		va_start(va, fmt);
		(void)vfprintf(stdout, fmt, va);
		fflush(stdout);
		va_end(va);
	}
}
	
static void
close_job_pipes(Job *job)
{
	int i;

	for (i = 1; i >= 0; i--) {
		FD_CLR(job->in[i].fd, output_mask);
		handle_job_output(job, i, true);
		(void)close(job->in[i].fd);
	}
}

/*-
 *-----------------------------------------------------------------------
 * JobFinish  --
 *	Do final processing for the given job including updating
 *	parents and starting new jobs as available/necessary. 
 *
 * Side Effects:
 *	Some nodes may be put on the toBeMade queue.
 *	Final commands for the job are placed on end_node.
 *
 *	If we got an error and are aborting (aborting == ABORT_ERROR) and
 *	the job list is now empty, we are done for the day.
 *	If we recognized an error we set the aborting flag
 *	to ABORT_ERROR so no more jobs will be started.
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/

static void
JobFinish(Job *job, int status)
{
	int reason, code;
	/* parse status */
	if (WIFEXITED(status)) {
		reason = JOB_EXITED;
		code = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		reason = JOB_SIGNALED;
		code = WTERMSIG(status);
	} else if (WIFCONTINUED(status)) {
		reason = JOB_CONTINUED;
		code = 0;
    	} else if (WIFSTOPPED(status)) {
		reason = JOB_STOPPED;
		code = WSTOPSIG(status);
	} else {
		/* can't happen, set things to be bad. */
		reason = UNKNOWN;
		code = status;
	}
	finish_job(job, reason, code);
}


static void
finish_job(Job *job, int reason, int code)
{
	bool	 done;

	if ((reason == JOB_EXITED &&
	     code != 0 && !(job->node->type & OP_IGNORE)) ||
	    (reason == JOB_SIGNALED && code != SIGCONT)) {
		/*
		 * If it exited non-zero and either we're doing things our
		 * way or we're not ignoring errors, the job is finished.
		 * Similarly, if the shell died because of a signal
		 * the job is also finished. In these
		 * cases, finish out the job's output before printing the exit
		 * status...
		 */
		close_job_pipes(job);
		done = true;
	} else if (reason == JOB_EXITED) {
		/*
		 * Deal with ignored errors in -B mode. We need to print a
		 * message telling of the ignored error as well as setting
		 * status.w_status to 0 so the next command gets run. To do
		 * this, we set done to be true if in -B mode and the job
		 * exited non-zero.
		 */
		done = code != 0;
		close_job_pipes(job);
	} else {
		/*
		 * No need to close things down or anything.
		 */
		done = false;
	}

	if (reason == JOB_STOPPED) {
		debug_printf("Process %ld stopped.\n", (long)job->pid);
		banner(job, stdout);
		(void)fprintf(stdout, "*** Stopped -- signal %d\n",
		    code);
		job->flags |= JOB_RESUME;
		Lst_AtEnd(&stoppedJobs, job);
		(void)fflush(stdout);
		return;
	}
	if (reason == JOB_SIGNALED && code == SIGCONT) {
		/*
		 * If the beastie has continued, shift the Job from the
		 * stopped list to the running one (or re-stop it if
		 * concurrency is exceeded) and go and get another
		 * child.
		 */
		if (job->flags & (JOB_RESUME|JOB_RESTART)) {
			banner(job, stdout);
			(void)fprintf(stdout, "*** Continued\n");
		}
		if (!(job->flags & JOB_CONTINUING)) {
			debug_printf(
			    "Warning: "
			    "process %ld was not continuing.\n",
			    (long)job->pid);
		}
		job->flags &= ~JOB_CONTINUING;
		Lst_AtEnd(&runningJobs, job);
		nJobs++;
		debug_printf("Process %ld is continuing locally.\n",
		    (long)job->pid);
		if (nJobs == maxJobs) {
			jobFull = true;
			debug_printf("Job queue is full.\n");
		}
		(void)fflush(stdout);
		return;
	}

	if (done || DEBUG(JOB)) {
		if (reason == JOB_EXITED) {
			debug_printf("Process %ld exited.\n", (long)job->pid);
			if (code != 0) {
				banner(job, stdout);
				(void)fprintf(stdout, "*** Error code %d %s\n",
				    code,
				    (job->node->type & OP_IGNORE) ? 
				    "(ignored)" : "");

				if (job->node->type & OP_IGNORE) {
					reason = JOB_EXITED;
					code = 0;
				}
			} else if (DEBUG(JOB)) {
				banner(job, stdout);
				(void)fprintf(stdout,
				    "*** Completed successfully\n");
			}
		} else {
			banner(job, stdout);
			(void)fprintf(stdout, "*** Signal %d\n", code);
		}

		(void)fflush(stdout);
	}

	done = true;

	if (done &&
	    aborting != ABORT_ERROR &&
	    aborting != ABORT_INTERRUPT &&
	    reason == JOB_EXITED && code == 0) {
		/* As long as we aren't aborting and the job didn't return a
		 * non-zero status that we shouldn't ignore, we call
		 * Make_Update to update the parents. */
		job->node->built_status = MADE;
		Make_Update(job->node);
		free(job);
	} else if (!(reason == JOB_EXITED && code == 0)) {
		register_error(reason, code, job);
		free(job);
	}

	/*
	 * Set aborting if any error.
	 */
	if (errors && !keepgoing && 
	    aborting != ABORT_INTERRUPT) {
		/*
		 * If we found any errors in this batch of children and the -k
		 * flag wasn't given, we set the aborting flag so no more jobs
		 * get started.
		 */
		aborting = ABORT_ERROR;
	}

	if (aborting != ABORT_ERROR)
		JobRestartJobs();

	if (aborting == ABORT_ERROR && Job_Empty()) {
		/*
		 * If we are aborting and the job table is now empty, we finish.
		 */
		Finish(errors);
	}
}

static void 
prepare_pipe(struct job_pipe *p, int *fd)
{
	p->pos = 0;
	(void)fcntl(fd[0], F_SETFD, FD_CLOEXEC);
	p->fd = fd[0]; 
	close(fd[1]);

	if (output_mask == NULL || p->fd > largest_fd) {
		int fdn, ofdn;

		fdn = howmany(p->fd+1, NFDBITS);
		ofdn = howmany(largest_fd+1, NFDBITS);

		if (fdn != ofdn) {
			output_mask = emult_realloc(output_mask, fdn, 
			    sizeof(fd_mask));
			memset(((char *)output_mask) + ofdn * sizeof(fd_mask), 
			    0, (fdn-ofdn) * sizeof(fd_mask));
			actual_mask = emult_realloc(actual_mask, fdn, 
			    sizeof(fd_mask));
			mask_size = fdn * sizeof(fd_mask);
		}
		largest_fd = p->fd;
	}
	fcntl(p->fd, F_SETFL, O_NONBLOCK);
	FD_SET(p->fd, output_mask);
}

/*-
 *-----------------------------------------------------------------------
 * JobExec --
 *	Execute the shell for the given job. Called from JobStart and
 *	JobRestart.
 *
 * Side Effects:
 *	A shell is executed, outputs is altered and the Job structure added
 *	to the job table.
 *-----------------------------------------------------------------------
 */
static void
JobExec(Job *job)
{
	pid_t cpid; 	/* ID of new child */
	int fds[4];
	int *fdout = fds;
	int *fderr = fds+2;
	int i;

	if (DEBUG(JOB)) {
		LstNode ln;

		(void)fprintf(stdout, "Running %s\n", job->node->name);
		for (ln = Lst_First(&job->node->commands); ln != NULL ; 
		    ln = Lst_Adv(ln))
		    	fprintf(stdout, "\t%s\n", (char *)Lst_Datum(ln));
		(void)fflush(stdout);
	}

	/*
	 * Some jobs produce no output and it's disconcerting to have
	 * no feedback of their running (since they produce no output, the
	 * banner with their name in it never appears). This is an attempt to
	 * provide that feedback, even if nothing follows it.
	 */
	banner(job, stdout);

	setup_engine();

	/* Create the pipe by which we'll get the shell's output. 
	 */
	if (pipe(fdout) == -1)
		Punt("Cannot create pipe: %s", strerror(errno));

	if (pipe(fderr) == -1)
		Punt("Cannot create pipe: %s", strerror(errno));

	if ((cpid = fork()) == -1) {
		Punt("Cannot fork");
	} else if (cpid == 0) {
		supervise_jobs = false;
		/* standard pipe code to route stdout and stderr */
		close(fdout[0]);
		if (dup2(fdout[1], 1) == -1)
			Punt("Cannot dup2(outPipe): %s", strerror(errno));
		if (fdout[1] != 1)
			close(fdout[1]);
		close(fderr[0]);
		if (dup2(fderr[1], 2) == -1)
			Punt("Cannot dup2(errPipe): %s", strerror(errno));
		if (fderr[1] != 2)
			close(fderr[1]);

#ifdef USE_PGRP
		/*
		 * We want to switch the child into a different process family
		 * so we can kill it and all its descendants in one fell swoop,
		 * by killing its process family, but not commit suicide.
		 */
# if defined(SYSV)
		(void)setsid();
# else
		(void)setpgid(0, getpid());
# endif
#endif /* USE_PGRP */

		if (random_delay)
			if (!(nJobs == 1 && no_jobs_left()))
				usleep(random() % random_delay);

		/* this exits directly */
		run_gnode_parallel(job->node);
		/*NOTREACHED*/
	} else {
		supervise_jobs = true;
		job->pid = cpid;

		/* we set the current position in the buffers to the beginning
		 * and mark another stream to watch in the outputs mask
		 */
		for (i = 0; i < 2; i++)
			prepare_pipe(&job->in[i], fds+2*i);
	}

	/*
	 * Now the job is actually running, add it to the table.
	 */
	nJobs++;
	Lst_AtEnd(&runningJobs, job);
	if (nJobs == maxJobs) {
		jobFull = true;
	}
}

static void
start_queued_job(Job *job)
{
	if (DEBUG(JOB)) {
		(void)fprintf(stdout, "Restarting %s...",
		    job->node->name);
		(void)fflush(stdout);
	}
	if (nJobs >= maxJobs && !(job->flags & JOB_SPECIAL)) {
		/*
		 * Can't be exported and not allowed to run locally --
		 * put it back on the hold queue and mark the table
		 * full
		 */
		debug_printf("holding\n");
		Lst_AtFront(&stoppedJobs, job);
		jobFull = true;
		debug_printf("Job queue is full.\n");
		return;
	} else {
		/*
		 * Job may be run locally.
		 */
		debug_printf("running locally\n");
	}
	JobExec(job);
}

/*-
 *-----------------------------------------------------------------------
 * JobRestart --
 *	Restart a job that stopped for some reason.
 *
 * Side Effects:
 *	jobFull will be set if the job couldn't be run.
 *-----------------------------------------------------------------------
 */
static void
JobRestart(Job *job)
{
	if (job->flags & JOB_RESTART) {
		start_queued_job(job);
	} else {
		/*
		 * The job has stopped and needs to be restarted. Why it
		 * stopped, we don't know...
		 */
		debug_printf("Resuming %s...", job->node->name);
		if ((nJobs < maxJobs || ((job->flags & JOB_SPECIAL) &&
		    maxJobs == 0)) && nJobs != maxJobs) {
			/*
			 * If we haven't reached the concurrency limit already
			 * (or maxJobs is 0), it's ok to resume the job.
			 */
			bool error;

			error = KILL(job->pid, SIGCONT) != 0;

			if (!error) {
				/*
				 * Make sure the user knows we've continued the
				 * beast and actually put the thing in the job
				 * table.
				 */
				job->flags |= JOB_CONTINUING;
				finish_job(job, JOB_SIGNALED, SIGCONT);

				job->flags &= ~(JOB_RESUME|JOB_CONTINUING);
				debug_printf("done\n");
			} else {
				Error("couldn't resume %s: %s",
				    job->node->name, strerror(errno));
				finish_job(job, JOB_EXITED, 1);
			}
		} else {
			/*
			 * Job cannot be restarted. Mark the table as full and
			 * place the job back on the list of stopped jobs.
			 */
			debug_printf("table full\n");
			Lst_AtFront(&stoppedJobs, job);
			jobFull = true;
			debug_printf("Job queue is full.\n");
		}
	}
}

static Job *
prepare_job(GNode *gn, int flags)
{
	bool cmdsOK;     	/* true if the nodes commands were all right */
	bool noExec;     	/* Set true if we decide not to run the job */

	/*
	 * Check the commands now so any attributes from .DEFAULT have a chance
	 * to migrate to the node
	 */
	cmdsOK = Job_CheckCommands(gn);
	expand_commands(gn);

	if ((gn->type & OP_MAKE) || (!noExecute && !touchFlag)) {
		/*
		 * We're serious here, but if the commands were bogus, we're
		 * also dead...
		 */
		if (!cmdsOK)
			job_failure(gn, Punt);

		if (Lst_IsEmpty(&gn->commands))
			noExec = true;
		else
			noExec = false;

	} else if (noExecute) {
		if (!cmdsOK || Lst_IsEmpty(&gn->commands))
			noExec = true;
		else
			noExec = false;
	} else {
		/*
		 * Just touch the target and note that no shell should be
		 * executed.  Check
		 * the commands, too, but don't die if they're no good -- it
		 * does no harm to keep working up the graph.
		 */
		Job_Touch(gn);
		noExec = true;
	}

	/*
	 * If we're not supposed to execute a shell, don't.
	 */
	if (noExec) {
		/*
		 * We only want to work our way up the graph if we aren't here
		 * because the commands for the job were no good.
		 */
		if (cmdsOK && !aborting) {
			gn->built_status = MADE;
			Make_Update(gn);
		}
		return NULL;
	} else {
		Job *job;       	/* new job descriptor */
		job = emalloc(sizeof(Job));
		if (job == NULL)
			Punt("JobStart out of memory");

		job->node = gn;

		/*
		 * Set the initial value of the flags for this job based on the
		 * global ones and the node's attributes... Any flags supplied
		 * by the caller are also added to the field.
		 */
		job->flags = flags;

		return job;
	}
}

/*-
 *-----------------------------------------------------------------------
 * JobStart  --
 *	Start a target-creation process going for the target described
 *	by the graph node gn.
 *
 * Side Effects:
 *	A new Job node is created and added to the list of running
 *	jobs. Make is forked and a child shell created.
 *-----------------------------------------------------------------------
 */
static void
JobStart(GNode *gn,	      	/* target to create */
    int flags)      		/* flags for the job to override normal ones.
			       	 * e.g. JOB_SPECIAL */
{
	Job *job;
	job = prepare_job(gn, flags);
	if (!job)
		return;
	if (nJobs >= maxJobs && !(job->flags & JOB_SPECIAL) &&
	    maxJobs != 0) {
		/*
		 * The job can only be run locally, but we've hit the limit of
		 * local concurrency, so put the job on hold until some other
		 * job finishes. Note that the special jobs (.BEGIN, .INTERRUPT
		 * and .END) may be run locally even when the local limit has
		 * been reached (e.g. when maxJobs == 0), though they will be
		 * exported if at all possible. In addition, any target marked
		 * with .NOEXPORT will be run locally if maxJobs is 0.
		 */
		jobFull = true;

		debug_printf("Can only run job locally.\n");
		job->flags |= JOB_RESTART;
		Lst_AtEnd(&stoppedJobs, job);
	} else {
		if (nJobs >= maxJobs) {
			/*
			 * If we're running this job locally as a special case
			 * (see above), at least say the table is full.
			 */
			jobFull = true;
			debug_printf("Local job queue is full.\n");
		}
		JobExec(job);
	}
}

/* Helper functions for JobDoOutput */


/* output debugging banner and print characters from 0 to endpos */
static void
print_partial_buffer(struct job_pipe *p, Job *job, FILE *out, size_t endPos)
{
	size_t i;

	banner(job, out);
	job->flags |= JOB_DIDOUTPUT;
	for (i = 0; i < endPos; i++)
		putc(p->buffer[i], out);
}

/* print partial buffer and shift remaining contents */
static void
print_partial_buffer_and_shift(struct job_pipe *p, Job *job, FILE *out,
    size_t endPos)
{
	size_t i;

	print_partial_buffer(p, job, out, endPos);

	for (i = endPos; i < p->pos; i++)
		p->buffer[i-endPos] = p->buffer[i];
	p->pos -= endPos;
}

/* print complete lines, looking back to the limit position
 * (stuff before limit was already scanned).
 * returns true if something was printed.
 */
static bool
print_complete_lines(struct job_pipe *p, Job *job, FILE *out, size_t limit)
{
	size_t i;

	for (i = p->pos; i > limit; i--) {
		if (p->buffer[i-1] == '\n') {
			print_partial_buffer_and_shift(p, job, out, i);
			return true;
		}
	}
	return false;
}

/*-
 *-----------------------------------------------------------------------
 * handle_pipe	--
 *	This functions is called whenever there is something to read on the
 *	pipe. We collect more output from the given job and store it in the
 *	job's outBuf. If this makes up lines, we print it tagged by the job's
 *	identifier, as necessary.
 *
 * Side Effects:
 *	curPos may be shifted as may the contents of outBuf.
 *-----------------------------------------------------------------------
 */
static void
handle_pipe(struct job_pipe *p, 
	Job *job, FILE *out, bool finish)
{
	int nr;		  	/* number of bytes read */
	int oldpos;		/* optimization */

	/* want to get everything ? -> we block */
	if (finish)
		fcntl(p->fd, F_SETFL, 0);

	do {
		nr = read(p->fd, &p->buffer[p->pos],
		    JOB_BUFSIZE - p->pos);
		if (nr == -1) {
			if (errno == EAGAIN)
				break;
			if (DEBUG(JOB)) {
				perror("JobDoOutput(piperead)");
			}
		}
		oldpos = p->pos;
		p->pos += nr;
		if (!print_complete_lines(p, job, out, oldpos))
			if (p->pos == JOB_BUFSIZE) {
				print_partial_buffer(p, job, out, p->pos);
				p->pos = 0;
			}
	} while (nr != 0);

	/* at end of file, we print whatever is left */
	if (nr == 0) {
		print_partial_buffer(p, job, out, p->pos);
		if (p->pos > 0 && p->buffer[p->pos - 1] != '\n')
			putchar('\n');
		p->pos = 0;
	}
}

static void
handle_job_output(Job *job, int i, bool finish)
{
	handle_pipe(&job->in[i], job, i == 0 ? stdout : stderr, finish);
}

/*-
 *-----------------------------------------------------------------------
 * Job_CatchChildren --
 *	Handle the exit of a child. Called by handle_running_jobs
 *
 * Side Effects:
 *	The job descriptor is removed from the list of children.
 *
 * Notes:
 *	We do waits, blocking or not, according to the wisdom of our
 *	caller, until there are no more children to report. For each
 *	job, call JobFinish to finish things off. This will take care of
 *	putting jobs on the stoppedJobs queue.
 *-----------------------------------------------------------------------
 */
void
Job_CatchChildren()
{
	pid_t pid;	/* pid of dead child */
	Job *job; 	/* job descriptor for dead child */
	LstNode jnode;	/* list element for finding job */
	int status;	/* Exit/termination status */

	/*
	 * Don't even bother if we know there's no one around.
	 */
	if (nJobs == 0)
		return;

	while ((pid = waitpid((pid_t) -1, &status, WNOHANG|WUNTRACED)) > 0) {
		handle_all_signals();
		debug_printf("Process %ld exited or stopped.\n", (long)pid);

		jnode = Lst_Find(&runningJobs, JobCmpPid, &pid);

		if (jnode == NULL) {
			if (WIFSIGNALED(status) &&
			    (WTERMSIG(status) == SIGCONT)) {
				jnode = Lst_Find(&stoppedJobs, JobCmpPid, &pid);
				if (jnode == NULL) {
					Error("Resumed child (%ld) not in table", (long)pid);
					continue;
				}
				job = (Job *)Lst_Datum(jnode);
				Lst_Remove(&stoppedJobs, jnode);
			} else {
				Error("Child (%ld) not in table?", (long)pid);
				continue;
			}
		} else {
			job = (Job *)Lst_Datum(jnode);
			Lst_Remove(&runningJobs, jnode);
			nJobs--;
			if (jobFull)
				debug_printf("Job queue is no longer full.\n");
			jobFull = false;
		}

		JobFinish(job, status);
	}
}

void
handle_all_jobs_output(void)
{
	int nfds;
	struct timeval timeout;
	LstNode ln, ln2;
	Job *job;
	int i;
	int status;

	/* no jobs */
	if (Lst_IsEmpty(&runningJobs))
		return;

	(void)fflush(stdout);

	memcpy(actual_mask, output_mask, mask_size);
	timeout.tv_sec = SEL_SEC;
	timeout.tv_usec = SEL_USEC;

	nfds = select(largest_fd+1, actual_mask, NULL, NULL, &timeout);
	handle_all_signals();
	for (ln = Lst_First(&runningJobs); nfds && ln != NULL;
	    ln = ln2) {
	    	ln2 = Lst_Adv(ln);
		job = (Job *)Lst_Datum(ln);
		job->flags &= ~JOB_DIDOUTPUT;
		for (i = 1; i >= 0; i--) {
			if (FD_ISSET(job->in[i].fd, actual_mask)) {
				nfds--;
				handle_job_output(job, i, false);
			}
		}
		if (job->flags & JOB_DIDOUTPUT) {
			if (wait4(job->pid, &status, WNOHANG|WUNTRACED, NULL) ==
			    job->pid) {
			    	Lst_Remove(&runningJobs, ln);
			    	nJobs--;
				jobFull = false;
				JobFinish(job, status);
			} else {
				Lst_Requeue(&runningJobs, ln);
			}
		}
	}
}

void
handle_running_jobs()
{
	handle_all_jobs_output();
	Job_CatchChildren();
}

static void
loop_handle_running_jobs()
{
	while (nJobs)
		handle_running_jobs();
}
/*-
 *-----------------------------------------------------------------------
 * Job_Make --
 *	Start the creation of a target. Basically a front-end for
 *	JobStart used by the Make module.
 *
 * Side Effects:
 *	Another job is started.
 *-----------------------------------------------------------------------
 */
void
Job_Make(GNode *gn)
{
	(void)JobStart(gn, 0);
}

/*-
 *-----------------------------------------------------------------------
 * Job_Init --
 *	Initialize the process module
 *
 * Side Effects:
 *	lists and counters are initialized
 *-----------------------------------------------------------------------
 */
void
Job_Init(int maxproc)
{
	Static_Lst_Init(&runningJobs);
	Static_Lst_Init(&stoppedJobs);
	Static_Lst_Init(&errorsList);
	maxJobs =	  maxproc;
	nJobs =	  	  0;
	jobFull =	  false;
	errors = 0;

	aborting =	  0;

	lastNode =	  NULL;

	if ((begin_node->type & OP_DUMMY) == 0) {
		JobStart(begin_node, JOB_SPECIAL);
		loop_handle_running_jobs();
	}
}

/*-
 *-----------------------------------------------------------------------
 * Job_Full --
 *	See if the job table is full. It is considered full if it is OR
 *	if we are in the process of aborting OR if we have
 *	reached/exceeded our local quota. This prevents any more jobs
 *	from starting up.
 *
 * Results:
 *	true if the job table is full, false otherwise
 *-----------------------------------------------------------------------
 */
bool
Job_Full(void)
{
	return aborting || jobFull;
}

/*-
 *-----------------------------------------------------------------------
 * Job_Empty --
 *	See if the job table is empty.	Because the local concurrency may
 *	be set to 0, it is possible for the job table to become empty,
 *	while the list of stoppedJobs remains non-empty. In such a case,
 *	we want to restart as many jobs as we can.
 *
 * Results:
 *	true if it is. false if it ain't.
 * -----------------------------------------------------------------------
 */
bool
Job_Empty(void)
{
	if (nJobs == 0) {
		if (!Lst_IsEmpty(&stoppedJobs) && !aborting) {
			/*
			 * The job table is obviously not full if it has no
			 * jobs in it...Try and restart the stopped jobs.
			 */
			jobFull = false;
			JobRestartJobs();
			return false;
		} else {
			return true;
		}
	} else {
		return false;
	}
}

/*-
 *-----------------------------------------------------------------------
 * JobInterrupt --
 *	Handle the receipt of an interrupt.
 *
 * Side Effects:
 *	All children are killed. Another job will be started if the
 *	.INTERRUPT target was given.
 *-----------------------------------------------------------------------
 */
static void
JobInterrupt(int runINTERRUPT,	/* Non-zero if commands for the .INTERRUPT
				 * target should be executed */
    int signo)			/* signal received */
{
	LstNode ln;		/* element in job table */
	Job *job; 		/* job descriptor in that element */

	aborting = ABORT_INTERRUPT;

	for (ln = Lst_First(&runningJobs); ln != NULL; ln = Lst_Adv(ln)) {
		job = (Job *)Lst_Datum(ln);

		if (!Targ_Precious(job->node)) {
			const char *file = job->node->path == NULL ?
			    job->node->name : job->node->path;
			if (!noExecute && eunlink(file) != -1) {
				Error("*** %s removed", file);
			}
		}
		if (job->pid) {
			debug_printf("JobInterrupt passing signal to "
			    "child %ld.\n", (long)job->pid);
			KILL(job->pid, signo);
		}
	}

	if (runINTERRUPT && !touchFlag) {
		if ((interrupt_node->type & OP_DUMMY) == 0) {
			ignoreErrors = false;

			JobStart(interrupt_node, 0);
			loop_handle_running_jobs();
		}
	}
	exit(signo);
}

/*
 *-----------------------------------------------------------------------
 * Job_Finish --
 *	Do final processing such as the running of the commands
 *	attached to the .END target.
 *
 * Results:
 *	Number of errors reported.
 *
 *-----------------------------------------------------------------------
 */
int
Job_Finish(void)
{
	if ((end_node->type & OP_DUMMY) == 0) {
		if (errors) {
			Error("Errors reported so .END ignored");
		} else {
			JobStart(end_node, JOB_SPECIAL);
			loop_handle_running_jobs();
		}
	}
	return errors;
}

#ifdef CLEANUP
void
Job_End(void)
{
}
#endif

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
 *	error. Most definitely NOT to be called from JobInterrupt.
 *
 * Side Effects:
 *	All children are killed, not just the firstborn
 *-----------------------------------------------------------------------
 */
void
Job_AbortAll(void)
{
	LstNode ln;	/* element in job table */
	Job *job;	/* the job descriptor in that element */
	int foo;

	aborting = ABORT_ERROR;

	if (nJobs) {
		for (ln = Lst_First(&runningJobs); ln != NULL; 
		    ln = Lst_Adv(ln)) {
			job = (Job *)Lst_Datum(ln);

			/*
			 * kill the child process with increasingly drastic
			 * signals to make darn sure it's dead.
			 */
			KILL(job->pid, SIGINT);
			KILL(job->pid, SIGKILL);
		}
	}

	/*
	 * Catch as many children as want to report in at first, then give up
	 */
	while (waitpid(-1, &foo, WNOHANG) > 0)
		continue;
}

/*-
 *-----------------------------------------------------------------------
 * JobRestartJobs --
 *	Tries to restart stopped jobs if there are slots available.
 *	Note that this tries to restart them regardless of pending errors.
 *	It's not good to leave stopped jobs lying around!
 *
 * Side Effects:
 *	Resumes(and possibly migrates) jobs.
 *-----------------------------------------------------------------------
 */
static void
JobRestartJobs(void)
{
	Job *job;

	while (!Job_Full() && 
	    (job = (Job *)Lst_DeQueue(&stoppedJobs)) != NULL) {
		debug_printf("Job queue is not full. "
		    "Restarting a stopped job.\n");
		JobRestart(job);
	}
}
