/*	$OpenBSD: endian.h,v 1.21 2014/10/22 23:56:47 dlg Exp $ */

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

#ifndef _POWERPC_ENDIAN_H_
#define _POWERPC_ENDIAN_H_

#ifdef _KERNEL

static inline __uint16_t
__mswap16(volatile const __uint16_t *m)
{
	__uint16_t v;

	__asm("lhbrx %0, 0, %1"
	    : "=r" (v)
            : "r" (m), "m" (*m));

	return (v);
}

static inline __uint32_t
__mswap32(volatile const __uint32_t *m)
{
	__uint32_t v;

	__asm("lwbrx %0, 0, %1"
	    : "=r" (v)
            : "r" (m), "m" (*m));

	return (v);
}

static inline __uint64_t
__mswap64(volatile const __uint64_t *m)
{
	__uint32_t *a = (__uint32_t *)m;
	__uint64_t v;

	v = (__uint64_t)__mswap32(a + 1) << 32 |
	    (__uint64_t)__mswap32(a);

	return (v);
}

static inline void
__swapm16(volatile __uint16_t *m, __uint16_t v)
{
	__asm("sthbrx %1, 0, %2"
	    : "=m" (*m)
	    : "r" (v), "r" (m));
}

static inline void
__swapm32(volatile __uint32_t *m, __uint32_t v)
{
	__asm("stwbrx %1, 0, %2"
	    : "=m" (*m)
	    : "r" (v), "r" (m));
}

static inline void
__swapm64(volatile __uint64_t *m, __uint64_t v)
{
	__uint32_t *a = (__uint32_t *)m;

	__swapm32(a + 1, v >> 32);
	__swapm32(a, v);
}

#define __HAVE_MD_SWAPIO
#endif /* _KERNEL */

#undef _BIG_ENDIAN	/* XXX - gcc may define _BIG_ENDIAN too */
#define _BYTE_ORDER _BIG_ENDIAN

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif

#endif /* _POWERPC_ENDIAN_H_ */
