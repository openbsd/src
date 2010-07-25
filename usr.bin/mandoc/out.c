/*	$Id: out.c,v 1.6 2010/07/25 18:05:54 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
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
 */
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "out.h"

/* 
 * Convert a `scaling unit' to a consistent form, or fail.  Scaling
 * units are documented in groff.7, mdoc.7, man.7.
 */
int
a2roffsu(const char *src, struct roffsu *dst, enum roffscale def)
{
	char		 buf[BUFSIZ], hasd;
	int		 i;
	enum roffscale	 unit;

	if ('\0' == *src)
		return(0);

	i = hasd = 0;

	switch (*src) {
	case ('+'):
		src++;
		break;
	case ('-'):
		buf[i++] = *src++;
		break;
	default:
		break;
	}

	if ('\0' == *src)
		return(0);

	while (i < BUFSIZ) {
		if ( ! isdigit((u_char)*src)) {
			if ('.' != *src)
				break;
			else if (hasd)
				break;
			else
				hasd = 1;
		}
		buf[i++] = *src++;
	}

	if (BUFSIZ == i || (*src && *(src + 1)))
		return(0);

	buf[i] = '\0';

	switch (*src) {
	case ('c'):
		unit = SCALE_CM;
		break;
	case ('i'):
		unit = SCALE_IN;
		break;
	case ('P'):
		unit = SCALE_PC;
		break;
	case ('p'):
		unit = SCALE_PT;
		break;
	case ('f'):
		unit = SCALE_FS;
		break;
	case ('v'):
		unit = SCALE_VS;
		break;
	case ('m'):
		unit = SCALE_EM;
		break;
	case ('\0'):
		if (SCALE_MAX == def)
			return(0);
		unit = SCALE_BU;
		break;
	case ('u'):
		unit = SCALE_BU;
		break;
	case ('M'):
		unit = SCALE_MM;
		break;
	case ('n'):
		unit = SCALE_EN;
		break;
	default:
		return(0);
	}

	/* FIXME: do this in the caller. */
	if ((dst->scale = atof(buf)) < 0)
		dst->scale = 0;
	dst->unit = unit;
	return(1);
}


/*
 * Correctly writes the time in nroff form, which differs from standard
 * form in that a space isn't printed in lieu of the extra %e field for
 * single-digit dates.
 */
void
time2a(time_t t, char *dst, size_t sz)
{
	struct tm	 tm;
	char		 buf[5];
	char		*p;
	size_t		 nsz;

	assert(sz > 1);
	localtime_r(&t, &tm);

	p = dst;
	nsz = 0;

	dst[0] = '\0';

	if (0 == (nsz = strftime(p, sz, "%B ", &tm)))
		return;

	p += (int)nsz;
	sz -= nsz;

	if (0 == strftime(buf, sizeof(buf), "%e, ", &tm))
		return;

	nsz = strlcat(p, buf + (' ' == buf[0] ? 1 : 0), sz);

	if (nsz >= sz)
		return;

	p += (int)nsz;
	sz -= nsz;

	(void)strftime(p, sz, "%Y", &tm);
}


int
a2roffdeco(enum roffdeco *d, const char **word, size_t *sz)
{
	int		 i, j, lim;
	char		 term, c;
	const char	*wp;

	*d = DECO_NONE;
	lim = i = 0;
	term = '\0';
	wp = *word;

	switch ((c = wp[i++])) {
	case ('('):
		*d = DECO_SPECIAL;
		lim = 2;
		break;
	case ('F'):
		/* FALLTHROUGH */
	case ('f'):
		*d = 'F' == c ? DECO_FFONT : DECO_FONT;

		switch (wp[i++]) {
		case ('('):
			lim = 2;
			break;
		case ('['):
			term = ']';
			break;
		case ('3'):
			/* FALLTHROUGH */
		case ('B'):
			*d = DECO_BOLD;
			return(i);
		case ('2'):
			/* FALLTHROUGH */
		case ('I'):
			*d = DECO_ITALIC;
			return(i);
		case ('P'):
			*d = DECO_PREVIOUS;
			return(i);
		case ('1'):
			/* FALLTHROUGH */
		case ('R'):
			*d = DECO_ROMAN;
			return(i);
		default:
			i--;
			lim = 1;
			break;
		}
		break;
	case ('M'):
		/* FALLTHROUGH */
	case ('m'):
		/* FALLTHROUGH */
	case ('*'):
		if ('*' == c)
			*d = DECO_RESERVED;

		switch (wp[i++]) {
		case ('('):
			lim = 2;
			break;
		case ('['):
			term = ']';
			break;
		default:
			i--;
			lim = 1;
			break;
		}
		break;
	case ('s'):
		if ('+' == wp[i] || '-' == wp[i])
			i++;

		j = ('s' != wp[i - 1]);

		switch (wp[i++]) {
		case ('('):
			lim = 2;
			break;
		case ('['):
			term = ']';
			break;
		case ('\''):
			term = '\'';
			break;
		case ('0'):
			j++;
			/* FALLTHROUGH */
		default:
			i--;
			lim = 1;
			break;
		}

		if ('+' == wp[i] || '-' == wp[i]) {
			if (j++)
				return(i);
			i++;
		} 
		
		if (0 == j)
			return(i);
		break;
	case ('['):
		*d = DECO_SPECIAL;
		term = ']';
		break;
	case ('c'):
		*d = DECO_NOSPACE;
		return(i);
	default:
		*d = DECO_SSPECIAL;
		i--;
		lim = 1;
		break;
	}

	assert(term || lim);
	*word = &wp[i];

	if (term) {
		j = i;
		while (wp[i] && wp[i] != term)
			i++;
		if ('\0' == wp[i]) {
			*d = DECO_NONE;
			return(i);
		}

		assert(i >= j);
		*sz = (size_t)(i - j);

		return(i + 1);
	}

	assert(lim > 0);
	*sz = (size_t)lim;

	for (j = 0; wp[i] && j < lim; j++)
		i++;
	if (j < lim)
		*d = DECO_NONE;

	return(i);
}
