/*	$NetBSD: tsreg.h,v 1.1 1996/01/06 16:43:47 ragge Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by
 * Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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
 * TSV05 u.g. 5-11:
 * 
 * The TSV05 Subsystem has four device registers that occupy only two
 * LSI-11 Bus word locations: a Data Buffer (TSDB), a Bus Address
 * Register (TSBA), a Status Register (TSSR), and an Extended Data
 * Bufffer (TSDBX). The TSDB is an 18-bit register that is ...
 */

struct tsdevice {
	unsigned short tsdb;/* Data Buffer (TSDB)/Bus Address Register (TSBA) */
	unsigned short tssr;/* Status Reg. (TSSR)/Extended Data Buffer(TSDBX) */
};

/*
 * TSSR Register bit definitions 
 */
#define TS_SC	0x8000	/* Special Condition */
#define TS_UPE	0x4000	/* not used in TSV05, UPE in TS11 */
#define TS_SCE	0x2000	/* Sanity Check Error, SPE in TS11 */
#define TS_RMR	0x1000	/* Register Modification Refused */
#define TS_NXM	0x0800	/* Nonexistent Memory */
#define TS_NBA	0x0400	/* Need Buffer Address */
#define TS_A11	0x0300	/* Address Bits 17-16 */
#define TS_SSR	0x0080 	/* Subsystem Ready */
#define TS_OFL	0x0040	/* Off Line */
#define TS_FTC	0x0030	/* Fatal Termination Class Code */
#define TS_TC	0x000E	/* Termination Class Code */
#define TS_NU	0x0001	/* Not Used */

#define TS_TSSR_BITS	"\20\20SC\17UPE\16SCE\15RMR\14NXM\13NBA\12A17\11A16" \
			   "\10SSR\7OFL\6FTC\5FTC\4FTL\3ERR\2ATTN\1NU"

/* 
 * Termination Codes
 */
#define TS_FTC_IDF	(0<<4)	/* internal diagnostic failure */
#define TS_FTC_RSVD	(1<<4)	/* Reserved */
#define TS_FTC_NU	(2<<4)	/* Not Used */
#define TS_FTC_DPD	(3<<4)	/* Detection of Power Down (not implemented) */

#define TS_TC_NORM	(0<<1)	/* Normal Termination */
#define TS_TC_ATTN	(1<<1)	/* Attention Condition */
#define TS_TC_TSA	(2<<1)	/* Tape status alert */
#define TS_TC_FR	(3<<1)	/* Function reject */
#define TS_TC_TPD	(4<<1)	/* Tape position is one record down (recov.) */
#define TS_TC_TNM	(5<<1)	/* Tape not moved (recoverable) */
#define TS_TC_TPL	(6<<1)	/* Tape position lost (unrecoverable) */
#define TS_TC_FCE	(7<<1)	/* Fatal Controller Error (see FTC) */

struct tscmd {			/* command packet (not all words required) */
	unsigned short cmdr;	/* command word */
	unsigned short cw1;	/* low order data pointer address  (A15-00) */
	unsigned short cw2;	/* high order data pointer address (A21-16) */
	unsigned short cw3;	/* count parameter */
};

/*
 * Command flags
 */
#define TS_CF_ACK	(1<<15)		/* Acknowledge */
#define TS_CF_CVC	(1<<14)		/* Clear Volume Check */
#define TS_CF_OPP	(1<<13)		/* Opposite */
#define TS_CF_SWB	(1<<12)		/* Swap Bytes */
#define TS_CF_IE	(1<< 7)		/* Interrupt Enable */
#define TS_CF_CMODE	0x0F00		/* Command Mode Field */
#define TS_CF_CCODE	0x001F		/* Command Code (major) */
#define TS_CF_CMASK	0x0F1F		/* mask for complete command */

#define TS_CMD(cMode,cCode)	((cMode<<8)|cCode)

#define TS_CC_READ	0x01			/* READ */
#define TS_CMD_RNF	TS_CMD(0,TS_CC_READ)	/* Read Next (Forward) */
#define TS_CMD_RPR	TS_CMD(1,TS_CC_READ)	/* Read Previous (Reverse) */
#define TS_CMD_RPF	TS_CMD(2,TS_CC_READ)	/* Read Previous (Forward) */ 
#define TS_CMD_RNR	TS_CMD(3,TS_CC_READ)	/* Read Next (Reverse) */

#define TS_CC_WCHAR	0x04			/* WRITE CHARACTERISTICS */
#define TS_CMD_WCHAR	TS_CMD(0,TS_CC_WCHAR)	/* Load msg-buffer etc. */

#define TS_CC_WRITE	0x05			/* WRITE */
#define TS_CMD_WD	TS_CMD(0,TS_CC_WRITE)	/* Write Data (Next) */
#define TS_CMD_WDR	TS_CMD(1,TS_CC_WRITE)	/* Write Data (Retry) */

#define TS_CC_WSM	0x06			/* WRITE SUBSYSTEM MEMORY */
#define TS_CMD_WSM	TS_CMD(0,TS_CC_WSM)	/* (diagnostics only) */

#define TS_CC_POS	0x08			/* POSITION */
#define TS_CMD_SRF	TS_CMD(0,TS_CC_POS)	/* Space Records Forward */
#define TS_CMD_SRR	TS_CMD(1,TS_CC_POS)	/* Space Records Reverse */
#define TS_CMD_STMF	TS_CMD(2,TS_CC_POS)	/* Skip Tape Marks Forward */
#define TS_CMD_STMR	TS_CMD(3,TS_CC_POS)	/* Skip Tape Marks Reverse */
#define TS_CMD_RWND	TS_CMD(4,TS_CC_POS)	/* Rewind */

#define TS_CC_FRMT	0x09			/* FORMAT */
#define TS_CMD_WTM	TS_CMD(0,TS_CC_FRMT)	/* Write Tape Mark */
#define TS_CMD_ETM	TS_CMD(1,TS_CC_FRMT)	/* Erase */
#define TS_CMD_WTMR	TS_CMD(2,TS_CC_FRMT)	/* Write Tape Mark (Retry) */

#define TS_CC_CTRL	0x0A			/* CONTROL */
#define TS_CMD_MBR	TS_CMD(0,TS_CC_CTRL)	/* Message Buffer Release */
#define TS_CMD_RWUL	TS_CMD(1,TS_CC_CTRL)	/* Rewind and Unload */
#define TS_CMD_NOP	TS_CMD(2,TS_CC_CTRL)	/* NO-OP (TS11: clean tape) */
#define TS_CMD_RWII	TS_CMD(4,TS_CC_CTRL)	/* Rewind with intermediate */
						/* interrupt (TS11: N.A.) */
#define TS_CC_INIT	0x0B			/* INITIALIZE */
#define TS_CMD_INIT	TS_CMD(0,TS_CC_INIT)	/* Controller/Drive Initial. */

#define TS_CC_STAT	0x0F			/* GET STATUS */
#define TS_CMD_STAT	TS_CMD(0,TS_CC_STAT)	/* Get Status (END) */

struct tsmsg {			/* message packet */
	unsigned short hdr;	/* ACK, class-code, format 1, message type */
	unsigned short dfl;	/* data field length (8 bit) */
	unsigned short rbpcr;	/* residual b/r/tm count word */
	unsigned short xst0;	/* Extended Status Registers 0-4 */
	unsigned short xst1;
	unsigned short xst2;
	unsigned short xst3;
	unsigned short xst4;	/* total size: 16 bytes */
};

/*
 * Flags used in write-characteristics command
 */
#define TS_WC_ESS	(1<<7)	/* Enable Skip Tape Marks Stop */
#define TS_WC_ENB	(1<<6)  /* Enable Tape Mark Stop at Bot */
#define TS_WC_EAI	(1<<5)	/* Enable Attention interrupts */
#define TS_WC_ERI	(1<<4)	/* Enable Message Buffer Release interrupts */
#define TS_WC_HSP	(1<<5)	/* High Speed Select (25 in/s vs. 100 in/s) */

/*
 * Status flags
 *
 * Extended Status register 0 (XST0)  --  XST0 appears as the fourth word 
 * in the message buffer stored by the TSV05 subsystem upon completion of 
 * a command or an ATTN
 */
#define TS_SF_TMK	(1<<15)	/* Tape Mark Detected */
#define TS_SF_RLS	(1<<14)	/* Record Length Short */
#define TS_SF_LET	(1<<13)	/* Logical End of Tape */
#define TS_SF_RLL	(1<<12)	/* Record Length Long */
#define TS_SF_WLE	(1<<11)	/* Write Lock Error */
#define TS_SF_NEF	(1<<10) /* Nonexecutable Function */
#define TS_SF_ILC	(1<< 9)	/* Illegal Command */
#define TS_SF_ILA	(1<< 8)	/* Illegal Address */
#define TS_SF_MOT	(1<< 7)	/* Motion */
#define TS_SF_ONL	(1<< 6)	/* On-Line */
#define TS_SF_IE	(1<< 5)	/* Interrupt Enable */
#define TS_SF_VCK	(1<< 4)	/* Volume Check */
#define TS_SF_PED	(1<< 3)	/* Phase Encoded Drive */
#define TS_SF_WLK	(1<< 2)	/* Write Locked */
#define TS_SF_BOT	(1<< 1)	/* Beginning of Tape */
#define TS_SF_EOT	(1<< 0)	/* End of Tape */

#define TS_XST0_BITS	"\20\20TMK\17RLS\16LET\15RLL\14WLE\13NEF\12ILC\11ILA" \
			   "\10MOT\07ONL\06IE \05VCK\04PED\03WLK\02BOT\01EOT"
/*
 * Extended Status register 1 (XST1)  --  XST1 appears as the fifth word 
 * in the message buffer stored by the TSV05 subsystem upon completion of 
 * a command or an ATTN
 */
#define TS_SF_DLT	(1<<15)	/* Data Late */
#define TS_SF_COR	(1<<13)	/* Correctable Data */
#define TS_SF_CRS	(1<<12)	/* TS11: Crease Detected */
#define TS_SF_TIG	(1<<11)	/* TS11: Trash in Gap */
#define TS_SF_DBF	(1<<10)	/* TS11: Desckew Buffer Fail */
#define TS_SF_SCK	(1<< 9)	/* TS11: Speed Check */
#define TS_SF_RBP	(1<< 8)	/* Read Bus Parity Error */
#define TS_SF_IPR	(1<< 7)	/* TS11: Invalid Preamble */
#define TS_SF_IPO	(1<< 6)	/* TS11: Invalid Postamble */
#define TS_SF_SYN	(1<< 5)	/* TS11: Sync Failure */
#define TS_SF_IED	(1<< 4)	/* TS11: Invalid End Data */
#define TS_SF_POS	(1<< 3)	/* TS11: Postamble short */
#define TS_SF_POL	(1<< 2)	/* TS11: Postamble long */
#define TS_SF_UNC	(1<< 1)	/* Uncorrectable Data or Hard Error */
#define TS_SF_MTE	(1<< 0)	/* TS11: Multitrack Error */

#define TS_XST1_BITS	"\20\20DLT\16COR\15CRS\14TIG\13DBF\12SCK\11RBP" \
			   "\10IPR\07IPO\06SYN\05IED\04POS\03POL\02UNC\01MTE"

/*
 * Extended Status register 2 (XST2)  --  sixth word 
 */
#define TS_SF_OPM	(1<<15)	/* Operation in Progress (tape moving) */
#define TS_SF_RCE	(1<<14)	/* RAM Checksum Error */
#define TS_SF_SBP	(1<<13)	/* TS11: Serial 08 bus parity */
#define TS_SF_CAF	(1<<12)	/* TS11: Capstan Acceleration fail */
#define TS_SF_WCF	(1<<10)	/* Write Clock Failure */
#define TS_SF_PDT	(1<< 8)	/* TS11: Parity Dead Track */
#define TS_SF_RL	0x00FF	/* Revision Level */
#define TS_SF_EFES	(1<< 7)	/* extended features enable switch */
#define TS_SF_BES	(1<< 6)	/* Buffering enable switch */
#define TS_SF_MCRL	0x003F	/* micro-code revision level */
#define TS_SF_UNIT	0x0003	/* unit number of selected transport */

#define TS_XST2_BITS	"\20\20OPM\17RCE\16SBP\15CAF\13WCF\11PDT\10EFES\7BES"

/*
 * Extended Status register 3 (XST3))  --  seventh word 
 */
#define TS_SF_MDE	0xFF00	/* Micro-Diagnostics Error Code */
#define TS_SF_LMX	(1<< 7)	/* TS11: Tension Arm Limit Exceeded */
#define TS_SF_OPI	(1<< 6)	/* Operation Incomplete */
#define TS_SF_REV	(1<< 5)	/* Revers */
#define TS_SF_CRF	(1<< 4)	/* TS11: Capstan Response Failure */
#define TS_SF_DCK	(1<< 3)	/* Density Check */
#define TS_SF_NBE	(1<< 2)	/* TS11: Noise Bit during Erase */
#define TS_SF_LSA	(1<< 1)	/* TS11: Limit Switch Activated */
#define TS_SF_RIB	(1<< 0)	/* Reverse into BOT */

#define TS_XST3_BITS	"\20\10LMX\07OPI\06REV\05CRF\04DCK\03NBE\02LSA\01RIB"

/*
 * Extended Status register 4 (XST4))  --  eighth word 
 */
#define TS_SF_HSP	(1<<15)	/* High Speed */
#define TS_SF_RCX	(1<<14)	/* Retry Count Exceeded */
#define TS_SF_WRC	0x00FF	/* Write Retry Count Statistics */

#define TS_XST4_BITS	"\20\20HSP\17RCX"


