/*	$OpenBSD: pmdb.h,v 1.5 2002/08/08 18:27:57 art Exp $	*/
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

#include <sys/signal.h>		/* for NSIG */
#include <sys/queue.h>
#include <sys/ptrace.h>
#include <sys/stat.h>

#include <err.h>

/* XXX - ugh, yuck, bleah. */
#ifndef PT_STEP
#define PT_STEP PT_CONTINUE
#endif

/*
 * Process handling.
 */

struct breakpoint;
struct callback;
struct corefile;
struct sym_table;
struct sym_ops;
struct reg;

/* XXX - should be machdep some day. */
typedef unsigned long reg;

/* The state for a debugged process. */
struct pstate {
	pid_t ps_pid;
	enum { NONE, LOADED, RUNNING, STOPPED, TERMINATED } ps_state;
	int ps_argc;
	char **ps_argv;
	int ps_flags;
	int ps_signum;
	int ps_sigstate[NSIG];
	reg ps_npc;
	TAILQ_HEAD(,sym_table) ps_syms;	/* all symbols tables in a list */
	struct sym_table *ps_sym_exe;	/* symbol table for the executable */
	struct sym_ops *ps_sops;	/* operations on symbol tables */
	struct stat exec_stat;		/* stat of the exec file */
	struct corefile *ps_core;	/* core file data */
	TAILQ_HEAD(,breakpoint) ps_bkpts; /* breakpoints */
	TAILQ_HEAD(,callback) ps_sstep_cbs; /* single step actions */
};

/* flags in ps_flags */
#define PSF_SYMBOLS	0x02		/* basic symbols loaded */
#define PSF_KILL	0x04		/* kill this process asap */
#define PSF_STEP	0x08		/* next continue should sstep */
#define PSF_CORE	0x10		/* core file loaded */
#define PSF_ATCH	0x20		/* process attached with PT_ATTACH */

/* ps_sigstate */
#define SS_STOP		0x00
#define SS_IGNORE	0x01

/* misc helper functions */
int process_kill(struct pstate *);
int read_from_pid(pid_t pid, off_t from, void *to, size_t size);
int write_to_pid(pid_t pid, off_t to, void *from, size_t size);

/* process.c */
int process_load(struct pstate *);
int process_run(struct pstate *);
int process_read(struct pstate *, off_t, void *, size_t);
int process_write(struct pstate *, off_t, void *, size_t);
int process_getregs(struct pstate *, struct reg *);

int cmd_process_run(int, char **, void *);
int cmd_process_cont(int, char **, void *);
int cmd_process_kill(int, char **, void *);
int cmd_process_setenv(int, char **, void *);

/* signal.c */
void init_sigstate(struct pstate *);
void process_signal(struct pstate *, int, int, int);
int cmd_signal_ignore(int, char **, void *);
int cmd_signal_show(int, char **, void *);

/*
 * Machine dependent stuff.
 */
/* register names */
struct md_def {
	const char **md_reg_names;	/* array of register names */
	const int nregs;		/* number of registers */
	const int pcoff;		/* offset of the pc */
};
extern struct md_def md_def;
void md_def_init(void);

#define MDF_MAX_ARGS	16

struct md_frame {
	reg pc, fp;
	int nargs;
	reg args[MDF_MAX_ARGS];
};

/*
 * Return the registers for the process "ps" in the frame "frame".
 */
int md_getframe(struct pstate *, int, struct md_frame *);
int md_getregs(struct pstate *, reg *);

/* misc */
void *emalloc(size_t);
