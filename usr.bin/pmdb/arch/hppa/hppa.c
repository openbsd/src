/*	$OpenBSD: hppa.c,v 1.6 2003/01/16 00:02:46 mickey Exp $	*/

/*
 * Copyright (c) 2002-2003 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include "pmdb.h"

static const char *md_reg_names[] = {
	"%pc", "%npc",
	"%sar", "%r1",  "%rp",  "%r3",  "%r4",  "%r5",  "%r6",  "%r7",
	"%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15",
	"%r16", "%r17", "%r18", "%r19", "%r20", "%r21", "%r22", "%r23",
	"%r24", "%r25", "%r26", "%r27", "%r28", "%r29", "%r30", "%r31"
};

struct md_def md_def = { md_reg_names, 34, 0 };

void
md_def_init(void)
{
	/* no need to do anything */
}

int
md_getframe(struct pstate *ps, int frame, struct md_frame *fram)
{
	reg fr[32];
	struct reg r;
	reg fp, pc, rp;
	int i;

	if (process_getregs(ps, &r))
		return (-1);

	if (frame == 0) {
		fram->pc = r.r_pc;
		fram->fp = r.r_regs[3];
		return (0);
	}

	rp = r.r_regs[2];
	fp = r.r_regs[3];
	pc = r.r_pc;

	for (i = 1; i < frame; i++) {

		if (process_read(ps, fp-60, &fr, sizeof(fr)) < 0)
			return (-1);

		pc = rp;
		fp = fr[15];
		rp = fr[10];
	}
	fram->pc = pc;
	fram->fp = fp;

	fram->nargs = 4;		/* XXX real number is in the symtab */
	fram->args[3] = fr[23];
	fram->args[2] = fr[24];
	fram->args[1] = fr[25];
	fram->args[0] = fr[26];

	return (0);
}

int
md_getregs(struct pstate *ps, reg *regs)
{
	struct reg r;
	int i;

	if (process_getregs(ps, &r))
		return (-1);

	regs[0] = r.r_pc;
	regs[1] = r.r_npc;

	for (i = 0; i < 32; i++)
		regs[2 + i] = r.r_regs[i];

	return (0);
}
