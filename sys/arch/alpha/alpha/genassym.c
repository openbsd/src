/*	$OpenBSD: genassym.c,v 1.6 1999/09/26 11:07:32 kstailey Exp $	*/
/*	$NetBSD: genassym.c,v 1.9 1996/08/20 23:00:24 cgd Exp $	*/

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

#include <stddef.h>
#include <stdio.h>
#include <err.h>

#ifdef COMPAT_NETBSD
# include <compat/netbsd/netbsd_syscall.h>
#endif

void	def __P((char *, long));
int	main __P((int argc, char **argv));

#define	off(what, s, m)	def(what, (int)offsetof(s, m))

void
def(what, val)
	char *what;
	long val;
{

	if (printf("#define\t%s\t%ld\n", what, val) < 0)
		err(1, "printf");
}

int
main(argc, argv)
	int argc;
	char **argv;
{

	/* general constants */
	def("NBPG", NBPG);
	def("PGSHIFT", PGSHIFT);
	def("VM_MAX_ADDRESS", VM_MAX_ADDRESS);

	/* Register offsets, for stack frames. */
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

	def("FRAME_SW_SIZE", FRAME_SW_SIZE);

	def("FRAME_PS", FRAME_PS);
	def("FRAME_PC", FRAME_PC);
	def("FRAME_GP", FRAME_GP);
	def("FRAME_A0", FRAME_A0);
	def("FRAME_A1", FRAME_A1);
	def("FRAME_A2", FRAME_A2);

	def("FRAME_SIZE", FRAME_SIZE);

	/* bits of the PS register */
	def("ALPHA_PSL_USERMODE", ALPHA_PSL_USERMODE);
	def("ALPHA_PSL_IPL_MASK", ALPHA_PSL_IPL_MASK);
	def("ALPHA_PSL_IPL_0", ALPHA_PSL_IPL_0);
	def("ALPHA_PSL_IPL_SOFT", ALPHA_PSL_IPL_SOFT);
	def("ALPHA_PSL_IPL_HIGH", ALPHA_PSL_IPL_HIGH);

	/* pte bits */
	def("ALPHA_PTE_VALID", ALPHA_PTE_VALID);
	def("ALPHA_PTE_ASM", ALPHA_PTE_ASM);
	def("ALPHA_PTE_KR", ALPHA_PTE_KR);
	def("ALPHA_PTE_KW", ALPHA_PTE_KW);

	/* Important offsets into the proc struct & associated constants */
	off("P_FORW", struct proc, p_forw);
	off("P_BACK", struct proc, p_back);
	off("P_ADDR", struct proc, p_addr);
	off("P_VMSPACE", struct proc, p_vmspace);
	off("P_MD_FLAGS", struct proc, p_md.md_flags);
	off("P_MD_PCBPADDR", struct proc, p_md.md_pcbpaddr);
	off("PH_LINK", struct prochd, ph_link);
	off("PH_RLINK", struct prochd, ph_rlink);

#ifndef NEW_PMAP
	/* offsets needed by cpu_switch(), et al., to switch mappings. */
	off("VM_PMAP_STPTE", struct vmspace, vm_pmap.pm_stpte);
	def("USTP_OFFSET", kvtol1pte(VM_MIN_ADDRESS) * sizeof(pt_entry_t));
#else /* NEW_PMAP */
	off("VM_PMAP", struct vmspace, vm_pmap);
#endif /* NEW_PMAP */

	/* Important offsets into the user struct & associated constants */
	def("UPAGES", UPAGES);
	off("U_PCB", struct user, u_pcb);
	off("PCB_HWPCB", struct pcb, pcb_hw);
	off("PCB_HWPCB_KSP", struct pcb, pcb_hw.apcb_ksp);
	off("PCB_CONTEXT", struct pcb, pcb_context[0]);
	off("PCB_ONFAULT", struct pcb, pcb_onfault);
	off("PCB_ACCESSADDR", struct pcb, pcb_accessaddr);

	/* Offsets into struct fpstate, for save, restore */
	off("FPREG_FPR_REGS", struct fpreg, fpr_regs[0]);
	off("FPREG_FPR_CR", struct fpreg, fpr_cr);

	/* Important other addresses */
	def("HWRPB_ADDR", HWRPB_ADDR);		/* Restart parameter block */
	def("VPTBASE", VPTBASE);		/* Virtual Page Table base */

	/* Kernel entries */
	def("ALPHA_KENTRY_ARITH", ALPHA_KENTRY_ARITH);
	def("ALPHA_KENTRY_MM", ALPHA_KENTRY_MM);
	def("ALPHA_KENTRY_IF", ALPHA_KENTRY_IF);
	def("ALPHA_KENTRY_UNA", ALPHA_KENTRY_UNA);

	/* errno values */
	def("ENAMETOOLONG", ENAMETOOLONG);
	def("EFAULT", EFAULT);

	/* Syscalls called from sigreturn. */
	def("SYS_sigreturn", SYS_sigreturn);
	def("SYS_exit", SYS_exit);
#ifdef COMPAT_NETBSD
	def("NETBSD_SYS___sigreturn14", NETBSD_SYS___sigreturn14);
	def("NETBSD_SYS_exit", NETBSD_SYS_exit);
#endif

	exit(0);
}
