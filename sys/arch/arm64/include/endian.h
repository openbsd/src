/* $OpenBSD: endian.h,v 1.3 2018/10/01 17:42:16 naddy Exp $ */

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

#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_

#ifndef __FROM_SYS__ENDIAN
#include <sys/_types.h>
#endif

static __inline __uint16_t
__swap16md(__uint16_t _x)
{
        __uint16_t _rv;

        __asm ("rev16 %w0, %w1" : "=r" (_rv) : "r"(_x));

	return (_rv);
}

static __inline __uint32_t
__swap32md(__uint32_t _x)
{
        __uint32_t _rv;

        __asm ("rev %w0, %w1" : "=r" (_rv) : "r"(_x));

        return (_rv);
}

static __inline __uint64_t
__swap64md(__uint64_t _x)
{
        __uint64_t _rv;

        __asm ("rev %x0, %x1" : "=r" (_rv) : "r"(_x));

        return (_rv);
}

/* Tell sys/endian.h we have MD variants of the swap macros.  */
#define __HAVE_MD_SWAP


#define _BYTE_ORDER _LITTLE_ENDIAN
#define	__STRICT_ALIGNMENT

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif
#endif /* _MACHINE_ENDIAN_H_ */
