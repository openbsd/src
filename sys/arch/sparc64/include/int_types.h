/*	$OpenBSD: int_types.h,v 1.2 2001/09/26 17:32:19 deraadt Exp $	*/
/*	$NetBSD: int_types.h,v 1.7 2001/04/28 15:41:33 kleink Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	from: @(#)types.h	8.1 (Berkeley) 6/11/93
 */

#ifndef	_SPARC64_INT_TYPES_H_
#define	_SPARC64_INT_TYPES_H_

/*
 * 7.18.1 Integer types
 */

/* 7.18.1.1 Exact-width integer types */

typedef	__signed char		 int8_t;
typedef	unsigned char		uint8_t;
typedef	unsigned char	       u_int8_t;
typedef	short int		int16_t;
typedef	unsigned short int     uint16_t;
typedef	unsigned short int    u_int16_t;
typedef	int			int32_t;
typedef	unsigned int	       uint32_t;
typedef	unsigned int	      u_int32_t;

#ifdef __COMPILER_INT64__
typedef	__COMPILER_INT64__	int64_t;
typedef	__COMPILER_UINT64__    uint64_t;
typedef	__COMPILER_UINT64__   u_int64_t;
#else
#ifdef __arch64__
/* 64-bit compiler */
typedef	long long int		int64_t;
typedef	unsigned long long int	uint64_t;
typedef	unsigned long long int	u_int64_t;
#else
/* 32-bit compiler */
/* LONGLONG */
typedef	long long int		int64_t;
/* LONGLONG */
typedef	unsigned long long int uint64_t;
typedef	unsigned long long int u_int64_t;
#endif
#endif /* !__COMPILER_INT64__ */

#define	__BIT_TYPES_DEFINED__

/* 7.18.1.4 Integer types capable of holding object pointers */

typedef	long int	       __intptr_t;
typedef	unsigned long int     __uintptr_t;

#endif	/* !_SPARC64_INT_TYPES_H_ */
