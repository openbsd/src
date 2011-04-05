/*	$OpenBSD: linux_hdio.h,v 1.2 2011/04/05 22:54:30 pirofti Exp $	*/
/*	$NetBSD: linux_hdio.h,v 1.1 2000/12/10 14:12:17 fvdl Exp $	*/

/*
 * Copyright (c) 2000 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_HDIO_H_
#define _LINUX_HDIO_H_

/*
 * Linux 'hd' (mostly really IDE disk) ioctl calls.
 */

#define LINUX_HDIO_GETGEO		0x0301
#define LINUX_HDIO_GETGEO_BIG		0x0330
#define LINUX_HDIO_GETGEO_BIG_RAW	0x0331
#define LINUX_HDIO_GET_UNMASKINTR	0x0302
#define LINUX_HDIO_GET_MULTCOUNT	0x0304
#define LINUX_HDIO_OBSOLETE_IDENTITY	0x0307
#define LINUX_HDIO_GET_KEEPSETTINGS	0x0308
#define LINUX_HDIO_GET_32BIT		0x0309
#define LINUX_HDIO_GET_NOWERR		0x030a
#define LINUX_HDIO_GET_DMA		0x030b
#define LINUX_HDIO_GET_NICE		0x030c
#define LINUX_HDIO_GET_IDENTITY		0x030d

#define LINUX_HDIO_DRIVE_RESET		0x031c
#define LINUX_HDIO_TRISTATE_HWIF	0x031d
#define LINUX_HDIO_DRIVE_TASK		0x031e
#define LINUX_HDIO_DRIVE_CMD		0x031f

#define LINUX_HDIO_SET_MULTCOUNT	0x0321
#define LINUX_HDIO_SET_UNMASKINTR	0x0322
#define LINUX_HDIO_SET_KEEPSETTINGS	0x0323
#define LINUX_HDIO_SET_32BIT		0x0324
#define LINUX_HDIO_SET_NOWERR		0x0325
#define LINUX_HDIO_SET_DMA		0x0326
#define LINUX_HDIO_SET_PIO_MODE		0x0327
#define LINUX_HDIO_SCAN_HWIF		0x0328
#define LINUX_HDIO_SET_NICE		0x0329
#define LINUX_HDIO_UNREGISTER_HWIF	0x032a

struct linux_hd_geometry {
	u_char heads;
	u_char sectors;
	u_short cylinders;
	u_long start;
};

struct linux_hd_big_geometry {
	u_char heads;
	u_char sectors;
	u_int cylinders;
	u_long start;
};

#endif /* _LINUX_HDIO_H_ */
