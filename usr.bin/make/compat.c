/*	$OpenPackages$ */
/*	$OpenBSD: compat.c,v 1.62 2007/10/27 08:44:12 espie Exp $	*/
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
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
#include "engine.h"
#include "compat.h"
#include "suff.h"
#include "var.h"
#include "targ.h"
#include "error.h"
#include "str.h"
#include "extern.h"
#include "memory.h"
#include "gnode.h"
#include "make.h"
#include "timestamp.h"
#include "lst.h"
#include "pathnames.h"

static void CompatInterrupt(int);
static int CompatRunCommand(LstNode, void *);
static void CompatMake(void *, void *);
static int run_gnode(GNode *);
static void setup_meta(void);
static char **recheck_command_for_shell(char **);
static void run_command(const char *, bool);

static volatile sig_atomic_t interrupted;

static void
CompatInterrupt(int signo)
{
	if (interrupted != SIGINT)
		interrupted = signo;
}

/* The following array is used to make a fast determination of which
 * characters are interpreted specially by the shell.  If a command
 * contains any of these characters, it is executed by the shell, not
 * directly by us.  */
static char	    meta[256];

static void
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
 * CompatRunCommand --
 *	Execute the next command for a target. If the command returns an
 *	error, the node's made field is set to ERROR and creation stops.
 *
 * Results:
 *	0 in case of error, 1 if ok.
 *
 * Side Effects:
 *	The node's 'made' field may be set to ERROR.
 *-----------------------------------------------------------------------
 */
static int
CompatRunCommand(LstNode cmdNode,	/* Command to execute */
    void *gnp)				/* Node from which the command came */
{
	char *cmdStart;	/* Start of expanded command */
	bool silent;	/* Don't print command */
	bool doExecute;	/* Execute the command */
	bool errCheck;	/* Check errors */
	int reason;	/* Reason for child's death */
	int status;	/* Description of child's death */
	pid_t cpid; 	/* Child actually found */
	pid_t stat;	/* Status of fork */
	char *cmd = (char *)Lst_Datum(cmdNode);
	GNode *gn = (GNode *)gnp;

	silent = gn->type & OP_SILENT;
	errCheck = !(gn->type & OP_IGNORE);
	doExecute = !noExecute;

	cmdStart = Var_Subst(cmd, &gn->context, false);

	/* How can we execute a null command ? we warn the user that the
	 * command expanded to nothing (is this the right thing to do?).  */
	if (*cmdStart == '\0') {
		free(cmdStart);
		Error("%s expands to empty string", cmd);
		return 1;
	} else
		cmd = cmdStart;

	if ((gn->type & OP_SAVE_CMDS) && (gn != end_node)) {
		Lst_AtEnd(&end_node->commands, cmdStart);
		end_node->type &= ~OP_DUMMY;
		return 1;
	} else if (strcmp(cmdStart, "...") == 0) {
		gn->type |= OP_SAVE_CMDS;
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
	free(cmdStart);

	/* The child is off and running. Now all we can do is wait...  */
	while (1) {

		while ((stat = wait(&reason)) != cpid) {
			if (stat == -1 && errno != EINTR)
				break;
		}

		if (interrupted)
			break;

		if (stat != -1) {
			if (WIFSTOPPED(reason))
				status = WSTOPSIG(reason);	/* stopped */
			else if (WIFEXITED(reason)) {
				status = WEXITSTATUS(reason);	/* exited */
				if (status != 0)
				    printf("*** Error code %d", status);
			} else {
				status = WTERMSIG(reason);	/* signaled */
				printf("*** Signal %d", status);
			}


			if (!WIFEXITED(reason) || status != 0) {
				if (errCheck) {
					gn->made = ERROR;
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
			Fatal("error in wait: %d", stat);
			/*NOTREACHED*/
	}

	/* This is reached only if interrupted */
	if (!Targ_Precious(gn)) {
		char	  *file = Varq_Value(TARGET_INDEX, gn);

		if (!noExecute && eunlink(file) != -1)
			Error("*** %s removed\n", file);
	}
	if (interrupted == SIGINT) {
		signal(SIGINT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		interrupted = 0;
		run_gnode(interrupt_node);
		exit(SIGINT);
	}
	exit(interrupted);
}

static int
run_gnode(GNode *gn)
{
	if (gn != NULL && (gn->type & OP_DUMMY) == 0) {
		Lst_ForEachNodeWhile(&gn->commands, CompatRunCommand, gn);
		return gn->made;
	} else
		return NOSUCHNODE;
}

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
	GNode *gn = (GNode *)gnp;
	GNode *pgn = (GNode *)pgnp;

	if (pgn->type & OP_MADE) {
		(void)Dir_MTime(gn);
		gn->made = UPTODATE;
	}

	if (gn->type & OP_USE) {
		Make_HandleUse(gn, pgn);
	} else if (gn->made == UNMADE) {
		/* First mark ourselves to be made, then apply whatever
		 * transformations the suffix module thinks are necessary.
		 * Once that's done, we can descend and make all our children.
		 * If any of them has an error but the -k flag was given,
		 * our 'make' field will be set false again.  This is our
		 * signal to not attempt to do anything but abort our
		 * parent as well.  */
		gn->make = true;
		gn->made = BEINGMADE;
		Suff_FindDeps(gn);
		Lst_ForEach(&gn->children, CompatMake, gn);
		if (!gn->make) {
			gn->made = ABORTED;
			pgn->make = false;
			return;
		}

		if (Lst_Member(&gn->iParents, pgn) != NULL) {
			Varq_Set(IMPSRC_INDEX, Varq_Value(TARGET_INDEX, gn),
			    pgn);
		}

		/* All the children were made ok. Now cmtime contains the
		 * modification time of the newest child, we need to find out
		 * if we exist and when we were modified last. The criteria
		 * for datedness are defined by the Make_OODate function.  */
		if (DEBUG(MAKE))
			printf("Examining %s...", gn->name);
		if (!Make_OODate(gn)) {
			gn->made = UPTODATE;
			if (DEBUG(MAKE))
				printf("up-to-date.\n");
			return;
		} else if (DEBUG(MAKE))
			printf("out-of-date.\n");

		/* If the user is just seeing if something is out-of-date,
		 * exit now to tell him/her "yes".  */
		if (queryFlag)
			exit(-1);

		/* We need to be re-made. We also have to make sure we've
		 * got a $?  variable. To be nice, we also define the $>
		 * variable using Make_DoAllVar().  */
		Make_DoAllVar(gn);

		/* Alter our type to tell if errors should be ignored or things
		 * should not be printed so CompatRunCommand knows what to do.
		 */
		if (Targ_Ignore(gn))
			gn->type |= OP_IGNORE;
		if (Targ_Silent(gn))
			gn->type |= OP_SILENT;

		if (Job_CheckCommands(gn, Fatal)) {
			/* Our commands are ok, but we still have to worry
			 * about the -t flag...	*/
			if (!touchFlag)
				run_gnode(gn);
			else
				Job_Touch(gn, gn->type & OP_SILENT);
		} else
			gn->made = ERROR;

		if (gn->made != ERROR) {
			/* If the node was made successfully, mark it so,
			 * update its modification time and timestamp all
			 * its parents.
			 * This is to keep its state from affecting that of
			 * its parent.  */
			gn->made = MADE;
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
				gn->mtime = now;
			if (is_strictly_before(gn->mtime, gn->cmtime))
				gn->mtime = gn->cmtime;
			if (DEBUG(MAKE))
				printf("update time: %s\n",
				    time_to_string(gn->mtime));
			if (!(gn->type & OP_EXEC)) {
				pgn->childMade = true;
				Make_TimeStamp(pgn, gn);
			}
		} else if (keepgoing)
			pgn->make = false;
		else {

			if (gn->lineno)
				printf("\n\nStop in %s (line %lu of %s).\n",
				    Var_Value(".CURDIR"),
				    (unsigned long)gn->lineno,
				    gn->fname);
			else
				printf("\n\nStop in %s.\n",
				    Var_Value(".CURDIR"));
			exit(1);
		}
	} else if (gn->made == ERROR)
		/* Already had an error when making this beastie. Tell the parent
		 * to abort.  */
		pgn->make = false;
	else {
		if (Lst_Member(&gn->iParents, pgn) != NULL) {
			Varq_Set(IMPSRC_INDEX, Varq_Value(TARGET_INDEX, gn),
			    pgn);
		}
		switch (gn->made) {
		case BEINGMADE:
			Error("Graph cycles through %s\n", gn->name);
			gn->made = ERROR;
			pgn->make = false;
			break;
		case MADE:
			if ((gn->type & OP_EXEC) == 0) {
				pgn->childMade = true;
				Make_TimeStamp(pgn, gn);
			}
			break;
		case UPTODATE:
			if ((gn->type & OP_EXEC) == 0)
				Make_TimeStamp(pgn, gn);
			break;
		default:
			break;
		}
	}
}

void
Compat_Run(Lst targs)		/* List of target nodes to re-create */
{
	GNode	  *gn = NULL;	/* Current root target */
	int 	  errors;   	/* Number of targets not remade due to errors */

	signal(SIGINT, CompatInterrupt);
	signal(SIGTERM, CompatInterrupt);
	signal(SIGHUP, CompatInterrupt);
	signal(SIGQUIT, CompatInterrupt);

	setup_meta();
	/* If the user has defined a .BEGIN target, execute the commands
	 * attached to it.  */
	if (!queryFlag) {
		if (run_gnode(begin_node) == ERROR) {
			printf("\n\nStop.\n");
			exit(1);
		}
	}

	/* For each entry in the list of targets to create, call CompatMake on
	 * it to create the thing. CompatMake will leave the 'made' field of gn
	 * in one of several states:
	 *	    UPTODATE	    gn was already up-to-date
	 *	    MADE	    gn was recreated successfully
	 *	    ERROR	    An error occurred while gn was being
	 *                          created
	 *	    ABORTED	    gn was not remade because one of its
	 *                          inferiors could not be made due to errors.
	 */
	errors = 0;
	while ((gn = (GNode *)Lst_DeQueue(targs)) != NULL) {
		CompatMake(gn, gn);

		if (gn->made == UPTODATE)
			printf("`%s' is up to date.\n", gn->name);
		else if (gn->made == ABORTED) {
			printf("`%s' not remade because of errors.\n",
			    gn->name);
			errors++;
		}
	}

	/* If the user has defined a .END target, run its commands.  */
	if (errors == 0)
		run_gnode(end_node);
}
