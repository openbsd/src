/*	$OpenBSD: endian.h,v 1.1 1997/11/09 23:04:58 niklas Exp $	*/

/*-
 * Copyright (c) 1997 Niklas Hallqvist.  All rights reserved.
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
 *	This product includes software developed by Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * Generic definitions for little- and big-endian systems.  Other endianesses
 * has to be dealt with in the specific machine/endian.h file for that port.
 *
 * This file is meant to be included from a little- or big-endian port's
 * machine/endian.h after setting BYTE_ORDER to either 1234 for little endian
 * or 4321 for big..
 */

#ifndef _SYS_ENDIAN_H_
#define _SYS_ENDIAN_H_

#ifndef _POSIX_SOURCE

#include <sys/cdefs.h>

#define LITTLE_ENDIAN	1234


#define BIG_ENDIAN	4321
#define PDP_ENDIAN	3412

typedef u_int32_t in_addr_t;
typedef u_int16_t in_port_t;

#ifdef __GNUC__

#define __swap16gen(x) ({ u_int16_t y = (x); (u_int16_t)(y << 8 | y >> 8); })
#define __swap32gen(x) ({						\
	u_int32_t y = (x);						\
									\
	(u_int32_t)(y << 24 | (y & 0xff00) << 8 | (y & 0xff0000) >> 8 |	\
	    y >> 24);							\
})

#else /* __GNUC__ */

/* Note that these macros evaluates their arguments several times.  */
#define __swap16gen(x) (u_int16_t)((u_int16_t)(x) << 8 | (u_int16_t)(x) >> 8)
#define __swap32gen(x) \
    (u_int32_t)((u_int32_t)(x) << 24 | ((u_int32_t)(x) & 0xff00) << 8 | \
    ((u_int32_t)(x) & 0xff0000) >> 8 | (u_int32_t)(x) >> 24)

#endif /* __GNUC__ */

/*
 * Define MD_SWAP if you provide swap{16,32}md functions/macros that are
 * optimized for your architecture,  These will be used for swap{16,32}
 * unless the argument is a constant and we are using GCC, where we can
 * take advantage of the CSE phase much better by using the generic version.
 */
#ifdef MD_SWAP
#if __GNUC__

#define swap16(x) ({							\
	u_int16_t __x = (x);						\
									\
	__builtin_constant_p(x) ? __swap16gen(__x) : __swap16md(__x);	\
})

#define swap32(x) ({							\
	u_int32_t __x = (x);						\
									\
	__builtin_constant_p(x) ? __swap32gen(__x) : __swap32md(__x);	\
})

#endif /* __GNUC__  */

#else /* MD_SWAP */
#define swap16 __swap16gen
#define swap32 __swap32gen
#endif /* MD_SWAP */

__BEGIN_DECLS
u_int32_t	htobe32 __P((u_int32_t));
u_int16_t	htobe16 __P((u_int16_t));
u_int32_t	betoh32 __P((u_int32_t));
u_int16_t	betoh16 __P((u_int16_t));

u_int32_t	htole32 __P((u_int32_t));
u_int16_t	htole16 __P((u_int16_t));
u_int32_t	letoh32 __P((u_int32_t));
u_int16_t	letoh16 __P((u_int16_t));
__END_DECLS

#if BYTE_ORDER == LITTLE_ENDIAN

#define _QUAD_HIGHWORD 1
#define _QUAD_LOWWORD 0

#define htobe16 swap16
#define htobe32 swap32
#define betoh16 swap16
#define betoh32 swap32

#define htole16(x) (x)
#define htole32(x) (x)
#define letoh16(x) (x)
#define letoh32(x) (x)

#endif /* BYTE_ORDER */

#if BYTE_ORDER == BIG_ENDIAN

#define _QUAD_HIGHWORD 0
#define _QUAD_LOWWORD 1

#define htole16 swap16
#define htole32 swap32
#define letoh16 swap16
#define letoh32 swap32

#define htobe16(x) (x)
#define htobe32(x) (x)
#define betoh16(x) (x)
#define betoh32(x) (x)

#endif /* BYTE_ORDER */

#define htons htobe16
#define htonl htobe32
#define ntohs betoh16
#define ntohl betoh32

#define	NTOHL(x) (x) = ntohl((u_int32_t)(x))
#define	NTOHS(x) (x) = ntohs((u_int16_t)(x))
#define	HTONL(x) (x) = htonl((u_int32_t)(x))
#define	HTONS(x) (x) = htons((u_int16_t)(x))

#endif /* _POSIX_SOURCE */
#endif /* _SYS_ENDIAN_H_ */
