/*	$OpenBSD: endian.h,v 1.14 2004/01/11 19:17:31 brad Exp $	*/

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

#ifdef __GNUC__

#define __swap16gen(x) __extension__({					\
	u_int16_t __swap16gen_x = (x);					\
									\
	(u_int16_t)((__swap16gen_x & 0xff) << 8 |			\
	    (__swap16gen_x & 0xff00) >> 8);				\
})

#define __swap32gen(x) __extension__({					\
	u_int32_t __swap32gen_x = (x);					\
									\
	(u_int32_t)((__swap32gen_x & 0xff) << 24 |			\
	    (__swap32gen_x & 0xff00) << 8 |				\
	    (__swap32gen_x & 0xff0000) >> 8 |				\
	    (__swap32gen_x & 0xff000000) >> 24);			\
})

#define __swap64gen(x) __extension__({					\
	u_int64_t __swap64gen_x = (x);					\
									\
	(u_int64_t)((__swap64gen_x & 0xff) << 56 |			\
	    (__swap64gen_x & 0xff00ULL) << 40 |				\
	    (__swap64gen_x & 0xff0000ULL) << 24 |			\
	    (__swap64gen_x & 0xff000000ULL) << 8 |			\
	    (__swap64gen_x & 0xff00000000ULL) >> 8 |			\
	    (__swap64gen_x & 0xff0000000000ULL) >> 24 |			\
	    (__swap64gen_x & 0xff000000000000ULL) >> 40 |		\
	    (__swap64gen_x & 0xff00000000000000ULL) >> 56);		\
})

#else /* __GNUC__ */

/* Note that these macros evaluate their arguments several times.  */
#define __swap16gen(x)							\
    (u_int16_t)(((u_int16_t)(x) & 0xff) << 8 | ((u_int16_t)(x) & 0xff00) >> 8)

#define __swap32gen(x)							\
    (u_int32_t)(((u_int32_t)(x) & 0xff) << 24 |				\
    ((u_int32_t)(x) & 0xff00) << 8 | ((u_int32_t)(x) & 0xff0000) >> 8 |	\
    ((u_int32_t)(x) & 0xff000000) >> 24)

#define __swap64gen(x)							\
	(u_int64_t)((((u_int64_t)(x) & 0xff) << 56) |			\
	    ((u_int64_t)(x) & 0xff00ULL) << 40 |			\
	    ((u_int64_t)(x) & 0xff0000ULL) << 24 |			\
	    ((u_int64_t)(x) & 0xff000000ULL) << 8 |			\
	    ((u_int64_t)(x) & 0xff00000000ULL) >> 8 |			\
	    ((u_int64_t)(x) & 0xff0000000000ULL) >> 24 |		\
	    ((u_int64_t)(x) & 0xff000000000000ULL) >> 40 |		\
	    ((u_int64_t)(x) & 0xff00000000000000ULL) >> 56)

#endif /* __GNUC__ */

/*
 * Define MD_SWAP if you provide swap{16,32}md functions/macros that are
 * optimized for your architecture,  These will be used for swap{16,32}
 * unless the argument is a constant and we are using GCC, where we can
 * take advantage of the CSE phase much better by using the generic version.
 */
#ifdef MD_SWAP
#if __GNUC__

#define swap16(x) __extension__({					\
	u_int16_t __swap16_x = (x);					\
									\
	__builtin_constant_p(x) ? __swap16gen(__swap16_x) :		\
	    __swap16md(__swap16_x);					\
})

#define swap32(x) __extension__({					\
	u_int32_t __swap32_x = (x);					\
									\
	__builtin_constant_p(x) ? __swap32gen(__swap32_x) :		\
	    __swap32md(__swap32_x);					\
})

#define swap64(x) __extension__({					\
	u_int64_t __swap64_x = (x);					\
									\
	__builtin_constant_p(x) ? __swap64gen(__swap64_x) :		\
	    __swap64md(__swap64_x);					\
})

#endif /* __GNUC__  */

#else /* MD_SWAP */
#define swap16 __swap16gen
#define swap32 __swap32gen
#define swap64 __swap64gen
#endif /* MD_SWAP */

#define swap16_multi(v, n) do {						\
	size_t __swap16_multi_n = (n);					\
	u_int16_t *__swap16_multi_v = (v);				\
									\
	while (__swap16_multi_n) {					\
		*__swap16_multi_v = swap16(*__swap16_multi_v);		\
		__swap16_multi_v++;					\
		__swap16_multi_n--;					\
	}								\
} while (0)

__BEGIN_DECLS
u_int64_t	htobe64(u_int64_t);
u_int32_t	htobe32(u_int32_t);
u_int16_t	htobe16(u_int16_t);
u_int64_t	betoh64(u_int64_t);
u_int32_t	betoh32(u_int32_t);
u_int16_t	betoh16(u_int16_t);

u_int64_t	htole64(u_int64_t);
u_int32_t	htole32(u_int32_t);
u_int16_t	htole16(u_int16_t);
u_int64_t	letoh64(u_int64_t);
u_int32_t	letoh32(u_int32_t);
u_int16_t	letoh16(u_int16_t);
__END_DECLS

#if BYTE_ORDER == LITTLE_ENDIAN

/* Can be overridden by machine/endian.h before inclusion of this file.  */
#ifndef _QUAD_HIGHWORD
#define _QUAD_HIGHWORD 1
#endif
#ifndef _QUAD_LOWWORD
#define _QUAD_LOWWORD 0
#endif

#define htobe16 swap16
#define htobe32 swap32
#define htobe64 swap64
#define betoh16 swap16
#define betoh32 swap32
#define betoh64 swap64

#define htole16(x) (x)
#define htole32(x) (x)
#define htole64(x) (x)
#define letoh16(x) (x)
#define letoh32(x) (x)
#define letoh64(x) (x)

#endif /* BYTE_ORDER */

#if BYTE_ORDER == BIG_ENDIAN

/* Can be overridden by machine/endian.h before inclusion of this file.  */
#ifndef _QUAD_HIGHWORD
#define _QUAD_HIGHWORD 0
#endif
#ifndef _QUAD_LOWWORD
#define _QUAD_LOWWORD 1
#endif

#define htole16 swap16
#define htole32 swap32
#define htole64 swap64
#define letoh16 swap16
#define letoh32 swap32
#define letoh64 swap64

#define htobe16(x) (x)
#define htobe32(x) (x)
#define htobe64(x) (x)
#define betoh16(x) (x)
#define betoh32(x) (x)
#define betoh64(x) (x)

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
