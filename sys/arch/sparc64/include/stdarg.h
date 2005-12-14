/*	$OpenBSD: stdarg.h,v 1.4 2005/12/14 21:46:31 millert Exp $	*/
/*	$NetBSD: stdarg.h,v 1.11 2000/07/23 21:36:56 mycroft Exp $ */

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
 *	from: @(#)stdarg.h	8.2 (Berkeley) 9/27/93
 */

#ifndef _SPARC64_STDARG_H_
#define	_SPARC64_STDARG_H_

#include <sys/cdefs.h>
#include <machine/ansi.h>

#ifdef __lint__
#define	__builtin_saveregs(t)		(0)
#define	__builtin_classify_type(t)	(0)
#define	__builtin_next_arg(t)		((t) ? 0 : 0)
#endif

typedef _BSD_VA_LIST_	va_list;

#define	va_start(ap, last) \
	(__builtin_next_arg(last), (ap) = (va_list)__builtin_saveregs())

#if __BSD_VISIBLE
#define	va_copy(dest, src) \
	((dest) = (src))
#endif

#define va_end(ap)	

/*
 * For sparcv9 code.
 */
#define	__va_arg8(ap, type) \
	(*(type *)(void *)((ap) += 8, (ap) - 8))
#define	__va_arg16(ap, type) \
	(*(type *)(void *)((ap) = (va_list)(((unsigned long)(ap) + 31) & -16),\
			   (ap) - 16))
#define	__va_int(ap, type) \
	(*(type *)(void *)((ap) += 8, (ap) - sizeof(type)))

#define	__REAL_TYPE_CLASS	8
#define	__RECORD_TYPE_CLASS	12
#define va_arg(ap, type) \
	(__builtin_classify_type(*(type *)0) == __REAL_TYPE_CLASS ?	\
	 (__alignof__(type) == 16 ? __va_arg16(ap, type) :		\
	  __va_arg8(ap, type)) :					\
	 (__builtin_classify_type(*(type *)0) < __RECORD_TYPE_CLASS ?	\
	  __va_int(ap, type) :						\
	  (sizeof(type) <= 8 ? __va_arg8(ap, type) :			\
	   (sizeof(type) <= 16 ? __va_arg16(ap, type) :			\
	    *__va_arg8(ap, type *)))))

#endif /* !_SPARC64_STDARG_H_ */
