/*	$OpenBSD: output-bgpd.c,v 1.17 2020/04/28 13:41:35 deraadt Exp $ */
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

#include <stdlib.h>
#include <openssl/ssl.h>

#include "extern.h"

int
output_bgpd(FILE *out, struct vrp_tree *vrps, struct stats *st)
{
	char		 buf1[64], buf2[32];
	struct vrp	*v;

	if (outputheader(out, st) < 0)
		return -1;

	if (fprintf(out, "roa-set {\n") < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		ip_addr_print(&v->addr, v->afi, buf1, sizeof(buf1));
		if (v->maxlength > v->addr.prefixlen)
			snprintf(buf2, sizeof(buf2), "maxlen %u ",
			    v->maxlength);
		else
			buf2[0] = '\0';
		if (fprintf(out, "\t%s %ssource-as %u\n", buf1, buf2, v->asid) < 0)
			return -1;
	}

	if (fprintf(out, "}\n") < 0)
		return -1;
	return 0;
}
