/*	$NetBSD: varargs.h,v 1.2 1995/02/16 03:08:11 cgd Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)varargs.h	8.2 (Berkeley) 3/22/94
 */

#ifndef _VARARGS_H_
#define	_VARARGS_H_

#include <machine/ansi.h>

typedef _BSD_VA_LIST_	va_list;

/*
 * Note that these macros are significantly different than the 'standard'
 * ones.  On the alpha, all arguments are passed as 64 bit quantities.
 */

#define	va_alist	__builtin_va_alist
#define	va_dcl		va_list __builtin_va_alist; ...

#define	va_start(a) \
	((a) = *(va_list *)__builtin_saveregs())

#define	__va_size(type) \
	(((sizeof(type) + sizeof(long) - 1) / sizeof(long)) * sizeof(long))

#define	__REAL_TYPE_CLASS	8
#define	__va_arg_offset(a, type)					\
	((__builtin_classify_type(*(type *)0) == __REAL_TYPE_CLASS &&	\
	    (a).offset <= (6 * 8) ? -(6 * 8) : 0) - __va_size(type))

#define	va_arg(a, type)							\
	(*((a).offset += __va_size(type),				\
	    (type *)((a).base + (a).offset + __va_arg_offset(a, type))))

#define	va_end(a)	((void) 0)

#endif /* !_VARARGS_H_ */
