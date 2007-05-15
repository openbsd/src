/*	$OpenBSD: cpu.h,v 1.16 2007/05/15 13:46:22 martin Exp $	*/
/*	$NetBSD: cpu.h,v 1.3 1997/02/02 06:56:57 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 * from: Utah $Hdr: cpu.h 1.16 91/03/25$
 *
 *	@(#)cpu.h	8.4 (Berkeley) 1/5/94
 */

#ifndef _M68K_CPU_H_
#define	_M68K_CPU_H_

/*
 * Exported definitions common to Motorola m68k-based ports.
 *
 * Note that are some port-specific definitions here, such as
 * HP and Sun MMU types.  These facilitate adding very small
 * amounts of port-specific code to what would otherwise be
 * identical.  The is especially true in the case of the HP
 * and other m68k pmaps.
 *
 * Individual ports are expected to define the following CPP symbols
 * in <machine/cpu.h> to enable conditional code:
 *
 *	M68K_MMU_MOTOROLA	Machine may have a Motorola MMU (incl.
 *				68851, 68030, 68040, 68060)
 *
 *	M68K_MMU_HP		Machine may have an HP MMU.
 *
 * Note also that while m68k-generic code conditionalizes on the
 * M68K_MMU_HP CPP symbol, none of the HP MMU definitions are in this
 * file (since none are used in otherwise sharable code).
 */

#ifdef _KERNEL
#ifndef _LOCORE
#include <sys/sched.h>

struct cpu_info {
	struct proc *ci_curproc;

	struct schedstate_percpu ci_schedstate;
};

extern struct cpu_info cpu_info_store;

#define	curcpu()	(&cpu_info_store)

#define CPU_IS_PRIMARY(ci)	1
#define	CPU_INFO_ITERATOR	int
#define	CPU_INFO_FOREACH(cii, ci) \
	for (cii = 0, ci = curcpu(); ci != NULL; ci = NULL)

#define cpu_number()	0

#endif /* !_LOCORE */

/*
 * All m68k ports must provide these globals.
 */
extern	int cputype;		/* CPU on this host */
extern	int ectype;		/* external cache on this host */
extern	int fputype;		/* FPU on this host */
extern	int mmutype;		/* MMU on this host */
#endif

/* values for cputype */
#define	CPU_68020	0	/* 68020 */
#define	CPU_68030	1	/* 68030 */
#define	CPU_68040	2	/* 68040 */
#define	CPU_68060	3	/* 68060 */

/* values for ectype */
#define	EC_PHYS		-1	/* external physical address cache */
#define	EC_NONE		0	/* no external cache */
#define	EC_VIRT		1	/* external virtual address cache */

/* values for fputype */
#define	FPU_NONE	0	/* no FPU */
#define	FPU_68881	1	/* 68881 FPU */
#define	FPU_68882	2	/* 68882 FPU */
#define	FPU_68040	3	/* 68040 on-chip FPU */
#define	FPU_68060	4	/* 68060 on-chip FPU */
#define	FPU_UNKNOWN	5	/* placeholder; unknown FPU */

/* values for mmutype (assigned for quick testing) */
#define	MMU_68060	-3	/* 68060 on-chip MMU */
#define	MMU_68040	-2	/* 68040 on-chip MMU */
#define	MMU_68030	-1	/* 68030 on-chip subset of 68851 */
#define	MMU_HP		0	/* HP proprietary */
#define	MMU_68851	1	/* Motorola 68851 */
#define	MMU_SUN		2	/* Sun MMU */

/*
 * 68851 and 68030 MMU
 */
#define	PMMU_LVLMASK	0x0007
#define	PMMU_INV	0x0400
#define	PMMU_WP		0x0800
#define	PMMU_ALV	0x1000
#define	PMMU_SO		0x2000
#define	PMMU_LV		0x4000
#define	PMMU_BE		0x8000
#define	PMMU_FAULT	(PMMU_WP|PMMU_INV)

/*
 * 68040 MMU
 */
#define	MMU40_RES	0x001
#define	MMU40_TTR	0x002
#define	MMU40_WP	0x004
#define	MMU40_MOD	0x010
#define	MMU40_CMMASK	0x060
#define	MMU40_SUP	0x080
#define	MMU40_U0	0x100
#define	MMU40_U1	0x200
#define	MMU40_GLB	0x400
#define	MMU40_BE	0x800

/* 680X0 function codes */
#define	FC_USERD	1	/* user data space */
#define	FC_USERP	2	/* user program space */
#define	FC_PURGE	3	/* HPMMU: clear TLB entries */
#define	FC_SUPERD	5	/* supervisor data space */
#define	FC_SUPERP	6	/* supervisor program space */
#define	FC_CPU		7	/* CPU space */

/* fields in the 68020 cache control register */
#define	IC_ENABLE	0x0001	/* enable instruction cache */
#define	IC_FREEZE	0x0002	/* freeze instruction cache */
#define	IC_CE		0x0004	/* clear instruction cache entry */
#define	IC_CLR		0x0008	/* clear entire instruction cache */

/* additional fields in the 68030 cache control register */
#define	IC_BE		0x0010	/* instruction burst enable */
#define	DC_ENABLE	0x0100	/* data cache enable */
#define	DC_FREEZE	0x0200	/* data cache freeze */
#define	DC_CE		0x0400	/* clear data cache entry */
#define	DC_CLR		0x0800	/* clear entire data cache */
#define	DC_BE		0x1000	/* data burst enable */
#define	DC_WA		0x2000	/* write allocate */

/* fields in the 68040 cache control register */
#define	IC40_ENABLE	0x00008000	/* instruction cache enable bit */
#define	DC40_ENABLE	0x80000000	/* data cache enable bit */

/* additional fields in the 68060 cache control register */
#define	DC60_NAD	0x40000000	/* no allocate mode, data cache */
#define	DC60_ESB	0x20000000	/* enable store buffer */
#define	DC60_DPI	0x10000000	/* disable CPUSH invalidation */
#define	DC60_FOC	0x08000000	/* four kB data cache mode (else 8) */

#define	IC60_EBC	0x00800000	/* enable branch cache */
#define IC60_CABC	0x00400000	/* clear all branch cache entries */
#define	IC60_CUBC	0x00200000	/* clear user branch cache entries */

#define	IC60_NAI	0x00004000	/* no allocate mode, instr. cache */
#define	IC60_FIC	0x00002000	/* four kB instr. cache (else 8) */

#define	CACHE_ON	(DC_WA|DC_BE|DC_CLR|DC_ENABLE|IC_BE|IC_CLR|IC_ENABLE)
#define	CACHE_OFF	(DC_CLR|IC_CLR)
#define	CACHE_CLR	(CACHE_ON)
#define	IC_CLEAR	(DC_WA|DC_BE|DC_ENABLE|IC_BE|IC_CLR|IC_ENABLE)
#define	DC_CLEAR	(DC_WA|DC_BE|DC_CLR|DC_ENABLE|IC_BE|IC_ENABLE)

#define	CACHE40_ON	(IC40_ENABLE|DC40_ENABLE)
#define	CACHE40_OFF	(0x00000000)

#define	CACHE60_ON	(CACHE40_ON|IC60_CABC|IC60_EBC|DC60_ESB)
#define	CACHE60_OFF	(CACHE40_OFF|IC60_CABC)

/* bits in the 68060 Processor Control Register */
#define	PCR_IDSHIFT	16
#define	PCR_IDMASK	0xffff
#define	PCR_68060		0x430
#define	PCR_68060LC		0x431
#define	PCR_REVSHIFT	8
#define	PCR_REVMASK	0xff
#define	PCR_DEBUG	0x80
#define	PCR_FPUDIS	0x02
#define	PCR_SUPERSCALAR	0x01

#ifdef _KERNEL
struct frame;
struct fpframe;
struct pcb;
struct proc;
struct trapframe;

void	copypage(void *fromaddr, void *toaddr);
void	zeropage(void *addr);

/* locore.s */
void	TBIS(vaddr_t);
void	TBIAS(void);
void	TBIAU(void);
void	ICIA(void);
void	DCIA(void);
void	DCIS(void);
void	DCIU(void);
#if defined(M68040) || defined(M68060)
void	ICPA(void);
void	DCFA(void);
void	ICPL(paddr_t);
void	ICPP(paddr_t);
void	DCPL(paddr_t);
void	DCPP(paddr_t);
void	DCFL(paddr_t);
void	DCFP(paddr_t);
#endif

u_long	getdfc(void);
u_long	getsfc(void);
void	loadustp(int);
void	m68881_restore(struct fpframe *);
void	m68881_save(struct fpframe *);
void	proc_trampoline(void);
void	savectx(struct pcb *);
int	suline(caddr_t, caddr_t);
void	switch_exit(struct proc *);

/* m68k_machdep.c */
void userret(struct proc *);

/* regdump.c */
void regdump(struct trapframe *, int);

/* sys_machdep.c */
int	cachectl(struct proc *, int, vaddr_t, int);
#define	CC_PURGE	0x00000001
#define	CC_FLUSH	0x00000002
#define	CC_IPURGE	0x00000004
#define	CC_EXTPURGE	0x80000000

/*
 * This is used during profiling to integrate system time.
 */
#define	PROC_PC(p)	(((struct trapframe *)((p)->p_md.md_regs))->tf_pc)

#endif /* _KERNEL */

#endif /* _M68K_CPU_H_ */
