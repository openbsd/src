/*	$OpenBSD: popen.c,v 1.13 2002/01/09 00:51:00 millert Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)popen.c	8.3 (Berkeley) 4/6/94";
#else
static char rcsid[] = "$OpenBSD: popen.c,v 1.13 2002/01/09 00:51:00 millert Exp $";
#endif
#endif /* not lint */

#include "cron.h"

#define MAX_ARGV	100
#define MAX_GARGV	1000
#define WANT_GLOBBING 0

/*
 * Special version of popen which avoids call to shell.  This ensures noone
 * may create a pipe to a hidden program as a side effect of a list or dir
 * command.
 */
static PID_T *pids;
static int fds;

FILE *
cron_popen(program, type, e)
	char *program;
	char *type;
	entry *e;
{
	char *cp;
	FILE *iop;
	int argc, pdes[2];
	PID_T pid;
	char *argv[MAX_ARGV];
#if WANT_GLOBBING
	char **pop, *gargv[MAX_GARGV];
	int gargc;
#endif

	if ((*type != 'r' && *type != 'w') || type[1])
		return (NULL);

	if (!pids) {
		if ((fds = sysconf(_SC_OPEN_MAX)) <= 0)
			return (NULL);
		if (!(pids = (PID_T *)malloc((size_t)(fds * sizeof(int)))))
			return (NULL);
		bzero(pids, fds * sizeof(PID_T));
	}
	if (pipe(pdes) < 0)
		return (NULL);

	/* break up string into pieces */
	for (argc = 0, cp = program;argc < MAX_ARGV-1; cp = NULL)
		if (!(argv[argc++] = strtok(cp, " \t\n")))
			break;
	argv[MAX_ARGV-1] = NULL;

#if WANT_GLOBBING
	/* glob each piece */
	gargv[0] = argv[0];
	for (gargc = argc = 1; argv[argc]; argc++) {
		glob_t gl;

		bzero(&gl, sizeof(gl));
		if (glob(argv[argc],
		    GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE|GLOB_LIMIT,
		    NULL, &gl)) {
			if (gargc < MAX_GARGV-1) {
				gargv[gargc++] = strdup(argv[argc]);
				if (gargv[gargc -1] == NULL)
					fatal ("Out of memory");
			}

		} else
			for (pop = gl.gl_pathv; *pop && gargc < MAX_GARGV-1; pop++) {
				gargv[gargc++] = strdup(*pop);
				if (gargv[gargc - 1] == NULL)
					fatal ("Out of memory");
			}
		globfree(&gl);
	}
	gargv[gargc] = NULL;
#endif

	iop = NULL;

	switch(pid = fork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		goto pfree;
		/* NOTREACHED */
	case 0:				/* child */
		if (e) {
#if defined(LOGIN_CAP)
			struct passwd *pwd;

			pwd = getpwuid(e->uid);
			if (pwd == NULL) {
				fprintf(stderr, "getpwuid: couldn't get entry for %d\n", e->uid);
				_exit(ERROR_EXIT);
			}
			if (setusercontext(0, pwd, e->uid, LOGIN_SETALL) < 0) {
				fprintf(stderr, "setusercontext failed for %d\n", e->uid);
				_exit(ERROR_EXIT);
			}
#else
			if (setgid(e->gid) ||
				setgroups(0, NULL) ||
				initgroups(env_get("LOGNAME", e->envp), e->gid))
				    _exit(1);
			setlogin(env_get("LOGNAME", e->envp));
			if (setuid(e->uid))
				_exit(1);
			chdir(env_get("HOME", e->envp));
#endif /* LOGIN_CAP */
		}
		closelog();
		if (*type == 'r') {
			if (pdes[1] != STDOUT) {
				dup2(pdes[1], STDOUT);
				(void)close(pdes[1]);
			}
			dup2(STDOUT, STDERR);	/* stderr too! */
			(void)close(pdes[0]);
		} else {
			if (pdes[0] != STDIN) {
				dup2(pdes[0], STDIN);
				(void)close(pdes[0]);
			}
			(void)close(pdes[1]);
		}
#if WANT_GLOBBING
		execvp(gargv[0], gargv);
#else
		execvp(argv[0], argv);
#endif
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

pfree:
#if WANT_GLOBBING
for (argc = 1; gargv[argc] != NULL; argc++)
		free(gargv[argc]);
#endif
	return (iop);
}

int
cron_pclose(iop)
	FILE *iop;
{
	int fdes;
	PID_T pid;
	WAIT_T status;
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
		return (pid);
	if (WIFEXITED(status))
		return (WEXITSTATUS(status));
	return (1);
}
