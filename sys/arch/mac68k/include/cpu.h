/*	$OpenBSD: cpu.h,v 1.18 1998/03/01 00:37:36 niklas Exp $	*/
/*	$NetBSD: cpu.h,v 1.45 1997/02/10 22:13:40 scottr Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
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
 */

/*
 *	Copyright (c) 1992, 1993 BCDL Labs.  All rights reserved.
 *	Allen Briggs, Chris Caputo, Michael Finch, Brad Grantham,
 *	Lawrence Kesteloot
 *
 *	Redistribution of this source code or any part thereof is permitted,
 *	 provided that the following conditions are met:
 *	1) Utilized source contains the copyright message above, this list
 *	 of conditions, and the following disclaimer.
 *	2) Binary objects containing compiled source reproduce the
 *	 copyright notice above on startup.
 *
 *	CAVEAT: This source code is provided "as-is" by BCDL Labs, and any
 *	 warranties of ANY kind are disclaimed.  We don't even claim that it
 *	 won't crash your hard disk.  Basically, we want a little credit if
 *	 it works, but we don't want to get mail-bombed if it doesn't. 
 */

/*
 * from: Utah $Hdr: cpu.h 1.16 91/03/25$
 *
 *	@(#)cpu.h	7.7 (Berkeley) 6/27/91
 */

#ifndef _MAC68K_CPU_H_
#define _MAC68K_CPU_H_

#include <machine/pcb.h>

/*
 * Get common m68k definitions.
 */
#include <m68k/cpu.h>
#define	M68K_MMU_MOTOROLA

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_swapin(p)			/* nothing */
#define	cpu_wait(p)			/* nothing */
#define	cpu_swapout(p)			/* nothing */
void cpu_set_kpc __P((struct proc *, void (*)(struct proc *)));

/*
 * Arguments to hardclock, softclock and gatherstats
 * encapsulate the previous machine state in an opaque
 * clockframe; for hp300, use just what the hardware
 * leaves on the stack.
 */

struct clockframe {
	u_short	sr;
	u_long	pc;
	u_short	vo;
};

#define	CLKF_USERMODE(framep)	(((framep)->sr & PSL_S) == 0)
#define	CLKF_BASEPRI(framep)	(((framep)->sr & PSL_IPL) == 0)
#define	CLKF_PC(framep)		((framep)->pc)
#define	CLKF_INTR(framep)	(0) /* XXX should use PSL_M (see hp300) */

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
#define	need_resched()	{ want_resched++; aston(); }

/*
 * Give a profiling tick to the current process from the softclock
 * interrupt.  Request an ast to send us through trap(),
 * marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	( (p)->p_flag |= P_OWEUPC, aston() )

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)	aston()

#define aston() (astpending++)

int	astpending;	/* need to trap before returning to user mode */
int	want_resched;	/* resched() was called */

/*
 * simulated software interrupt register
 */
extern volatile u_int8_t ssir;

#define	SIR_NET		0x01
#define	SIR_CLOCK	0x02
#define	SIR_SERIAL	0x04

/* Mac-specific SSIR(s) */
#define	SIR_DTMGR	0x80

#define	siroff(mask)	\
	__asm __volatile ( "andb %0,_ssir" : : "ir" (~(mask) & 0xff));
#define	setsoftnet()	\
	__asm __volatile ( "orb %0,_ssir" : : "i" (SIR_NET))
#define	setsoftclock()	\
	__asm __volatile ( "orb %0,_ssir" : : "i" (SIR_CLOCK))
#define	setsoftserial()	\
	__asm __volatile ( "orb %0,_ssir" : : "i" (SIR_SERIAL))
#define	setsoftdtmgr()	\
	__asm __volatile ( "orb %0,_ssir" : : "i" (SIR_DTMGR))

#define CPU_CONSDEV	1
#define CPU_MAXID	2

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}

/* physical memory sections */
#define	ROMBASE		(0x40800000)
#define	ROMLEN		(0x00200000)		/* 2MB should be enough! */
#define	ROMMAPSIZE	btoc(ROMLEN)		/* 32k of page tables.  */

#define IIOMAPSIZE	btoc(0x00100000)

/* XXX -- Need to do something about superspace.
 * Technically, NuBus superspace starts at 0x60000000, but no
 * known Macintosh has used any slot lower numbered than 9, and
 * the super space is defined as 0xS000 0000 through 0xSFFF FFFF
 * where S is the slot number--ranging from 0x9 - 0xE.
 */
#define	NBSBASE		0x90000000
#define	NBSTOP		0xF0000000
#define NBBASE		0xF9000000	/* NUBUS space */
#define NBTOP		0xFF000000	/* NUBUS space */
#define NBMAPSIZE	btoc(NBTOP-NBBASE)	/* ~ 96 megs */
#define NBMEMSIZE	0x01000000	/* 16 megs per card */

__BEGIN_DECLS
/* machdep.c */
void	mac68k_set_bell_callback __P((int (*)(void *, int, int, int), void *));
int	mac68k_ring_bell __P((int, int, int));
u_int	get_mapping __P((void));

/* locore.s */
void	m68881_restore __P((struct fpframe *));
void	m68881_save __P((struct fpframe *));
u_int	getsfc __P((void));
u_int	getdfc __P((void));
void	TBIA __P((void));
void	TBIAS __P((void));
void	TBIS __P((vm_offset_t));
void	DCFP __P((vm_offset_t));
void	ICPP __P((vm_offset_t));
void	DCIU __P((void));
void	ICIA __P((void));
void	DCFL __P((vm_offset_t));
int	suline __P((caddr_t, caddr_t));
void	savectx __P((struct pcb *));
void	proc_trampoline __P((void));

/* pmap.c */
vm_offset_t pmap_map __P((vm_offset_t, vm_offset_t, vm_offset_t, int));

/* trap.c */
void	child_return __P((struct proc *, struct frame));

/* vm_machdep.c */
void	physaccess __P((caddr_t, caddr_t, register int, register int));

__END_DECLS

#endif	/* _MAC68K_CPU_H_ */
