/*	$OpenBSD: endian.h,v 1.7 1997/08/08 08:26:15 downsj Exp $ */
/*	$NetBSD: endian.h,v 1.6 1996/10/11 00:43:00 christos Exp $ */

/*
 * Copyright (c) 1987, 1991 Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)endian.h	7.7 (Berkeley) 4/3/91
 */

#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_

#define _QUAD_HIGHWORD	0
#define _QUAD_LOWWORD	1

/*
 * Definitions for byte order, according to byte significance from low
 * address to high.
 */
#define	LITTLE_ENDIAN	1234	/* LSB first: i386, vax */
#define	BIG_ENDIAN	4321	/* MSB first: 68000, ibm, net */
#define	PDP_ENDIAN	3412	/* LSB first in word, MSW first in int32_t */

#define	BYTE_ORDER	BIG_ENDIAN

#include <sys/cdefs.h>

typedef u_int32_t in_addr_t;                      
typedef u_int16_t in_port_t;

__BEGIN_DECLS
u_int32_t	htonl __P((u_int32_t));
u_int16_t	htons __P((u_int16_t));
u_int32_t	ntohl __P((u_int32_t));
u_int16_t	ntohs __P((u_int16_t));
__END_DECLS

/*
 * Macros for network/external number representation conversion.
 *
 * The way this works is that HTONS(x) modifies x and *can't* be used as
 * and rvalue i.e.  foo=HTONS(bar) is wrong.  Likewise x=htons(x) should
 * never be used where HTONS(x) will serve i.e. foo=htons(foo) is wrong.
 * Failing to observe these rule will result in code that appears to work
 * and probably does work, but generates gcc warnings on architectures
 * where the macros are used to optimize away an unneeded conversion.
 */
#if BYTE_ORDER == BIG_ENDIAN && !defined(lint)
#define	ntohl(x)	(x)
#define	ntohs(x)	(x)
#define	htonl(x)	(x)
#define	htons(x)	(x)

#define	NTOHL(x)	(void) (x)
#define	NTOHS(x)	(void) (x)
#define	HTONL(x)	(void) (x)
#define	HTONS(x)	(void) (x)

#else

#define	NTOHL(x)	(x) = ntohl((in_addr_t)x)
#define	NTOHS(x)	(x) = ntohs((in_port_t)x)
#define	HTONL(x)	(x) = htonl((in_addr_t)x)
#define	HTONS(x)	(x) = htons((in_port_t)x)
#endif

#endif /* _MACHINE_ENDIAN_H_ */
