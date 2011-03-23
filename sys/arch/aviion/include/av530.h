/*	$OpenBSD: av530.h,v 1.4 2011/03/23 16:54:34 pirofti Exp $	*/
/*
 * Copyright (c) 2006, 2010 Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHINE_AV530_H_
#define	_MACHINE_AV530_H_

#define	AV530_PROM		0xffc00000
#define	AV530_PROM_SIZE		0x00080000
#define	AV530_UTILITY		0xfff00000
#define	AV530_UTILITY_SIZE	0x00100000

/*
 * AV530 VME mappings
 */

#define	AV530_VME32_BASE	0x00000000
#define	AV530_VME32_START1	0x40000000
#define	AV530_VME32_END1	0x7fffffff
#define	AV530_VME32_START2	0x90000000
#define	AV530_VME32_END2	0xfdffffff
#define	AV530_VME24_BASE	0xfe000000
#define	AV530_VME24_START	0x00000000
#define	AV530_VME24_END		0x00ffffff
#define	AV530_VME16_BASE	0xffff0000
#define	AV530_VME16_START	0x00000000
#define	AV530_VME16_END		0x0000ffff

/*
 * AV530 declarations for hardware level device registers and such.
 */

#define	AV530_SETSWI	0xfff84080 	/* generate soft interrupt */
#define	AV530_CLRSWI	0xfff84084 	/* reset soft interrupt */

#define	AV530_VIRQLV	0xfff85000
#define	AV530_VIACK1V	0xfff85004
#define	AV530_VIACK2V	0xfff85008
#define	AV530_VIACK3V	0xfff8500c
#define	AV530_VIACK4V	0xfff85010
#define	AV530_VIACK5V	0xfff85014
#define	AV530_VIACK6V	0xfff85018
#define	AV530_VIACK7V	0xfff8501c
#define	AV530_VIRQV	0xfff85020

/*
 * IEN and IST register bits
 * See ``Programming System Control and I/O Registers: AViiON 4600 and
 * 530 series'', section 3 (Interrupts).
 */

#define	AV530_IRQ_RESERVED	0x0020aa00	/* all reserved bits */
#define AV530_IRQ_ABORT		0x80000000	/* 31 - Abort */
#define AV530_IRQ_ACF		0x40000000	/* 30 - AC Fail */
#define AV530_IRQ_ARBTO		0x20000000	/* 29 - VME Arbiter Timeout */
#define	AV530_IRQ_DTI		0x10000000	/* 28 - DUART Timer Interrupt */
#define AV530_IRQ_SWI7		0x08000000	/* 27 - SW Interrupt level 7 */
#define AV530_IRQ_SWI6		0x04000000	/* 26 - SW Interrupt level 6 */
#define AV530_IRQ_SWI5		0x02000000	/* 25 - SW Interrupt level 5 */
#define AV530_IRQ_SWI4		0x01000000	/* 24 - SW Interrupt level 4 */
#define AV530_IRQ_VME7		0x00800000	/* 23 - VMEBus level 7 */
#define AV530_IRQ_KBD		0x00400000	/* 22 - Keyboard */
#define AV530_IRQ_SF		0x00100000	/* 20 - System Failure */
#define AV530_IRQ_VME6		0x00080000	/* 19 - VMEBus level 6 */
#define AV530_IRQ_MEM		0x00040000	/* 18 - Memory Error */
#define AV530_IRQ_DI		0x00020000	/* 17 - DUART */
#define AV530_IRQ_SIGHPI	0x00010000	/* 16 - SIGHPI */
#define AV530_IRQ_VME5		0x00004000	/* 14 - VMEBus level 5 */
#define AV530_IRQ_VME4		0x00001000	/* 12 - VMEBus level 4 */
#define AV530_IRQ_VME3		0x00000400	/* 10 - VMEBus level 3 */
#define	AV530_IRQ_LMI		0x00000100	/* 08 - Location Monitor */
#define	AV530_IRQ_SIGLPI	0x00000080	/* 07 - SIGLPI */
#define AV530_IRQ_VME2		0x00000040	/* 06 - VMEBus level 2 */
#define AV530_IRQ_VME1		0x00000010	/* 04 - VMEBus level 1 */
#define AV530_IRQ_SWI3		0x00000008	/* 03 - SW Interrupt level 3 */
#define AV530_IRQ_SWI2		0x00000004	/* 02 - SW Interrupt level 2 */
#define AV530_IRQ_SWI1		0x00000002	/* 01 - SW Interrupt level 1 */
#define AV530_IRQ_SWI0		0x00000001	/* 00 - SW Interrupt level 0 */

#define AV530_IST_STRING	"\20" \
	"\40ABRT\37ACF\36ARBTO\35DTI\34SWI7\33SWI6\32SWI5\31SWI4" \
	"\30IRQ7\27KBD\25SF\24IRQ6\23MEM\22DI\21SIGHPI" \
	"\17IRQ5\15IRQ4\13IRQ3\11LMI" \
	"\10SIGLPI\7IRQ2\5IRQ1\4SWI3\3SWI2\2SWI1\1SWI0"

/*
 * Software interrupts 0 to 3, and 4 to 7, are used to deliver IPIs to
 * CPU0-3. We use two software interrupts per CPU because we want clock
 * IPIs to be maskable.
 */
#define	AV530_CLOCK_IPI_MASK	(AV530_IRQ_SWI7 | AV530_IRQ_SWI6 | \
				 AV530_IRQ_SWI5 | AV530_IRQ_SWI4)
#define	AV530_IPI_MASK		(AV530_IRQ_SWI3 | AV530_IRQ_SWI2 | \
				 AV530_IRQ_SWI1 | AV530_IRQ_SWI0)
/* values for SETSWI and CLRSWI registers */
#define	AV530_SWI_IPI_BIT(cpu)		(0x01 << (cpu))
#define	AV530_SWI_CLOCK_IPI_BIT(cpu)	(0x10 << (cpu))
/* values for IEN and IST registers */
#define	AV530_SWI_IPI_MASK(cpu)		(AV530_IRQ_SWI0 << (cpu))
#define	AV530_SWI_CLOCK_IPI_MASK(cpu)	(AV530_IRQ_SWI4 << (cpu))

/*
 * Extended interrupts
 */

#define	AV_EXIEN_BASE	0xfff8e000
#define	AV_EXIEN0	0xfff8e004
#define	AV_EXIEN1	0xfff8e008
#define	AV_EXIEN2	0xfff8e010
#define	AV_EXIEN3	0xfff8e020
#define	AV_EXIENALL	0xfff8e03c
#define	AV_EXIEN(cpu)	(AV_EXIEN_BASE + (4 << (cpu)))

#define	AV_EXIST	0xfff8e040

/*
 * EXIEN and EXIST register bits
 * See ``Programming System Control and I/O Registers: AViiON 4600 and
 * 530 series'', section 3 (Interrupts).
 */

#define	AV530_EXIRQ_RESERVED	0x04000e9f	/* all reserved bits */
#define AV530_EXIRQ_RTCOF	0x80000000	/* 31 - RTC Overflow */
#define AV530_EXIRQ_PIT3OF	0x40000000	/* 30 - PIT 3 Overflow */
#define AV530_EXIRQ_PIT2OF	0x20000000	/* 29 - PIT 2 Overflow */
#define AV530_EXIRQ_PIT1OF	0x10000000	/* 28 - PIT 1 Overflow */
#define AV530_EXIRQ_PIT0OF	0x08000000	/* 27 - PIT 0 Overflow */
#define	AV530_EXIRQ_DMA4C	0x02000000	/* 25 - DMA4 Channel Complete */
#define	AV530_EXIRQ_DMA3C	0x01000000	/* 24 - DMA3 Channel Complete */
#define	AV530_EXIRQ_DMA2C	0x00800000	/* 23 - DMA2 Channel Complete */
#define	AV530_EXIRQ_DMA1C	0x00400000	/* 22 - DMA1 Channel Complete */
#define	AV530_EXIRQ_DMA0C	0x00200000	/* 21 - DMA0 Channel Complete */
#define	AV530_EXIRQ_SCC		0x00100000	/* 20 - SCC */
#define	AV530_EXIRQ_LAN0	0x00080000	/* 19 - Ethernet 0 */
#define	AV530_EXIRQ_LAN1	0x00040000	/* 18 - Ethernet 1 */
#define	AV530_EXIRQ_SCSI0	0x00020000	/* 17 - SCSI0 */
#define	AV530_EXIRQ_SCSI1	0x00010000	/* 16 - SCSI1 */
#define	AV530_EXIRQ_VIDEO	0x00008000	/* 15 - Video */
#define	AV530_EXIRQ_ZBUF	0x00004000	/* 14 - Z Buffer */
#define	AV530_EXIRQ_DUART2	0x00002000	/* 13 - DUART2 */
#define	AV530_EXIRQ_VDMA	0x00001000	/* 12 - VDMA */
#define	AV530_EXIRQ_IOEXP1	0x00000100	/* 8 - IO Expansion 1 */
#define	AV530_EXIRQ_IOEXP2	0x00000040	/* 6 - IO Expansion 2 */
#define	AV530_EXIRQ_PDMA	0x00000020	/* 5 - Parallel Printer DMA */

#define AV530_EXIST_STRING	"\20" \
	"\40RTCOF\37PIT3OF\36PIT2OF\35PIT1OF\34PIT0OF\32DMA4\31DMA3" \
	"\30DMA2\27DMA1\26DMA0\25SCC\24LAN0\23LAN1\22SCSI0\21SCSI1" \
	"\20VIDEO\17ZBUF\16DUART2\15VDMA\11IOEXP1" \
	"\7IOEXP2\6PDMA"

/*
 * Miscellaneous registers
 */

#define	AV530_SRST		0xfff83100	/* software reset */
#define	SRST_KBD			0x08
#define	SRST_DUART2			0x02
#define	SRST_DUART1			0x01
#define	AV530_IOBRDID0		0xfffcf000	/* byte access */
#define	AV530_IOBRDID1		0xfffcf004	/* byte access */
#define	AV530_CONFIG		0xfff8fffc

#define	AV530_IOFUSE0		0xfffb0040	/* byte access */
#define	AV530_IOFUSE1		0xfffb00c0	/* byte access */
#define	AV530_IOFUSE_LAN		0x02
#define	AV530_IOFUSE_SCSI		0x01

/*
 * PIT and RTC
 */

#define	AV530_PIT0_CNT		0xfff8f004
#define	AV530_PIT1_CNT		0xfff8f008
#define	AV530_PIT2_CNT		0xfff8f010
#define	AV530_PIT3_CNT		0xfff8f020
#define	AV530_PIT0_CS		0xfff8f044
#define	AV530_PIT1_CS		0xfff8f048
#define	AV530_PIT2_CS		0xfff8f050
#define	AV530_PIT3_CS		0xfff8f060
#define	AV530_PIT_CMD_ALL	0xfff8f07c
#define	AV530_PIT_CTEN			0x00000008
#define	AV530_PIT_TEST			0x00000004
#define	AV530_PIT_IACK			0x00000002
#define	AV530_PIT_RESET			0x00000001

#define	AV530_RTC_CNT		0xfff8f084
#define	AV530_RTC_CS		0xfff8f088
#define	AV530_RTC_IACK			0x00000002
#define	AV530_RTC_RESET			0x00000001

/*
 * Onboard device addresses
 */

#define	AV530_SCSI1	0xfffb0000
#define	AV530_SCSI2	0xfffb0080
#define	AV530_LAN1	0xfffb00c0
#define	AV530_LAN2	0xfffb0140

/*
 * CMMU addresses
 */

#define AV530_CMMU_D0	0xfff00000
#define AV530_CMMU_I0	0xfff01000
#define AV530_CMMU_D1	0xfff02000
#define AV530_CMMU_I1	0xfff03000

#define AV530_CMMU6_D0	0xfff00000
#define AV530_CMMU6_D1	0xfff01000
#define AV530_CMMU6_I0	0xfff04000
#define AV530_CMMU6_I1	0xfff05000
#define AV530_CMMU6_I2	0xfff06000
#define AV530_CMMU6_I3	0xfff07000
#define AV530_CMMU6_D2	0xfff08000
#define AV530_CMMU6_D3	0xfff09000
#define AV530_CMMU6_I4	0xfff0c000
#define AV530_CMMU6_I5	0xfff0d000
#define AV530_CMMU6_I6	0xfff0e000
#define AV530_CMMU6_I7	0xfff0f000

#endif	/* _MACHINE_AV530_H_ */
