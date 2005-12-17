/*	$OpenBSD: types.h,v 1.5 2005/12/17 07:31:25 miod Exp $	*/
/*	$NetBSD: types.h,v 1.4 2002/02/28 03:17:25 simonb Exp $	*/

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)types.h	7.5 (Berkeley) 3/9/91
 */

#ifndef	_ARM_TYPES_H_
#define	_ARM_TYPES_H_

/* OpenBSD only supports arm32 */
#ifdef _KERNEL
#define	__PROG32		/* indicate 32-bit mode */
#endif

#include <sys/cdefs.h>

#if defined(_KERNEL)
typedef struct label_t {	/* Used by setjmp & longjmp */
        int val[11];
} label_t;
#endif
         
/* NB: This should probably be if defined(_KERNEL) */
#if __BSD_VISIBLE
typedef unsigned long	paddr_t;
typedef unsigned long	psize_t;
typedef unsigned long	vaddr_t;
typedef unsigned long	vsize_t;
#endif

#define	__HAVE_MINIMAL_EMUL

/*
 * Basic integral types.  Omit the typedef if
 * not possible for a machine/compiler combination.
 */
#define	__BIT_TYPES_DEFINED__
typedef	__signed char		   int8_t;
typedef	unsigned char		 u_int8_t;
typedef	unsigned char		  uint8_t;
typedef	short			  int16_t;
typedef	unsigned short		u_int16_t;
typedef	unsigned short		 uint16_t;
typedef	int			  int32_t;
typedef	unsigned int		u_int32_t;
typedef	unsigned int		 uint32_t;
/* LONGLONG */
typedef	long long		  int64_t;
/* LONGLONG */
typedef	unsigned long long	u_int64_t;
/* LONGLONG */
typedef	unsigned long long	 uint64_t;

typedef int32_t			register_t;

/*
 * 7.18.1 Integer types
 */

/* 7.18.1.1 Exact-width integer types */

typedef	__signed char		 __int8_t;
typedef	unsigned char		__uint8_t;
typedef	short int		__int16_t;
typedef	unsigned short int     __uint16_t;
typedef	int			__int32_t;
typedef	unsigned int	       __uint32_t;
#ifdef __COMPILER_INT64__
typedef	__COMPILER_INT64__	__int64_t;
typedef	__COMPILER_UINT64__    __uint64_t;
#else
/* LONGLONG */
typedef	long long int		__int64_t;
/* LONGLONG */
typedef	unsigned long long int __uint64_t;
#endif


/* 7.18.1.4 Integer types capable of holding object pointers */

typedef long int	       __intptr_t;
typedef unsigned long int     __uintptr_t;

#endif	/* _ARM_TYPES_H_ */
