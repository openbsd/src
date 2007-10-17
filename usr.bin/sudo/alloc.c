/*
 * Copyright (c) 1999-2005 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <config.h>

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
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include "sudo.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: alloc.c,v 1.23.2.4 2007/09/11 12:20:15 millert Exp $";
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
#  define SIZE_MAX	INT_MAX
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
easprintf(ret, fmt, va_alist)
    char **ret;
    const char *fmt;
    va_dcl
#endif
{
    int len;
    va_list ap;
#ifdef __STDC__
    va_start(ap, fmt);
#else
    va_start(ap);
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

/*
 * Wrapper for free(3) so we can depend on C89 semantics.
 */
void
efree(ptr)
    VOID *ptr;
{
    if (ptr != NULL)
	free(ptr);
}
