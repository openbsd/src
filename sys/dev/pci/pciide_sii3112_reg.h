/*	$OpenBSD: pciide_sii3112_reg.h,v 1.1 2003/05/17 18:45:15 grange Exp $	*/
/*	$NetBSD: pciide_sii3112_reg.h,v 1.1 2003/03/20 04:22:50 thorpej Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_PCI_PCIIDE_SII3112_REG_H_
#define	_DEV_PCI_PCIIDE_SII3112_REG_H_

/*
 * PCI configuration space registers.
 */

#define	SII3112_PCI_CFGCTL	0x40
#define	CFGCTL_CFGWREN		(1U << 0)	/* enable cfg writes */
#define	CFGCTL_BA5INDEN		(1U << 1)	/* BA5 indirect access enable */

#define	SII3112_PCI_SWDATA	0x44

#define	SII3112_PCI_BM_IDE0	0x70
	/* == BAR4+0x00 */

#define	SII3112_PCI_PRD_IDE0	0x74
	/* == BAR4+0x04 */

#define	SII3112_PCI_BM_IDE1	0x78
	/* == BAR4+0x08 */

#define	SII3112_PCI_PRD_IDE1	0x7c
	/* == BAR4+0x0c */

#define	SII3112_DTM_IDE0	0x80	/* Data Transfer Mode - IDE0 */
#define	SII3112_DTM_IDE1	0x84	/* Data Transfer Mode - IDE1 */
#define	DTM_IDEx_PIO		0x00000000	/* PCI DMA, IDE PIO (or 1) */
#define	DTM_IDEx_DMA		0x00000002	/* PCI DMA, IDE DMA (or 3) */


#define	SII3112_SCS_CMD		0x88	/* System Config Status */
#define	SCS_CMD_PBM_RESET	(1U << 0)	/* PBM module reset */
#define	SCS_CMD_ARB_RESET	(1U << 1)	/* ARB module reset */
#define	SCS_CMD_FF1_RESET	(1U << 4)	/* IDE1 FIFO reset */
#define	SCS_CMD_FF0_RESET	(1U << 5)	/* IDE0 FIFO reset */
#define	SCS_CMD_IDE1_RESET	(1U << 6)	/* IDE1 module reset */
#define	SCS_CMD_IDE0_RESET	(1U << 7)	/* IDE0 module reset */
#define	SCS_CMD_BA5_EN		(1U << 16)	/* BA5 is enabled */
#define	SCS_CMD_IDE0_INT_BLOCK	(1U << 22)	/* IDE0 interrupt block */
#define	SCS_CMD_IDE1_INT_BLOCK	(1U << 23)	/* IDE1 interrupt block */

#define	SII3112_SSDR		0x8c	/* System SW Data Register */

#define	SII3112_FMA_CSR		0x90	/* Flash Memory Addr - CSR */

#define	SII3112_FM_DATA		0x94	/* Flash Memory Data */

#define	SII3112_EEA_CSR		0x98	/* EEPROM Memory Addr - CSR */

#define	SII3112_EE_DATA		0x9c	/* EEPROM Data */

#define	SII3112_TCS_IDE0	0xa0	/* IDEx config, status */
#define	SII3112_TCS_IDE1	0xb0
#define	TCS_IDEx_BCA		(1U << 1)	/* buffered command active */
#define	TCS_IDEx_CH_RESET	(1U << 2)	/* channel reset */
#define	TCS_IDEx_VDMA_INT	(1U << 10)	/* virtual DMA interrupt */
#define	TCS_IDEx_INT		(1U << 11)	/* interrupt status */
#define	TCS_IDEx_WTT		(1U << 12)	/* watchdog timer timeout */
#define	TCS_IDEx_WTEN		(1U << 13)	/* watchdog timer enable */
#define	TCS_IDEx_WTINTEN	(1U << 14)	/* watchdog timer int. enable */

#define	SII3112_BA5_IND_ADDR	0xc0	/* BA5 indirect address */

#define	SII3112_BA5_IND_DATA	0xc4	/* BA5 indirect data */

#endif /* _DEV_PCI_PCIIDE_SII3112_REG_H_ */
