/*      $OpenBSD: mips64.c,v 1.1 2004/11/11 18:47:14 pefo Exp $       */
/*
 * Copyright (c) 2002 Dale Rahn <drahn@openbsd.org>
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
 

#include <stddef.h>
#include <string.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include "pmdb.h"

static const char *md_reg_names[] = {
	"zero", "AT", "v0", "v1", "a0", "a1", "a2", "a3",
	"a4", "a5", "a6","a7","t0","t1","t2","t3",
	"s0","s1","s2","s3","s4","s5","s6","s7",
	"t8","t9","k0","k1","gp","sp","s8","ra",
	"sr", "mullo", "mulhi", "badvaddr", "cause", "pc"
};

struct md_def md_def = { md_reg_names, 38, 38};

void
md_def_init(void)
{
	/* no need to do anything */
}

int
md_getframe(struct pstate *ps, int frame, struct md_frame *fram)
{
	struct reg r;
	reg fp, pc;

	if (process_getregs(ps, &r))
		return (-1);
	fp = r.r_regs[SP];
	if (frame == 0) {
		pc = r.r_regs[PC];
		goto out;
	}
	pc = r.r_regs[RA];

	/* XXX Digging out the frames are a little tricky
         * especially with gcc3 which throws around the code
         * quite a bit. We don't have a frame pointer and
	 * and does not know the size of the frame.
	 * Need to think about a strategy...
	 */
	if (frame > 1)
		return -1;
out:
	fram->pc = pc;
	fram->fp = fp;
	fram->nargs = 0;
	return 0;
}

int
md_getregs(struct pstate *ps, reg *regs)
{
	struct reg md_reg;
	if (process_getregs(ps, &md_reg))
		return -1;
	memcpy(regs, &md_reg, 38 * sizeof(long));
	return 0;
}
