/* $OpenBSD: genassym.c,v 1.9 2001/01/15 11:58:54 art Exp $ */
/* $NetBSD: genassym.c,v 1.27 2000/05/26 00:36:42 thorpej Exp $ */

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 *	from: @(#)genassym.c	8.3 (Berkeley) 1/4/94
 */

/*
 * This program is designed so that it can be both:
 * (1) Run on the native machine to generate assym.h
 * (2) Converted to assembly that genassym.awk will
 *     translate into the same assym.h as (1) does.
 * The second method is done as follows:
 *   m68k-xxx-gcc [options] -S .../genassym.c
 *   awk -f genassym.awk < genassym.s > assym.h
 *
 * Using actual C code here (instead of genassym.cf)
 * has the advantage that "make depend" automatically
 * tracks dependencies of this C code on the (many)
 * header files used here.  Also, the awk script used
 * to convert the assembly output to assym.h is much
 * smaller and simpler than sys/kern/genassym.sh.
 *
 * Both this method and the genassym.cf method have the
 * disadvantage that they depend on gcc-specific features.
 * This method depends on the format of assembly output for
 * data, and the genassym.cf method depends on features of
 * the gcc asm() statement (inline assembly).
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/syscall.h>
#include <sys/user.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <machine/rpb.h>
#include <machine/vmparam.h>

#ifdef COMPAT_NETBSD
#include <compat/netbsd/netbsd_syscall.h>
#endif

#include <vm/vm.h>

/* Note: Avoid /usr/include for cross compilation! */
extern void printf __P((const char *fmt, ...));
extern void exit __P((int));

#define	offsetof(type, member) ((size_t)(&((type *)0)->member))

#ifdef	__STDC__
#define def(name, value) 	{ #name, value }
#define	def1(name)	 	{ #name, name }
#define	off(name, type, member)	{ #name, offsetof(type, member) }
#else
#define def(name, value) 	{ "name", value }
#define	def1(name)	 	{ "name", name }
#define	off(name, type, member)	{ "name", offsetof(type, member) }
#endif

/*
 * Note: genassym.awk cares about the form of this structure,
 * as well as the names and placement of the "asdefs" array
 * and the "nassefs" variable below.  Clever, but fragile.
 */
struct nv {
	char n[28];
	long v;
};

struct nv assyms[] = {
	/* general constants */
	def1(NBPG),
	def1(PGSHIFT),
	def1(VM_MAX_ADDRESS),

	/* Register offsets, for stack frames. */
	def1(FRAME_V0),
	def1(FRAME_T0),
	def1(FRAME_T1),
	def1(FRAME_T2),
	def1(FRAME_T3),
	def1(FRAME_T4),
	def1(FRAME_T5),
	def1(FRAME_T6),
	def1(FRAME_T7),
	def1(FRAME_S0),
	def1(FRAME_S1),
	def1(FRAME_S2),
	def1(FRAME_S3),
	def1(FRAME_S4),
	def1(FRAME_S5),
	def1(FRAME_S6),
	def1(FRAME_A3),
	def1(FRAME_A4),
	def1(FRAME_A5),
	def1(FRAME_T8),
	def1(FRAME_T9),
	def1(FRAME_T10),
	def1(FRAME_T11),
	def1(FRAME_RA),
	def1(FRAME_T12),
	def1(FRAME_AT),
	def1(FRAME_SP),

	def1(FRAME_SW_SIZE),

	def1(FRAME_PS),
	def1(FRAME_PC),
	def1(FRAME_GP),
	def1(FRAME_A0),
	def1(FRAME_A1),
	def1(FRAME_A2),

	def1(FRAME_SIZE),

	/* bits of the PS register */
	def1(ALPHA_PSL_USERMODE),
	def1(ALPHA_PSL_IPL_MASK),
	def1(ALPHA_PSL_IPL_0),
	def1(ALPHA_PSL_IPL_SOFT),
	def1(ALPHA_PSL_IPL_HIGH),

	/* pte bits */
	def1(ALPHA_PTE_VALID),
	def1(ALPHA_PTE_ASM),
	def1(ALPHA_PTE_KR),
	def1(ALPHA_PTE_KW),

	/* Important offsets into the proc struct & associated constants */
	off(P_FORW, struct proc, p_forw),
	off(P_BACK, struct proc, p_back),
	off(P_ADDR, struct proc, p_addr),
	off(P_VMSPACE, struct proc, p_vmspace),
	off(P_STAT, struct proc, p_stat),
	off(P_MD_FLAGS, struct proc, p_md.md_flags),
	off(P_MD_PCBPADDR, struct proc, p_md.md_pcbpaddr),
	off(PH_LINK, struct prochd, ph_link),
	off(PH_RLINK, struct prochd, ph_rlink),

	/* XXXXX - Extremly bogus! */
	def(SONPROC, SRUN),
	/* XXX */

	/* offsets needed by cpu_switch() to switch mappings. */
	off(VM_MAP_PMAP, struct vmspace, vm_map.pmap), 

	/* Important offsets into the user struct & associated constants */
	def1(UPAGES),
	off(U_PCB, struct user, u_pcb),
	off(U_PCB_HWPCB, struct user, u_pcb.pcb_hw),
	off(U_PCB_HWPCB_KSP, struct user, u_pcb.pcb_hw.apcb_ksp),
	off(U_PCB_CONTEXT, struct user, u_pcb.pcb_context[0]),
	off(U_PCB_ONFAULT, struct user, u_pcb.pcb_onfault),
	off(U_PCB_ACCESSADDR, struct user, u_pcb.pcb_accessaddr),

	/* Offsets into struct fpstate, for save, restore */
	off(FPREG_FPR_REGS, struct fpreg, fpr_regs[0]),
	off(FPREG_FPR_CR, struct fpreg, fpr_cr),

	/* Important other addresses */
	def1(HWRPB_ADDR),		/* Restart parameter block */
	def1(VPTBASE),			/* Virtual Page Table base */

	/* Offsets into the HWRPB. */
	off(RPB_PRIMARY_CPU_ID, struct rpb, rpb_primary_cpu_id),

	/* Kernel entries */
	def1(ALPHA_KENTRY_ARITH),
	def1(ALPHA_KENTRY_MM),
	def1(ALPHA_KENTRY_IF),
	def1(ALPHA_KENTRY_UNA),

	/* errno values */
	def1(ENAMETOOLONG),
	def1(EFAULT),

	/* Syscalls called from sigreturn. */
	def1(SYS_sigreturn),
	def1(SYS_exit),

#ifdef COMPAT_NETBSD
	/* XXX - these should probably use the magic macro from machine/asm.h */
	def1(NETBSD_SYS___sigreturn14),
	def1(NETBSD_SYS_exit),
#endif

	/* CPU info */
	off(CPU_INFO_CURPROC, struct cpu_info, ci_curproc),
	off(CPU_INFO_FPCURPROC, struct cpu_info, ci_fpcurproc),
	off(CPU_INFO_CURPCB, struct cpu_info, ci_curpcb),
	off(CPU_INFO_IDLE_PCB_PADDR, struct cpu_info, ci_idle_pcb_paddr),
	off(CPU_INFO_WANT_RESCHED, struct cpu_info, ci_want_resched),
	off(CPU_INFO_ASTPENDING, struct cpu_info, ci_astpending),
	def(CPU_INFO_SIZEOF, sizeof(struct cpu_info)),
};
int nassyms = sizeof(assyms)/sizeof(assyms[0]);

int main __P((int argc, char **argv));

int
main(argc, argv)
	int argc;
	char **argv;
{
	char *name;
	long i, val;

	for (i = 0; i < nassyms; i++) {
		name = assyms[i].n;
		val  = assyms[i].v;

		printf("#define\t%s\t", name);
		/* Hack to make the output easier to verify. */
		if ((val < 0) || (val > 999))
			printf("0x%lx\n", val);
		else
			printf("%ld\n", val);
	}

	exit(0);
}
