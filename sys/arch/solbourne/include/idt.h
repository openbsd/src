/*	$OpenBSD: idt.h,v 1.2 2011/03/23 16:54:37 pirofti Exp $	*/
/*
 * Copyright (c) 2005, Miodrag Vallat
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

#ifndef	_MACHINE_IDT_H_
#define	_MACHINE_IDT_H_

/*
 * Definitions for the core chips found on the IDT motherboard.
 *
 * All addresses are physical.
 */

/*
 * iGLU: GLUE Logic
 */

#define	GLU_BASE	0x60000000

/* profiling timer (level 14) */
#define	GLU_L14_DIVISOR	0x60000000
#define	GLU_L14_RESOLUTION	(256 / 5)	/* in microseconds */
#define	GLU_L14_ENABLE	0x60000008
#define	GLU_L14_IACK	0x6000000c

/* scheduling timer (level 10) */
#define	GLU_L10_IACK	0x60000800

/* board status register */
#define	GLU_BSR		0x60001800
#define	GBSR_LED_MASK		0x07
#define	GBSR_LED_OFF		00
#define	GBSR_LED_AMBER		02
#define	GBSR_LED_AMBER_BLINK	03
#define	GBSR_LED_GREEN		04
#define	GBSR_LED_GREEN_BLINK	05
#define	GBSR_LED_BOTH_BLINK	07
#define	GBSR_DIAG		0x08
#define	GBSR_WARM		0x10
#define	GBSR_NMI		0x20

/* board diagnostic register */
#define	GLU_DIAG	0x60001808
#define	GD_EXTRA_MEMORY		0x10
#define	GD_36MHZ		0x20
#define	GD_L2_CACHE		0x40

/* interrupt control register */
#define	GLU_ICR		0x60002000
#define	GICR_DISPATCH_MASK	0x0000000f	/* post a software interrupt */
#define	GICR_DISABLE_ALL	0x00000010

/* programmable interrupt levels for sbus and onboard audio */
#define	GLU_SBUS1	0x60002008
#define	GLU_SBUS2	0x60002010
#define	GLU_SBUS3	0x60002018
#define	GLU_SBUS4	0x60002020
#define	GLU_SBUS5	0x60002028
#define	GLU_SBUS6	0x60002030
#define	GLU_SBUS7	0x60002038
#define	GLU_AUDIO	0x60002040

/* reset register */
#define	GLU_RESET	0x60002800

/* programmable base for on-board i/o devices */
#define	GLU_IOBASE	0x60003800

/*
 * iMC: Memory Controller
 */

#define	MC_BASE		0x70000000

#define	MC0_MCR		0x70000001
#define	MC1_MCR		0x71000001	/* may be missing */
#define	MCR_BANK1_AVAIL		0x08
#define	MCR_BANK0_AVAIL		0x04
#define	MCR_BANK1_32M		0x02
#define	MCR_BANK0_32M		0x01

/*
 * iCU: DMA and Interrupt Controller
 */

#define	ICU_BASE	0x50000000

/* interrupt status register */
#define	ICU_ISR		0x50000000
#define	ISR_S0_DMA_SECC		0x00000001
#define	ISR_S0_DMA_MECC		0x00000002
#define	ISR_S0_DMA_SERR		0x00000004
#define	ISR_S1_DMA_SECC		0x00000008
#define	ISR_S1_DMA_MECC		0x00000010
#define	ISR_S1_DMA_SERR		0x00000020
#define	ISR_S2_DMA_SECC		0x00000040
#define	ISR_S2_DMA_MECC		0x00000080
#define	ISR_S2_DMA_SERR		0x00000100
#define	ISR_EN_DMA_SECC		0x00000200
#define	ISR_EN_DMA_MECC		0x00000400
#define	ISR_EN_DMA_SERR		0x00000800
#define	ISR_SCSI_DMA_SECC	0x00001000
#define	ISR_SCSI_DMA_MECC	0x00002000
#define	ISR_SCSI_DMA_SERR	0x00004000
#define	ISR_RIO_NMI_ENABLE	0x00008000
#define	ISR_DMA_NMI_ENABLE	0x00010000
#define	ISR_ICU_INT_ENABLE	0x00020000
#define	ISR_SECC_COUNT		0x003c0000
#define	ISR_SECC_OVERFLOW	0x00400000
#define	ISR_MEMDEC_MISS		0x00800000
#define	ISR_XLAT_INVALID	0x01000000
#define	ISR_WIN_MISS		0x02000000
#define	ISR_FAULT		0x04000000
#define	ISR_S0_RIO_ERR		0x08000000
#define	ISR_S1_RIO_ERR		0x10000000
#define	ISR_S2_RIO_ERR		0x20000000
#define	ISR_EN_RIO_ERR		0x40000000
#define	ISR_RIO_RETRY_TMO	0x80000000

#define	ISR_BITS	"\020" \
	"\01S0_SECC\02S0_MECC\03S0_SERR\04S1_SECC\05S1_MECC\06S1_SERR" \
	"\07S2_SECC\10S2_MECC\11S2_SERR\12EN_SECC\13EN_MECC\14EN_SERR" \
	"\15SCSI_SECC\16SCSSI_MECC\17SCSI_SERR\20RIO_NMIE\21DMA_NMIE\22ICU_IE" \
	"\27SECC_OVERFLOW\30MEMDEC_MISS\31XLAT_INVALID\32WIN_MISS\33FAULT" \
	"\34S0_RIO\35S1_RIO\36S2_RIO\37RIO_TMO"

#define	ICU_TIR		0x50000008

#define	ICU_TER		0x5000000c
#define	TER_S0			0x00000002
#define	TER_S1			0x00000004
#define	TER_S2			0x00000008
#define	TER_ETHERNET		0x00000010
#define	TER_SCSI		0x00000020
#define	TER_IO_DISABLE		0x00000040
#define	TER_W_COMP_DIS		0x00000080

#define	ICU_TWR		0x50000010

#define	ICU_TRR		0x50000014

#define	ICU_CONF	0x50000018
#define	CONF_ECC_ENABLE		0x00000004
#define	CONF_NO_EXTRA_MEMORY	0x00000008
#define	CONF_SBUS_25MHZ		0x00000020
#define	CONF_SLOW_DMA_WRITE	0x00000080
#define	CONF_SLOW_DMA_READ	0x00000100
#define	CONF_ICACHE_DISABLE	0x00000400

/*
 * Onboard devices
 */

#define	SE_BASE		0x40000000	/* scsi and ethernet */
#define	NVRAM_BASE	0x80000000
#define	ZS1_BASE	0x80004000
#define	ZS0_BASE	0x80008000
#define	FDC_BASE	0x8000c000
#define	AUDIO_BASE	0x80010000
#define	TODCLOCK_BASE	0x80014000

/* we map the following range 1:1 in kernel space */
#define	OBIO_PA_START	0x80000000
#define	OBIO_PA_END	0x80018000

#endif	/* _MACHINE_IDT_H_ */
