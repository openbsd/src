/*	$OpenBSD: sa1111_reg.h,v 1.3 2008/06/26 05:42:09 ray Exp $ */
/*	$NetBSD: sa1111_reg.h,v 1.3 2002/12/18 04:09:31 bsh Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by IWAMOTO Toshihiro.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ARM_SA11X0_SA1111_REG_H
#define _ARM_SA11X0_SA1111_REG_H


/* Interrupt Controller */

/* number of interrupt bits */
#define SACCIC_LEN	55

/* System Bus Interface */
#define SACCSBI_SKCR		0x0000
#define  SKCR_PLLBYPASS  	(1<<0)
#define  SKCR_RCLKEN    	(1<<1)
#define  SKCR_SLEEP     	(1<<2)
#define  SKCR_DOZE		(1<<3)
#define  SKCR_VCOOFF		(1<<4)
#define  SKCR_RDYEN		(1<<7)
#define  SKCR_SELAC		(1<<8)	/* AC Link or I2S */
#define  SKCR_NOEEN		(1<<12)	/* Enable nOE */
#define SACCSBI_SMCR		0x0004
#define SACCSBI_SKID		0x0008

/* System Controller */
#define SACCSC_SKPCR		0x0200

/* USB Host Controller */
#define SACCUSB_REVISION	0x0400
#define SACCUSB_CONTROL		0x0404
#define SACCUSB_STATUS		0x0408
#define SACCUSB_RESET		0x051C

/* Interrupt Controller */
#define SACCIC_INTTEST0		0x1600
#define SACCIC_INTTEST1		0x1604
#define SACCIC_INTEN0		0x1608
#define SACCIC_INTEN1		0x160C
#define SACCIC_INTPOL0		0x1610
#define SACCIC_INTPOL1		0x1614
#define SACCIC_INTTSTSEL	0x1618
#define SACCIC_INTSTATCLR0	0x161C
#define SACCIC_INTSTATCLR1	0x1620
#define SACCIC_INTSET0		0x1624
#define SACCIC_INTSET1		0x1628
#define SACCIC_WAKE_EN0		0x162C
#define SACCIC_WAKE_EN1		0x1630
#define SACCIC_WAKE_POL0	0x1634
#define SACCIC_WAKE_POL1	0x1638

/* GPIO registers */
#define SACCGPIOA_DDR		0x1000		/* data direction */
#define SACCGPIOA_DVR		0x1004		/* data value */
#define SACCGPIOA_SDR		0x1008		/* sleep direction */
#define SACCGPIOA_SSR		0x100C		/* sleep state */
#define SACCGPIOB_DDR		0x1010
#define SACCGPIOB_DVR		0x1014
#define SACCGPIOB_SDR		0x1018
#define SACCGPIOB_SSR		0x101C
#define SACCGPIOC_DDR		0x1020
#define SACCGPIOC_DVR		0x1024
#define SACCGPIOC_SDR		0x1028
#define SACCGPIOC_SSR		0x102C

#define SACC_KBD0		0x0a00
#define SACC_KBD1		0x0c00

#define SACCKBD_CR		0x00
#define  KBDCR_FKC		(1<<0) /* Force MSCLK/TPCLK low */
#define  KBDCR_FKD		(1<<1) /* Force MSDATA/TPDATA low */
#define  KBDCR_ENA		(1<<3) /* Enable */
#define SACCKBD_STAT		0x04
#define  KBDSTAT_KBC		(1<<0) /* KBCLK pin value */
#define  KBDSTAT_KBD		(1<<1) /* KBDATA pin value */
#define  KBDSTAT_RXP		(1<<2) /* Parity */
#define  KBDSTAT_ENA		(1<<3) /* Enable */
#define  KBDSTAT_RXB		(1<<4) /* Rx busy */
#define  KBDSTAT_RXF		(1<<5) /* Rx full */
#define  KBDSTAT_TXB		(1<<6) /* Tx busy */
#define  KBDSTAT_TXE		(1<<7) /* Tx empty */
#define  KBDSTAT_STP		(1<<8) /* Stop bit error */
#define SACCKBD_DATA		0x08
#define SACCKBD_CLKDIV		0x0c
#define  KBDCLKDIV_DIV8		0
#define  KBDCLKDIV_DIV4		1
#define  KBDCLKDIV_DIV2		2
#define SACCKBD_CLKPRECNT	0x10
#define SACCKBD_KBDITR		0x14 /* Interrupt test */

#define SACCKBD_SIZE  		0x18

#endif /* _ARM_SA11X0_SA1111_REG_H */
