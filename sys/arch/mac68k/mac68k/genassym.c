/*	$NetBSD: genassym.c,v 1.18 1996/05/05 06:18:27 briggs Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
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
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/user.h>

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/trap.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/pte.h>

#include "clockreg.h"

int
main(void)
{
	register struct proc *p = (struct proc *)0;
	register struct mdproc *mdproc = (struct mdproc *)0;
	register struct vmmeter *vm = (struct vmmeter *)0;
	register struct user *up = (struct user *)0;
	register struct rusage *rup = (struct rusage *)0;
	register struct frame *frame = (struct frame *)0;
	struct vmspace *vms = (struct vmspace *)0;
	pmap_t pmap = (pmap_t)0;
	struct pcb *pcb = (struct pcb *)0;

	printf("#define\tP_FORW %p\n", &p->p_forw);
	printf("#define\tP_BACK %p\n", &p->p_back);
	printf("#define\tP_VMSPACE %p\n", &p->p_vmspace);
	printf("#define\tP_ADDR %p\n", &p->p_addr);
	printf("#define\tP_MD %p\n", &p->p_md);
	printf("#define\tP_PID %p\n", &p->p_pid);
	printf("#define\tP_PRIORITY %p\n", &p->p_priority);
	printf("#define\tP_STAT %p\n", &p->p_stat);
	printf("#define\tP_WCHAN %p\n", &p->p_wchan);
	printf("#define\tP_FLAG %p\n", &p->p_flag);
	printf("#define\tP_MD_REGS %p\n", &p->p_md.md_regs);
	printf("#define\tP_MD_FLAGS %p\n", &p->p_md.md_flags);
	printf("#define\tSSLEEP %d\n", SSLEEP);
	printf("#define\tSRUN %d\n", SRUN);

	printf("#define\tMD_REGS %p\n", &mdproc->md_regs);

	printf("#define\tPM_STCHG %p\n", &pmap->pm_stchanged);

	printf("#define\tVM_PMAP %p\n", &vms->vm_pmap);
	printf("#define\tV_SWTCH %p\n", &vm->v_swtch);
	printf("#define\tV_TRAP %p\n", &vm->v_trap);
	printf("#define\tV_SYSCALL %p\n", &vm->v_syscall);
	printf("#define\tV_INTR %p\n", &vm->v_intr);
	printf("#define\tV_SOFT %p\n", &vm->v_soft);

	printf("#define\tUPAGES %d\n", UPAGES);
	printf("#define\tUSPACE %d\n", USPACE);
	printf("#define\tP1PAGES %d\n", P1PAGES);
	printf("#define\tCLSIZE %d\n", CLSIZE);
	printf("#define\tNBPG %d\n", NBPG);
	printf("#define\tNPTEPG %d\n", NPTEPG);
	printf("#define\tPGSHIFT %d\n", PGSHIFT);
	printf("#define\tSYSPTSIZE %d\n", SYSPTSIZE);
	printf("#define\tUSRPTSIZE %d\n", USRPTSIZE);
	printf("#define\tUSRIOSIZE %d\n", USRIOSIZE);
	printf("#define\tUSRSTACK %d\n", USRSTACK);

	printf("#define\tMSGBUFPTECNT %d\n", btoc(sizeof (struct msgbuf)));
	printf("#define\tNMBCLUSTERS %d\n", NMBCLUSTERS);
	printf("#define\tMCLBYTES %d\n", MCLBYTES);
	printf("#define\tNKMEMCLUSTERS %d\n", NKMEMCLUSTERS);

#ifdef SYSVSHM
	printf("#define\tSHMMAXPGS %d\n", SHMMAXPGS);
#endif
	printf("#define\tU_PROF %p\n", &up->u_stats.p_prof);
	printf("#define\tU_PROFSCALE %p\n", &up->u_stats.p_prof.pr_scale);
	printf("#define\tRU_MINFLT %p\n", &rup->ru_minflt);

	printf("#define\tT_BUSERR %d\n", T_BUSERR);
	printf("#define\tT_ADDRERR %d\n", T_ADDRERR);
	printf("#define\tT_ILLINST %d\n", T_ILLINST);
	printf("#define\tT_ZERODIV %d\n", T_ZERODIV);
	printf("#define\tT_CHKINST %d\n", T_CHKINST);
	printf("#define\tT_TRAPVINST %d\n", T_TRAPVINST);
	printf("#define\tT_PRIVINST %d\n", T_PRIVINST);
	printf("#define\tT_TRACE %d\n", T_TRACE);
	printf("#define\tT_MMUFLT %d\n", T_MMUFLT);
	printf("#define\tT_SSIR %d\n", T_SSIR);
	printf("#define\tT_FMTERR %d\n", T_FMTERR);
	printf("#define\tT_COPERR %d\n", T_COPERR);
	printf("#define\tT_FPERR %d\n", T_FPERR);
	printf("#define\tT_ASTFLT %d\n", T_ASTFLT);
	printf("#define\tT_TRAP15 %d\n", T_TRAP15);
	printf("#define\tT_FPEMULI %d\n", T_FPEMULI);
	printf("#define\tT_FPEMULD %d\n", T_FPEMULD);

	printf("#define\tPSL_S %d\n", PSL_S);
	printf("#define\tPSL_IPL7 %d\n", PSL_IPL7);
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
	printf("#define\tCACHE4_OFF %d\n", CACHE4_OFF);
	printf("#define\tIC_CLEAR %d\n", IC_CLEAR);
	printf("#define\tDC_CLEAR %d\n", DC_CLEAR);

	printf("#define\tPG_FRAME %d\n", PG_FRAME);

	printf("#define\tSIZEOF_PCB %d\n", sizeof(struct pcb));
	printf("#define\tPCB_FLAGS %p\n", &pcb->pcb_flags);
	printf("#define\tPCB_PS %p\n", &pcb->pcb_ps);
	printf("#define\tPCB_USTP %p\n", &pcb->pcb_ustp);
	printf("#define\tPCB_USP %p\n", &pcb->pcb_usp);
	printf("#define\tPCB_REGS %p\n", pcb->pcb_regs);
	printf("#define\tPCB_ONFAULT %p\n", &pcb->pcb_onfault);
	printf("#define\tPCB_FPCTX %p\n", &pcb->pcb_fpregs);
	printf("#define\tPCB_TRCB %d\n", 5);

	printf("#define\tFR_SP %p\n", &frame->f_regs[15]);
	printf("#define\tFR_HW %p\n", &frame->f_sr);
	printf("#define\tFR_ADJ %p\n", &frame->f_stackadj);

	printf("#define\tB_READ %d\n", B_READ);

	printf("#define\tENOENT %d\n", ENOENT);
	printf("#define\tEFAULT %d\n", EFAULT);
	printf("#define\tENAMETOOLONG %d\n", ENAMETOOLONG);

	printf("#define\tSYS_exit %d\n", SYS_exit);
	printf("#define\tSYS_execve %d\n", SYS_execve);
	printf("#define\tSYS_sigreturn %d\n", SYS_sigreturn);

	printf("#define\tMMU_68040 %d\n", MMU_68040);
	printf("#define\tMMU_68030 %d\n", MMU_68030);
	printf("#define\tMMU_68851 %d\n", MMU_68851);
	exit(0);
}
