/*	$OpenBSD: if_esreg.h,v 1.4 2002/12/09 00:45:37 millert Exp $	*/
/*	$NetBSD: if_esreg.h,v 1.4 1996/05/01 15:51:08 mhitch Exp $	*/

/*
 * Copyright (c) 1995 Michael L. Hitch
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
 *      This product includes software developed by Michael L. Hitch.
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
 */

/*
 * SMC 91C90 register definitions
 */

union smcregs {
	struct {
		volatile u_short tcr;	/* Transmit Control Register */
		volatile u_short ephsr;	/* EPH Status Register */
		volatile u_short rcr;	/* Receive Control Register */
		volatile u_short ecr;	/* Counter Register */
		volatile u_short mir;	/* Memory Information Register */
		volatile u_short mcr;	/* Memory Configuration Register */
		volatile u_short resv;
		volatile u_short bsr;	/* Bank Select Register */
	} b0;
	struct {
		volatile u_short cr;	/* Configuration Register */
		volatile u_short bar;	/* Base Address Register */
		volatile u_short iar[3]; /* Individual Address Registers */
		volatile u_short gpr;	/* General Purpose Register */
		volatile u_short ctr;	/* Control Register */
		volatile u_short bsr;	/* Bank Select Register */
	} b1;
	struct {
		volatile u_short mmucr;	/* MMU Command Register */
		volatile u_char pnr;	/* Packet Number Register */
		volatile u_char arr;	/* Allocation Result Register */
		volatile u_short fifo;	/* FIFO Ports Register */
		volatile u_short ptr;	/* Pointer Register */
		volatile u_short data;	/* Data Register */
		volatile u_short datax;	/* Data Register (2nd mapping) */
		volatile u_char ist;	/* Interrupt Status Register */
		volatile u_char msk;	/* Interrupt Mask Register */
		volatile u_short bsr;	/* Bank Select Register */
	} b2;
	struct {
		volatile u_short mt[4];	/* Multicast Table */
		volatile u_short resv[3];
		volatile u_short bsr;	/* Bank Select Register */
	} b3;
/*
 * Bank 2 registers defined as u_short fields
 */
	struct {
		volatile u_short mmucr;	/* MMU Command Register */
		volatile u_short pnrarr;/* Packet Number/Allocation Result */
		volatile u_short fifo;	/* FIFO Ports Register */
		volatile u_short ptr;	/* Pointer Register */
		volatile u_short data;	/* Data Register */
		volatile u_short datax;	/* Data Register (2nd mapping) */
		volatile u_short istmsk;/* Interrupt Status/Mask Register */
		volatile u_short bsr;	/* Bank Select Register */
	} w2;
};

/* Transmit Control Register */
#define	TCR_PAD_EN	0x8000		/* Pad short frames */
#define	TCR_TXENA	0x0100		/* Transmit enabled */
#define	TCR_MON_CSN	0x0004		/* Monitor carrier */

/* EPH Status Register */
#define	EPHSR_16COL	0x1000		/* 16 collisions reached */
#define	EPHSR_MULCOL	0x0400		/* Multiple collsions */
#define	EPHSR_TX_SUC	0x0100		/* Last transmit successful */
#define	EPHSR_LOST_CAR	0x0004		/* Lost carrier */

/* Receive Control Register */
#define	RCR_ALLMUL	0x0400		/* Accept all Multicast frames */
#define	RCR_PRMS	0x0200		/* Promiscuous mode */
#define	RCR_EPH_RST	0x0080		/* Software activated Reset */
#define	RCR_FILT_CAR	0x0040		/* Filter carrier */
#define	RCR_STRIP_CRC	0x0002		/* Strip CRC */
#define	RCR_RXEN	0x0001		/* Receiver enabled */

/* Counter Register */
#define	ECR_MCC		0xf000		/* Multiple collision count */
#define	ECR_SCC		0x0f00		/* Single collision count */
#define	ECR_EDTX	0x00f0		/* Excess deferred TX count */
#define	ECR_DTX		0x000f		/* Deferred TX count */

/* Configuration Register */
#define	CR_RAM32K	0x2000		/* 32Kx16 RAM */
#define	CR_NO_WAIT_ST	0x0010		/* No wait state */
#define	CR_SET_SQLCH	0x0002		/* Squelch level 240mv */

/* Control Register */
#define	CTR_AUTO_RLSE	0x0008		/* Auto Release */

/* MMU Command Register */
#define	MMUCR_NOOP	0x0000		/* No operation */
#define	MMUCR_ALLOC	0x2000		/* Allocate memory for TX */
#define	MMUCR_RESET	0x4000		/* Reset to intitial state */
#define	MMUCR_REM_RX	0x6000		/* Remove frame from top of RX FIFO */
#define	MMUCR_REMRLS_RX	0x8000		/* Remove & release from top of RX FIFO */
#define	MMUCR_RLSPKT	0xa000		/* Release specific packet */
#define	MMUCR_ENQ_TX	0xc000		/* Enqueue packet into TX FIFO */
#define	MMUCR_RESET_TX	0xe000		/* Reset TX FIFOs */
#define	MMUCR_BUSY	0x0100		/* MMU busy */

/* Allocation Result Register */
#define	ARR_FAILED	0x80		/* Allocation failed */
#define	ARR_APN		0x1f		/* Allocated packet number */

/* FIFO Ports Register */
#define	FIFO_TEMPTY	0x8000		/* TX queue empty */
#define	FIFO_TXPNR	0x1f00		/* TX done packet number */
#define	FIFO_REMPTY	0x0080		/* RX FIFO empty */
#define	FIFO_RXPNR	0x001f		/* RX FIFO packet number */

/* Pointer Register */
#define	PTR_RCV		0x0080		/* Use Receive area */
#define	PTR_AUTOINCR	0x0040		/* Auto increment pointer on access */
#define	PTR_READ	0x0020		/* Read access */

/* Interrupt Status Register */
#define	IST_EPHINT	0x20		/* EPH Interrupt */
#define	IST_RX_OVRN	0x10		/* RX Overrun */
#define	IST_ALLOC	0x08		/* MMU Allocation completed */
#define	IST_TX_EMPTY	0x04		/* TX FIFO empty */
#define	IST_TX		0x02		/* TX complete */
#define	IST_RX		0x01		/* RX complete */

/* Interrupt Acknowlege Register */
#define	ACK_RX_OVRN	IST_RX_OVRN
#define	ACK_TX_EMPTY	IST_TX_EMPTY
#define	ACK_TX		IST_TX

/* Interrupt Mask Register */
#define	MSK_EPHINT	0x20		/* EPH Interrupt */
#define	MSK_RX_OVRN	0x10		/* RX Overrun */
#define	MSK_ALLOC	0x08		/* MMU Allocation completed */
#define	MSK_TX_EMPTY	0x04		/* TX FIFO empty */
#define	MSK_TX		0x02		/* TX complete */
#define	MSK_RX		0x01		/* RX complete */

/* Bank Select Register */
#define	BSR_MASK	0x0300
#define	BSR_BANK0	0x0000		/* Select bank 0 */
#define	BSR_BANK1	0x0100		/* Select bank 1 */
#define	BSR_BANK2	0x0200		/* Select bank 2 */
#define	BSR_BANK3	0x0300		/* Select bank 3 */

/* Packet Receive Frame Status Word */
#define	RFSW_ALGNERR	0x8000		/* Alignment Error */
#define	RFSW_BRDCST	0x4000		/* Broadcast frame */
#define	RFSW_BADCRC	0x2000		/* Bad CRC */
#define	RFSW_ODDFRM	0x1000		/* Odd number of bytes in frame */
#define	RFSW_TOOLNG	0x0800		/* Frame was too long */
#define	RFSW_TOOSHORT	0x0400		/* Frame was too short */
#define	RFSW_HASH	0x007e		/* Multicast hash value */
#define	RFSW_MULTCAST	0x0001		/* Multicast frame */

/* Control byte */
#define	CTLB_ODD	0x20		/* Odd number of bytes in frame */
#define	CTLB_CRC	0x10		/* Append CRC to transmitted frame */
