/*	$OpenBSD: vax.c,v 1.4 2002/07/22 01:20:50 art Exp $	*/
/*
 * Copyright (c) 2002 Federico Schwindt <fgsch@openbsd.org>
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
	"%r0", "%r1", "%r2", "%r3", "%r4", "%r5", "%r6", "%r7", "%r8",
	"%r9", "%r10", "%r11", "%ap", "%fp", "%sp", "%pc", "%ps"
};

struct md_def md_def = { md_reg_names, 17, 16 };

void
md_def_init(void)
{
	/* no need to do anything */
}

int
md_getframe(struct pstate *ps, int frame, struct md_frame *fram)
{
	struct callsframe fr;
	struct reg r;
	int count;

	if (ptrace(PT_GETREGS, ps->ps_pid, (caddr_t)&r, 0) != 0)
		return (-1);

	fr.ca_fp = r.fp;
	fr.ca_pc = r.pc;
	for (count = 0; count < frame; count++) {
		if (process_read(ps, fr.ca_fp, &fr, sizeof(fr)) < 0)
			return (-1);

		if (fr.ca_pc < 0x1000)
			return (-1);
	}

	fram->pc = fr.ca_pc;
	fram->fp = fr.ca_fp;

	return (0);
}

int
md_getregs(struct pstate *ps, reg *regs)
{
	struct reg r;

	if (ptrace(PT_GETREGS, ps->ps_pid, (caddr_t)&r, 0) != 0)
		return (-1);

	memcpy(regs, &r, sizeof(r));

	return (0);
}
