/* $NetBSD: genassym.c,v 1.4 1996/03/13 21:22:32 mark Exp $ */

/*-
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/device.h>
#include <sys/user.h>
#include <sys/signal.h>

#include <vm/vm.h>

#include <machine/pmap.h>
#include <machine/frame.h>
#include <machine/vmparam.h>
#include <machine/irqhandler.h>

main()
{
	struct proc *p = 0;
	struct vmmeter *vm = 0;
	struct pcb *pcb = 0;
	struct trapframe *tf = 0;
	struct sigframe *sigf = 0;
	struct uprof *uprof = 0;
	irqhandler_t *ih = 0;
	struct vconsole *vc = 0;
	struct vidc_info *vi = 0;
	struct vmspace *vms = 0;

#define	def(N,V)	printf("#define\t%s %d\n", N, V)

	def("UPAGES", UPAGES);
	def("PGSHIFT", PGSHIFT);
	def("PDSHIFT", PDSHIFT);

	def("P_ADDR", &p->p_addr);
	def("P_BACK", &p->p_back);
	def("P_FORW", &p->p_forw);
	def("P_PRIORITY", &p->p_priority);
	def("P_STAT", &p->p_stat);
	def("P_WCHAN", &p->p_wchan);
	def("P_VMSPACE", &p->p_vmspace);
	def("P_SPARE", &p->p_md.__spare);

	def("PCB_PAGEDIR", &pcb->pcb_pagedir);
	def("PCB_FLAGS", &pcb->pcb_flags);
	def("PCB_R0", &pcb->pcb_r0);
	def("PCB_R1", &pcb->pcb_r1);
	def("PCB_R2", &pcb->pcb_r2);
	def("PCB_R3", &pcb->pcb_r3);
	def("PCB_R4", &pcb->pcb_r4);
	def("PCB_R5", &pcb->pcb_r5);
	def("PCB_R6", &pcb->pcb_r6);
	def("PCB_R7", &pcb->pcb_r7);
	def("PCB_R8", &pcb->pcb_r8);
	def("PCB_R9", &pcb->pcb_r9);
	def("PCB_R10", &pcb->pcb_r10);
	def("PCB_R11", &pcb->pcb_r11);
	def("PCB_R12", &pcb->pcb_r12);
	def("PCB_SP", &pcb->pcb_sp);
	def("PCB_LR", &pcb->pcb_lr);
	def("PCB_PC", &pcb->pcb_pc);
	def("PCB_UND_SP", &pcb->pcb_und_sp);
	def("PCB_ONFAULT", &pcb->pcb_onfault);

	def("USER_SIZE", sizeof(struct user));

	def("V_TRAP", &vm->v_trap);
	def("V_INTR", &vm->v_intr);
	def("V_SOFT", &vm->v_soft);

	def("VM_MAP", &vms->vm_map);
	def("VM_PMAP", &vms->vm_pmap);

	def("PR_BASE", &uprof->pr_base);
	def("PR_SIZE", &uprof->pr_size);
	def("PR_OFF", &uprof->pr_off);
	def("PR_SCALE", &uprof->pr_scale);

	def("IH_FUNC", &ih->ih_func);
	def("IH_ARG", &ih->ih_arg);
	def("IH_LEVEL", &ih->ih_level);
	def("IH_NUM", &ih->ih_num);
	def("IH_FLAGS", &ih->ih_flags);
	def("IH_MASK", &ih->ih_irqmask);
	def("IH_BIT", &ih->ih_irqbit);
	def("IH_NEXT", &ih->ih_next);

	def("SIGF_HANDLER", &sigf->sf_handler);
	def("SIGF_SC", &sigf->sf_sc);

	def("SIGTRAP", SIGTRAP);
	def("SIGEMT", SIGEMT);

	def("TF_R0", &tf->tf_r0);
	def("TF_R10", &tf->tf_r10);
	def("TF_PC", &tf->tf_pc);

	def("PROCSIZE", sizeof(struct proc));
	def("TRAPFRAMESIZE", sizeof(struct trapframe));
	exit(0);
}
