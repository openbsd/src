/*	$OpenBSD: if_mcreg.h,v 1.2 2004/12/15 06:48:24 martin Exp $	*/
/*	NetBSD: if_mcreg.h,v 1.3 2004/03/26 12:15:46 wiz Exp $	*/

/*-
 * Copyright (c) 1997 David Huang <khym@azeotrope.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 * AMD MACE (Am79C940) register definitions
 */
#define	MACE_RCVFIFO	0   /* Receive FIFO [15-00] (read only) */
#define	MACE_XMTFIFO	1   /* Transmit FIFO [15-00] (write only) */
#define	MACE_XMTFC	2   /* Transmit Frame Control (read/write) */
#define	MACE_XMTFS	3   /* Transmit Frame Status (read only) */
#define	MACE_XMTRC	4   /* Transmit Retry Count (read only) */
#define	MACE_RCVFC	5   /* Receive Frame Control (read/write) */
#define	MACE_RCVFS	6   /* Receive Frame Status (4 bytes) (read only) */
#define	MACE_FIFOFC	7   /* FIFO Frame Count (read only) */
#define	MACE_IR		8   /* Interrupt Register (read only) */
#define	MACE_IMR	9   /* Interrupt Mask Register (read/write) */
#define	MACE_PR		10  /* Poll Register (read only) */
#define	MACE_BIUCC	11  /* BIU Configuration Control (read/write) */
#define	MACE_FIFOCC	12  /* FIFO Configuration Control (read/write) */
#define	MACE_MACCC	13  /* MAC Configuration Control (read/write) */
#define	MACE_PLSCC	14  /* PLS Configuration Control (read/write) */
#define	MACE_PHYCC	15  /* PHY Confiuration Control (read/write) */
#define	MACE_CHIPIDL	16  /* Chip ID Register [07-00] (read only) */
#define	MACE_CHIPIDH	17  /* Chip ID Register [15-08] (read only) */
#define	MACE_IAC	18  /* Internal Address Configuration (read/write) */
/*	RESERVED	19     Reserved (read/write as 0) */
#define	MACE_LADRF	20  /* Logical Address Filter (8 bytes) (read/write) */
#define	MACE_PADR	21  /* Physical Address (6 bytes) (read/write) */
/*	RESERVED	22     Reserved (read/write as 0) */
/*	RESERVED	23     Reserved (read/write as 0) */
#define	MACE_MPC	24  /* Missed Packet Count (read only) */
/*	RESERVED	25     Reserved (read/write as 0) */
#define	MACE_RNTPC	26  /* Runt Packet Count (read only) */
#define	MACE_RCVCC	27  /* Receive Collision Count (read only) */
/*	RESERVED	28     Reserved (read/write as 0) */
#define	MACE_UTR	29  /* User Test Register (read/write) */
#define	MACE_RTR1	30  /* Reserved Test Register 1 (read/write as 0) */
#define	MACE_RTR2	31  /* Reserved Test Register 2 (read/write as 0) */

#define	MACE_NREGS	32

/* 2: Transmit Frame Control (XMTFC) */
#define	DRTRY		0x80	/* Disable Retry */
#define	DXMTFCS		0x08	/* Disable Transmit FCS */
#define	APADXMT		0x01	/* Auto Pad Transmit */

/* 3: Transmit Frame Status (XMTFS) */
#define	XMTSV		0x80	/* Transmit Status Valid */
#define	UFLO		0x40	/* Underflow */
#define	LCOL		0x20	/* Late Collision */
#define	MORE		0x10	/* More than one retry needed */
#define	ONE		0x08	/* Exactly one retry needed */
#define	DEFER		0x04	/* Transmission deferred */
#define	LCAR		0x02	/* Loss of Carrier */
#define	RTRY		0x01	/* Retry Error */

/* 4: Transmit Retry Count (XMTRC) */
#define	EXDEF		0x80	/* Excessive Defer */
#define	XMTRC		0x0f	/* Transmit Retry Count */

/* 5: Receive Frame Control (RCVFC) */
#define	LLRCV		0x08	/* Low Latency Receive */
#define	MR		0x04	/* Match/Reject */
#define	ASTRPRCV	0x01	/* Auto Strip Receive */

/* 6: Receive Frame Status (RCVFS) */
/* 4 byte register; read 4 times to get all of the bytes */
/* Read 1: RFS0 - Receive Message Byte Count [7-0] (RCVCNT) */

/* Read 2: RFS1 - Receive Status (RCVSTS) */
#define	OFLO		0x80	/* Overflow flag */
#define	CLSN		0x40	/* Collision flag */
#define	FRAM		0x20	/* Framing Error flag */
#define	FCS		0x10	/* FCS Error flag */
#define	RCVCNT		0x0f	/* Receive Message Byte Count [11-8] */

/* Read 3: RFS2 - Runt Packet Count (RNTPC) [7-0] */

/* Read 4: RFS3 - Receive Collision Count (RCVCC) [7-0] */

/* 7: FIFO Frame Count (FIFOFC) */
#define	RCVFC		0xf0	/* Receive Frame Count */
#define	XMTFC		0x0f	/* Transmit Frame Count */

/* 8: Interrupt Register (IR) */
#define	JAB		0x80	/* Jabber Error */
#define	BABL		0x40	/* Babble Error */
#define	CERR		0x20	/* Collision Error */
#define	RCVCCO		0x10	/* Receive Collision Count Overflow */
#define	RNTPCO		0x08	/* Runt Packet Count Overflow */
#define	MPCO		0x04	/* Missed Packet Count Overflow */
#define	RCVINT		0x02	/* Receive Interrupt */
#define	XMTINT		0x01	/* Transmit Interrupt */

/* 9: Interrut Mask Register (IMR) */
#define	JABM		0x80	/* Jabber Error Mask */
#define	BABLM		0x40	/* Babble Error Mask */
#define	CERRM		0x20	/* Collision Error Mask */
#define	RCVCCOM		0x10	/* Receive Collision Count Overflow Mask */
#define	RNTPCOM		0x08	/* Runt Packet Count Overflow Mask */
#define	MPCOM		0x04	/* Missed Packet Count Overflow Mask */
#define	RCVINTM		0x02	/* Receive Interrupt Mask */
#define	XMTINTM		0x01	/* Transmit Interrupt Mask */

/* 10: Poll Register (PR) */
#define	XMTSV		0x80	/* Transmit Status Valid */
#define	TDTREQ		0x40	/* Transmit Data Transfer Request */
#define	RDTREQ		0x20	/* Receive Data Transfer Request */

/* 11: BIU Configuration Control (BIUCC) */
#define	BSWP		0x40	/* Byte Swap */
#define	XMTSP		0x30	/* Transmit Start Point */
#define	XMTSP_4		0x00	/* 4 bytes */
#define	XMTSP_16	0x10	/* 16 bytes */
#define	XMTSP_64	0x20	/* 64 bytes */
#define	XMTSP_112	0x30	/* 112 bytes */
#define	SWRST		0x01	/* Software Reset */

/* 12: FIFO Configuration Control (FIFOCC) */
#define	XMTFW		0xc0	/* Transmit FIFO Watermark */
#define	XMTFW_8		0x00	/* 8 write cycles */
#define	XMTFW_16	0x40	/* 16 write cycles */
#define	XMTFW_32	0x80	/* 32 write cycles */
#define	RCVFW		0x30	/* Receive FIFO Watermark */
#define	RCVFW_16	0x00	/* 16 bytes */
#define	RCVFW_32	0x10	/* 32 bytes */
#define	RCVFW_64	0x20	/* 64 bytes */
#define	XMTFWU		0x08	/* Transmit FIFO Watermark Update */
#define	RCVFWU		0x04	/* Receive FIFO Watermark Update */
#define	XMTBRST		0x02	/* Transmit Burst */
#define	RCVBRST		0x01	/* Receive Burst */

/* 13: MAC Configuration (MACCC) */
#define	PROM		0x80	/* Promiscuous */
#define	DXMT2PD		0x40	/* Disable Transmit Two Part Deferral */
#define	EMBA		0x20	/* Enable Modified Back-off Algorithm */
#define	DRCVPA		0x08	/* Disable Receive Physical Address */
#define	DRCVBC		0x04	/* Disable Receive Broadcast */
#define	ENXMT		0x02	/* Enable Transmit */
#define	ENRCV		0x01	/* Enable Receive */

/* 14: PLS Configuration Control (PLSCC) */
#define	XMTSEL		0x08	/* Transmit Mode Select */
#define	PORTSEL		0x06	/* Port Select */
#define	PORTSEL_AUI	0x00	/* Select AUI */
#define	PORTSEL_10BT	0x02	/* Select 10BASE-T */
#define	PORTSEL_DAI	0x04	/* Select DAI port */
#define	PORTSEL_GPSI	0x06	/* Select GPSI */
#define	ENPLSIO		0x01	/* Enable PLS I/O */

/* 15: PHY Configuration (PHYCC) */
#define	LNKFL		0x80	/* Link Fail */
#define	DLNKTST		0x40	/* Disable Link Test */
#define	REVPOL		0x20	/* Reversed Polarity */
#define	DAPC		0x10	/* Disable Auto Polarity Correction */
#define	LRT		0x08	/* Low Receive Threshold */
#define	ASEL		0x04	/* Auto Select */
#define	RWAKE		0x02	/* Remote Wake */
#define	AWAKE		0x01	/* Auto Wake */

/* 18: Internal Address Configuration (IAC) */
#define	ADDRCHG		0x80	/* Address Change */
#define	PHYADDR		0x04	/* Physical Address Reset */
#define	LOGADDR		0x02	/* Logical Address Reset */

/* 28: User Test Register (UTR) */
#define	RTRE		0x80	/* Reserved Test Register Enable */
#define	RTRD		0x40	/* Reserved Test Register Disable */
#define	RPA		0x20	/* Run Packet Accept */
#define	FCOLL		0x10	/* Force Collision */
#define	RCVFCSE		0x08	/* Receive FCS Enable */
#define	LOOP		0x06	/* Loopback Control */
#define	LOOP_NONE	0x00	/* No Loopback */
#define	LOOP_EXT	0x02	/* External Loopback */
#define	LOOP_INT	0x04	/* Internal Loopback, excludes MENDEC */
#define	LOOP_INT_MENDEC	0x06	/* Internal Loopback, includes MENDEC */
