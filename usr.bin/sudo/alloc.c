/*
 * Copyright (c) 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
#include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#include <sys/param.h>
#include <sys/types.h>

#include "sudo.h"

#ifndef STDC_HEADERS
#if !defined(__GNUC__) && !defined(HAVE_MALLOC_H)
extern VOID *malloc	__P((size_t));
#endif /* !__GNUC__ && !HAVE_MALLOC_H */
#endif /* !STDC_HEADERS */

extern char **Argv;		/* from sudo.c */

#ifndef lint
static const char rcsid[] = "$Sudo: alloc.c,v 1.8 1999/07/31 16:19:44 millert Exp $";
#endif /* lint */


/*
 * emalloc() calls the system malloc(3) and exits with an error if
 * malloc(3) fails.
 */
VOID *
emalloc(size)
    size_t size;
{
    VOID *ptr;

    if ((ptr = malloc(size)) == NULL) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }
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

    if ((ptr = ptr ? realloc(ptr, size) : malloc(size)) == NULL) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }
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

    if (src != NULL) {
	dst = (char *) emalloc(strlen(src) + 1);
	(void) strcpy(dst, src);
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

    if (len == -1) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }
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

    if ((len = vasprintf(ret, format, args)) == -1) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }
    return(len);
}
