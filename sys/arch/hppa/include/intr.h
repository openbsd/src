/*	$OpenBSD: intr.h,v 1.2 1998/07/07 21:32:41 mickey Exp $	*/

/* 
 * Copyright (c) 1990,1991,1992,1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: machspl.h 1.16 94/12/14$
 *	Author: Jeff Forys, Bob Wheeler, University of Utah CSL
 */

#ifndef	_HPPA_INTR_H_
#define	_HPPA_INTR_H_

#include <machine/iodc.h>
#include <machine/iomod.h>

/*
 * While the original 8 SPL's were "plenty", the PA-RISC chip provides us
 * with 32 possible interrupt levels.  We take advantage of this by using
 * the standard SPL names (e.g. splbio, splimp) and mapping them into the
 * PA-RISC interrupt scheme.  Obviously, to understand how SPL's work on
 * the PA-RISC, one must first have an understanding as to how interrupts
 * are handled on these chips!
 *
 * Briefly, the CPU has a 32-bit control register for External Interrupt
 * Requests (EIR).  Each bit corresponds to a specific external interrupt.
 * Bits in the EIR can be masked by the External Interrupt Enable Mask
 * (EIEM) control register.  Zero bits in the EIEM mask pending external
 * interrupt requests for the corresponding bit positions.  Finally, the
 * PSW I-bit must be set to allow interrupts to occur.
 *
 * SPL values then, are possible values for the EIEM.  For example, SPL0
 * would set the EIEM to 0xffffffff (enable all external interrupts), and
 * SPLCLOCK would set the EIEM to 0x0 (disable all external interrupts).
 */

/*
 * Define all possible External Interrupt Enable Masks (EIEMs).
 */
#define	INTPRI_00	0x00000000
#define	INTPRI_01	0x80000000
#define	INTPRI_02	0xc0000000
#define	INTPRI_03	0xe0000000
#define	INTPRI_04	0xf0000000
#define	INTPRI_05	0xf8000000
#define	INTPRI_06	0xfc000000
#define	INTPRI_07	0xfe000000
#define	INTPRI_08	0xff000000
#define	INTPRI_09	0xff800000
#define	INTPRI_10	0xffc00000
#define	INTPRI_11	0xffe00000
#define	INTPRI_12	0xfff00000
#define	INTPRI_13	0xfff80000
#define	INTPRI_14	0xfffc0000
#define	INTPRI_15	0xfffe0000
#define	INTPRI_16	0xffff0000
#define	INTPRI_17	0xffff8000
#define	INTPRI_18	0xffffc000
#define	INTPRI_19	0xffffe000
#define	INTPRI_20	0xfffff000
#define	INTPRI_21	0xfffff800
#define	INTPRI_22	0xfffffc00
#define	INTPRI_23	0xfffffe00
#define	INTPRI_24	0xffffff00
#define	INTPRI_25	0xffffff80
#define	INTPRI_26	0xffffffc0
#define	INTPRI_27	0xffffffe0
#define	INTPRI_28	0xfffffff0
#define	INTPRI_29	0xfffffff8
#define	INTPRI_30	0xfffffffc
#define	INTPRI_31	0xfffffffe
#define	INTPRI_32	0xffffffff

/*
 * Convert PA-RISC EIEMs into machine-independent SPLs as follows:
 *
 *                        1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2 3 3
 *  0 1 2 3 4 5 6 7  8  9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+---+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |c|p| | |v| | | |b i| | | | | | | |t| | | | | | | |n| | |s| | | | |
 * |l|w| | |m| | | |i m| | | | | | | |t| | | | | | | |e| | |c| | | | |
 * |k|r| | | | | | |o p| | | | | | | |y| | | | | | | |t| | |l| | | | |
 * | | | | | | | | |   | | | | | | | | | | | | | | | | | | |k| | | | |
 * +-+-+-+-+-+-+-+-+---+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * The machine-dependent SPL's are also included below (indented).
 * To change the interrupt priority of a particular device, you need
 * only change it's corresponding #define below.
 *
 * Notes:
 *	- software prohibits more than one machine-dependent SPL per bit on
 *	  a given architecture (e.g. hp700 or hp800).  In cases where there
 *	  are multiple equivalent devices which interrupt at the same level
 *	  (e.g. ASP RS232 #1 and #2), the interrupt table insertion routine
 *	  will always add in the unit number (arg0) to offset the entry.
 *	- hard clock must be the first bit (i.e. 0x80000000).
 *	- SPL7 is any non-zero value (since the PSW I-bit is off).
 *	- SPLIMP serves two purposes: blocks network interfaces and blocks
 *	  memory allocation via malloc.  In theory, SPLLAN would be high
 *	  enough.  However, on the 700, the SCSI driver uses malloc at
 *	  interrupt time requiring SPLIMP >= SPLBIO.  On the 800, we are
 *	  still using HP-UX drivers which make the assumption that
 *	  SPLIMP >= SPLCIO.  New drivers would address both problems.
 */
#define	SPLHIGH		0x00000007	/* any non-zero, non-INTPRI value */
#define	SPLCLOCK	INTPRI_00	/* hard clock */
#define	SPLPOWER	INTPRI_01	/* power failure (unused) */
#define	 SPLVIPER	 INTPRI_03		/* (hp700) Viper */
#define	SPLVM		INTPRI_04	/* TLB shootdown (unused) */
#define	SPLBIO		INTPRI_08	/* block I/O */
#define	 SPLASP		 INTPRI_08		/* (hp700) ASP */
#define	 SPLCIO		 INTPRI_08		/* (hp800) CIO */
#define	SPLIMP		INTPRI_08	/* network & malloc */
#define	 SPLEISA	 INTPRI_09		/* (hp700 EISA) */
#define  SPLCIOHPIB	 INTPRI_09		/* (hp800) CIO HP-IB */
#define	 SPLFWSCSI	 INTPRI_10		/* (hp700 internal FW SCSI) */
#define	 SPLSCSI	 INTPRI_11		/* (hp700 internal SCSI) */
#define	 SPLLAN		 INTPRI_12		/* (hp700 LAN) */
#define	 SPLCIOLAN	 INTPRI_12		/* (hp800 CIO LAN) */
#define  SPLFDDI_1	 INTPRI_13		/* FDDI #1 (graphics #1) */
#define	 SPLFDDI_2	 INTPRI_14		/* FDDI #2 (graphics #2) */
#define	SPLTTY		INTPRI_16	/* TTY */
#define	 SPLCIOMUX	 INTPRI_16		/* (hp800) CIO MUX */
#define	 SPLDCA		 INTPRI_16		/* (hp700) RS232 #1 */
/*			 INTPRI_17		 * (hp700) RS232 #2 */
#define	 SPLGRF		 INTPRI_18		/* (hp700/hp800) graphics #1) */
/*			 INTPRI_19		 * (hp700/hp800) graphics #2) */
#define	 SPLHIL		 INTPRI_20		/* (hp700/hp800) HIL */
#define	SPLNET		INTPRI_24	/* soft net */
#define	SPLSCLK		INTPRI_27	/* soft clock */
#define	SPL0		INTPRI_32	/* no interrupts masked */

/*
 * Define interrupt bits/masks.
 * N.B. A lower privilege transfer trap uses an illegal SPL_IBIT.
 */
#define SPL_IMASK_CLOCK		INTPRI_01

#define SPL_IBIT_CIO		8
#define SPL_IBIT_CIOHPIB	9
#define SPL_IBIT_CIOLAN		12
#define SPL_IBIT_CIOMUX		16
#define	SPL_IBIT_SOFTNET	24
#define	SPL_IBIT_SOFTCLK	27
#define	SPL_IBIT_LPRIV		32	/* fake interrupt */

#if !defined(_LOCORE)
/*
 * Define the machine-independent SPL routines in terms of splx().
 * To prevent cluttering the global spl() namespace, routines that
 * need machine-dependent SPLs should roll their own.
 *
 * If compiling with GCC, it's easy to inline spl's with constant
 * arguments.  However, when the argument can be variable, there
 * is little or no win; as a result, splx() is not inline'd.
 */
#define __splhigh(splhval) \
({ \
	register unsigned int _ctl_r; \
	__asm __volatile	("mfctl	15,%0"	: "=r" (_ctl_r) : ); \
	__asm __volatile	("mtctl	%0,15"	: : "r" (splhval) ); \
	__asm __volatile	("rsm	1,%%r0"	: : ); \
	_ctl_r; \
})

#define __spllow(spllval) \
({ \
	register unsigned int _ctl_r; \
	__asm __volatile	("mfctl	15,%0"	: "=r" (_ctl_r) : ); \
	__asm __volatile	("mtctl	%0,15"	: : "r" (spllval) ); \
	__asm __volatile	("ssm	1,%%r0"	: : ); \
	_ctl_r; \
})

#define	splhigh()	__splhigh(SPLHIGH)
#define	splclock()	__spllow(SPLCLOCK)
#define	splpower()	__spllow(SPLPOWER)
#define	splvm()		__spllow(SPLVM)
#define	splbio()	__spllow(SPLBIO)
#define	splimp()	__spllow(SPLIMP)
#define	spltty()	__spllow(SPLTTY)
#define	splnet()	__spllow(SPLNET)
#define	splstatclock()	__spllow(SPLCLOCK)
#define	splsoft()	__spllow(SPLSCLK)
#define	splsoftnet()	splsoft()
#define	splsoftclock()	splsoft()
#define	spl0()		__spllow(SPL0)

int	splx __P((int));
#define	setsoftclock()		(PAGE0->mem_hpa->io_eir = SPL_IBIT_SOFTCLK)
#define	setsoftnet()		(void)(1)

/*
 * BASEPRI is true when the specified EIEM is equal to the SPL level of
 * the idle loop in swtch() (i.e. SPL0).
 */
#define	BASEPRI(eiem)	((eiem) == (unsigned int)SPL0)

/*
 * This is a generic interrupt switch table.  It may be used by various
 * interrupt systems.  For each interrupt, it holds a handler and an
 * EIEM mask (selected from SPL* or, more generally, INTPRI*).
 *
 * So that these tables can be easily found, please prefix them with
 * the label "itab_" (e.g. "itab_proc").
 */
struct intrtab {
	int (*handler) __P((void));	/* ptr to routine to call */
	unsigned int intpri;	/* INTPRI (SPL) with which to call it */
	int arg0, arg1;		/* 2 arguments to handler: arg0 is unit */
};

#endif	/* !_LOCORE */
#endif	/* _HPPA_INTR_H_ */
