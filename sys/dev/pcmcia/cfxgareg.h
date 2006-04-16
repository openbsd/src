/*	$OpenBSD: cfxgareg.h,v 1.1 2006/04/16 20:45:00 miod Exp $	*/

/*
 * Copyright (c) 2005 Matthieu Herrb and Miodrag Vallat
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * (very scarce) registers layout. Actual chip documentation would be immensely
 * appreciated.
 */
#define	CFREG_RESET		0x0000	/* reset and ID register */
#define	CR_RESET			0x8080
#define	CR_ID_MASK			0x00fc
#define	CR_ID				0x001c

#define	CFREG_BLT_CTRL		0x0100	/* control/status */
#define	CC_FIFO_BUSY			0x0010
#define	CC_BLT_BUSY			0x0080
#define	CC_BPP_8			0x0000
#define	CC_BPP_16			0x0180
#define	CFREG_BLT_ROP		0x0102	/* raster op */
#if 0
#define	CROP_SRCXORDST			0x0606	/* not so right... */
#endif
#define	CROP_SOLIDFILL			0x0c0c
#define	CROP_EXTCOPY			0x000c
#define	CFREG_BLT_UNK1		0x0104
#define	CFREG_BLT_UNK2		0x0106
#define	CFREG_BLT_SRCLOW	0x0108
#define	CFREG_BLT_SRCHIGH	0x010a
#define	CFREG_BLT_STRIDE	0x010c
#define	CFREG_BLT_WIDTH		0x0110
#define	CFREG_BLT_HEIGHT	0x0112
#define	CFREG_BLT_SRCCOLOR	0x0118

#define	CFREG_VIDEO		0x01fc
#define	CV_VIDEO_VGA			0x0002

#define	CFREG_BLT_DATA		0x0400

#ifdef	_KERNEL
#define	CFXGA_MEM_RANGE		0x0800
#endif
