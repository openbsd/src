/*	$NetBSD: libtos.h,v 1.1.1.1 1996/01/07 21:50:49 leo Exp $	*/

/*
 * Copyright (c) 1995 Waldi Ravens.
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
 *        This product includes software developed by Waldi Ravens.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef LIBTOS_H
#define LIBTOS_H

#ifdef __STDC__
#define	PROTO(x)	x
#define EXTERN
#else
#define	PROTO(x)	()
#define	EXTERN		extern
#endif

#ifdef __GNUC__
#if (__GNUC__ > 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 5))
#define NORETURN	__attribute__((noreturn))
#else
#define	NORETURN
#endif
#endif

#include <sys/types.h>
#include <stdarg.h>
#include "kparamb.h"

EXTERN void	bsd_startup      PROTO((struct kparamb *)) NORETURN;
EXTERN void	init_toslib      PROTO((char *));
EXTERN void	redirect_output  PROTO((char *));
EXTERN int	eprintf          PROTO((char *, ...));
EXTERN int	veprintf         PROTO((char *, va_list));
EXTERN void	set_wait_for_key PROTO((void));
EXTERN void	press_any_key    PROTO((void));
EXTERN int	key_wait         PROTO((char *));
EXTERN void	xexit            PROTO((int)) NORETURN;
EXTERN void	error            PROTO((int, char *, ...));
EXTERN void	fatal            PROTO((int, char *, ...)) NORETURN;
EXTERN void *	xmalloc          PROTO((size_t));
EXTERN void *	xrealloc         PROTO((void *, size_t));

#endif	/* LIBTOS_H */
