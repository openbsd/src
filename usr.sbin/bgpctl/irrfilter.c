/*	$OpenBSD: irrfilter.c,v 1.5 2015/01/16 06:40:15 deraadt Exp $ */

/*
 * Copyright (c) 2007 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "irrfilter.h"

__dead void
irr_main(u_int32_t AS, int flags, char *outdir)
{
	char	*query;
	int	 r;

	fprintf(stderr, "irrfilter for: %u, writing to %s\n", AS, outdir);

	irrflags = flags;
	irrverbose = 0;
	TAILQ_INIT(&router_head);

	/* send query for own AS, parse policy */
	if (asprintf(&query, "AS%u", AS) == -1)
		err(1, "parse_policy asprintf");
	if ((r = whois(query, QTYPE_OWNAS)) == -1)
		exit(1);
	if (r == 0)
		errx(1, "aut-num object %s not found", query);
	free(query);

	write_filters(outdir);

	exit(0);
}
