/*	$OpenBSD: stdarg.h,v 1.7 2005/12/14 21:46:30 millert Exp $	*/
/*	$NetBSD: stdarg.h,v 1.4 1996/10/09 21:13:05 cgd Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)stdarg.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _ALPHA_STDARG_H_
#define	_ALPHA_STDARG_H_

#include <sys/cdefs.h>
#include <machine/ansi.h>

typedef _BSD_VA_LIST_	va_list;

#define	__va_size(type) \
	(((sizeof(type) + sizeof(long) - 1) / sizeof(long)) * sizeof(long))

#define	va_start(ap, last) \
	(__builtin_next_arg(last), (ap) = *(va_list *)__builtin_saveregs(), (ap).pad = 0)

#define	__REAL_TYPE_CLASS	8
#define	__va_arg_offset(ap, type)					\
	((__builtin_classify_type(*(type *)0) == __REAL_TYPE_CLASS &&	\
	    (ap).offset <= (6 * 8) ? -(6 * 8) : 0) - __va_size(type))

#define	va_arg(ap, type)						\
	(*(type *)((ap).offset += __va_size(type),			\
		   (ap).base + (ap).offset + __va_arg_offset(ap, type)))

#if __ISO_C_VISIBLE >= 1999
#define va_copy(dest, src) \
	((dest) = (src))
#endif

#define	va_end(ap)	((void)0)

#endif /* !_ALPHA_STDARG_H_ */
