/*	$Id: out.c,v 1.5 2010/06/27 20:28:56 schwarze Exp $ */
/*
 * Copyright (c) 2009 Kristaps Dzonsons <kristaps@kth.se>
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

/* See a2roffdeco(). */
#define	C2LIM(c, l) do { \
	(l) = 1; \
	if ('[' == (c) || '\'' == (c)) \
		(l) = 0; \
	else if ('(' == (c)) \
		(l) = 2; } \
	while (/* CONSTCOND */ 0)

/* See a2roffdeco(). */
#define	C2TERM(c, t) do { \
	(t) = 0; \
	if ('\'' == (c)) \
		(t) = 1; \
	else if ('[' == (c)) \
		(t) = 2; \
	else if ('(' == (c)) \
		(t) = 3; } \
	while (/* CONSTCOND */ 0)

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


/* 
 * Returns length of parsed string (the leading "\" should NOT be
 * included).  This can be zero if the current character is the nil
 * terminator.  "d" is set to the type of parsed decorator, which may
 * have an adjoining "word" of size "sz" (e.g., "(ab" -> "ab", 2).
 */
int
a2roffdeco(enum roffdeco *d,
		const char **word, size_t *sz)
{
	int		 j, term, lim;
	char		 set;
	const char	*wp, *sp;

	*d = DECO_NONE;
	wp = *word;

	switch ((set = *wp)) {
	case ('\0'):
		return(0);

	case ('('):
		if ('\0' == *(++wp))
			return(1);
		if ('\0' == *(wp + 1))
			return(2);

		*d = DECO_SPECIAL;
		*sz = 2;
		*word = wp;
		return(3);

	case ('F'):
		/* FALLTHROUGH */
	case ('f'):
		/*
		 * FIXME: this needs work and consolidation (it should
		 * follow the sequence that special characters do, for
		 * one), but isn't a priority at the moment.  Note, for
		 * one, that in reality \fB != \FB, although here we let
		 * these slip by.
		 */
		switch (*(++wp)) {
		case ('\0'):
			return(1);
		case ('3'):
			/* FALLTHROUGH */
		case ('B'):
			*d = DECO_BOLD;
			return(2);
		case ('2'):
			/* FALLTHROUGH */
		case ('I'):
			*d = DECO_ITALIC;
			return(2);
		case ('P'):
			*d = DECO_PREVIOUS;
			return(2);
		case ('1'):
			/* FALLTHROUGH */
		case ('R'):
			*d = DECO_ROMAN;
			return(2);
		case ('('):
			if ('\0' == *(++wp))
				return(2);
			if ('\0' == *(wp + 1))
				return(3);

			*d = 'F' == set ? DECO_FFONT : DECO_FONT;
			*sz = 2;
			*word = wp;
			return(4);
		case ('['):
			*word = ++wp;
			for (j = 0; *wp && ']' != *wp; wp++, j++)
				/* Loop... */ ;

			if ('\0' == *wp)
				return(j + 2);

			*d = 'F' == set ? DECO_FFONT : DECO_FONT;
			*sz = (size_t)j;
			return(j + 3);
		default:
			break;
		}

		*d = 'F' == set ? DECO_FFONT : DECO_FONT;
		*sz = 1;
		*word = wp;
		return(2);

	case ('*'):
		switch (*(++wp)) {
		case ('\0'):
			return(1);

		case ('('):
			if ('\0' == *(++wp))
				return(2);
			if ('\0' == *(wp + 1))
				return(3);

			*d = DECO_RESERVED;
			*sz = 2;
			*word = wp;
			return(4);

		case ('['):
			*word = ++wp;
			for (j = 0; *wp && ']' != *wp; wp++, j++)
				/* Loop... */ ;

			if ('\0' == *wp)
				return(j + 2);

			*d = DECO_RESERVED;
			*sz = (size_t)j;
			return(j + 3);

		default:
			break;
		}

		*d = DECO_RESERVED;
		*sz = 1;
		*word = wp;
		return(2);

	case ('s'):
		sp = wp;
		if ('\0' == *(++wp))
			return(1);

		C2LIM(*wp, lim);
		C2TERM(*wp, term);

		if (term) 
			wp++;

		*word = wp;

		if (*wp == '+' || *wp == '-')
			++wp;

		switch (*wp) {
		case ('\''):
			/* FALLTHROUGH */
		case ('['):
			/* FALLTHROUGH */
		case ('('):
			if (term) 
				return((int)(wp - sp));

			C2LIM(*wp, lim);
			C2TERM(*wp, term);
			wp++;
			break;
		default:
			break;
		}

		if ( ! isdigit((u_char)*wp))
			return((int)(wp - sp));

		for (j = 0; isdigit((u_char)*wp); j++) {
			if (lim && j >= lim)
				break;
			++wp;
		}

		if (term && term < 3) {
			if (1 == term && *wp != '\'')
				return((int)(wp - sp));
			if (2 == term && *wp != ']')
				return((int)(wp - sp));
			++wp;
		}

		*d = DECO_SIZE;
		return((int)(wp - sp));

	case ('['):
		*word = ++wp;

		for (j = 0; *wp && ']' != *wp; wp++, j++)
			/* Loop... */ ;

		if ('\0' == *wp)
			return(j + 1);

		*d = DECO_SPECIAL;
		*sz = (size_t)j;
		return(j + 2);

	case ('c'):
		*d = DECO_NOSPACE;
		*sz = 1;
		return(1);

	default:
		break;
	}

	*d = DECO_SPECIAL;
	*word = wp;
	*sz = 1;
	return(1);
}
