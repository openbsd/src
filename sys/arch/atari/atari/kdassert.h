/*	$NetBSD: kdassert.h,v 1.1.1.1 1995/03/26 07:12:18 leo Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if ! defined (_KDASSERT_H)
#define _KDASSERT_H
#if defined (DEBUG)
#if defined (__STDC__)
#if defined (__GNUC__)
#define KDASSERT(x) if (!(x)) panic ("kernel assertion:\"%s\" failed\nfile: %s\n" \
	"func: %s\nline: %d", #x , __FILE__, __FUNCTION__, __LINE__)
#else /* !__GNUC__ */
#define KDASSERT(x) if (!(x)) panic ("kernel assertion:\"%s\" failed\nfile: %s\n" \
	"line: %d", #x, __FILE__, __LINE__)
#endif /* !__GNUC__ */
#else /* !__STDC__ */
#if defined (__GNUC__)
#define KDASSERT(x) if (!(x)) panic ("kernel assertion:\"%s\" failed\nfile: %s\n" \
	"func: %s\nline: %d", "x", __FILE__, __FUNCTION__, __LINE__)
#else /* !__GNUC__ */
#define KDASSERT(x) if (!(x)) panic ("kernel assertion:\"%s\" failed\nfile: %s\n" \
	"line: %d", "x", __FILE__, __LINE__)
#endif /* !__GNUC__ */
#endif /* !__STDC__ */
#else /* !DEBUG */
#define KDASSERT(x)
#endif /* !DEBUG */
#endif /* _KDASSERT_H
