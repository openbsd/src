/*	$NetBSD: process_machdep.c,v 1.20 1996/01/13 06:14:44 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * From:
 *	Id: procfs_i386.c,v 4.1 1993/12/17 10:47:45 jsp Rel
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
#include <sys/ptrace.h>

#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/segments.h>

#ifdef VM86
#include <machine/vm86.h>
#endif

static inline struct trapframe *
process_frame(p)
	struct proc *p;
{

	return (p->p_md.md_regs);
}

static inline struct save87 *
process_fpframe(p)
	struct proc *p;
{

	return (&p->p_addr->u_pcb.pcb_savefpu);
}

int
process_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct trapframe *tf = process_frame(p);
	struct pcb *pcb = &p->p_addr->u_pcb;

#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		regs->r_gs = tf->tf_vm86_gs;
		regs->r_fs = tf->tf_vm86_fs;
		regs->r_es = tf->tf_vm86_es;
		regs->r_ds = tf->tf_vm86_ds;
		regs->r_eflags = tf->tf_eflags;
		SETFLAGS(regs->r_eflags, VM86_EFLAGS(p),
			 VM86_FLAGMASK(p)|PSL_VIF);
	} else
#endif
	{
		regs->r_gs = pcb->pcb_gs;
		regs->r_fs = pcb->pcb_fs;
		regs->r_es = tf->tf_es;
		regs->r_ds = tf->tf_ds;
		regs->r_eflags = tf->tf_eflags;
	}
	regs->r_edi = tf->tf_edi;
	regs->r_esi = tf->tf_esi;
	regs->r_ebp = tf->tf_ebp;
	regs->r_ebx = tf->tf_ebx;
	regs->r_edx = tf->tf_edx;
	regs->r_ecx = tf->tf_ecx;
	regs->r_eax = tf->tf_eax;
	regs->r_eip = tf->tf_eip;
	regs->r_cs = tf->tf_cs;
	regs->r_esp = tf->tf_esp;
	regs->r_ss = tf->tf_ss;

	return (0);
}

int
process_read_fpregs(p, regs)
	struct proc *p;
	struct fpreg *regs;
{

	if (p->p_md.md_flags & MDP_USEDFPU) {
		struct save87 *frame = process_fpframe(p);

#if NNPX > 0
		if (npxproc == p)
			npxsave();
#endif

		bcopy(frame, regs, sizeof(frame));
	} else
		bzero(regs, sizeof(regs));

	return (0);
}

int
process_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct trapframe *tf = process_frame(p);
	struct pcb *pcb = &p->p_addr->u_pcb;

	/*
	 * Check for security violations.
	 */
	if (((regs->r_eflags ^ tf->tf_eflags) & PSL_USERSTATIC) != 0 ||
	    !USERMODE(regs->r_cs, regs->r_eflags))
		return (EINVAL);

#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		tf->tf_vm86_gs = regs->r_gs;
		tf->tf_vm86_fs = regs->r_fs;
		tf->tf_vm86_es = regs->r_es;
		tf->tf_vm86_ds = regs->r_ds;
		tf->tf_eflags = regs->r_eflags;
		SETFLAGS(VM86_EFLAGS(p), regs->r_eflags,
			 VM86_FLAGMASK(p)|PSL_VIF);
	} else
#endif
	{
		extern int gdt_size;
		extern union descriptor *dynamic_gdt;

#define	verr_ldt(slot)	(slot < pcb->pcb_ldt_len && \
			 (pcb->pcb_ldt[slot].sd.sd_type & SDT_MEMRO) != 0 && \
			 pcb->pcb_ldt[slot].sd.sd_dpl == SEL_UPL && \
			 pcb->pcb_ldt[slot].sd.sd_p == 1)
#define	verr_gdt(slot)	(slot < gdt_size && \
			 (dynamic_gdt[slot].sd.sd_type & SDT_MEMRO) != 0 && \
			 dynamic_gdt[slot].sd.sd_dpl == SEL_UPL && \
			 dynamic_gdt[slot].sd.sd_p == 1)
#define	verr(sel)	(ISLDT(sel) ? verr_ldt(IDXSEL(sel)) : \
				      verr_gdt(IDXSEL(sel)))
#define	valid_sel(sel)	(ISPL(sel) == SEL_UPL && verr(sel))
#define	null_sel(sel)	(!ISLDT(sel) && IDXSEL(sel) == 0)

		if ((regs->r_gs != pcb->pcb_gs && \
		     !valid_sel(regs->r_gs) && !null_sel(regs->r_gs)) ||
		    (regs->r_fs != pcb->pcb_fs && \
		     !valid_sel(regs->r_fs) && !null_sel(regs->r_fs)))
			return (EINVAL);

		pcb->pcb_gs = regs->r_gs;
		pcb->pcb_fs = regs->r_fs;
		tf->tf_es = regs->r_es;
		tf->tf_ds = regs->r_ds;
		tf->tf_eflags = regs->r_eflags;
	}
	tf->tf_edi = regs->r_edi;
	tf->tf_esi = regs->r_esi;
	tf->tf_ebp = regs->r_ebp;
	tf->tf_ebx = regs->r_ebx;
	tf->tf_edx = regs->r_edx;
	tf->tf_ecx = regs->r_ecx;
	tf->tf_eax = regs->r_eax;
	tf->tf_eip = regs->r_eip;
	tf->tf_cs = regs->r_cs;
	tf->tf_esp = regs->r_esp;
	tf->tf_ss = regs->r_ss;

	return (0);
}

int
process_write_fpregs(p, regs)
	struct proc *p;
	struct fpreg *regs;
{
	struct save87 *frame = process_fpframe(p);

#if NNPX > 0
	if (npxproc == p)
		npxdrop();
#endif

	p->p_md.md_flags |= MDP_USEDFPU;
	bcopy(regs, frame, sizeof(frame));

	return (0);
}

int
process_sstep(p, sstep)
	struct proc *p;
{
	struct trapframe *tf = process_frame(p);

	if (sstep)
		tf->tf_eflags |= PSL_T;
	else
		tf->tf_eflags &= ~PSL_T;
	
	return (0);
}

int
process_set_pc(p, addr)
	struct proc *p;
	caddr_t addr;
{
	struct trapframe *tf = process_frame(p);

	tf->tf_eip = (int)addr;

	return (0);
}
