/*	$OpenBSD: bsdos_ioctl.h,v 1.3 2003/06/17 21:56:25 millert Exp $	*/

/*
 * Copyright (c) 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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

#ifndef _BSDOS_IOCTL_H
#define _BSDOS_IOCTL_H

struct bsdos_audio_buf_info {
	int fragments;
	int fragsize;
	int bytes;
};

#define BSDOS_IOCPARM_MASK	0x1fff
#define	BSDOS_IOCGROUP(x)	(((x) >> 8) & 0xff)

#define BSDOS_IOC_VOID		(unsigned long)0x20000000
#define BSDOS_IOC_OUT		(unsigned long)0x40000000
#define BSDOS_IOC_IN		(unsigned long)0x80000000
#define BSDOS_IOC_INOUT		(BSDOS_IOC_IN|BSDOS_IOC_OUT)
#define BSDOS_IOC_DIRMASK	(unsigned long)0xe0000000

#define _BSDOS_IOC(inout,group,num,len) \
	(inout | ((len & BSDOS_IOCPARM_MASK) << 16) | ((group) << 8) | (num))
#define _BSDOS_IOR(g,n,t)		_BSDOS_IOC(BSDOS_IOC_OUT, (g), (n), sizeof(t))

#define BSDOS_SNDCTL_DSP_GETOSPACE	_BSDOS_IOR('P', 12, struct bsdos_audio_buf_info)
#define BSDOS_SNDCTL_DSP_GETISPACE	_BSDOS_IOR('P', 13, struct bsdos_audio_buf_info)

#endif /* _BSDOS_IOCTL_H */
