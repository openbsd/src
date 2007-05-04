/*	$OpenBSD: libkern.h,v 1.23 2007/05/04 22:14:37 deraadt Exp $	*/
/*	$NetBSD: libkern.h,v 1.7 1996/03/14 18:52:08 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
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
 *	@(#)libkern.h	8.1 (Berkeley) 6/10/93
 */

#ifndef __LIBKERN_H__
#define __LIBKERN_H__

#include <sys/types.h>

#ifndef LIBKERN_INLINE
#define LIBKERN_INLINE	static __inline
#define LIBKERN_BODY
#endif


LIBKERN_INLINE int imax(int, int);
LIBKERN_INLINE int imin(int, int);
LIBKERN_INLINE u_int max(u_int, u_int);
LIBKERN_INLINE u_int min(u_int, u_int);
LIBKERN_INLINE long lmax(long, long);
LIBKERN_INLINE long lmin(long, long);
LIBKERN_INLINE u_long ulmax(u_long, u_long);
LIBKERN_INLINE u_long ulmin(u_long, u_long);
LIBKERN_INLINE int abs(int);

#ifdef LIBKERN_BODY
LIBKERN_INLINE int
imax(a, b)
	int a, b;
{
	return (a > b ? a : b);
}
LIBKERN_INLINE int
imin(a, b)
	int a, b;
{
	return (a < b ? a : b);
}
LIBKERN_INLINE long
lmax(a, b)
	long a, b;
{
	return (a > b ? a : b);
}
LIBKERN_INLINE long
lmin(a, b)
	long a, b;
{
	return (a < b ? a : b);
}
LIBKERN_INLINE u_int
max(a, b)
	u_int a, b;
{
	return (a > b ? a : b);
}
LIBKERN_INLINE u_int
min(a, b)
	u_int a, b;
{
	return (a < b ? a : b);
}
LIBKERN_INLINE u_long
ulmax(a, b)
	u_long a, b;
{
	return (a > b ? a : b);
}
LIBKERN_INLINE u_long
ulmin(a, b)
	u_long a, b;
{
	return (a < b ? a : b);
}

LIBKERN_INLINE int
abs(j)
	int j;
{
	return(j < 0 ? -j : j);
}
#endif

#ifdef NDEBUG						/* tradition! */
#define	assert(e)	((void)0)
#else
#ifdef __STDC__
#define	assert(e)	((e) ? (void)0 :				    \
			    __assert("", __FILE__, __LINE__, #e))
#else
#define	assert(e)	((e) ? (void)0 :				    \
			    __assert("", __FILE__, __LINE__, "e"))
#endif
#endif

#ifndef DIAGNOSTIC
#define	KASSERT(e)	((void)0)
#else
#ifdef __STDC__
#define	KASSERT(e)	((e) ? (void)0 :				    \
			    __assert("diagnostic ", __FILE__, __LINE__, #e))
#else
#define	KASSERT(e)	((e) ? (void)0 :				    \
			    __assert("diagnostic ", __FILE__, __LINE__, "e"))
#endif
#endif

#ifndef DEBUG
#define	KDASSERT(e)	((void)0)
#else
#ifdef __STDC__
#define	KDASSERT(e)	((e) ? (void)0 :				    \
			    __assert("debugging ", __FILE__, __LINE__, #e))
#else
#define	KDASSERT(e)	((e) ? (void)0 :				    \
			    __assert("debugging ", __FILE__, __LINE__, "e"))
#endif
#endif

/* Prototypes for non-quad routines. */
void	 __assert(const char *, const char *, int, const char *)
	    __attribute__ ((__noreturn__));
int	 bcmp(const void *, const void *, size_t);
void	 bzero(void *, size_t);
int	 ffs(int);
int	 locc(int, char *, u_int);
void	*memchr(const void *, int, size_t);
int	 memcmp(const void *, const void *, size_t);
u_long	 random(void);
void	 srandom(u_long);
int	 scanc(u_int, const u_char *, const u_char [], int);
int	 skpc(int, size_t, u_char *);
size_t	 strlen(const char *);
char	*strncpy(char *, const char *, size_t)
		__attribute__ ((__bounded__(__string__,1,3)));
size_t	 strlcpy(char *, const char *, size_t)
		__attribute__ ((__bounded__(__string__,1,3)));
size_t	 strlcat(char *, const char *, size_t)
		__attribute__ ((__bounded__(__string__,1,3)));
int	 strcmp(const char *, const char *);
int	 strncmp(const char *, const char *, size_t);
int	 strncasecmp(const char *, const char *, size_t);
int	 getsn(char *, int);
char	*strchr(const char *, int);
char	*strrchr(const char *, int);

#endif /* __LIBKERN_H__ */
