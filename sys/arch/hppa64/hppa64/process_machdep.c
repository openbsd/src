/*	$OpenBSD: process_machdep.c,v 1.2 2011/04/16 22:02:32 kettenis Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/user.h>

int
process_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct trapframe *tf = p->p_md.md_regs;

	regs->r_regs[0] = tf->tf_sar;
	bcopy(&tf->tf_r1, &regs->r_regs[1], 31*8);
	regs->r_pc  = tf->tf_iioq[0];
	regs->r_npc = tf->tf_iioq[1];

	return (0);
}

int
process_read_fpregs(struct proc *p, struct fpreg *fpregs)
{
	fpu_proc_save(p);

	bcopy(&p->p_addr->u_pcb.pcb_fpstate->hfp_regs, fpregs, 32 * 8);

	return (0);
}

#ifdef PTRACE

int
process_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct trapframe *tf = p->p_md.md_regs;

	tf->tf_sar = regs->r_regs[0];
	bcopy(&regs->r_regs[1], &tf->tf_r1, 31*8);
	tf->tf_iioq[0] = regs->r_pc | 3;
	tf->tf_iioq[1] = regs->r_npc | 3;

	return (0);
}

int
process_write_fpregs(struct proc *p, struct fpreg *fpregs)
{
	fpu_proc_flush(p);

	bcopy(fpregs, &p->p_addr->u_pcb.pcb_fpstate->hfp_regs, 32 * 8);

	return (0);
}

int
process_sstep(p, sstep)
	struct proc *p;
	int sstep;
{
	if (sstep)
		return (EINVAL);

	return (0);
}

int
process_set_pc(p, addr)
	struct proc *p;
	caddr_t addr;
{
	p->p_md.md_regs->tf_iioq[1] = 4 +
	    (p->p_md.md_regs->tf_iioq[0] = (register_t)addr | 3);

	return (0);
}

#endif	/* PTRACE */
