/*	$OpenBSD: process.c,v 1.7 2002/07/22 01:26:08 art Exp $	*/
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
#include <sys/stat.h>

#include <machine/reg.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pmdb.h"
#include "core.h"
#include "symbol.h"
#include "break.h"

int
process_load(struct pstate *ps)
{
	if (ps->ps_state == LOADED)
		return (0);

	if (access(*ps->ps_argv, R_OK|X_OK) < 0) {
		fprintf(stderr, "%s: %s.\n", *ps->ps_argv,
		    strerror(errno));
		return (0);
	}

	if (stat(ps->ps_argv[0], &(ps->exec_stat)) < 0)
		err(1, "stat()");

	if ((ps->ps_flags & PSF_SYMBOLS) == 0) {
		sym_init_exec(ps, ps->ps_argv[0]);
		ps->ps_flags |= PSF_SYMBOLS;
	}

	ps->ps_state = LOADED;

	if (ps->ps_pid != 0) {
		/* attach to an already running process */
		if (ptrace(PT_ATTACH, ps->ps_pid, (caddr_t) 0, 0) < 0)
			err(1, "failed to ptrace process");
		ps->ps_state = STOPPED;
		ps->ps_flags |= PSF_ATCH;
	}

	return (0);
}


int
process_run(struct pstate *ps)
{
	int status;

	if ((ps->ps_state == RUNNING) || (ps->ps_state == STOPPED)) {
		warnx("process is already running");
		return 0;
	}

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
		warnx("process started with PID %d", ps->ps_pid);
		break;
	}

	ps->ps_state = LOADED;

	if (wait(&status) == 0)
		err(1, "wait");

	return (0);
}


int
process_kill(struct pstate *ps)
{
	switch(ps->ps_state) {
	case RUNNING:
	case STOPPED:
		if (ptrace(PT_KILL, ps->ps_pid, NULL, 0) != 0)
			err(1, "ptrace(PT_KILL)");
		return (1);
	default:
		return (0);
	}
}

int
process_read(struct pstate *ps, off_t from, void *to, size_t size)
{
	struct ptrace_io_desc piod;

	if (((ps->ps_state == NONE) || (ps->ps_state == LOADED) ||
	    (ps->ps_state == TERMINATED)) && (ps->ps_flags & PSF_CORE)) {
		return core_read(ps, from, to, size);
	}
	else {
		piod.piod_op = PIOD_READ_D;
		piod.piod_offs = (void *)(long)from;
		piod.piod_addr = to;
		piod.piod_len = size;

		return (ptrace(PT_IO, ps->ps_pid, (caddr_t)&piod, 0));
	}
}

int
process_write(struct pstate *ps, off_t to, void *from, size_t size)
{
	struct ptrace_io_desc piod;

	if ((ps->ps_state == NONE) && (ps->ps_flags & PSF_CORE))
		return core_write(ps, to, from, size);
	else {
		piod.piod_op = PIOD_WRITE_D;
		piod.piod_offs = (void *)(long)to;
		piod.piod_addr = from;
		piod.piod_len = size;

		return (ptrace(PT_IO, ps->ps_pid, (caddr_t)&piod, 0));
	}
}

int
process_getregs(struct pstate *ps, struct reg *r)
{

	if (ps->ps_state == STOPPED) {
		if (ptrace(PT_GETREGS, ps->ps_pid, (caddr_t)&r, 0) != 0)
			return (-1);
	} else if (ps->ps_flags & PSF_CORE) {
		memcpy(r, ps->ps_core->regs, sizeof(*r));
	} else
		return (-1);

	return (0);
}

int
cmd_process_kill(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;

	process_kill(ps);

	return (1);
}

int
process_bkpt_main(struct pstate *ps, void *arg)
{
	sym_update(ps);

	return (BKPT_DEL_CONT);
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
		return (0);
	}

	process_run(ps);
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
