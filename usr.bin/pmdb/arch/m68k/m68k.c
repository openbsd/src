/*	$OpenBSD: m68k.c,v 1.1 2003/05/30 19:32:53 miod Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include "pmdb.h"

static const char *md_reg_names[] = {
	"%d0", "%d1", "%d2", "%d3", "%d4", "%d5", "%d6", "%d7",
	"%a0", "%a1", "%a2", "%a3", "%a4", "%a5", "%a6", "%a7",
	"%sr", "%pc"
};

struct md_def md_def = { md_reg_names, 18, 17 };

void
md_def_init(void)
{
	/* nothing to do */
}

int
md_getframe(struct pstate *ps, int framenum, struct md_frame *fram)
{
	struct frame fr;
	struct reg r;
	int count;

	if (process_getregs(ps, &r) != 0)
		return (-1);

	fr.f_regs[15] = r.r_regs[15];	/* a7 */
	fr.f_pc = r.r_pc;
	for (count = 0; count < framenum; count++) {
		if (process_read(ps, fr.f_regs[15], &fr, sizeof(fr)) < 0)
			return (-1);

		if (fr.f_pc < 0x1000)
			return (-1);
	}

	fram->pc = fr.f_pc;
	fram->fp = fr.f_regs[15];

	return (0);
}

int
md_getregs(struct pstate *ps, reg *regs)
{
	struct reg r;

	if (process_getregs(ps, &r) != 0)
		return (-1);

	memcpy(regs, &r, sizeof(r));

	return (0);
}
