/*	$OpenBSD: endian.h,v 1.11 2018/10/02 21:30:44 naddy Exp $	*/

/*
 * Copyright (c) 2015 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _ARM_ENDIAN_H_
#define _ARM_ENDIAN_H_

#ifndef __FROM_SYS__ENDIAN
#include <sys/_types.h>
#endif

static __inline __uint16_t
__swap16md(__uint16_t _x)
{
	__uint16_t _rv;

	__asm ("rev16 %0, %1" : "=r" (_rv) : "r" (_x));

	return (_rv);
}

static __inline __uint32_t
__swap32md(__uint32_t _x)
{
	__uint32_t _rv;

	__asm ("rev %0, %1" : "=r" (_rv) : "r" (_x));

	return (_rv);
}

static __inline __uint64_t
__swap64md(__uint64_t _x)
{
	__uint64_t _rv;

	_rv = (__uint64_t)__swap32md(_x >> 32) |
	    (__uint64_t)__swap32md(_x) << 32;

	return (_rv);
}

/* Tell sys/endian.h we have MD variants of the swap macros.  */
#define __HAVE_MD_SWAP

#define _BYTE_ORDER _LITTLE_ENDIAN
#define	__STRICT_ALIGNMENT

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif
#endif /* _ARM_ENDIAN_H_ */
