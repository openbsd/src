/*	$NetBSD: scsi96reg.h,v 1.5 1996/05/05 06:18:02 briggs Exp $	*/

/*
 * Copyright (C) 1994	Allen K. Briggs
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SCSI96REG_MACHINE_
#define _SCSI96REG_MACHINE_

typedef volatile unsigned char	v_uchar;

#define PAD(x)	u_char	x [15];
struct ncr53c96regs {
	v_uchar	tcreg_lsb;	/* r == ctc, w == stc */
	PAD(pad0);
	v_uchar	tcreg_msb;	/* r == ctc, w == stc */
	PAD(pad1);
	v_uchar	fifo;		/* fifo reg */
	PAD(pad2);
	v_uchar	cmdreg;		/* command reg */
	PAD(pad3);
	v_uchar	statreg;	/* status reg */
#define sdidreg	statreg
	PAD(pad4);
	v_uchar	instreg;	/* interrupt status reg */
#define stimreg	instreg
	PAD(pad5);
	v_uchar	isreg;		/* internal state reg */
	PAD(pad6);
	v_uchar	fifostatereg;	/* fifo state reg */
	PAD(pad7);
	v_uchar	ctrlreg1;	/* control register 1 */
	PAD(pad8);
	v_uchar	clkfactorreg;	/* clock factor register */
	PAD(pad9);
	v_uchar	ftmreg;		/* forced test mode register */
	PAD(pad10);
	v_uchar	ctrlreg2;	/* control register 2 */
	PAD(pad11);
	v_uchar	ctrlreg3;	/* control register 3 */
	PAD(pad12);
	v_uchar	unused1;	/* unknown */
	PAD(pad13);
	v_uchar	unused2;	/* unknown */
	PAD(pad14);
	v_uchar	dareg;		/* data alignment register */
	PAD(pad15);
};
#undef PAD

#define	NCR96_CTCREG	0x0	/* Current transfer count.	R   */
				/* 16 bits, LSB first. */
#define NCR96_STCREG	0x0	/* Short transfer count.	W   */
				/* 16 bits, LSB first. */
#define NCR96_FFREG	0x2	/* FIFO register.		R/W */

#define NCR96_CMDREG	0x3	/* Command register.		R/W */
#define   NCR96_DMA	0x80	/* This flag means to use DMA mode. */
/* Initiator Commands */
#define   NCR96_CMD_INFOXFER	0x10	/* Information Transfer. */
#define   NCR96_CMD_ICCS	0x11	/* Initiator Cmd Complete steps.  */
#define   NCR96_CMD_MSGACC	0x12	/* Message Accepted. */
#define   NCR96_CMD_TPB		0x18	/* Transfer pad bytes. */
#define   NCR96_CMD_SETATN	0x1A	/* Set ATN */
#define   NCR96_CMD_RESETATN	0x1B	/* Reset ATN */
/* Target Commands -- skipped. */
/* Idle State Commands. */
#define   NCR96_CMD_RESEL	0x40	/* Reselect steps */
#define   NCR96_CMD_SEL		0x41	/* Select without ATN steps */
#define   NCR96_CMD_SELATN	0x42	/* Select with ATN steps */
#define   NCR96_CMD_SELATNS	0x43	/* Select with ATN and stop steps */
#define   NCR96_CMD_ENSEL	0x44	/* Enable selection/reselection */
#define   NCR96_CMD_DISSEL	0x45	/* Disable selection/reselection */
#define   NCR96_CMD_SELATN3	0x46	/* Select with ATN3 */
/* General Commands. */
#define   NCR96_CMD_NOOP	0x00	/* No Operation */
#define   NCR96_CMD_CLRFIFO	0x01	/* Clear FIFO */
#define   NCR96_CMD_RESETDEV	0x02	/* Reset Device */
#define   NCR96_CMD_RESETBUS	0x03	/* Reset SCSI Bus */

#define NCR96_STATREG	0x4	/* Status register.		R   */
#define   NCR96_STAT_INT	0x80	/* Interrupt */
#define   NCR96_STAT_IOE	0x40	/* Illegal Operation Error */
#define   NCR96_STAT_PE		0x20	/* Parity Error */
#define   NCR96_STAT_CTZ	0x10	/* Count To Zero */
#define   NCR96_STAT_GCV	0x08	/* Group Code Valid */
#define   NCR96_STAT_PHASE	0x07	/* Mask for SCSI Phase */
#define   NCR96_STAT_MSG	0x04	/* Message */
#define   NCR96_STAT_CD		0x02	/* Command/Data */
#define   NCR96_STAT_IO		0x01	/* Input/Output */

#define NCR96_SDIDREG	0x4	/* SCSI Dest. ID register.	W   */
#define   NCR96_SDID_MASK	0x07	/* Mask for Dest. ID */

#define NCR96_INSTREG	0x5	/* Interrupt status register.	R   */
#define   NCR96_ISR_SRST	0x80	/* SCSI Reset */
#define   NCR96_ISR_INVAL	0x40	/* Invalid Command */
#define   NCR96_ISR_DISCONN	0x20	/* Disconnected */
#define   NCR96_ISR_SREQ	0x10	/* Service Request */
#define   NCR96_ISR_SO		0x08	/* Successful Operation */
#define   NCR96_ISR_RESEL	0x04	/* Relected */
#define   NCR96_ISR_SELATN	0x02	/* Selected with ATN */
#define   NCR96_ISR_SEL		0x01	/* Selected */

#define NCR96_STIMREG	0x5	/* SCSI Timeout register.	W   */

#define NCR96_ISREG	0x6	/* Internal state register.	R   */
#define   NCR96_IS_MASK		0x0f	/* Mask for non-reserved fields.  */

#define NCR96_STPREG	0x6	/* Synch. Trans. per. register.	W   */
#define   NCR96_STP_MASK	0x1f	/* Mask for non-reserved fields.  */

#define NCR96_CFISREG	0x7	/* Current FIFO/i.s. register.	R   */
#define   NCR96_CF_MASK		0x1f	/* Mask for current FIFO count. */

#define NCR96_SOFREG	0x7	/* Synch. Offset register.	W   */
#define   NCR96_SOF_MASK	0x0f	/* Mask for non-reserved fields. */

#define NCR96_CNTLREG1	0x8	/* Control register one.	R/W */
#define   NCR96_C1_ETM		0x80	/* Extended Timing mode */
#define   NCR96_C1_DISR		0x40	/* Disable interrupt on SCSI Reset */
#define   NCR96_C1_PTE		0x20	/* Parity Test Enable */
#define   NCR96_C1_PERE		0x10	/* Parity Error Reporting Enable */
#define   NCR96_C1_STE		0x08	/* Self Test Enable */
#define   NCR96_C1_SCSIID_MSK	0x07	/* Chip SCSI ID Mask */

#define NCR96_CLKFREG	0x9	/* Clock Factor register.	W   */
#define   NCR96_CLKF_MASK	0x07	/* Mask for non-reserved fields */

#define NCR96_FTMREG	0xA	/* Forced Test Mode register.	W   */
#define   NCR96_FTM_MASK	0x07	/* Mask for non-reserved fields */

#define NCR96_CNTLREG2	0xB	/* Control register two.	R/W */
#define   NCR96_C2_DAE		0x80	/* Data alignment enable */
#define   NCR96_C2_LSP		0x40	/* Latch SCSI Phase */
#define   NCR96_C2_SBO		0x20	/* Select Byte Order */
#define   NCR96_C2_TSDR		0x10	/* Tri-state DMA request */
#define   NCR96_C2_S2FE		0x08	/* SCSI-2 Features Enable */
#define   NCR96_C2_ACDPE	0x04	/* Abort on Cmd/Data parity error */
#define   NCR96_C2_PGRP		0x02	/* Pass through/gen register parity */
#define   NCR96_C2_PGDP		0x01	/* Pass through/gen data parity */

#define NCR96_CNTLREG3	0xC	/* Control register three.	R/W */
#define   NCR96_C3_LBTM		0x04	/* Last byte transfer mode */
#define   NCR96_C3_MDM		0x02	/* Modity DMA mode */
#define   NCR96_C3_BS8		0x01	/* Burst Size 8 */

#define NCR96_DALREG	0xF	/* Data alignment register.	W   */

#endif /* _SCSI96REG_MACHINE_ */
