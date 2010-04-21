/*	$OpenBSD: map_parser.c,v 1.1 2010/04/21 21:04:29 gilles Exp $	*/

/*
 * Copyright (c) 2010 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"

struct map_parser *map_parser_lookup(enum map_kind);


struct map_parser map_parsers[] = {
	{ K_NONE, NULL },
	{ K_ALIASES, NULL },
	{ K_VIRTUAL, NULL },
	{ K_SECRETS, NULL }
};

struct map_parser *
map_parser_lookup(enum map_kind kind)
{
	u_int8_t i;

	for (i = 0; i < nitems(map_parsers); ++i)
		if (map_parsers[i].kind == kind)
			break;

	if (i == nitems(map_parsers))
		fatalx("invalid map kind");

	return &map_parsers[i];
}
