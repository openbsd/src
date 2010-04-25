/*	$OpenBSD: engine.c,v 1.28 2010/04/25 13:59:53 espie Exp $ */
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

#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
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
#include "pathnames.h"
#include "error.h"
#include "str.h"
#include "memory.h"
#include "buf.h"
#include "job.h"

static void MakeTimeStamp(void *, void *);
static int rewrite_time(const char *);
static void setup_signal(int, psighandler);
static void setup_meta(void);
static char **recheck_command_for_shell(char **);

static int setup_and_run_command(char *, GNode *, int);
static void run_command(const char *, bool);
static int run_prepared_gnode(GNode *);
static void handle_compat_interrupts(GNode *);

bool
Job_CheckCommands(GNode *gn)
{
	/* Alter our type to tell if errors should be ignored or things
	 * should not be printed so setup_and_run_command knows what to do.
	 */
	if (Targ_Ignore(gn))
		gn->type |= OP_IGNORE;
	if (Targ_Silent(gn))
		gn->type |= OP_SILENT;

	if (OP_NOP(gn->type) && Lst_IsEmpty(&gn->commands) &&
	    (gn->type & OP_LIB) == 0) {
		/*
		 * No commands. Look for .DEFAULT rule from which we might infer
		 * commands
		 */
		if ((gn->type & OP_NODEFAULT) == 0 &&
		    (DEFAULT->type & OP_DUMMY) == 0 &&
		    !Lst_IsEmpty(&DEFAULT->commands)) {
			/*
			 * Make only looks for a .DEFAULT if the node was never
			 * the target of an operator, so that's what we do too.
			 * If a .DEFAULT was given, we substitute its commands
			 * for gn's commands and set the IMPSRC variable to be
			 * the target's name The DEFAULT node acts like a
			 * transformation rule, in that gn also inherits any
			 * attributes or sources attached to .DEFAULT itself.
			 */
			Make_HandleUse(DEFAULT, gn);
			Var(IMPSRC_INDEX, gn) = Var(TARGET_INDEX, gn);
		} else if (is_out_of_date(Dir_MTime(gn))) {
			/*
			 * The node wasn't the target of an operator we have no
			 * .DEFAULT rule to go on and the target doesn't
			 * already exist. There's nothing more we can do for
			 * this branch.
			 */
			 return false;
	    	}
	}
	return true;
}

void
job_failure(GNode *gn, void (*abortProc)(char *, ...))
{
	/*
	 If the -k flag wasn't given, we stop in
	 * our tracks, otherwise we just don't update this
	 * node's parents so they never get examined.
	 */
	static const char msg[] =
	    "make: don't know how to make";

	if (gn->type & OP_OPTIONAL) {
		(void)fprintf(stdout, "%s %s(ignored)\n", msg,
		    gn->name);
		(void)fflush(stdout);
	} else if (keepgoing) {
		(void)fprintf(stdout, "%s %s(continuing)\n",
		    msg, gn->name);
		(void)fflush(stdout);
	} else {
		(*abortProc)("%s %s. Stop in %s.", msg,
		    gn->name, Var_Value(".CURDIR"));
	}
}
/* touch files the hard way, by writing stuff to them */
static int
rewrite_time(const char *name)
{
	int fd;
	char c;

	fd = open(name, O_RDWR | O_CREAT, 0666);
	if (fd < 0)
		return -1;
	/*
	 * Read and write a byte to the file to change
	 * the modification time.
	 */
	if (read(fd, &c, 1) == 1) {
		(void)lseek(fd, 0, SEEK_SET);
		(void)write(fd, &c, 1);
	}

	(void)close(fd);
	return 0;
}

void
Job_Touch(GNode *gn)
{
	if (gn->type & (OP_JOIN|OP_USE|OP_EXEC|OP_OPTIONAL|OP_PHONY)) {
		/*
		 * .JOIN, .USE, and .OPTIONAL targets are "virtual" targets
		 * and, as such, shouldn't really be created.
		 * Likewise, .PHONY targets are not really files
		 */
		return;
	}

	if (!(gn->type & OP_SILENT)) {
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
			if (rewrite_time(file) == -1) {
				(void)fprintf(stdout,
				    "*** couldn't touch %s: %s", file,
				    strerror(errno));
				(void)fflush(stdout);
		    	}
		}
	}
}

void
Make_TimeStamp(GNode *parent, GNode *child)
{
	if (is_strictly_before(parent->cmtime, child->mtime))
		parent->cmtime = child->mtime;
}

void
Make_HandleUse(GNode	*cgn,	/* The .USE node */
    GNode	*pgn)	/* The target of the .USE node */
{
	GNode	*gn;	/* A child of the .USE node */
	LstNode	ln;	/* An element in the children list */


	assert(cgn->type & (OP_USE|OP_TRANSFORM));

	if ((cgn->type & OP_USE) || Lst_IsEmpty(&pgn->commands)) {
		/* .USE or transformation and target has no commands
		 * -- append the child's commands to the parent.  */
		Lst_Concat(&pgn->commands, &cgn->commands);
	}

	for (ln = Lst_First(&cgn->children); ln != NULL;
	    ln = Lst_Adv(ln)) {
		gn = (GNode *)Lst_Datum(ln);

		if (Lst_AddNew(&pgn->children, gn)) {
			Lst_AtEnd(&gn->parents, pgn);
			pgn->unmade++;
		}
	}

	pgn->type |= cgn->type & ~(OP_OPMASK|OP_USE|OP_TRANSFORM);

	/*
	 * This child node is now "made", so we decrement the count of
	 * unmade children in the parent... We also remove the child
	 * from the parent's list to accurately reflect the number of
	 * decent children the parent has. This is used by Make_Run to
	 * decide whether to queue the parent or examine its children...
	 */
	if (cgn->type & OP_USE)
		pgn->unmade--;

	/* if the parent node doesn't have any location, then inherit the
	 * use stuff, since that gives us better error messages.
	 */
	if (!pgn->lineno) {
		pgn->lineno = cgn->lineno;
		pgn->fname = cgn->fname;
	}
}

void
Make_DoAllVar(GNode *gn)
{
	GNode *child;
	LstNode ln;
	BUFFER allsrc, oodate;
	char *target;
	bool do_oodate;
	int oodate_count, allsrc_count = 0;

	oodate_count = 0;
	allsrc_count = 0;

	for (ln = Lst_First(&gn->children); ln != NULL; ln = Lst_Adv(ln)) {
		child = (GNode *)Lst_Datum(ln);
		if ((child->type & (OP_EXEC|OP_USE|OP_INVISIBLE)) != 0)
			continue;
		if (OP_NOP(child->type) ||
		    (target = Var(TARGET_INDEX, child)) == NULL) {
			/*
			 * this node is only source; use the specific pathname
			 * for it
			 */
			target = child->path != NULL ? child->path :
			    child->name;
		}

		/*
		 * It goes in the OODATE variable if the parent is younger than
		 * the child or if the child has been modified more recently
		 * than the start of the make.  This is to keep make from
		 * getting confused if something else updates the parent after
		 * the make starts (shouldn't happen, I know, but sometimes it
		 * does). In such a case, if we've updated the kid, the parent
		 * is likely to have a modification time later than that of the
		 * kid and anything that relies on the OODATE variable will be
		 * hosed.
		 */
		do_oodate = false;
		if (gn->type & OP_JOIN) {
			if (child->built_status == MADE)
				do_oodate = true;
		} else if (is_strictly_before(gn->mtime, child->mtime) ||
		   (!is_strictly_before(child->mtime, now) &&
		   child->built_status == MADE))
		   	do_oodate = true;
		if (do_oodate) {
			oodate_count++;
			if (oodate_count == 1)
				Var(OODATE_INDEX, gn) = target;
			else {
				if (oodate_count == 2) {
					Buf_Init(&oodate, 0);
					Buf_AddString(&oodate,
					    Var(OODATE_INDEX, gn));
				}
				Buf_AddSpace(&oodate);
				Buf_AddString(&oodate, target);
			}
		}
		allsrc_count++;
		if (allsrc_count == 1)
			Var(ALLSRC_INDEX, gn) = target;
		else {
			if (allsrc_count == 2) {
				Buf_Init(&allsrc, 0);
				Buf_AddString(&allsrc,
				    Var(ALLSRC_INDEX, gn));
			}
			Buf_AddSpace(&allsrc);
			Buf_AddString(&allsrc, target);
		}
	}

	if (allsrc_count > 1)
		Var(ALLSRC_INDEX, gn) = Buf_Retrieve(&allsrc);
	if (oodate_count > 1)
		Var(OODATE_INDEX, gn) = Buf_Retrieve(&oodate);

	if (gn->impliedsrc)
		Var(IMPSRC_INDEX, gn) = Var(TARGET_INDEX, gn->impliedsrc);

	if (gn->type & OP_JOIN)
		Var(TARGET_INDEX, gn) = Var(ALLSRC_INDEX, gn);
}

/* Wrapper to call Make_TimeStamp from a forEach loop.	*/
static void
MakeTimeStamp(void *parent, void *child)
{
    Make_TimeStamp((GNode *)parent, (GNode *)child);
}

bool
Make_OODate(GNode *gn)
{
	bool	    oodate;

	/*
	 * Certain types of targets needn't even be sought as their datedness
	 * doesn't depend on their modification time...
	 */
	if ((gn->type & (OP_JOIN|OP_USE|OP_EXEC|OP_PHONY)) == 0) {
		(void)Dir_MTime(gn);
		if (DEBUG(MAKE)) {
			if (!is_out_of_date(gn->mtime))
				printf("modified %s...",
				    time_to_string(gn->mtime));
			else
				printf("non-existent...");
		}
	}

	/*
	 * A target is remade in one of the following circumstances:
	 * - its modification time is smaller than that of its youngest child
	 *   and it would actually be run (has commands or type OP_NOP)
	 * - it's the object of a force operator
	 * - it has no children, was on the lhs of an operator and doesn't
	 *   exist already.
	 *
	 * Libraries are only considered out-of-date if the archive module says
	 * they are.
	 */
	if (gn->type & OP_USE) {
		/*
		 * If the node is a USE node it is *never* out of date
		 * no matter *what*.
		 */
		if (DEBUG(MAKE))
			printf(".USE node...");
		oodate = false;
	} else if ((gn->type & OP_LIB) && Arch_IsLib(gn)) {
		if (DEBUG(MAKE))
		    printf("library...");

		/* always out of date if no children and :: target */
		oodate = Arch_LibOODate(gn) ||
		    (is_out_of_date(gn->cmtime) && (gn->type & OP_DOUBLEDEP));
	} else if (gn->type & OP_JOIN) {
		/*
		 * A target with the .JOIN attribute is only considered
		 * out-of-date if any of its children was out-of-date.
		 */
		if (DEBUG(MAKE))
			printf(".JOIN node...");
		oodate = gn->childMade;
	} else if (gn->type & (OP_FORCE|OP_EXEC|OP_PHONY)) {
		/*
		 * A node which is the object of the force (!) operator or which
		 * has the .EXEC attribute is always considered out-of-date.
		 */
		if (DEBUG(MAKE)) {
			if (gn->type & OP_FORCE)
				printf("! operator...");
			else if (gn->type & OP_PHONY)
				printf(".PHONY node...");
			else
				printf(".EXEC node...");
		}
		oodate = true;
	} else if (is_strictly_before(gn->mtime, gn->cmtime) ||
	   (is_out_of_date(gn->cmtime) &&
	    (is_out_of_date(gn->mtime) || (gn->type & OP_DOUBLEDEP)))) {
		/*
		 * A node whose modification time is less than that of its
		 * youngest child or that has no children (cmtime ==
		 * OUT_OF_DATE) and either doesn't exist (mtime == OUT_OF_DATE)
		 * or was the object of a :: operator is out-of-date.
		 */
		if (DEBUG(MAKE)) {
			if (is_strictly_before(gn->mtime, gn->cmtime))
				printf("modified before source...");
			else if (is_out_of_date(gn->mtime))
				printf("non-existent and no sources...");
			else
				printf(":: operator and no sources...");
		}
		oodate = true;
	} else {
		oodate = false;
	}

	/*
	 * If the target isn't out-of-date, the parents need to know its
	 * modification time. Note that targets that appear to be out-of-date
	 * but aren't, because they have no commands and aren't of type OP_NOP,
	 * have their mtime stay below their children's mtime to keep parents
	 * from thinking they're out-of-date.
	 */
	if (!oodate)
		Lst_ForEach(&gn->parents, MakeTimeStamp, gn);

	return oodate;
}

volatile sig_atomic_t got_signal;

volatile sig_atomic_t got_SIGINT, got_SIGHUP, got_SIGQUIT, got_SIGTERM;

static void
setup_signal(int sig, psighandler h)
{
	if (signal(sig, SIG_IGN) != SIG_IGN)
		(void)signal(sig, h);
}

void
setup_all_signals(psighandler interrupt, psighandler jc)
{
	/*
	 * Catch the four signals that POSIX specifies if they aren't ignored.
	 * handle_signal will take care of calling JobInterrupt if appropriate.
	 */
	setup_signal(SIGINT, interrupt);
	setup_signal(SIGHUP, interrupt);
	setup_signal(SIGQUIT, interrupt);
	setup_signal(SIGTERM, interrupt);
	setup_signal(SIGTSTP, jc);
	setup_signal(SIGTTOU, jc);
	setup_signal(SIGTTIN, jc);
	setup_signal(SIGWINCH, jc);
	setup_signal(SIGCONT, jc);
	got_signal = 0;
}

void
SigHandler(int sig)
{
	switch(sig) {
	case SIGINT:
		got_SIGINT++;
		got_signal = 1;
		break;
	case SIGHUP:
		got_SIGHUP++;
		got_signal = 1;
		break;
	case SIGQUIT:
		got_SIGQUIT++;
		got_signal = 1;
		break;
	case SIGTERM:
		got_SIGTERM++;
		got_signal = 1;
		break;
	}
}

/* The following array is used to make a fast determination of which
 * characters are interpreted specially by the shell.  If a command
 * contains any of these characters, it is executed by the shell, not
 * directly by us.  */
static char	    meta[256];

void
setup_meta(void)
{
	char *p;

	for (p = "#=|^(){};&<>*?[]:$`\\\n"; *p != '\0'; p++)
		meta[(unsigned char) *p] = 1;
	/* The null character serves as a sentinel in the string.  */
	meta[0] = 1;
}

static char **
recheck_command_for_shell(char **av)
{
	char *runsh[] = {
		"alias", "cd", "eval", "exit", "read", "set", "ulimit",
		"unalias", "unset", "wait", "umask", NULL
	};

	char **p;

	/* optimization: if exec cmd, we avoid the intermediate shell */
	if (strcmp(av[0], "exec") == 0)
		av++;

	for (p = runsh; *p; p++)
		if (strcmp(av[0], *p) == 0)
			return NULL;

	return av;
}

static void
run_command(const char *cmd, bool errCheck)
{
	const char *p;
	char *shargv[4];
	char **todo;

	shargv[0] = _PATH_BSHELL;

	shargv[1] = errCheck ? "-ec" : "-c";
	shargv[2] = (char *)cmd;
	shargv[3] = NULL;

	todo = shargv;


	/* Search for meta characters in the command. If there are no meta
	 * characters, there's no need to execute a shell to execute the
	 * command.  */
	for (p = cmd; !meta[(unsigned char)*p]; p++)
		continue;
	if (*p == '\0') {
		char *bp;
		char **av;
		int argc;
		/* No meta-characters, so probably no need to exec a shell.
		 * Break the command into words to form an argument vector
		 * we can execute.  */
		av = brk_string(cmd, &argc, &bp);
		av = recheck_command_for_shell(av);
		if (av != NULL)
			todo = av;
	}
	execvp(todo[0], todo);

	if (errno == ENOENT)
		fprintf(stderr, "%s: not found\n", todo[0]);
	else
		perror(todo[0]);
	_exit(1);
}

/*-
 *-----------------------------------------------------------------------
 * setup_and_run_command --
 *	Execute the next command for a target. If the command returns an
 *	error, the node's built_status field is set to ERROR and creation stops.
 *
 * Results:
 *	0 in case of error, 1 if ok.
 *
 * Side Effects:
 *	The node's 'built_status' field may be set to ERROR.
 *-----------------------------------------------------------------------
 */
static int
setup_and_run_command(char *cmd, GNode *gn, int dont_fork)
{
	bool silent;	/* Don't print command */
	bool doExecute;	/* Execute the command */
	bool errCheck;	/* Check errors */
	int reason;	/* Reason for child's death */
	int status;	/* Description of child's death */
	pid_t cpid; 	/* Child actually found */
	pid_t stat;	/* Status of fork */

	silent = gn->type & OP_SILENT;
	errCheck = !(gn->type & OP_IGNORE);
	doExecute = !noExecute;

	/* How can we execute a null command ? we warn the user that the
	 * command expanded to nothing (is this the right thing to do?).  */
	if (*cmd == '\0') {
		Error("%s expands to empty string", cmd);
		return 1;
	}

	for (;; cmd++) {
		if (*cmd == '@')
			silent = DEBUG(LOUD) ? false : true;
		else if (*cmd == '-')
			errCheck = false;
		else if (*cmd == '+')
			doExecute = true;
		else
			break;
	}
	while (isspace(*cmd))
		cmd++;
	/* Print the command before echoing if we're not supposed to be quiet
	 * for this one. We also print the command if -n given.  */
	if (!silent || noExecute) {
		printf("%s\n", cmd);
		fflush(stdout);
	}
	/* If we're not supposed to execute any commands, this is as far as
	 * we go...  */
	if (!doExecute)
		return 1;

	/* if we're running in parallel mode, we try not to fork the last
	 * command, since it's exit status will be just fine... unless
	 * errCheck is not set, in which case we must deal with the
	 * status ourselves.
	 */
	if (dont_fork && errCheck)
		run_command(cmd, errCheck);
		/*NOTREACHED*/

	/* Fork and execute the single command. If the fork fails, we abort.  */
	switch (cpid = fork()) {
	case -1:
		Fatal("Could not fork");
		/*NOTREACHED*/
	case 0:
		run_command(cmd, errCheck);
		/*NOTREACHED*/
	default:
		break;
	}

	/* The child is off and running. Now all we can do is wait...  */
	while (1) {

		while ((stat = waitpid(cpid, &reason, 0)) != cpid) {
			if (stat == -1 && errno != EINTR)
				break;
		}

		if (got_signal)
			break;

		if (stat != -1) {
			if (WIFEXITED(reason)) {
				status = WEXITSTATUS(reason);	/* exited */
				if (status != 0)
					printf("*** Error code %d", status);
			} else {
				status = WTERMSIG(reason);	/* signaled */
				printf("*** Signal %d", status);
			}


			if (!WIFEXITED(reason) || status != 0) {
				if (errCheck) {
					gn->built_status = ERROR;
					if (keepgoing)
						/* Abort the current target,
						 * but let others continue.  */
						printf(" (continuing)\n");
				} else {
					/* Continue executing commands for
					 * this target.  If we return 0,
					 * this will happen...  */
					printf(" (ignored)\n");
					status = 0;
				}
			}
			return !status;
		} else
			Fatal("error in wait: %s", strerror(errno));
			/*NOTREACHED*/
	}
	return 0;
}

static void
handle_compat_interrupts(GNode *gn)
{
	if (!Targ_Precious(gn)) {
		char *file = Var(TARGET_INDEX, gn);

		if (!noExecute && eunlink(file) != -1)
			Error("*** %s removed\n", file);
	}
	if (got_SIGINT) {
		signal(SIGINT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		got_signal = 0;
		got_SIGINT = 0;
		run_gnode(interrupt_node);
		exit(255);
	}
	exit(255);
}

void
expand_commands(GNode *gn)
{
	LstNode ln;
	char *cmd;

	for (ln = Lst_First(&gn->commands); ln != NULL; ln = Lst_Adv(ln)) {
		cmd = Var_Subst(Lst_Datum(ln), &gn->context, false);
		Lst_AtEnd(&gn->expanded, cmd);
	}
}

int
run_gnode(GNode *gn)
{
	if (gn != NULL && (gn->type & OP_DUMMY) == 0) {
		expand_commands(gn);
		return run_prepared_gnode(gn);
	} else {
		return NOSUCHNODE;
	}
}

static int
run_prepared_gnode(GNode *gn)
{
	char *cmd;

	gn->built_status = MADE;
	while ((cmd = Lst_DeQueue(&gn->expanded)) != NULL) {
		if (setup_and_run_command(cmd, gn, 0) == 0)
			break;
		free(cmd);
	}
	free(cmd);
	if (got_signal)
		handle_compat_interrupts(gn);
	return gn->built_status;
}

void
run_gnode_parallel(GNode *gn)
{
	char *cmd;

	gn->built_status = MADE;
	/* XXX don't bother freeing cmd, we're dead anyways ! */
	while ((cmd = Lst_DeQueue(&gn->expanded)) != NULL) {
		if (setup_and_run_command(cmd, gn,
		    Lst_IsEmpty(&gn->expanded)) == 0)
			break;
	}
	/* Normally, we don't reach this point, unless the last command
	 * ignores error, in which case we interpret the status ourselves.
	 */
	switch(gn->built_status) {
	case MADE:
		exit(0);
	case ERROR:
		exit(1);
	default:
		fprintf(stderr, "Could not run gnode, returned %d\n",
		    gn->built_status);
		exit(1);
	}
}

void
setup_engine(int parallel)
{
	static int already_setup = 0;

	if (!already_setup) {
		setup_meta();
		if (parallel)
			setup_all_signals(parallel_handler, parallel_handler);
		else
			setup_all_signals(SigHandler, SIG_DFL);
		already_setup = 1;
	}
}
