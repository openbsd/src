/*	$OpenBSD: signal.c,v 1.5 2008/06/01 18:38:29 sobrado Exp $	*/
/*
 * Copyright (c) 2002 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

#include "pmdb.h"

void
init_sigstate(struct pstate *ps)
{
	int i;

	for (i = 1; i < NSIG; i++)
		ps->ps_sigstate[i] = SS_STOP;

	/* XXX - add more default ignored signals. */
	ps->ps_sigstate[SIGALRM] = SS_IGNORE;
	ps->ps_sigstate[SIGCHLD] = SS_IGNORE;
}

void
process_signal(struct pstate *ps, int signum, int stopped, int force_ignore)
{
	int ignore, status;

	if (stopped && (ps->ps_sigstate[signum] == SS_IGNORE || force_ignore))
		ignore = 1;
	else
		ignore = 0;

	if (force_ignore && ignore)
		signum = 0;

	ps->ps_signum = signum;

	if (!stopped) {
		/*
		 * Process terminated.
		 */
		/* Let it be restarted if it wasn't a forced termination. */
		if ((ps->ps_flags & PSF_KILL) == 0)
			ps->ps_state = NONE;
		else
			ps->ps_state = TERMINATED;
		/*
		 * Wait for it as a parent.
		 * XXX - only if we're the real parent.
		 */
		wait(&status);
	} else {
		ps->ps_state = STOPPED;
	}

	if (!ignore) {
		fprintf(stderr, "PMDB %s child. signal: %s\n",
		    stopped ? "stopping" : "terminating",
		    sys_signame[signum]);
	} else {
		cmd_process_cont(0, NULL, ps);
	}
}

int
cmd_signal_ignore(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;
	int signum;
	long l;
	char *ep;
	char *signame = argv[2];
	int newstate;

	if (!strcmp(argv[1], "ignore")) {
		newstate = SS_IGNORE;
	} else if (!strcmp(argv[1], "stop")) {
		newstate = SS_STOP;
	} else {
		goto usage;
	}

	l = strtol(signame, &ep, 0);
	if (signame[0] == '\0' || *ep != '\0' || l < 1 || l >= NSIG) {
		if (!strncmp("SIG", signame, 3))
			signame += 3;
		for (signum = 1; signum < NSIG; signum++) {
			if (!strcmp(sys_signame[signum], signame))
				break;
		}
	} else {
		signum = l;
	}

	switch (signum) {
	case SIGINT:
	case SIGSTOP:
	case SIGKILL:
		fprintf(stderr, "%s can't be ignored\n", signame);
		goto usage;
	case NSIG:
		fprintf(stderr, "%s is not a valid signal\n", signame);
		goto usage;
	default:
		break;
	}

	ps->ps_sigstate[signum] = newstate;

	return 0;
usage:
	fprintf(stderr, "usage: signal ignore|stop signum|signame\n");
	return 0;
}

int
cmd_signal_show(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;
	int i;

	for (i = 1; i < NSIG; i++) {
		char *state;

		switch (ps->ps_sigstate[i]) {
		case SS_STOP:
			state = "stop";
			break;
		case SS_IGNORE:
			state = "ignore";
			break;
		default:
			state = "error";
			break;
		}
		printf("%2d %-6s\t%s\n", i, sys_signame[i], state);
	}

	return 0;
}
