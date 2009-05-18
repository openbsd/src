/*	$OpenBSD: util.h,v 1.21 2009/05/18 20:20:01 deraadt Exp $	*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __DL_UTIL_H__
#define __DL_UTIL_H__

#include <stdarg.h>

void *_dl_malloc(const size_t size);
void _dl_free(void *);
char *_dl_strdup(const char *);
void _dl_printf(const char *fmt, ...);
void _dl_vprintf(const char *fmt, va_list ap);
void _dl_fdprintf(int, const char *fmt, ...);
void _dl_show_objects(void);
unsigned int _dl_random(void);
ssize_t _dl_write(int fd, const char* buf, size_t len);

long _dl_strtol(const char *nptr, char **endptr, int base);

#define	_dl_round_page(x)	(((x) + (__LDPGSZ - 1)) & ~(__LDPGSZ - 1))

/*
 *	The following functions are declared inline so they can
 *	be used before bootstrap linking has been finished.
 */
static inline void
_dl_wrstderr(const char *s)
{
	const char *p = s;
	size_t n = 0;

	while (*p++)
		n++;
	_dl_write(2, s, n);
}

static inline void *
_dl_memset(void *dst, const int c, size_t n)
{
	if (n != 0) {
		char *d = dst;

		do
			*d++ = c;
		while (--n != 0);
	}
	return (dst);
}

static inline void
_dl_bcopy(const void *src, void *dest, int size)
{
	unsigned const char *psrc = src;
	unsigned char *pdest = dest;
	int i;

	for (i = 0; i < size; i++)
		pdest[i] = psrc[i];
}

static inline int
_dl_strlen(const char *str)
{
	const char *s;

	for (s = str; *s; ++s)
		;
	return (s - str);
}

static inline size_t
_dl_strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

static inline int
_dl_strncmp(const char *s1, const char *s2, size_t n)
{
	if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(unsigned char *)s1 - *(unsigned char *)--s2);
		if (*s1++ == 0)
			break;
	} while (--n != 0);
	return (0);
}

static inline int
_dl_strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == 0)
			return (0);
	return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}

static inline const char *
_dl_strchr(const char *p, const int ch)
{
	for (;; ++p) {
		if (*p == ch)
			return((char *)p);
		if (!*p)
			return((char *)NULL);
	}
	/* NOTREACHED */
}

static inline char *
_dl_strrchr(const char *str, const int ch)
{
	const char *p;
	char *retval = NULL;

	for (p = str; *p != '\0'; ++p)
		if (*p == ch)
			retval = (char *)p;

	return retval;
}

static inline char *
_dl_strstr(const char *s, const char *find)
{
	char c, sc;
	size_t len;
	if ((c = *find++) != 0) {
		len = _dl_strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while (sc != c);
		} while (_dl_strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

#endif /*__DL_UTIL_H__*/
