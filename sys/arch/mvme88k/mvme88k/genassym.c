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
 *	$Id: genassym.c,v 1.1 1995/10/18 12:32:22 deraadt Exp $
 */

#ifndef KERNEL
#define KERNEL
#endif /* KERNEL */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <machine/cpu.h>
#include <machine/trap.h>
#include <machine/psl.h>
#include <machine/vmparam.h>
#include <sys/syscall.h>
#include <vm/vm.h>
#include <sys/user.h>

#define pair(TOKEN, ELEMENT) \
    printf("#define " TOKEN " %u\n", (unsigned)(ELEMENT))

#define int_offset_of_element(ELEMENT) (((unsigned)&(ELEMENT))/sizeof(int))

main()
{
	register struct proc *p = (struct proc *)0;
	struct m88100_saved_state *ss = (struct m88100_saved_state *) 0;
	register struct vmmeter *vm = (struct vmmeter *)0;
	register struct user *up = (struct user *)0;
	register struct rusage *rup = (struct rusage *)0;
	struct vmspace *vms = (struct vmspace *)0;
	pmap_t pmap = (pmap_t)0;
	struct pcb *pcb = (struct pcb *)0;
	register unsigned i;

	printf("#ifndef __GENASSYM_INCLUDED\n");
	printf("#define __GENASSYM_INCLUDED 1\n\n");

	printf("#ifdef ASSEMBLER\n"
		"#define NEWLINE \\\\ \n"
	"#endif\n");	

	printf("#define\tP_FORW %d\n", &p->p_forw);
	printf("#define\tP_BACK %d\n", &p->p_back);
	printf("#define\tP_VMSPACE %d\n", &p->p_vmspace);
	printf("#define\tP_ADDR %d\n", &p->p_addr);
	printf("#define\tP_PRIORITY %d\n", &p->p_priority);
	printf("#define\tP_STAT %d\n", &p->p_stat);
	printf("#define\tP_WCHAN %d\n", &p->p_wchan);
	printf("#define\tSRUN %d\n", SRUN);
	
	printf("#define\tVM_PMAP %d\n", &vms->vm_pmap);
	printf("#define\tV_INTR %d\n", &vm->v_intr);
	
	printf("#define\tUPAGES %d\n", UPAGES);
	printf("#define\tPGSHIFT %d\n", PGSHIFT);

	printf("#define\tU_PROF %d\n", &up->u_stats.p_prof);
	printf("#define\tU_PROFSCALE %d\n", &up->u_stats.p_prof.pr_scale);
	printf("#define\tPCB_ONFAULT %d\n", &pcb->pcb_onfault);
	printf("#define\tSIZEOF_PCB %d\n", sizeof(struct pcb));

	printf("#define\tSYS_exit %d\n", SYS_exit);
	printf("#define\tSYS_execve %d\n", SYS_execve);
	printf("#define\tSYS_sigreturn %d\n", SYS_sigreturn);
	
	pair("EF_R0",	int_offset_of_element(ss->r[0]));
	pair("EF_R31",	int_offset_of_element(ss->r[31]));
	pair("EF_FPSR",	int_offset_of_element(ss->fpsr));
	pair("EF_FPCR",	int_offset_of_element(ss->fpcr));
	pair("EF_EPSR",	int_offset_of_element(ss->epsr));
	pair("EF_SXIP",	int_offset_of_element(ss->sxip));
	pair("EF_SFIP",	int_offset_of_element(ss->sfip));
	pair("EF_SNIP",	int_offset_of_element(ss->snip));
	pair("EF_SSBR",	int_offset_of_element(ss->ssbr));
	pair("EF_DMT0",	int_offset_of_element(ss->dmt0));
	pair("EF_DMD0",	int_offset_of_element(ss->dmd0));
	pair("EF_DMA0",	int_offset_of_element(ss->dma0));
	pair("EF_DMT1",	int_offset_of_element(ss->dmt1));
	pair("EF_DMD1",	int_offset_of_element(ss->dmd1));
	pair("EF_DMA1",	int_offset_of_element(ss->dma1));
	pair("EF_DMT2",	int_offset_of_element(ss->dmt2));
	pair("EF_DMD2",	int_offset_of_element(ss->dmd2));
	pair("EF_DMA2",	int_offset_of_element(ss->dma2));
	pair("EF_FPECR",	int_offset_of_element(ss->fpecr));
	pair("EF_FPHS1",	int_offset_of_element(ss->fphs1));
	pair("EF_FPLS1",	int_offset_of_element(ss->fpls1));
	pair("EF_FPHS2",	int_offset_of_element(ss->fphs2));
	pair("EF_FPLS2",	int_offset_of_element(ss->fpls2));
	pair("EF_FPPT",	int_offset_of_element(ss->fppt));
	pair("EF_FPRH",	int_offset_of_element(ss->fprh));
	pair("EF_FPRL",	int_offset_of_element(ss->fprl));
	pair("EF_FPIT",	int_offset_of_element(ss->fpit));
	pair("EF_VECTOR",	int_offset_of_element(ss->vector));
	pair("EF_MASK",	int_offset_of_element(ss->mask));
	pair("EF_MODE",	int_offset_of_element(ss->mode));

	pair("EF_RET",	int_offset_of_element(ss->scratch1));
	pair("EF_NREGS",	sizeof(*ss)/sizeof(int));

	/* make a sanity check */
	if (sizeof(*ss) & 7)
	{
		/* 
		 * This contortion using write instead of fputs(stderr)
		 * is necessary because we can't include stdio.h in here.
		 */
		static char buf[] = 
	  "Exception frame not a multiple of double words\n";
		write(2 /* stderr */,buf,sizeof(buf));
		exit(1);
	}
	pair("SIZEOF_EF", sizeof(*ss));
	printf("\n#endif /* __GENASSYM_INCLUDED */\n");
	exit(0);
}
