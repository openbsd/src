/*	$OpenBSD: cpu.h,v 1.10 1996/10/14 01:06:58 briggs Exp $	*/
/*	$NetBSD: cpu.h,v 1.42 1996/10/12 07:35:29 scottr Exp $	*/

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
 *	Allen Briggs, Chris Caputo, Michael Finch, Brad Grantham, Lawrence Kesteloot

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

/*
   ALICE
	BG -- Sat May 23 23:58:23 EDT 1992
	Exported defines and stuff unique to mac68k.
   A lot of this stuff is really specific to the m68k, not just the macs,
   but there isn't time to do anything about that right now...
 */

#ifndef _CPU_MACHINE_
#define _CPU_MACHINE_

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
extern unsigned char ssir;

#define SIR_NET		0x1
#define SIR_CLOCK	0x2
#define SIR_SERIAL	0x4

#define siroff(x)	ssir &= ~(x)
#define setsoftnet()	ssir |= SIR_NET
#define setsoftclock()	ssir |= SIR_CLOCK
#define setsoftserial()	ssir |= SIR_SERIAL

#define CPU_CONSDEV	1
#define CPU_MAXID	2

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}

/* values for machineid --
 * 	These are equivalent to the MacOS Gestalt values. */
#define MACH_MACII		6
#define MACH_MACIIX		7
#define MACH_MACIICX		8
#define MACH_MACSE30		9
#define MACH_MACIICI		11
#define MACH_MACIIFX		13
#define MACH_MACIISI		18
#define MACH_MACQ900		20
#define MACH_MACPB170		21
#define MACH_MACQ700		22
#define MACH_MACCLASSICII	23
#define MACH_MACPB100		24
#define MACH_MACPB140		25
#define MACH_MACQ950		26
#define MACH_MACLCIII		27
#define MACH_MACPB210		29
#define MACH_MACC650		30
#define MACH_MACPB230		32
#define MACH_MACPB180		33
#define MACH_MACPB160		34
#define MACH_MACQ800		35
#define MACH_MACQ650		36
#define MACH_MACLCII		37
#define MACH_MACPB250		38
#define MACH_MACIIVI		44
#define MACH_MACP600		45
#define MACH_MACIIVX		48
#define MACH_MACCCLASSIC	49
#define MACH_MACPB165C		50
#define MACH_MACC610		52
#define MACH_MACQ610		53
#define MACH_MACPB145		54
#define MACH_MACLC520		56
#define MACH_MACC660AV		60
#define MACH_MACP460		62
#define MACH_MACPB180C		71
#define	MACH_MACPB500		72
#define MACH_MACPB270		77
#define MACH_MACQ840AV		78
#define MACH_MACP550		80
#define MACH_MACCCLASSICII	83
#define MACH_MACPB165		84
#define MACH_MACTV		88
#define MACH_MACLC475		89
#define MACH_MACLC575		92
#define MACH_MACQ605		94
#define MACH_MACQ630		98
#define MACH_MACPB280		102
#define MACH_MACPB280C		103
#define MACH_MACPB150		115

/*
 * Machine classes.  These define subsets of the above machines.
 */
#define MACH_CLASSH	0x0000	/* Hopeless cases... */
#define MACH_CLASSII	0x0001	/* MacII class */
#define MACH_CLASSIIci	0x0004	/* Have RBV, but no Egret */
#define MACH_CLASSIIsi	0x0005	/* Similar to IIci -- Have Egret. */
#define MACH_CLASSIIvx	0x0006	/* Similar to IIsi -- different via2 emul? */
#define MACH_CLASSLC	0x0007	/* Low-Cost/Performa/Wal-Mart Macs. */
#define MACH_CLASSPB	0x0008	/* Powerbooks.  Power management. */
#define MACH_CLASSDUO	0x0009	/* Powerbooks Duos.  More integration/Docks. */
#define MACH_CLASSIIfx	0x0080	/* The IIfx is in a class by itself. */
#define MACH_CLASSQ	0x0100	/* non-A/V Centris/Quadras. */
#define MACH_CLASSAV	0x0101	/* A/V Centris/Quadras. */

#define MACH_68020	0
#define MACH_68030	1
#define MACH_68040	2
#define MACH_PENTIUM	3	/* 66 and 99 MHz versions *only* */

#ifdef _KERNEL
struct mac68k_machine_S {
	int			cpu_model_index;
	/*
	 * Misc. info from booter.
	 */
	int			machineid;
	int			mach_processor;
	int			mach_memsize;
	int			booter_version;
	/*
	 * Debugging flags.
	 */
	int			do_graybars;
	int			serial_boot_echo;
	int			serial_console;
	int			modem_flags;
	int			modem_cts_clk;
	int			modem_dcd_clk;
	int			print_flags;
	int			print_cts_clk;
	int			print_dcd_clk;
	/*
	 * Misc. hardware info.
	 */
	int			scsi80;		/* Has NCR 5380 */
	int			scsi96;		/* Has NCR 53C96 */
	int			scsi96_2;	/* Has 2nd 53C96 */
	int			sonic;		/* Has SONIC e-net */

	int			sccClkConst;	/* "Constant" for SCC bps */
};

	/* What kind of model is this */
struct cpu_model_info {
	int	machineid;	/* MacOS Gestalt value. */
	char	*model_major;	/* Make this distinction to save a few */
	char	*model_minor;	/*      bytes--might be useful, too. */
	int	class;		/* Rough class of machine. */
	  /* forwarded romvec_s is defined in mac68k/macrom.h */
	struct romvec_s *rom_vectors; /* Pointer to our known rom vectors */
};
extern struct cpu_model_info *current_mac_model;

extern unsigned long		IOBase;		/* Base address of I/O */
extern unsigned long		NuBusBase;	/* Base address of NuBus */

extern  struct mac68k_machine_S	mac68k_machine;
extern	unsigned long		load_addr;
#endif /* _KERNEL */

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
u_int get_mapping __P((void));

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
int	susword __P((caddr_t, u_int));
void	savectx __P((struct pcb *));
void	proc_trampoline __P((void));

/* trap.c */
void	child_return __P((struct proc *, struct frame));

__END_DECLS

#endif	/* _CPU_MACHINE_ */
