/*	$OpenBSD: sparc64.c,v 1.6 2002/07/22 02:54:23 art Exp $	*/
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

#include <sys/param.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include "pmdb.h"

static const char *md_reg_names[] = {
	"%pc", "%npc", /* %y */
	"%o0", "%o1", "%o2", "%o3", "%o4", "%o5", "%o6", "%o7",
	"%i0", "%i1", "%i2", "%i3", "%i4", "%i5", "%i6", "%i7",
	"%g0", "%g1", "%g2", "%g3", "%g4", "%g5", "%g6", "%g7",
	"%l0", "%l1", "%l2", "%l3", "%l4", "%l5", "%l6", "%l7",
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
	struct frame64 fr;
	struct reg r;
	reg fp, pc;
	reg *outs;
	int i;

	if (process_getregs(ps, &r))
		return (-1);

	if (frame == 0) {
		pc = r.r_pc;
		fp = r.r_out[6] + BIAS;
		/*
		 * XXX - we need some kind of heuristics here to decide
		 * if the function has done a save or not and then pick
		 * the in registers. the problem is just that there are
		 * no in registers in PT_GETREGS.
		 */
		outs = (reg *)&r.r_out;
		goto out;
	}

	fp = r.r_out[6] + BIAS;
	pc = r.r_out[7];

	for (i = 1; i < frame; i++) {
		/* Too low or unaligned frame pointer? */
		if (fp < 8192 || (fp & 7) != 0)
			return (-1);

		if (process_read(ps, fp, &fr, sizeof(fr)) < 0)
			return (-1);

		fp = (unsigned long)v9next_frame((&fr));
		pc = fr.fr_pc;

		/* Too low or unaligned pc ? */
		if ((pc < 8192) || (pc & 3) != 0)
			return (-1);

		outs = (reg *)&fr.fr_arg;
	}

out:
	fram->pc = pc;
	fram->fp = fp;

	fram->nargs = 6;	/* XXX - don't know the real number */
	for (i = 0; i < 6; i++) {
		fram->args[i] = fr.fr_arg[i];
	}

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

	for (i = 0; i < 8; i++) {
		regs[2 + i] = r.r_out[i];
		regs[10 + i] = r.r_in[i];
		regs[18 + i] = r.r_global[i];
		regs[26 + i] = r.r_local[i];
	}

	return (0);
}
