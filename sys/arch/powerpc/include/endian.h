/*	$NetBSD: endian.h,v 1.2 1996/10/13 03:16:41 christos Exp $	*/

/*-
 * Copyright (C) 1995 Wolfgang Solfrank.
 * Copyright (C) 1995 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_PPC_ENDIAN_H_
#define	_PPC_ENDIAN_H_

#ifndef	_POSIX_SOURCE

#define _QUAD_HIGHWORD	0
#define _QUAD_LOWWORD	1

/*
 * Definitions for byte order, according to byte significance from low
 * address to high.
 */
#define	LITTLE_ENDIAN	1234	/* LSB first: i386, vax */
#define	BIG_ENDIAN	4321	/* MSB first: 68000, ibm, net */
#define	PDP_ENDIAN	3412	/* LSB first in word, MSW first in long */

#define	BYTE_ORDER	BIG_ENDIAN	/* for now */

#include <sys/cdefs.h>

__BEGIN_DECLS
u_int32_t       htonl __P((u_int32_t));
u_int16_t       htons __P((u_int16_t));
u_int32_t       ntohl __P((u_int32_t));
u_int16_t       ntohs __P((u_int16_t));
__END_DECLS

/*
 * Macros for network/external number representation conversion.
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

#endif	/* _POSIX_SOURCE */
#endif	/* _PPC_ENDIAN_H_ */
