/*	$OpenBSD: output-bird.c,v 1.16 2023/04/26 22:05:28 beck Exp $ */
/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2020 Robert Scheck <robert@fedoraproject.org>
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
output_bird1v4(FILE *out, struct vrp_tree *vrps, struct brk_tree *brks,
    struct vap_tree *vaps, struct stats *st)
{
	extern		const char *bird_tablename;
	struct vrp	*v;

	if (outputheader(out, st) < 0)
		return -1;

	if (fprintf(out, "\nroa table %s {\n", bird_tablename) < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		char buf[64];

		if (v->afi == AFI_IPV4) {
			ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));
			if (fprintf(out, "\troa %s max %u as %u;\n", buf,
			    v->maxlength, v->asid) < 0)
				return -1;
		}
	}

	if (fprintf(out, "}\n") < 0)
		return -1;
	return 0;
}

int
output_bird1v6(FILE *out, struct vrp_tree *vrps, struct brk_tree *brks,
    struct vap_tree *vaps, struct stats *st)
{
	extern		const char *bird_tablename;
	struct vrp	*v;

	if (outputheader(out, st) < 0)
		return -1;

	if (fprintf(out, "\nroa table %s {\n", bird_tablename) < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		char buf[64];

		if (v->afi == AFI_IPV6) {
			ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));
			if (fprintf(out, "\troa %s max %u as %u;\n", buf,
			    v->maxlength, v->asid) < 0)
				return -1;
		}
	}

	if (fprintf(out, "}\n") < 0)
		return -1;
	return 0;
}

int
output_bird2(FILE *out, struct vrp_tree *vrps, struct brk_tree *brks,
    struct vap_tree *vaps, struct stats *st)
{
	extern		const char *bird_tablename;
	struct vrp	*v;
	time_t		 now = get_current_time();

	if (outputheader(out, st) < 0)
		return -1;

	if (fprintf(out, "\ndefine force_roa_table_update = %lld;\n\n"
	    "roa4 table %s4;\nroa6 table %s6;\n\n"
	    "protocol static {\n\troa4 { table %s4; };\n\n",
	    (long long)now, bird_tablename, bird_tablename,
	    bird_tablename) < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		char buf[64];

		if (v->afi == AFI_IPV4) {
			ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));
			if (fprintf(out, "\troute %s max %u as %u;\n", buf,
			    v->maxlength, v->asid) < 0)
				return -1;
		}
	}

	if (fprintf(out, "}\n\nprotocol static {\n\troa6 { table %s6; };\n\n",
	    bird_tablename) < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		char buf[64];

		if (v->afi == AFI_IPV6) {
			ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));
			if (fprintf(out, "\troute %s max %u as %u;\n", buf,
			    v->maxlength, v->asid) < 0)
				return -1;
		}
	}

	if (fprintf(out, "}\n") < 0)
		return -1;
	return 0;
}
