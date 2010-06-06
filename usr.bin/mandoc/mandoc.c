/*	$Id: mandoc.c,v 1.13 2010/06/06 20:30:08 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@kth.se>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "libmandoc.h"

static int	 a2time(time_t *, const char *, const char *);


int
mandoc_special(const char *p)
{
	int		 terminator;	/* Terminator for \s. */
	int		 lim;		/* Limit for N in \s. */
	int		 c, i;
	
	if ('\\' != *p++)
		return(0);

	switch (*p) {
	case ('\''):
		/* FALLTHROUGH */
	case ('`'):
		/* FALLTHROUGH */
	case ('q'):
		/* FALLTHROUGH */
	case ('-'):
		/* FALLTHROUGH */
	case ('~'):
		/* FALLTHROUGH */
	case ('^'):
		/* FALLTHROUGH */
	case ('%'):
		/* FALLTHROUGH */
	case ('0'):
		/* FALLTHROUGH */
	case (' '):
		/* FALLTHROUGH */
	case ('}'):
		/* FALLTHROUGH */
	case ('|'):
		/* FALLTHROUGH */
	case ('&'):
		/* FALLTHROUGH */
	case ('.'):
		/* FALLTHROUGH */
	case (':'):
		/* FALLTHROUGH */
	case ('c'):
		return(2);
	case ('e'):
		return(2);
	case ('s'):
		if ('\0' == *++p)
			return(2);

		c = 2;
		terminator = 0;
		lim = 1;

		if (*p == '\'') {
			lim = 0;
			terminator = 1;
			++p;
			++c;
		} else if (*p == '[') {
			lim = 0;
			terminator = 2;
			++p;
			++c;
		} else if (*p == '(') {
			lim = 2;
			terminator = 3;
			++p;
			++c;
		}

		if (*p == '+' || *p == '-') {
			++p;
			++c;
		}

		if (*p == '\'') {
			if (terminator)
				return(0);
			lim = 0;
			terminator = 1;
			++p;
			++c;
		} else if (*p == '[') {
			if (terminator)
				return(0);
			lim = 0;
			terminator = 2;
			++p;
			++c;
		} else if (*p == '(') {
			if (terminator)
				return(0);
			lim = 2;
			terminator = 3;
			++p;
			++c;
		}

		/* TODO: needs to handle floating point. */

		if ( ! isdigit((u_char)*p))
			return(0);

		for (i = 0; isdigit((u_char)*p); i++) {
			if (lim && i >= lim)
				break;
			++p;
			++c;
		}

		if (terminator && terminator < 3) {
			if (1 == terminator && *p != '\'')
				return(0);
			if (2 == terminator && *p != ']')
				return(0);
			++p;
			++c;
		}

		return(c);
	case ('f'):
		/* FALLTHROUGH */
	case ('F'):
		/* FALLTHROUGH */
	case ('*'):
		if (0 == *++p || ! isgraph((u_char)*p))
			return(0);
		switch (*p) {
		case ('('):
			if (0 == *++p || ! isgraph((u_char)*p))
				return(0);
			return(4);
		case ('['):
			for (c = 3, p++; *p && ']' != *p; p++, c++)
				if ( ! isgraph((u_char)*p))
					break;
			return(*p == ']' ? c : 0);
		default:
			break;
		}
		return(3);
	case ('('):
		if (0 == *++p || ! isgraph((u_char)*p))
			return(0);
		if (0 == *++p || ! isgraph((u_char)*p))
			return(0);
		return(4);
	case ('['):
		break;
	default:
		return(0);
	}

	for (c = 3, p++; *p && ']' != *p; p++, c++)
		if ( ! isgraph((u_char)*p))
			break;

	return(*p == ']' ? c : 0);
}


void *
mandoc_calloc(size_t num, size_t size)
{
	void		*ptr;

	ptr = calloc(num, size);
	if (NULL == ptr) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(ptr);
}


void *
mandoc_malloc(size_t size)
{
	void		*ptr;

	ptr = malloc(size);
	if (NULL == ptr) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(ptr);
}


void *
mandoc_realloc(void *ptr, size_t size)
{

	ptr = realloc(ptr, size);
	if (NULL == ptr) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(ptr);
}


char *
mandoc_strdup(const char *ptr)
{
	char		*p;

	p = strdup(ptr);
	if (NULL == p) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(p);
}


static int
a2time(time_t *t, const char *fmt, const char *p)
{
	struct tm	 tm;
	char		*pp;

	memset(&tm, 0, sizeof(struct tm));

	pp = strptime(p, fmt, &tm);
	if (NULL != pp && '\0' == *pp) {
		*t = mktime(&tm);
		return(1);
	}

	return(0);
}


/*
 * Convert from a manual date string (see mdoc(7) and man(7)) into a
 * date according to the stipulated date type.
 */
time_t
mandoc_a2time(int flags, const char *p)
{
	time_t		 t;

	if (MTIME_MDOCDATE & flags) {
		if (0 == strcmp(p, "$" "Mdocdate$"))
			return(time(NULL));
		if (a2time(&t, "$" "Mdocdate: %b %d %Y $", p))
			return(t);
	}

	if (MTIME_CANONICAL & flags || MTIME_REDUCED & flags) 
		if (a2time(&t, "%b %d, %Y", p))
			return(t);

	if (MTIME_ISO_8601 & flags) 
		if (a2time(&t, "%Y-%m-%d", p))
			return(t);

	if (MTIME_REDUCED & flags) {
		if (a2time(&t, "%d, %Y", p))
			return(t);
		if (a2time(&t, "%Y", p))
			return(t);
	}

	return(0);
}


int
mandoc_eos(const char *p, size_t sz)
{

	if (0 == sz)
		return(0);

	/*
	 * End-of-sentence recognition must include situations where
	 * some symbols, such as `)', allow prior EOS punctuation to
	 * propogate outward.
	 */

	for ( ; sz; sz--) {
		switch (p[(int)sz - 1]) {
		case ('\"'):
			/* FALLTHROUGH */
		case ('\''):
			/* FALLTHROUGH */
		case (']'):
			/* FALLTHROUGH */
		case (')'):
			break;
		case ('.'):
			/* Escaped periods. */
			if (sz > 1 && '\\' == p[(int)sz - 2])
				return(0);
			/* FALLTHROUGH */
		case ('!'):
			/* FALLTHROUGH */
		case ('?'):
			return(1);
		default:
			return(0);
		}
	}

	return(0);
}


int
mandoc_hyph(const char *start, const char *c)
{

	/*
	 * Choose whether to break at a hyphenated character.  We only
	 * do this if it's free-standing within a word.
	 */

	/* Skip first/last character of buffer. */
	if (c == start || '\0' == *(c + 1))
		return(0);
	/* Skip first/last character of word. */
	if ('\t' == *(c + 1) || '\t' == *(c - 1))
		return(0);
	if (' ' == *(c + 1) || ' ' == *(c - 1))
		return(0);
	/* Skip double invocations. */
	if ('-' == *(c + 1) || '-' == *(c - 1))
		return(0);
	/* Skip escapes. */
	if ('\\' == *(c - 1))
		return(0);

	return(1);
}
