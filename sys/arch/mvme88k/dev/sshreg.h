/*	$OpenBSD: sshreg.h,v 1.3 2003/02/11 19:20:26 mickey Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)sshreg.h	7.3 (Berkeley) 2/5/91
 */

/*
 * NCR 53C710 SCSI interface hardware description.
 *
 * From the Mach scsi driver for the 53C700
 */

typedef struct {
/*00*/	volatile unsigned char	ssh_sien;	/* rw: SCSI Interrupt Enable */
/*01*/	volatile unsigned char	ssh_sdid;	/* rw: SCSI Destination ID */
/*02*/	volatile unsigned char	ssh_scntl1;	/* rw: SCSI control reg 1 */
/*03*/	volatile unsigned char	ssh_scntl0;	/* rw: SCSI control reg 0 */
/*04*/	volatile unsigned char	ssh_socl;	/* rw: SCSI Output Control Latch */
/*05*/	volatile unsigned char	ssh_sodl;	/* rw: SCSI Output Data Latch */
/*06*/	volatile unsigned char	ssh_sxfer;	/* rw: SCSI Transfer reg */
/*07*/	volatile unsigned char	ssh_scid;	/* rw: SCSI Chip ID reg */
/*08*/	volatile unsigned char	ssh_sbcl;	/* ro: SCSI Bus Control Lines */
/*09*/	volatile unsigned char	ssh_sbdl;	/* ro: SCSI Bus Data Lines */
/*0a*/	volatile unsigned char	ssh_sidl;	/* ro: SCSI Input Data Latch */
/*0b*/	volatile unsigned char	ssh_sfbr;	/* ro: SCSI First Byte Received */
/*0c*/	volatile unsigned char	ssh_sstat2;	/* ro: SCSI status reg 2 */
/*0d*/	volatile unsigned char	ssh_sstat1;	/* ro: SCSI status reg 1 */
/*0e*/	volatile unsigned char	ssh_sstat0;	/* ro: SCSI status reg 0 */
/*0f*/	volatile unsigned char	ssh_dstat;	/* ro: DMA status */
/*10*/	volatile unsigned long	ssh_dsa;	/* rw: Data Structure Address */
/*14*/	volatile unsigned char	ssh_ctest3;	/* ro: Chip test register 3 */
/*15*/	volatile unsigned char	ssh_ctest2;	/* ro: Chip test register 2 */
/*16*/	volatile unsigned char	ssh_ctest1;	/* ro: Chip test register 1 */
/*17*/	volatile unsigned char	ssh_ctest0;	/* ro: Chip test register 0 */
/*18*/	volatile unsigned char	ssh_ctest7;	/* rw: Chip test register 7 */
/*19*/	volatile unsigned char	ssh_ctest6;	/* rw: Chip test register 6 */
/*1a*/	volatile unsigned char	ssh_ctest5;	/* rw: Chip test register 5 */
/*1b*/	volatile unsigned char	ssh_ctest4;	/* rw: Chip test register 4 */
/*1c*/	volatile unsigned long	ssh_temp;	/* rw: Temporary Stack reg */
/*20*/	volatile unsigned char	ssh_lcrc;	/* rw: LCRC value */
/*21*/	volatile unsigned char	ssh_ctest8;	/* rw: Chip test register 8 */
/*22*/	volatile unsigned char	ssh_istat;	/* rw: Interrupt Status reg */
/*23*/	volatile unsigned char	ssh_dfifo;	/* rw: DMA FIFO */
/*24*/	volatile unsigned char	ssh_dcmd;	/* rw: DMA Command Register */
/*25*/	volatile unsigned char	ssh_dbc2;	/* rw: DMA Byte Counter reg */
/*26*/	volatile unsigned char	ssh_dbc1;
/*27*/	volatile unsigned char	ssh_dbc0;
/*28*/	volatile unsigned long	ssh_dnad;	/* rw: DMA Next Address */
/*2c*/	volatile unsigned long	ssh_dsp;	/* rw: DMA SCRIPTS Pointer reg */
/*30*/	volatile unsigned long	ssh_dsps;	/* rw: DMA SCRIPTS Pointer Save reg */
/*34*/	volatile unsigned long	ssh_scratch;	/* rw: Scratch Register */
/*38*/	volatile unsigned char	ssh_dcntl;	/* rw: DMA Control reg */
/*39*/	volatile unsigned char	ssh_dwt;	/* rw: DMA Watchdog Timer */
/*3a*/	volatile unsigned char	ssh_dien;	/* rw: DMA Interrupt Enable */
/*3b*/	volatile unsigned char	ssh_dmode;	/* rw: DMA Mode reg */
/*3c*/	volatile unsigned long	ssh_adder;

} ssh_regmap_t;
typedef volatile ssh_regmap_t *ssh_regmap_p;

/*
 * Register defines
 */

/* Scsi control register 0 (scntl0) */

#define	SSH_SCNTL0_ARB		0xc0	/* Arbitration mode */
#	define	SSH_ARB_SIMPLE	0x00
#	define	SSH_ARB_FULL	0xc0
#define	SSH_SCNTL0_START	0x20	/* Start Sequence */
#define	SSH_SCNTL0_WATN	0x10	/* (Select) With ATN */
#define	SSH_SCNTL0_EPC		0x08	/* Enable Parity Checking */
#define	SSH_SCNTL0_EPG		0x04	/* Enable Parity Generation */
#define	SSH_SCNTL0_AAP		0x02	/* Assert ATN on Parity Error */
#define	SSH_SCNTL0_TRG		0x01	/* Target Mode */

/* Scsi control register 1 (scntl1) */

#define	SSH_SCNTL1_EXC		0x80	/* Extra Clock Cycle of data setup */
#define	SSH_SCNTL1_ADB		0x40	/* Assert Data Bus */
#define	SSH_SCNTL1_ESR		0x20	/* Enable Selection/Reselection */
#define	SSH_SCNTL1_CON		0x10	/* Connected */
#define	SSH_SCNTL1_RST		0x08	/* Assert RST */
#define	SSH_SCNTL1_AESP	0x04	/* Assert even SCSI parity */
#define	SSH_SCNTL1_RES0	0x02	/* Reserved */
#define	SSH_SCNTL1_RES1	0x01	/* Reserved */

/* Scsi interrupt enable register (sien) */

#define	SSH_SIEN_M_A		0x80	/* Phase Mismatch or ATN active */
#define	SSH_SIEN_FCMP		0x40	/* Function Complete */
#define	SSH_SIEN_STO		0x20	/* (Re)Selection timeout */
#define	SSH_SIEN_SEL		0x10	/* (Re)Selected */
#define	SSH_SIEN_SGE		0x08	/* SCSI Gross Error */
#define	SSH_SIEN_UDC		0x04	/* Unexpected Disconnect */
#define	SSH_SIEN_RST		0x02	/* RST asserted */
#define	SSH_SIEN_PAR		0x01	/* Parity Error */

/* Scsi chip ID (scid) */

#define	SSH_SCID_VALUE(i)	(1<<i)

/* Scsi transfer register (sxfer) */

#define	SSH_SXFER_DHP		0x80	/* Disable Halt on Parity error/ ATN asserted */
#define	SSH_SXFER_TP		0x70	/* Synch Transfer Period */
					/* see specs for formulas:
						Period = TCP * (4 + XFERP )
						TCP = 1 + CLK + 1..2;
					 */
#define	SSH_SXFER_MO		0x0f	/* Synch Max Offset */
#	define	SSH_MAX_OFFSET	8

/* Scsi output data latch register (sodl) */

/* Scsi output control latch register (socl) */

#define	SSH_REQ		0x80	/* SCSI signal <x> asserted */
#define	SSH_ACK		0x40
#define	SSH_BSY		0x20
#define	SSH_SEL		0x10
#define	SSH_ATN		0x08
#define	SSH_MSG		0x04
#define	SSH_CD			0x02
#define	SSH_IO			0x01

#define	SSH_PHASE(socl)	SCSI_PHASE(socl)

/* Scsi first byte received register (sfbr) */

/* Scsi input data latch register (sidl) */

/* Scsi bus data lines register (sbdl) */

/* Scsi bus control lines register (sbcl).  Same as socl */

/* DMA status register (dstat) */

#define	SSH_DSTAT_DFE		0x80	/* DMA FIFO empty */
#define	SSH_DSTAT_RES		0x40
#define	SSH_DSTAT_BF		0x20	/* Bus fault */
#define	SSH_DSTAT_ABRT		0x10	/* Aborted */
#define	SSH_DSTAT_SSI		0x08	/* SCRIPT Single Step */
#define	SSH_DSTAT_SIR		0x04	/* SCRIPT Interrupt Instruction */
#define	SSH_DSTAT_WTD		0x02	/* Watchdog Timeout Detected */
#define	SSH_DSTAT_IID		0x01	/* Invalid Instruction Detected */

/* Scsi status register 0 (sstat0) */

#define	SSH_SSTAT0_M_A		0x80	/* Phase Mismatch or ATN active */
#define	SSH_SSTAT0_FCMP	0x40	/* Function Complete */
#define	SSH_SSTAT0_STO		0x20	/* (Re)Selection timeout */
#define	SSH_SSTAT0_SEL		0x10	/* (Re)Selected */
#define	SSH_SSTAT0_SGE		0x08	/* SCSI Gross Error */
#define	SSH_SSTAT0_UDC		0x04	/* Unexpected Disconnect */
#define	SSH_SSTAT0_RST		0x02	/* RST asserted */
#define	SSH_SSTAT0_PAR		0x01	/* Parity Error */

/* Scsi status register 1 (sstat1) */

#define	SSH_SSTAT1_ILF		0x80	/* Input latch (sidl) full */
#define	SSH_SSTAT1_ORF		0x40	/* output reg (sodr) full */
#define	SSH_SSTAT1_OLF		0x20	/* output latch (sodl) full */
#define	SSH_SSTAT1_AIP		0x10	/* Arbitration in progress */
#define	SSH_SSTAT1_LOA		0x08	/* Lost arbitration */
#define	SSH_SSTAT1_WOA		0x04	/* Won arbitration */
#define	SSH_SSTAT1_RST		0x02	/* SCSI RST current value */
#define	SSH_SSTAT1_SDP		0x01	/* SCSI SDP current value */

/* Scsi status register 2 (sstat2) */

#define	SSH_SSTAT2_FF		0xf0	/* SCSI FIFO flags (bytecount) */
#	define SSH_SCSI_FIFO_DEEP	8
#define	SSH_SSTAT2_SDP		0x08	/* Latched (on REQ) SCSI SDP */
#define	SSH_SSTAT2_MSG		0x04	/* Latched SCSI phase */
#define	SSH_SSTAT2_CD		0x02
#define	SSH_SSTAT2_IO		0x01

/* Chip test register 0 (ctest0) */

#define	SSH_CTEST0_RES0	0x80
#define	SSH_CTEST0_BTD		0x40	/* Byte-to-byte Timer Disable */
#define	SSH_CTEST0_GRP		0x20	/* Generate Receive Parity for Passthrough */
#define	SSH_CTEST0_EAN		0x10	/* Enable Active Negation */
#define	SSH_CTEST0_HSC		0x08	/* Halt SCSI clock */
#define	SSH_CTEST0_ERF		0x04	/* Extend REQ/ACK Filtering */
#define	SSH_CTEST0_RES1	0x02
#define	SSH_CTEST0_DDIR	0x01	/* Xfer direction (1-> from SCSI bus) */

/* Chip test register 1 (ctest1) */

#define	SSH_CTEST1_FMT		0xf0	/* Byte empty in DMA FIFO bottom (high->byte3) */
#define	SSH_CTEST1_FFL		0x0f	/* Byte full in DMA FIFO top, same */

/* Chip test register 2 (ctest2) */

#define	SSH_CTEST2_RES		0x80
#define	SSH_CTEST2_SIGP	0x40	/* Signal process */
#define	SSH_CTEST2_SOFF	0x20	/* Synch Offset compare (1-> zero Init, max Tgt */
#define	SSH_CTEST2_SFP		0x10	/* SCSI FIFO Parity */
#define	SSH_CTEST2_DFP		0x08	/* DMA FIFO Parity */
#define	SSH_CTEST2_TEOP	0x04	/* True EOP (a-la 5380) */
#define	SSH_CTEST2_DREQ	0x02	/* DREQ status */
#define	SSH_CTEST2_DACK	0x01	/* DACK status */

/* Chip test register 3 (ctest3) read-only, top of SCSI FIFO */

/* Chip test register 4 (ctest4) */

#define	SSH_CTEST4_MUX		0x80	/* Host bus multiplex mode */
#define	SSH_CTEST4_ZMOD	0x40	/* High-impedance outputs */
#define	SSH_CTEST4_SZM		0x20	/* ditto, SCSI "outputs" */
#define	SSH_CTEST4_SLBE	0x10	/* SCSI loobpack enable */
#define	SSH_CTEST4_SFWR	0x08	/* SCSI FIFO write enable (from sodl) */
#define	SSH_CTEST4_FBL		0x07	/* DMA FIFO Byte Lane select (from ctest6)
					   4->0, .. 7->3 */

/* Chip test register 5 (ctest5) */

#define	SSH_CTEST5_ADCK	0x80	/* Clock Address Incrementor */
#define	SSH_CTEST5_BBCK	0x40	/* Clock Byte counter */
#define	SSH_CTEST5_ROFF	0x20	/* Reset SCSI offset */
#define	SSH_CTEST5_MASR	0x10	/* Master set/reset pulses (of bits 3-0) */
#define	SSH_CTEST5_DDIR	0x08	/* (re)set internal DMA direction */
#define	SSH_CTEST5_EOP		0x04	/* (re)set internal EOP */
#define	SSH_CTEST5_DREQ	0x02	/* (re)set internal REQ */
#define	SSH_CTEST5_DACK	0x01	/* (re)set internal ACK */

/* Chip test register 6 (ctest6)  DMA FIFO access */

/* Chip test register 7 (ctest7) */

#define	SSH_CTEST7_CDIS	0x80	/* Cache burst disable */
#define	SSH_CTEST7_SC1		0x40	/* Snoop control 1 */
#define	SSH_CTEST7_SC0		0x20	/* Snoop contorl 0 */
#define SSH_CTEST7_INHIBIT	(0 << 5)
#define SSH_CTEST7_SNOOP	(1 << 5)
#define SSH_CTEST7_INVAL	(2 << 5)
#define SSH_CTEST7_RESV	(3 << 5)
#define	SSH_CTEST7_STD		0x10	/* Selection timeout disable */
#define	SSH_CTEST7_DFP		0x08	/* DMA FIFO parity bit */
#define	SSH_CTEST7_EVP		0x04	/* Even parity (to host bus) */
#define	SSH_CTEST7_TT1		0x02	/* Transfer type bit */
#define	SSH_CTEST7_DIFF	0x01	/* Differential mode */

/* DMA FIFO register (dfifo) */

#define	SSH_DFIFO_RES		0x80
#define	SSH_DFIFO_BO		0x7f	/* FIFO byte offset counter */

/* Interrupt status register (istat) */

#define	SSH_ISTAT_ABRT		0x80	/* Abort operation */
#define	SSH_ISTAT_RST		0x40	/* Software reset */
#define	SSH_ISTAT_SIGP		0x20	/* Signal process */
#define	SSH_ISTAT_RES		0x10
#define	SSH_ISTAT_CON		0x08	/* Connected */
#define	SSH_ISTAT_RES1		0x04
#define	SSH_ISTAT_SIP		0x02	/* SCSI Interrupt pending */
#define	SSH_ISTAT_DIP		0x01	/* DMA Interrupt pending */

/* Chip test register 8 (ctest8) */

#define	SSH_CTEST8_V		0xf0	/* Chip revision level */
#define	SSH_CTEST8_FLF		0x08	/* Flush DMA FIFO */
#define	SSH_CTEST8_CLF		0x04	/* Clear DMA and SCSI FIFOs */
#define	SSH_CTEST8_FM		0x02	/* Fetch pin mode */
#define	SSH_CTEST8_SM		0x01	/* Snoop pins mode */

/* DMA Mode register (dmode) */

#define	SSH_DMODE_BL_MASK	0xc0	/* 0->1 1->2 2->4 3->8 */
#define	SSH_DMODE_FC		0x30	/* Function code */
#define	SSH_DMODE_PD		0x08	/* Program/data */
#define	SSH_DMODE_FAM		0x04	/* Fixed address mode */
#define	SSH_DMODE_U0		0x02	/* User programmable transfer type */
#define	SSH_DMODE_MAN		0x01	/* Manual start mode */

/* DMA interrupt enable register (dien) */

#define	SSH_DIEN_RES		0xc0
#define	SSH_DIEN_BF		0x20	/* On Bus Fault */
#define	SSH_DIEN_ABRT		0x10	/* On Abort */
#define	SSH_DIEN_SSI		0x08	/* On SCRIPTS sstep */
#define	SSH_DIEN_SIR		0x04	/* On SCRIPTS intr instruction */
#define	SSH_DIEN_WTD		0x02	/* On watchdog timeout */
#define	SSH_DIEN_IID		0x01	/* On illegal instruction detected */

/* DMA control register (dcntl) */

#define	SSH_DCNTL_CF_MASK	0xc0	/* Clock frequency dividers:
						0 --> 37.51..50.00 MHz, div=2
						1 --> 25.01..37.50 MHz, div=1.5
						2 --> 16.67..25.00 MHz, div=1
						3 --> 50.01..66.67 MHz, div=3
					 */
#define	SSH_DCNTL_EA		0x20	/* Enable ack */
#define	SSH_DCNTL_SSM		0x10	/* Single step mode */
#define	SSH_DCNTL_LLM		0x08	/* Enable SCSI Low-level mode */
#define	SSH_DCNTL_STD		0x04	/* Start DMA operation */
#define	SSH_DCNTL_FA		0x02	/* Fast arbitration */
#define	SSH_DCNTL_COM		0x01	/* 53C700 compatibility */
