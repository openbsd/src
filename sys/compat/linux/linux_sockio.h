/*	$OpenBSD: linux_sockio.h,v 1.8 2002/02/06 01:55:04 jasoni Exp $ */
/*	$NetBSD: linux_sockio.h,v 1.5 1996/03/08 04:56:07 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
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

#ifndef _LINUX_SOCKIO_H
#define _LINUX_SOCKIO_H

#define	LINUX_FIOSETOWN		_LINUX_IO(0x89, 1)
#define	LINUX_SIOCSPGRP		_LINUX_IO(0x89, 2)
#define	LINUX_FIOGETOWN		_LINUX_IO(0x89, 3)
#define	LINUX_SIOCGPGRP		_LINUX_IO(0x89, 4)
#define	LINUX_SIOCATMARK	_LINUX_IO(0x89, 5)
#define	LINUX_SIOCGSTAMP	_LINUX_IO(0x89, 6)
#define	LINUX_SIOCGIFCONF	_LINUX_IO(0x89, 18)
#define	LINUX_SIOCGIFFLAGS	_LINUX_IO(0x89, 19)
#define LINUX_SIOCSIFFLAGS	_LINUX_IO(0x89, 20)
#define	LINUX_SIOCGIFADDR	_LINUX_IO(0x89, 21)
#define LINUX_SIOCSIFADDR	_LINUX_IO(0x89, 22)
#define	LINUX_SIOCGIFDSTADDR	_LINUX_IO(0x89, 23)
#define	LINUX_SIOCGIFBRDADDR	_LINUX_IO(0x89, 25)
#define	LINUX_SIOCGIFNETMASK	_LINUX_IO(0x89, 27)
#define	LINUX_SIOCGIFMETRIC	_LINUX_IO(0x89, 29)
#define	LINUX_SIOCGIFMTU	_LINUX_IO(0x89, 33)
#define	LINUX_SIOCGIFHWADDR	_LINUX_IO(0x89, 39)
#define LINUX_SIOCADDMULTI	_LINUX_IO(0x89, 49)
#define LINUX_SIOCDELMULTI	_LINUX_IO(0x89, 50)
#define LINUX_SIOCGIFBR		_LINUX_IO(0x89, 64)
#define LINUX_SIOCDEVPRIVATE	_LINUX_IO(0x89, 0xf0)

#endif /* _LINUX_SOCKIO_H */
