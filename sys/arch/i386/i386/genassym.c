/*	$NetBSD: genassym.c,v 1.46 1996/02/02 19:42:43 mycroft Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
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
#include <sys/user.h>

#include <vm/vm.h>

#include <machine/trap.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>

#ifdef COMPAT_SVR4
#include <compat/svr4/svr4_ucontext.h>
#endif

#ifdef COMPAT_LINUX
#include <machine/linux_machdep.h>
#endif

#ifdef COMPAT_FREEBSD
#include <machine/freebsd_machdep.h>
#endif

#include "isa.h"
#if NISA > 0
#include <i386/isa/isa_machdep.h>
#endif

main()
{
	struct proc *p = 0;
	struct vmmeter *vm = 0;
	struct pcb *pcb = 0;
	struct trapframe *tf = 0;
	struct sigframe *sigf = 0;
	struct sigcontext *sc = 0;
	struct uprof *uprof = 0;
#if NISA > 0
	struct intrhand *ih = 0;
#endif
#ifdef COMPAT_SVR4
	struct svr4_sigframe *svr4_sigf = 0;
	struct svr4_ucontext *svr4_uc = 0;
#endif
#ifdef COMPAT_LINUX
	struct linux_sigframe *linux_sigf = 0;
	struct linux_sigcontext *linux_sc = 0;
#endif
#ifdef COMPAT_FREEBSD
	struct freebsd_sigframe *freebsd_sigf = 0;
	struct freebsd_sigcontext *freebsd_sc = 0;
#endif

#define	def(N,V)	printf("#define\t%s %d\n", N, V)

	def("SRUN", SRUN);

	def("PTDPTDI", PTDPTDI);
	def("KPTDI", KPTDI);
	def("NKPDE", NKPDE);
	def("APTDPTDI", APTDPTDI);

	def("VM_MAXUSER_ADDRESS", VM_MAXUSER_ADDRESS);

	def("P_ADDR", &p->p_addr);
	def("P_BACK", &p->p_back);
	def("P_FORW", &p->p_forw);
	def("P_PRIORITY", &p->p_priority);
	def("P_STAT", &p->p_stat);
	def("P_WCHAN", &p->p_wchan);
	def("P_VMSPACE", &p->p_vmspace);
	def("P_FLAG", &p->p_flag);

	def("P_SYSTEM", P_SYSTEM);

	def("V_TRAP", &vm->v_trap);
	def("V_INTR", &vm->v_intr);

	def("PCB_CR3", &pcb->pcb_cr3);
	def("PCB_EBP", &pcb->pcb_ebp);
	def("PCB_ESP", &pcb->pcb_esp);
	def("PCB_FS", &pcb->pcb_fs);
	def("PCB_GS", &pcb->pcb_gs);
	def("PCB_CR0", &pcb->pcb_cr0);
	def("PCB_LDT_SEL", &pcb->pcb_ldt_sel);
	def("PCB_TSS_SEL", &pcb->pcb_tss_sel);
	def("PCB_ONFAULT", &pcb->pcb_onfault);

	def("TF_CS", &tf->tf_cs);
	def("TF_TRAPNO", &tf->tf_trapno);
	def("TF_EFLAGS", &tf->tf_eflags);

	def("FRAMESIZE", sizeof(struct trapframe));

	def("SIGF_HANDLER", &sigf->sf_handler);
	def("SIGF_SC", &sigf->sf_sc);
	def("SC_FS", &sc->sc_fs);
	def("SC_GS", &sc->sc_gs);
	def("SC_EFLAGS", &sc->sc_eflags);

#ifdef COMPAT_SVR4
	def("SVR4_SIGF_HANDLER", &svr4_sigf->sf_handler);
	def("SVR4_SIGF_UC", &svr4_sigf->sf_uc);
	def("SVR4_UC_FS", &svr4_uc->uc_mcontext.greg[SVR4_X86_FS]);
	def("SVR4_UC_GS", &svr4_uc->uc_mcontext.greg[SVR4_X86_GS]);
	def("SVR4_UC_EFLAGS", &svr4_uc->uc_mcontext.greg[SVR4_X86_EFL]);
#endif

#ifdef COMPAT_LINUX
	def("LINUX_SIGF_HANDLER", &linux_sigf->sf_handler);
	def("LINUX_SIGF_SC", &linux_sigf->sf_sc);
	def("LINUX_SC_FS", &linux_sc->sc_fs);
	def("LINUX_SC_GS", &linux_sc->sc_gs);
	def("LINUX_SC_EFLAGS", &linux_sc->sc_eflags);
#endif

#ifdef COMPAT_FREEBSD
	def("FREEBSD_SIGF_HANDLER", &freebsd_sigf->sf_handler);
	def("FREEBSD_SIGF_SC", &freebsd_sigf->sf_sc);
#endif

#if NISA > 0
	def("IH_FUN", &ih->ih_fun);
	def("IH_ARG", &ih->ih_arg);
	def("IH_COUNT", &ih->ih_count);
	def("IH_NEXT", &ih->ih_next);
#endif

	exit(0);
}
