/*	$OpenBSD: linux_blkio.h,v 1.1 2001/04/09 06:53:44 tholo Exp $	*/
/*	$NetBSD: linux_blkio.h,v 1.2 2001/01/18 17:48:04 tv Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
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

/*
 * Definitions for ioctl calls that work on filesystems, as defined
 * in <linux/fs.h>
 */

#ifndef _LINUX_BLKIO_H
#define _LINUX_BLKIO_H

#define LINUX_BLKROSET		_LINUX_IO(0x12, 93)
#define LINUX_BLKROGET		_LINUX_IO(0x12, 94)
#define LINUX_BLKRRPART		_LINUX_IO(0x12, 95)
#define LINUX_BLKGETSIZE	_LINUX_IO(0x12, 96)
#define LINUX_BLKFLSBUF		_LINUX_IO(0x12, 97)
#define LINUX_BLKRASET		_LINUX_IO(0x12, 98)
#define LINUX_BLKRAGET		_LINUX_IO(0x12, 99)
#define LINUX_BLKFRASET		_LINUX_IO(0x12, 100)
#define LINUX_BLKFRAGET		_LINUX_IO(0x12, 101)
#define LINUX_BLKSECTSET	_LINUX_IO(0x12, 102)
#define LINUX_BLKSECTGET	_LINUX_IO(0x12, 103)
#define LINUX_BLKSSZGET		_LINUX_IO(0x12, 104)
#define LINUX_BLKPG		_LINUX_IO(0x12, 105)

#endif /* _LINUX_BLKIO_H */
