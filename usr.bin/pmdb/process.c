/*	$PMDB: process.c,v 1.19 2002/03/11 23:39:49 art Exp $	*/
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

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>

#include "pmdb.h"
#include "symbol.h"
#include "break.h"

int
process_load(struct pstate *ps)
{
	int status;

	if (ps->ps_state == LOADED)
		return (0);

	switch (ps->ps_pid = fork()) {
	case 0:
		if (ptrace(PT_TRACE_ME, getpid(), NULL, 0) != 0)
			err(1, "ptrace(PT_TRACE_ME)");
		execvp(*ps->ps_argv, ps->ps_argv);
		err(1, "exec");
		/* NOTREACHED */
	case -1:
		err(1, "fork");
		/* NOTREACHED */
	default:
		break;
	}

	if ((ps->ps_flags & PSF_SYMBOLS) == 0) {
		sym_init_exec(ps, ps->ps_argv[0]);
		ps->ps_flags |= PSF_SYMBOLS;
	}

	if (wait(&status) == 0)
		err(1, "wait");

	ps->ps_state = LOADED;
	return 0;
}

int
process_kill(struct pstate *ps)
{
	switch(ps->ps_state) {
	case LOADED:
	case RUNNING:
	case STOPPED:
		if (ptrace(PT_KILL, ps->ps_pid, NULL, 0) != 0)
			err(1, "ptrace(PT_KILL)");
		return 1;
	default:
		return 0;
	}
}

int
cmd_process_kill(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;

	process_kill(ps);

	return 1;
}

int
process_bkpt_main(struct pstate *ps, void *arg)
{
	sym_update(ps);

	return BKPT_DEL_CONT;
}

int
cmd_process_run(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;

	if (ps->ps_state == NONE) {
		reg main_addr;

		process_load(ps);
		if (sym_lookup(ps, "main", &main_addr))
			warnx("no main");
		else if (bkpt_add_cb(ps, main_addr, process_bkpt_main, NULL))
			warn("no bkpt at main 0x%lx", main_addr);
	}

	if (ps->ps_state != LOADED) {
		fprintf(stderr, "Process already running.\n");
		return 0;
	}

	/*
	 * XXX - there isn't really any difference between STOPPED and
	 * LOADED, we should probably get rid of one.
	 */
	ps->ps_state = STOPPED;
	ps->ps_signum = 0;

	return (cmd_process_cont(argc, argv, arg));
}

int
cmd_process_cont(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;
	int signum;
	int req = (ps->ps_flags & PSF_STEP) ? PT_STEP : PT_CONTINUE;

	if (ps->ps_state != STOPPED) {
		fprintf(stderr, "Process not loaded and stopped %d\n",
		    ps->ps_state);
		return (0);
	}

	/* Catch SIGINT and SIGTRAP, pass all other signals. */
	switch (ps->ps_signum) {
	case SIGINT:
	case SIGTRAP:
		signum = 0;
		break;
	default:
		signum = ps->ps_signum;
		break;
	}

	if (ptrace(req, ps->ps_pid, (caddr_t)ps->ps_npc, signum) != 0) {
		err(1, "ptrace(%s)", req == PT_STEP ? "PT_STEP":"PT_CONTINUE");
	}

	ps->ps_state = RUNNING;
	ps->ps_npc = 1;

	return (1);
}
