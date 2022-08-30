/*	$OpenBSD: output-bgpd.c,v 1.24 2022/08/30 18:56:49 job Exp $ */
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

#include "extern.h"

int
output_bgpd(FILE *out, struct vrp_tree *vrps, struct brk_tree *brks,
    struct vap_tree *vaps, struct stats *st)
{
	struct vrp	*v;

	if (outputheader(out, st) < 0)
		return -1;

	if (fprintf(out, "roa-set {\n") < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		char ipbuf[64], maxlenbuf[100];

		ip_addr_print(&v->addr, v->afi, ipbuf, sizeof(ipbuf));
		if (v->maxlength > v->addr.prefixlen) {
			int ret = snprintf(maxlenbuf, sizeof(maxlenbuf),
			    "maxlen %u ", v->maxlength);
			if (ret < 0 || (size_t)ret > sizeof(maxlenbuf))
				return -1;
		} else
			maxlenbuf[0] = '\0';
		if (fprintf(out, "\t%s %ssource-as %u expires %lld\n",
		    ipbuf, maxlenbuf, v->asid, (long long)v->expires) < 0)
			return -1;
	}

	if (fprintf(out, "}\n") < 0)
		return -1;
	return 0;
}
