/*	$OpenBSD: cpu.h,v 1.21 2002/06/07 21:33:43 nordin Exp $	*/
/*	$NetBSD: cpu.h,v 1.36 1996/09/11 00:11:42 thorpej Exp $	*/

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
 *
 * from: Utah $Hdr: cpu.h 1.16 91/03/25$
 *
 *	@(#)cpu.h	7.7 (Berkeley) 6/27/91
 */
#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_

/*
 * Exported definitions unique to amiga/68k cpu support.
 */
#include <machine/psl.h>

/*
 * Get common m68k CPU definitions.
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

/*
 * Arguments to hardclock and gatherstats encapsulate the previous
 * machine state in an opaque clockframe.  One the hp300, we use
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
#define	need_resched()	{want_resched = 1; setsoftast();}

/*
 * Give a profiling tick to the current process from the softclock
 * interrupt.  On hp300, request an ast to send us through trap(),
 * marking the proc as needing a profiling tick.
 */
#define	profile_tick(p, framep)	((p)->p_flag |= P_OWEUPC, setsoftast())
#define	need_proftick(p)	((p)->p_flag |= P_OWEUPC, setsoftast())

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
#define	signotify(p)	setsoftast()

#define setsoftast()	(astpending = 1)

int	astpending;		/* need trap before returning to user mode */
int	want_resched;		/* resched() was called */

/* include support for software interrupts */
#include <machine/mtpr.h>

/*
 * The rest of this should probably be moved to ../amiga/amigacpu.h,
 * although some of it could probably be put into generic 68k headers.
 */

/* values for machineid (happen to be AFF_* settings of AttnFlags) */
#define AMIGA_68020	(1L<<1)
#define AMIGA_68030	(1L<<2)
#define AMIGA_68040	(1L<<3)
#define AMIGA_68881	(1L<<4)
#define AMIGA_68882	(1L<<5)
#define	AMIGA_FPU40	(1L<<6)
#define AMIGA_68060	(1L<<7)

#ifdef _KERNEL
int machineid;
#endif

/*
 * CTL_MACHDEP definitions.
 */
#define CPU_CONSDEV	1	/* dev_t: console terminal device */
#define CPU_MAXID	2	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}

#ifdef _KERNEL
/*
 * Prototypes from amiga_init.c
 */
void	*alloc_z2mem(long);

/*
 * Prototypes from autoconf.c
 */
int	is_a1200(void);
int	is_a3000(void);
int	is_a4000(void);
#ifdef DRACO
int	is_draco(void);
#endif

/*
 * Prototypes from clock.c
 */
u_long	clkread(void);

#ifdef DRACO
/*
 * Prototypes from kbd.c
 */
void	drkbdintr(void);

/*
 * Prototypes from drsc.c
 */
void	drsc_handler(void);
#endif

/*
 * Prototypes from locore.s
 */
struct fpframe;
struct user;
struct pcb;

void	clearseg(vm_offset_t);
void	doboot(void) __attribute__((__noreturn__));
void	loadustp(int);
void	m68881_save(struct fpframe *);
void	m68881_restore(struct fpframe *);
void	physcopyseg(vm_offset_t, vm_offset_t);
u_int	probeva(u_int, u_int);
void	proc_trampoline(void);
void	savectx(struct pcb *);
void	switch_exit(struct proc *);
void	DCIAS(vm_offset_t);
void	DCIA(void);
void	DCIS(void);
void	DCIU(void);
void	ICIA(void);
void	ICPA(void);
void	PCIA(void);
void	TBIA(void);
void	TBIS(vm_offset_t);
void	TBIAS(void);
void	TBIAU(void);
#if defined(M68040) || defined(M68060)
void	DCFA(void);
void	DCFP(vm_offset_t);
void	DCFL(vm_offset_t);
void	DCPL(vm_offset_t);
void	DCPP(vm_offset_t);
void	ICPL(vm_offset_t);
void	ICPP(vm_offset_t);
#endif

/*
 * Prototypes from machdep.c
 */
int	badaddr(caddr_t);
int	badbaddr(caddr_t);
void	bootsync(void);
void	dumpconf(void);

/*
 * Prototypes from sys_machdep.c:
 */
int	cachectl(int, caddr_t, int);
int	dma_cachectl(caddr_t, int);

/*
 * Prototypes from vm_machdep.c
 */
int	kvtop(caddr_t);
void	physaccess(caddr_t,  caddr_t, int, int);
void	physunaccess(caddr_t, int);

#ifdef GENERIC
/*
 * Prototypes from swapgeneric.c:
 */
void	setconf(void);
#endif

/*
 * Prototypes from pmap.c:
 */
void	pmap_bootstrap(vm_offset_t, vm_offset_t);
vm_offset_t pmap_map(vm_offset_t, vm_offset_t, vm_offset_t, int);

#endif /* _KERNEL */

#endif /* !_MACHINE_CPU_H_ */
