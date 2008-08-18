/*	$OpenBSD: ka420.h,v 1.5 2008/08/18 23:07:24 miod Exp $ */
/*	$NetBSD: ka420.h,v 1.2 1998/06/07 18:34:09 ragge Exp $ */
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
 *	VAXstation 3100 models 30, 40	(PVAX)
 *	MicroVAX 3100 models 10, 20	(Teammate II)
 *	MicroVAX 3100 models 10e, 20e	(Teammate II)
 *	VAXstation 3100 models 38, 48	(PVAX rev#7)
 */

#define KA420_SIDEX	0x20040004	/* SID extension register */

#define KA420_CH2_BASE	0x10000000	/* 2nd level cache data area */
#define KA420_CH2_END	0x10007FFF
#define KA420_CH2_SIZE	    0x8000
#define KA420_CT2_BASE	0x10010000	/* 2nd level cache tag area */
#define KA420_CT2_END	0x10017FFF
#define KA420_CT2_SIZE	    0x8000
#define KA420_CH2_CREG	0x20084000	/* 2nd level cache control register */

#define KA420_CFGTST	0x20020000	/* Configuration and Test register */
#define KA420_IORESET	0x20020000	/* I/O Reset register */

#define KA420_ROM_BASE	0x20040000	/* System module ROM */
#define KA420_ROM_END	0x2007FFFF
#define KA420_ROM_SIZE	   0x40000	/* ??? */

#define KA420_IVN_BASE	0x20040020	/* Interrupt Vector Numbers */
#define KA420_IVN_END	0x2004003F
#define KA420_IVN_SIZE	      0x20

#define KA420_HLTCOD	0x20080000	/* Halt Code Register */
#define KA420_MSER	0x20080004	/* Memory System Error register */
#define KA420_MEAR	0x20080008	/* Memory Error Address register */
#define KA420_INTMSK	0x2008000C	/* Interrupt Mask register */
#define KA420_VDCORG	0x2008000D	/* Video Controller Origin Register */
#define KA420_VDCSEL	0x2008000E	/* Video Controller Select Register */
#define KA420_INTREQ	0x2008000F	/* Interrupt Request register */
#define KA420_INTCLR	0x2008000F	/* Interrupt Request clear register */

#define KA420_CACR	0x20084000	/* L2 cache ctrl reg */

/*
 * Other fixed addresses which should be mapped
 */
#define KA420_NWA_BASE	0x20090000	/* Network Address ROM */
#define KA420_NWA_END	0x2009007F
#define KA420_NWA_SIZE	      0x80
#define KA420_SER_BASE	0x200A0000	/* Serial line controller */
#define KA420_SER_END	0x200A000F
#define KA420_SER_SIZE        0x10
#define KA420_WAT_BASE	0x200B0000	/* TOY clock and NV-RAM */
#define KA420_WAT_END	0x200B00FF
#define KA420_WAT_SIZE	     0x100
#define KA420_DKC_BASE	0x200C0000	/* Disk Controller Ports */
#define KA420_DKC_END	0x200C0007
#define KA420_DKC_SIZE	      0x08
#define KA420_SCS_BASE	0x200C0080	/* Tape (SCSI) Controller Chip */
#define KA420_SCS_END	0x200C009F
#define KA420_SCS_SIZE	      0x20
#define KA420_D16_BASE	0x200D0000	/* 16KB (compatibility) Data Buffer */
#define KA420_D16_END	0x200D3FFF
#define KA420_D16_SIZE	    0x4000
#define KA420_LAN_BASE	0x200E0000	/* LANCE chip registers */
#define KA420_LAN_END	0x200E0007
#define KA420_LAN_SIZE	      0x08
#define KA420_CUR_BASE	0x200F0000	/* Monochrome video cursor chip */
#define KA420_CUR_END	0x200F003F
#define KA420_CUR_SIZE	      0x40
#define KA420_DMA_BASE	0x202D0000	/* 128KB Data Buffer */
#define KA420_DMA_END	0x202EFFFF
#define KA420_DMA_SIZE     0x20000

#define KA420_SCD_DADR	0x200C00A0	/* Tape(SCSI) DMA address register */
#define KA420_SCD_DCNT	0x200C00C0	/* Tape(SCSI) DMA byte count reg. */
#define KA420_SCD_DDIR	0x200C00C4	/* Tape(SCSI) DMA transfer direction */

#define KA420_STC_MODE	0x200C00E0	/* Storage Controller Mode register */

/*
 * Clock-Chip data in NVRAM
 */
#define KA420_CPMBX	0x200B0038	/* Console Mailbox (1 byte) */
#define KA420_CPFLG	0x200B003C	/* Console Program Flags (1 byte) */
#define KA420_LK201_ID	0x200B0040	/* Keyboard Variation (1 byte) */
#define KA420_CONS_ID	0x200B0044	/* Console Device Type (1 byte) */
#define KA420_SCR	0x200B0048	/* Console Scratch RAM */
#define KA420_TEMP	0x200B0058	/* Used by System Firmware */
#define KA420_BAT_CHK	0x200B0088	/* Battery Check Data */
#define KA420_BOOTDEV	0x200B0098	/* Default Boot Device (4 bytes) */
#define KA420_BOOTFLG	0x200B00A8	/* Default Boot Flags (4 bytes) */
#define KA420_SCRLEN	0x200B00B8	/* Number of pages of SCR (1 byte) */
#define KA420_SCSIPORT	0x200B00BC	/* Tape Controller Port Data */
#define KA420_RESERVED	0x200B00C0	/* Reserved (16 bytes) */

/* Used bits in the CFGTST (20020000) register */
#define	KA420_CFG_STCMSK	0xc000	/* Storage controller mask */
#define	KA420_CFG_RB		0x0000	/* RB (ST506/SCSI) present */
#define	KA420_CFG_RD		0x4000	/* RD (SCSI/SCSI) present */
#define	KA420_CFG_NONE		0xc000	/* No storage ctlr present */
#define	KA420_CFG_MULTU		0x80	/* MicroVAX or VAXstation */
#define	KA420_CFG_CACHPR	0x40	/* Secondary cache present */
#define	KA420_CFG_L3CON		0x20	/* Console on line #3 of dc */
#define	KA420_CFG_CURTEST	0x10	/* Cursor Test (monochrom) */
#define	KA420_CFG_VIDOPT	0x08	/* Video option present */

/* Secondary cache bits (CACR, 20084000) */
#define	KA420_CACR_CP3		0x80000000	/* last parity read */
#define	KA420_CACR_CP2		0x40000000	/* last parity read */
#define	KA420_CACR_CP1		0x20000000	/* last parity read */
#define	KA420_CACR_CP0		0x10000000	/* last parity read */
#define	KA420_CACR_TPP		0x00100000	/* tag predicted parity */
#define	KA420_CACR_TGP		0x00080000	/* tag parity read */
#define	KA420_CACR_TGV		0x00040000	/* valid flag */
#define	KA420_CACR_TPE		0x00000020	/* tag parity error */
#define	KA420_CACR_CEN		0x00000010	/* cache enable */

