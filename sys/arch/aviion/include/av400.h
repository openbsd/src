/*	$OpenBSD: av400.h,v 1.2 2006/05/11 19:50:30 miod Exp $	*/
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

#ifndef	__MACHINE_AV400_H__
#define	__MACHINE_AV400_H__

#define	AV400_PROM		0xffc00000
#define	AV400_PROM_SIZE		0x00200000	/* was 020000 */
#define	AV400_SRAM		0xffe00000
#define	AV400_SRAM_SIZE		0x00020000
#define	AV400_UTILITY		0xfff00000
#define	AV400_UTILITY_SIZE	0x00100000
#define	PROM_VBR		0xffc00000

/*
 * AV400 VME mappings
 */

#define	AV400_VME32_BASE	0x00000000
#define	AV400_VME32_START1	0x10000000
#define	AV400_VME32_END1	0x7fffffff
#define	AV400_VME32_START2	0x90000000
#define	AV400_VME32_END2	0xfdffffff
#define	AV400_VME24_BASE	0xfe000000
#define	AV400_VME24_START	0x00000000
#define	AV400_VME24_END		0x00ffffff
#define	AV400_VME16_BASE	0xffff0000
#define	AV400_VME16_START	0x00000000
#define	AV400_VME16_END		0x0000ffff

#define	AV400_ISVMEA32(addr) \
	(((addr) >= AV400_VME32_START1 && (addr) < AV400_VME32_END1 + 1U) || \
	 ((addr) >= AV400_VME32_START2 && (addr) < AV400_VME32_END2 + 1U))
#define	AV400_ISVMEA24(addr) \
	((addr) >= AV400_VME24_START && (addr) < AV400_VME24_END + 1U)
#define	AV400_ISVMEA16(addr) \
	((addr) >= AV400_VME16_START /* && (addr) <= AV400_VME16_END */)

/*
 * AV400 declarations for hardware level device registers and such.
 */

/* per-processor interrupt enable registers */
#define	AV400_IEN0	0xfff84004	/* interrupt enable CPU 0 */
#define	AV400_IEN1	0xfff84008	/* interrupt enable CPU 1 */
#define	AV400_IEN2	0xfff84010	/* interrupt enable CPU 2 */
#define	AV400_IEN3	0xfff84020	/* interrupt enable CPU 3 */
#define	AV400_IENALL	0xfff8403c	/* simultaneous write */
#define	AV400_IEN(cpu)	(AV400_IEN0 + ((cpu) << 2))

#define	AV400_IST	0xfff84040 	/* interrupt status register */

#define	AV400_SETSWI	0xfff84080 	/* generate soft interrupt */
#define	AV400_CLRSWI	0xfff84084 	/* reset soft interrupt */
#define	AV400_ISTATE	0xfff84088 	/* HW interrupt status */
#define	AV400_CLRINT	0xfff8408c 	/* reset HW interrupt */

#define	AV400_VIRQLV	0xfff85000
#define	AV400_VIACK1V	0xfff85004
#define	AV400_VIACK2V	0xfff85008
#define	AV400_VIACK3V	0xfff8500c
#define	AV400_VIACK4V	0xfff85010
#define	AV400_VIACK5V	0xfff85014
#define	AV400_VIACK6V	0xfff85018
#define	AV400_VIACK7V	0xfff8501c
#define	AV400_VIRQV	0xfff85020
#define	AV400_IVEC		0x40	/* vector returned upon AV400 int */

#define	AV400_GCSR	0xfff86000	/* global control and status reg */
#define	AV400_UCSR	0xfff87000	/* utility control and status reg */
#define	AV400_BASAD	0xfff87004	/* base address reg */
#define	AV400_GLBRES	0xfff8700c	/* global reset reg */

#define AV400_GLOBAL0	0xfff86001	/* global control and status regs */
#define AV400_GLOBAL1	0xfff86003
#define	AV400_LRST		0x80
#define	AV400_SYSCON		0x40
#define AV400_BRDID	0xfff86005
#define AV400_CGCSR0	0xfff86007
#define AV400_CGCSR1	0xfff86009
#define AV400_CGCSR2	0xfff8600b
#define AV400_CGCSR3	0xfff8600d
#define AV400_CGCSR4	0xfff8600f
#define	AV400_UCSR	0xfff87000	/* utility control and status reg */
#define	AV400_BASAD	0xfff87004	/* base address reg */
#define	AV400_GLBRES	0xfff8700c	/* global reset reg */

#define	AV400_CCSR	0xfff88000	/* CPU board control status reg */
#define	AV400_ERROR	0xfff88004	/* Mbus fault reg */
#define	AV400_PCNFA	0xfff88008	/* Pbus A decoder reg */
#define	AV400_PCNFB	0xfff8800c	/* Pbus B decoder reg */
#define	AV400_EXTAD	0xfff88010	/* A24 master A24-A31 addr reg */
#define	AV400_EXTAM	0xfff88014	/* AM3-AM0 addr modifiers reg */
#define	AV400_WHOAMI	0xfff88018	/* whoami reg */
#define	AV400_WMAD	0xfff88020	/* write mbus addr decoder reg */
#define	AV400_RMAD	0xfff88024	/* read mbus addr decoder reg */
#define	AV400_WVAD	0xfff88028	/* write vmebus addr decoder reg */
#define	AV400_RVAD	0xfff8802c	/* read vmebus adds decoder reg */

/*
 * IEN and IST register bits
 * See ``Programming System control and I/O registers for the 100, 200, 300,
 * 400, 3000 and 4000 series'', section 3 (Interrupts).
 */

#define	IRQ_RESERVED	0x1800018c	/* all reserved bits */
#define IRQ_ABORT	0x80000000	/* 31 - Abort */
#define IRQ_ACF		0x40000000	/* 30 - AC Fail */
#define IRQ_ARBTO	0x20000000	/* 29 - VME Arbiter Timeout */
#define IRQ_ZBUF	0x04000000	/* 26 - Z Buffer */
#define IRQ_VID		0x02000000	/* 25 - Video */
#define IRQ_PAR		0x01000000	/* 24 - Parity Error */
#define IRQ_VME7	0x00800000	/* 23 - VMEBus level 7 */
#define IRQ_KBD		0x00400000	/* 22 - Keyboard */
#define IRQ_CIOI	0x00200000	/* 21 - CIO */
#define IRQ_SF		0x00100000	/* 20 - System Failure */
#define IRQ_VME6	0x00080000	/* 19 - VMEBus level 6 */
#define IRQ_PPI		0x00040000	/* 18 - Parallel Port */
#define IRQ_DI1		0x00020000	/* 17 - DUART1 */
#define IRQ_DI2		0x00010000	/* 16 - DUART2 */
#define IRQ_ECI		0x00008000	/* 15 - Ethernet Controller */
#define IRQ_VME5	0x00004000	/* 14 - VMEBus level 5 */
#define IRQ_DTC		0x00002000	/* 13 - DMA Terminal Count */
#define IRQ_VME4	0x00001000	/* 12 - VMEBus level 4 */
#define IRQ_DWP		0x00000800	/* 11 - DMA Write Protect */
#define IRQ_VME3	0x00000400	/* 10 - VMEBus level 3 */
#define IRQ_DVB		0x00000200	/* 09 - DMA Valid Bit */
#define IRQ_VME2	0x00000040	/* 06 - VMEBus level 2 */
#define IRQ_SCI		0x00000020	/* 05 - SCSI Controller */
#define IRQ_VME1	0x00000010	/* 04 - VMEBus level 1 */
#define IRQ_SWI1	0x00000002	/* 01 - SW Interrupt level 1 */
#define IRQ_SWI0	0x00000001	/* 00 - SW Interrupt level 0 */

#define IST_STRING	"\20" \
	"\40ABRT\37ACF\36ARBTO\33ZBUF\32VID\31PAR" \
	"\30IRQ7\27KBD\26CIOI\25SF\24IRQ6\23PPI\22DI1\21DI2" \
	"\20ECI\17IRQ5\16DTC\15IRQ4\14DWP\13IRQ3\12DVB" \
	"\7IRQ2\6SCI\5IRQ1\2SWI1\1SWI0"

/* groups by function */

/* hardware irq bits */
#define HW_FAILURE_MASK		(IRQ_ABORT | IRQ_ACF | IRQ_ARBTO | IRQ_SF | \
				 IRQ_PAR)
/* software irq bits */
#define SOFT_INTERRUPT_MASK	(IRQ_SWI1 | IRQ_SWI0)
/* VME irq bits */
#define VME_INTERRUPT_MASK	(IRQ_VME7 | IRQ_VME6 | IRQ_VME5 | IRQ_VME4 | \
				 IRQ_VME3 | IRQ_VME2 | IRQ_VME1)
/* on-board irq bits */
#define OBIO_INTERRUPT_MASK	(IRQ_ZBUF | IRQ_VID | IRQ_KBD | IRQ_CIOI | \
				 IRQ_PPI | IRQ_DI1 | IRQ_DI2 | IRQ_ECI | \
				 IRQ_DTC | IRQ_DWP | IRQ_DVB | IRQ_SCI)

/* groups by interrupt levels */
/* we do not enable and define levels yet for: ZBUF, VID, KBD, PPI, SCI, DMA */

#define LVL7			(IRQ_ABORT | IRQ_ACF | IRQ_VME7 | IRQ_SF)
#define LVL6			(IRQ_VME6)
#define LVL5			(IRQ_VME5 | IRQ_CIOI)
#define LVL4			(IRQ_VME4)
#define LVL3			(IRQ_VME3 | IRQ_DI1 | IRQ_DI2)
#define LVL2			(IRQ_VME2) /* | IRQ_SCI */
#define LVL1			(IRQ_VME1 | IRQ_ECI )
#define LVL0			(0x0)

/* interrupts we want to process on the master CPU only */
#define SLAVE_MASK		(HW_FAILURE_MASK | OBIO_INTERRUPT_MASK)

#define MASK_LVL_0		(LVL7 | LVL6 | LVL5 | LVL4 | LVL3 | LVL2 | LVL1)
#define MASK_LVL_1		(LVL7 | LVL6 | LVL5 | LVL4 | LVL3 | LVL2)
#define MASK_LVL_2		(LVL7 | LVL6 | LVL5 | LVL4 | LVL3)
#define MASK_LVL_3		(LVL7 | LVL6 | LVL5 | LVL4)
#define MASK_LVL_4		(LVL7 | LVL6 | LVL5)
#define MASK_LVL_5		(LVL7 | LVL6)
#define MASK_LVL_6		(LVL7)
#define MASK_LVL_7		(IRQ_ABORT)

#define INT_LEVEL	        8		/* # of interrupt level + 1 */
#define ISR_GET_CURRENT_MASK(cpu) \
	(*(volatile u_int *)AV400_IST & int_mask_reg[cpu])

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

#define CIO_MICR		0x00	/* Master interrupt control register */
#define CIO_MICR_MIE		0x80
#define CIO_MICR_DLC		0x40
#define CIO_MICR_NV		0x20
#define CIO_MICR_PAVIS		0x10
#define CIO_MICR_PBVIS		0x08
#define CIO_MICR_CTVIS		0x04
#define CIO_MICR_RJA		0x02
#define CIO_MICR_RESET		0x01

#define CIO_MCCR		0x01	/* Master config control register */
#define CIO_MCCR_PBE		0x80
#define CIO_MCCR_CT1E		0x40
#define CIO_MCCR_CT2E		0x20
#define CIO_MCCR_CT3E		0x10
#define CIO_MCCR_PLC		0x08
#define CIO_MCCR_PAE		0x04

#define CIO_CTMS1		0x1c	/* Counter/timer mode specification #1 */
#define CIO_CTMS2		0x1d	/* Counter/timer mode specification #2 */
#define CIO_CTMS3		0x1e	/* Counter/timer mode specification #3 */
#define CIO_CTMS_CSC		0x80	/* Continuous Single Cycle */
#define CIO_CTMS_EOE		0x40	/* External Output Enable  */
#define CIO_CTMS_ECE		0x20	/* External Count Enable   */
#define CIO_CTMS_ETE		0x10	/* External Trigger Enable */
#define CIO_CTMS_EGE		0x08	/* External Gate Enable    */
#define CIO_CTMS_REB		0x04	/* Retrigger Enable Bit    */
#define CIO_CTMS_PO		0x00	/* Pulse Output            */
#define CIO_CTMS_OSO		0x01	/* One Shot Output         */
#define CIO_CTMS_SWO		0x02	/* Square Wave Output      */

#define CIO_IVR			0x04	/* Interrupt vector register */

#define CIO_CSR1		0x0a	/* Command and status register CTC #1 */
#define CIO_CSR2		0x0b	/* Command and status register CTC #2 */
#define CIO_CSR3		0x0c	/* Command and status register CTC #3 */

#define CIO_CT1MSB		0x16	/* CTC #1 Timer constant - MSB */
#define CIO_CT1LSB		0x17	/* CTC #1 Timer constant - LSB */
#define CIO_CT2MSB		0x18	/* CTC #2 Timer constant - MSB */
#define CIO_CT2LSB		0x19	/* CTC #2 Timer constant - LSB */
#define CIO_CT3MSB		0x1a	/* CTC #3 Timer constant - MSB */
#define CIO_CT3LSB		0x1b	/* CTC #3 Timer constant - LSB */
#define CIO_PDCA		0x23	/* Port A data direction control */
#define CIO_PDCB		0x2b	/* Port B data direction control */

#define CIO_GCB			0x04	/* CTC Gate command bit */
#define CIO_TCB			0x02	/* CTC Trigger command bit */
#define CIO_IE			0xc0	/* CTC Interrupt enable (set) */
#define CIO_CIP			0x20	/* CTC Clear interrupt pending */
#define CIO_IP			0x20	/* CTC Interrupt pending */

#define DART_BASE		0xfff82000

/*
 * CMMU addresses
 */

#define AV400_CMMU_D0	0xfff00000
#define AV400_CMMU_I0	0xfff01000
#define AV400_CMMU_D1	0xfff02000
#define AV400_CMMU_I1	0xfff03000
#define AV400_CMMU_D2	0xfff04000
#define AV400_CMMU_I2	0xfff05000
#define AV400_CMMU_D3	0xfff06000
#define AV400_CMMU_I3	0xfff07000

#endif	/* __MACHINE_AV400_H__ */
