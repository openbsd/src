/*	$OpenBSD: limits.h,v 1.5 1998/03/22 21:15:24 millert Exp $	*/
/*	$NetBSD: limits.h,v 1.7 1996/01/05 18:10:57 pk Exp $ */

/*
 * Copyright (c) 1988 The Regents of the University of California.
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
 *	@(#)limits.h	8.3 (Berkeley) 1/4/94
 */

#define	CHAR_BIT	8		/* number of bits in a char */
#define	MB_LEN_MAX	1		/* no multibyte characters */

#define	SCHAR_MIN	(-0x7f-1)	/* max value for a signed char */
#define	SCHAR_MAX	0x7f		/* min value for a signed char */

#define	UCHAR_MAX	0xffU		/* max value for an unsigned char */
#define	CHAR_MAX	0x7f		/* max value for a char */
#define	CHAR_MIN	(-0x7f-1)	/* min value for a char */

#define	USHRT_MAX	0xffffU		/* max value for an unsigned short */
#define	SHRT_MAX	0x7fff		/* max value for a short */
#define	SHRT_MIN	(-0x7fff-1)	/* min value for a short */

#define	UINT_MAX	0xffffffffU	/* max value for an unsigned int */
#define	INT_MAX		0x7fffffff	/* max value for an int */
#define	INT_MIN		(-0x7fffffff-1)	/* min value for an int */

#define	ULONG_MAX	0xffffffffUL	/* max value for an unsigned long */
#define	LONG_MAX	0x7fffffffL	/* max value for a long */
#define	LONG_MIN	(-0x7fffffffL-1)	/* min value for a long */

#if !defined(_ANSI_SOURCE)
#define	SSIZE_MAX	INT_MAX		/* max value for a ssize_t */

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
#define	SIZE_T_MAX	UINT_MAX	/* max value for a size_t */

#define	UID_MAX		UINT_MAX	/* max value for a uid_t */
#define	GID_MAX		UINT_MAX	/* max value for a gid_t */

/* GCC requires that quad constants be written as expressions. */
#define	UQUAD_MAX	((u_quad_t)0-1)	/* max value for a uquad_t */
					/* max value for a quad_t */
#define	QUAD_MAX	((quad_t)(UQUAD_MAX >> 1))
#define	QUAD_MIN	(-QUAD_MAX-1)	/* min value for a quad_t */

#endif /* !_POSIX_SOURCE && !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE */

#if (!defined(_ANSI_SOURCE)&&!defined(_POSIX_SOURCE)) || defined(_XOPEN_SOURCE)
#define LONG_BIT	32
#define WORD_BIT	32

#define DBL_DIG		15
#define DBL_MAX		1.7976931348623157E+308
#define DBL_MIN		2.2250738585072014E-308

#define FLT_DIG		6
#define FLT_MAX		3.40282347E+38F
#define FLT_MIN		1.17549435E-38F
#endif
