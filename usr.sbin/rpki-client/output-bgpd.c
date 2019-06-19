/*	$Id: output-bgpd.c,v 1.2 2019/06/17 15:04:59 deraadt Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>

#include <openssl/ssl.h>

#include "extern.h"

static int
cmp(const void *p1, const void *p2)
{
	const char *a1 = *(const char **)p1, *a2 = *(const char **)p2;

	return strcmp(a1, a2);
}

void
output_bgpd(const struct roa **roas, size_t roasz,
	int quiet, size_t *routes, size_t *unique)
{
	char	  buf1[64], buf2[32], linebuf[128];
	char	**lines = NULL;
	size_t	  i, j, k;

	*routes = *unique = 0;

	for (i = 0; i < roasz; i++)
		for (j = 0; j < roas[i]->ipsz; j++)
			(*routes)++;

	if ((lines = calloc(*routes, sizeof(char *))) == NULL)
		err(EXIT_FAILURE, NULL);

	for (i = k = 0; i < roasz; i++)
		for (j = 0; j < roas[i]->ipsz; j++) {
			ip_addr_print(&roas[i]->ips[j].addr,
				roas[i]->ips[j].afi, buf1, sizeof(buf1));
			if (roas[i]->ips[j].maxlength >
			    (roas[i]->ips[j].addr.sz * 8 -
			     roas[i]->ips[j].addr.unused))
				snprintf(buf2, sizeof(buf2),
					"maxlen %zu ",
					roas[i]->ips[j].maxlength);
			else
				buf2[0] = '\0';
			snprintf(linebuf, sizeof(linebuf),
				"%s %ssource-as %" PRIu32,
				buf1, buf2, roas[i]->asid);
			if ((lines[k++] = strdup(linebuf)) == NULL)
				err(EXIT_FAILURE, NULL);
		}

	assert(k == *routes);
	qsort(lines, *routes, sizeof(char *), cmp);

	if (!quiet)
		puts("roa-set {");
	for (i = 0; i < *routes; i++)
		if (i == 0 || strcmp(lines[i], lines[i - 1])) {
			if (!quiet)
				printf("    %s\n", lines[i]);
			(*unique)++;
		}
	if (!quiet)
		puts("}");

	for (i = 0; i < *routes; i++)
		free(lines[i]);
	free(lines);
}
