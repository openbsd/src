/*	$NetBSD: espreg.h,v 1.5 1995/01/07 05:17:15 mycroft Exp $ */

/*
 * Copyright (c) 1994 Peter Galbavy.  All rights reserved.
 * Copyright (c) 1995 Theo de Raadt.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy and
 *	Theo de Raadt
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

/*
 * Register addresses, relative to some base address
 */

struct espregs {
	volatile u_char	espr_tcl;
	volatile u_char	x01;
	volatile u_char	x02;
	volatile u_char	x03;
	volatile u_char	espr_tcm;
	volatile u_char	x05;
	volatile u_char	x06;
	volatile u_char	x07;
	volatile u_char	espr_fifo;
	volatile u_char	x09;
	volatile u_char	x0a;
	volatile u_char	x0b;
	volatile u_char	espr_cmd;
	volatile u_char	x0d;
	volatile u_char	x0e;
	volatile u_char	x0f;
	volatile u_char	espr_stat;
#define espr_selid espr_stat
	volatile u_char	x11;
	volatile u_char	x12;
	volatile u_char	x13;
	volatile u_char	espr_intr;
#define espr_timeout espr_intr
	volatile u_char	x15;
	volatile u_char	x16;
	volatile u_char	x17;
	volatile u_char	espr_step;
#define espr_synctp espr_step
	volatile u_char	x19;
	volatile u_char	x1a;
	volatile u_char	x1b;
	volatile u_char	espr_fflag;
#define espr_syncoff espr_fflag
	volatile u_char	x1d;
	volatile u_char	x1e;
	volatile u_char	x1f;
	volatile u_char	espr_cfg1;
	volatile u_char	x21;
	volatile u_char	x22;
	volatile u_char	x23;
	volatile u_char	espr_ccf;
	volatile u_char	x25;
	volatile u_char	x26;
	volatile u_char	x27;
	volatile u_char	espr_test;
	volatile u_char	x29;
	volatile u_char	x2a;
	volatile u_char	x2b;
	volatile u_char	espr_cfg2;
	volatile u_char	x2d;
	volatile u_char	x2e;
	volatile u_char	x2f;
	volatile u_char	espr_cfg3;		/* ESP200 only */
	volatile u_char	x31;
	volatile u_char	x32;
	volatile u_char	x33;
	volatile u_char	espr_tch;		/* ESP200 only */
};

#define ESPCMD_DMA	0x80		/* DMA Bit */
#define ESPCMD_NOP	0x00		/* No Operation */
#define ESPCMD_FLUSH	0x01		/* Flush FIFO */
#define ESPCMD_RSTCHIP	0x02		/* Reset Chip */
#define ESPCMD_RSTSCSI	0x03		/* Reset SCSI Bus */
#define ESPCMD_RESEL	0x40		/* Reselect Sequence */
#define ESPCMD_SELNATN	0x41		/* Select without ATN */
#define ESPCMD_SELATN	0x42		/* Select with ATN */
#define ESPCMD_SELATNS	0x43		/* Select with ATN & Stop */
#define ESPCMD_ENSEL	0x44		/* Enable (Re)Selection */
#define ESPCMD_DISSEL	0x45		/* Disable (Re)Selection */
#define ESPCMD_SELATN3	0x46		/* Select with ATN3 */
#define ESPCMD_RESEL3	0x47		/* Reselect3 Sequence */
#define ESPCMD_SNDMSG	0x20		/* Send Message */
#define ESPCMD_SNDSTAT	0x21		/* Send Status */
#define ESPCMD_SNDDATA	0x22		/* Send Data */
#define ESPCMD_DISCSEQ	0x23		/* Disconnect Sequence */
#define ESPCMD_TERMSEQ	0x24		/* Terminate Sequence */
#define ESPCMD_TCCS	0x25		/* Target Command Comp Seq */
#define ESPCMD_DISC	0x27		/* Disconnect */
#define ESPCMD_RECMSG	0x28		/* Receive Message */
#define ESPCMD_RECCMD	0x29		/* Receive Command  */
#define ESPCMD_RECDATA	0x2a		/* Receive Data */
#define ESPCMD_RECCSEQ	0x2b		/* Receive Command Sequence*/
#define ESPCMD_ABORT	0x04		/* Target Abort DMA */
#define ESPCMD_TRANS	0x10		/* Transfer Information */
#define ESPCMD_ICCS	0x11		/* Initiator Cmd Comp Seq  */
#define ESPCMD_MSGOK	0x12		/* Message Accepted */
#define ESPCMD_TRPAD	0x18		/* Transfer Pad */
#define ESPCMD_SETATN	0x1a		/* Set ATN */
#define ESPCMD_RSTATN	0x1b		/* Reset ATN */

#define ESPSTAT_INT	0x80		/* Interrupt */
#define ESPSTAT_GE	0x40		/* Gross Error */
#define ESPSTAT_PE	0x20		/* Parity Error */
#define ESPSTAT_TC	0x10		/* Terminal Count */
#define ESPSTAT_VGC	0x08		/* Valid Group Code */
#define ESPSTAT_PHASE	0x07		/* Phase bits */

#define ESPINTR_SBR	0x80		/* SCSI Bus Reset */
#define ESPINTR_ILL	0x40		/* Illegal Command */
#define ESPINTR_DIS	0x20		/* Disconnect */
#define ESPINTR_BS	0x10		/* Bus Service */
#define ESPINTR_FC	0x08		/* Function Complete */
#define ESPINTR_RESEL	0x04		/* Reselected */
#define ESPINTR_SELATN	0x02		/* Select with ATN */
#define ESPINTR_SEL	0x01		/* Selected */

#define ESPSTEP_MASK	0x07		/* the last 3 bits */
#define ESPSTEP_DONE	0x04		/* command went out */

#define ESPFIFO_SS	0xe0		/* Sequence Step (Dup) */
#define ESPFIFO_FF	0x1f		/* Bytes in FIFO */

#define ESPCFG1_SLOW	0x80		/* Slow Cable Mode */
#define ESPCFG1_SRR	0x40		/* SCSI Reset Rep Int Dis */
#define ESPCFG1_PTEST	0x20		/* Parity Test Mod */
#define ESPCFG1_PARENB	0x10		/* Enable Parity Check */
#define ESPCFG1_CTEST	0x08		/* Enable Chip Test */
#define ESPCFG1_BUSID	0x07		/* Bus ID */

#define ESPCFG2_RSVD	0xe0		/* reserved */
#define ESPCFG2_FE	0x40		/* Features Enable */
#define ESPCFG2_DREQ	0x10		/* DREQ High Impedance */
#define ESPCFG2_SCSI2	0x08		/* SCSI-2 Enable */
#define ESPCFG2_BPA	0x04		/* Target Bad Parity Abort */
#define ESPCFG2_RPE	0x02		/* Register Parity Error */
#define ESPCFG2_DPE	0x01		/* DMA Parity Error */

#define ESPCFG3_IDM	0x10		/* ID Message Res Check */
#define ESPCFG3_QTE	0x08		/* Queue Tag Enable */
#define ESPCFG3_CDB	0x04		/* CDB 10-bytes OK */
#define ESPCFG3_FSCSI	0x02		/* Fast SCSI */
#define ESPCFG3_FCLK	0x01		/* Fast Clock (>25Mhz) */
