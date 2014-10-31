/*	$OpenBSD: engine.c,v 1.49 2014/10/31 13:29:42 gsoares Exp $ */
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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
#include "lowparse.h"

static void MakeTimeStamp(void *, void *);
static int rewrite_time(const char *);
static void setup_meta(void);
static void setup_engine(void);
static char **recheck_command_for_shell(char **);
static void list_parents(GNode *, FILE *);

/* XXX due to a bug in make's logic, targets looking like *.a or -l*
 * have been silently dropped when make couldn't figure them out.
 * Now, we warn about them until all Makefile bugs have been fixed.
 */
static bool
drop_silently(const char *s)
{
	size_t len;

	if (s[0] == '-' && s[1] == 'l')
		return true;

	len = strlen(s);
	if (len >=2 && s[len-2] == '.' && s[len-1] == 'a')
		return true;
	return false;
}

bool
node_find_valid_commands(GNode *gn)
{
	/* Alter our type to tell if errors should be ignored or things
	 * should not be printed so setup_and_run_command knows what to do.
	 */
	if (Targ_Ignore(gn))
		gn->type |= OP_IGNORE;
	if (Targ_Silent(gn))
		gn->type |= OP_SILENT;

	if (DEBUG(DOUBLE) && (gn->type & OP_DOUBLE))
		fprintf(stderr, "Warning: target %s had >1 lists of "
		    "shell commands (ignoring later ones)\n", gn->name);
	if (OP_NOP(gn->type) && Lst_IsEmpty(&gn->commands)) {
		if (drop_silently(gn->name)) {
			printf("Warning: target %s", gn->name);
			list_parents(gn, stdout);
			printf(" does not have any command (BUG)\n");
			return true;
		}
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

static void
list_parents(GNode *gn, FILE *out)
{
	LstNode ln;
	bool first = true;

	for (ln = Lst_First(&gn->parents); ln != NULL; ln = Lst_Adv(ln)) {
		GNode *p = Lst_Datum(ln);
		if (!p->must_make)
			continue;
		if (first) {
			fprintf(out, " (prerequisite of:");
			first = false;
		}
		fprintf(out, " %s", p->name);
	}
	if (!first)
		fprintf(out, ")");
}

void
node_failure(GNode *gn)
{
	/*
	 If the -k flag wasn't given, we stop in
	 * our tracks, otherwise we just don't update this
	 * node's parents so they never get examined.
	 */
	const char *diag;
	FILE *out;

	if (gn->type & OP_OPTIONAL) {
		out = stdout;
		diag = "(ignored)";
	} else if (keepgoing) {
		out = stdout;
		diag = "(continuing)";
	} else {
		out = stderr;
		diag = "";
	}
	fprintf(out, "make: don't know how to make %s", gn->name);
	list_parents(gn, out);
	fprintf(out, "%s\n", diag);
	if (out == stdout)
		fflush(stdout);
	else {
		print_errors();
		Punt(NULL);
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
	handle_all_signals();
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
	} else {
		const char *file = gn->path != NULL ? gn->path : gn->name;

		if (set_times(file) == -1){
			if (rewrite_time(file) == -1) {
				(void)fprintf(stderr,
				    "*** couldn't touch %s: %s", file,
				    strerror(errno));
		    	}
		}
	}
}

void
Make_TimeStamp(GNode *parent, GNode *child)
{
	if (is_strictly_before(parent->youngest->mtime, child->mtime)) {
 		parent->youngest = child;
	}
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

	if (DEBUG(DOUBLE) && (cgn->type & OP_DOUBLE))
		fprintf(stderr, 
		    "Warning: .USE %s expanded in %s had >1 lists of "
		    "shell commands (ignoring later ones)\n", 
		    cgn->name, pgn->name);
	pgn->type |= cgn->type & ~(OP_OPMASK|OP_USE|OP_TRANSFORM|OP_DOUBLE);

	/*
	 * This child node is now "made", so we decrement the count of
	 * unmade children in the parent... We also remove the child
	 * from the parent's list to accurately reflect the number of
	 * decent children the parent has. This is used by Make_Run to
	 * decide whether to queue the parent or examine its children...
	 */
	if (cgn->type & OP_USE)
		pgn->unmade--;
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

	Var(OODATE_INDEX, gn) = "";
	Var(ALLSRC_INDEX, gn) = "";

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
		   (!is_strictly_before(child->mtime, starttime) &&
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
				    time_to_string(&gn->mtime));
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
	 */
	if (gn->type & OP_USE) {
		/*
		 * If the node is a USE node it is *never* out of date
		 * no matter *what*.
		 */
		if (DEBUG(MAKE))
			printf(".USE node...");
		oodate = false;
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
	} else if (is_strictly_before(gn->mtime, gn->youngest->mtime) ||
	   (gn == gn->youngest &&
	    (is_out_of_date(gn->mtime) || (gn->type & OP_DOUBLEDEP)))) {
		/*
		 * A node whose modification time is less than that of its
		 * youngest child or that has no children (gn->youngest == gn)
		 * and either doesn't exist (mtime == OUT_OF_DATE)
		 * or was the object of a :: operator is out-of-date.
		 */
		if (DEBUG(MAKE)) {
			if (is_strictly_before(gn->mtime, gn->youngest->mtime))
				printf("modified before source(%s)...",
				    gn->youngest->name);
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

/* The following array is used to make a fast determination of which
 * characters are interpreted specially by the shell.  If a command
 * contains any of these characters, it is executed by the shell, not
 * directly by us.  */
static char	    meta[256];

void
setup_meta(void)
{
	char *p;

	for (p = "#=|^(){};&<>*?[]:$`\\\n~"; *p != '\0'; p++)
		meta[(unsigned char) *p] = 1;
	/* The null character serves as a sentinel in the string.  */
	meta[0] = 1;
}

static char **
recheck_command_for_shell(char **av)
{
	char *runsh[] = {
		"!", "alias", "cd", "eval", "exit", "read", "set", "ulimit",
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

static Job myjob;

void
job_attach_node(Job *job, GNode *node)
{
	job->node = node;
	job->node->built_status = BUILDING;
	job->next_cmd = Lst_First(&node->commands);
	job->exit_type = JOB_EXIT_OKAY;
	job->location = NULL;
	job->flags = 0;
}

void
job_handle_status(Job *job, int status)
{
	bool silent;
	int dying;

	/* if there's one job running and we don't keep going, no need 
	 * to report right now.
	 */
	if ((job->flags & JOB_ERRCHECK) && !keepgoing && runningJobs == NULL) 
		silent = !DEBUG(JOB);
	else
		silent = false;

	debug_job_printf("Process %ld (%s) exited with status %d.\n",
	    (long)job->pid, job->node->name, status);

	/* classify status */
	if (WIFEXITED(status)) {
		job->code = WEXITSTATUS(status);/* exited */
		if (job->code != 0) {
			/* if we're already dying from that signal, be silent */
			if (!silent && job->code > 128 
			    && job->code <= 128 + _NSIG) {
				dying = check_dying_signal();
				silent = dying && job->code == dying + 128;
			}
			if (!silent)
				printf("*** Error %d", job->code);
			job->exit_type = JOB_EXIT_BAD;
		} else 
			job->exit_type = JOB_EXIT_OKAY;
	} else {
		job->exit_type = JOB_SIGNALED;
		job->code = WTERMSIG(status);	/* signaled */
		/* if we're already dying from that signal, be silent */
		if (!silent) {
			dying = check_dying_signal();
			silent = dying && job->code == dying;
		}
		if (!silent)
			printf("*** Signal %d", job->code);
	}

	/* if there is a problem, what's going on ? */
	if (job->exit_type != JOB_EXIT_OKAY) {
		if (!silent)
			printf(" in target '%s'", job->node->name);
		if (job->flags & JOB_ERRCHECK) {
			job->node->built_status = ERROR;
			/* compute expensive status if we really want it */
			if ((job->flags & JOB_SILENT) && job == &myjob)
				determine_expensive_job(job);
			if (!keepgoing) {
				if (!silent)
					printf("\n");
				job->next = errorJobs;
				errorJobs = job;
				/* XXX don't free the command */
				return;
			}
			printf(", line %lu of %s", job->location->lineno, 
			    job->location->fname);
			if ((job->flags & (JOB_SILENT | JOB_IS_EXPENSIVE)) 
			    == JOB_SILENT)
				printf(": %s", job->cmd);
			/* Abort the current target,
			 * but let others continue.  */
			printf(" (continuing)\n");
		} else {
			/* Continue executing commands for
			 * this target.  If we return 0,
			 * this will happen...  */
			printf(" (ignored)\n");
			job->exit_type = JOB_EXIT_OKAY;
		}
	}
	free(job->cmd);
}

int
run_gnode(GNode *gn)
{
	if (!gn || (gn->type & OP_DUMMY))
		return NOSUCHNODE;

	gn->built_status = MADE;

	job_attach_node(&myjob, gn);
	while (myjob.exit_type == JOB_EXIT_OKAY) {
		bool finished = job_run_next(&myjob);
		if (finished)
			break;
		handle_one_job(&myjob);
	}

	return gn->built_status;
}


static void
setup_engine(void)
{
	static int already_setup = 0;

	if (!already_setup) {
		setup_meta();
		already_setup = 1;
	}
}

static bool
do_run_command(Job *job)
{
	bool silent;	/* Don't print command */
	bool doExecute;	/* Execute the command */
	bool errCheck;	/* Check errors */
	pid_t cpid; 	/* Child pid */

	const char *cmd = job->cmd;
	silent = job->node->type & OP_SILENT;
	errCheck = !(job->node->type & OP_IGNORE);
	if (job->node->type & OP_MAKE)
		doExecute = true;
	else
		doExecute = !noExecute;

	/* How can we execute a null command ? we warn the user that the
	 * command expanded to nothing (is this the right thing to do?).  */
	if (*cmd == '\0') {
		Error("%s expands to empty string", cmd);
		return false;
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
	while (ISSPACE(*cmd))
		cmd++;
	/* Print the command before fork if make -n or !silent*/
	if ( noExecute || !silent)
		printf("%s\n", cmd);
	
	if (silent)
		job->flags |= JOB_SILENT;
	else
		job->flags &= ~JOB_SILENT;

	/* If we're not supposed to execute any commands, this is as far as
	 * we go...  */
	if (!doExecute)
		return false;
	/* always flush for other stuff */
	fflush(stdout);

	/* Fork and execute the single command. If the fork fails, we abort.  */
	switch (cpid = fork()) {
	case -1:
		Punt("Could not fork");
		/*NOTREACHED*/
	case 0:
		/* put a random delay unless we're the only job running
		 * and there's nothing left to do.
		 */
		if (random_delay)
			if (!(runningJobs == NULL && no_jobs_left()))
				usleep(arc4random_uniform(random_delay));
		run_command(cmd, errCheck);
		/*NOTREACHED*/
	default:
		job->pid = cpid;
		job->next = runningJobs;
		runningJobs = job;
		if (errCheck)
			job->flags |= JOB_ERRCHECK;
		else
			job->flags &= ~JOB_ERRCHECK;
		debug_job_printf("Running %ld (%s) %s\n", (long)job->pid, 
		    job->node->name, (noExecute || !silent) ? "" : cmd);
		return true;
	}
}

bool
job_run_next(Job *job)
{
	bool started;
	GNode *gn = job->node;

	setup_engine();
	while (job->next_cmd != NULL) {
		struct command *command = Lst_Datum(job->next_cmd);

		handle_all_signals();
		job->location = &command->location;
		Parse_SetLocation(job->location);
		job->cmd = Var_Subst(command->string, &gn->context, false);
		job->next_cmd = Lst_Adv(job->next_cmd);
		if (fatal_errors)
			Punt(NULL);
		started = do_run_command(job);
		if (started)
			return false;
		else
			free(job->cmd);
	}
	job->exit_type = JOB_EXIT_OKAY;
	return true;
}

