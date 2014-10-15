/*	$OpenBSD: rfc822.c,v 1.3 2014/10/15 07:35:09 gilles Exp $	*/

/*
 * Copyright (c) 2014 Gilles Chehade <gilles@poolp.org>
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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rfc822.h"

static int
parse_addresses(struct rfc822_parser *rp, const char *buffer, size_t len)
{
	const char		*s;
	char			*wptr;
	struct rfc822_address	*ra;

	s = buffer;

	/* skip over whitespaces */
	for (s = buffer; *s && isspace(*s); ++s, len--)
		;

	/* we should now pointing to the beginning of a recipient */
	if (*s == '\0')
		return 0;

	ra = calloc(1, sizeof *ra);
	if (ra == NULL)
		return -1;

	wptr = ra->name;
	for (; len; s++, len--) {
		if (*s == '(' && !rp->escape && !rp->quote)
			rp->comment++;
		if (*s == '"' && !rp->escape && !rp->comment)
			rp->quote = !rp->quote;
		if (!rp->comment && !rp->quote && !rp->escape) {
			if (*s == '<' && rp->bracket) {
				free(ra);
				return 0;
			}
			if (*s == '>' && !rp->bracket) {
				free(ra);
				return 0;
			}

			if (*s == '<') {
				wptr = ra->address;
				rp->bracket++;
				continue;
			}
			if (*s == '>') {
				rp->bracket--;
				continue;
			}
			if (*s == ',' || *s == ';')
				break;
		}
		if (*s == ')' && !rp->escape && !rp->quote && rp->comment)
			rp->comment--;
		if (*s == '\\' && !rp->escape && !rp->comment && !rp->quote)
			rp->escape = 1;
		else
			rp->escape = 0;
		*wptr++ = *s;
	}

	/* some flags still set, malformed header */
	if (rp->escape || rp->comment || rp->quote || rp->bracket) {
		free(ra);
		return 0;
	}

	/* no value, malformed header */
	if (ra->name[0] == '\0' && ra->address[0] == '\0') {
		free(ra);
		return 0;
	}

	/* no <>, use name as address */
	if (ra->address[0] == '\0') {
		memcpy(ra->address, ra->name, sizeof ra->address);
		memset(ra->name, 0, sizeof ra->name);
	}

	/* strip first trailing whitespace from name */
	wptr = &ra->name[0] + strlen(ra->name);
	while (wptr != &ra->name[0]) {
		if (*wptr && ! isspace(*wptr))
			break;
		*wptr-- = '\0';
	}

	TAILQ_INSERT_TAIL(&rp->addresses, ra, next);

	/* do we have more to process ? */
	for (; *s; ++s, --len)
		if (*s == ',' || *s == ';')
			break;

	/* nope, we're done */
	if (*s == '\0')
		return 1;

	/* there's more to come */
	if (*s == ',' || *s == ';') {
		s++;
		len--;
	}
	if (len)
		return parse_addresses(rp, s, len);
	return 1;
}

void
rfc822_parser_init(struct rfc822_parser *rp)
{
	memset(rp, 0, sizeof *rp);
	TAILQ_INIT(&rp->addresses);
}

void
rfc822_parser_reset(struct rfc822_parser *rp)
{
	struct rfc822_address	*ra;

	while ((ra = TAILQ_FIRST(&rp->addresses))) {
		TAILQ_REMOVE(&rp->addresses, ra, next);
		free(ra);
	}
	memset(rp, 0, sizeof *rp);
}

int
rfc822_parser_feed(struct rfc822_parser *rp, const char *line)
{
	return parse_addresses(rp, line, strlen(line));
}
