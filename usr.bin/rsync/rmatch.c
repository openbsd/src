/*	$OpenBSD: rmatch.c,v 1.3 2022/12/26 19:16:02 jmc Exp $	*/

/*
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
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

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
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
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "charclass.h"
#include "extern.h"

#define	RANGE_MATCH	1
#define	RANGE_NOMATCH	0
#define	RANGE_ERROR	(-1)

static int
classmatch(const char *pattern, char test, const char **ep)
{
	const char *mismatch = pattern;
	const struct cclass *cc;
	const char *colon;
	size_t len;
	int rval = RANGE_NOMATCH;

	if (*pattern++ != ':') {
		*ep = mismatch;
		return RANGE_ERROR;
	}
	if ((colon = strchr(pattern, ':')) == NULL || colon[1] != ']') {
		*ep = mismatch;
		return RANGE_ERROR;
	}
	*ep = colon + 2;
	len = (size_t)(colon - pattern);

	for (cc = cclasses; cc->name != NULL; cc++) {
		if (!strncmp(pattern, cc->name, len) && cc->name[len] == '\0') {
			if (cc->isctype((unsigned char)test))
				rval = RANGE_MATCH;
			return rval;
		}
	}

	/* invalid character class, treat as normal text */
	*ep = mismatch;
	return RANGE_ERROR;
}

static int
rangematch(const char **pp, char test)
{
	const char *pattern = *pp;
	int negate, ok;
	char c, c2;

	/*
	 * A bracket expression starting with an unquoted circumflex
	 * character produces unspecified results (IEEE 1003.2-1992,
	 * 3.13.2).  This implementation treats it like '!', for
	 * consistency with the regular expression syntax.
	 * J.T. Conklin (conklin@ngai.kaleida.com)
	 */
	if ((negate = (*pattern == '!' || *pattern == '^')))
		++pattern;

	/*
	 * A right bracket shall lose its special meaning and represent
	 * itself in a bracket expression if it occurs first in the list.
	 * -- POSIX.2 2.8.3.2
	 */
	ok = 0;
	c = *pattern++;
	do {
		if (c == '[') {
			switch (classmatch(pattern, test, &pattern)) {
			case RANGE_MATCH:
				ok = 1;
				continue;
			case RANGE_NOMATCH:
				continue;
			default:
				/* invalid character class, treat literally. */
				break;
			}
		}
		if (c == '\\')
			c = *pattern++;
		if (c == '\0')
			return RANGE_ERROR;
		/* patterns can not match on '/' */
		if (c == '/')
			return RANGE_NOMATCH;
		if (*pattern == '-'
		    && (c2 = *(pattern + 1)) != '\0' && c2 != ']') {
			pattern += 2;
			if (c2 == '\\')
				c2 = *pattern++;
			if (c2 == '\0')
				return RANGE_ERROR;
			if (c <= test && test <= c2)
				ok = 1;
		} else if (c == test)
			ok = 1;
	} while ((c = *pattern++) != ']');

	*pp = pattern;
	return (ok == negate ? RANGE_NOMATCH : RANGE_MATCH);
}

/*
 * Single character match, advances pattern as much as needed.
 * Return 0 on match and !0 (aka 1) on mismatch.
 * When matched pp is advanced to the end of the pattern matched.
 */
static int
matchchar(const char **pp, const char in)
{
	const char *pattern = *pp;
	char c;
	int rv = 0;

	switch (c = *pattern++) {
	case '?':
		if (in == '\0')
			rv = 1;
		if (in == '/')
			rv = 1;
		break;
	case '[':
		if (in == '\0')
			rv = 1;
		if (in == '/')
			rv = 1;
		if (rv == 1)
			break;

		switch (rangematch(&pattern, in)) {
		case RANGE_ERROR:
			/* not a good range, treat as normal text */
			goto normal;
		case RANGE_MATCH:
			break;
		case RANGE_NOMATCH:
			rv = 1;
		}
		break;
	case '\\':
		if ((c = *pattern++) == '\0') {
			c = '\\';
			--pattern;
		}
		/* FALLTHROUGH */
	default:
	normal:
		if (c != in)
			rv = 1;
		break;
	}

	*pp = pattern;
	return rv;
}

/*
 * Do a substring match. If wild is set then the pattern started with a '*'.
 * The match will go until '*', '/' or '\0' is encountered in pattern or
 * the input string is consumed up to end.
 * The pattern and string handles pp and ss are updated only on success.
 */
static int
matchsub(const char **pp, const char **ss, const char *end, int wild)
{
	const char *pattern = *pp;
	const char *p = pattern;
	const char *string = *ss;
	size_t matchlen;

	/* first calculate how many characters the submatch will consume */
	for (matchlen = 0; *p != '\0'; matchlen++) {
		if (p[0] == '*')
			break;
		/* '/' acts as barrier */
		if (p[0] == '/' || (p[0] == '\\' && p[1] == '/')) {
			if (wild) {
				/* match needs to match up to end of segment */
				if (string > end - matchlen)
					return 1;
				string = end - matchlen;
				wild = 0;
			}
			break;
		}
		/*
		 * skip forward one character in pattern by doing a
		 * dummy lookup.
		 */
		matchchar(&p, ' ');
	}

	/* not enough char to match */
	if (string > end - matchlen)
		return 1;

	if (*p == '\0') {
		if (wild) {
			/* match needs to match up to end of segment */
			string = end - matchlen;
			wild = 0;
		}
	}

	while (*pattern != '\0' && *pattern != '*') {
		/* eat possible escape char before '/' */
		if (pattern[0] == '\\' && pattern[1] == '/')
			pattern++;
		if (pattern[0] == '/')
			break;

		/* check if there are still characters available to compare */
		if (string >= end)
			return 1;
		/* Compare one char at a time. */
		if (!matchchar(&pattern, *string++))
			continue;
		if (wild) {
			/* skip forward one char and restart match */
			string = ++*ss;
			pattern = *pp;
			/* can it still match? */
			if (string > end - matchlen)
				return 1;
		} else {
			/* failed match */
			return 1;
		}
	}

	*pp = pattern;
	*ss = string;
	return 0;
}

/*
 * File matching with the addition of the special '**'.
 * Returns 0 on match and !0 for strings that do not match pattern.
 */
int
rmatch(const char *pattern, const char *string, int leading_dir)
{
	const char *segend, *segnext, *mismatch = NULL;
	int wild, starstar;

	while (*pattern && *string) {

		/* handle leading '/' first */
		if (pattern[0] == '\\' && pattern[1] == '/')
			pattern++;
		if (*string == '/' && *pattern == '/') {
			string++;
			pattern++;
		}

		/* match to the next '/' in string */
		segend = strchr(string, '/');
		if (segend == NULL)
			segend = strchr(string, '\0');

		while (*pattern) {
			/*
			 * Check for '*' and '**'. For '*' reduce '*' and '?'
			 * sequences into n-'?' and trailing '*'.
			 * For '**' this optimisation can not be done
			 * since '**???/' will match 'a/aa/aaa/' but not
			 * 'a/aa/aa/' still additional '*' will be reduced.
			 */
			wild = 0;
			starstar = 0;
			for ( ; *pattern == '*' || *pattern == '?'; pattern++) {
				if (pattern[0] == '*') {
					if (pattern[1] == '*') {
						starstar = 1;
						pattern++;
					}
					wild = 1;
				} else if (!starstar) {	/* pattern[0] == '?' */
					if (string < segend && *string != '/')
						string++;
					else
						/* no match possible */
						return 1;
				} else
					break;
			}

			/* pattern ends in '**' so it is a match */
			if (starstar && *pattern == '\0')
				return 0;

			if (starstar) {
				segnext = segend;
				mismatch = pattern;
			}

			while (string < segend) {
				if (matchsub(&pattern, &string, segend, wild)) {
failed_match:
					/*
					 * failed to match, if starstar retry
					 * with the next segment.
					 */
					if (mismatch) {
						pattern = mismatch;
						wild = 1;
						string = segnext;
						if (*string == '/')
							string++;
						segend = strchr(string, '/');
						if (!segend)
							segend = strchr(string,
							    '\0');
						segnext = segend;
						if (string < segend)
							continue;
					}
					/* no match possible */
					return 1;
				}
				break;
			}

			/* at end of string segment, eat up any extra '*' */
			if (string >= segend && *pattern != '*')
				break;
		}
		if (*string != '\0' && *string != '/')
			goto failed_match;
		if (*pattern != '\0' && *pattern != '/')
			goto failed_match;
	}

	/* if both pattern and string are consumed it was a match */
	if (*pattern == '\0' && *string == '\0')
		return 0;
	/* if leading_dir is set then string can also be '/' for success */
	if (leading_dir && *pattern == '\0' && *string == '/')
		return 0;
	/* else failure */
	return 1;
}
