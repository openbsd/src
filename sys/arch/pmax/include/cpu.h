/*	$OpenBSD: cpu.h,v 1.7 1998/05/18 00:28:11 millert Exp $	*/
/*	$NetBSD: cpu.h,v 1.15 1996/03/23 20:28:19 jonathan Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)cpu.h	8.4 (Berkeley) 1/4/94
 */

#ifndef _CPU_H_
#define _CPU_H_

#include <machine/machConst.h>

/*
 * Exported definitions unique to OpenBSD/mips cpu support.
 */

/*
 * Macros to find the CPU architecture we're on at run-time,
 * or if possible, at compile-time.
 */

#if (MIPS1 + MIPS3) == 1
#ifdef MIPS1
# define CPUISMIPS3	0
#endif /* mips1 */

#ifdef MIPS3
#  define CPUISMIPS3	 1
#endif /* mips1 */

#else /* run-time test */
extern int cpu_arch;
#define CPUISMIPS3	(cpu_arch == 3)
#endif /* run-time test */

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_wait(p)			/* nothing */
#define cpu_set_init_frame(p, fp)	/* nothing */
#define	cpu_swapout(p)			panic("cpu_swapout: can't get here");

/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe.
 */
struct clockframe {
	int	pc;	/* program counter at time of interrupt */
	int	sr;	/* status register at time of interrupt */
};

/*
 * A port must provde CLKF_USERMODE() and CLKF_BASEPRI() for use
 * in machine-independent code. These differ on r4000 and r3000 systems;
 * provide them in the port-dependent file that includes this one, using
 * the macros below.
 */

/* r3000 versions */
#define	CLKF_USERMODE_R3K(framep)	((framep)->sr & MIPS_SR_KU_PREV)
#define	CLKF_BASEPRI_R3K(framep)	\
	((~(framep)->sr & (MIPS_INT_MASK | MIPS_SR_INT_ENA_PREV)) == 0)

/* r4000 versions */
#define	CLKF_USERMODE_R4K(framep)	((framep)->sr & MIPS_SR_KSU_USER)
#define	CLKF_BASEPRI_R4K(framep)	\
	((~(framep)->sr & (MIPS_INT_MASK | MIPS_SR_INT_ENAB)) == 0)

#define	CLKF_PC(framep)		((framep)->pc)
#define	CLKF_INTR(framep)	(0)

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#define	need_resched()	{ want_resched = 1; aston(); }

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the MIPS, request an ast to send us
 * through trap, marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	{ (p)->p_flag |= P_OWEUPC; aston(); }

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)	aston()

#define aston()		(astpending = 1)

int	astpending;	/* need to trap before returning to user mode */
int	want_resched;	/* resched() was called */

/*
 * CPU identification, from PRID register.
 */
union cpuprid {
	int	cpuprid;
	struct {
#if BYTE_ORDER == BIG_ENDIAN
		u_int	pad1:16;	/* reserved */
		u_int	cp_imp:8;	/* implementation identifier */
		u_int	cp_majrev:4;	/* major revision identifier */
		u_int	cp_minrev:4;	/* minor revision identifier */
#else
		u_int	cp_minrev:4;	/* minor revision identifier */
		u_int	cp_majrev:4;	/* major revision identifier */
		u_int	cp_imp:8;	/* implementation identifier */
		u_int	pad1:16;	/* reserved */
#endif
	} cpu;
};

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}


/*
 * MIPS CPU types (cp_imp).
 */
#define	MIPS_R2000	0x01	/* MIPS R2000 CPU		ISA I   */
#define	MIPS_R3000	0x02	/* MIPS R3000 CPU		ISA I   */
#define	MIPS_R6000	0x03	/* MIPS R6000 CPU		ISA II	*/
#define	MIPS_R4000	0x04	/* MIPS R4000/4400 CPU		ISA III	*/
#define MIPS_R3LSI	0x05	/* LSI Logic R3000 derivate	ISA I	*/
#define	MIPS_R6000A	0x06	/* MIPS R6000A CPU		ISA II	*/
#define	MIPS_R3IDT	0x07	/* IDT R3000 derivate		ISA I	*/
#define	MIPS_R10000	0x09	/* MIPS R10000/T5 CPU		ISA IV  */
#define	MIPS_R4200	0x0a	/* MIPS R4200 CPU (ICE)		ISA III */
#define MIPS_UNKC1	0x0b	/* unnanounced product cpu	ISA III */
#define MIPS_UNKC2	0x0c	/* unnanounced product cpu	ISA III */
#define	MIPS_R8000	0x10	/* MIPS R8000 Blackbird/TFP	ISA IV  */
#define	MIPS_R4600	0x20	/* QED R4600 Orion		ISA III */
#define	MIPS_R3SONY	0x21	/* Sony R3000 based CPU		ISA I   */
#define	MIPS_R3TOSH	0x22	/* Toshiba R3000 based CPU	ISA I	*/
#define	MIPS_R3NKK	0x23	/* NKK R3000 based CPU		ISA I   */


/*
 * MIPS FPU types
 */
#define	MIPS_SOFT	0x00	/* Software emulation		ISA I   */
#define	MIPS_R2360	0x01	/* MIPS R2360 FPC		ISA I   */
#define	MIPS_R2010	0x02	/* MIPS R2010 FPC		ISA I   */
#define	MIPS_R3010	0x03	/* MIPS R3010 FPC		ISA I   */
#define	MIPS_R6010	0x04	/* MIPS R6010 FPC		ISA II  */
#define	MIPS_R4010	0x05	/* MIPS R4000/R4400 FPC		ISA II  */
#define MIPS_R31LSI	0x06	/* LSI Logic derivate		ISA I	*/
#define	MIPS_R10010	0x09	/* MIPS R10000/T5 FPU		ISA IV  */
#define	MIPS_R4210	0x0a	/* MIPS R4200 FPC (ICE)		ISA III */
#define MIPS_UNKF1	0x0b	/* unnanounced product cpu	ISA III */
#define	MIPS_R8000	0x10	/* MIPS R8000 Blackbird/TFP	ISA IV  */
#define	MIPS_R4600	0x20	/* QED R4600 Orion		ISA III */
#define	MIPS_R3SONY	0x21	/* Sony R3000 based FPU		ISA I   */
#define	MIPS_R3TOSH	0x22	/* Toshiba R3000 based FPU	ISA I	*/
#define	MIPS_R3NKK	0x23	/* NKK R3000 based FPU		ISA I   */

/*
 * XXX port-dependent code should define cpu_id and fpu_id variables
 * and machine-dependent cache descriptor variables.
 */

/*
 * Enable realtime clock (always enabled).
 */
#define	enablertclock()

/* Stuff from the NetBSD mips tree TTTTT */
#define CLKF_USERMODE(framep)   CLKF_USERMODE_R3K(framep)
#define CLKF_BASEPRI(framep)    CLKF_BASEPRI_R3K(framep)

#ifdef _KERNEL
union   cpuprid cpu_id;
union   cpuprid fpu_id;
u_int   machDataCacheSize;
u_int   machInstCacheSize;
extern  struct intr_tab intr_tab[];
#endif
/* End of stuff from the NetBSD mips tree TTTTT */

#endif /* _CPU_H_ */
