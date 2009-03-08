/*	$OpenBSD: mvme188.h,v 1.33 2009/03/08 16:03:05 miod Exp $ */
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

#ifndef	__MACHINE_MVME188_H__
#define	__MACHINE_MVME188_H__

#define MVME188_EPROM		0xffc00000
#define MVME188_EPROM_SIZE	0x00080000
#define MVME188_SRAM		0xffe00000
#define MVME188_SRAM_SIZE	0x00020000
#define MVME188_UTILITY		0xfff00000
#define MVME188_UTILITY_SIZE	0x00090000

/*
 * MVME188 declarations for hardware level device registers and such.
 */

/* per-processor interrupt enable registers */
#define	MVME188_IENBASE	0xfff84000
#define MVME188_IEN0	0xfff84004	/* interrupt enable CPU 0 */
#define MVME188_IEN1	0xfff84008	/* interrupt enable CPU 1 */
#define MVME188_IEN2	0xfff84010	/* interrupt enable CPU 2 */
#define MVME188_IEN3	0xfff84020	/* interrupt enable CPU 3 */
#define	MVME188_IENALL	0xfff8403c	/* simultaneous write */
#define	MVME188_IEN(cpu)	(MVME188_IENBASE + (4 << (cpu)))

#define MVME188_IST	0xfff84040 	/* interrupt status register */

#define MVME188_SETSWI	0xfff84080 	/* generate soft interrupt */
#define MVME188_CLRSWI	0xfff84084 	/* reset soft interrupt */
#define MVME188_ISTATE	0xfff84088 	/* HW interrupt status */
#define MVME188_CLRINT	0xfff8408c 	/* reset HW interrupt */

#define MVME188_VIRQLV	0xfff85000
#define	MVME188_VIACK1V	0xfff85004
#define	MVME188_VIACK2V	0xfff85008
#define	MVME188_VIACK3V	0xfff8500c
#define	MVME188_VIACK4V	0xfff85010
#define	MVME188_VIACK5V	0xfff85014
#define	MVME188_VIACK6V	0xfff85018
#define	MVME188_VIACK7V	0xfff8501c
#define	MVME188_VIRQV	0xfff85020
#define M188_IVEC		0x40	/* vector returned upon MVME188 int */

#define	MVME188_GLOBAL0	0xfff86001	/* global control and status regs */
#define	MVME188_GLOBAL1	0xfff86003
#define	M188_LRST		0x80
#define	M188_SYSCON		0x40
#define	MVME188_BRDID	0xfff86005
#define	MVME188_CGCSR0	0xfff86007
#define	MVME188_CGCSR1	0xfff86009
#define	MVME188_CGCSR2	0xfff8600b
#define	MVME188_CGCSR3	0xfff8600d
#define	MVME188_CGCSR4	0xfff8600f
#define MVME188_UCSR	0xfff87000	/* utility control and status reg */
#define	MVME188_BASAD	0xfff87004	/* base address reg */
#define MVME188_GLBRES	0xfff8700c	/* global reset reg */

#define MVME188_CCSR	0xfff88000	/* CPU board control status reg */
#define MVME188_ERROR	0xfff88004	/* Mbus fault reg */
#define MVME188_PCNFA	0xfff88008	/* Pbus A decoder reg */
#define MVME188_PCNFB	0xfff8800c	/* Pbus B decoder reg */
#define MVME188_EXTAD	0xfff88010	/* A24 master A24-A31 addr reg */
#define MVME188_WHOAMI	0xfff88018	/* whoami reg */
#define MVME188_WMAD	0xfff88020	/* write mbus addr decoder reg */
#define MVME188_RMAD	0xfff88024	/* read mbus addr decoder reg */
#define MVME188_WVAD	0xfff88028	/* write vmebus addr decoder reg */
#define MVME188_RVAD	0xfff8802c	/* read vmebus adds decoder reg */

/*
 * IEN and IST register bits
 * Refer to MVME188 RISC Microcomputer User's Manual, table 4.3
 */

#define IRQ_ABORT		0x80000000	/* 31 */
#define IRQ_ACF			0x40000000	/* 30 */
#define IRQ_ARBTO		0x20000000	/* 29 */
#define IRQ_DTI			0x10000000	/* 28 */
#define IRQ_SWI7		0x08000000	/* 27 */
#define IRQ_SWI6		0x04000000	/* 26 */
#define IRQ_SWI5		0x02000000	/* 25 */
#define IRQ_SWI4		0x01000000	/* 24 */
#define IRQ_VME7		0x00800000	/* 23 */
#define IRQ_CIOI		0x00200000	/* 21 */
#define IRQ_SF			0x00100000	/* 20 */
#define IRQ_VME6		0x00080000	/* 19 */
#define IRQ_DI			0x00020000	/* 17 */
#define IRQ_SIGHPI		0x00010000	/* 16 */
#define IRQ_VME5		0x00004000	/* 14 */
#define IRQ_VME4		0x00001000	/* 12 */
#define IRQ_VME3		0x00000400	/* 10 */
#define IRQ_LMI			0x00000100	/* 08 */
#define IRQ_SIGLPI		0x00000080	/* 07 */
#define IRQ_VME2		0x00000040	/* 06 */
#define IRQ_VME1		0x00000010	/* 04 */
#define IRQ_SWI3		0x00000008	/* 03 */
#define IRQ_SWI2		0x00000004	/* 02 */
#define IRQ_SWI1		0x00000002	/* 01 */
#define IRQ_SWI0		0x00000001	/* 00 */

#define IST_STRING	"\20" \
	"\40ABRT\37ACF\36ARBTO\35DTI\34SWI7\33SWI6\32SWI5\31SWI4" \
	"\30IRQ7\26CIOI\25SF\24IRQ6\22DI\21SIGHPI" \
	"\17IRQ5\15IRQ4\13IRQ3\11LMI" \
	"\10SIGLPI\7IRQ2\5IRQ1\4SWI3\3SWI2\2SWI1\1SWI0"

/* IPI bits (see below) */
#define	CLOCK_IPI_MASK		(IRQ_SWI7 | IRQ_SWI6 | IRQ_SWI5 | IRQ_SWI4)
#define	IPI_MASK		(IRQ_SWI3 | IRQ_SWI2 | IRQ_SWI1 | IRQ_SWI0)

#define ISR_GET_CURRENT_MASK(cpu) \
	(*(volatile u_int *)MVME188_IST & int_mask_reg[cpu])

/*
 * Software interrupts 0 to 3, and 4 to 7, are used to deliver IPIs to cpu0-3.
 * We use two bits because we want clock ipis to be maskable.
 * We rely on the fact that the control bits for these interrupts are
 * the same in the interrupt registers and the set/clear SWI registers.
 */
/* values for SETSWI and CLRSWI registers */
#define	SWI_IPI_BIT(cpuid)		(0x01 << (cpuid))
#define	SWI_CLOCK_IPI_BIT(cpuid)	(0x10 << (cpuid))
/* values for IEN and IST registers */
#define	SWI_IPI_MASK(cpuid)		(IRQ_SWI0 << (cpuid))
#define	SWI_CLOCK_IPI_MASK(cpuid)	(IRQ_SWI4 << (cpuid))

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

/* these are the various Z8536 CIO counter/timer registers */
#define CIO_BASE		0xfff83000
#define CIO_PORTC		0xfff83000
#define CIO_PORTB		0xfff83004
#define CIO_PORTA		0xfff83008
#define CIO_CTRL		0xfff8300c

#define DART_BASE		0xfff82000

/*
 * HYPERmodule CMMU addresses
 */

#define VME_CMMU_I0	0xfff7e000
#define VME_CMMU_I1	0xfff7d000
#define VME_CMMU_I2	0xfff7b000
#define VME_CMMU_I3	0xfff77000
#define VME_CMMU_D0	0xfff6f000
#define VME_CMMU_D1	0xfff5f000
#define VME_CMMU_D2	0xfff3f000
#define VME_CMMU_D3	0xfff7f000

#if defined(_KERNEL) && !defined(_LOCORE)
extern u_int32_t pfsr_save_188_straight[];
extern u_int32_t pfsr_save_188_double[];
extern u_int32_t pfsr_save_188_quad[];

extern u_int32_t int_mask_val[NIPLS];
#endif

#endif	/* __MACHINE_MVME188_H__ */
