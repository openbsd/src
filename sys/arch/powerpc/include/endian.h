/*	$OpenBSD: endian.h,v 1.8 1997/10/13 10:53:43 pefo Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef	_MACHINE_ENDIAN_H_
#define	_MACHINE_ENDIAN_H_

#ifndef	_POSIX_SOURCE

#define _QUAD_HIGHWORD	0
#define _QUAD_LOWWORD	1

/*
 * Byte order definition. Byte numbers given in increasing address order.
 */
#define	LITTLE_ENDIAN	1234	/* LSB first: i386, NS32K */
#define	BIG_ENDIAN	4321	/* MSB first: M68K */
#define	PDP_ENDIAN	3412	/* LSB first in word, MSW first in long */

#define	BYTE_ORDER	BIG_ENDIAN

#include <sys/cdefs.h>

typedef u_int32_t in_addr_t;                      
typedef u_int16_t in_port_t;

__BEGIN_DECLS
u_int32_t       htonl __P((u_int32_t));
u_int16_t       htons __P((u_int16_t));
u_int32_t       ntohl __P((u_int32_t));
u_int16_t       ntohs __P((u_int16_t));
__END_DECLS

/*
 * Macros for network/external number representation conversion where
 * network/external is defined to be in BIG_ENDIAN byte order.
 *
 * *NOTE* That the macros are supposed to work on the arrgument (x) and
 * thus should *NOT* be used in assignments such as 'foo=HTONS(bar)'.
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

#define	NTOHL(x)	(x) = ntohl((u_int32_t)x)
#define	NTOHS(x)	(x) = ntohs((u_int16_t)x)
#define	HTONL(x)	(x) = htonl((u_int32_t)x)
#define	HTONS(x)	(x) = htons((u_int16_t)x)
#endif

#endif	/* _POSIX_SOURCE */
#endif	/* _MACHINE_ENDIAN_H_ */
