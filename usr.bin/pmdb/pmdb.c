/*	$OpenBSD: pmdb.c,v 1.10 2002/07/22 01:20:50 art Exp $	*/
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
#include <sys/endian.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <errno.h>
#include <string.h>

#include "pmdb.h"
#include "symbol.h"
#include "clit.h"
#include "break.h"
#include "core.h"

static int cmd_show_registers(int, char **, void *);
static int cmd_show_backtrace(int, char **, void *);
static int cmd_quit(int, char **, void *);

struct clit cmds[] = {
	/* debugging info commands. */
	{ "regs", "show registers", 0, 0, cmd_show_registers, (void *)-1 },
	{ "trace", "show backtrace", 0, 0, cmd_show_backtrace, (void *)-1 },

	/* Process handling commands. */
	{ "run", "run process", 0, 0, cmd_process_run, (void *)-1 },
	{ "continue", "continue process", 0, 0, cmd_process_cont, (void *)-1 },
	{ "kill", "kill process", 0, 0, cmd_process_kill, (void *)-1 },

	/* signal handling commands. */
	{ "signal", "ignore signal", 2, 2, cmd_signal_ignore, (void *)-1 },
	{ "sigstate", "show signal state", 0, 0, cmd_signal_show, (void *)-1 },

	/* breakpoints */
	{ "break", "set breakpoint", 1, 1, cmd_bkpt_add, (void *)-1 },
	{ "step", "single step one insn", 0, 0, cmd_sstep, (void *)-1 },

	/* symbols */
	{ "sym_load", "load symbol table", 2, 2, cmd_sym_load, (void *)-1 },

	/* misc commands. */
	{ "help", "print help", 0, 1, cmd_help, NULL },
	{ "quit", "quit", 0, 0, cmd_quit, (void *)-1 },
	{ "exit", "quit", 0, 0, cmd_quit, (void *)-1 },
};

#define NCMDS	sizeof(cmds)/sizeof(cmds[0])

void
usage()
{
	extern char *__progname;

	fprintf(stderr, "Usage: %s [-c core] [-p pid] <program> args\n",
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct pstate ps;
	int i, c;
	int status;
	void *cm;
	char *pmenv, *core, *perr;
	int level;
	pid_t pid;

	if (argc < 2) {
		usage();
	}

	core = NULL;
	pid = 0;

	while ((c = getopt(argc, argv, "c:p:")) != -1) {
		switch(c) {
			case 'c':
				core = optarg;
				break;
			case 'p':
				pid = (pid_t) strtol(optarg, &perr, 10);
				if (*perr != '\0')
					errx(1, "invalid PID");
				break;
			case '?':
			default:
				usage();
				/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if ((pmenv = getenv("IN_PMDB")) != NULL) {
		level = atoi(pmenv);
		level++;
	} else
		level = 0;

	if (level > 0)
		asprintf(&prompt_add, "(%d)", level);
	asprintf(&pmenv, "%d", level);
	setenv("IN_PMDB", pmenv, 1);

	ps.ps_pid = pid;
	ps.ps_state = NONE;
	ps.ps_argc = argc;
	ps.ps_argv = argv;
	ps.ps_flags = 0;
	ps.ps_signum = 0;
	ps.ps_npc = 1;
	TAILQ_INIT(&ps.ps_bkpts);
	TAILQ_INIT(&ps.ps_sstep_cbs);

	signal(SIGINT, SIG_IGN);

	for (i = 0; i < NCMDS; i++)
		if (cmds[i].arg == (void *)-1)
			cmds[i].arg = &ps;

	md_def_init();
	init_sigstate(&ps);

	if ((core != NULL) && (read_core(core, &ps) < 0))
		warnx("failed to load core file");

	if (process_load(&ps) < 0)
		errx(1, "failed to load process");

	cm = cmdinit(cmds, NCMDS);
	while (ps.ps_state != TERMINATED) {
		int signum;
		int stopped;
		int cont;

		if (ps.ps_state == STOPPED) {
			sym_update(&ps);
		}

		if (ps.ps_state != RUNNING && cmdloop(cm) == 0) {
			cmd_quit(0, NULL, &ps);
		}

		if (ps.ps_state == TERMINATED)
			break;

		if (wait(&status) == 0)
			err(1, "wait");
		if (WIFEXITED(status)) {
			if ((ps.ps_flags & PSF_KILL) == 0) {
				ps.ps_state = NONE;
			} else {
				ps.ps_state = TERMINATED;
			}
			fprintf(stderr, "process exited with status %d\n",
			    WEXITSTATUS(status));
			continue;
		}
		if (WIFSIGNALED(status)) {
			signum = WTERMSIG(status);
			stopped = 0;
		} else {
			signum = WSTOPSIG(status);
			stopped = 1;
		}
		cont = 0;
		if (stopped)
			cont = bkpt_check(&ps);
		process_signal(&ps, signum, stopped, cont);
	}

	cmdend(cm);

	sym_destroy(&ps);

	return (0);
}

/* XXX - move to some other file. */
int
read_from_pid(pid_t pid, off_t from, void *to, size_t size)
{
	struct ptrace_io_desc piod;

	piod.piod_op = PIOD_READ_D;
	piod.piod_offs = (void *)(long)from;
	piod.piod_addr = to;
	piod.piod_len = size;

	return (ptrace(PT_IO, pid, (caddr_t)&piod, 0));
}


int
write_to_pid(pid_t pid, off_t to, void *from, size_t size)
{
	struct ptrace_io_desc piod;

	piod.piod_op = PIOD_WRITE_D;
	piod.piod_offs = (void *)(long)to;
	piod.piod_addr = from;
	piod.piod_len = size;

	return (ptrace(PT_IO, pid, (caddr_t)&piod, 0));
}

static int
cmd_show_registers(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;
	char buf[256];
	int i;
	reg *rg;

	if (ps->ps_state != STOPPED) {
		if (ps->ps_flags & PSF_CORE) {
			/* dump registers from core */
			core_printregs(ps->ps_core);
			return (0);
		}
		fprintf(stderr, "process not stopped\n");
		return (0);
	}

	rg = alloca(sizeof(*rg) * md_def.nregs);

	if (md_getregs(ps, rg))
		err(1, "can't get registers");
	for (i = 0; i < md_def.nregs; i++)
		printf("%s:\t0x%.*lx\t%s\n", md_def.md_reg_names[i],
		    (int)(sizeof(reg) * 2), (long)rg[i],
		    sym_print(ps, rg[i], buf, sizeof(buf)));
	return (0);
}

static int
cmd_show_backtrace(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;
	int i;

	if (ps->ps_state != STOPPED && !(ps->ps_flags & PSF_CORE)) {
		fprintf(stderr, "process not stopped\n");
		return (0);
	}

	/* no more than 100 frames */
	for (i = 0; i < 100; i++) {
		struct md_frame mfr;
		char namebuf[1024], *name;
		reg offs;
		int j;

		mfr.nargs = -1;

		if (md_getframe(ps, i, &mfr))
			break;

		name = sym_name_and_offset(ps, mfr.pc, namebuf,
		    sizeof(namebuf), &offs);
		if (name == NULL) {
			snprintf(namebuf, sizeof(namebuf), "0x%lx", mfr.pc);
			name = namebuf;
			offs = 0;
		}

		printf("%s(", name);
		for (j = 0; j < mfr.nargs; j++) {
			printf("0x%lx", mfr.args[j]);
			if (j < mfr.nargs - 1)
				printf(", ");
		}
		if (offs == 0) {
			printf(")\n");
		} else {
			printf(")+0x%lx\n", offs);
		}
	}

	return (0);
}

static int
cmd_quit(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;

	if ((ps->ps_flags & PSF_ATCH)) {
		if ((ps->ps_flags & PSF_ATCH) &&
		    ptrace(PT_DETACH, ps->ps_pid, NULL, 0) < 0)
			err(1, "ptrace(PT_DETACH)");
	} else {
		ps->ps_flags |= PSF_KILL;

		if (process_kill(ps))
			return (1);
	}

	ps->ps_state = TERMINATED;
	return (1);
}

/*
 * Perform command completion.
 * Pretty simple. if there are spaces in "buf", the last string is a symbol
 * otherwise it's a command.
 */
int
cmd_complt(char *buf, size_t buflen)
{
	struct clit *match;
	char *start;
	int command;
	int i, j, len;
	int onlymatch;

	command = (strchr(buf, ' ') == NULL);

	if (!command) {
		/* XXX - can't handle symbols yet. */
		return (-1);
	}

	start = buf;
	len = strlen(buf);

	match = NULL;
	for (i = 0; i < sizeof(cmds) / sizeof(cmds[i]); i++) {
		if (strncmp(start, cmds[i].cmd, len) == 0) {
			struct clit *cmdp;

			cmdp = &cmds[i];
			if (match == NULL) {
				onlymatch = 1;
				match = cmdp;
				strlcpy(buf, match->cmd, buflen);
				continue;
			}
			onlymatch = 0;
			for (j = len; j < buflen; j++) {
				if (buf[j] != cmdp->cmd[j]) {
					buf[j] = '\0';
					break;
				}
				if (cmdp->cmd[j] == '\0')
					break;
			}
		}
	}

	/*
	 * Be nice. If there could be arguments for this command and it's
	 * the only match append a space.
	 */
	if (match && onlymatch /*&& match->maxargc > 0*/)
		strlcat(buf, " ", buflen);

	return (match && onlymatch) ? 0 : -1;
}

/*
 * The "standard" wrapper
 */
void *
emalloc(size_t sz)
{
	void *ret;
	if ((ret = malloc(sz)) == NULL)
		err(1, "malloc");
	return (ret);
}
