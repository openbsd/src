/*	$OpenBSD: output-bgpd.c,v 1.10 2019/08/20 16:01:52 claudio Exp $ */
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
output_bgpd(FILE *out, const struct roa **roas, size_t roasz,
    size_t *vrps, size_t *unique)
{
	char		 buf1[64], buf2[32];
	char		**lines = NULL;
	size_t		 i, j, k;

	*vrps = *unique = 0;

	for (i = 0; i < roasz; i++)
		*vrps += roas[i]->ipsz;

	if ((lines = calloc(*vrps, sizeof(char *))) == NULL)
		err(EXIT_FAILURE, NULL);

	for (i = k = 0; i < roasz; i++)
		for (j = 0; j < roas[i]->ipsz; j++) {
			ip_addr_print(&roas[i]->ips[j].addr,
			    roas[i]->ips[j].afi, buf1, sizeof(buf1));
			if (roas[i]->ips[j].maxlength >
			    roas[i]->ips[j].addr.prefixlen)
				snprintf(buf2, sizeof(buf2), "maxlen %zu ",
				    roas[i]->ips[j].maxlength);
			else
				buf2[0] = '\0';
			if (asprintf(&lines[k++], "%s %ssource-as %u",
			    buf1, buf2, roas[i]->asid) == -1)
				err(EXIT_FAILURE, NULL);
		}

	assert(k == *vrps);
	qsort(lines, *vrps, sizeof(char *), cmp);

	fprintf(out, "roa-set {\n");
	for (i = 0; i < *vrps; i++)
		if (i == 0 || strcmp(lines[i], lines[i - 1])) {
			fprintf(out, "\t%s\n", lines[i]);
			(*unique)++;
		}
	fprintf(out, "}\n");

	for (i = 0; i < *vrps; i++)
		free(lines[i]);
	free(lines);
}
