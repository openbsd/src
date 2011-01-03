/*	$Id: man_argv.c,v 1.4 2011/01/03 22:27:21 schwarze Exp $ */
/*
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
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

#include "mandoc.h"
#include "libman.h"
#include "libmandoc.h"


int
man_args(struct man *m, int line, int *pos, char *buf, char **v)
{
	char	 *start;

	assert(*pos);
	*v = start = buf + *pos;
	assert(' ' != *start);

	if ('\0' == *start)
		return(ARGS_EOLN);

	*v = mandoc_getarg(v, m->msg, m->data, line, pos);
	return('"' == *start ? ARGS_QWORD : ARGS_WORD);
}
