/*	$OpenBSD: dmareg.h,v 1.5 1999/06/23 16:47:36 deraadt Exp $	*/
/*	$NetBSD: dmareg.h,v 1.10 1996/11/28 09:37:34 pk Exp $ */

/*
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#define DMACSRBITS "\020\01INT\02ERR\03DR1\04DR2\05IEN\011WRITE\016ENCNT\017TC\032DMAON"

struct dma_regs {
	volatile u_long		csr;		/* DMA CSR */
#define  D_INT_PEND		0x00000001	/* interrupt pending */
#define  D_ERR_PEND		0x00000002	/* error pending */
#define  D_DRAINING		0x0000000c	/* fifo draining */
#define  D_INT_EN		0x00000010	/* interrupt enable */
#define  D_INVALIDATE		0x00000020	/* invalidate fifo */
#define  D_SLAVE_ERR		0x00000040	/* slave access size error */
#define  D_DRAIN		0x00000040	/* rev0,1,esc: drain fifo */
#define  D_RESET		0x00000080	/* reset scsi */
#define  D_WRITE		0x00000100	/* 1 = dev -> mem */
#define  D_EN_DMA		0x00000200	/* enable DMA requests */
#define  D_R_PEND		0x00000400	/* rev0,1: request pending */
#define  D_ESC_BURST		0x00000800	/* DMA ESC: 16 byte bursts */
#define  D_EN_CNT		0x00002000	/* enable byte counter */
#define  D_TC			0x00004000	/* terminal count */
#define  D_DSBL_CSR_DRN		0x00010000	/* disable fifo drain on csr */
#define  D_DSBL_SCSI_DRN	0x00020000	/* disable fifo drain on reg */
#define  D_BURST_SIZE		0x000c0000	/* sbus read/write burst size */
#define   D_BURST_0		0x00080000	/*   no bursts (SCSI-only) */
#define   D_BURST_16		0x00000000	/*   16-byte bursts */
#define   D_BURST_32    	0x00040000	/*   32-byte bursts */
#define  D_AUTODRAIN		0x00040000	/* DMA ESC: Auto-drain */
#define  D_DIAG			0x00100000	/* disable fifo drain on addr */
#define  D_TWO_CYCLE		0x00200000	/* 2 clocks per transfer */
#define  D_FASTER		0x00400000	/* 3 clocks per transfer */
#define	 DE_AUI_TP		0x00400000	/* 1 for TP, 0 for AUI */
#define  D_TCI_DIS		0x00800000	/* disable intr on D_TC */
#define  D_EN_NEXT		0x01000000	/* enable auto next address */
#define  D_DMA_ON		0x02000000	/* enable dma from scsi */
#define  D_A_LOADED		0x04000000	/* address loaded */
#define  D_NA_LOADED		0x08000000	/* next address loaded */
#define  D_DEV_ID		0xf0000000	/* device ID */
#define   DMAREV_0		0x00000000	/* Sunray DMA */
#define   DMAREV_ESC		0x40000000	/*  DMA ESC array */
#define   DMAREV_1		0x80000000	/* 'DMA' */
#define   DMAREV_PLUS		0x90000000	/* 'DMA+' */
#define   DMAREV_2		0xa0000000	/* 'DMA2' */
#define   DMAREV_HME		0xb0000000	/* 'HME' gate array */

	volatile caddr_t	addr;
#define DMA_D_ADDR		0x01		/* DMA ADDR (in u_longs) */

	volatile u_long		bcnt;		/* DMA COUNT (in u_longs) */
#define  D_BCNT_MASK		0x00ffffff	/* only 24 bits */

	volatile u_long		test;		/* DMA TEST (in u_longs) */
#define en_testcsr	addr			/* enet registers overlap */
#define en_cachev	bcnt
#define en_bar		test

};

/*
 * PROM-reported DMA burst sizes for the SBus
 */
#define SBUS_BURST_1	0x1
#define SBUS_BURST_2	0x2
#define SBUS_BURST_4	0x4
#define SBUS_BURST_8	0x8
#define SBUS_BURST_16	0x10
#define SBUS_BURST_32	0x20
#define SBUS_BURST_64	0x40
