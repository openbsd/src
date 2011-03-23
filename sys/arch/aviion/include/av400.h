/*	$OpenBSD: av400.h,v 1.8 2011/03/23 16:54:34 pirofti Exp $	*/
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

#ifndef	_MACHINE_AV400_H_
#define	_MACHINE_AV400_H_

#define	AV400_PROM		0xffc00000
#define	AV400_PROM_SIZE		0x00200000
#define	AV400_SRAM		0xffe00000
#define	AV400_SRAM_SIZE		0x00020000
#define	AV400_UTILITY		0xfff00000
#define	AV400_UTILITY_SIZE	0x00100000

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

/*
 * AV400 declarations for hardware level device registers and such.
 */

#define	AV400_SETSWI	0xfff84080 	/* generate soft interrupt */
#define	AV400_CLRSWI	0xfff84084 	/* reset soft interrupt */

#define	AV400_VIRQLV	0xfff85000
#define	AV400_VIACK1V	0xfff85004
#define	AV400_VIACK2V	0xfff85008
#define	AV400_VIACK3V	0xfff8500c
#define	AV400_VIACK4V	0xfff85010
#define	AV400_VIACK5V	0xfff85014
#define	AV400_VIACK6V	0xfff85018
#define	AV400_VIACK7V	0xfff8501c
#define	AV400_VIRQV	0xfff85020

/*
 * IEN and IST register bits
 * See ``Programming System control and I/O registers for the 100, 200, 300,
 * 400, 3000 and 4000 series'', section 3 (Interrupts).
 */

#define	AV400_IRQ_RESERVED	0x1800018c	/* all reserved bits */
#define AV400_IRQ_ABORT		0x80000000	/* 31 - Abort */
#define AV400_IRQ_ACF		0x40000000	/* 30 - AC Fail */
#define AV400_IRQ_ARBTO		0x20000000	/* 29 - VME Arbiter Timeout */
#define AV400_IRQ_ZBUF		0x04000000	/* 26 - Z Buffer */
#define AV400_IRQ_VID		0x02000000	/* 25 - Video */
#define AV400_IRQ_PAR		0x01000000	/* 24 - Parity Error */
#define AV400_IRQ_VME7		0x00800000	/* 23 - VMEBus level 7 */
#define AV400_IRQ_KBD		0x00400000	/* 22 - Keyboard */
#define AV400_IRQ_CIOI		0x00200000	/* 21 - CIO */
#define AV400_IRQ_SF		0x00100000	/* 20 - System Failure */
#define AV400_IRQ_VME6		0x00080000	/* 19 - VMEBus level 6 */
#define AV400_IRQ_PPI		0x00040000	/* 18 - Parallel Port */
#define AV400_IRQ_DI1		0x00020000	/* 17 - DUART1 */
#define AV400_IRQ_DI2		0x00010000	/* 16 - DUART2 */
#define AV400_IRQ_ECI		0x00008000	/* 15 - Ethernet Controller */
#define AV400_IRQ_VME5		0x00004000	/* 14 - VMEBus level 5 */
#define AV400_IRQ_DTC		0x00002000	/* 13 - DMA Terminal Count */
#define AV400_IRQ_VME4		0x00001000	/* 12 - VMEBus level 4 */
#define AV400_IRQ_DWP		0x00000800	/* 11 - DMA Write Protect */
#define AV400_IRQ_VME3		0x00000400	/* 10 - VMEBus level 3 */
#define AV400_IRQ_DVB		0x00000200	/* 09 - DMA Valid Bit */
#define AV400_IRQ_VME2		0x00000040	/* 06 - VMEBus level 2 */
#define AV400_IRQ_SCI		0x00000020	/* 05 - SCSI Controller */
#define AV400_IRQ_VME1		0x00000010	/* 04 - VMEBus level 1 */
#define AV400_IRQ_SWI1		0x00000002	/* 01 - SW Interrupt level 1 */
#define AV400_IRQ_SWI0		0x00000001	/* 00 - SW Interrupt level 0 */

#define AV400_IST_STRING	"\20" \
	"\40ABRT\37ACF\36ARBTO\33ZBUF\32VID\31PAR" \
	"\30IRQ7\27KBD\26CIOI\25SF\24IRQ6\23PPI\22DI1\21DI2" \
	"\20ECI\17IRQ5\16DTC\15IRQ4\14DWP\13IRQ3\12DVB" \
	"\7IRQ2\6SCI\5IRQ1\2SWI1\1SWI0"

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

#endif	/* _MACHINE_AV400_H_ */
