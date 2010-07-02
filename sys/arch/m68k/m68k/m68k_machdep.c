/*	$OpenBSD: m68k_machdep.c,v 1.14 2010/07/02 19:57:14 tedu Exp $	*/
/*	$NetBSD: m68k_machdep.c,v 1.3 1997/06/12 09:57:04 veego Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bernd Ernesti.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/ktrace.h>
#include <sys/user.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/reg.h>

struct cpu_info cpu_info_store;

/*
 * Set registers on exec.
 */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	struct frame *frame = (struct frame *)p->p_md.md_regs;

	frame->f_sr = PSL_USERSET;
	frame->f_pc = pack->ep_entry & ~1;
	bzero(frame->f_regs, 15 * sizeof(register_t));
	frame->f_regs[A2] = (int)PS_STRINGS;
	frame->f_regs[SP] = stack;

	/* restore a null state frame */
	p->p_addr->u_pcb.pcb_fpregs.fpf_null = 0;
	if (fputype != FPU_NONE) {
		m68881_restore(&p->p_addr->u_pcb.pcb_fpregs);
	}

	retval[1] = 0;
}

/*
 * Process the tail end of a fork() for the child
 */
void
child_return(arg)
	void *arg;
{
	struct proc *p = (struct proc *)arg;
	struct frame *f = (struct frame *)p->p_md.md_regs;

	f->f_regs[D0] = 0;
	f->f_sr &= ~PSL_C;	/* carry bit */
	f->f_format = FMT0;

	userret(p);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p,
		    (p->p_flag & P_PPWAIT) ? SYS_vfork : SYS_fork, 0, 0);
#endif
}
