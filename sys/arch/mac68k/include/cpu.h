/*	$NetBSD: cpu.h,v 1.26 1995/12/21 05:02:01 mycroft Exp $	*/

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

#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_	1

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#define	cpu_swapin(p)			/* nothing */
#define	cpu_wait(p)			/* nothing */
#define	cpu_swapout(p)			/* nothing */

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
#define MACH_MACPB270		77
#define MACH_MACQ840AV		78
#define MACH_MACP550		80
#define MACH_MACPB165		84
#define MACH_MACTV		88
#define MACH_MACLC475		89
#define MACH_MACLC575		92
#define MACH_MACQ605		94

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
#define MACH_CLASSIIfx	0x0080	/* The IIfx is in a class by itself. */
#define MACH_CLASSQ	0x0100	/* Centris/Quadras. */

#define MACH_68020	0
#define MACH_68030	1
#define MACH_68040	2
#define MACH_PENTIUM	3	/* 66 and 99 MHz versions *only* */

/* Defines for mmutype */
#define MMU_68040	-2
#define MMU_68030	-1
/* #define MMU_HP	0    Just a reminder as to where this came from. */
#define MMU_68851	1

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
extern	int			mmutype  ;
extern	unsigned long		load_addr;
#endif /* _KERNEL */

/* physical memory sections */
#define	ROMBASE		(0x40800000)
#define	ROMLEN		(0x01000000)		/* 16MB should be plenty! */
#define	ROMMAPSIZE	btoc(ROMLEN)		/* 16k of page tables.  */

/* This should not be used.  Use IOBase, instead. */
#define INTIOBASE	(0x50000000)

#define INTIOTOP	(IOBase+0x01000000)
#define IIOMAPSIZE	btoc(0x01000000)

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
#define NBROMOFFSET	0x00FF0000	/* Last 64K == ROM */

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
#define MMU4_RES	0x001
#define MMU4_TTR	0x002
#define MMU4_WP		0x004
#define MMU4_MOD	0x010
#define MMU4_CMMASK	0x060
#define MMU4_SUP	0x080
#define MMU4_U0		0x100
#define MMU4_U1		0x200
#define MMU4_GLB	0x400
#define MMU4_BE		0x800

/* 680X0 function codes */
#define	FC_USERD	1	/* user data space */
#define	FC_USERP	2	/* user program space */
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

#define	CACHE_ON	(DC_WA|DC_BE|DC_CLR|DC_ENABLE|IC_BE|IC_CLR|IC_ENABLE)
#define	CACHE_OFF	(DC_CLR|IC_CLR)
#define	CACHE_CLR	(CACHE_ON)
#define	IC_CLEAR	(DC_WA|DC_BE|DC_ENABLE|IC_BE|IC_CLR|IC_ENABLE)
#define	DC_CLEAR	(DC_WA|DC_BE|DC_CLR|DC_ENABLE|IC_BE|IC_ENABLE)

/* 68040 cache control register */
#define IC4_ENABLE	0x00008000	/* enable instruction cache */
#define DC4_ENABLE	0x80000000	/* enable data cache */

#define CACHE4_ON	(IC4_ENABLE|DC4_ENABLE)
#define CACHE4_OFF	0x00000000

#endif	/* !_MACHINE_CPU_H_ */
