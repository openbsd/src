/*	$NetBSD: ka410.h,v 1.1 1996/08/02 11:22:13 ragge Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
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
 * interrupt vector numbers
 */
#define IVEC_BASE	0x20040020
#define IVEC_SR		0x000002C0
#define IVEC_ST		0x000002C4
#define IVEC_NP		0x00000250
#define IVEC_NS		0x00000254
#define IVEC_VF		0x00000244
#define IVEC_VS		0x00000248
#define IVEC_SC		0x000003F8
#define IVEC_DC		0x000003FC

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

