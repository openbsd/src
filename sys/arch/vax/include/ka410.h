/*	$OpenBSD: ka410.h,v 1.6 2009/06/20 20:57:39 miod Exp $ */
/*	$NetBSD: ka410.h,v 1.2 1997/02/19 10:06:05 ragge Exp $ */
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
 * Definition for  I/O addresses of
 *
 *	MicroVAX 2000	(TeamMate)
 *	VAXstation 2000 (VAXstar)
 */

#define KA410_SIDEX	0x20040004	/* SID extension register */

#define KA410_CFGTST	0x20020000	/* Configuration and Test register */
#define KA410_IORESET	0x20020000	/* I/O Reset register */

#define KA410_ROM_BASE	0x20040000	/* System module ROM */
#define KA410_ROM_END	0x2007FFFF
#define KA410_ROM_SIZE	   0x40000

#define KA410_IVN_BASE	0x20040020	/* Interrupt Vector Numbers */
#define KA410_IVN_END	0x2004003F
#define KA410_IVN_SIZE	      0x20

#define KA410_HLTCOD	0x20080000	/* Halt Code Register */
#define KA410_MSER	0x20080004	/* Memory System Error register */
#define KA410_MEAR	0x20080008	/* Memory Error Address register */
#define KA410_INTMSK	0x2008000C	/* Interrupt Mask register */
#define KA410_VDCORG	0x2008000D	/* Video Controller Origin Register */
#define KA410_VDCSEL	0x2008000E	/* Video Controller Select Register */
#define KA410_INTREQ	0x2008000F	/* Interrupt Request register */
#define KA410_INTCLR	0x2008000F	/* Interrupt Request clear register */

/*
 * Other fixed addresses which should be mapped
 */
#define KA410_NWA_BASE	0x20090000	/* Network Address ROM */
#define KA410_NWA_END	0x2009007F
#define KA410_NWA_SIZE	      0x80
#define KA410_SER_BASE	0x200A0000	/* Serial line controller */
#define KA410_SER_END	0x200A000F
#define KA410_SER_SIZE        0x10
#define KA410_WAT_BASE	((struct ka410_clock *)0x200B0000)/* TOY clock */
#define KA410_WAT_END	0x200B00FF
#define KA410_WAT_SIZE	     0x100
#define KA410_DKC_BASE	0x200C0000	/* Disk Controller Ports */
#define KA410_DKC_END	0x200C0007
#define KA410_DKC_SIZE	      0x08
#define KA410_SCS_BASE	0x200C0080	/* Tape (SCSI) Controller Chip */
#define KA410_SCS_END	0x200C009F
#define KA410_SCS_SIZE	      0x20
#define KA410_DMA_BASE	0x200D0000	/* Disk Data buffer RAM */
#define KA410_DMA_END	0x200D3FFF
#define KA410_DMA_SIZE	    0x4000
#define KA410_LAN_BASE	0x200E0000	/* LANCE chip registers */
#define KA410_LAN_END	0x200E0007
#define KA410_LAN_SIZE	      0x08
#define KA410_CUR_BASE	0x200F0000	/* Monochrome video cursor chip */
#define KA410_CUR_END	0x200F003F
#define KA410_CUR_SIZE	      0x40

#define KA410_SCS_DADR	0x200C00A0	/* Tape(SCSI) DMA address register */
#define KA410_SCS_DCNT	0x200C00C0	/* Tape(SCSI) DMA byte count reg. */
#define KA410_SCS_DDIR	0x200C00C4	/* Tape(SCSI) DMA transfer direction */

/*
 * Definitions for the Configuration and Test Register
 */
#define KA410_CFG_MULTU		0x80	/* MicroVAX or VAXstation */
#define KA410_CFG_NETOPT	0x40	/* Network option present */
#define KA410_CFG_L3CON		0x20	/* Console on line #3 of dc */
#define KA410_CFG_CURTEST	0x10	/* Cursor Test (monochrom) */
#define KA410_CFG_VIDOPT	0x08	/* Video option present */
#define KA410_CFG_MEMSZ		0x07	/* Memory option type/size */

#define KA410_CFG_0MB		0x00	/* No additional Memory board */
#define KA410_CFG_1MB		0x01
#define KA410_CFG_2MB		0x02
#define KA410_CFG_4MB		0x03
#define KA410_CFG_6MB		0x04
#define KA410_CFG_8MB		0x05
#define KA410_CFG_12MB		0x06
#define KA410_CFG_14MB		0x07


/* 
 * interrupt request-, clear-, and mask register 
 */
extern volatile unsigned char *ka410_intreq;
extern volatile unsigned char *ka410_intclr;
extern volatile unsigned char *ka410_intmsk;

#define INTR_SR	(1<<7)	/* Serial line receiver or silo full */
#define INTR_ST	(1<<6)	/* Serial line transmitter done */
#define INTR_NP	(1<<5)	/* Network controller primary */
#define INTR_NS	(1<<4)	/* Network controller secondary */
#define INTR_VF	(1<<3)	/* Video end of frame */
#define INTR_VS	(1<<2)	/* Video secondary */
#define INTR_SC	(1<<1)	/* SCSI controller */
#define INTR_DC	(1<<0)	/* Disk controller */

/*
 * Clock-Chip data in NVRAM
 */
#define KA410_CPMBX	0x200B0038	/* Console Mailbox (1 byte) */
#define KA410_CPFLG	0x200B003C	/* Console Program Flags (1 byte) */
#define KA410_LK201_ID	0x200B0040	/* Keyboard Variation (1 byte) */
#define KA410_CONS_ID	0x200B0044	/* Console Device Type (1 byte) */
#define KA410_SCR	0x200B0048	/* Console Scratch RAM */
#define KA410_TEMP	0x200B0058	/* Used by System Firmware */
#define KA410_BAT_CHK	0x200B0088	/* Battery Check Data */
#define KA410_BOOTDEV	0x200B0098	/* Default Boot Device (4 bytes) */
#define KA410_BOOTFLG	0x200B00A8	/* Default Boot Flags (4 bytes) */
#define KA410_SCRLEN	0x200B00B8	/* Number of pages of SCR (1 byte) */
#define KA410_SCSIPORT	0x200B00BC	/* Tape Controller Port Data */
#define KA410_RESERVED	0x200B00C0	/* Reserved (16 bytes) */


/*
 * KA410 uses bits 2-9 of longwords to store single bytes in NVRAM, 
 * thus we declare the clock as an struct of bit-fields, so that the
 * generic clock-routines work for KA410...
 */
struct ka410_clock {
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
	u_long  :2;	u_long	cpmbx	:8;	u_long  :22;
};
