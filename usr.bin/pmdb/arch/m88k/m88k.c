/*	$OpenBSD: m88k.c,v 1.3 2006/11/19 09:27:21 miod Exp $	*/
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
#include <machine/pcb.h>
#include "pmdb.h"

static const char *md_reg_names[] = {
	"%r0",  "%r1",  "%r2",  "%r3",  "%r4",  "%r5",  "%r6",  "%r7",
	"%r8",  "%r9",  "%r10", "%r11", "%r12", "%r13", "%r14", "%r15",
	"%r16", "%r17", "%r18", "%r19", "%r20", "%r21", "%r22", "%r23",
	"%r24", "%r25", "%r26", "%r27", "%r28", "%r29", "%r30", "%sp",
	"%epsr", "%fpsr", "%fpcr",
	"%sxip", "%snip", "%sfip",
	"%ssbr",
	"%dmt0", "%dmd0", "%dma0",
	"%dmt1", "%dmd1", "%dma1",
	"%dmt2", "%dmd2", "%dma2",
	"%fpecr", "%fphs1", "%fpls1", "%fphs2", "%fpls2",
	"%fppt", "%fprh", "%fprl", "%fpit"
};

struct md_def md_def = { md_reg_names, 57, 35 };

void
md_def_init(void)
{
	/* nothing to do */
}

int
md_getframe(struct pstate *ps, int framenum, struct md_frame *fram)
{
	struct trapframe fr;
	struct reg r;
	int count;

	if (process_getregs(ps, &r) != 0)
		return (-1);

	fr.tf_sp = r.r[31];
	fr.tf_sxip = r.sxip;

	for (count = 0; count < framenum; count++) {
		if (process_read(ps, fr.tf_sp, &fr, sizeof(fr)) < 0)
			return (-1);

		if (fr.tf_sxip < 0x1000)
			return (-1);
	}

	fram->pc = fr.tf_sxip;
	fram->fp = fr.tf_sp;

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
