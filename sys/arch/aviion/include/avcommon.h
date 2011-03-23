/*	$OpenBSD: avcommon.h,v 1.6 2011/03/23 16:54:34 pirofti Exp $	*/
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
 * All rights reserved.
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
 *      This product includes software developed by Steve Murphree, Jr.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */

#ifndef	_MACHINE_AVCOMMON_H_
#define	_MACHINE_AVCOMMON_H_

/*
 * Common declarations for hardware level device registers and such,
 * for 88100-based designs.
 */

/* per-processor interrupt enable registers */
#define	AV_IEN_BASE	0xfff84000
#define	AV_IEN0		0xfff84004	/* interrupt enable CPU 0 */
#define	AV_IEN1		0xfff84008	/* interrupt enable CPU 1 */
#define	AV_IEN2		0xfff84010	/* interrupt enable CPU 2 */
#define	AV_IEN3		0xfff84020	/* interrupt enable CPU 3 */
#define	AV_IENALL	0xfff8403c	/* simultaneous write */
#define	AV_IEN(cpu)	(AV_IEN_BASE + (4 << (cpu)))

#define	AV_IST		0xfff84040 	/* interrupt status register */

#define	AV_ISTATE	0xfff84088 	/* HW interrupt status */
#define	AV_CLRINT	0xfff8408c 	/* reset HW interrupt */

#define	AV_GCSR		0xfff86000	/* global control and status reg */
#define AV_GLOBAL0	0xfff86001	/* global control and status regs */
#define AV_GLOBAL1	0xfff86003
#define	AV_LRST			0x80
#define	AV_SYSCON		0x40
#define AV_BRDID	0xfff86005
#define AV_CGCSR0	0xfff86007
#define AV_CGCSR1	0xfff86009
#define AV_CGCSR2	0xfff8600b
#define AV_CGCSR3	0xfff8600d
#define AV_CGCSR4	0xfff8600f
#define	AV_UCSR		0xfff87000	/* utility control and status reg */
#define	AV_BASAD	0xfff87004	/* base address reg */
#define	AV_GLBRES	0xfff8700c	/* global reset reg */

#define	AV_CCSR		0xfff88000	/* CPU board control status reg */
#define	AV_ERROR	0xfff88004	/* Mbus fault reg */
#define	AV_PCNFA	0xfff88008	/* Pbus A decoder reg */
#define	AV_PCNFB	0xfff8800c	/* Pbus B decoder reg */
#define	AV_EXTAD	0xfff88010	/* A24 master A24-A31 addr reg */
#define	AV_EXTAM	0xfff88014	/* AM3-AM0 addr modifiers reg */
#define	AV_WHOAMI	0xfff88018	/* whoami reg */
#define	AV_WMAD		0xfff88020	/* write mbus addr decoder reg */
#define	AV_RMAD		0xfff88024	/* read mbus addr decoder reg */
#define	AV_WVAD		0xfff88028	/* write vmebus addr decoder reg */
#define	AV_RVAD		0xfff8802c	/* read vmebus adds decoder reg */

/*
 * ISTATE and CLRINT register bits
 */

#define ISTATE_ABORT		0x04
#define	ISTATE_ACFAIL		0x02
#define	ISTATE_SYSFAIL		0x01

/*
 * UCSR register bits
 */

#define UCSR_PWRUPBIT 	0x00004000	/* powerup indicator */
#define UCSR_DRVSFBIT 	0x00002000	/* Board system fail */
#define UCSR_BRIRQBIT 	0x00001000	/* drives VME IRQ1 broadcast int */
#define UCSR_ROBINBIT 	0x00000800	/* sel round robin VME arbiter mode */
#define UCSR_BRLVBITS 	0x00000600	/* VME bus request level 0-3 */
#define UCSR_RNEVERBIT  0x00000100	/* VME bus never release once req'd */
#define UCSR_RONRBIT	0x00000080	/* VME bus req release on no request */
#define UCSR_RWDBIT	0x00000040	/* VME bus request release when done */
#define UCSR_EARBTOBIT  0x00000020	/* enable VME arbiter bus timeout */
#define VTOSELBITS	0x00000018	/* VMEbus timeout select bits */
#define VTO32US			0x00	/* 32 usec */
#define VTO64US			0x01	/* 64 usec */
#define VTO128US		0x10	/* 128 usec */
#define VTODISABLE		0x18	/* disabled */

/* Z8536 counter/timer register (not found on all models) */
#define CIO_BASE		0xfff83000
#define CIO_PORTC		0xfff83000
#define CIO_PORTB		0xfff83004
#define CIO_PORTA		0xfff83008
#define CIO_CTRL		0xfff8300c

#define CONSOLE_DART_BASE	0xfff82000

#if defined(_KERNEL) && !defined(_LOCORE)

#include <machine/intr.h>

/*
 * Interrupt masks, one per IPL level.
 */
extern u_int32_t int_mask_val[NIPLS];
extern u_int32_t ext_int_mask_val[NIPLS];

#endif

#endif	/* _MACHINE_AVCOMMON_H_ */
