/*	$OpenBSD: limits.h,v 1.7 2000/03/20 17:04:33 rahnds Exp $	*/
/*	$NetBSD: limits.h,v 1.1 1996/09/30 16:34:28 ws Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_LIMITS_H_
#define _MACHINE_LIMITS_H_

#define	CHAR_BIT	8		/* bits per char		*/
#define	MB_LEN_MAX	1		/* no multibyte characters	*/
#define	CHAR_MIN	0		/* min value in char		*/
#define	CHAR_MAX	0xff		/* max value in char		*/
#define	UCHAR_MAX	0xff		/* max value in unsigned char	*/
#define	SCHAR_MIN	(-0x7f-1)	/* min value for a signed char	*/
#define	SCHAR_MAX	0x7f		/* max value for a signed char	*/

#define	SHRT_MIN	(-0x7fff-1)	/* min value in short		*/
#define	SHRT_MAX	0x7fff		/* max value in short		*/
#define	USHRT_MAX	0xffff		/* max value in unsigned short	*/

#define	INT_MIN		(-0x7fffffff-1)	/* min value in int		*/
#define	INT_MAX		0x7fffffff	/* max value in int		*/
#define	UINT_MAX	0xffffffff	/* max value in unsigned int	*/

#define	LONG_MIN	(-0x7fffffff-1)	/* min value in long		*/
#define	LONG_MAX	0x7fffffff	/* max value in long		*/
#define	ULONG_MAX	0xffffffff	/* max value in unsigned long	*/

#if !defined(_ANSI_SOURCE)
#define SSIZE_MAX	INT_MAX		/* max value for a ssize_t */  

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
#define	SIZE_T_MAX	UINT_MAX	/* max value for a size_t */

#define	UID_MAX		UINT_MAX	/* max value for a uid_t */
#define	GID_MAX		UINT_MAX	/* max value for a gid_t */

#define	UQUAD_MAX	0xffffffffffffffffULL		/* max unsigned quad */
#define	QUAD_MAX	0x7fffffffffffffffLL		/* max signed quad */
#define	QUAD_MIN	(-0x7fffffffffffffffLL-1)	/* min signed quad */
#endif	/* !_POSIX_SOURCE && !_XOPEN_SOURCE */
#endif	/* !_ANSI_SOURCE */

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
#endif /* _MACHINE_LIMITS_H_ */
