/*	$OpenBSD: endian.h,v 1.7 1997/08/08 18:27:17 niklas Exp $	*/
/*	$NetBSD: endian.h,v 1.3 1996/10/13 19:57:59 cgd Exp $	*/

/*
 * Copyright (c) 1987, 1991, 1993
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
 *	@(#)endian.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _ENDIAN_H_
#define	_ENDIAN_H_

/*
 * Define the order of 32-bit words in 64-bit words.
 */
#define _QUAD_HIGHWORD 1
#define _QUAD_LOWWORD 0

#ifndef _POSIX_SOURCE
/*
 * Definitions for byte order, according to byte significance from low
 * address to high.
 */
#define	LITTLE_ENDIAN	1234	/* LSB first: i386, vax */
#define	BIG_ENDIAN	4321	/* MSB first: 68000, ibm, net */
#define	PDP_ENDIAN	3412	/* LSB first in word, MSW first in long */

#define	BYTE_ORDER	LITTLE_ENDIAN

#include <sys/cdefs.h>

typedef u_int32_t in_addr_t;
typedef u_int16_t in_port_t;

__BEGIN_DECLS
#if 0
/*
 * prototypes supplied for documentation purposes solely
 */
u_int32_t	htobe32 __P((u_int32_t));
u_int16_t	htobe16 __P((u_int16_t));
u_int32_t	betoh32 __P((u_int32_t));
u_int16_t	betoh16 __P((u_int16_t));

u_int32_t	htole32 __P((u_int32_t));
u_int16_t	htole16 __P((u_int16_t));
u_int32_t	letoh32 __P((u_int32_t));
u_int16_t	letoh16 __P((u_int16_t));

u_int32_t	htonl __P((u_int32_t));
u_int16_t	htons __P((u_int16_t));
u_int32_t	ntohl __P((u_int32_t));
u_int16_t	ntohs __P((u_int16_t));
#endif
__END_DECLS

#ifdef __GNUC__

/* XXX perhaps some __asm would be better here?  */
#define	__byte_swap_int32_variable(x) \
({ register u_int32_t __x = (x); \
    (((__x) & 0xff000000) >> 24) | (((__x) & 0x00ff0000) >> 8) | \
    (((__x) & 0x0000ff00) << 8) | (((__x) & 0x000000ff) << 24); })

#define	__byte_swap_int16_variable(x) \
({ register u_int16_t __x = (x); \
    (((__x) & 0xff00) >> 8) | (((__x) & 0x00ff) << 8); })

#ifdef __OPTIMIZE__

#define	__byte_swap_int32_constant(x) \
	((((x) & 0xff000000) >> 24) | \
	 (((x) & 0x00ff0000) >>  8) | \
	 (((x) & 0x0000ff00) <<  8) | \
	 (((x) & 0x000000ff) << 24))
#define	__byte_swap_int16_constant(x) \
	((((x) & 0xff00) >> 8) | \
	 (((x) & 0x00ff) << 8))
#define	__byte_swap_int32(x) \
	(__builtin_constant_p((x)) ? \
	 __byte_swap_int32_constant(x) : __byte_swap_int32_variable(x))
#define	__byte_swap_int16(x) \
	(__builtin_constant_p((x)) ? \
	 __byte_swap_int16_constant(x) : __byte_swap_int16_variable(x))

#else /* __OPTIMIZE__ */

#define	__byte_swap_int32(x)	__byte_swap_int32_variable(x)
#define	__byte_swap_int16(x)	__byte_swap_int16_variable(x)

#endif /* __OPTIMIZE__ */
#endif /* __GNUC__ */

/*
 * Macros for big/little endian to host and vice versa.
 */
#define	betoh32(x)	__byte_swap_int32(x)
#define	betoh16(x)	__byte_swap_int16(x)
#define	htobe32(x)	__byte_swap_int32(x)
#define	htobe16(x)	__byte_swap_int16(x)

#define	letoh32(x)	(x)
#define	letoh16(x)	(x)
#define	htole32(x)	(x)
#define	htole16(x)	(x)

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
#define	ntohl(x)	betoh32(x)
#define	ntohs(x)	betoh16(x)
#define	htonl(x)	htobe32(x)
#define	htons(x)	htobe16(x)

#define	NTOHL(x)	(void)((x) = ntohl(x))
#define	NTOHS(x)	(void)((x) = ntohs(x))
#define	HTONL(x)	(void)((x) = htonl(x))
#define	HTONS(x)	(void)((x) = htons(x))

#endif /* !_POSIX_SOURCE */
#endif /* !_ENDIAN_H_ */
