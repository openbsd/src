/*	$NetBSD: fdreg.h,v 1.2 1995/04/22 22:18:21 leo Exp $	*/

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

#ifndef _FDREG_H
#define _FDREG_H
/*
 * Atari TT hardware:
 * Western Digital 1772 Floppy Disk Controller.
 */

/*
 * Accessing the FDC registers is indirect through ST-specific
 * DMA circuitry. See also dma.h.
 */
#define	FDC_CS	(DMA_FDC              )	/* command/status		*/
#define	FDC_TR	(DMA_FDC|       DMA_A0)	/* track register		*/
#define	FDC_SR	(DMA_FDC|DMA_A1       )	/* sector register		*/
#define	FDC_DR	(DMA_FDC|DMA_A1|DMA_A0)	/* data register		*/

/*
 * commands (relevant bits/fields indicated)
 */
#define	RESTORE	0x00		/* ( HVRR) seek to track 0		*/
#define	SEEK	0x10		/* ( HVRR) seek to track		*/
#define	STEP	0x20		/* (UHVRR) step in same direction	*/
#define	STEPI	0x40		/* (UHVRR) step in			*/
#define	STEPO	0x60		/* (UHVRR) step out			*/
#define	F_READ	0x80		/* (MHE00) read sector			*/
#define	F_WRITE	0xA0		/* (MHEPA) write sector			*/
#define	READID	0xC0		/* ( HE00) read sector ID		*/
#define	READTR	0xE0		/* ( HE00) read track			*/
#define	WRITETR	0xF0		/* ( HEP0) write track			*/
#define	IRUPT	0xD0		/* ( IIII) force interrupt		*/

/*
 * other bits/fields in command register
 */
#define	RATE6	0x00		/* not 2, but  6 msec steprate		*/
#define	RATE12	0x01		/* not 3, but 12 msec steprate		*/
#define	RATE2	0x02		/* not 5, but  2 msec steprate		*/
#define	RATE3	0x03		/* not 6, but  3 msec steprate		*/
#define	VBIT	0x04		/* verify sector ID			*/
#define	HBIT	0x08		/* suppress motor on sequence		*/
#define	UBIT	0x10		/* update track register		*/
#define	EBIT	0x04		/* wait 30 msec to settle		*/
#define	MBIT	0x10		/* multi-sector				*/
#define	PBIT	0x02		/* write precompensate			*/
#define	A0BIT	0x01		/* suppress (?) data address mark	*/
#define	IINDEX	0x04		/* interrupt on each index pulse	*/
#define	IFORCE	0x08		/* force interrupt			*/

/*
 * status register
 */
#define	BUSY	0x01		/* set if command under execution	*/
#define	DRQ	0x02		/* Data Register status (pin c1)	*/
#define	LD_T00	0x04		/* lost data; track 00			*/
#define	CRCERR	0x08		/* CRC error				*/
#define	RNF	0x10		/* Record Not Found			*/
#define	RT_SU	0x20		/* Record Type; Spin Up completed	*/
#define	WRI_PRO	0x40		/* Write Protected			*/
#define	MOTORON	0x80		/* Motor On				*/

#endif /* _FDREG_H */
