/*	$OpenBSD: bsdos_ioctl.h,v 1.1 1999/11/13 22:13:00 millert Exp $	*/

/*
 * Copyright (c) 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
