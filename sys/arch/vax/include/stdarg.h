/*	$OpenBSD: stdarg.h,v 1.10 2008/10/23 21:25:08 kettenis Exp $	*/
/*	$NetBSD: stdarg.h,v 1.11 1999/05/03 16:30:34 christos Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *      @(#)stdarg.h    7.2 (Berkeley) 5/4/91
 */

#ifndef _VAX_STDARG_H_
#define	_VAX_STDARG_H_

#include <sys/cdefs.h>
#include <machine/_types.h>

typedef __va_list	va_list;

#ifdef __lint__
#define __builtin_next_arg(t)		((t) ? 0 : 0)
#endif

#define	__va_size(type) \
	(((sizeof(type) + sizeof(long) - 1) / sizeof(long)) * sizeof(long))

#ifdef lint
#define	va_start(ap,lastarg)	((ap) = (ap))
#else
#define va_start(ap, last) \
	((ap) = (va_list)__builtin_next_arg(last))
#endif /* lint */

#define	va_arg(ap, type) \
	(*(type *)(void *)((ap) += __va_size(type), (ap) - __va_size(type)))

#if __BSD_VISIBLE
#define __va_copy(dest, src) \
	((dest) = (src))
#endif

#define va_end(ap)	

#endif /* !_VAX_STDARG_H_ */
