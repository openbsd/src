/*	$OpenBSD: output-bgpd.c,v 1.26 2023/01/20 15:42:34 claudio Exp $ */
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
	struct vrp	*vrp;
	struct vap	*vap;
	size_t		 i;

	if (outputheader(out, st) < 0)
		return -1;

	if (fprintf(out, "roa-set {\n") < 0)
		return -1;

	RB_FOREACH(vrp, vrp_tree, vrps) {
		char ipbuf[64], maxlenbuf[100];

		ip_addr_print(&vrp->addr, vrp->afi, ipbuf, sizeof(ipbuf));
		if (vrp->maxlength > vrp->addr.prefixlen) {
			int ret = snprintf(maxlenbuf, sizeof(maxlenbuf),
			    "maxlen %u ", vrp->maxlength);
			if (ret < 0 || (size_t)ret > sizeof(maxlenbuf))
				return -1;
		} else
			maxlenbuf[0] = '\0';
		if (fprintf(out, "\t%s %ssource-as %u expires %lld\n",
		    ipbuf, maxlenbuf, vrp->asid, (long long)vrp->expires) < 0)
			return -1;
	}

	if (fprintf(out, "}\n") < 0)
		return -1;

	if (excludeaspa)
		return 0;

	if (fprintf(out, "\naspa-set {\n") < 0)
		return -1;
	RB_FOREACH(vap, vap_tree, vaps) {
		if (fprintf(out, "\tcustomer-as %d expires %lld "
		    "provider-as { ", vap->custasid,
		    (long long)vap->expires) < 0)
			return -1;
		for (i = 0; i < vap->providersz; i++) {
			if (fprintf(out, "%u", vap->providers[i].as) < 0)
				return -1;
			switch (vap->providers[i].afi) {
			case AFI_IPV4:
				if (fprintf(out, "inet") < 0)
					return -1;
				break;
			case AFI_IPV6:
				if (fprintf(out, "inet6") < 0)
					return -1;
				break;
			}
			if (i + 1 < vap->providersz)
				if (fprintf(out, ", ") < 0)
					return -1;
		}

		if (fprintf(out, " }\n") < 0)
			return -1;
	}
	if (fprintf(out, "}\n") < 0)
		return -1;

	return 0;
}
