/*	$Id: man_argv.c,v 1.3 2010/07/31 23:42:04 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "libman.h"


int
man_args(struct man *m, int line, int *pos, char *buf, char **v)
{

	assert(*pos);
	assert(' ' != buf[*pos]);

	if (0 == buf[*pos])
		return(ARGS_EOLN);

	*v = &buf[*pos];

	/* 
	 * Process a quoted literal.  A quote begins with a double-quote
	 * and ends with a double-quote NOT preceded by a double-quote.
	 * Whitespace is NOT involved in literal termination.
	 */

	if ('\"' == buf[*pos]) {
		*v = &buf[++(*pos)];

		for ( ; buf[*pos]; (*pos)++) {
			if ('\"' != buf[*pos])
				continue;
			if ('\"' != buf[*pos + 1])
				break;
			(*pos)++;
		}

		if (0 == buf[*pos]) {
			if ( ! man_pmsg(m, line, *pos, MANDOCERR_BADQUOTE))
				return(ARGS_ERROR);
			return(ARGS_QWORD);
		}

		buf[(*pos)++] = 0;

		if (0 == buf[*pos])
			return(ARGS_QWORD);

		while (' ' == buf[*pos])
			(*pos)++;

		if (0 == buf[*pos])
			if ( ! man_pmsg(m, line, *pos, MANDOCERR_EOLNSPACE))
				return(ARGS_ERROR);

		return(ARGS_QWORD);
	}

	/* 
	 * A non-quoted term progresses until either the end of line or
	 * a non-escaped whitespace.
	 */

	for ( ; buf[*pos]; (*pos)++)
		if (' ' == buf[*pos] && '\\' != buf[*pos - 1])
			break;

	if (0 == buf[*pos])
		return(ARGS_WORD);

	buf[(*pos)++] = 0;

	while (' ' == buf[*pos])
		(*pos)++;

	if (0 == buf[*pos])
		if ( ! man_pmsg(m, line, *pos, MANDOCERR_EOLNSPACE))
			return(ARGS_ERROR);

	return(ARGS_WORD);
}

