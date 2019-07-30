/*	$OpenBSD: endian.h,v 1.10 2017/02/06 03:52:35 dlg Exp $	*/

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

#ifdef _KERNEL
#ifdef __GNUC__

static inline __uint16_t
___swap16md(__uint16_t x)
{
	__uint16_t rv;

	__asm ("rev16 %0, %1" : "=r" (rv) : "r" (x));

	return (rv);
}
#define __swap16md(x) ___swap16md(x)

static inline __uint32_t
___swap32md(__uint32_t x)
{
	__uint32_t rv;

	__asm ("rev %0, %1" : "=r" (rv) : "r" (x));

	return (rv);
}
#define __swap32md(x) ___swap32md(x)

static inline __uint64_t
___swap64md(__uint64_t x)
{
	__uint64_t rv;

	rv = (__uint64_t)__swap32md(x >> 32) |
	    (__uint64_t)__swap32md(x) << 32;

	return (rv);
}
#define __swap64md(x) ___swap64md(x)

/* Tell sys/endian.h we have MD variants of the swap macros.  */
#define __HAVE_MD_SWAP

#endif  /* __GNUC__ */
#endif  /* _KERNEL */

#define _BYTE_ORDER _LITTLE_ENDIAN
#define	__STRICT_ALIGNMENT

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif
#endif /* _ARM_ENDIAN_H_ */
