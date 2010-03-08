/*	$OpenBSD: popen.c,v 1.24 2010/03/08 19:34:44 kettenis Exp $	*/
/*	$NetBSD: popen.c,v 1.5 1995/04/11 02:45:00 cgd Exp $	*/

/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software written by Ken Arnold and
 * published in UNIX Review, Vol. 6, No. 8.
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
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <glob.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <netinet/in.h>

#include "monitor.h"
#include "extern.h"

/*
 * Special version of popen which avoids call to shell.  This ensures noone
 * may create a pipe to a hidden program as a side effect of a list or dir
 * command.
 */
static pid_t *pids;
static int fds;

#define MAX_ARGV	100
#define MAX_GARGV	1000

FILE *
ftpd_popen(char *program, char *type)
{
	char *cp;
	FILE *iop;
	int argc, gargc, pdes[2];
	pid_t pid;
	char **pop, *argv[MAX_ARGV], *gargv[MAX_GARGV];

	if ((*type != 'r' && *type != 'w') || type[1])
		return (NULL);

	if (!pids) {
		if ((fds = getdtablesize()) <= 0)
			return (NULL);
		if ((pids = calloc(fds, sizeof(pid_t))) == NULL)
			return (NULL);
	}
	if (pipe(pdes) < 0)
		return (NULL);

	/* break up string into pieces */
	for (argc = 0, cp = program;argc < MAX_ARGV-1; cp = NULL)
		if (!(argv[argc++] = strtok(cp, " \t\n")))
			break;
	argv[MAX_ARGV-1] = NULL;

	/* glob each piece */
	gargv[0] = argv[0];
	for (gargc = argc = 1; argv[argc]; argc++) {
		glob_t gl;

		memset(&gl, 0, sizeof(gl));
		if (glob(argv[argc],
		    GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE|GLOB_LIMIT,
		    NULL, &gl)) {
			if (gargc < MAX_GARGV-1) {
				gargv[gargc++] = strdup(argv[argc]);
				if (gargv[gargc -1] == NULL)
					fatal ("Out of memory.");
			}

		} else if (gl.gl_pathc > 0) {
			for (pop = gl.gl_pathv; *pop && gargc < MAX_GARGV-1; pop++) {
				gargv[gargc++] = strdup(*pop);
				if (gargv[gargc - 1] == NULL)
					fatal ("Out of memory.");
			}
		}
		globfree(&gl);
	}
	gargv[gargc] = NULL;

	iop = NULL;

	switch (pid = fork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		goto pfree;
		/* NOTREACHED */
	case 0:				/* child */
		if (*type == 'r') {
			if (pdes[1] != STDOUT_FILENO) {
				dup2(pdes[1], STDOUT_FILENO);
				(void)close(pdes[1]);
			}
			dup2(STDOUT_FILENO, STDERR_FILENO); /* stderr too! */
			(void)close(pdes[0]);
		} else {
			if (pdes[0] != STDIN_FILENO) {
				dup2(pdes[0], STDIN_FILENO);
				(void)close(pdes[0]);
			}
			(void)close(pdes[1]);
		}
		closelog();

		if (strcmp(gargv[0], "/bin/ls") == 0) {
			extern int optreset;
			extern int ls_main(int, char **);

			/* reset getopt for ls_main */
			optreset = optind = 1;
			exit(ls_main(gargc, gargv));
		}

		execv(gargv[0], gargv);
		_exit(1);
	}
	/* parent; assume fdopen can't fail...  */
	if (*type == 'r') {
		iop = fdopen(pdes[0], type);
		(void)close(pdes[1]);
	} else {
		iop = fdopen(pdes[1], type);
		(void)close(pdes[0]);
	}
	pids[fileno(iop)] = pid;

pfree:	for (argc = 1; gargv[argc] != NULL; argc++)
		free(gargv[argc]);

	return (iop);
}

int
ftpd_pclose(FILE *iop)
{
	int fdes, status;
	pid_t pid;
	sigset_t sigset, osigset;

	/*
	 * pclose returns -1 if stream is not associated with a
	 * `popened' command, or, if already `pclosed'.
	 */
	if (pids == 0 || pids[fdes = fileno(iop)] == 0)
		return (-1);
	(void)fclose(iop);
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGQUIT);
	sigaddset(&sigset, SIGHUP);
	sigprocmask(SIG_BLOCK, &sigset, &osigset);
	while ((pid = waitpid(pids[fdes], &status, 0)) < 0 && errno == EINTR)
		continue;
	sigprocmask(SIG_SETMASK, &osigset, NULL);
	pids[fdes] = 0;
	if (pid < 0)
		return (-1);
	if (WIFEXITED(status))
		return (WEXITSTATUS(status));
	return (1);
}
