/*	$OpenPackages$ */
/*	$OpenBSD: job.c,v 1.52 2002/07/31 19:29:20 mickey Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	Job_CatchChildren	Check for and handle the termination of any
 *				children. This must be called reasonably
 *				frequently to keep the whole make going at
 *				a decent clip, since job table entries aren't
 *				removed until their process is caught this way.
 *				Its single argument is true if the function
 *				should block waiting for a child to terminate.
 *
 *	Job_CatchOutput 	Print any output our children have produced.
 *				Should also be called fairly frequently to
 *				keep the user informed of what's going on.
 *				If no output is waiting, it will block for
 *				a time given by the SEL_* constants, below,
 *				or until output is ready.
 *
 *	Job_Init		Called to intialize this module. in addition,
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
 *	Job_ParseShell		Given the line following a .SHELL target, parse
 *				the line as a shell specification. Returns
 *				false if the spec was incorrect.
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
 *	Job_CheckCommands	Verify that the commands for a target are
 *				ok. Provide them if necessary and possible.
 *
 *	Job_Touch		Update a target without really updating it.
 *
 *	Job_Wait		Wait for all currently-running jobs to finish.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include "defines.h"
#include "dir.h"
#include "job.h"
#include "pathnames.h"
#include "arch.h"
#include "var.h"
#include "targ.h"
#include "error.h"
#include "str.h"
#include "lst.h"
#include "extern.h"
#include "gnode.h"
#include "memory.h"
#include "make.h"
#include "timestamp.h"
#include "main.h"

#define TMPPAT	"/tmp/makeXXXXXXXXXX"

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
 *	3) A LstNode for the first command to be saved after the job
 *	   completes. This is NULL if there was no "..." in the job's
 *	   commands.
 *	4) An FILE* for writing out the commands. This is only
 *	   used before the job is actually started.
 *	5) A union of things used for handling the shell's output. Different
 *	   parts of the union are used based on the value of the usePipes
 *	   flag. If it is true, the output is being caught via a pipe and
 *	   the descriptors of our pipe, an array in which output is line
 *	   buffered and the current position in that buffer are all
 *	   maintained for each job. If, on the other hand, usePipes is false,
 *	   the output is routed to a temporary file and all that is kept
 *	   is the name of the file and the descriptor open to the file.
 *	6) An identifier provided by and for the exclusive use of the
 *	   Rmt module.
 *	7) A word of flags which determine how the module handles errors,
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
typedef struct Job_ {
    pid_t 	pid;	    /* The child's process ID */
    GNode	*node;	    /* The target the child is making */
    LstNode	tailCmds;   /* The node of the first command to be
			     * saved when the job has been run */
    FILE	*cmdFILE;   /* When creating the shell script, this is
			     * where the commands go */
    int 	rmtID;	   /* ID returned from Rmt module */
    short	flags;	    /* Flags to control treatment of job */
#define JOB_IGNERR	0x001	/* Ignore non-zero exits */
#define JOB_SILENT	0x002	/* no output */
#define JOB_SPECIAL	0x004	/* Target is a special one. i.e. run it locally
				 * if we can't export it and maxLocal is 0 */
#define JOB_IGNDOTS	0x008	/* Ignore "..." lines when processing
				 * commands */
#define JOB_FIRST	0x020	/* Job is first job for the node */
#define JOB_RESTART	0x080	/* Job needs to be completely restarted */
#define JOB_RESUME	0x100	/* Job needs to be resumed b/c it stopped,
				 * for some reason */
#define JOB_CONTINUING	0x200	/* We are in the process of resuming this job.
				 * Used to avoid infinite recursion between
				 * JobFinish and JobRestart */
    union {
	struct {
	    int 	op_inPipe;	/* Input side of pipe associated
					 * with job's output channel */
	    int 	op_outPipe;	/* Output side of pipe associated with
					 * job's output channel */
	    char	op_outBuf[JOB_BUFSIZE + 1];
					/* Buffer for storing the output of the
					 * job, line by line */
	    int 	op_curPos;	/* Current position in op_outBuf */
	}	    o_pipe;	    /* data used when catching the output via
				     * a pipe */
	struct {
	    char	of_outFile[sizeof(TMPPAT)];
					/* Name of file to which shell output
					 * was rerouted */
	    int 	of_outFd;	/* Stream open to the output
					 * file. Used to funnel all
					 * from a single job to one file
					 * while still allowing
					 * multiple shell invocations */
	}	    o_file;	    /* Data used when catching the output in
				     * a temporary file */
    }		output;     /* Data for tracking a shell's output */
} Job;

#define outPipe 	output.o_pipe.op_outPipe
#define inPipe		output.o_pipe.op_inPipe
#define outBuf		output.o_pipe.op_outBuf
#define curPos		output.o_pipe.op_curPos
#define outFile 	output.o_file.of_outFile
#define outFd		output.o_file.of_outFd


/*-
 * Shell Specifications:
 * Each shell type has associated with it the following information:
 *	1) The string which must match the last character of the shell name
 *	   for the shell to be considered of this type. The longest match
 *	   wins.
 *	2) A command to issue to turn off echoing of command lines
 *	3) A command to issue to turn echoing back on again
 *	4) What the shell prints, and its length, when given the echo-off
 *	   command. This line will not be printed when received from the shell
 *	5) A boolean to tell if the shell has the ability to control
 *	   error checking for individual commands.
 *	6) The string to turn this checking on.
 *	7) The string to turn it off.
 *	8) The command-flag to give to cause the shell to start echoing
 *	   commands right away.
 *	9) The command-flag to cause the shell to Lib_Exit when an error is
 *	   detected in one of the commands.
 *
 * Some special stuff goes on if a shell doesn't have error control. In such
 * a case, errCheck becomes a printf template for echoing the command,
 * should echoing be on and ignErr becomes another printf template for
 * executing the command while ignoring the return status. If either of these
 * strings is empty when hasErrCtl is false, the command will be executed
 * anyway as is and if it causes an error, so be it.
 */
typedef struct Shell_ {
    char	  *name;	/* the name of the shell. For Bourne and C
				 * shells, this is used only to find the
				 * shell description when used as the single
				 * source of a .SHELL target. For user-defined
				 * shells, this is the full path of the shell.
				 */
    bool	  hasEchoCtl;	/* True if both echoOff and echoOn defined */
    char	  *echoOff;	/* command to turn off echo */
    char	  *echoOn;	/* command to turn it back on again */
    char	  *noPrint;	/* command to skip when printing output from
				 * shell. This is usually the command which
				 * was executed to turn off echoing */
    int 	  noPLen;	/* length of noPrint command */
    bool	  hasErrCtl;	/* set if can control error checking for
				 * individual commands */
    char	  *errCheck;	/* string to turn error checking on */
    char	  *ignErr;	/* string to turn off error checking */
    /*
     * command-line flags
     */
    char	  *echo;	/* echo commands */
    char	  *exit;	/* exit on error */
}		Shell;

/*
 * error handling variables
 */
static int	errors = 0;	    /* number of errors reported */
static int	aborting = 0;	    /* why is the make aborting? */
#define ABORT_ERROR	1	    /* Because of an error */
#define ABORT_INTERRUPT 2	    /* Because it was interrupted */
#define ABORT_WAIT	3	    /* Waiting for jobs to finish */

/*
 * XXX: Avoid SunOS bug... FILENO() is fp->_file, and file
 * is a char! So when we go above 127 we turn negative!
 */
#define FILENO(a) ((unsigned) fileno(a))

/*
 * post-make command processing. The node postCommands is really just the
 * .END target but we keep it around to avoid having to search for it
 * all the time.
 */
static GNode	  *postCommands;    /* node containing commands to execute when
				     * everything else is done */
static int	  numCommands;	    /* The number of commands actually printed
				     * for a target. Should this number be
				     * 0, no shell will be executed. */

/*
 * Return values from JobStart.
 */
#define JOB_RUNNING	0	/* Job is running */
#define JOB_ERROR	1	/* Error in starting the job */
#define JOB_FINISHED	2	/* The job is already finished */
#define JOB_STOPPED	3	/* The job is stopped */

/*
 * tfile is the name of a file into which all shell commands are put. It is
 * used over by removing it before the child shell is executed. The XXXXXXXXXX
 * in the string are replaced by mkstemp(3).
 */
static char	tfile[sizeof(TMPPAT)];


/*
 * Descriptions for various shells.
 */
static Shell	shells[] = {
    /*
     * CSH description. The csh can do echo control by playing
     * with the setting of the 'echo' shell variable. Sadly,
     * however, it is unable to do error control nicely.
     */
{
    "csh",
    true, "unset verbose", "set verbose", "unset verbose", 10,
    false, "echo \"%s\"\n", "csh -c \"%s || exit 0\"",
    "v", "e",
},
    /*
     * SH description. Echo control is also possible and, under
     * sun UNIX anyway, one can even control error checking.
     */
{
    "sh",
    true, "set -", "set -v", "set -", 5,
    true, "set -e", "set +e",
#ifdef OLDBOURNESHELL
    false, "echo \"%s\"\n", "sh -c '%s || exit 0'\n",
#endif
    "v", "e",
},
    /*
     * UNKNOWN.
     */
{
    (char *)0,
    false, (char *)0, (char *)0, (char *)0, 0,
    false, (char *)0, (char *)0,
    (char *)0, (char *)0,
}
};
static Shell	*commandShell = &shells[DEFSHELL];/* this is the shell to
						   * which we pass all
						   * commands in the Makefile.
						   * It is set by the
						   * Job_ParseShell function */
static char	*shellPath = NULL,		  /* full pathname of
						   * executable image */
		*shellName = NULL,		  /* last component of shell */
		*shellArgv = NULL;		  /* Custom shell args */


static int	maxJobs;	/* The most children we can run at once */
static int	maxLocal;	/* The most local ones we can have */
static int	nJobs = 0;	/* The number of children currently running */
static int	nLocal; 	/* The number of local children */
static LIST	jobs;		/* The structures that describe them */
static bool	jobFull;	/* Flag to tell when the job table is full. It
				 * is set true when (1) the total number of
				 * running jobs equals the maximum allowed or
				 * (2) a job can only be run locally, but
				 * nLocal equals maxLocal */
static fd_set	*outputsp;	/* Set of descriptors of pipes connected to
				 * the output channels of children */
static int	outputsn;
static GNode	*lastNode;	/* The node for which output was most recently
				 * produced. */
static char	*targFmt;	/* Format string to use to head output from a
				 * job when it's not the most-recent job heard
				 * from */

# define TARG_FMT  "--- %s ---\n" /* Default format */
# define MESSAGE(fp, gn) \
	(void)fprintf(fp, targFmt, gn->name);

/*
 * When JobStart attempts to run a job but isn't allowed to,
 * the job is placed on the stoppedJobs queue to be run
 * when the next job finishes.
 */
static LIST	stoppedJobs;	/* Lst of Job structures describing
				 * jobs that were stopped due to concurrency
				 * limits or migration home */


#if defined(USE_PGRP) && defined(SYSV)
# define KILL(pid, sig) 	killpg(-(pid), (sig))
#else
# if defined(USE_PGRP)
#  define KILL(pid, sig)	killpg((pid), (sig))
# else
#  define KILL(pid, sig)	kill((pid), (sig))
# endif
#endif

/*
 * Grmpf... There is no way to set bits of the wait structure
 * anymore with the stupid W*() macros. I liked the union wait
 * stuff much more. So, we devise our own macros... This is
 * really ugly, use dramamine sparingly. You have been warned.
 */
#define W_SETMASKED(st, val, fun)				\
	{							\
		int sh = (int) ~0;				\
		int mask = fun(sh);				\
								\
		for (sh = 0; ((mask >> sh) & 1) == 0; sh++)	\
			continue;				\
		*(st) = (*(st) & ~mask) | ((val) << sh);	\
	}

#define W_SETTERMSIG(st, val) W_SETMASKED(st, val, WTERMSIG)
#define W_SETEXITSTATUS(st, val) W_SETMASKED(st, val, WEXITSTATUS)


static void JobCondPassSig(void *, void *);
static void JobPassSig(int);
static int JobCmpPid(void *, void *);
static int JobPrintCommand(void *, void *);
static void JobSaveCommand(void *, void *);
static void JobClose(Job *);
static void JobFinish(Job *, int *);
static void JobExec(Job *, char **);
static void JobMakeArgv(Job *, char **);
static void JobRestart(Job *);
static int JobStart(GNode *, int, Job *);
static char *JobOutput(Job *, char *, char *, int);
static void JobDoOutput(Job *, bool);
static Shell *JobMatchShell(char *);
static void JobInterrupt(int, int);
static void JobRestartJobs(void);

/*-
 *-----------------------------------------------------------------------
 * JobCondPassSig --
 *	Pass a signal to a job if USE_PGRP
 *	is defined.
 *
 * Side Effects:
 *	None, except the job may bite it.
 *-----------------------------------------------------------------------
 */
static void
JobCondPassSig(jobp, signop)
    void		*jobp;		/* Job to biff */
    void		*signop;	/* Signal to send it */
{
    Job *job = (Job *)jobp;
    int signo = *(int *)signop;
    if (DEBUG(JOB)) {
	(void)fprintf(stdout,
		       "JobCondPassSig passing signal %d to child %ld.\n",
		       signo, (long)job->pid);
	(void)fflush(stdout);
    }
    KILL(job->pid, signo);
}

/*-
 *-----------------------------------------------------------------------
 * JobPassSig --
 *	Pass a signal to all local jobs if USE_PGRP is defined, 
 *	then die ourselves.
 *
 * Side Effects:
 *	We die by the same signal.
 *-----------------------------------------------------------------------
 */
static void
JobPassSig(signo)
    int     signo;	/* The signal number we've received */
{
    int save_errno = errno;
    sigset_t nmask, omask;
    struct sigaction act;

    if (DEBUG(JOB)) {
	(void)fprintf(stdout, "JobPassSig(%d) called.\n", signo);
	(void)fflush(stdout);
    }
    Lst_ForEach(&jobs, JobCondPassSig, &signo);

    /*
     * Deal with proper cleanup based on the signal received. We only run
     * the .INTERRUPT target if the signal was in fact an interrupt. The other
     * three termination signals are more of a "get out *now*" command.
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
     * Send ourselves the signal now we've given the message to everyone else.
     * Note we block everything else possible while we're getting the signal.
     * This ensures that all our jobs get continued when we wake up before
     * we take any other signal.
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
		       "JobPassSig passing signal to self, mask = %x.\n",
		       ~0 & ~(1 << (signo-1)));
	(void)fflush(stdout);
    }
    (void)signal(signo, SIG_DFL);

    (void)KILL(getpid(), signo);

    signo = SIGCONT;
    Lst_ForEach(&jobs, JobCondPassSig, &signo);

    (void)sigprocmask(SIG_SETMASK, &omask, NULL);
    sigprocmask(SIG_SETMASK, &omask, NULL);
    act.sa_handler = JobPassSig;
    sigaction(signo, &act, NULL);
    errno = save_errno;
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
JobCmpPid(job, pid)
    void *job;	/* job to examine */
    void *pid;	/* process id desired */
{
    return *(pid_t *)pid - ((Job *)job)->pid;
}

/*-
 *-----------------------------------------------------------------------
 * JobPrintCommand  --
 *	Put out another command for the given job. If the command starts
 *	with an @ or a - we process it specially. In the former case,
 *	so long as the -s and -n flags weren't given to make, we stick
 *	a shell-specific echoOff command in the script. In the latter,
 *	we ignore errors for the entire job, unless the shell has error
 *	control.
 *	If the command is just "..." we take all future commands for this
 *	job to be commands to be executed once the entire graph has been
 *	made and return non-zero to signal that the end of the commands
 *	was reached. These commands are later attached to the postCommands
 *	node and executed by Job_End when all things are done.
 *	This function is called from JobStart via Lst_Find
 *
 * Results:
 *	Always 1, unless the command was "..."
 *
 * Side Effects:
 *	If the command begins with a '-' and the shell has no error control,
 *	the JOB_IGNERR flag is set in the job descriptor.
 *	If the command is "..." and we're not ignoring such things,
 *	tailCmds is set to the successor node of the cmd.
 *	numCommands is incremented if the command is actually printed.
 *-----------------------------------------------------------------------
 */
static int
JobPrintCommand(cmdp, jobp)
    void	*cmdp;		    /* command string to print */
    void	*jobp;		    /* job for which to print it */
{
    bool	  noSpecials;	    /* true if we shouldn't worry about
				     * inserting special commands into
				     * the input stream. */
    bool	  shutUp = false;   /* true if we put a no echo command
				     * into the command file */
    bool	  errOff = false;   /* true if we turned error checking
				     * off before printing the command
				     * and need to turn it back on */
    char	  *cmdTemplate;     /* Template to use when printing the
				     * command */
    char	  *cmdStart;	    /* Start of expanded command */
    LstNode	  cmdNode;	    /* Node for replacing the command */
    char	  *cmd = (char *)cmdp;
    Job 	  *job = (Job *)jobp;

    noSpecials = (noExecute && !(job->node->type & OP_MAKE));

    if (strcmp(cmd, "...") == 0) {
	job->node->type |= OP_SAVE_CMDS;
	if ((job->flags & JOB_IGNDOTS) == 0) {
	    job->tailCmds = Lst_Succ(Lst_Member(&job->node->commands, cmd));
	    return 0;
	}
	return 1;
    }

#define DBPRINTF(fmt, arg) if (DEBUG(JOB)) {	\
	(void)fprintf(stdout, fmt, arg);	\
	(void)fflush(stdout);			\
    }						\
   (void)fprintf(job->cmdFILE, fmt, arg);	\
   (void)fflush(job->cmdFILE);

    numCommands += 1;

    /* For debugging, we replace each command with the result of expanding
     * the variables in the command.  */
    cmdNode = Lst_Member(&job->node->commands, cmd);
    cmdStart = cmd = Var_Subst(cmd, &job->node->context, false);
    Lst_Replace(cmdNode, cmdStart);

    cmdTemplate = "%s\n";

    /*
     * Check for leading @' and -'s to control echoing and error checking.
     */
    for (;; cmd++) {
	if (*cmd == '@')
	    shutUp = DEBUG(LOUD) ? false : true;
	else if (*cmd == '-')
	    errOff = true;
	else if (*cmd != '+')
	    break;
    }

    while (isspace(*cmd))
	cmd++;

    if (shutUp) {
	if (!(job->flags & JOB_SILENT) && !noSpecials &&
	    commandShell->hasEchoCtl) {
		DBPRINTF("%s\n", commandShell->echoOff);
	} else {
	    shutUp = false;
	}
    }

    if (errOff) {
	if ( !(job->flags & JOB_IGNERR) && !noSpecials) {
	    if (commandShell->hasErrCtl) {
		/*
		 * we don't want the error-control commands showing
		 * up either, so we turn off echoing while executing
		 * them. We could put another field in the shell
		 * structure to tell JobDoOutput to look for this
		 * string too, but why make it any more complex than
		 * it already is?
		 */
		if (!(job->flags & JOB_SILENT) && !shutUp &&
		    commandShell->hasEchoCtl) {
			DBPRINTF("%s\n", commandShell->echoOff);
			DBPRINTF("%s\n", commandShell->ignErr);
			DBPRINTF("%s\n", commandShell->echoOn);
		} else {
		    DBPRINTF("%s\n", commandShell->ignErr);
		}
	    } else if (commandShell->ignErr &&
		      (*commandShell->ignErr != '\0'))
	    {
		/*
		 * The shell has no error control, so we need to be
		 * weird to get it to ignore any errors from the command.
		 * If echoing is turned on, we turn it off and use the
		 * errCheck template to echo the command. Leave echoing
		 * off so the user doesn't see the weirdness we go through
		 * to ignore errors. Set cmdTemplate to use the weirdness
		 * instead of the simple "%s\n" template.
		 */
		if (!(job->flags & JOB_SILENT) && !shutUp &&
		    commandShell->hasEchoCtl) {
			DBPRINTF("%s\n", commandShell->echoOff);
			DBPRINTF(commandShell->errCheck, cmd);
			shutUp = true;
		}
		cmdTemplate = commandShell->ignErr;
		/*
		 * The error ignoration (hee hee) is already taken care
		 * of by the ignErr template, so pretend error checking
		 * is still on.
		 */
		errOff = false;
	    } else {
		errOff = false;
	    }
	} else {
	    errOff = false;
	}
    }

    DBPRINTF(cmdTemplate, cmd);

    if (errOff) {
	/*
	 * If echoing is already off, there's no point in issuing the
	 * echoOff command. Otherwise we issue it and pretend it was on
	 * for the whole command...
	 */
	if (!shutUp && !(job->flags & JOB_SILENT) && commandShell->hasEchoCtl){
	    DBPRINTF("%s\n", commandShell->echoOff);
	    shutUp = true;
	}
	DBPRINTF("%s\n", commandShell->errCheck);
    }
    if (shutUp) {
	DBPRINTF("%s\n", commandShell->echoOn);
    }
    return 1;
}

/*-
 *-----------------------------------------------------------------------
 * JobSaveCommand --
 *	Save a command to be executed when everything else is done.
 *	Callback function for JobFinish...
 *
 * Side Effects:
 *	The command is tacked onto the end of postCommands's commands list.
 *-----------------------------------------------------------------------
 */
static void
JobSaveCommand(cmd, gn)
    void	*cmd;
    void	*gn;
{
    GNode	*g = (GNode *)gn;
    char	*result;

    result = Var_Subst((char *)cmd, &g->context, false);
    Lst_AtEnd(&postCommands->commands, result);
}


/*-
 *-----------------------------------------------------------------------
 * JobClose --
 *	Called to close both input and output pipes when a job is finished.
 *
 * Side Effects:
 *	The file descriptors associated with the job are closed.
 *-----------------------------------------------------------------------
 */
static void
JobClose(job)
    Job *job;
{
    if (usePipes) {
	FD_CLR(job->inPipe, outputsp);
	if (job->outPipe != job->inPipe) {
	   (void)close(job->outPipe);
	}
	JobDoOutput(job, true);
	(void)close(job->inPipe);
    } else {
	(void)close(job->outFd);
	JobDoOutput(job, true);
    }
}

/*-
 *-----------------------------------------------------------------------
 * JobFinish  --
 *	Do final processing for the given job including updating
 *	parents and starting new jobs as available/necessary. Note
 *	that we pay no attention to the JOB_IGNERR flag here.
 *	This is because when we're called because of a noexecute flag
 *	or something, jstat.w_status is 0 and when called from
 *	Job_CatchChildren, the status is zeroed if it s/b ignored.
 *
 * Side Effects:
 *	Some nodes may be put on the toBeMade queue.
 *	Final commands for the job are placed on postCommands.
 *
 *	If we got an error and are aborting (aborting == ABORT_ERROR) and
 *	the job list is now empty, we are done for the day.
 *	If we recognized an error (errors !=0), we set the aborting flag
 *	to ABORT_ERROR so no more jobs will be started.
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
JobFinish(job, status)
    Job 	*job;		  /* job to finish */
    int 	*status;	  /* sub-why job went away */
{
    bool	 done;

    if ((WIFEXITED(*status) &&
	 WEXITSTATUS(*status) != 0 && !(job->flags & JOB_IGNERR)) ||
	(WIFSIGNALED(*status) && WTERMSIG(*status) != SIGCONT))
    {
	/*
	 * If it exited non-zero and either we're doing things our
	 * way or we're not ignoring errors, the job is finished.
	 * Similarly, if the shell died because of a signal
	 * the job is also finished. In these
	 * cases, finish out the job's output before printing the exit
	 * status...
	 */
	JobClose(job);
	if (job->cmdFILE != NULL && job->cmdFILE != stdout) {
	   (void)fclose(job->cmdFILE);
	}
	done = true;
    } else if (WIFEXITED(*status)) {
	/*
	 * Deal with ignored errors in -B mode. We need to print a message
	 * telling of the ignored error as well as setting status.w_status
	 * to 0 so the next command gets run. To do this, we set done to be
	 * true if in -B mode and the job exited non-zero.
	 */
	done = WEXITSTATUS(*status) != 0;
	/*
	 * Old comment said: "Note we don't
	 * want to close down any of the streams until we know we're at the
	 * end."
	 * But we do. Otherwise when are we going to print the rest of the
	 * stuff?
	 */
	JobClose(job);
    } else {
	/*
	 * No need to close things down or anything.
	 */
	done = false;
    }

    if (done ||
	WIFSTOPPED(*status) ||
	(WIFSIGNALED(*status) && WTERMSIG(*status) == SIGCONT) ||
	DEBUG(JOB))
    {
	FILE	  *out;

	if (compatMake && !usePipes && (job->flags & JOB_IGNERR)) {
	    /*
	     * If output is going to a file and this job is ignoring
	     * errors, arrange to have the exit status sent to the
	     * output file as well.
	     */
	    out = fdopen(job->outFd, "w");
	} else {
	    out = stdout;
	}

	if (WIFEXITED(*status)) {
	    if (DEBUG(JOB)) {
		(void)fprintf(stdout, "Process %ld exited.\n", (long)job->pid);
		(void)fflush(stdout);
	    }
	    if (WEXITSTATUS(*status) != 0) {
		if (usePipes && job->node != lastNode) {
		    MESSAGE(out, job->node);
		    lastNode = job->node;
		}
		(void)fprintf(out, "*** Error code %d%s\n",
			       WEXITSTATUS(*status),
			       (job->flags & JOB_IGNERR) ? "(ignored)" : "");

		if (job->flags & JOB_IGNERR) {
		    *status = 0;
		}
	    } else if (DEBUG(JOB)) {
		if (usePipes && job->node != lastNode) {
		    MESSAGE(out, job->node);
		    lastNode = job->node;
		}
		(void)fprintf(out, "*** Completed successfully\n");
	    }
	} else if (WIFSTOPPED(*status)) {
	    if (DEBUG(JOB)) {
		(void)fprintf(stdout, "Process %ld stopped.\n", (long)job->pid);
		(void)fflush(stdout);
	    }
	    if (usePipes && job->node != lastNode) {
		MESSAGE(out, job->node);
		lastNode = job->node;
	    }
	    (void)fprintf(out, "*** Stopped -- signal %d\n",
		WSTOPSIG(*status));
	    job->flags |= JOB_RESUME;
	    Lst_AtEnd(&stoppedJobs, job);
	    (void)fflush(out);
	    return;
	} else if (WTERMSIG(*status) == SIGCONT) {
	    /*
	     * If the beastie has continued, shift the Job from the stopped
	     * list to the running one (or re-stop it if concurrency is
	     * exceeded) and go and get another child.
	     */
	    if (job->flags & (JOB_RESUME|JOB_RESTART)) {
		if (usePipes && job->node != lastNode) {
		    MESSAGE(out, job->node);
		    lastNode = job->node;
		}
		(void)fprintf(out, "*** Continued\n");
	    }
	    if (!(job->flags & JOB_CONTINUING)) {
		if (DEBUG(JOB)) {
		    (void)fprintf(stdout,
				   "Warning: process %ld was not continuing.\n",
				   (long)job->pid);
		    (void)fflush(stdout);
		}
#ifdef notdef
		/*
		 * We don't really want to restart a job from scratch just
		 * because it continued, especially not without killing the
		 * continuing process!	That's why this is ifdef'ed out.
		 * FD - 9/17/90
		 */
		JobRestart(job);
#endif
	    }
	    job->flags &= ~JOB_CONTINUING;
	    Lst_AtEnd(&jobs, job);
	    nJobs += 1;
	    if (DEBUG(JOB)) {
		(void)fprintf(stdout,
			       "Process %ld is continuing locally.\n",
			       (long)job->pid);
		(void)fflush(stdout);
	    }
	    nLocal += 1;
	    if (nJobs == maxJobs) {
		jobFull = true;
		if (DEBUG(JOB)) {
		    (void)fprintf(stdout, "Job queue is full.\n");
		    (void)fflush(stdout);
		}
	    }
	    (void)fflush(out);
	    return;
	} else {
	    if (usePipes && job->node != lastNode) {
		MESSAGE(out, job->node);
		lastNode = job->node;
	    }
	    (void)fprintf(out, "*** Signal %d\n", WTERMSIG(*status));
	}

	(void)fflush(out);
    }

    /*
     * Now handle the -B-mode stuff. If the beast still isn't finished,
     * try and restart the job on the next command. If JobStart says it's
     * ok, it's ok. If there's an error, this puppy is done.
     */
    if (compatMake && WIFEXITED(*status) && job->node->current != NULL) {
	switch (JobStart(job->node, job->flags & JOB_IGNDOTS, job)) {
	case JOB_RUNNING:
	    done = false;
	    break;
	case JOB_ERROR:
	    done = true;
	    W_SETEXITSTATUS(status, 1);
	    break;
	case JOB_FINISHED:
	    /*
	     * If we got back a JOB_FINISHED code, JobStart has already
	     * called Make_Update and freed the job descriptor. We set
	     * done to false here to avoid fake cycles and double frees.
	     * JobStart needs to do the update so we can proceed up the
	     * graph when given the -n flag..
	     */
	    done = false;
	    break;
	}
    } else
	done = true;

    if (done &&
	aborting != ABORT_ERROR &&
	aborting != ABORT_INTERRUPT &&
	*status == 0) {
	/* As long as we aren't aborting and the job didn't return a non-zero
	 * status that we shouldn't ignore, we call Make_Update to update
	 * the parents. In addition, any saved commands for the node are placed
	 * on the .END target.	*/
	Lst_ForEachFrom(job->tailCmds, JobSaveCommand, job->node);
	job->node->made = MADE;
	Make_Update(job->node);
	free(job);
    } else if (*status != 0) {
	errors += 1;
	free(job);
    }

    JobRestartJobs();

    /*
     * Set aborting if any error.
     */
    if (errors && !keepgoing && aborting != ABORT_INTERRUPT) {
	/*
	 * If we found any errors in this batch of children and the -k flag
	 * wasn't given, we set the aborting flag so no more jobs get
	 * started.
	 */
	aborting = ABORT_ERROR;
    }

    if (aborting == ABORT_ERROR && Job_Empty()) {
	/*
	 * If we are aborting and the job table is now empty, we finish.
	 */
	(void)eunlink(tfile);
	Finish(errors);
    }
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
Job_Touch(gn, silent)
    GNode	  *gn;		/* the node of the file to touch */
    bool	  silent;	/* true if should not print messages */
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
Job_CheckCommands(gn, abortProc)
    GNode	   *gn; 	    /* The target whose commands need
				     * verifying */
    void	 (*abortProc)(char *, ...);
			/* Function to abort with message */
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
JobExec(job, argv)
    Job 	  *job; 	/* Job to execute */
    char	  **argv;
{
    pid_t 	  cpid; 	/* ID of new child */

    if (DEBUG(JOB)) {
	int	  i;

	(void)fprintf(stdout, "Running %s\n", job->node->name);
	(void)fprintf(stdout, "\tCommand: ");
	for (i = 0; argv[i] != NULL; i++) {
	    (void)fprintf(stdout, "%s ", argv[i]);
	}
	(void)fprintf(stdout, "\n");
	(void)fflush(stdout);
    }

    /*
     * Some jobs produce no output and it's disconcerting to have
     * no feedback of their running (since they produce no output, the
     * banner with their name in it never appears). This is an attempt to
     * provide that feedback, even if nothing follows it.
     */
    if (lastNode != job->node && (job->flags & JOB_FIRST) &&
	!(job->flags & JOB_SILENT)) {
	MESSAGE(stdout, job->node);
	lastNode = job->node;
    }

    if ((cpid = fork()) == -1) {
	Punt("Cannot fork");
    } else if (cpid == 0) {

	/*
	 * Must duplicate the input stream down to the child's input and
	 * reset it to the beginning (again). Since the stream was marked
	 * close-on-exec, we must clear that bit in the new input.
	 */
	if (dup2(FILENO(job->cmdFILE), 0) == -1)
	    Punt("Cannot dup2: %s", strerror(errno));
	(void)fcntl(0, F_SETFD, 0);
	(void)lseek(0, 0, SEEK_SET);

	if (usePipes) {
	    /*
	     * Set up the child's output to be routed through the pipe
	     * we've created for it.
	     */
	    if (dup2(job->outPipe, 1) == -1)
		Punt("Cannot dup2: %s", strerror(errno));
	} else {
	    /*
	     * We're capturing output in a file, so we duplicate the
	     * descriptor to the temporary file into the standard
	     * output.
	     */
	    if (dup2(job->outFd, 1) == -1)
		Punt("Cannot dup2: %s", strerror(errno));
	}
	/*
	 * The output channels are marked close on exec. This bit was
	 * duplicated by the dup2 (on some systems), so we have to clear
	 * it before routing the shell's error output to the same place as
	 * its standard output.
	 */
	(void)fcntl(1, F_SETFD, 0);
	if (dup2(1, 2) == -1)
	    Punt("Cannot dup2: %s", strerror(errno));

#ifdef USE_PGRP
	/*
	 * We want to switch the child into a different process family so
	 * we can kill it and all its descendants in one fell swoop,
	 * by killing its process family, but not commit suicide.
	 */
# if defined(SYSV)
	(void)setsid();
# else
	(void)setpgid(0, getpid());
# endif
#endif /* USE_PGRP */

	   (void)execv(shellPath, argv);

	(void)write(2, "Could not execute shell\n",
		     sizeof("Could not execute shell"));
	_exit(1);
    } else {
	job->pid = cpid;

	if (usePipes && (job->flags & JOB_FIRST) ) {
	    /*
	     * The first time a job is run for a node, we set the current
	     * position in the buffer to the beginning and mark another
	     * stream to watch in the outputs mask
	     */
	    job->curPos = 0;

	    if (outputsp == NULL || job->inPipe > outputsn) {
		int bytes = howmany(job->inPipe+1, NFDBITS) * sizeof(fd_mask);
		int obytes = howmany(outputsn+1, NFDBITS) * sizeof(fd_mask);

		if (outputsp == NULL || obytes != bytes) {
			outputsp = realloc(outputsp, bytes);
			if (outputsp == NULL)
				return;
			memset(outputsp + obytes, 0, bytes - obytes);
		}
		outputsn = job->inPipe;
	    }
	    FD_SET(job->inPipe, outputsp);
	}

	nLocal += 1;
	/*
	 * XXX: Used to not happen if REMOTE. Why?
	 */
	if (job->cmdFILE != NULL && job->cmdFILE != stdout) {
	    (void)fclose(job->cmdFILE);
	    job->cmdFILE = NULL;
	}
    }

    /*
     * Now the job is actually running, add it to the table.
     */
    nJobs += 1;
    Lst_AtEnd(&jobs, job);
    if (nJobs == maxJobs) {
	jobFull = true;
    }
}

/*-
 *-----------------------------------------------------------------------
 * JobMakeArgv --
 *	Create the argv needed to execute the shell for a given job.
 *-----------------------------------------------------------------------
 */
static void
JobMakeArgv(job, argv)
    Job 	  *job;
    char	  **argv;
{
    int 	  argc;
    static char   args[10];	/* For merged arguments */

    argv[0] = shellName;
    argc = 1;

    if ((commandShell->exit && *commandShell->exit != '-') ||
	(commandShell->echo && *commandShell->echo != '-'))
    {
	/*
	 * At least one of the flags doesn't have a minus before it, so
	 * merge them together. Have to do this because the *(&(@*#*&#$#
	 * Bourne shell thinks its second argument is a file to source.
	 * Grrrr. Note the ten-character limitation on the combined arguments.
	 */
	(void)sprintf(args, "-%s%s",
		      ((job->flags & JOB_IGNERR) ? "" :
		       (commandShell->exit ? commandShell->exit : "")),
		      ((job->flags & JOB_SILENT) ? "" :
		       (commandShell->echo ? commandShell->echo : "")));

	if (args[1]) {
	    argv[argc] = args;
	    argc++;
	}
    } else {
	if (!(job->flags & JOB_IGNERR) && commandShell->exit) {
	    argv[argc] = commandShell->exit;
	    argc++;
	}
	if (!(job->flags & JOB_SILENT) && commandShell->echo) {
	    argv[argc] = commandShell->echo;
	    argc++;
	}
    }
    argv[argc] = NULL;
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
JobRestart(job)
    Job 	  *job; 	/* Job to restart */
{
    if (job->flags & JOB_RESTART) {
	/*
	 * Set up the control arguments to the shell. This is based on the
	 * flags set earlier for this job. If the JOB_IGNERR flag is clear,
	 * the 'exit' flag of the commandShell is used to cause it to exit
	 * upon receiving an error. If the JOB_SILENT flag is clear, the
	 * 'echo' flag of the commandShell is used to get it to start echoing
	 * as soon as it starts processing commands.
	 */
	char	  *argv[4];

	JobMakeArgv(job, argv);

	if (DEBUG(JOB)) {
	    (void)fprintf(stdout, "Restarting %s...", job->node->name);
	    (void)fflush(stdout);
	}
	{
	    if (nLocal >= maxLocal && !(job->flags & JOB_SPECIAL)) {
		/*
		 * Can't be exported and not allowed to run locally -- put it
		 * back on the hold queue and mark the table full
		 */
		if (DEBUG(JOB)) {
		    (void)fprintf(stdout, "holding\n");
		    (void)fflush(stdout);
		}
		Lst_AtFront(&stoppedJobs, job);
		jobFull = true;
		if (DEBUG(JOB)) {
		    (void)fprintf(stdout, "Job queue is full.\n");
		    (void)fflush(stdout);
		}
		return;
	    } else {
		/*
		 * Job may be run locally.
		 */
		if (DEBUG(JOB)) {
		    (void)fprintf(stdout, "running locally\n");
		    (void)fflush(stdout);
		}
	    }
	}
	JobExec(job, argv);
    } else {
	/*
	 * The job has stopped and needs to be restarted. Why it stopped,
	 * we don't know...
	 */
	if (DEBUG(JOB)) {
	   (void)fprintf(stdout, "Resuming %s...", job->node->name);
	   (void)fflush(stdout);
	}
	if ((nLocal < maxLocal ||
	    ((job->flags & JOB_SPECIAL) &&
	     maxLocal == 0)
	   ) && nJobs != maxJobs)
	{
	    /*
	     * If we haven't reached the concurrency limit already (or
	     * maxLocal is 0), it's ok to resume the job.
	     */
	    bool error;
	    int status;

		error = KILL(job->pid, SIGCONT) != 0;

	    if (!error) {
		/*
		 * Make sure the user knows we've continued the beast and
		 * actually put the thing in the job table.
		 */
		job->flags |= JOB_CONTINUING;
		W_SETTERMSIG(&status, SIGCONT);
		JobFinish(job, &status);

		job->flags &= ~(JOB_RESUME|JOB_CONTINUING);
		if (DEBUG(JOB)) {
		   (void)fprintf(stdout, "done\n");
		   (void)fflush(stdout);
		}
	    } else {
		Error("couldn't resume %s: %s",
		    job->node->name, strerror(errno));
		status = 0;
		W_SETEXITSTATUS(&status, 1);
		JobFinish(job, &status);
	    }
	} else {
	    /*
	     * Job cannot be restarted. Mark the table as full and
	     * place the job back on the list of stopped jobs.
	     */
	    if (DEBUG(JOB)) {
		(void)fprintf(stdout, "table full\n");
		(void)fflush(stdout);
	    }
	    Lst_AtFront(&stoppedJobs, job);
	    jobFull = true;
	    if (DEBUG(JOB)) {
		(void)fprintf(stdout, "Job queue is full.\n");
		(void)fflush(stdout);
	    }
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * JobStart  --
 *	Start a target-creation process going for the target described
 *	by the graph node gn.
 *
 * Results:
 *	JOB_ERROR if there was an error in the commands, JOB_FINISHED
 *	if there isn't actually anything left to do for the job and
 *	JOB_RUNNING if the job has been started.
 *
 * Side Effects:
 *	A new Job node is created and added to the list of running
 *	jobs. PMake is forked and a child shell created.
 *-----------------------------------------------------------------------
 */
static int
JobStart(gn, flags, previous)
    GNode	  *gn;	      /* target to create */
    int 	   flags;      /* flags for the job to override normal ones.
			       * e.g. JOB_SPECIAL or JOB_IGNDOTS */
    Job 	  *previous;  /* The previous Job structure for this node,
			       * if any. */
{
    Job 	  *job;       /* new job descriptor */
    char	  *argv[4];   /* Argument vector to shell */
    bool	  cmdsOK;     /* true if the nodes commands were all right */
    bool	  local;      /* Set true if the job was run locally */
    bool	  noExec;     /* Set true if we decide not to run the job */

    if (previous != NULL) {
	previous->flags &= ~(JOB_FIRST|JOB_IGNERR|JOB_SILENT);
	job = previous;
    } else {
	job = emalloc(sizeof(Job));
	if (job == NULL) {
	    Punt("JobStart out of memory");
	}
	flags |= JOB_FIRST;
    }

    job->node = gn;
    job->tailCmds = NULL;

    /*
     * Set the initial value of the flags for this job based on the global
     * ones and the node's attributes... Any flags supplied by the caller
     * are also added to the field.
     */
    job->flags = 0;
    if (Targ_Ignore(gn)) {
	job->flags |= JOB_IGNERR;
    }
    if (Targ_Silent(gn)) {
	job->flags |= JOB_SILENT;
    }
    job->flags |= flags;

    /*
     * Check the commands now so any attributes from .DEFAULT have a chance
     * to migrate to the node
     */
    if (!compatMake && job->flags & JOB_FIRST) {
	cmdsOK = Job_CheckCommands(gn, Error);
    } else {
	cmdsOK = true;
    }

    /*
     * If the -n flag wasn't given, we open up OUR (not the child's)
     * temporary file to stuff commands in it. The thing is rd/wr so we don't
     * need to reopen it to feed it to the shell. If the -n flag *was* given,
     * we just set the file to be stdout. Cute, huh?
     */
    if ((gn->type & OP_MAKE) || (!noExecute && !touchFlag)) {
	/*
	 * We're serious here, but if the commands were bogus, we're
	 * also dead...
	 */
	if (!cmdsOK) {
	    DieHorribly();
	}

	job->cmdFILE = fopen(tfile, "w+");
	if (job->cmdFILE == NULL) {
	    Punt("Could not open %s", tfile);
	}
	(void)fcntl(FILENO(job->cmdFILE), F_SETFD, 1);
	/*
	 * Send the commands to the command file, flush all its buffers then
	 * rewind and remove the thing.
	 */
	noExec = false;

	/*
	 * used to be backwards; replace when start doing multiple commands
	 * per shell.
	 */
	if (compatMake) {
	    /*
	     * Be compatible: If this is the first time for this node,
	     * verify its commands are ok and open the commands list for
	     * sequential access by later invocations of JobStart.
	     * Once that is done, we take the next command off the list
	     * and print it to the command file. If the command was an
	     * ellipsis, note that there's nothing more to execute.
	     */
	    if ((job->flags&JOB_FIRST))
		gn->current = Lst_First(&gn->commands);
	    else
		gn->current = Lst_Succ(gn->current);

	    if (gn->current == NULL ||
		!JobPrintCommand(Lst_Datum(gn->current), job)) {
		noExec = true;
		gn->current = NULL;
	    }
	    if (noExec && !(job->flags & JOB_FIRST)) {
		/*
		 * If we're not going to execute anything, the job
		 * is done and we need to close down the various
		 * file descriptors we've opened for output, then
		 * call JobDoOutput to catch the final characters or
		 * send the file to the screen... Note that the i/o streams
		 * are only open if this isn't the first job.
		 * Note also that this could not be done in
		 * Job_CatchChildren b/c it wasn't clear if there were
		 * more commands to execute or not...
		 */
		JobClose(job);
	    }
	} else {
	    /*
	     * We can do all the commands at once. hooray for sanity
	     */
	    numCommands = 0;
	    Lst_Find(&gn->commands, JobPrintCommand, job);

	    /*
	     * If we didn't print out any commands to the shell script,
	     * there's not much point in executing the shell, is there?
	     */
	    if (numCommands == 0) {
		noExec = true;
	    }
	}
    } else if (noExecute) {
	/*
	 * Not executing anything -- just print all the commands to stdout
	 * in one fell swoop. This will still set up job->tailCmds correctly.
	 */
	if (lastNode != gn) {
	    MESSAGE(stdout, gn);
	    lastNode = gn;
	}
	job->cmdFILE = stdout;
	/*
	 * Only print the commands if they're ok, but don't die if they're
	 * not -- just let the user know they're bad and keep going. It
	 * doesn't do any harm in this case and may do some good.
	 */
	if (cmdsOK) {
	    Lst_Find(&gn->commands, JobPrintCommand, job);
	}
	/*
	 * Don't execute the shell, thank you.
	 */
	noExec = true;
    } else {
	/*
	 * Just touch the target and note that no shell should be executed.
	 * Set cmdFILE to stdout to make life easier. Check the commands, too,
	 * but don't die if they're no good -- it does no harm to keep working
	 * up the graph.
	 */
	job->cmdFILE = stdout;
	Job_Touch(gn, job->flags&JOB_SILENT);
	noExec = true;
    }

    /*
     * If we're not supposed to execute a shell, don't.
     */
    if (noExec) {
	/*
	 * Unlink and close the command file if we opened one
	 */
	if (job->cmdFILE != stdout) {
	    (void)eunlink(tfile);
	    if (job->cmdFILE != NULL)
		(void)fclose(job->cmdFILE);
	} else {
	     (void)fflush(stdout);
	}

	/*
	 * We only want to work our way up the graph if we aren't here because
	 * the commands for the job were no good.
	 */
	if (cmdsOK) {
	    if (aborting == 0) {
		Lst_ForEachFrom(job->tailCmds, JobSaveCommand, job->node);
		Make_Update(job->node);
	    }
	    free(job);
	    return JOB_FINISHED;
	} else {
	    free(job);
	    return JOB_ERROR;
	}
    } else {
	(void)fflush(job->cmdFILE);
	(void)eunlink(tfile);
    }

    /*
     * Set up the control arguments to the shell. This is based on the flags
     * set earlier for this job.
     */
    JobMakeArgv(job, argv);

    /*
     * If we're using pipes to catch output, create the pipe by which we'll
     * get the shell's output. If we're using files, print out that we're
     * starting a job and then set up its temporary-file name.
     */
    if (!compatMake || (job->flags & JOB_FIRST)) {
	if (usePipes) {
	    int fd[2];
	    if (pipe(fd) == -1)
		Punt("Cannot create pipe: %s", strerror(errno));
	    job->inPipe = fd[0];
	    job->outPipe = fd[1];
	    (void)fcntl(job->inPipe, F_SETFD, 1);
	    (void)fcntl(job->outPipe, F_SETFD, 1);
	} else {
	    (void)fprintf(stdout, "Remaking `%s'\n", gn->name);
	    (void)fflush(stdout);
	    (void)strcpy(job->outFile, TMPPAT);
	    if ((job->outFd = mkstemp(job->outFile)) == -1)
		Punt("Cannot create temp file: %s", strerror(errno));
	    (void)fcntl(job->outFd, F_SETFD, 1);
	}
    }

	local = true;

    if (local && nLocal >= maxLocal &&
	!(job->flags & JOB_SPECIAL) &&
	maxLocal != 0
	)
    {
	/*
	 * The job can only be run locally, but we've hit the limit of
	 * local concurrency, so put the job on hold until some other job
	 * finishes. Note that the special jobs (.BEGIN, .INTERRUPT and .END)
	 * may be run locally even when the local limit has been reached
	 * (e.g. when maxLocal == 0), though they will be exported if at
	 * all possible. In addition, any target marked with .NOEXPORT will
	 * be run locally if maxLocal is 0.
	 */
	jobFull = true;

	if (DEBUG(JOB)) {
	   (void)fprintf(stdout, "Can only run job locally.\n");
	   (void)fflush(stdout);
	}
	job->flags |= JOB_RESTART;
	Lst_AtEnd(&stoppedJobs, job);
    } else {
	if (nLocal >= maxLocal && local) {
	    /*
	     * If we're running this job locally as a special case (see above),
	     * at least say the table is full.
	     */
	    jobFull = true;
	    if (DEBUG(JOB)) {
		(void)fprintf(stdout, "Local job queue is full.\n");
		(void)fflush(stdout);
	    }
	}
	JobExec(job, argv);
    }
    return JOB_RUNNING;
}

static char *
JobOutput(job, cp, endp, msg)
    Job *job;
    char *cp, *endp;
    int msg;
{
    char *ecp;

    if (commandShell->noPrint) {
	ecp = strstr(cp, commandShell->noPrint);
	while (ecp != NULL) {
	    if (cp != ecp) {
		*ecp = '\0';
		if (msg && job->node != lastNode) {
		    MESSAGE(stdout, job->node);
		    lastNode = job->node;
		}
		/*
		 * The only way there wouldn't be a newline after
		 * this line is if it were the last in the buffer.
		 * however, since the non-printable comes after it,
		 * there must be a newline, so we don't print one.
		 */
		(void)fprintf(stdout, "%s", cp);
		(void)fflush(stdout);
	    }
	    cp = ecp + commandShell->noPLen;
	    if (cp != endp) {
		/*
		 * Still more to print, look again after skipping
		 * the whitespace following the non-printable
		 * command....
		 */
		cp++;
		while (*cp == ' ' || *cp == '\t' || *cp == '\n') {
		    cp++;
		}
		ecp = strstr(cp, commandShell->noPrint);
	    } else {
		return cp;
	    }
	}
    }
    return cp;
}

/*-
 *-----------------------------------------------------------------------
 * JobDoOutput	--
 *	This function is called at different times depending on
 *	whether the user has specified that output is to be collected
 *	via pipes or temporary files. In the former case, we are called
 *	whenever there is something to read on the pipe. We collect more
 *	output from the given job and store it in the job's outBuf. If
 *	this makes up a line, we print it tagged by the job's identifier,
 *	as necessary.
 *	If output has been collected in a temporary file, we open the
 *	file and read it line by line, transfering it to our own
 *	output channel until the file is empty. At which point we
 *	remove the temporary file.
 *	In both cases, however, we keep our figurative eye out for the
 *	'noPrint' line for the shell from which the output came. If
 *	we recognize a line, we don't print it. If the command is not
 *	alone on the line (the character after it is not \0 or \n), we
 *	do print whatever follows it.
 *
 * Side Effects:
 *	curPos may be shifted as may the contents of outBuf.
 *-----------------------------------------------------------------------
 */
static void
JobDoOutput(job, finish)
    Job 	  *job;   /* the job whose output needs printing */
    bool	   finish;	  /* true if this is the last time we'll be
				   * called for this job */
{
    bool	  gotNL = false;  /* true if got a newline */
    bool	  fbuf; 	  /* true if our buffer filled up */
    int 	  nr;		  /* number of bytes read */
    int 	  i;		  /* auxiliary index into outBuf */
    int 	  max;		  /* limit for i (end of current data) */
    int 	  nRead;	  /* (Temporary) number of bytes read */

    FILE	  *oFILE;	  /* Stream pointer to shell's output file */
    char	  inLine[132];


    if (usePipes) {
	/*
	 * Read as many bytes as will fit in the buffer.
	 */
end_loop:
	gotNL = false;
	fbuf = false;

	nRead = read(job->inPipe, &job->outBuf[job->curPos],
			 JOB_BUFSIZE - job->curPos);
	if (nRead == -1) {
	    if (DEBUG(JOB)) {
		perror("JobDoOutput(piperead)");
	    }
	    nr = 0;
	} else {
	    nr = nRead;
	}

	/*
	 * If we hit the end-of-file (the job is dead), we must flush its
	 * remaining output, so pretend we read a newline if there's any
	 * output remaining in the buffer.
	 * Also clear the 'finish' flag so we stop looping.
	 */
	if (nr == 0 && job->curPos != 0) {
	    job->outBuf[job->curPos] = '\n';
	    nr = 1;
	    finish = false;
	} else if (nr == 0) {
	    finish = false;
	}

	/*
	 * Look for the last newline in the bytes we just got. If there is
	 * one, break out of the loop with 'i' as its index and gotNL set
	 * true.
	 */
	max = job->curPos + nr;
	for (i = job->curPos + nr - 1; i >= job->curPos; i--) {
	    if (job->outBuf[i] == '\n') {
		gotNL = true;
		break;
	    } else if (job->outBuf[i] == '\0') {
		/*
		 * Why?
		 */
		job->outBuf[i] = ' ';
	    }
	}

	if (!gotNL) {
	    job->curPos += nr;
	    if (job->curPos == JOB_BUFSIZE) {
		/*
		 * If we've run out of buffer space, we have no choice
		 * but to print the stuff. sigh.
		 */
		fbuf = true;
		i = job->curPos;
	    }
	}
	if (gotNL || fbuf) {
	    /*
	     * Need to send the output to the screen. Null terminate it
	     * first, overwriting the newline character if there was one.
	     * So long as the line isn't one we should filter (according
	     * to the shell description), we print the line, preceeded
	     * by a target banner if this target isn't the same as the
	     * one for which we last printed something.
	     * The rest of the data in the buffer are then shifted down
	     * to the start of the buffer and curPos is set accordingly.
	     */
	    job->outBuf[i] = '\0';
	    if (i >= job->curPos) {
		char *cp;

		cp = JobOutput(job, job->outBuf, &job->outBuf[i], false);

		/*
		 * There's still more in that thar buffer. This time, though,
		 * we know there's no newline at the end, so we add one of
		 * our own free will.
		 */
		if (*cp != '\0') {
		    if (job->node != lastNode) {
			MESSAGE(stdout, job->node);
			lastNode = job->node;
		    }
		    (void)fprintf(stdout, "%s%s", cp, gotNL ? "\n" : "");
		    (void)fflush(stdout);
		}
	    }
	    if (i < max - 1) {
		/* shift the remaining characters down */
		(void)memcpy(job->outBuf, &job->outBuf[i + 1], max - (i + 1));
		job->curPos = max - (i + 1);

	    } else {
		/*
		 * We have written everything out, so we just start over
		 * from the start of the buffer. No copying. No nothing.
		 */
		job->curPos = 0;
	    }
	}
	if (finish) {
	    /*
	     * If the finish flag is true, we must loop until we hit
	     * end-of-file on the pipe. This is guaranteed to happen
	     * eventually since the other end of the pipe is now closed
	     * (we closed it explicitly and the child has exited). When
	     * we do get an EOF, finish will be set false and we'll fall
	     * through and out.
	     */
	    goto end_loop;
	}
    } else {
	/*
	 * We've been called to retrieve the output of the job from the
	 * temporary file where it's been squirreled away. This consists of
	 * opening the file, reading the output line by line, being sure not
	 * to print the noPrint line for the shell we used, then close and
	 * remove the temporary file. Very simple.
	 *
	 * Change to read in blocks and do FindSubString type things as for
	 * pipes? That would allow for "@echo -n..."
	 */
	oFILE = fopen(job->outFile, "r");
	if (oFILE != NULL) {
	    (void)fprintf(stdout, "Results of making %s:\n", job->node->name);
	    (void)fflush(stdout);
	    while (fgets(inLine, sizeof(inLine), oFILE) != NULL) {
		char	*cp, *endp, *oendp;

		cp = inLine;
		oendp = endp = inLine + strlen(inLine);
		if (endp[-1] == '\n') {
		    *--endp = '\0';
		}
		cp = JobOutput(job, inLine, endp, false);

		/*
		 * There's still more in that thar buffer. This time, though,
		 * we know there's no newline at the end, so we add one of
		 * our own free will.
		 */
		(void)fprintf(stdout, "%s", cp);
		(void)fflush(stdout);
		if (endp != oendp) {
		    (void)fprintf(stdout, "\n");
		    (void)fflush(stdout);
		}
	    }
	    (void)fclose(oFILE);
	    (void)eunlink(job->outFile);
	}
    }
}

/*-
 *-----------------------------------------------------------------------
 * Job_CatchChildren --
 *	Handle the exit of a child. Called from Make_Make.
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
Job_CatchChildren(block)
    bool	  block;	/* true if should block on the wait. */
{
    pid_t 	  pid;		/* pid of dead child */
    Job 	  *job; 	/* job descriptor for dead child */
    LstNode	  jnode;	/* list element for finding job */
    int 	  status;	/* Exit/termination status */

    /*
     * Don't even bother if we know there's no one around.
     */
    if (nLocal == 0) {
	return;
    }

    while ((pid = waitpid((pid_t) -1, &status,
			  (block?0:WNOHANG)|WUNTRACED)) > 0)
    {
	if (DEBUG(JOB)) {
	    (void)fprintf(stdout, "Process %ld exited or stopped.\n", (long)pid);
	    (void)fflush(stdout);
	}


	jnode = Lst_Find(&jobs, JobCmpPid, &pid);

	if (jnode == NULL) {
	    if (WIFSIGNALED(status) && (WTERMSIG(status) == SIGCONT)) {
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
	    Lst_Remove(&jobs, jnode);
	    nJobs -= 1;
	    if (jobFull && DEBUG(JOB)) {
		(void)fprintf(stdout, "Job queue is no longer full.\n");
		(void)fflush(stdout);
	    }
	    jobFull = false;
	    nLocal -= 1;
	}

	JobFinish(job, &status);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Job_CatchOutput --
 *	Catch the output from our children, if we're using
 *	pipes do so. Otherwise just block time until we get a
 *	signal (most likely a SIGCHLD) since there's no point in
 *	just spinning when there's nothing to do and the reaping
 *	of a child can wait for a while.
 *
 * Side Effects:
 *	Output is read from pipes if we're piping.
 * -----------------------------------------------------------------------
 */
void
Job_CatchOutput()
{
    int 		  nfds;
    struct timeval	  timeout;
    LstNode		  ln;
    Job 		  *job;

    (void)fflush(stdout);
    if (usePipes) {
	int count = howmany(outputsn+1, NFDBITS) * sizeof(fd_mask);
	fd_set *readfdsp = malloc(count);
	if (readfdsp == NULL)
	    return;

	memcpy(readfdsp, outputsp, count);
	timeout.tv_sec = SEL_SEC;
	timeout.tv_usec = SEL_USEC;

	if ((nfds = select(outputsn+1, readfdsp, (fd_set *) 0,
			   (fd_set *) 0, &timeout)) <= 0) {
	    free(readfdsp);
	    return;
	} else {
	    for (ln = Lst_First(&jobs); nfds && ln != NULL; ln = Lst_Adv(ln)) {
		job = (Job *)Lst_Datum(ln);
		if (FD_ISSET(job->inPipe, readfdsp)) {
		    JobDoOutput(job, false);
		    nfds -= 1;
		}
	    }
	}
	free(readfdsp);
    }
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
Job_Make(gn)
    GNode   *gn;
{
    (void)JobStart(gn, 0, NULL);
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
Job_Init(maxproc, maxlocal)
    int 	  maxproc;  /* the greatest number of jobs which may be
			     * running at one time */
    int 	  maxlocal; /* the greatest number of local jobs which may
			     * be running at once. */
{
    GNode	  *begin;     /* node for commands to do at the very start */
    int 	  tfd;

    (void)strcpy(tfile, TMPPAT);
    if ((tfd = mkstemp(tfile)) == -1)
	Punt("Cannot create temp file: %s", strerror(errno));
    else
	(void)close(tfd);

    Static_Lst_Init(&jobs);
    Static_Lst_Init(&stoppedJobs);
    maxJobs =	  maxproc;
    maxLocal =	  maxlocal;
    nJobs =	  0;
    nLocal =	  0;
    jobFull =	  false;

    aborting =	  0;
    errors =	  0;

    lastNode =	  NULL;

    if (maxJobs == 1) {
	/*
	 * If only one job can run at a time, there's no need for a banner,
	 * no is there?
	 */
	targFmt = "";
    } else {
	targFmt = TARG_FMT;
    }

    if (shellPath == NULL) {
	/*
	 * The user didn't specify a shell to use, so we are using the
	 * default one... Both the absolute path and the last component
	 * must be set. The last component is taken from the 'name' field
	 * of the default shell description pointed-to by commandShell.
	 * All default shells are located in _PATH_DEFSHELLDIR.
	 */
	shellName = commandShell->name;
	shellPath = Str_concat(_PATH_DEFSHELLDIR, shellName, '/');
    }

    if (commandShell->exit == NULL) {
	commandShell->exit = "";
    }
    if (commandShell->echo == NULL) {
	commandShell->echo = "";
    }

    /*
     * Catch the four signals that POSIX specifies if they aren't ignored.
     * JobPassSig will take care of calling JobInterrupt if appropriate.
     */
    if (signal(SIGINT, SIG_IGN) != SIG_IGN) {
	(void)signal(SIGINT, JobPassSig);
    }
    if (signal(SIGHUP, SIG_IGN) != SIG_IGN) {
	(void)signal(SIGHUP, JobPassSig);
    }
    if (signal(SIGQUIT, SIG_IGN) != SIG_IGN) {
	(void)signal(SIGQUIT, JobPassSig);
    }
    if (signal(SIGTERM, SIG_IGN) != SIG_IGN) {
	(void)signal(SIGTERM, JobPassSig);
    }
    /*
     * There are additional signals that need to be caught and passed if
     * either the export system wants to be told directly of signals or if
     * we're giving each job its own process group (since then it won't get
     * signals from the terminal driver as we own the terminal)
     */
#if defined(USE_PGRP)
    if (signal(SIGTSTP, SIG_IGN) != SIG_IGN) {
	(void)signal(SIGTSTP, JobPassSig);
    }
    if (signal(SIGTTOU, SIG_IGN) != SIG_IGN) {
	(void)signal(SIGTTOU, JobPassSig);
    }
    if (signal(SIGTTIN, SIG_IGN) != SIG_IGN) {
	(void)signal(SIGTTIN, JobPassSig);
    }
    if (signal(SIGWINCH, SIG_IGN) != SIG_IGN) {
	(void)signal(SIGWINCH, JobPassSig);
    }
#endif

    begin = Targ_FindNode(".BEGIN", TARG_NOCREATE);

    if (begin != NULL) {
	JobStart(begin, JOB_SPECIAL, (Job *)0);
	while (nJobs) {
	    Job_CatchOutput();
	    Job_CatchChildren(!usePipes);
	}
    }
    postCommands = Targ_FindNode(".END", TARG_CREATE);
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
Job_Full()
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
Job_Empty()
{
    if (nJobs == 0) {
	if (!Lst_IsEmpty(&stoppedJobs) && !aborting) {
	    /*
	     * The job table is obviously not full if it has no jobs in
	     * it...Try and restart the stopped jobs.
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
 * JobMatchShell --
 *	Find a matching shell in 'shells' given its final component.
 *
 * Results:
 *	A pointer to the Shell structure.
 *-----------------------------------------------------------------------
 */
static Shell *
JobMatchShell(name)
    char	  *name;      /* Final component of shell path */
{
    Shell	  *sh;	      /* Pointer into shells table */
    Shell	  *match;     /* Longest-matching shell */
    char	  *cp1,
		  *cp2;
    char	  *eoname;

    eoname = name + strlen(name);

    match = NULL;

    for (sh = shells; sh->name != NULL; sh++) {
	for (cp1 = eoname - strlen(sh->name), cp2 = sh->name;
	     *cp1 != '\0' && *cp1 == *cp2;
	     cp1++, cp2++) {
		 continue;
	}
	if (*cp1 != *cp2) {
	    continue;
	} else if (match == NULL || strlen(match->name) < strlen(sh->name)) {
	   match = sh;
	}
    }
    return match == NULL ? sh : match;
}

/*-
 *-----------------------------------------------------------------------
 * Job_ParseShell --
 *	Parse a shell specification and set up commandShell, shellPath
 *	and shellName appropriately.
 *
 * Results:
 *	false if the specification was incorrect.
 *
 * Side Effects:
 *	commandShell points to a Shell structure (either predefined or
 *	created from the shell spec), shellPath is the full path of the
 *	shell described by commandShell, while shellName is just the
 *	final component of shellPath.
 *
 * Notes:
 *	A shell specification consists of a .SHELL target, with dependency
 *	operator, followed by a series of blank-separated words. Double
 *	quotes can be used to use blanks in words. A backslash escapes
 *	anything (most notably a double-quote and a space) and
 *	provides the functionality it does in C. Each word consists of
 *	keyword and value separated by an equal sign. There should be no
 *	unnecessary spaces in the word. The keywords are as follows:
 *	    name	    Name of shell.
 *	    path	    Location of shell. Overrides "name" if given
 *	    quiet	    Command to turn off echoing.
 *	    echo	    Command to turn echoing on
 *	    filter	    Result of turning off echoing that shouldn't be
 *			    printed.
 *	    echoFlag	    Flag to turn echoing on at the start
 *	    errFlag	    Flag to turn error checking on at the start
 *	    hasErrCtl	    True if shell has error checking control
 *	    check	    Command to turn on error checking if hasErrCtl
 *			    is true or template of command to echo a command
 *			    for which error checking is off if hasErrCtl is
 *			    false.
 *	    ignore	    Command to turn off error checking if hasErrCtl
 *			    is true or template of command to execute a
 *			    command so as to ignore any errors it returns if
 *			    hasErrCtl is false.
 *-----------------------------------------------------------------------
 */
bool
Job_ParseShell(line)
    char	  *line;  /* The shell spec */
{
    char	  **words;
    int 	  wordCount;
    char	  **argv;
    int 	  argc;
    char	  *path;
    Shell	  newShell;
    bool	  fullSpec = false;

    while (isspace(*line)) {
	line++;
    }

    efree(shellArgv);

    words = brk_string(line, &wordCount, &shellArgv);

    memset(&newShell, 0, sizeof(newShell));

    /*
     * Parse the specification by keyword
     */
    for (path = NULL, argc = wordCount - 1, argv = words;
	 argc != 0;
	 argc--, argv++) {
	     if (strncmp(*argv, "path=", 5) == 0) {
		 path = &argv[0][5];
	     } else if (strncmp(*argv, "name=", 5) == 0) {
		 newShell.name = &argv[0][5];
	     } else {
		 if (strncmp(*argv, "quiet=", 6) == 0) {
		     newShell.echoOff = &argv[0][6];
		 } else if (strncmp(*argv, "echo=", 5) == 0) {
		     newShell.echoOn = &argv[0][5];
		 } else if (strncmp(*argv, "filter=", 7) == 0) {
		     newShell.noPrint = &argv[0][7];
		     newShell.noPLen = strlen(newShell.noPrint);
		 } else if (strncmp(*argv, "echoFlag=", 9) == 0) {
		     newShell.echo = &argv[0][9];
		 } else if (strncmp(*argv, "errFlag=", 8) == 0) {
		     newShell.exit = &argv[0][8];
		 } else if (strncmp(*argv, "hasErrCtl=", 10) == 0) {
		     char c = argv[0][10];
		     newShell.hasErrCtl = !(c != 'Y' && c != 'y' &&
					   c != 'T' && c != 't');
		 } else if (strncmp(*argv, "check=", 6) == 0) {
		     newShell.errCheck = &argv[0][6];
		 } else if (strncmp(*argv, "ignore=", 7) == 0) {
		     newShell.ignErr = &argv[0][7];
		 } else {
		     Parse_Error(PARSE_FATAL, "Unknown keyword \"%s\"",
				  *argv);
		     free(words);
		     return false;
		 }
		 fullSpec = true;
	     }
    }

    if (path == NULL) {
	/*
	 * If no path was given, the user wants one of the pre-defined shells,
	 * yes? So we find the one s/he wants with the help of JobMatchShell
	 * and set things up the right way. shellPath will be set up by
	 * Job_Init.
	 */
	if (newShell.name == NULL) {
	    Parse_Error(PARSE_FATAL, "Neither path nor name specified");
	    return false;
	} else {
	    commandShell = JobMatchShell(newShell.name);
	    shellName = newShell.name;
	}
    } else {
	/*
	 * The user provided a path. If s/he gave nothing else (fullSpec is
	 * false), try and find a matching shell in the ones we know of.
	 * Else we just take the specification at its word and copy it
	 * to a new location. In either case, we need to record the
	 * path the user gave for the shell.
	 */
	shellPath = path;
	path = strrchr(path, '/');
	if (path == NULL) {
	    path = shellPath;
	} else {
	    path += 1;
	}
	if (newShell.name != NULL) {
	    shellName = newShell.name;
	} else {
	    shellName = path;
	}
	if (!fullSpec) {
	    commandShell = JobMatchShell(shellName);
	} else {
	    commandShell = emalloc(sizeof(Shell));
	    *commandShell = newShell;
	}
    }

    if (commandShell->echoOn && commandShell->echoOff) {
	commandShell->hasEchoCtl = true;
    }

    if (!commandShell->hasErrCtl) {
	if (commandShell->errCheck == NULL) {
	    commandShell->errCheck = "";
	}
	if (commandShell->ignErr == NULL) {
	    commandShell->ignErr = "%s\n";
	}
    }

    /*
     * Do not free up the words themselves, since they might be in use by the
     * shell specification...
     */
    free(words);
    return true;
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
JobInterrupt(runINTERRUPT, signo)
    int     runINTERRUPT;	/* Non-zero if commands for the .INTERRUPT
				 * target should be executed */
    int     signo;		/* signal received */
{
    LstNode	  ln;		/* element in job table */
    Job 	  *job; 	/* job descriptor in that element */
    GNode	  *interrupt;	/* the node describing the .INTERRUPT target */

    aborting = ABORT_INTERRUPT;

    for (ln = Lst_First(&jobs); ln != NULL; ln = Lst_Adv(ln)) {
	job = (Job *)Lst_Datum(ln);

	if (!Targ_Precious(job->node)) {
	    const char	*file = job->node->path == NULL ?
				 job->node->name :
				 job->node->path;
	    if (!noExecute && eunlink(file) != -1) {
		Error("*** %s removed", file);
	    }
	}
	if (job->pid) {
	    if (DEBUG(JOB)) {
		(void)fprintf(stdout,
			       "JobInterrupt passing signal to child %ld.\n",
			       (long)job->pid);
		(void)fflush(stdout);
	    }
	    KILL(job->pid, signo);
	}
    }

    if (runINTERRUPT && !touchFlag) {
	interrupt = Targ_FindNode(".INTERRUPT", TARG_NOCREATE);
	if (interrupt != NULL) {
	    ignoreErrors = false;

	    JobStart(interrupt, JOB_IGNDOTS, (Job *)0);
	    while (nJobs) {
		Job_CatchOutput();
		Job_CatchChildren(!usePipes);
	    }
	}
    }
    (void)eunlink(tfile);
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
 * Side Effects:
 *	The process' temporary file (tfile) is removed if it still
 *	existed.
 *-----------------------------------------------------------------------
 */
int
Job_Finish()
{
    if (postCommands != NULL && !Lst_IsEmpty(&postCommands->commands)) {
	if (errors) {
	    Error("Errors reported so .END ignored");
	} else {
	    JobStart(postCommands, JOB_SPECIAL | JOB_IGNDOTS, NULL);

	    while (nJobs) {
		Job_CatchOutput();
		Job_CatchChildren(!usePipes);
	    }
	}
    }
    (void)eunlink(tfile);
    return errors;
}

/*-
 *-----------------------------------------------------------------------
 * Job_End --
 *	Cleanup any memory used by the jobs module
 *
 * Side Effects:
 *	Memory is freed
 *-----------------------------------------------------------------------
 */
#ifdef CLEANUP
void
Job_End()
{
    efree(shellArgv);
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
Job_Wait()
{
    aborting = ABORT_WAIT;
    while (nJobs != 0) {
	Job_CatchOutput();
	Job_CatchChildren(!usePipes);
    }
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
Job_AbortAll()
{
    LstNode		ln;	/* element in job table */
    Job 		*job;	/* the job descriptor in that element */
    int 		foo;

    aborting = ABORT_ERROR;

    if (nJobs) {
	for (ln = Lst_First(&jobs); ln != NULL; ln = Lst_Adv(ln)) {
	    job = (Job *)Lst_Datum(ln);

	    /*
	     * kill the child process with increasingly drastic signals to make
	     * darn sure it's dead.
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
    (void)eunlink(tfile);
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
JobRestartJobs()
{
    Job *job;

    while (!jobFull && (job = (Job *)Lst_DeQueue(&stoppedJobs)) != NULL) {
	if (DEBUG(JOB)) {
	    (void)fprintf(stdout,
		       "Job queue is not full. Restarting a stopped job.\n");
	    (void)fflush(stdout);
	}
	JobRestart(job);
    }
}
