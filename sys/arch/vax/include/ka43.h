/*	$OpenBSD: ka43.h,v 1.6 2011/09/13 21:25:23 miod Exp $ */
/*	$NetBSD: ka43.h,v 1.6 2005/01/14 11:47:43 ragge Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 * Definitions for I/O addresses of
 *
 *	VAXstation 3100 model 76	(RigelMAX)
 */

#define KA43_SIDEX	0x20040004	/* SID extension register */

#define KA43_CFGTST	0x20020000	/* Configuration and Test register */
#define KA43_IORESET	0x20020000	/* I/O Reset register */

#define KA43_ROMGETC	0x20040044
#define KA43_ROMPUTC	0x20040058
#define KA43_ROMPUTS	0x2004004C

#define KA43_CH2_BASE	0x10000000	/* 2nd level cache data area */
#define KA43_CH2_END	0x1FFFFFFF
#define KA43_CH2_SIZE	0x10000000
#define KA43_CT2_BASE	0x21000000	/* 2nd level cache tag area */
#define KA43_CT2_END	0x2101FFFF
#define KA43_CT2_SIZE	   0x20000
#define KA43_CH2_CREG	0x21100000	/* 2nd level cache control register */

#define KA43_ROM_BASE	0x20040000	/* System module ROM */
#define KA43_ROM_END	0x2007FFFF
#define KA43_ROM_SIZE	   0x40000	/* ??? */

#define KA43_IVN_BASE	0x20040020	/* Interrupt Vector Numbers */
#define KA43_IVN_END	0x2004003F
#define KA43_IVN_SIZE	      0x20

#define KA43_HLTCOD	0x20080000	/* Halt Code Register */
/* #define KA43_MSER	0x20080004 */	/* Memory System Error register */
/* #define KA43_MEAR	0x20080008 */	/* Memory Error Address register */
#define KA43_INTMSK	0x2008000C	/* Interrupt Mask register */
#define KA43_VDCORG	0x2008000D	/* Video Controller Origin Register */
#define KA43_VDCSEL	0x2008000E	/* Video Controller Select Register */
#define KA43_INTREQ	0x2008000F	/* Interrupt Request register */
#define KA43_INTCLR	0x2008000F	/* Interrupt Request clear register */
#define KA43_DIAGDSP	0x20080010	/* Diagnostic display register */
#define KA43_PARCTL	0x20080014	/* Parity Control Register */
#define KA43_DIAGTME	0x2008001E	/* diagnostic time register */

#define KA43_PCTL_DPEN	0x00000001	/* DMA parity enable (bit 0) */
#define KA43_PCTL_CPEN	0x00000002	/* CPU Parity enable (bit 1) */
#define KA43_PCTL_DMA	0x01000000	/* LANCE DMA control (bit 24) */

/*
 * "CH2" and "SESR" are two common names related to Secondary Cache 
 */
#define KA43_SESR	0x21100000	/* same as KA43_CH2_CREG */

#define KA43_SESR_CENB	0x00000001	/* Cache Enable */
#define KA43_SESR_SERR  0x00000002
#define KA43_SESR_LERR	0x00000004
#define KA43_SESR_CERR	0x00000008
#define KA43_SESR_DIRTY	0x00000010
#define KA43_SESR_MISS	0x00000020
#define KA43_SESR_DPE	0x00000040	/* Dal Parity Error */
#define KA43_SESR_TPE	0x00000080	/* Tag Parity Error */
#define KA43_SESR_WSB	0x00010000
#define KA43_SESR_CIEA	0x7FFC0000

#define KA43_SESR_BITS \
	"\020\010TPE\007DPE\006MISS\005DIRTY\004CERR\003LERR\002SERR\001ENABLE"

/*
 * The following values refer to bits/bitfields within the 4 internal 
 * registers controlling primary cache: 
 * PR_PCTAG(124, tag-register)		PR_PCIDX(125, index-register)
 * PR_PCERR(126, error-register)	PR_PCSTS(127, status-register)
 */
#define KA43_PCTAG_TAG		0x1FFFF800	/* bits 11-29 */
#define KA43_PCTAG_PARITY	0x40000000
#define KA43_PCTAG_VALID	0x80000000

#define KA43_PCIDX_INDEX	0x000007F8	/* 0x100 Q-word entries */

#define KA43_PCERR_ADDR		0x3FFFFFFF

#define KA43_PCS_FORCEHIT	0x00000001	/* Force hit */
#define KA43_PCS_ENABLE		0x00000002	/* Enable primary cache */
#define KA43_PCS_FLUSH		0x00000004	/* Flush cache */
#define KA43_PCS_REFRESH	0x00000008	/* Enable refresh */
#define KA43_PCS_HIT		0x00000010	/* Cache hit */
#define KA43_PCS_INTERRUPT	0x00000020	/* Interrupt pending */
#define KA43_PCS_TRAP2		0x00000040	/* Trap while trap */
#define KA43_PCS_TRAP1		0x00000080	/* Micro trap/machine check */
#define KA43_PCS_TPERR		0x00000100	/* Tag parity error */
#define KA43_PCS_DPERR		0x00000200	/* Dal data parity error */
#define KA43_PCS_PPERR		0x00000400	/* P data parity error */
#define KA43_PCS_BUSERR		0x00000800	/* Bus error */
#define KA43_PCS_BCHIT		0x00001000	/* B cache hit */

#define KA43_PCSTS_BITS \
	"\020\015BCHIT\014BUSERR\013PPERR\012DPERR\011TPERR\010TRAP1" \
	"\007TRAP2\006INTR\005HIT\004REFRESH\003FLUSH\002ENABLE\001FORCEHIT"

/*
 * Bits in PR_ACCS (Floating Point Accelerator Register)
 */
#define KA43_ACCS_VECTOR	(1<<0)	/* Vector Unit Present */
#define KA43_ACCS_FCHIP		(1<<1)	/* FPU chip present */
#define KA43_ACCS_WEP		(1<<31)	/* Write Even Parity */

/*
 * Other fixed addresses which should be mapped
 */
#define KA43_CPU_BASE	0x20080000	/* so called "CPU registers" */
#define KA43_CPU_END	0x200800FF
#define KA43_CPU_SIZE	     0x100
#define KA43_NWA_BASE	0x20090000	/* Network Address ROM */
#define KA43_NWA_END	0x2009007F
#define KA43_NWA_SIZE	      0x80
#define KA43_SER_BASE	0x200A0000	/* Serial line controller */
#define KA43_SER_END	0x200A000F
#define KA43_SER_SIZE         0x10
#define KA43_WAT_BASE	0x200B0000	/* TOY clock and NV-RAM */
#define KA43_WAT_END	0x200B00FF
#define KA43_WAT_SIZE	     0x100
#define KA43_SC1_BASE	0x200C0080	/* 1st SCSI Controller Chip */
#define KA43_SC1_END	0x200C009F
#define KA43_SC1_SIZE	      0x20
#define KA43_SC2_BASE	0x200C0180	/* 2nd SCSI Controller Chip */
#define KA43_SC2_END	0x200C019F
#define KA43_SC2_SIZE	      0x20
#define KA43_SCS_BASE	0x200C0000	/* area occupied by SCSI 1+2 */
#define KA43_SCS_END	0x200C01FF
#define KA43_SCS_SIZE	     0x200
#define KA43_LAN_BASE	0x200E0000	/* LANCE chip registers */
#define KA43_LAN_END	0x200E0007
#define KA43_LAN_SIZE	      0x08
#define KA43_CUR_BASE	0x200F0000	/* Monochrome video cursor chip */
#define KA43_CUR_END	0x200F003C
#define KA43_CUR_SIZE	      0x40
#define KA43_DMA_BASE	0x202D0000	/* 128KB Data Buffer */
#define KA43_DMA_END	0x202EFFFF
#define KA43_DMA_SIZE      0x20000
#define KA43_VME_BASE	0x30000000
#define KA43_VME_END	0x3003FFFF
#define KA43_VME_SIZE	   0x40000

#define KA43_DIAGMEM	0x28000000	/* start of diagnostic memory */

#define KA43_SC1_DADR	0x200C00A0	/* (1st SCSI) DMA address register */
#define KA43_SC1_DCNT	0x200C00C0	/* (1st SCSI) DMA byte count reg. */
#define KA43_SC1_DDIR	0x200C00C4	/* (1st SCSI) DMA transfer direction */
#define KA43_SC2_DADR	0x200C01A0
#define KA43_SC2_DCNT	0x200C01C0
#define KA43_SC2_DDIR	0x200C01C4

/*
 * Clock-Chip data in NVRAM
 */
#define KA43_CPMBX	0x200B0038	/* Console Mailbox (1 byte) */
#define KA43_CPFLG	0x200B003C	/* Console Program Flags (1 byte) */
#define KA43_LK201_ID	0x200B0040	/* Keyboard Variation (1 byte) */
#define KA43_CONS_ID	0x200B0044	/* Console Device Type (1 byte) */
#define KA43_SCR	0x200B0048	/* Console Scratch RAM */
#define KA43_TEMP	0x200B0058	/* Used by System Firmware */
#define KA43_BAT_CHK	0x200B0088	/* Battery Check Data */
#define KA43_PASSWD	0x200B0098	/* ??? */
#define KA43_BOOTFLG	0x200B00A8	/* Default Boot Flags (4 bytes) */
#define KA43_SCRLEN	0x200B00B8	/* Number of pages of SCR (1 byte) */
#define KA43_SCSIPORT	0x200B00BC	/* Tape Controller Port Data */
#define KA43_RESERVED	0x200B00C0	/* Reserved (16 bytes) */

struct ka43_cpu {
	u_long  hltcod;		/* Halt Code Register */
	u_long  pad2;		
	u_long  pad3;		
	u_char  intreg[4];	/* Four 1-byte registers */
	u_short	diagdsp;	/* Diagnostic display register */
	u_short pad4;
	u_long	parctl;		/* Parity Control Register */
	u_short pad5;
	u_short pad6;
	u_short pad7;
	u_short	diagtme;	/* Diagnostic time register */
};

struct ka43_clock {
	u_long  :2;	u_long	sec	:8;	u_long  :22;
	u_long  :2;	u_long	secalrm :8;	u_long  :22;
	u_long  :2;	u_long	min	:8;	u_long  :22;
	u_long  :2;	u_long	minalrm	:8;	u_long  :22;
	u_long  :2;	u_long	hr	:8;	u_long  :22;
	u_long  :2;	u_long	hralrm	:8;	u_long  :22;
	u_long  :2;	u_long	dayofwk	:8;	u_long  :22;
	u_long  :2;	u_long	day	:8;	u_long  :22;
	u_long  :2;	u_long	mon	:8;	u_long  :22;
	u_long  :2;	u_long	yr	:8;	u_long  :22;
	u_long  :2;	u_long	csr0	:8;	u_long  :22;
	u_long  :2;	u_long	csr1	:8;	u_long  :22;
	u_long  :2;	u_long	csr2	:8;	u_long  :22;
	u_long  :2;	u_long	csr3	:8;	u_long  :22;
	u_long  :2;	u_long	req	:4;
			u_long	halt	:4;	u_long  :22;
};
