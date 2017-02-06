/* $OpenBSD: endian.h,v 1.2 2017/02/06 04:08:57 dlg Exp $ */

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

#define __swap32md(x) __statement({                                     \
        __uint32_t __swap32md_x; 	                                 \
                                                                        \
        __asm ("rev %w0, %w1" : "=r" (__swap32md_x) : "r"(x));          \
        __swap32md_x;                                                   \
})

#define __swap64md(x) __statement({                                     \
        __uint64_t __swap64md_x;					\
                                                                        \
        __asm ("rev %x0, %x1" : "=r" (__swap64md_x) : "r"(x));          \
        __swap64md_x;                                                   \
})

#define __swap16md(x) __statement({                                     \
        __uint16_t __swap16md_x;					\
                                                                        \
        __asm ("rev16 %w0, %w1" : "=r" (__swap16md_x) : "r"(x));        \
        __swap16md_x;                                                   \
})

/* Tell sys/endian.h we have MD variants of the swap macros.  */
#define __HAVE_MD_SWAP


#define _BYTE_ORDER _LITTLE_ENDIAN
#define	__STRICT_ALIGNMENT

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif
#endif /* _MACHINE_ENDIAN_H_ */
