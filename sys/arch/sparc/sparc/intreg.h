/*	$OpenBSD: intreg.h,v 1.4 2000/02/21 17:08:36 art Exp $	*/
/*	$NetBSD: intreg.h,v 1.6 1997/07/22 20:19:10 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)intreg.h	8.1 (Berkeley) 6/11/93
 */

#include <sparc/sparc/vaddrs.h>

/*
 * sun4c interrupt enable register.
 *
 * The register is a single byte.  C code must use the ienab_bis and
 * ienab_bic functions found in locore.s.
 *
 * The register's physical address is defined here as the register
 * must be mapped early in the boot process (otherwise NMI handling
 * will fail).
 */
#define	INT_ENABLE_REG_PHYSADR	0xf5000000	/* phys addr in IOspace */

/*
 * Bits in interrupt enable register.  Software interrupt requests must
 * be cleared in software.  This is done in locore.s.  The ALLIE bit must
 * be cleared to clear asynchronous memory error (level 15) interrupts.
 */
#define	IE_L14		0x80	/* enable level 14 (counter 1) interrupts */
#define	IE_L10		0x20	/* enable level 10 (counter 0) interrupts */
#define	IE_L8		0x10	/* enable level 8 interrupts */
#define	IE_L6		0x08	/* request software level 6 interrupt */
#define	IE_L4		0x04	/* request software level 4 interrupt */
#define	IE_L1		0x02	/* request software level 1 interrupt */
#define	IE_ALLIE	0x01	/* enable interrupts */

#ifndef _LOCORE
void	ienab_bis __P((int bis));	/* set given bits */
void	ienab_bic __P((int bic));	/* clear given bits */
#endif

#if defined(SUN4M)
#ifdef notyet
#define IENAB_SYS	((_MAXNBPG * _MAXNCPU) + 0xc)
#define IENAB_P0	0x0008
#define IENAB_P1	0x1008
#define IENAB_P2	0x2008
#define IENAB_P3	0x3008
#endif /* notyet */
#endif

#if defined(SUN4M)
/*
 * Interrupt Control Registers, located in IO space.
 * (mapped to `locore' for now..)
 * There are two sets of interrupt registers called `Processor Interrupts'
 * and `System Interrupts'. The `Processor' set corresponds to the 15
 * interrupt levels as seen by the CPU. The `System' set corresponds to
 * a set of devices supported by the implementing chip-set.
 *
 * Briefly, the ICR_PI_* are per-processor interrupts; the ICR_SI_* are
 * system-wide interrupts, and the ICR_ITR selects the processor to get
 * the system's interrupts.
 */
#define ICR_PI_PEND		(PI_INTR_VA + 0x0)
#define ICR_PI_CLR		(PI_INTR_VA + 0x4)
#define ICR_PI_SET		(PI_INTR_VA + 0x8)
#define ICR_SI_PEND		(SI_INTR_VA)
#define ICR_SI_MASK		(SI_INTR_VA + 0x4)
#define ICR_SI_CLR		(SI_INTR_VA + 0x8)
#define ICR_SI_SET		(SI_INTR_VA + 0xc)
#define ICR_ITR			(SI_INTR_VA + 0x10)

/*
 * Bits in interrupt registers.  Software interrupt requests must
 * be cleared in software.  This is done in locore.s.
 * There are separate registers for reading pending interrupts and
 * setting/clearing (software) interrupts.
 */
#define PINTR_SINTRLEV(n)	(1 << (16 + (n)))
#define PINTR_IC		0x8000		/* Level 15 clear */

#define SINTR_MA		0x80000000	/* Mask All interrupts */
#define SINTR_ME		0x40000000	/* Module Error (async) */
#define SINTR_I			0x20000000	/* MSI (MBus-SBus) */
#define SINTR_M			0x10000000	/* ECC Memory controller */
#define SINTR_V			0x08000000	/* VME Async error */
#define SINTR_RSVD2		0x07800000
#define SINTR_F			0x00400000	/* Floppy */
#define SINTR_MI		0x00200000	/* Module interrupt */
#define SINTR_VI		0x00100000	/* Video (Supersparc only) */
#define SINTR_T			0x00080000	/* Level 10 counter */
#define SINTR_SC		0x00040000	/* SCSI */
#define SINTR_A			0x00020000	/* Audio/ISDN */
#define SINTR_E			0x00010000	/* Ethernet */
#define SINTR_S			0x00008000	/* Serial port */
#define SINTR_K			0x00004000	/* Keyboard/mouse */
#define SINTR_SBUSMASK		0x00003f80	/* SBus */
#define SINTR_SBUS(n)		(1 << (7+(n)-1))
#define SINTR_VMEMASK		0x0000007f	/* VME */
#define SINTR_VME(n)		(1 << ((n)-1))
#define SINTR_BITS		"\177\020" \
				"f\0\7VME\0f\7\7SBUS\0b\16K\0b\17S\0b\20E\0" \
				"b\21A\0b\22SC\0b\23T\0b\24VI\0b\25MI\0"     \
				"b\26F\0b\33V\0b\34M\0b\35I\0b\36ME\0b\37MA\0"


#endif
