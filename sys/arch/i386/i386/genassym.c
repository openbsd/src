/*	$NetBSD: genassym.c,v 1.48 1996/03/28 23:44:04 mycroft Exp $	*/

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

#include <stdio.h>
#include <stddef.h>

int
main()
{

#define	def(N,V)	printf("#define\t%s %d\n", N, V)
#define	off(N,S,M)	def(N, (int)offsetof(S, M))

	def("SRUN", SRUN);

	def("PTDPTDI", PTDPTDI);
	def("KPTDI", KPTDI);
	def("NKPDE", NKPDE);
	def("APTDPTDI", APTDPTDI);

	def("VM_MAXUSER_ADDRESS", (int)VM_MAXUSER_ADDRESS);

	off("P_ADDR", struct proc, p_addr);
	off("P_BACK", struct proc, p_back);
	off("P_FORW", struct proc, p_forw);
	off("P_PRIORITY", struct proc, p_priority);
	off("P_STAT", struct proc, p_stat);
	off("P_WCHAN", struct proc, p_wchan);
	off("P_VMSPACE", struct proc, p_vmspace);
	off("P_FLAG", struct proc, p_flag);

	def("P_SYSTEM", P_SYSTEM);

	off("V_TRAP", struct vmmeter, v_trap);
	off("V_INTR", struct vmmeter, v_intr);

	off("PCB_CR3", struct pcb, pcb_cr3);
	off("PCB_EBP", struct pcb, pcb_ebp);
	off("PCB_ESP", struct pcb, pcb_esp);
	off("PCB_FS", struct pcb, pcb_fs);
	off("PCB_GS", struct pcb, pcb_gs);
	off("PCB_CR0", struct pcb, pcb_cr0);
	off("PCB_LDT_SEL", struct pcb, pcb_ldt_sel);
	off("PCB_TSS_SEL", struct pcb, pcb_tss_sel);
	off("PCB_ONFAULT", struct pcb, pcb_onfault);

	off("TF_CS", struct trapframe, tf_cs);
	off("TF_TRAPNO", struct trapframe, tf_trapno);
	off("TF_EFLAGS", struct trapframe, tf_eflags);

	def("FRAMESIZE", sizeof(struct trapframe));

	off("SIGF_HANDLER", struct sigframe, sf_handler);
	off("SIGF_SC", struct sigframe, sf_sc);
	off("SC_FS", struct sigcontext, sc_fs);
	off("SC_GS", struct sigcontext, sc_gs);
	off("SC_EFLAGS", struct sigcontext, sc_eflags);

#ifdef COMPAT_SVR4
	off("SVR4_SIGF_HANDLER", struct svr4_sigframe, sf_handler);
	off("SVR4_SIGF_UC", struct svr4_sigframe, sf_uc);
	off("SVR4_UC_FS", struct svr4_ucontext, uc_mcontext.greg[SVR4_X86_FS]);
	off("SVR4_UC_GS", struct svr4_ucontext, uc_mcontext.greg[SVR4_X86_GS]);
	off("SVR4_UC_EFLAGS", struct svr4_ucontext, uc_mcontext.greg[SVR4_X86_EFL]);
#endif

#ifdef COMPAT_LINUX
	off("LINUX_SIGF_HANDLER", struct linux_sigframe, sf_handler);
	off("LINUX_SIGF_SC", struct linux_sigframe, sf_sc);
	off("LINUX_SC_FS", struct linux_sigcontext, sc_fs);
	off("LINUX_SC_GS", struct linux_sigcontext, sc_gs);
	off("LINUX_SC_EFLAGS", struct linux_sigcontext, sc_eflags);
#endif

#ifdef COMPAT_FREEBSD
	off("FREEBSD_SIGF_HANDLER", struct freebsd_sigframe, sf_handler);
	off("FREEBSD_SIGF_SC", struct freebsd_sigframe, sf_sc);
#endif

#if NISA > 0
	off("IH_FUN", struct intrhand, ih_fun);
	off("IH_ARG", struct intrhand, ih_arg);
	off("IH_COUNT", struct intrhand, ih_count);
	off("IH_NEXT", struct intrhand, ih_next);
#endif

	exit(0);
}
