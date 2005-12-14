/*	$OpenBSD: ansi.h,v 1.12 2005/12/14 16:30:03 millert Exp $	*/
/*	$NetBSD: ansi.h,v 1.7 1998/04/27 17:39:11 kleink Exp $	*/

/*-
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
 *      @(#)ansi.h      7.1 (Berkeley) 3/9/91
 */

#ifndef _ANSI_H_
#define _ANSI_H_

/*
 * Types which are fundamental to the implementation and may appear in
 * more than one standard header are defined here.  Standard headers
 * then use:
 *      #ifdef  _SIZE_T_
 *      typedef _SIZE_T_ size_t;
 *      #undef  _SIZE_T_
 *      #endif
 *
 * Thanks, ANSI!
 */
#define	_BSD_CLOCK_T_		int		/* clock() */
#define	_BSD_PTRDIFF_T_		int		/* ptr1 - ptr2 */
#define	_BSD_SIZE_T_		unsigned int	/* sizeof() */
#define	_BSD_SSIZE_T_		int		/* byte count or error */
#define	_BSD_TIME_T_		int		/* time() */
#if defined(__GNUC__) && __GNUC__ >= 3
#define _BSD_VA_LIST_	__builtin_va_list
#else
#define	_BSD_VA_LIST_		char *		/* va_list */
#endif
#define	_BSD_WCHAR_T_		int		/* wchar_t */
#define	_BSD_WINT_T_		int		/* wint_t */
#define	_BSD_CLOCKID_T_		int		/* clockid_t */
#define	_BSD_TIMER_T_		int		/* timer_t */
#define	_BSD_SUSECONDS_T_	int		/* suseconds_t */
#define	_BSD_USECONDS_T_	unsigned int	/* useconds_t */
#define	_BSD_INTPTR_T_		int		/* intptr_t */
#define	_BSD_UINTPTR_T_		unsigned int	/* uintptr_t */

#define	_BSD_WCHAR_T_		int		/* wchar_t */
#define	_BSD_WINT_T_            int		/* wint_t */
#define	_BSD_RUNE_T_            int		/* rune_t */
#define	_BSD_WCTRANS_T_		void *		/* wctrans_t */
#define	_BSD_WCTYPE_T_		void *		/* wctype_t */

#define _BSD_OFF_T_		long long	/* off_t */

/*
 * mbstate_t is an opaque object to keep conversion state, during multibyte
 * stream conversions. The content must not be referenced by user programs.
 */
typedef union {
	char __mbstate8[128];
	long long __mbstateL;			/* for alignment */
} __mbstate_t;
#define	_BSD_MBSTATE_T_		__mbstate_t	/* mbstate_t */

#endif  /* _ANSI_H_ */
