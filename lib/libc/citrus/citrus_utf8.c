/*	$OpenBSD: citrus_utf8.c,v 1.4 2011/04/21 00:16:06 yasuoka Exp $ */

/*-
 * Copyright (c) 2002-2004 Tim J. Robbins
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/limits.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>

#include "citrus_ctype.h"
#include "citrus_utf8.h"

_CITRUS_CTYPE_DEF_OPS(utf8);

struct _utf8_state {
	wchar_t	ch;
	int	want;
	wchar_t	lbound;
};

size_t
/*ARGSUSED*/
_citrus_utf8_ctype_mbrtowc(wchar_t * __restrict pwc,
			   const char * __restrict s, size_t n,
			   void * __restrict pspriv)
{
	struct _utf8_state *us;
	int ch, i, mask, want;
	wchar_t lbound, wch;

	us = (struct _utf8_state *)pspriv;

	if (us->want < 0 || us->want > _CITRUS_UTF8_MB_CUR_MAX) {
		errno = EINVAL;
		return ((size_t)-1);
	}

	if (s == NULL) {
		s = "";
		n = 1;
		pwc = NULL;
	}

	if (n == 0) {
		/* Incomplete multibyte sequence */
		return ((size_t)-2);
	}

	if (us->want == 0 && ((ch = (unsigned char)*s) & ~0x7f) == 0) {
		/* Fast path for plain ASCII characters. */
		if (pwc != NULL)
			*pwc = ch;
		return (ch != '\0' ? 1 : 0);
	}

	if (us->want == 0) {
		/*
		 * Determine the number of octets that make up this character
		 * from the first octet, and a mask that extracts the
		 * interesting bits of the first octet. We already know
		 * the character is at least two bytes long.
		 *
		 * We also specify a lower bound for the character code to
		 * detect redundant, non-"shortest form" encodings. For
		 * example, the sequence C0 80 is _not_ a legal representation
		 * of the null character. This enforces a 1-to-1 mapping
		 * between character codes and their multibyte representations.
		 */
		ch = (unsigned char)*s;
		if ((ch & 0x80) == 0) {
			mask = 0x7f;
			want = 1;
			lbound = 0;
		} else if ((ch & 0xe0) == 0xc0) {
			mask = 0x1f;
			want = 2;
			lbound = 0x80;
		} else if ((ch & 0xf0) == 0xe0) {
			mask = 0x0f;
			want = 3;
			lbound = 0x800;
		} else if ((ch & 0xf8) == 0xf0) {
			mask = 0x07;
			want = 4;
			lbound = 0x10000;
		} else {
			/*
			 * Malformed input; input is not UTF-8.
			 * See RFC 3629.
			 */
			errno = EILSEQ;
			return ((size_t)-1);
		}
	} else {
		want = us->want;
		lbound = us->lbound;
	}

	/*
	 * Decode the octet sequence representing the character in chunks
	 * of 6 bits, most significant first.
	 */
	if (us->want == 0)
		wch = (unsigned char)*s++ & mask;
	else
		wch = us->ch;
	for (i = (us->want == 0) ? 1 : 0; i < MIN(want, n); i++) {
		if ((*s & 0xc0) != 0x80) {
			/*
			 * Malformed input; bad characters in the middle
			 * of a character.
			 */
			errno = EILSEQ;
			return ((size_t)-1);
		}
		wch <<= 6;
		wch |= *s++ & 0x3f;
	}
	if (i < want) {
		/* Incomplete multibyte sequence. */
		us->want = want - i;
		us->lbound = lbound;
		us->ch = wch;
		return ((size_t)-2);
	}
	if (wch < lbound) {
		/*
		 * Malformed input; redundant encoding.
		 */
		errno = EILSEQ;
		return ((size_t)-1);
	}
	if ((wch >= 0xd800 && wch <= 0xdfff) ||
	    wch == 0xfffe || wch == 0xffff) {
		/*
		 * Malformed input; invalid code points.
		 */
		errno = EILSEQ;
		return ((size_t)-1);
	}
	if (pwc != NULL)
		*pwc = wch;
	us->want = 0;
	return (wch == L'\0' ? 0 : want);
}

int
/*ARGSUSED*/
_citrus_utf8_ctype_mbsinit(const void * __restrict pspriv)
{
	return (pspriv == NULL ||
	    ((const struct _utf8_state *)pspriv)->want == 0);
}

size_t
/*ARGSUSED*/
_citrus_utf8_ctype_mbsrtowcs(wchar_t * __restrict pwcs,
			     const char ** __restrict s, size_t n,
			     void * __restrict pspriv)
{
	struct _utf8_state *us;
	const char *src;
	size_t nchr;
	wchar_t wc;
	size_t nb;

	us = (struct _utf8_state *)pspriv;
	src = *s;
	nchr = 0;

	if (pwcs == NULL) {
		/*
		 * The fast path in the loop below is not safe if an ASCII
		 * character appears as anything but the first byte of a
		 * multibyte sequence. Check now to avoid doing it in the loop.
		 */
		if (us->want > 0 && (signed char)*src > 0) {
			errno = EILSEQ;
			return ((size_t)-1);
		}
		for (;;) {
			if ((signed char)*src > 0) {
				/*
				 * Fast path for plain ASCII characters
				 * excluding NUL.
				 */
				nb = 1;
			} else {
				nb = _citrus_utf8_ctype_mbrtowc(&wc, src,
				    _CITRUS_UTF8_MB_CUR_MAX, us);
				if (nb == (size_t)-1) {
					/* Invalid sequence. */
					return (nb);
				}
				if (nb == 0 || nb == (size_t)-2) {
					return (nchr);
				}
			}

			src += nb;
			nchr++;
		}
		/*NOTREACHED*/
	}

	/*
	 * The fast path in the loop below is not safe if an ASCII
	 * character appears as anything but the first byte of a
	 * multibyte sequence. Check now to avoid doing it in the loop.
	 */
	if (n > 0 && us->want > 0 && (signed char)*src > 0) {
		errno = EILSEQ;
		return ((size_t)-1);
	}
	while (n-- > 0) {
		if ((signed char)*src > 0) {
			/*
			 * Fast path for plain ASCII characters
			 * excluding NUL.
			 */
			*pwcs = (wchar_t)*src;
			nb = 1;
		} else {
			nb = _citrus_utf8_ctype_mbrtowc(pwcs, src,
			    _CITRUS_UTF8_MB_CUR_MAX, us);
			if (nb == (size_t)-1) {
				*s = src;
				return (nb);
			}
			if (nb == (size_t)-2) {
				*s = src;
				return (nchr);
			}
			if (nb == 0) {
				*s = NULL;
				return (nchr);
			}
		}
		src += nb;
		nchr++;
		pwcs++;
	}
	*s = src;
	return (nchr);
}

size_t
/*ARGSUSED*/
_citrus_utf8_ctype_wcrtomb(char * __restrict s,
			   wchar_t wc, void * __restrict pspriv)
{
	struct _utf8_state *us;
	unsigned char lead;
	int i, len;

	us = (struct _utf8_state *)pspriv;

	if (us->want != 0) {
		errno = EINVAL;
		return ((size_t)-1);
	}

	if (s == NULL) {
		/* Reset to initial shift state (no-op) */
		return (1);
	}

	if ((wc & ~0x7f) == 0) {
		/* Fast path for plain ASCII characters. */
		*s = (char)wc;
		return (1);
	}

	/*
	 * Determine the number of octets needed to represent this character.
	 * We always output the shortest sequence possible. Also specify the
	 * first few bits of the first octet, which contains the information
	 * about the sequence length.
	 */
	if ((wc & ~0x7f) == 0) {
		lead = 0;
		len = 1;
	} else if ((wc & ~0x7ff) == 0) {
		lead = 0xc0;
		len = 2;
	} else if ((wc & ~0xffff) == 0) {
		lead = 0xe0;
		len = 3;
	} else if ((wc & ~0x1fffff) == 0) {
		lead = 0xf0;
		len = 4;
	} else {
		errno = EILSEQ;
		return ((size_t)-1);
	}

	/*
	 * Output the octets representing the character in chunks
	 * of 6 bits, least significant last. The first octet is
	 * a special case because it contains the sequence length
	 * information.
	 */
	for (i = len - 1; i > 0; i--) {
		s[i] = (wc & 0x3f) | 0x80;
		wc >>= 6;
	}
	*s = (wc & 0xff) | lead;

	return (len);
}

size_t
/*ARGSUSED*/
_citrus_utf8_ctype_wcsrtombs(char * __restrict s,
			     const wchar_t ** __restrict pwcs, size_t n,
			     void * __restrict pspriv)
{
	struct _utf8_state *us;
	char buf[_CITRUS_UTF8_MB_CUR_MAX];
	const wchar_t *src;
	size_t nbytes;
	size_t nb;

	us = (struct _utf8_state *)pspriv;

	if (us->want != 0) {
		errno = EINVAL;
		return ((size_t)-1);
	}

	src = *pwcs;
	nbytes = 0;

	if (s == NULL) {
		for (;;) {
			if (0 <= *src && *src < 0x80)
				/* Fast path for plain ASCII characters. */
				nb = 1;
			else {
				nb = _citrus_utf8_ctype_wcrtomb(buf, *src, us);
				if (nb == (size_t)-1) {
					/* Invalid character */
					return (nb);
				}
			}
			if (*src == L'\0') {
				return (nbytes + nb - 1);
			}
			src++;
			nbytes += nb;
		}
		/*NOTREACHED*/
	}

	while (n > 0) {
		if (0 <= *src && *src < 0x80) {
			/* Fast path for plain ASCII characters. */
			nb = 1;
			*s = *src;
		} else if (n > (size_t)_CITRUS_UTF8_MB_CUR_MAX) {
			/* Enough space to translate in-place. */
			nb = _citrus_utf8_ctype_wcrtomb(s, *src, us);
			if (nb == (size_t)-1) {
				*pwcs = src;
				return (nb);
			}
		} else {
			/*
			 * May not be enough space; use temp. buffer.
			 */
			nb = _citrus_utf8_ctype_wcrtomb(buf, *src, us);
			if (nb == (size_t)-1) {
				*pwcs = src;
				return (nb);
			}
			if (nb > n)
				/* MB sequence for character won't fit. */
				break;
			memcpy(s, buf, nb);
		}
		if (*src == L'\0') {
			*pwcs = NULL;
			return (nbytes + nb - 1);
		}
		src++;
		s += nb;
		n -= nb;
		nbytes += nb;
	}
	*pwcs = src;
	return (nbytes);
}
