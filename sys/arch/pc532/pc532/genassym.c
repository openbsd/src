/*	$NetBSD: genassym.c,v 1.8 1995/06/09 05:59:58 phil Exp $	*/

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
 *
 *	@(#)genassym.c	5.11 (Berkeley) 5/10/91
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/device.h>
#include <vm/vm.h>
#include <sys/user.h>

#include <machine/cpu.h>
#include <machine/trap.h>

#include <stdio.h>

main()
{
	struct proc *p = (struct proc *)0;
	struct user *up = (struct user *)0;
	struct rusage *rup = (struct rusage *)0;
	struct uprof *uprof = (struct uprof *)0;
	struct pcb *pcb = (struct pcb *)0;
	struct on_stack *regs = (struct on_stack *)0;
	struct iv *iv = (struct iv *)0;
	struct vmmeter *vm = 0;
	register unsigned i;

	printf("#define\tKERNBASE 0x%x\n", KERNBASE);
	printf("#define\tUDOT_SZ %d\n", sizeof(struct user));
	printf("#define\tP_FORW %d\n", &p->p_forw);
	printf("#define\tP_BACK %d\n", &p->p_back);
	printf("#define\tP_VMSPACE %d\n", &p->p_vmspace);
	printf("#define\tP_ADDR %d\n", &p->p_addr);
	printf("#define\tP_PRIORITY %d\n", &p->p_priority);
	printf("#define\tP_STAT %d\n", &p->p_stat);
	printf("#define\tP_WCHAN %d\n", &p->p_wchan);
	printf("#define\tP_FLAG %d\n", &p->p_flag);
	printf("#define\tP_PID %d\n", &p->p_pid);

	printf("#define\tSSLEEP %d\n", SSLEEP);
	printf("#define\tSRUN %d\n", SRUN);
	printf("#define\tUPAGES %d\n", UPAGES);
	printf("#define\tHIGHPAGES %d\n", HIGHPAGES);
	printf("#define\tCLSIZE %d\n", CLSIZE);
	printf("#define\tNBPG %d\n", NBPG);
	printf("#define\tNPTEPG %d\n", NPTEPG);
	printf("#define\tPGSHIFT %d\n", PGSHIFT);
	printf("#define\tSYSPTSIZE %d\n", SYSPTSIZE);
	printf("#define\tUSRPTSIZE %d\n", USRPTSIZE);

	printf("#define\tKERN_STK_START 0x%x\n",
		USRSTACK + UPAGES*NBPG);
	printf("#define\tKSTK_SIZE %d\n", UPAGES*NBPG);
	printf("#define\tON_STK_SIZE %d\n", sizeof(struct on_stack));
	printf("#define\tREGS_USP %d\n", &regs->pcb_usp);
	printf("#define\tREGS_FP %d\n", &regs->pcb_fp);
	printf("#define\tREGS_SB %d\n", &regs->pcb_sb);
	printf("#define\tREGS_PSR %d\n", &regs->pcb_psr);

	printf("#define\tPCB_ONSTACK %d\n", &pcb->pcb_onstack);
	printf("#define\tPCB_FSR %d\n", &pcb->pcb_fsr);
	for (i=0; i<8; i++)
	  printf("#define\tPCB_F%d %d\n", i, &pcb->pcb_freg[i]);
	printf("#define\tPCB_KSP %d\n", &pcb->pcb_ksp);
	printf("#define\tPCB_KFP %d\n", &pcb->pcb_kfp);
	printf("#define\tPCB_PTB %d\n", &pcb->pcb_ptb);
	printf("#define\tPCB_PL %d\n", &pcb->pcb_pl);
	printf("#define\tPCB_FLAGS %d\n", &pcb->pcb_flags);
	printf("#define\tPCB_ONFAULT %d\n", &pcb->pcb_onfault);

	printf("#define\tV_TRAP %d\n", &vm->v_trap);
	printf("#define\tV_INTR %d\n", &vm->v_intr);

	printf("#define\tIV_VEC %d\n", &iv->iv_vec);
	printf("#define\tIV_ARG %d\n", &iv->iv_arg);
	printf("#define\tIV_CNT %d\n", &iv->iv_cnt);
	printf("#define\tIV_USE %d\n", &iv->iv_use);

	printf("#define\tUSRSTACK 0x%x\n", USRSTACK);
#ifdef SYSVSHM
	printf("#define\tSHMMAXPGS %d\n", SHMMAXPGS);
#endif
	printf("#define\tENOENT %d\n", ENOENT);
	printf("#define\tEFAULT %d\n", EFAULT);
	printf("#define\tENAMETOOLONG %d\n", ENAMETOOLONG);
	exit(0);
}
