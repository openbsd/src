/*	$OpenBSD: types.h,v 1.8 2005/12/14 21:46:31 millert Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)types.h	8.3 (Berkeley) 1/5/94
 */

#ifndef	_MIPS_TYPES_H_
#define	_MIPS_TYPES_H_

/*
 *  We need to handle the various ISA levels for sizes.
 */
#define _MIPS_ISA_MIPS1 1       /* R2000/R3000 */
#define _MIPS_ISA_MIPS2 2       /* R4000/R6000 */
#define _MIPS_ISA_MIPS3 3       /* R4000 */
#define _MIPS_ISA_MIPS4 4       /* TFP (R1x000) */

#include <sys/cdefs.h>

#if __BSD_VISIBLE
typedef unsigned long	vaddr_t;
typedef unsigned long	paddr_t;
typedef unsigned long	vsize_t;
typedef unsigned long	psize_t;
#endif

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

#if (_MIPS_ISA == _MIPS_ISA_MIPS3 || _MIPS_ISA == _MIPS_ISA_MIPS4)
typedef int64_t		       register_t;
typedef int64_t		     f_register_t;
#else
typedef int32_t		       register_t;
typedef int32_t		     f_register_t;
#endif

#if defined(_KERNEL)
typedef struct label_t {
	register_t val[14];
} label_t;
#endif

/* XXX check why this still has to be defined. pmap.c issue? */
#define  __SWAP_BROKEN

#define __HAVE_TIMECOUNTER

#endif	/* !_MIPS_TYPES_H_ */
