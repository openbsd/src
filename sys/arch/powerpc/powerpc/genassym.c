/*	$NetBSD: genassym.c,v 1.1 1996/09/30 16:34:46 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>

#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>

#include <machine/pcb.h>
#include <machine/pmap.h>

int
main()
{
#define	def(N,V)	printf("#define\t%s\t%d\n", #N, (V))
#define	offsetof(s,f)	(&(((s *)0)->f))

	def(FRAMELEN, FRAMELEN);
	def(FRAME_0, offsetof(struct trapframe, fixreg[0]));
	def(FRAME_1, offsetof(struct trapframe, fixreg[1]));
	def(FRAME_2, offsetof(struct trapframe, fixreg[2]));
	def(FRAME_3, offsetof(struct trapframe, fixreg[3]));
	def(FRAME_LR, offsetof(struct trapframe, lr));
	def(FRAME_CR, offsetof(struct trapframe, cr));
	def(FRAME_CTR, offsetof(struct trapframe, ctr));
	def(FRAME_XER, offsetof(struct trapframe, xer));
	def(FRAME_SRR0, offsetof(struct trapframe, srr0));
	def(FRAME_SRR1, offsetof(struct trapframe, srr1));
	def(FRAME_DAR, offsetof(struct trapframe, dar));
	def(FRAME_DSISR, offsetof(struct trapframe, dsisr));
	def(FRAME_EXC, offsetof(struct trapframe, exc));

	def(SFRAMELEN, roundup(sizeof(struct switchframe), 16));

	def(PCB_PMR, offsetof(struct pcb, pcb_pmreal));
	def(PCB_SP, offsetof(struct pcb, pcb_sp));
	def(PCB_SPL, offsetof(struct pcb, pcb_spl));
	def(PCB_FAULT, offsetof(struct pcb, pcb_onfault));

	def(PM_USRSR, offsetof(struct pmap, pm_sr[USER_SR]));
	def(PM_KERNELSR, offsetof(struct pmap, pm_sr[KERNEL_SR]));

	def(P_FORW, offsetof(struct proc, p_forw));
	def(P_BACK, offsetof(struct proc, p_back));
	def(P_ADDR, offsetof(struct proc, p_addr));

	return 0;
}
