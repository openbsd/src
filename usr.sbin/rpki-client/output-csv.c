/*	$OpenBSD: output-csv.c,v 1.2 2019/11/18 08:36:38 claudio Exp $ */
/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
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

void
output_csv(FILE *out, struct vrp_tree *vrps)
{
	char		 buf[64];
	struct vrp	*v;

	fprintf(out, "ASN,IP Prefix,Max Length,Trust Anchor\n");

	RB_FOREACH(v, vrp_tree, vrps) {
		ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));
		fprintf(out, "AS%u,%s,%u,%s\n", v->asid, buf, v->maxlength,
		    v->tal);
	}
}
