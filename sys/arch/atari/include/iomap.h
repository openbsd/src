/*	$NetBSD: iomap.h,v 1.2 1995/03/26 07:24:36 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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

#ifndef _MACHINE_IOMAP_H
#define _MACHINE_IOMAP_H
/*
 * Atari TT hardware:
 * I/O Address map
 */

#define	AD_RAM		(0x000000L)	/* main memory			*/
#define	AD_CART		(0xFA0000L)	/* expansion cartridge		*/
#define	AD_ROM		(0xFC0000L)	/* system ROM			*/
#define	AD_IO		(0xFF8000L)	/* I/O devices			*/
#define	AD_EIO		(0xFFFFFFL)	/* End of I/O devices		*/

/*
 * I/O address parts
 */
#define	AD_RAMCFG	(0xFF8000L)	/* ram configuration		*/
#define	AD_VIDEO	(0xFF8200L)	/* video controller		*/
#define AD_RESERVED	(0xFF8400L)	/* reserved			*/
#define	AD_DMA		(0xFF8600L)	/* DMA device access		*/
#define	AD_SCSI_DMA	(0xFF8700L)	/* SCSI DMA registers		*/
#define	AD_NCR5380	(0xFF8780L)	/* SCSI controller		*/
#define	AD_SOUND	(0xFF8800L)	/* YM-2149			*/
#define	AD_RTC		(0xFF8960L)	/* TT realtime clock		*/
#define	AD_SCC		(0xFF8C80L)	/* SCC 8530			*/
#define	AD_SCU		(0xFF8E00L)	/* System Control Unit		*/

#define	AD_MFP		(0xFFFA00L)	/* 68901			*/
#define	AD_MFP2		(0xFFFA80L)	/* 68901-TT			*/
#define	AD_ACIA		(0xFFFC00L)	/* 2 * 6850			*/
#endif /* _MACHINE_IOMAP_H */
