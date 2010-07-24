/*	$OpenBSD: cpu.h,v 1.33 2010/07/24 12:18:58 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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

#ifndef _MVME68K_CPU_H_
#define _MVME68K_CPU_H_

/*
 * Exported definitions unique to mvme68k cpu support.
 */

/*
 * Get common m68k CPU definiti÷ns.
 */
#define M68K_MMU_MOTOROLA
#include <m68k/cpu.h>

#ifdef _KERNEL

/*
 * Get interrupt glue.
 */
#include <machine/intr.h>

/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe.  On the m68k, we use
 * what the hardware pushes on an interrupt (frame format 0).
 */
struct clockframe {
	u_short	sr;		/* sr at time of interrupt */
	u_long	pc;		/* pc at time of interrupt */
	u_short	vo;		/* vector offset (4-word frame) */
};

#define	CLKF_USERMODE(framep)	(((framep)->sr & PSL_S) == 0)
#define	CLKF_PC(framep)		((framep)->pc)
#if 0
/* We would like to do it this way... */
#define	CLKF_INTR(framep)	(((framep)->sr & PSL_M) == 0)
#else
/* but until we start using PSL_M, we have to do this instead */
#define	CLKF_INTR(framep)	(0)	/* XXX */
#endif


/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
extern int want_resched;
#define	need_resched(ci)	{ want_resched = 1; aston(); }
#define clear_resched(ci) 	want_resched = 0

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the m68k, request an ast to send us
 * through trap, marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	aston()

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)	aston()

extern int astpending;
#define aston() (astpending = 1)

#endif	/* _KERNEL */

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}

#ifdef _KERNEL

extern	vaddr_t intiobase, intiolimit;
extern	vaddr_t iiomapbase;
extern	int iiomapsize;

/* physical memory sections for mvme141 */
#define	INTIOBASE_141	(0xfff50000)
#define	INTIOTOP_141	(0xfffc0000)
#define	INTIOSIZE_141	((INTIOTOP_141-INTIOBASE_141)/NBPG)

/* physical memory sections for mvme147 */
#define	INTIOBASE_147	(0xfffe0000)
#define	INTIOTOP_147	(0xfffe5000)
#define	INTIOSIZE_147	((INTIOTOP_147-INTIOBASE_147)/NBPG)

/* physical memory sections for mvme16x */
#define	INTIOBASE_162	(0xfff00000)
#define	INTIOTOP_162	(0xfffd0000)		/* was 0xfff50000 */
#define	INTIOSIZE_162	((INTIOTOP_162-INTIOBASE_162)/NBPG)

/* physical memory sections for mvme165 */
#define	INTIOBASE_165	(0xfff90000)
#define	INTIOTOP_165	(0xffff0000)
#define	INTIOSIZE_165	((INTIOTOP_165-INTIOBASE_165)/NBPG)

/*
 * Internal IO space (iiomapsize).
 *
 * Internal IO space is mapped in the kernel from ``intiobase'' to
 * ``intiolimit'' (defined in locore.s).  Since it is always mapped,
 * conversion between physical and kernel virtual addresses is easy.
 */
#define	ISIIOVA(va) \
	((va) >= intiobase && (va) < intiolimit)
#define	IIOV(pa)	((pa) - iiomapbase + intiobase)
#define	IIOP(va)	((va) - intiobase + iiomapbase)
#define	IIOPOFF(pa)	((pa) - iiomapbase)

extern int	cputyp;
#define CPU_141			0x141
#define CPU_147			0x147
#define CPU_162			0x162
#define CPU_165			0x165
#define CPU_166			0x166
#define CPU_167			0x167
#define CPU_172			0x172
#define CPU_176			0x176
#define CPU_177			0x177

#include <sys/evcount.h>

struct intrhand {
	SLIST_ENTRY(intrhand) ih_link;
	int	(*ih_fn)(void *);
	void	*ih_arg;
	int	ih_ipl;
	int	ih_wantframe;
	struct evcount ih_count;
};

int intr_establish(int, struct intrhand *, const char *);

#define	NVMEINTR	256

/* locore.s */
__dead void	doboot(void);

int badpaddr(paddr_t, int);
int badvaddr(vaddr_t, int);
void nmihand(void *);
int intr_findvec(int, int);

paddr_t kvtop(vaddr_t);

#endif	/* _KERNEL */
#endif	/* _MVME68K_CPU_H_ */
