/*	$OpenBSD: if_tun.h,v 1.15 2007/02/06 10:49:40 claudio Exp $	*/

/*
 * Copyright (c) 1988, Julian Onions <Julian.Onions@nexor.co.uk>
 * Nottingham University 1987.
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

#ifndef _NET_IF_TUN_H_
#define _NET_IF_TUN_H_

#include <sys/ioccom.h>

#define	TUN_OPEN	0x0001
#define	TUN_INITED	0x0002
#define	TUN_RCOLL	0x0004	/* unused */
#define	TUN_IASET	0x0008
#define	TUN_DSTADDR	0x0010
#define	TUN_RWAIT	0x0040
#define	TUN_ASYNC	0x0080
#define	TUN_NBIO	0x0100
#define TUN_BRDADDR	0x0200
#define TUN_STAYUP	0x0400
#define TUN_LAYER2	0x0800

#define	TUN_READY	(TUN_OPEN | TUN_INITED)

/* Maximum packet size */
#define	TUNMTU		3000

/* Maximum receive packet size (hard limit) */
#define TUNMRU          16384

/* iface info */
struct tuninfo {
	u_int	mtu;
	u_short	type;
	u_short	flags;
	u_int	baudrate;
};
#define TUNSIFINFO	_IOW('t', 91, struct tuninfo)
#define TUNGIFINFO	_IOR('t', 92, struct tuninfo)

/* ioctl for changing the broadcast/point-to-point status */
#define TUNSIFMODE      _IOW('t', 93, int)

/* ioctl's for get/set debug */
#define	TUNSDEBUG	_IOW('t', 94, int)
#define	TUNGDEBUG	_IOR('t', 95, int)

#endif /* _NET_IF_TUN_H_ */
