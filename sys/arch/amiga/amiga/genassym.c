/*	$OpenBSD: genassym.c,v 1.5 1996/05/02 06:43:17 niklas Exp $	*/
/*	$NetBSD: genassym.c,v 1.25 1996/04/21 21:07:01 veego Exp $	*/

/*
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *	@(#)genassym.c	7.8 (Berkeley) 5/7/91
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/trap.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/vmparam.h>
#include <machine/pte.h>

#include <amiga/amiga/cia.h>
#include <amiga/amiga/isr.h>

int main __P((void));

int
main()
{
	register struct proc *p = (struct proc *)0;
	register struct vmmeter *vm = (struct vmmeter *)0;
	register struct user *up = (struct user *)0;
	struct frame *frame = NULL;
	struct vmspace *vms = (struct vmspace *)0;
	pmap_t pmap = (pmap_t)0;
	struct pcb *pcb = (struct pcb *)0;
	struct CIA *cia = (struct CIA *)0;
	struct isr *isr = (struct isr *)0;
	struct mdproc *mdproc = (struct mdproc *)0;

	printf("#define\tP_FORW %p\n", (void *)&p->p_forw);
	printf("#define\tP_BACK %p\n", (void *)&p->p_back);
	printf("#define\tP_VMSPACE %p\n", (void *)&p->p_vmspace);
	printf("#define\tP_ADDR %p\n", (void *)&p->p_addr);
	printf("#define\tP_PRIORITY %p\n", (void *)&p->p_priority);
	printf("#define\tP_STAT %p\n", (void *)&p->p_stat);
	printf("#define\tP_WCHAN %p\n", (void *)&p->p_wchan);
	printf("#define\tP_MD %p\n", (void *)&p->p_md);
	printf("#define\tP_PID %p\n", (void *)&p->p_pid);
	printf("#define\tMD_REGS %p\n", (void *)&mdproc->md_regs);
	printf("#define\tSRUN %d\n", SRUN);
	
	printf("#define\tPM_STCHG %p\n", (void *)&pmap->pm_stchanged);

	printf("#define\tVM_PMAP %p\n", (void *)&vms->vm_pmap);
	printf("#define\tV_INTR %p\n", (void *)&vm->v_intr);
	
	printf("#define\tUPAGES %d\n", UPAGES);
	printf("#define\tUSPACE %d\n", USPACE);
	printf("#define\tNBPG %d\n", NBPG);
	printf("#define\tPGSHIFT %d\n", PGSHIFT);
	printf("#define\tUSRSTACK %d\n", USRSTACK);

	printf("#define\tU_PROF %p\n", (void *)&up->u_stats.p_prof);
	printf("#define\tU_PROFSCALE %p\n",
				(void *)&up->u_stats.p_prof.pr_scale);
	printf("#define\tT_BUSERR %d\n", T_BUSERR);
	printf("#define\tT_ADDRERR %d\n", T_ADDRERR);
	printf("#define\tT_ILLINST %d\n", T_ILLINST);
	printf("#define\tT_ZERODIV %d\n", T_ZERODIV);
	printf("#define\tT_CHKINST %d\n", T_CHKINST);
	printf("#define\tT_TRAPVINST %d\n", T_TRAPVINST);
	printf("#define\tT_PRIVINST %d\n", T_PRIVINST);
	printf("#define\tT_TRACE %d\n", T_TRACE);
	printf("#define\tT_MMUFLT %d\n", T_MMUFLT);
	printf("#define\tT_FMTERR %d\n", T_FMTERR);
	printf("#define\tT_COPERR %d\n", T_COPERR);
	printf("#define\tT_FPERR %d\n", T_FPERR);
	printf("#define\tT_ASTFLT %d\n", T_ASTFLT);
	printf("#define\tT_TRAP15 %d\n", T_TRAP15);
	printf("#define\tPSL_S %d\n", PSL_S);
	printf("#define\tPSL_IPL7 %d\n", PSL_IPL7);
	printf("#define\tPSL_IPL %d\n", PSL_IPL);
	printf("#define\tPSL_LOWIPL %d\n", PSL_LOWIPL);
	printf("#define\tPSL_HIGHIPL %d\n", PSL_HIGHIPL);
	printf("#define\tPSL_USER %d\n", PSL_USER);
	printf("#define\tSPL1 %d\n", PSL_S | PSL_IPL1);
	printf("#define\tSPL2 %d\n", PSL_S | PSL_IPL2);
	printf("#define\tSPL3 %d\n", PSL_S | PSL_IPL3);
	printf("#define\tSPL4 %d\n", PSL_S | PSL_IPL4);
	printf("#define\tSPL5 %d\n", PSL_S | PSL_IPL5);
	printf("#define\tSPL6 %d\n", PSL_S | PSL_IPL6);
	printf("#define\tFC_USERD %d\n", FC_USERD);
	printf("#define\tFC_SUPERD %d\n", FC_SUPERD);
	printf("#define\tCACHE_ON %d\n", CACHE_ON);
	printf("#define\tCACHE_OFF %d\n", CACHE_OFF);
	printf("#define\tCACHE_CLR %d\n", CACHE_CLR);
	printf("#define\tIC_CLEAR %d\n", IC_CLEAR);
	printf("#define\tDC_CLEAR %d\n", DC_CLEAR);
	printf("#define\tCACHE40_ON %d\n", CACHE40_ON);
	printf("#define\tCACHE40_OFF %d\n", CACHE40_OFF);
	printf("#define\tPG_V %d\n", PG_V);
	printf("#define\tPG_NV %d\n", PG_NV);
	printf("#define\tPG_RO %d\n", PG_RO);
	printf("#define\tPG_RW %d\n", PG_RW);
	printf("#define\tPG_CI %d\n", PG_CI);
	printf("#define\tPG_PROT %d\n", PG_PROT);
	printf("#define\tPG_FRAME %d\n", PG_FRAME);
	printf("#define\tPCB_FLAGS %p\n", (void *)&pcb->pcb_flags);
	printf("#define\tPCB_PS %p\n", (void *)&pcb->pcb_ps);
	printf("#define\tPCB_USTP %p\n", (void *)&pcb->pcb_ustp);
	printf("#define\tPCB_USP %p\n", (void *)&pcb->pcb_usp);
	printf("#define\tPCB_REGS %p\n", (void *)pcb->pcb_regs);
	printf("#define\tPCB_CMAP2 %p\n", (void *)&pcb->pcb_cmap2);
	printf("#define\tPCB_ONFAULT %p\n", (void *)&pcb->pcb_onfault);
	printf("#define\tPCB_FPCTX %p\n", (void *)&pcb->pcb_fpregs);
	printf("#define\tSIZEOF_PCB %d\n", sizeof(struct pcb));

	printf("#define\tFR_SP %p\n", (void *)&frame->f_regs[15]);
	printf("#define\tFR_HW %p\n", (void *)&frame->f_sr);
	printf("#define\tFR_ADJ %p\n", (void *)&frame->f_stackadj);

	printf("#define\tSP %d\n", SP);
	printf("#define\tSYS_exit %d\n", SYS_exit);
	printf("#define\tSYS_execve %d\n", SYS_execve);
	printf("#define\tSYS_sigreturn %d\n", SYS_sigreturn);
	printf("#define\tCIAICR %p\n", (void *)&cia->icr);
	printf("#define\tAMIGA_68020 %ld\n", AMIGA_68020);
	printf("#define\tAMIGA_68030 %ld\n", AMIGA_68030);
	printf("#define\tAMIGA_68040 %ld\n", AMIGA_68040);
	printf("#define\tAMIGA_68060 %ld\n", AMIGA_68060);
	printf("#define\tISR_FORW %p\n", (void *)&isr->isr_forw);
	printf("#define\tISR_INTR %p\n", (void *)&isr->isr_intr);
	printf("#define\tISR_ARG %p\n", (void *)&isr->isr_arg);
	printf("#define\tMMU_68040 %d\n", MMU_68040);
	exit(0);
}
