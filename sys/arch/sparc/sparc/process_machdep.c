/*	$OpenBSD: process_machdep.c,v 1.9 2005/12/16 18:48:27 kettenis Exp $	*/
/*	$NetBSD: process_machdep.c,v 1.6 1996/03/14 21:09:26 christos Exp $ */

/*
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1993 Jan-Simon Pendry
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Id: procfs_i386.c,v 4.1 1993/12/17 10:47:45 jsp Rel
 */

/*
 * This file may seem a bit stylized, but that so that it's easier to port.
 * Functions to be implemented here are:
 *
 * process_read_regs(proc, regs)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_regs is called.
 *
 * process_write_regs(proc, regs)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	The process is stopped at the time write_regs is called.
 *
 * process_sstep(proc)
 *	Arrange for the process to trap after executing a single instruction.
 *
 * process_set_pc(proc)
 *	Set the process's program counter.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <sys/ptrace.h>

#include <sparc/sparc/cpuvar.h>

int
process_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	/* NOTE: struct reg == struct trapframe */
	bcopy(p->p_md.md_tf, (caddr_t)regs, sizeof(struct reg));
	return (0);
}

int
process_read_fpregs(p, regs)
	struct proc	*p;
	struct fpreg	*regs;
{
	extern struct fpstate	initfpstate;
	struct fpstate		*statep = &initfpstate;

	/* NOTE: struct fpreg == struct fpstate */
	if (p->p_md.md_fpstate) {
		if (p == cpuinfo.fpproc)
			savefpstate(p->p_md.md_fpstate);
		statep = p->p_md.md_fpstate;
	}
	bcopy(statep, regs, sizeof(struct fpreg));
	return 0;
}

#ifdef PTRACE

int
process_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	int	psr = p->p_md.md_tf->tf_psr & ~PSR_ICC;

	if (((regs->r_pc | regs->r_npc) & 0x03) != 0)
		return (EINVAL);

	bcopy((caddr_t)regs, p->p_md.md_tf, sizeof(struct reg));
	p->p_md.md_tf->tf_psr = psr | (regs->r_psr & PSR_ICC);
	return (0);
}

int
process_sstep(p, sstep)
	struct proc *p;
	int sstep;
{
	if (sstep)
		return EINVAL;
	return (0);
}

int
process_set_pc(p, addr)
	struct proc *p;
	caddr_t addr;
{
	if (((u_int)addr & 0x03) != 0)
		return (EINVAL);

	p->p_md.md_tf->tf_pc = (u_int)addr;
	p->p_md.md_tf->tf_npc = (u_int)addr + 4;
	return (0);
}

int
process_write_fpregs(p, regs)
	struct proc	*p;
	struct fpreg	*regs;
{
	if (p->p_md.md_fpstate == NULL)
		return EINVAL;

	if (p == cpuinfo.fpproc) {
		/* Release the fpu. */
		savefpstate(p->p_md.md_fpstate);
		cpuinfo.fpproc = NULL;
	}

	bcopy(regs, p->p_md.md_fpstate, sizeof(struct fpreg));
	return 0;
}

register_t
process_get_wcookie(p)
	struct proc *p;
{
	return p->p_addr->u_pcb.pcb_wcookie;
}

#endif	/* PTRACE */
