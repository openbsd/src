/*	$NetBSD: genassym.c,v 1.3 1995/11/23 02:34:06 cgd Exp $	*/

/*
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)genassym.c	8.3 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/syscall.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <machine/rpb.h>
#include <machine/trap.h>

#include <stddef.h>
#include <stdio.h>
#include <err.h>

void	def __P((char *, long));

#define	off(what, s, m)	def(what, (int)offsetof(s, m))

void
def(what, val)
	char *what;
	long val;
{

	if (printf("#define\t%s\t%ld\n", what, val) < 0)
		err(1, "printf");
}

main()
{

	/* general constants */
	def("NBPG", NBPG);
	def("PGSHIFT", PGSHIFT);
	def("VM_MAX_ADDRESS", VM_MAX_ADDRESS);

	/* Register offsets, for stack frames. */
	def("FRAMESIZE", sizeof(struct trapframe));
	def("FRAME_NSAVEREGS", FRAME_NSAVEREGS);
	def("FRAME_V0", FRAME_V0);
	def("FRAME_T0", FRAME_T0);
	def("FRAME_T1", FRAME_T1);
	def("FRAME_T2", FRAME_T2);
	def("FRAME_T3", FRAME_T3);
	def("FRAME_T4", FRAME_T4);
	def("FRAME_T5", FRAME_T5);
	def("FRAME_T6", FRAME_T6);
	def("FRAME_T7", FRAME_T7);
	def("FRAME_S0", FRAME_S0);
	def("FRAME_S1", FRAME_S1);
	def("FRAME_S2", FRAME_S2);
	def("FRAME_S3", FRAME_S3);
	def("FRAME_S4", FRAME_S4);
	def("FRAME_S5", FRAME_S5);
	def("FRAME_S6", FRAME_S6);
	def("FRAME_A3", FRAME_A3);
	def("FRAME_A4", FRAME_A4);
	def("FRAME_A5", FRAME_A5);
	def("FRAME_T8", FRAME_T8);
	def("FRAME_T9", FRAME_T9);
	def("FRAME_T10", FRAME_T10);
	def("FRAME_T11", FRAME_T11);
	def("FRAME_RA", FRAME_RA);
	def("FRAME_T12", FRAME_T12);
	def("FRAME_AT", FRAME_AT);
	def("FRAME_SP", FRAME_SP);
	off("TF_PS", struct trapframe, tf_ps);
	off("TF_PC", struct trapframe, tf_ps);
	off("TF_A0", struct trapframe, tf_a0);
	off("TF_A1", struct trapframe, tf_a1);
	off("TF_A2", struct trapframe, tf_a2);

	/* bits of the PS register */
	def("PSL_U", PSL_U);
	def("PSL_IPL", PSL_IPL);
	def("PSL_IPL_0", PSL_IPL_0);
	def("PSL_IPL_SOFT", PSL_IPL_SOFT);
	def("PSL_IPL_HIGH", PSL_IPL_HIGH);

	/* pte bits */
	def("PG_V", PG_V);
	def("PG_ASM", PG_ASM);
	def("PG_KRE", PG_KRE);
	def("PG_KWE", PG_KWE);
	def("PG_SHIFT", PG_SHIFT);

	/* Important offsets into the proc struct & associated constants */
	off("P_FORW", struct proc, p_forw);
	off("P_BACK", struct proc, p_back);
	off("P_ADDR", struct proc, p_addr);
	off("P_VMSPACE", struct proc, p_vmspace);
	off("P_MD_FLAGS", struct proc, p_md.md_flags);
	off("P_MD_PCBPADDR", struct proc, p_md.md_pcbpaddr);
	off("PH_LINK", struct prochd, ph_link);
	off("PH_RLINK", struct prochd, ph_rlink);

	/* offsets needed by cpu_switch(), et al., to switch mappings. */
	off("VM_PMAP_STPTE", struct vmspace, vm_pmap.pm_stpte);
	def("USTP_OFFSET", kvtol1pte(VM_MIN_ADDRESS) * sizeof(pt_entry_t));

	/* Important offsets into the user struct & associated constants */
	def("UPAGES", UPAGES);
	off("U_PCB", struct user, u_pcb);
	off("U_PCB_KSP", struct user, u_pcb.pcb_ksp);
	off("U_PCB_CONTEXT", struct user, u_pcb.pcb_context[0]);
	off("U_PCB_ONFAULT", struct user, u_pcb.pcb_onfault);

	/* Offsets into struct fpstate, for save, restore */
	off("FPREG_FPR_REGS", struct fpreg, fpr_regs[0]);
	off("FPREG_FPR_CR", struct fpreg, fpr_cr);

	/* Important other addresses */
	def("HWRPB_ADDR", HWRPB_ADDR);		/* Restart parameter block */
	def("VPTBASE", VPTBASE);		/* Virtual Page Table base */

	/* Trap types and qualifiers */
	def("T_ASTFLT", T_ASTFLT);
	def("T_UNAFLT", T_UNAFLT);
	def("T_ARITHFLT", T_ARITHFLT);
	def("T_IFLT", T_IFLT);			/* qualifier */
	def("T_MMFLT", T_MMFLT);		/* qualifier */

	/* errno values */
	def("ENAMETOOLONG", ENAMETOOLONG);
	def("EFAULT", EFAULT);

	/* Syscalls called from sigreturn. */
	def("SYS_sigreturn", SYS_sigreturn);
	def("SYS_exit", SYS_exit);

	exit(0);
}
