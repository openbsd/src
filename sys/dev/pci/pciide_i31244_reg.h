/*	$OpenBSD: pciide_i31244_reg.h,v 1.2 2006/11/19 20:09:59 brad Exp $	*/
/*	$NetBSD: pciide_i31244_reg.h,v 1.2 2005/02/11 21:12:32 rearnsha Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
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

#ifndef _DEV_PCI_PCIIDE_I31244_REG_H_
#define	_DEV_PCI_PCIIDE_I31244_REG_H_

/*
 * Register definitions for the Intel i31244 Serial ATA Controller.
 */

/*
 * In DPA mode, the i31244 has a single 64-bit BAR.
 */
#define	ARTISEA_PCI_DPA_BASE	PCI_MAPREG_START

/*
 * Extended Control and Status Register 0
 */
#define	ARTISEA_PCI_SUECSR0	0x98
#define	SUECSR0_LED0_ONLY	(1U << 28)	/* activity on LED0 only */
#define	SUECSR0_SFSS		(1U << 16)	/* Superset Features
						   Secondary Select */

#define ARTISEA_PCI_SUDCSCR	0xa0
#define SUDCSCR_DMA_WCAE	0x02		/* Write cache align enable */
#define SUDCSCR_DMA_RCAE	0x01		/* Read cache align enable */

/*
 * DPA mode shared registers.
 */
#define	ARTISEA_SUPDIPR		0x00	/* DPA interrupt pending register */
#define	SUPDIPR_PORTSHIFT(x)	((x) * 8)
#define	SUPDIPR_PHY_CS		(1U << 0)	/* PHY change state */
#define	SUPDIPR_PHY_RDY		(1U << 1)	/* PHY ready */
#define	SUPDIPR_FIFO_ERR	(1U << 2)	/* FIFO error */
#define	SUPDIPR_ERR_RCVD	(1U << 3)	/* ERR received */
#define	SUPDIPR_U_FIS_R		(1U << 4)	/* unrecog. FIS reception */
#define	SUPDIPR_DATA_I		(1U << 5)	/* data integrity */
#define	SUPDIPR_CRC_ED		(1U << 6)	/* CRC error detected */
#define	SUPDIPR_IDE		(1U << 7)	/* IDE interrupt */

#define	ARTISEA_SUPDIMR		0x04	/* DPA interrupt mask register */
	/* See SUPDIPR bits. */

/*
 * DPA mode offset to per-port registers.
 */
#define	ARTISEA_DPA_PORT_BASE(x) (((x) + 1) * 0x200)

/*
 * DPA mode per-port registers.
 */
#define	ARTISEA_SUPDDR		0x00	/* DPA data port register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDER		0x04	/* DPA error register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDFR		0x06	/* DPA features register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDCSR		0x08	/* DPA sector count register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDSNR		0x0c	/* DPA sector number register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDCLR		0x10	/* DPA cylinder low register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDCHR		0x14	/* DPA cylinder high register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDDHR		0x18	/* DPA device/head register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDSR		0x1c	/* DPA status register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDCR		0x1d	/* DPA command register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDASR		0x28	/* DPA alt. status register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDDCTLR	0x29	/* DPA device control register */
	/* ATA/ATAPI compatible */

#define	ARTISEA_SUPDUDDTPR	0x64	/* DPA upper DMA desc. table pointer */

#define	ARTISEA_SUPDUDDPR	0x6c	/* DPA upper DMA data buffer pointer */

#define	ARTISEA_SUPDDCMDR	0x70	/* DPA DMA command register */
	/* Almost compatible with PCI IDE, but not quite. */
#define	SUPDDCMDR_START		(1U << 0)	/* start DMA transfer (c) */
#define	SUPDDCMDR_WRITE		(1U << 3)	/* write *to memory* (c) */
#define	SUPDDCMDR_DP_DMA_ACT	(1U << 8)	/* first party DMA active */
#define	SUPDDCMDR_FP_DMA_DIR	(1U << 9)	/* 1 = host->device */

#define	ARTISEA_SUPDDSR		0x72	/* DPA DMA status register */
	/* PCI IDE compatible */

#define	ARTISEA_SUPDDDTPR	0x74	/* DPA DMA desc. table pointer */

#define	ARTISEA_SUPERSET_DPA_OFF 0x100	/* offset to Superset regs: DPA mode */

#define	ARTISEA_SUPDSSSR	0x000	/* DPA SATA SStatus register */
#define	SUPDSSSR_IPM_NP		(0 << 8)	/* device not present */
#define	SUPDSSSR_IPM_ACT	(1U << 8)	/* active state */
#define	SUPDSSSR_IPM_PARTIAL	(2U << 8)	/* partial power mgmt */
#define	SUPDSSSR_IPM_SLUMBER	(6U << 8)	/* slumber power mgmt */
#define	SUPDSSSR_SPD_NP		(0 << 4)	/* device not present */
#define	SUPDSSSR_SPD_G1		(1U << 4)	/* Generation 1 speed */
#define	SUPDSSSR_DET_NP		(0 << 0)	/* device not present */
#define	SUPDSSSR_DET_PHY_CNE	(1U << 0)	/* PHY comm. not established */
#define	SUPDSSSR_DET_PHY_CE	(3U << 0)	/* PHY comm. established */
#define	SUPDSSSR_DET_PHY_LOOP	(4U << 0)	/* loopback mode */

#define	ARTISEA_SUPDSSER	0x004	/* DPA SATA SError register */
#define	SUPDSSER_DIAG_F		(1U << 25)	/* invalid FIS type */
#define	SUPDSSER_DIAG_T		(1U << 24)	/* not implemented */
#define	SUPDSSER_DIAG_S		(1U << 23)	/* not implemented */
#define	SUPDSSER_DIAG_H		(1U << 22)	/* handshake error */
#define	SUPDSSER_DIAG_C		(1U << 21)	/* CRC error */
#define	SUPDSSER_DIAG_D		(1U << 20)	/* disparity error */
#define	SUPDSSER_DIAG_B		(1U << 19)	/* not implemented */
#define	SUPDSSER_DIAG_W		(1U << 18)	/* comm wake */
#define	SUPDSSER_DIAG_I		(1U << 17)	/* not implemented */
#define	SUPDSSER_DIAG_N		(1U << 16)	/* PHY RDY state change */
#define	SUPDSSER_ERR_E		(1U << 11)	/* internal error */
#define	SUPDSSER_ERR_P		(1U << 10)	/* protocol error */
#define	SUPDSSER_ERR_C		(1U << 9)	/* non-recovered comm. */
#define	SUPDSSER_ERR_T		(1U << 8)	/* non-recovered TDIE */
#define	SUPDSSER_ERR_M		(1U << 1)	/* recovered comm. */
#define	SUPDSSER_ERR_I		(1U << 0)	/* not implemented */

#define	ARTISEA_SUPDSSCR	0x008	/* DPA SATA SControl register */
#define	SUPDSSCR_IPM_ANY	(0 << 8)	/* no IPM mode restrictions */
#define	SUPDSSCR_IPM_NO_PARTIAL	(1U << 8)	/* no PARTIAL mode */
#define	SUPDSSCR_IPM_NO_SLUMBER	(2U << 8)	/* no SLUMBER mode */
#define	SUPDSSCR_IPM_NONE	(3U << 8)	/* no PM allowed */
#define	SUPDSSCR_SPD_ANY	(0 << 4)	/* no speed restrictions */
#define	SUPDSSCR_SPD_G1		(1U << 4)	/* <= Generation 1 */
#define	SUPDSSCR_DET_NORM	(0 << 0)	/* normal operation */
#define	SUPDSSCR_DET_INIT	(1U << 0)	/* comm. init */
#define	SUPDSSCR_DET_DISABLE	(4U << 0)	/* disable interface */

#define	ARTISEA_SUPDSDBR	0x00c	/* DPA Set Device Bits register */

#define	ARTISEA_SUPDPFR		0x040	/* DPA PHY feature register */
#define	SUPDPFR_SSCEN		(1U << 16)	/* SSC enable */
#define	SUPDPFR_FVS		(1U << 14)	/* full voltage swing */

#define	ARTISEA_SUPDBFCSR	0x044	/* DPA BIST FIS ctrl/stat register */
#define	SUPDBFCSR_PAT_D21_5	(0 << 30)	/* D21.5s */
#define	SUPDBFCSR_PAT_D24_3	(1U << 30)	/* D24.3s */
#define	SUPDBFCSR_PAT_D10_2	(2U << 30)	/* D10.2 / K28.5 */
#define	SUPDBFCSR_PAT_COUNT	(3U << 30)	/* counting */
#define	SUPDBFCSR_CS_D21_5	(0 << 28)
#define	SUPDBFCSR_CS_D24_3	(1U << 28)
#define	SUPDBFCSR_CS_D10_2	(2U << 28)
#define	SUPDBFCSR_CS_COUNT	(3U << 30)
#define	SUPDBFCSR_CLEAR_ERRS	(1U << 25)	/* clear errors/frames */
#define	SUPDBFCSR_CE		(1U << 24)	/* BIST check enable */
#define	SUPDBFCSR_PE		(1U << 23)	/* BIST pattern enable */
#define	SUPDBFCSR_K28_5		((1U << 16) |				\
				 (1U << 8)	/* send K28.5s */
#define	SUPDBFCSR_BIST_ACT_RX	(1U << 15)	/* BIST Act. FIS was rx'd */
#define	SUPDBFCSR_BIST_ACT_RX_TO (1U << 14)	/* ...with transmit-only */
#define	SUPDBFCSR_BIST_ACT_RX_AB (1U << 13)	/* ...with align-bypass */
#define	SUPDBFCSR_BIST_ACT_RX_SB (1U << 12)	/* ...with scrambling-bypass */
#define	SUPDBFCSR_BIST_ACT_RX_RT (1U << 11)	/* ...with retimed */
#define	SUPDBFCSR_BIST_ACT_RX_P  (1U << 10)	/* ...with primitive */
#define	SUPDBFCSR_BIST_ACT_RX_AFEL (1U << 9)	/* ...with AFE loopback */
#define	SUPDBFCSR_BIST_ACT_TX	(1U << 7)	/* send BIST Act. FIS */
#define	SUPDBFCSR_BIST_ACT_TX_TO (1U << 6)	/* ...with transmit-only */
#define	SUPDBFCSR_BIST_ACT_TX_AB (1U << 5)	/* ...with align-bypass */
#define	SUPDBFCSR_BIST_ACT_TX_SB (1U << 4)	/* ...with scrambling-bypass */
#define	SUPDBFCSR_BIST_ACT_TX_RT (1U << 3)	/* ...with retimed */
#define	SUPDBFCSR_BIST_ACT_TX_P  (1U << 2)	/* ...with primitive */
#define	SUPDBFCSR_BIST_ACT_TX_AFEL (1U << 1)	/* ...with AFE loopback */
#define	SUPDBFCSR_INIT_NE_TO	(1U << 0)	/* init. near-end tx-only */

#define	ARTISEA_SUPDBER		0x048	/* DPA BIST errors register */

#define	ARTISEA_SUPDBFR		0x04c	/* DPA BIST frames register */

#define	ARTISEA_SUPDHBDLR	0x050	/* DPA Host BIST data low register */

#define	ARTISEA_SUPDHBDHR	0x054	/* DPA Host BIST data high register */

#define	ARTISEA_SUPDDBDLR	0x058	/* DPA Device BIST data low */

#define	ARTISEA_SUPDDBDHR	0x05c	/* DPA Device BIST data high */

#define	ARTISEA_SUPDDSFCSR	0x068	/* DPA DMA setup FIS ctrl/stat */
#define	SUPDDSFCSR_DIR		(1U << 31)	/* First Party setup FIS
						   word 0 direction bit
						   (1 == tx -> rx) */
#define	SUPDDSFCSR_INTR		(1U << 30)	/* rcvd's First Party setup
						   FIS with I bit set */
#define	SUPDDSFCSR_START_SETUP	(1U << 28)	/* send DMA setup FIS */
#define	SUPDDSFCSR_EN_FP_AP	(1U << 27)	/* enab. FP DMA auto-process */
#define	SUPDDSFCSR_ABORT_TSM	(1U << 24)	/* abort xport/link SMs */

#define	ARTISEA_SUPDHDBILR	0x06c	/* DPA Host DMA Buff. Id low */

#define	ARTISEA_SUPDHDBIHR	0x070	/* DPA Host DMA Buff. Id high */

#define	ARTISEA_SUPDHRDR0	0x074	/* DPA Host Resvd. DWORD 0 */

#define	ARTISEA_SUPDHDBOR	0x078	/* DPA Host DMA Buff. offset */

#define	ARTISEA_SUPDHDTCR	0x07c	/* DPA Host DMA xfer count */

#define	ARTISEA_SUPDHRDR1	0x080	/* DPA Host Resvd. DWORD 1 */

#define	ARTISEA_SUPDDDBILR	0x084	/* DPA Device DMA Buff. Id low */

#define	ARTISEA_SUPDDDBIHR	0x088	/* DPA Device DMA Buff. Id high */

#define	ARTISEA_SUPDDRDR0	0x08c	/* DPA Device Resvd. DWORD 0 */

#define	ARTISEA_SUPDDDBOR	0x090	/* DPA Device DMA Buff. offset */

#define	ARTISEA_SUPDDTCR	0x094	/* DPA Device DMA xfer count */

#define	ARTISEA_SUPDDRDR1	0x09c	/* DPA Device Resvd. DWORD 1 */

#endif /* _DEV_PCI_PCIIDE_I31244_REG_H_ */
