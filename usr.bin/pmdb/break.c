/*	$OpenBSD: break.c,v 1.4 2002/07/22 01:20:50 art Exp $	*/
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
#include <unistd.h>
#include <signal.h>
#include <err.h>
#include <errno.h>
#include <string.h>

#include "pmdb.h"
#include "symbol.h"
#include "pmdb_machdep.h"
#include "break.h"

struct callback {
	TAILQ_ENTRY(callback) cb_list;
	int (*cb_fun)(struct pstate *, void *);
	void *cb_arg;
};

struct breakpoint {
	TAILQ_ENTRY(breakpoint) bkpt_list;
	TAILQ_HEAD(,callback) bkpt_cbs;		/* list of all callbacks */
	char bkpt_old[BREAKPOINT_LEN];		/* old contents at bkpt */
	reg bkpt_pc;
};

static char bkpt_insn[BREAKPOINT_LEN] = BREAKPOINT;

/*
 * Find a breakpoint at this address.
 */
struct breakpoint *
bkpt_find_at_pc(struct pstate *ps, reg pc)
{
	struct breakpoint *bkpt;

	TAILQ_FOREACH(bkpt, &ps->ps_bkpts, bkpt_list)
		if (bkpt->bkpt_pc == pc)
			break;

	return (bkpt);
}

/*
 * Enable this breakpoint.
 */
static int
bkpt_enable(struct pstate *ps, struct breakpoint *bkpt)
{
	reg pc = bkpt->bkpt_pc;

	if (process_read(ps, pc, &bkpt->bkpt_old, BREAKPOINT_LEN)) {
		warn("Can't read process contents at 0x%lx", pc);
		return (-1);
	}
	if (process_write(ps, pc, &bkpt_insn, BREAKPOINT_LEN)) {
		warn("Can't write breakpoint at 0x%lx, attempting backout.", pc);
		if (process_write(ps, pc, &bkpt->bkpt_old, BREAKPOINT_LEN))
			warn("Backout failed, process unstable");
		return (-1);
	}
	return (0);
}

/*
 * Create a new breakpoint and enable it.
 */
int
bkpt_add_cb(struct pstate *ps, reg pc, int (*fun)(struct pstate *, void *),
    void *arg)
{
	struct breakpoint *bkpt;
	struct callback *cb;

	if ((bkpt = bkpt_find_at_pc(ps, pc)) == NULL) {
		bkpt = emalloc(sizeof(*bkpt));
		TAILQ_INIT(&bkpt->bkpt_cbs);
		TAILQ_INSERT_TAIL(&ps->ps_bkpts, bkpt, bkpt_list);
		bkpt->bkpt_pc = pc;
		if (bkpt_enable(ps, bkpt)) {
			free(bkpt);
			return (-1);
		}
	}

	cb = emalloc(sizeof(*cb));
	cb->cb_fun = fun;
	cb->cb_arg = arg;
	TAILQ_INSERT_TAIL(&bkpt->bkpt_cbs, cb, cb_list);

	return (0);
}

/*
 * Disable and delete a breakpoint.
 */
void
bkpt_delete(struct pstate *ps, struct breakpoint *bkpt)
{
	TAILQ_REMOVE(&ps->ps_bkpts, bkpt, bkpt_list);

	if (process_write(ps, bkpt->bkpt_pc, &bkpt->bkpt_old, BREAKPOINT_LEN))
		warn("Breakpoint removal failed, process unstable");

	free(bkpt);		
}

/*
 * Normal standard breakpoint. Keep it.
 */
static int
bkpt_normal(struct pstate *ps, void *arg)
{
	return (BKPT_KEEP_STOP);
}

/*
 * Single-step callback for "stepping over" a breakpoint (we restore the
 * breakpoint instruction to what it was, single-step over it and then
 * call this function).
 */
static int
sstep_bkpt_readd(struct pstate *ps, void *arg)
{
	reg pc = (reg)arg;

	bkpt_add_cb(ps, pc, bkpt_normal, NULL);

	return (0);	/* let the process continue */
}

/*
 * Return 0 for stop, 1 for silent continue.
 */
int
bkpt_check(struct pstate *ps)
{
	struct breakpoint *bkpt;
	struct callback *cb;
	TAILQ_HEAD(,callback) sstep_cbs;
	reg *rg, pc;
	int ret;
	int didsome = 0;
	int stop = 0;

	/* Requeue all single-step callbacks because bkpts can add ssteps. */
	TAILQ_INIT(&sstep_cbs);
	while ((cb = TAILQ_FIRST(&ps->ps_sstep_cbs)) != NULL) {
		TAILQ_REMOVE(&ps->ps_sstep_cbs, cb, cb_list);
		TAILQ_INSERT_TAIL(&sstep_cbs, cb, cb_list);
	}

	/*
	 * The default is to stop. Unless we do some processing and none
	 * of the callbacks require a stop.
	 */
	rg = alloca(sizeof(*rg) * md_def.nregs);
	if (rg == NULL)
		err(1, "bkpt_check: Can't allocate stack space.");

	if (md_getregs(ps, rg))
		err(1, "bkpt_check: Can't get registers.");

	pc = rg[md_def.pcoff];
	pc -= BREAKPOINT_DECR_PC;

	bkpt = bkpt_find_at_pc(ps, pc);
	if (bkpt == NULL)
		goto sstep;

	ps->ps_npc = pc;

	while ((cb = TAILQ_FIRST(&bkpt->bkpt_cbs)) != NULL) {
		didsome = 1;
		TAILQ_REMOVE(&bkpt->bkpt_cbs, cb, cb_list);
		ret = (*cb->cb_fun)(ps, cb->cb_arg);
		free(cb);
		switch (ret) {
		case BKPT_DEL_STOP:
			stop = 1;
		case BKPT_DEL_CONT:
			break;
		case BKPT_KEEP_STOP:
			stop = 1;
		case BKPT_KEEP_CONT:
			sstep_set(ps, sstep_bkpt_readd, (void *)bkpt->bkpt_pc);
			break;
		default:
			errx(1, "unkonwn bkpt_fun return, internal error");
		}
	}

	bkpt_delete(ps, bkpt);

sstep:

	while ((cb = TAILQ_FIRST(&sstep_cbs)) != NULL) {
		didsome = 1;
		TAILQ_REMOVE(&sstep_cbs, cb, cb_list);
		stop |= (*cb->cb_fun)(ps, cb->cb_arg);
		free(cb);
	}
	ps->ps_flags &= ~PSF_STEP;

	return (didsome && !stop);
}

int
cmd_bkpt_add(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;
	char *ep, *bkpt_name;
	reg pc;

        if (ps->ps_state != STOPPED && ps->ps_state != LOADED) {
                fprintf(stderr, "Process not loaded and stopped %d\n",
                    ps->ps_state);
                return (0);
        }

	bkpt_name = argv[1];
	pc = strtol(bkpt_name, &ep, 0);
	if (bkpt_name[0] == '\0' || *ep != '\0' || pc < 1) {
		if (sym_lookup(ps, bkpt_name, &pc)) {
			warnx("%s is not a valid pc", bkpt_name);
			return (0);
		}
	}

	if (bkpt_add_cb(ps, pc, bkpt_normal, 0))
		warn("Can't set break point");

	return (0);
}

static int
sstep_normal(struct pstate *ps, void *arg)
{
	return (1);	/* stop the command line. */
}

int
cmd_sstep(int argc, char **argv, void *arg)
{
	struct pstate *ps = arg;

        if (ps->ps_state != STOPPED) {
                fprintf(stderr, "Process not loaded and stopped %d\n",
                    ps->ps_state);
                return 0;
        }

	if (sstep_set(ps, sstep_normal, NULL))
		warn("Can't set single step");

	return (cmd_process_cont(argc, argv, arg));
}

int
sstep_set(struct pstate *ps, int (*fun)(struct pstate *, void *), void *arg)
{
	struct callback *cb;

	cb = emalloc(sizeof(*cb));
	cb->cb_fun = fun;
	cb->cb_arg = arg;
	TAILQ_INSERT_TAIL(&ps->ps_sstep_cbs, cb, cb_list);

	ps->ps_flags |= PSF_STEP;

	return (0);
}
