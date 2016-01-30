/*	$OpenBSD: dpme.h,v 1.26 2016/01/30 17:09:11 krw Exp $	*/

/*
 * dpme.h - Disk Partition Map Entry (dpme)
 *
 * Written by Eryk Vershen
 *
 * This file describes structures and values related to the standard
 * Apple SCSI disk partitioning scheme.
 *
 * Each entry is (and shall remain) 512 bytes long.
 *
 * For more information see:
 *	"Inside Macintosh: Devices" pages 3-12 to 3-15.
 *	"Inside Macintosh - Volume V" pages V-576 to V-582
 *	"Inside Macintosh - Volume IV" page IV-292
 *
 * There is a kernel file with much of the same info (under different names):
 *	/usr/src/mklinux-1.0DR2/osfmk/src/mach_kernel/ppc/POWERMAC/mac_label.h
 */

/*
 * Copyright 1996 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __dpme__
#define __dpme__

#define	BLOCK0_SIGNATURE	0x4552	/* i.e. 'ER' */
#define	DPME_SIGNATURE		0x504D	/* i.e. 'PM' */

#define	DPISTRLEN	32

#endif /* __dpme__ */
