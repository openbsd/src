/*
 * Copyright (c) 1999-2003 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
# include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <limits.h>

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: alloc.c,v 1.19 2003/04/02 18:25:19 millert Exp $";
#endif /* lint */

/*
 * If there is no SIZE_MAX or SIZE_T_MAX we have to assume that size_t
 * could be signed (as it is on SunOS 4.x).  This just means that
 * emalloc2() and erealloc3() cannot allocate huge amounts on such a
 * platform but that is OK since sudo doesn't need to do so anyway.
 */
#ifndef SIZE_MAX
# ifdef SIZE_T_MAX
#  define SIZE_MAX	SIZE_T_MAX
# else
#  ifdef INT_MAX
#   define SIZE_MAX	INT_MAX
#  else
#   define SIZE_MAX	0x7fffffff
#  endif /* ULONG_MAX */
# endif /* SIZE_T_MAX */
#endif /* SIZE_MAX */

/*
 * emalloc() calls the system malloc(3) and exits with an error if
 * malloc(3) fails.
 */
VOID *
emalloc(size)
    size_t size;
{
    VOID *ptr;

    if (size == 0)
	errx(1, "internal error, tried to emalloc(0)");

    if ((ptr = (VOID *) malloc(size)) == NULL)
	errx(1, "unable to allocate memory");
    return(ptr);
}

/*
 * emalloc2() allocates nmemb * size bytes and exits with an error
 * if overflow would occur or if the system malloc(3) fails.
 */
VOID *
emalloc2(nmemb, size)
    size_t nmemb;
    size_t size;
{
    VOID *ptr;

    if (nmemb == 0 || size == 0)
	errx(1, "internal error, tried to emalloc2(0)");
    if (nmemb > SIZE_MAX / size)
	errx(1, "internal error, emalloc2() overflow");

    size *= nmemb;
    if ((ptr = (VOID *) malloc(size)) == NULL)
	errx(1, "unable to allocate memory");
    return(ptr);
}

/*
 * erealloc() calls the system realloc(3) and exits with an error if
 * realloc(3) fails.  You can call erealloc() with a NULL pointer even
 * if the system realloc(3) does not support this.
 */
VOID *
erealloc(ptr, size)
    VOID *ptr;
    size_t size;
{

    if (size == 0)
	errx(1, "internal error, tried to erealloc(0)");

    ptr = ptr ? (VOID *) realloc(ptr, size) : (VOID *) malloc(size);
    if (ptr == NULL)
	errx(1, "unable to allocate memory");
    return(ptr);
}

/*
 * erealloc3() realloc(3)s nmemb * size bytes and exits with an error
 * if overflow would occur or if the system malloc(3)/realloc(3) fails.
 * You can call erealloc() with a NULL pointer even if the system realloc(3)
 * does not support this.
 */
VOID *
erealloc3(ptr, nmemb, size)
    VOID *ptr;
    size_t nmemb;
    size_t size;
{

    if (nmemb == 0 || size == 0)
	errx(1, "internal error, tried to erealloc3(0)");
    if (nmemb > SIZE_MAX / size)
	errx(1, "internal error, erealloc3() overflow");

    size *= nmemb;
    ptr = ptr ? (VOID *) realloc(ptr, size) : (VOID *) malloc(size);
    if (ptr == NULL)
	errx(1, "unable to allocate memory");
    return(ptr);
}

/*
 * estrdup() is like strdup(3) except that it exits with an error if
 * malloc(3) fails.  NOTE: unlike strdup(3), estrdup(NULL) is legal.
 */
char *
estrdup(src)
    const char *src;
{
    char *dst = NULL;
    size_t size;

    if (src != NULL) {
	size = strlen(src) + 1;
	dst = (char *) emalloc(size);
	(void) memcpy(dst, src, size);
    }
    return(dst);
}

/*
 * easprintf() calls vasprintf() and exits with an error if vasprintf()
 * returns -1 (out of memory).
 */
int
#ifdef __STDC__
easprintf(char **ret, const char *fmt, ...)
#else
easprintf(va_alist)
    va_dcl
#endif
{
    int len;
    va_list ap;
#ifdef __STDC__
    va_start(ap, fmt);
#else
    char **ret;
    const char *fmt;

    va_start(ap);
    ret = va_arg(ap, char **);
    fmt = va_arg(ap, const char *);
#endif
    len = vasprintf(ret, fmt, ap);
    va_end(ap);

    if (len == -1)
	errx(1, "unable to allocate memory");
    return(len);
}

/*
 * evasprintf() calls vasprintf() and exits with an error if vasprintf()
 * returns -1 (out of memory).
 */
int
evasprintf(ret, format, args)
    char **ret;
    const char *format;
    va_list args;
{
    int len;

    if ((len = vasprintf(ret, format, args)) == -1)
	errx(1, "unable to allocate memory");
    return(len);
}
