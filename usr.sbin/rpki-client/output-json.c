/*	$OpenBSD: output-json.c,v 1.3 2019/11/30 02:31:12 deraadt Exp $ */
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
output_json(struct vrp_tree *vrps)
{
	char		 buf[64];
	struct vrp	*v;
	int		 first = 1;
	FILE		*out;

	out = output_createtmp("json");

	fprintf(out, "{\n\t\"roas\": [\n");

	RB_FOREACH(v, vrp_tree, vrps) {
		if (first)
			first = 0;
		else
			fprintf(out, ",\n");

		ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));

		fprintf(out, "\t\t{ \"asn\": \"AS%u\", \"prefix\": \"%s\", "
		    "\"maxLength\": %u, \"ta\": \"%s\" }",
		    v->asid, buf, v->maxlength, v->tal);
	}

	fprintf(out, "\n\t]\n}\n");

	output_finish(out, "json");
}

