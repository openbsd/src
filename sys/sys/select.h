/*	$OpenBSD: select.h,v 1.9 2006/03/21 08:17:33 otto Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)select.h	8.2 (Berkeley) 1/4/94
 */

#ifndef _SYS_SELECT_H_
#define	_SYS_SELECT_H_

#include <sys/cdefs.h>
#include <sys/time.h>		/* for types and struct timeval */

/*
 * Select uses bit masks of file descriptors in longs.  These macros
 * manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here should
 * be enough for most uses.
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	1024
#endif

/*
 * We don't want to pollute the namespace with select(2) internals.
 * Non-underscore versions are exposed later #if __BSD_VISIBLE
 */
#define	__NBBY	8				/* number of bits in a byte */
typedef int32_t	__fd_mask;
#define __NFDBITS ((unsigned)(sizeof(__fd_mask) * __NBBY)) /* bits per mask */
#define	__howmany(x, y)	(((x) + ((y) - 1)) / (y))

typedef	struct fd_set {
	__fd_mask fds_bits[__howmany(FD_SETSIZE, __NFDBITS)];
} fd_set;

#define	FD_SET(n, p) \
	((p)->fds_bits[(n) / __NFDBITS] |= (1 << ((n) % __NFDBITS)))
#define	FD_CLR(n, p) \
	((p)->fds_bits[(n) / __NFDBITS] &= ~(1 << ((n) % __NFDBITS)))
#define	FD_ISSET(n, p) \
	((p)->fds_bits[(n) / __NFDBITS] & (1 << ((n) % __NFDBITS)))
#ifdef _KERNEL
#define	FD_COPY(f, t)	bcopy(f, t, sizeof(*(f)))
#define	FD_ZERO(p)	bzero(p, sizeof(*(p)))
#else
#define	FD_COPY(f, t)	memcpy(t, f, sizeof(*(f)))
#define	FD_ZERO(p)	memset(p, 0, sizeof(*(p)))
#endif

#if __BSD_VISIBLE
#define	NBBY	__NBBY
#define fd_mask	__fd_mask
#define NFDBITS	__NFDBITS
#ifndef howmany
#define howmany(x, y)	__howmany(x, y)
#endif
#endif /* __BSD_VISIBLE */

#ifndef _KERNEL
#ifndef _SELECT_DEFINED_
#define _SELECT_DEFINED_
__BEGIN_DECLS
struct timeval;
int	select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
__END_DECLS
#endif
#endif /* !_KERNEL */

#endif /* !_SYS_SELECT_H_ */
