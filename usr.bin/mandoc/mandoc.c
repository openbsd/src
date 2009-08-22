/*	$Id: mandoc.c,v 1.3 2009/08/22 15:18:11 schwarze Exp $ */
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

#include "libmandoc.h"

int
mandoc_special(const char *p)
{
	int		 c;
	
	if ('\\' != *p++)
		return(0);

	switch (*p) {
	case ('\\'):
		/* FALLTHROUGH */
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
	case ('f'):
		if (0 == *++p || ! isgraph((u_char)*p))
			return(0);
		return(3);
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

