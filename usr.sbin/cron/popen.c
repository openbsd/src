/*	$OpenBSD: popen.c,v 1.21 2009/10/27 23:59:51 deraadt Exp $	*/

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

/* this came out of the ftpd sources; it's been modified to avoid the
 * globbing stuff since we don't need it.  also execvp instead of execv.
 */

#include "cron.h"

#define MAX_ARGV	100
#define MAX_GARGV	1000

/*
 * Special version of popen which avoids call to shell.  This ensures noone
 * may create a pipe to a hidden program as a side effect of a list or dir
 * command.
 */
static PID_T *pids;
static int fds;

FILE *
cron_popen(char *program, char *type, struct passwd *pw) {
	char *cp;
	FILE *iop;
	int argc, pdes[2];
	PID_T pid;
	char *argv[MAX_ARGV];

	if ((*type != 'r' && *type != 'w') || type[1] != '\0')
		return (NULL);

	if (!pids) {
		if ((fds = sysconf(_SC_OPEN_MAX)) <= 0)
			return (NULL);
		if (!(pids = calloc(fds, sizeof(PID_T))))
			return (NULL);
	}
	if (pipe(pdes) < 0)
		return (NULL);

	/* break up string into pieces */
	for (argc = 0, cp = program; argc < MAX_ARGV - 1; cp = NULL)
		if (!(argv[argc++] = strtok(cp, " \t\n")))
			break;
	argv[MAX_ARGV-1] = NULL;

	switch (pid = fork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		return (NULL);
		/* NOTREACHED */
	case 0:				/* child */
		if (pw) {
#ifdef LOGIN_CAP
			if (setusercontext(0, pw, pw->pw_uid, LOGIN_SETALL) < 0) {
				fprintf(stderr,
				    "setusercontext failed for %s\n",
				    pw->pw_name);
				_exit(ERROR_EXIT);
			}
#else
			if (setgid(pw->pw_gid) < 0 ||
			    initgroups(pw->pw_name, pw->pw_gid) < 0) {
				fprintf(stderr,
				    "unable to set groups for %s\n",
				    pw->pw_name);
				_exit(1);
			}
#if (defined(BSD)) && (BSD >= 199103)
			setlogin(pw->pw_name);
#endif /* BSD */
			if (setuid(pw->pw_uid)) {
				fprintf(stderr,
				    "unable to set uid for %s\n",
				    pw->pw_name);
				_exit(1);
			}
#endif /* LOGIN_CAP */
		}
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
		execvp(argv[0], argv);
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

	return (iop);
}

int
cron_pclose(FILE *iop) {
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
