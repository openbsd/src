/*	$OpenBSD: endian.h,v 1.15 2011/03/12 04:03:04 guenther Exp $	*/

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

#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_

#ifdef	__GNUC__

#define	__swap64md	__swap64gen

#define	__swap32md(x) __statement({					\
	u_int32_t __swap32md_y, __swap32md_x = (x);			\
									\
	__asm ("rotl $-8, %1, %0; insv %0, $16, $8, %0; "		\
	    "rotl $8, %1, r1; movb r1, %0" :				\
	    "&=r" (__swap32md_y) : "r" (__swap32md_x) : "r1", "cc");	\
	__swap32md_y;							\
})

#define __swap16md(x) __statement({					\
	u_int16_t __swap16md_y, __swap16md_x = (x);			\
									\
	__asm ("rotl $8, %1, %0; rotl $-8, %1, r1; movb r1, %0; "	\
	    "movzwl %0, %0" :						\
	    "&=r" (__swap16md_y) : "r" (__swap16md_x) : "r1", "cc");	\
	__swap16md_y;							\
})

/* Tell sys/endian.h we have MD variants of the swap macros.  */
#ifdef notyet
#define MD_SWAP
#endif

#endif /* __GNUC__ */

#define _BYTE_ORDER _LITTLE_ENDIAN
#include <sys/endian.h>

#endif /* _MACHINE_ENDIAN_H_ */
