/*	$OpenBSD: pxa2x0reg.h,v 1.30 2007/05/25 21:27:15 krw Exp $ */
/* $NetBSD: pxa2x0reg.h,v 1.4 2003/06/11 20:43:01 scw Exp $ */

/*
 * Copyright (c) 2002  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
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
 *	This product includes software developed for the NetBSD Project by
 *	Genetec Corporation.
 * 4. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Intel PXA2[15]0 processor is XScale based integrated CPU
 *
 * Reference:
 *  Intel(r) PXA250 and PXA210 Application Processors
 *   Developer's Manual
 *  (278522-001.pdf)
 *
 *  Intel PXA 27x Processor Family Developers Manual (280000-002)
 */
#ifndef _ARM_XSCALE_PXA2X0REG_H_
#define _ARM_XSCALE_PXA2X0REG_H_

/* Borrow some register definitions from sa11x0 */
#include <arm/sa11x0/sa11x0_reg.h>

#ifndef _LOCORE
#include <sys/types.h>		/* for uint32_t */
#endif

/*
 * Chip select domains
 */
#define PXA2X0_CS0_START 0x00000000
#define PXA2X0_CS1_START 0x04000000
#define PXA2X0_CS2_START 0x08000000
#define PXA2X0_CS3_START 0x0c000000
#define PXA2X0_CS4_START 0x10000000
#define PXA2X0_CS5_START 0x14000000

#define PXA2X0_PCMCIA_SLOT0  0x20000000
#define PXA2X0_PCMCIA_SLOT1  0x30000000

#define PXA2X0_PERIPH_START 0x40000000
/* #define PXA2X0_MEMCTL_START 0x48000000 */
#define PXA2X0_PERIPH_END   0x480fffff

#define PXA2X0_SDRAM0_START 0xa0000000
#define PXA2X0_SDRAM1_START 0xa4000000
#define PXA2X0_SDRAM2_START 0xa8000000
#define PXA2X0_SDRAM3_START 0xac000000
#define	PXA2X0_SDRAM_BANKS      4
#define	PXA2X0_SDRAM_BANK_SIZE  0x04000000

/*
 * Physical address of integrated peripherals
 */

#define PXA2X0_DMAC_BASE	0x40000000
#define PXA2X0_DMAC_SIZE	0x300
#define PXA27X_DMAC_SIZE	0x0400
#define PXA2X0_FFUART_BASE	0x40100000 /* Full Function UART */
#define PXA2X0_BTUART_BASE	0x40200000 /* Bluetooth UART */
#define PXA2X0_I2C_BASE		0x40300000 /* I2C Bus Interface Unit */
#define PXA2X0_I2C_SIZE		0x16a4
#define PXA2X0_I2S_BASE		0x40400000 /* Inter-IC Sound Controller */
#define PXA2X0_I2S_SIZE		0x0084
#define PXA2X0_AC97_BASE	0x40500000 /* AC '97 Controller */
#define PXA2X0_AC97_SIZE	0x0600
#define PXA2X0_USBDC_BASE 	0x40600000 /* USB Client Controller */
#define PXA2X0_USBDC_SIZE 	0x0460
#define PXA2X0_STUART_BASE	0x40700000 /* Standard UART */
#define PXA2X0_ICP_BASE 	0x40800000
#define PXA2X0_RTC_BASE 	0x40900000
#define PXA2X0_RTC_SIZE 	0x10
#define PXA2X0_OST_BASE 	0x40a00000 /* OS Timer */
#define PXA2X0_OST_SIZE		0x24
#define PXA2X0_PWM0_BASE	0x40b00000
#define PXA2X0_PWM1_BASE	0x40c00000
#define PXA2X0_INTCTL_BASE	0x40d00000 /* Interrupt controller */
#define	PXA2X0_INTCTL_SIZE	0x20
#define PXA2X0_GPIO_BASE	0x40e00000
#define PXA2X0_GPIO_SIZE  	0x70
#define PXA2X0_POWMAN_BASE  	0x40f00000 /* Power management */
#define PXA2X0_POWMAN_SIZE	0x1a4	   /* incl. PI2C unit */
#define PXA2X0_SSP_BASE		0x41000000 /* SSP serial port */
#define PXA2X0_SSP1_BASE	0x41700000 /* PXA270 */
#define PXA2X0_SSP2_BASE	0x41900000 /* PXA270 */
#define PXA2X0_SSP_SIZE		0x40
#define PXA2X0_MMC_BASE 	0x41100000 /* MultiMediaCard/SD/SDIO */
#define PXA2X0_MMC_SIZE		0x50
#define PXA2X0_CLKMAN_BASE  	0x41300000 /* Clock Manager */
#define PXA2X0_CLKMAN_SIZE	12
#define PXA2X0_LCDC_BASE	0x44000000 /* LCD Controller */
#define PXA2X0_LCDC_SIZE	0x220
#define PXA2X0_MEMCTL_BASE	0x48000000 /* Memory Controller */
#define PXA2X0_MEMCTL_SIZE	0x84
#define PXA2X0_USBHC_BASE	0x4c000000 /* USB Host Controller */
#define PXA2X0_USBHC_SIZE	0x70

/* width of interrupt controller */
#define ICU_LEN			32	/* but some are not used */
#define ICU_INT_HWMASK		0xffffff0f
#define PXA2X0_IRQ_MIN		1

/*
 * [0..1,15..16] are used as soft intrs by SI_TO_IRQBIT,
 * and [4..6] are not likely to be used by us.
 */
#define PXA2X0_INT_USBH2	2	/* USB host (all other events) */
#define PXA2X0_INT_USBH1	3	/* USB host (OHCI) */
#define PXA2X0_INT_OST		7	/* OS timers */
#define PXA2X0_INT_GPIO0	8
#define PXA2X0_INT_GPIO1	9
#define PXA2X0_INT_GPION	10	/* IRQ from GPIO[2..80] */
#define PXA2X0_INT_USB  	11
#define PXA2X0_INT_PMU  	12
#define PXA2X0_INT_I2S  	13
#define PXA2X0_INT_AC97  	14
#define PXA2X0_INT_LCD  	17
#define PXA2X0_INT_I2C  	18
#define PXA2X0_INT_ICP  	19
#define PXA2X0_INT_STUART  	20
#define PXA2X0_INT_BTUART  	21
#define PXA2X0_INT_FFUART  	22
#define PXA2X0_INT_MMC  	23
#define PXA2X0_INT_SSP  	24
#define PXA2X0_INT_DMA  	25
#define PXA2X0_INT_OST0  	26
#define PXA2X0_INT_OST1  	27
#define PXA2X0_INT_OST2  	28
#define PXA2X0_INT_OST3  	29
#define PXA2X0_INT_RTCHZ  	30
#define PXA2X0_INT_ALARM  	31	/* RTC Alarm interrupt */

/* Interrupt Controller similar to SA11x0's, but not exactly the same. */
#define INTCTL_ICIP	0x00
#define INTCTL_ICMR	0x04
#define INTCTL_ICLR	0x08
#define INTCTL_ICFP	0x0c
#define INTCTL_ICPR	0x10
#define INTCTL_ICCR	0x14
#define  ICCR_DIM	(1<<0)

/* DMAC */
#define DMAC_N_CHANNELS 		16
#define DMAC_N_CHANNELS_PXA27X		32
#define DMAC_N_PRIORITIES		3
#define DMAC_N_PRIORITIES_PXA27X	4

#define DMAC_DCSR(n)	((n)*4)
#define  DCSR_BUSERRINTR    (1<<0)	/* bus error interrupt */
#define  DCSR_STARTINR      (1<<1)	/* start interrupt */
#define  DCSR_ENDINTR       (1<<2)	/* end interrupt */
#define  DCSR_STOPSTATE     (1<<3)	/* channel is not running */
#define  DCSR_REQPEND       (1<<8)	/* request pending */
#define  DCSR_STOPIRQEN     (1<<29)     /* stop interrupt enable */
#define  DCSR_NODESCFETCH   (1<<30)	/* no-descriptor fetch mode */
#define  DCSR_RUN  	    (1<<31)
#define DMAC_DINT 	0x00f0		/* DMA interrupt */
#define  DMAC_DINT_MASK	0xffffu
#define DMAC_DRCMR(n)	(0x100+(n)*4)	/* Channel map register */
#define  DRCMR_CHLNUM	0x0f		/* channel number */
#define  DRCMR_MAPVLD	(1<<7)		/* map valid */
#define DMAC_DDADR(n)	(0x0200+(n)*16)
#define  DDADR_STOP	(1<<0)
#define DMAC_DSADR(n)	(0x0204+(n)*16)
#define DMAC_DTADR(n)	(0x0208+(n)*16)
#define DMAC_DCMD(n)	(0x020c+(n)*16)
#define  DCMD_LENGTH_MASK	0x1fff
#define  DCMD_WIDTH_SHIFT  14
#define  DCMD_WIDTH_0	(0<<DCMD_WIDTH_SHIFT)	/* for mem-to-mem transfer*/
#define  DCMD_WIDTH_1	(1<<DCMD_WIDTH_SHIFT)
#define  DCMD_WIDTH_2	(2<<DCMD_WIDTH_SHIFT)
#define  DCMD_WIDTH_4	(3<<DCMD_WIDTH_SHIFT)
#define  DCMD_SIZE_SHIFT  16
#define  DCMD_SIZE_8	(1<<DCMD_SIZE_SHIFT)
#define  DCMD_SIZE_16	(2<<DCMD_SIZE_SHIFT)
#define  DCMD_SIZE_32	(3<<DCMD_SIZE_SHIFT)
#define  DCMD_LITTLE_ENDIAN	(0<<18)
#define	 DCMD_ENDIRQEN	  (1<<21)
#define  DCMD_STARTIRQEN  (1<<22)
#define  DCMD_FLOWTRG     (1<<28)	/* flow control by target */
#define  DCMD_FLOWSRC     (1<<29)	/* flow control by source */
#define  DCMD_INCTRGADDR  (1<<30)	/* increment target address */
#define  DCMD_INCSRCADDR  (1<<31)	/* increment source address */

#ifndef __ASSEMBLER__
/* DMA descriptor */
struct pxa2x0_dma_desc {
	volatile uint32_t	dd_ddadr;
#define	DMAC_DESC_LAST	0x1
	volatile uint32_t	dd_dsadr;
	volatile uint32_t	dd_dtadr;
	volatile uint32_t	dd_dcmd;		/* command and length */
};
#endif

/* UART */
#define PXA2X0_COM_FREQ   14745600L

/* I2C */
#define I2C_IBMR	0x1680		/* Bus monitor register */
#define I2C_IDBR	0x1688		/* Data buffer */
#define I2C_ICR  	0x1690		/* Control register */
#define  ICR_START	(1<<0)
#define  ICR_STOP	(1<<1)
#define  ICR_ACKNAK	(1<<2)
#define  ICR_TB  	(1<<3)
#define  ICR_MA  	(1<<4)
#define  ICR_SCLE	(1<<5)		/* PXA270? */
#define  ICR_IUE	(1<<6)		/* PXA270? */
#define  ICR_UR		(1<<14)		/* PXA270? */
#define  ICR_FM		(1<<15)		/* PXA270? */
#define I2C_ISR  	0x1698		/* Status register */
#define  ISR_ACKNAK	(1<<1)
#define  ISR_ITE	(1<<6)
#define  ISR_IRF	(1<<7)
#define I2C_ISAR	0x16a0		/* Slave address */

/* Power Manager */
#define POWMAN_PMCR	0x00
#define POWMAN_PSSR	0x04	/* Sleep Status register */
#define  PSSR_SSS	 (1<<0)		/* Software Sleep Status */
#define  PSSR_BFS	 (1<<1)		/* Battery Fault Status */
#define  PSSR_VFS	 (1<<2)		/* VCC Fault Status */
#define  PSSR_STS	 (1<<3)		/* Standby Mode Status */
#define  PSSR_PH	 (1<<4)		/* Peripheral Control Hold */
#define  PSSR_RDH	 (1<<5)		/* Read Disable Hold */
#define  PSSR_OTGPH	 (1<<6)		/* OTG Peripheral Control Hold */
#define POWMAN_PSPR	0x08
#define POWMAN_PWER	0x0c
#define POWMAN_PRER	0x10
#define POWMAN_PFER	0x14
#define POWMAN_PEDR	0x18
#define POWMAN_PCFR	0x1c	/* General Configuration register */
#define  PCFR_OPDE	 (1<<0)
#define  PCFR_GPR_EN	 (1<<4)		/* PXA270 */
#define  PCFR_PI2C_EN	 (1<<6)		/* PXA270 */
#define  PCFR_GP_ROD	 (1<<8)		/* PXA270 */
#define  PCFR_FVC	 (1<<10)	/* PXA270 */
#define POWMAN_PGSR0	0x20	/* GPIO Sleep State register */
#define POWMAN_PGSR1	0x24
#define POWMAN_PGSR2	0x28
#define POWMAN_PGSR3	0x2c		/* PXA270 */
#define POWMAN_RCSR	0x30	/* Reset Controller Status register */
#define  RCSR_HWR	 (1<<0)
#define  RCSR_WDR	 (1<<1)
#define  RCSR_SMR	 (1<<2)
#define  RCSR_GPR	 (1<<3)
#define POWMAN_PSLR	0x34		/* PXA270 */
#define POWMAN_PKWR	0x50		/* PXA270 */
#define POWMAN_PKSR	0x54		/* PXA270 */

/* Power Manager I2C unit */
#define POWMAN_PIDBR	0x188
#define POWMAN_PICR	0x190
#define  PICR_START	ICR_START
#define  PICR_STOP	ICR_STOP
#define  PICR_ACKNAK	ICR_ACKNAK
#define  PICR_TB	ICR_TB
#define  PICR_SCLE	(1<<5)		/* PXA270? */
#define  PICR_IUE	(1<<6)		/* PXA270? */
#define  PICR_UR	(1<<14)		/* PXA270? */
#define POWMAN_PISR	0x198
#define  PISR_ACKNAK	(1<<1)
#define  PISR_ITE	(1<<6)
#define  PISR_IRF	(1<<7)
#define POWMAN_PISAR	0x1a0

/* Clock Manager */
#define CLKMAN_CCCR	0x00	/* Core Clock Configuration */
#define  CCCR_CPDIS	 (1<<31)	/* PXA270 */
#define  CCCR_A		 (1<<25)	/* PXA270 */
#define  CCCR_TURBO_X1	 (2<<7)
#define  CCCR_TURBO_X15	 (3<<7)	/* x 1.5 */
#define  CCCR_TURBO_X2	 (4<<7)
#define  CCCR_TURBO_X25	 (5<<7)	/* x 2.5 */
#define  CCCR_TURBO_X3	 (6<<7)	/* x 3.0 */
/* PXA255 */
#define  CCCR_RUN_X1	 (1<<5)
#define  CCCR_RUN_X2	 (2<<5)
#define  CCCR_RUN_X4	 (3<<5)
#define  CCCR_MEM_X27	 (1<<0)	/* x27, 99.53MHz */
#define  CCCR_MEM_X32	 (2<<0)	/* x32, 117,96MHz */
#define  CCCR_MEM_X36	 (3<<0)	/* x26, 132.71MHz */
#define  CCCR_MEM_X40	 (4<<0)	/* x27, 99.53MHz */
#define  CCCR_MEM_X45	 (5<<0)	/* x27, 99.53MHz */
#define  CCCR_MEM_X9	 (0x1f<<0)	/* x9, 33.2MHz */
/* PXA27x: L is the core run frequency to 13MHz oscillator ratio. */
#define  CCCR_RUN_X7	 (7<<0)	 /* 91MHz, 91MHz mem, 91MHz LCD */
#define  CCCR_RUN_X8	 (8<<0)	 /* 104MHz, 104MHz mem, 52MHz LCD */
#define  CCCR_RUN_X16	 (16<<0) /* 208MHz, 104/208MHz mem, 104MHz LCD */

#define CLKMAN_CKEN	0x04	/* Clock Enable Register */
#define CLKMAN_OSCC	0x08	/* Oscillator Configuration Register */

#define CCCR_N_SHIFT	7
#define CCCR_N_MASK	(0x07<<CCCR_N_SHIFT)
#define CCCR_M_SHIFT	5
#define CCCR_M_MASK	(0x03<<CCCR_M_SHIFT)
#define CCCR_L_MASK	0x1f

#define CKEN_PWM0	(1<<0)
#define CKEN_PWM1	(1<<1)
#define CKEN_AC97	(1<<2)
#define CKEN_SSP	(1<<3)
#define CKEN_STUART	(1<<5)
#define CKEN_FFUART	(1<<6)
#define CKEN_BTUART	(1<<7)
#define CKEN_I2S	(1<<8)
#define CKEN_USBHC	(1<<10)
#define CKEN_USBDC	(1<<11)
#define CKEN_MMC	(1<<12)
#define CKEN_FICP	(1<<13)
#define CKEN_I2C	(1<<14)
#define CKEN_PI2C	(1<<15)	/* PXA270? */
#define CKEN_LCD	(1<<16)
#define CKEN_KEY	(1<<19)	/* PXA270? */
#define CKEN_MEM	(1<<22)	/* PXA270? */
#define CKEN_AC97CC	(1<<31) /* PXA27x */

#define OSCC_OOK	(1<<0)	/* 32.768KHz oscillator status */
#define OSCC_OON	(1<<1)	/* 32.768KHz oscillator */

/*
 * RTC
 */
#define RTC_RCNR	0x0000	/* count register */
#define RTC_RTAR	0x0004	/* alarm register */
#define RTC_RTSR	0x0008	/* status register */
#define  RTSR_AL	(1<<0)
#define  RTSR_HZ	(1<<1)
#define  RTSR_ALE	(1<<2)
#define RTC_RTTR	0x000c	/* trim register */
/*
 * GPIO
 */
#define GPIO_GPLR0  0x00	/* Level reg [31:0] */
#define GPIO_GPLR1  0x04	/* Level reg [63:32] */
#define GPIO_GPLR2  0x08	/* Level reg [80:64] PXA 270 [95:64] */

#define GPIO_GPDR0  0x0c	/* dir reg [31:0] */
#define GPIO_GPDR1  0x10	/* dir reg [63:32] */
#define GPIO_GPDR2  0x14	/* dir reg [80:64] PXA 270 [95:64] */

#define GPIO_GPSR0  0x18	/* set reg [31:0] */
#define GPIO_GPSR1  0x1c	/* set reg [63:32] */
#define GPIO_GPSR2  0x20	/* set reg [80:64] PXA 270 [95:64] */

#define GPIO_GPCR0  0x24	/* clear reg [31:0] */
#define GPIO_GPCR1  0x28	/* clear reg [63:32] */
#define GPIO_GPCR2  0x2c	/* clear reg [80:64] PXA 270 [95:64] */

#define GPIO_GPER0  0x30	/* rising edge [31:0] */
#define GPIO_GPER1  0x34	/* rising edge [63:32] */
#define GPIO_GPER2  0x38	/* rising edge [80:64] PXA 270 [95:64] */

#define GPIO_GRER0  0x30	/* rising edge [31:0] */
#define GPIO_GRER1  0x34	/* rising edge [63:32] */
#define GPIO_GRER2  0x38	/* rising edge [80:64] PXA 270 [95:64] */

#define GPIO_GFER0  0x3c	/* falling edge [31:0] */
#define GPIO_GFER1  0x40	/* falling edge [63:32] */
#define GPIO_GFER2  0x44	/* falling edge [80:64] PXA 270 [95:64] */

#define GPIO_GEDR0  0x48	/* edge detect [31:0] */
#define GPIO_GEDR1  0x4c	/* edge detect [63:32] */
#define GPIO_GEDR2  0x50	/* edge detect [80:64] PXA 270 [95:64] */

#define GPIO_GAFR0_L  0x54	/* alternate function [15:0] */
#define GPIO_GAFR0_U  0x58	/* alternate function [31:16] */
#define GPIO_GAFR1_L  0x5c	/* alternate function [47:32] */
#define GPIO_GAFR1_U  0x60	/* alternate function [63:48] */
#define GPIO_GAFR2_L  0x64	/* alternate function [79:64] */
#define GPIO_GAFR2_U  0x68	/* alternate function [80] PXA 270 [95:80] */

#define GPIO_GAFR3_L  0x6C	/* alternate function PXA 270 [111:96] */
#define GPIO_GAFR3_U  0x70	/* alternate function PXA 270 [120:112] */

#define GPIO_GPLR3  0x100	/* Level PXA 270 [120:96] */
#define GPIO_GPDR3  0x10C	/* dir reg PXA 270 [120:96] */
#define GPIO_GPSR3  0x118	/* set reg PXA 270 [120:96] */
#define GPIO_GPCR3  0x124	/* clear reg PXA 270 [120:96] */
#define GPIO_GRER3  0x130	/* rising edge PXA 270 [120:96] */
#define GPIO_GFER3  0x13c	/* falling edge PXA 270 [120:96] */
#define GPIO_GEDR3  0x148	/* edge detect PXA270 [120:96] */

#define	GPIO_REG(r, pin)	((r) + \
    ((pin > 95) ? GPIO_GPLR3 : (((pin) / 32) * 4)))
#define	GPIO_BANK(pin)		((pin) / 32)
#define	GPIO_BIT(pin)		(1u << ((pin) & 0x1f))
#define	GPIO_FN_REG(pin)	(GPIO_GAFR0_L + (((pin) / 16) * 4))
#define	GPIO_FN_SHIFT(pin)	((pin & 0xf) * 2)

#define	GPIO_IN		  	0x00	/* Regular GPIO input pin */
#define	GPIO_OUT	  	0x10	/* Regular GPIO output pin */
#define	GPIO_ALT_FN_1_IN	0x01	/* Alternate function 1 input */
#define	GPIO_ALT_FN_1_OUT	0x11	/* Alternate function 1 output */
#define	GPIO_ALT_FN_2_IN	0x02	/* Alternate function 2 input */
#define	GPIO_ALT_FN_2_OUT	0x12	/* Alternate function 2 output */
#define	GPIO_ALT_FN_3_IN	0x03	/* Alternate function 3 input */
#define	GPIO_ALT_FN_3_OUT	0x13	/* Alternate function 3 output */
#define	GPIO_SET		0x20	/* Initial state is Set */
#define	GPIO_CLR		0x00	/* Initial state is Clear */

#define	GPIO_FN_MASK		0x03
#define	GPIO_FN_IS_OUT(n)	((n) & GPIO_OUT)
#define	GPIO_FN_IS_SET(n)	((n) & GPIO_SET)
#define	GPIO_FN(n)		((n) & GPIO_FN_MASK)
#define	GPIO_IS_GPIO(n)		(GPIO_FN(n) == 0)
#define	GPIO_IS_GPIO_IN(n)	(((n) & (GPIO_FN_MASK|GPIO_OUT)) == GPIO_IN)
#define	GPIO_IS_GPIO_OUT(n)	(((n) & (GPIO_FN_MASK|GPIO_OUT)) == GPIO_OUT)

#define	GPIO_NPINS_25x	85
#define	GPIO_NPINS	121

/*
 * memory controller
 */

#define MEMCTL_MDCNFG	0x0000
#define  MDCNFG_DE0		(1<<0)
#define  MDCNFG_DE1		(1<<1)
#define  MDCNFD_DWID01_SHIFT	2
#define  MDCNFD_DCAC01_SHIFT	3
#define  MDCNFD_DRAC01_SHIFT	5
#define  MDCNFD_DNB01_SHIFT	7
#define  MDCNFG_DE2		(1<<16)
#define  MDCNFG_DE3		(1<<17)
#define  MDCNFD_DWID23_SHIFT	18
#define  MDCNFD_DCAC23_SHIFT	19
#define  MDCNFD_DRAC23_SHIFT	21
#define  MDCNFD_DNB23_SHIFT	23

#define  MDCNFD_DWID_MASK	0x1
#define  MDCNFD_DCAC_MASK	0x3
#define  MDCNFD_DRAC_MASK	0x3
#define  MDCNFD_DNB_MASK	0x1
	
#define MEMCTL_MDREFR   0x04	/* refresh control register */
#define  MDREFR_DRI	0xfff
#define  MDREFR_E0PIN	(1<<12)
#define  MDREFR_K0RUN   (1<<13)	/* SDCLK0 enable */
#define  MDREFR_K0DB2   (1<<14)	/* SDCLK0 1/2 freq */
#define  MDREFR_E1PIN	(1<<15)
#define  MDREFR_K1RUN   (1<<16)	/* SDCLK1 enable */
#define  MDREFR_K1DB2   (1<<17)	/* SDCLK1 1/2 freq */
#define  MDREFR_K2RUN   (1<<18)	/* SDCLK2 enable */
#define  MDREFR_K2DB2	(1<<19)	/* SDCLK2 1/2 freq */
#define	 MDREFR_APD	(1<<20)	/* Auto Power Down */
#define  MDREFR_SLFRSH	(1<<22)	/* Self Refresh */
#define  MDREFR_K0FREE	(1<<23)	/* SDCLK0 free run */
#define  MDREFR_K1FREE	(1<<24)	/* SDCLK1 free run */
#define  MDREFR_K2FREE	(1<<25)	/* SDCLK2 free run */

#define MEMCTL_MSC0	0x08	/* Asynchronous Static memory Control CS[01] */
#define MEMCTL_MSC1	0x0c	/* Asynchronous Static memory Control CS[23] */
#define MEMCTL_MSC2	0x10	/* Asynchronous Static memory Control CS[45] */
#define  MSC_RBUFF_SHIFT 15	/* return data buffer */
#define  MSC_RBUFF	(1<<MSC_RBUFF_SHIFT)
#define  MSC_RRR_SHIFT   12  	/* recovery time */
#define	 MSC_RRR	(7<<MSC_RRR_SHIFT)
#define  MSC_RDN_SHIFT    8	/* ROM delay next access */
#define  MSC_RDN	(0x0f<<MSC_RDN_SHIFT)
#define  MSC_RDF_SHIFT    4	/*  ROM delay first access*/
#define  MSC_RDF  	(0x0f<<MSC_RDF_SHIFT)
#define  MSC_RBW_SHIFT    3	/* 32/16 bit bus */
#define  MSC_RBW 	(1<<MSC_RBW_SHIFT)
#define  MSC_RT_SHIFT	   0	/* type */
#define  MSC_RT 	(7<<MSC_RT_SHIFT)
#define  MSC_RT_NONBURST	0
#define  MSC_RT_SRAM    	1
#define  MSC_RT_BURST4  	2
#define  MSC_RT_BURST8  	3
#define  MSC_RT_VLIO   	 	4

/* expansion memory timing configuration */
#define MEMCTL_MCMEM(n)	(0x28+4*(n))
#define MEMCTL_MCATT(n)	(0x30+4*(n))
#define MEMCTL_MCIO(n)	(0x38+4*(n))

#define  MC_HOLD_SHIFT	14
#define  MC_ASST_SHIFT	7
#define  MC_SET_SHIFT	0
#define  MC_TIMING_VAL(hold,asst,set)	(((hold)<<MC_HOLD_SHIFT)| \
		((asst)<<MC_ASST_SHIFT)|((set)<<MC_SET_SHIFT))

#define MEMCTL_MECR	0x14	/* Expansion memory configuration */
#define MECR_NOS	(1<<0)	/* Number of sockets */
#define MECR_CIT	(1<<1)	/* Card-is-there */

#define MEMCTL_MDMRS	0x0040

#define MEMCTL_ARB_CNTRL 0x0048	/* System Bus Arbiter */

/*
 * LCD Controller
 */
#define LCDC_LCCR0	0x000	/* Controller Control Register 0 */
#define  LCCR0_ENB	(1U<<0)	/* LCD Controller Enable */
#define  LCCR0_CMS	(1U<<1)	/* Color/Mono select */
#define  LCCR0_SDS	(1U<<2)	/* Single/Dual -panel */
#define  LCCR0_LDM	(1U<<3)	/* LCD Disable Done Mask */
#define  LCCR0_SFM	(1U<<4)	/* Start of Frame Mask */
#define  LCCR0_IUM	(1U<<5)	/* Input FIFO Underrun Mask */
#define  LCCR0_EFM	(1U<<6)	/* End of Frame Mask */
#define  LCCR0_PAS	(1U<<7)	/* Passive/Active Display select */
#define  LCCR0_DPD	(1U<<9)	/* Double-Pixel Data pin mode */
#define  LCCR0_DIS	(1U<<10) /* LCD Disable */
#define  LCCR0_QDM	(1U<<11) /* LCD Quick Disable Mask */
#define  LCCR0_BM	(1U<<20) /* Branch Mask */
#define  LCCR0_OUM	(1U<<21) /* Output FIFO Underrun Mask */
/* PXA270 */
#define  LCCR0_LCDT	(1U<<22) /* LCD Panel Type */
#define  LCCR0_RDSTM	(1U<<23) /* Read Status Interrupt Mask */
#define  LCCR0_CMDIM	(1U<<24) /* Command Interrupt Mask */
#define  LCCR0_OUC	(1U<<25) /* Overlay Underlay Control */
#define  LCCR0_LDDALT	(1U<<26) /* LDD Alternate Mapping Control Bit */

#define  LCCR0_IMASK	(LCCR0_LDM|LCCR0_SFM|LCCR0_IUM|LCCR0_EFM|LCCR0_QDM|LCCR0_BM|LCCR0_OUM)


#define LCDC_LCCR1	0x004	/* Controller Control Register 1 */
#define LCDC_LCCR2	0x008	/* Controller Control Register 2 */
#define LCDC_LCCR3	0x00c	/* Controller Control Register 2 */
#define  LCCR3_BPP_SHIFT 24		/* Bits per pixel */
#define  LCCR3_BPP	(0x07<<LCCR3_BPP_SHIFT)
#define LCDC_FBR0	0x020	/* DMA ch0 frame branch register */
#define LCDC_FBR1	0x024	/* DMA ch1 frame branch register */
#define LCDC_LCSR	0x038	/* controller status register */
#define  LCSR_LDD	(1U<<0) /* LCD disable done */
#define  LCSR_SOF	(1U<<1) /* Start of frame */
#define LCDC_LIIDR	0x03c	/* controller interrupt ID Register */
#define LCDC_TRGBR	0x040	/* TMED RGB Speed Register */
#define LCDC_TCR	0x044	/* TMED Control Register */
#define LCDC_FDADR0	0x200	/* DMA ch0 frame descriptor address */
#define LCDC_FSADR0	0x204	/* DMA ch0 frame source address */
#define LCDC_FIDR0	0x208	/* DMA ch0 frame ID register */
#define LCDC_LDCMD0	0x20c	/* DMA ch0 command register */
#define LCDC_FDADR1	0x210	/* DMA ch1 frame descriptor address */
#define LCDC_FSADR1	0x214	/* DMA ch1 frame source address */
#define LCDC_FIDR1	0x218	/* DMA ch1 frame ID register */
#define LCDC_LDCMD1	0x21c	/* DMA ch1 command register */

/*
 * MMC/SD controller
 */
#define MMC_STRPCL	0x00	/* start/stop MMC clock */
#define  STRPCL_NOOP	0
#define  STRPCL_STOP	1	/* stop MMC clock */
#define  STRPCL_START	2	/* start MMC clock */
#define MMC_STAT	0x04	/* status register */
#define  STAT_READ_TIME_OUT   		(1<<0)
#define  STAT_TIMEOUT_RESPONSE		(1<<1)
#define  STAT_CRC_WRITE_ERROR		(1<<2)
#define  STAT_CRC_READ_ERROR		(1<<3)
#define  STAT_SPI_READ_ERROR_TOKEN	(1<<4)
#define  STAT_RES_CRC_ERR		(1<<5)
#define  STAT_XMIT_FIFO_EMPTY		(1<<6) /* (PXA27x: reserved) */
#define  STAT_RECV_FIFO_FULL		(1<<7) /* (PXA27x: reserved) */
#define  STAT_CLK_EN			(1<<8)
#define  STAT_DATA_TRAN_DONE		(1<<11)
#define  STAT_PRG_DONE			(1<<12)
#define  STAT_END_CMD_RES		(1<<13)
#define MMC_CLKRT	0x08	/* MMC clock rate */
#define  CLKRT_20M	0
#define  CLKRT_10M	1
#define  CLKRT_5M	2
#define  CLKRT_2_5M	3
#define  CLKRT_1_25M	4
#define  CLKRT_625K	5
#define  CLKRT_312K	6
#define MMC_SPI  	0x0c	/* SPI mode control */
#define  SPI_EN  	(1<<0)	/* enable SPI mode */
#define  SPI_CRC_ON	(1<<1)	/* enable CRC generation */
#define  SPI_CS_EN	(1<<2)	/* Enable CS[01] */
#define  SPI_CS_ADDRESS	(1<<3)	/* CS0/CS1 */
#define MMC_CMDAT	0x10	/* command/response/data */
#define  CMDAT_RESPONSE_FORMAT	0x03
#define  CMDAT_RESPONSE_FORMAT_NO 0 /* no response */
#define  CMDAT_RESPONSE_FORMAT_R1 1 /* R1, R1b, R4, R5, R5b, R6 */
#define  CMDAT_RESPONSE_FORMAT_R2 2
#define  CMDAT_RESPONSE_FORMAT_R3 3
#define  CMDAT_DATA_EN		(1<<2)
#define  CMDAT_WRITE		(1<<3) /* 1=write 0=read operation */
#define  CMDAT_STREAM_BLOCK	(1<<4) /* stream mode */
#define  CMDAT_BUSY		(1<<5) /* busy signal is expected */
#define  CMDAT_INIT		(1<<6) /* precede command with 80 clocks */
#define  CMDAT_MMC_DMA_EN	(1<<7) /* DMA enable */
#define MMC_RESTO	0x14	/* expected response time out */
#define MMC_RDTO 	0x18	/* expected data read time out */
#define MMC_BLKLEN	0x1c	/* block length of data transaction */
#define MMC_NUMBLK  	0x20	/* number of blocks (block mode) */
#define MMC_PRTBUF	0x24	/* partial MMC_TXFIFO written */
#define  PRTBUF_BUF_PART_FULL (1<<0) /* buffer partially full */
#define MMC_I_MASK	0x28	/* interrupt mask */
#define MMC_I_REG	0x2c	/* interrupt register */
#define  MMC_I_DATA_TRAN_DONE	(1<<0)
#define  MMC_I_PRG_DONE		(1<<1)
#define  MMC_I_END_CMD_RES	(1<<2)
#define  MMC_I_STOP_CMD		(1<<3)
#define  MMC_I_CLK_IS_OFF	(1<<4)
#define  MMC_I_RXFIFO_RD_REQ	(1<<5)
#define  MMC_I_TXFIFO_WR_REQ	(1<<6)
#define  MMC_I_DAT_ERR		(1<<8)	/* PXA27x */
#define  MMC_I_RES_ERR		(1<<9)	/* PXA27x */
#define  MMC_I_SDIO_INT		(1<<11)	/* PXA27x */
#define MMC_CMD  	0x30	/* index of current command */
#define MMC_ARGH 	0x34	/* MSW part of the current command arg */
#define MMC_ARGL 	0x38	/* LSW part of the current command arg */
#define MMC_RES  	0x3c	/* response FIFO */
#define MMC_RXFIFO	0x40	/* receive FIFO */
#define MMC_TXFIFO	0x44 	/* transmit FIFO */


/*
 * Inter-IC Sound (I2S) Controller
 */
#define I2S_SACR0	0x0000	/* Serial Audio Global Control */
#define  SACR0_ENB		(1<<0)	/* Enable I2S Function */
#define  SACR0_BCKD		(1<<2)	/* I/O Direction of I2S_BITCLK */
#define  SACR0_RST		(1<<3)	/* FIFO Reset */
#define  SACR0_EFWR		(1<<4)	/* Special-Purpose FIFO W/R Func */
#define  SACR0_STRF		(1<<5)	/* Select TX or RX FIFO */
#define  SACR0_TFTH_MASK	(0xf<<8) /* Trans FIFO Intr/DMA Trig Thresh */
#define  SACR0_RFTH_MASK	(0xf<<12) /* Recv FIFO Intr/DMA Trig Thresh */
#define  SACR0_SET_TFTH(x)	(((x) & 0xf)<<8)
#define  SACR0_SET_RFTH(x)	(((x) & 0xf)<<12)
#define I2S_SACR1	0x0004	/* Serial Audio I2S/MSB-Justified Control */
#define  SACR1_AMSL		(1<<0)	/* Specify Alt Mode (I2S or MSB) */
#define  SACR1_DREC		(1<<3)	/* Disable Recording Func */
#define  SACR1_DRPL		(1<<4)	/* Disable Replay Func */
#define  SACR1_ENLBF		(1<<5)	/* Enable Interface Loopback Func */
#define I2S_SASR0	0x000c	/* Serial Audio I2S/MSB-Justified Status */
#define  SASR0_TNF		(1<<0)	/* Transmit FIFO Not Full */
#define  SASR0_RNE		(1<<1)	/* Recv FIFO Not Empty */
#define  SASR0_BSY		(1<<2)	/* I2S Busy */
#define  SASR0_TFS		(1<<3)	/* Trans FIFO Service Request */
#define  SASR0_RFS		(1<<4)	/* Recv FIFO Service Request */
#define  SASR0_TUR		(1<<5)	/* Trans FIFO Underrun */
#define  SASR0_ROR		(1<<6)	/* Recv FIFO Overrun */
#define  SASR0_I2SOFF		(1<<7)	/* I2S Controller Off */
#define  SASR0_TFL_MASK		(0xf<<8) /* Trans FIFO Level */
#define  SASR0_RFL_MASK		(0xf<<12) /* Recv FIFO Level */
#define  SASR0_GET_TFL(x)	(((x) & 0xf) >> 8)
#define  SASR0_GET_RFL(x)	(((x) & 0xf) >> 12)
#define I2S_SAIMR	0x0014	/* Serial Audio Interrupt Mask */
#define  SAIMR_TFS		(1<<3)	/* Enable TX FIFO Service Req Intr */
#define  SAIMR_RFS		(1<<4)	/* Enable RX FIFO Service Req Intr */
#define  SAIMR_TUR		(1<<5)	/* Enable TX FIFO Underrun Intr */
#define  SAIMR_ROR		(1<<6)	/* Enable RX FIFO Overrun Intr */
#define I2S_SAICR	0x0018	/* Serial Audio Interrupt Clear */
#define  SAICR_TUR		(1<<5)	/* Clear Intr and SASR0_TUR */
#define  SAICR_ROR		(1<<6)	/* Clear Intr and SASR0_ROR */
#define I2S_SADIV	0x0060	/* Audio Clock Divider */
#define  SADIV_MASK		0x7f
#define  SADIV_3_058MHz		0x0c	/* 3.058 MHz */
#define  SADIV_2_836MHz		0x0d	/* 2.836 MHz */
#define  SADIV_1_405MHz		0x1a	/* 1.405 MHz */
#define  SADIV_1_026MHz		0x24	/* 1.026 MHz */
#define  SADIV_702_75kHz	0x34	/* 702.75 kHz */
#define  SADIV_513_25kHz	0x48	/* 513.25 kHz */
#define I2S_SADR	0x0080	/* Serial Audio Data Register */
#define  SADR_DTL		(0xffff<<0) /* Left Data Sample */
#define  SADR_DTH		(0xffff<<16) /* Right Data Sample */

/*
 * AC '97 Controller
 */
#define AC97_POCR	0x0000	/* PCM Out Control Register */
#define  POCR_FSRIE		(1<<1)	/* FIFO Service Request Intr Enable */
#define  POCR_FEIE		(1<<3)	/* FIFO Error Intr Enable */
#define AC97_PCMICR	0x0004	/* PCM In Control Register */
#define  PCMICR_FSRIE		(1<<1)	/* FIFO Service Request Intr Enable */
#define  PCMICR_FEIE		(1<<3)	/* FIFO Error Intr Enable */
#define AC97_MCCR	0x0008	/* Microphone In Control Register */
#define  MCCR_FSRIE		(1<<1)	/* FIFO Service Request Intr Enable */
#define  MCCR_FEIE		(1<<3)	/* FIFO Error Intr Enable */
#define AC97_GCR	0x000c	/* Global Control Register */
#define  GCR_GPI_IE		(1<<0)	/* Codec GPI Interrupt Enable */
#define  GCR_nCRST		(1<<1)	/* AC '97 Cold Reset */
#define  GCR_WRST		(1<<2)	/* AC '97 Warm Reset */
#define  GCR_ACOFF		(1<<3)	/* AC-Link Shut Off */
#define  GCR_PRES_IE		(1<<4)	/* Primary Resume Intr Enable */
#define  GCR_SRES_IE		(1<<5)	/* Secondary Resume Intr Enable */
#define  GCR_PRDY_IE		(1<<8)	/* Primary Ready Intr Enable */
#define  GCR_SRDY_IE		(1<<9)	/* Secondary Ready Intr Enable */
#define  GCR_SDONE_IE		(1<<18)	/* Status Done Intr Enable */
#define  GCR_CDONE_IE		(1<<19)	/* Command Done Intr Enable */
#define  GCR_nDMAEN		(1<<24)	/* DMA Enable (PXA27x) */
#define AC97_POSR	0x0010	/* PCM Out Status Register */
#define  POSR_FSR		(1<<2)	/* FIFO Service Request */
#define  POSR_FIFOE		(1<<4)	/* FIFO Error */
#define AC97_PCMISR	0x0014	/* PCM In Status Register */
#define  PCMISR_FSR		(1<<2)	/* FIFO Service Request */
#define  PCMISR_ECC		(1<<3)	/* DMA End of Chain Intr */
#define  PCMISR_FIFOE		(1<<4)	/* FIFO Error */
#define AC97_MCSR	0x0018	/* Microphone In Status Register */
#define  MCSR_FSR		(1<<2)	/* FIFO Service Request */
#define  MCSR_ECC		(1<<3)	/* DMA End of Chain Intr */
#define  MCSR_FIFOE		(1<<4)	/* FIFO Error */
#define AC97_GSR	0x001c	/* Global Status Register */
#define  GSR_GSCI		(1<<0)	/* Codec GPI Status Change Intr */
#define  GSR_MIINT		(1<<1)	/* Modem-In Intr */
#define  GSR_MOINT		(1<<2)	/* Modem-Out Intr */
#define  GSR_ACOFFD		(1<<3)	/* AC-link Shut Off Done */
#define  GSR_PIINT		(1<<5)	/* PCM-In Intr */
#define  GSR_POINT		(1<<6)	/* PCM-Out Intr */
#define  GSR_MCINT		(1<<7)	/* Mic-In Intr */
#define  GSR_PCRDY		(1<<8)	/* Primay Codec Ready */
#define  GSR_SCRDY		(1<<9)	/* Secondary Codec Ready */
#define  GSR_PRESINT		(1<<10)	/* Primary Resume Intr */
#define  GSR_SRESINT		(1<<11)	/* Secondary Resume Intr */
#define  GSR_B1S12		(1<<12)	/* Bit 1 of Slot 12 */
#define  GSR_B2S12		(1<<13)	/* Bit 2 of Slot 12 */
#define  GSR_B3S12		(1<<14)	/* Bit 3 of Slot 12 */
#define  GSR_RCS		(1<<15)	/* Read Completion Status */
#define  GSR_SDONE		(1<<18)	/* Status Done */
#define  GSR_CDONE		(1<<19)	/* Command Done */
#define AC97_CAR	0x0020	/* Codec Access Register */
#define  CAR_CAIP		(1<<0)	/* Codec Access In Progress */
/* 0x0024 to 0x003c is reserved */
#define AC97_PCDR	0x0040	/* PCM Data Register */
#define  PCDR_PCML		(0xffff<<0)	/* PCM Left Channel Data */
#define  PCDR_PCMR		(0xffff<<16)	/* PCM Right Channel Data */
/* 0x0044 to 0x005c is reserved */
#define AC97_MCDR	0x0060	/* Microphone In Data Register */
#define  MCDR_MCDAT		(0xffff<<0)	/* Mic-In Data */
/* 0x0064 to 0x00fc is reserved */
#define AC97_MOCR	0x0100	/* Modem Out Control Register */
#define  MOCR_FSRIE		(1<<1)	/* FIFO Service Request Intr Enable */
#define  MOCR_FEIE		(1<<3)	/* FIFO Error Intr Enable */
/* 0x0104 is reserved */
#define AC97_MICR	0x0108	/* Modem In Control Register */
#define  MICR_FSRIE		(1<<1)	/* FIFO Service Request Intr Enable */
#define  MICR_FEIE		(1<<3)	/* FIFO Error Intr Enable */
/* 0x010c is reserved */
#define AC97_MOSR	0x0110	/* Modem Out Status Register */
#define  MOSR_FSR		(1<<2)	/* FIFO Service Request */
#define  MOSR_FIFOE		(1<<2)	/* FIFO Error */
/* 0x0114 is reserved */
#define AC97_MISR	0x0118	/* Modem In Status Register */
#define  MOSR_FSR		(1<<2)	/* FIFO Service Request */
#define  MOSR_EOC		(1<<2)	/* DMA End of Chain Intr */
#define  MOSR_FIFOE		(1<<2)	/* FIFO Error */
/* 0x011c  to 0x013c is reserved */
#define AC97_MODR	0x0140	/* Modem Data Register */
#define  MODR_MODAT		(0xffff<<0)	/* Modem Data */
/* 0x0144 to 0x01fc is reserved */

#define AC97_PRIAUDIO	0x0200	/* Primary Audio Codec Registers */
#define AC97_SECAUDIO	0x0300	/* Secondary Audio Codec Registers */
#define AC97_PRIMODEM	0x0400	/* Primary Modem Codec Registers */
#define AC97_SECMODEM	0x0500	/* Secondary modem Codec Registers */

/*
 * USB device controller differs between pxa255 and pxa27x, defined separately
 */

/*
 * USB Host Controller
 */
#define USBHC_STAT	0x0060	/* UHC Status Register */
#define  USBHC_STAT_RWUE	(1<<7)	/* HCI Remote Wake-Up Event */
#define  USBHC_STAT_HBA		(1<<8)	/* HCI Buffer Active */
#define  USBHC_STAT_HTA		(1<<10)	/* HCI Transfer Abort */
#define  USBHC_STAT_UPS1	(1<<11)	/* USB Power Sense Port 1 */
#define  USBHC_STAT_UPS2	(1<<12)	/* USB Power Sense Port 2 */
#define  USBHC_STAT_UPRI	(1<<13)	/* USB Port Resume Interrupt */
#define  USBHC_STAT_SBTAI	(1<<14)	/* System Bus Target Abort Interrupt */
#define  USBHC_STAT_SBMAI	(1<<15)	/* System Bus Master Abort Interrupt */
#define  USBHC_STAT_UPS3	(1<<16)	/* USB Power Sense Port 3 */
#define  USBHC_STAT_MASK	(USBHC_STAT_RWUE | USBHC_STAT_HBA | \
    USBHC_STAT_HTA | USBHC_STAT_UPS1 | USBHC_STAT_UPS2 | USBHC_STAT_UPRI | \
    USBHC_STAT_SBTAI | USBHC_STAT_SBMAI | USBHC_STAT_UPS3)
#define USBHC_HR	0x0064	/* UHC Reset Register */
#define  USBHC_HR_FSBIR		(1<<0)	/* Force System Bus Interface Reset */
#define  USBHC_HR_FHR		(1<<1)	/* Force Host Controller Reset */
#define  USBHC_HR_CGR		(1<<2)	/* Clock Generation Reset */
#define  USBHC_HR_SSDC		(1<<3)	/* Simulation Scale Down Clock */
#define  USBHC_HR_UIT		(1<<4)	/* USB Interrupt Test */
#define  USBHC_HR_SSE		(1<<5)	/* Sleep Standby Enable */
#define  USBHC_HR_PSPL		(1<<6)	/* Power Sense Polarity Low */
#define  USBHC_HR_PCPL		(1<<7)	/* Power Control Polarity Low */
#define  USBHC_HR_SSEP1		(1<<9)	/* Sleep Standby Enable for Port 1 */
#define  USBHC_HR_SSEP2		(1<<10)	/* Sleep Standby Enable for Port 2 */
#define  USBHC_HR_SSEP3		(1<<11)	/* Sleep Standby Enable for Port 3 */
#define  USBHC_HR_MASK		(USBHC_HR_FSBIR | USBHC_HR_FHR | \
    USBHC_HR_CGR | USBHC_HR_SSDC | USBHC_HR_UIT | USBHC_HR_SSE | \
    USBHC_HR_PSPL | USBHC_HR_PCPL | USBHC_HR_SSEP1 | USBHC_HR_SSEP2 | \
    USBHC_HR_SSEP3)
#define USBHC_HIE	0x0068	/* UHC Interrupt Enable Register */
#define  USBHC_HIE_RWIE		(1<<7)	/* HCI Remote Wake-Up */
#define  USBHC_HIE_HBAIE	(1<<8)	/* HCI Buffer Active */
#define  USBHC_HIE_TAIE		(1<<10)	/* HCI Interface Transfer Abort */
#define  USBHC_HIE_UPS1IE	(1<<11)	/* USB Power Sense Port 1 */
#define  USBHC_HIE_UPS2IE	(1<<12)	/* USB Power Sense Port 2 */
#define  USBHC_HIE_UPRIE	(1<<13)	/* USB Port Resume */
#define  USBHC_HIE_UPS3IE	(1<<14)	/* USB Power Sense Port 3 */
#define  USBHC_HIE_MASK		(USBHC_HIE_RWIE | USBHC_HIE_HBAIE | \
    USBHC_HIE_TAIE | USBHC_HIE_UPS1IE | USBHC_HIE_UPS2IE | USBHC_HIE_UPRIE | \
    USBHC_HIE_UPS3IE)
#define USBHC_HIT	0x006C	/* UHC Interrupt Test Register */
#define  USBHC_HIT_RWUT		(1<<7)	/* HCI Remote Wake-Up */
#define  USBHC_HIT_BAT		(1<<8)	/* HCI Buffer Active */
#define  USBHC_HIT_IRQT		(1<<9)	/* Normal OHC */
#define  USBHC_HIT_TAT		(1<<10)	/* HCI Interface Transfer Abort */
#define  USBHC_HIT_UPS1T	(1<<11)	/* USB Power Sense Port 1 */
#define  USBHC_HIT_UPS2T	(1<<12)	/* USB Power Sense Port 2 */
#define  USBHC_HIT_UPRT		(1<<13)	/* USB Port Resume */
#define  USBHC_HIT_STAT		(1<<14)	/* System Bus Target Abort */
#define  USBHC_HIT_SMAT		(1<<15)	/* System Bus Master Abort */
#define  USBHC_HIT_UPS3T	(1<<16)	/* USB Power Sense Port 3 */
#define  USBHC_HIT_MASK		(USBHC_HIT_RWUT | USBHC_HIT_BAT | \
    USBHC_HIT_IRQT | USBHC_HIT_TAT | USBHC_HIT_UPS1T | USBHC_HIT_UPS2T | \
    USBHC_HIT_UPRT | USBHC_HIT_STAT | USBHC_HIT_SMAT | USBHC_HIT_UPS3T)
#define USBHC_RST_WAIT	10000	/* usecs to wait for reset */

/* OS Timer */
#define OST_OSMR0	0x0000	/* Match 0 */
#define OST_OSMR1	0x0004	/* Match 1 */
#define OST_OSMR2	0x0008	/* Match 2 */
#define OST_OSMR3	0x000c	/* Match 3 */
#define OST_OSCR0	0x0010	/* Counter 0 */

#define OST_OSCR4	0x0040	/* Counter 4 */
#define OST_OMCR4	0x00c0	/* Counter 4 match control */
#define OST_OSMR4	0x0080	/* Counter 4 match */
#define OST_OSCR5	0x0044	/* Counter 5 */
#define OST_OMCR5	0x00c4	/* Counter 5 match control */
#define OST_OSMR5	0x0084	/* Counter 4 match */

#define OST_OSSR	0x0014	/* Status (all counters) */
#define OST_OWER	0x0018	/* Watchdog Enable */
#define  OWER_WME	 (1<<0)
#define OST_OIER	0x001c	/* Interrupt Enable */
#define  OIER_E3	 (1<<3)

/* Synchronous Serial Protocol (SSP) serial ports */
#define SSP_SSCR0	0x00
#define SSP_SSCR1	0x04
#define SSP_SSSR	0x08
#define  SSSR_TNF	(1<<2)
#define  SSSR_RNE	(1<<3)
#define SSP_SSDR	0x10

#endif /* _ARM_XSCALE_PXA2X0REG_H_ */
