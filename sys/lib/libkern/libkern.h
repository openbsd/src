/*	$OpenBSD: libkern.h,v 1.4 1996/06/16 01:14:21 deraadt Exp $	*/
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
 *	@(#)libkern.h	8.1 (Berkeley) 6/10/93
 */

#include <sys/types.h>

#ifndef LIBKERN_INLINE
#define LIBKERN_INLINE	static __inline
#define LIBKERN_BODY
#endif


LIBKERN_INLINE int imax __P((int, int));
LIBKERN_INLINE int imin __P((int, int));
LIBKERN_INLINE u_int max __P((u_int, u_int));
LIBKERN_INLINE u_int min __P((u_int, u_int));
LIBKERN_INLINE long lmax __P((long, long));
LIBKERN_INLINE long lmin __P((long, long));
LIBKERN_INLINE u_long ulmax __P((u_long, u_long));
LIBKERN_INLINE u_long ulmin __P((u_long, u_long));
LIBKERN_INLINE int abs __P((int));

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

/* Prototypes for non-quad routines. */
int	 bcmp __P((const void *, const void *, size_t));
int	 ffs __P((int));
int	 locc __P((int, char *, u_int));
u_long	 random __P((void));
char	*rindex __P((const char *, int));
int	 scanc __P((u_int, u_char *, u_char *, int));
int	 skpc __P((int, size_t, u_char *));
size_t	 strlen __P((const char *));
char	*strcat __P((char *, const char *));
char	*strcpy __P((char *, const char *));
char	*strncpy __P((char *, const char *, size_t));
int	 strcmp __P((const char *, const char *));
int	 strncmp __P((const char *, const char *, size_t));
int	 strncasecmp __P((const char *, const char *, size_t));
int	 getsn __P((char *, int));
