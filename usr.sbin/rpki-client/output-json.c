/*	$OpenBSD: output-json.c,v 1.26 2022/05/15 16:43:34 tb Exp $ */
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
#include <unistd.h>
#include <time.h>
#include <netdb.h>

#include "extern.h"

static int
outputheader_json(FILE *out, struct stats *st)
{
	char		 hn[NI_MAXHOST], tbuf[26];
	struct tm	*tp;
	time_t		 t;
	int		 i;

	time(&t);
	tp = gmtime(&t);
	strftime(tbuf, sizeof tbuf, "%FT%TZ", tp);

	gethostname(hn, sizeof hn);

	if (fprintf(out,
	    "{\n\t\"metadata\": {\n"
	    "\t\t\"buildmachine\": \"%s\",\n"
	    "\t\t\"buildtime\": \"%s\",\n"
	    "\t\t\"elapsedtime\": \"%lld\",\n"
	    "\t\t\"usertime\": \"%lld\",\n"
	    "\t\t\"systemtime\": \"%lld\",\n"
	    "\t\t\"roas\": %zu,\n"
	    "\t\t\"failedroas\": %zu,\n"
	    "\t\t\"invalidroas\": %zu,\n"
	    "\t\t\"bgpsec_pubkeys\": %zu,\n"
	    "\t\t\"certificates\": %zu,\n"
	    "\t\t\"invalidcertificates\": %zu,\n"
	    "\t\t\"tals\": %zu,\n"
	    "\t\t\"invalidtals\": %zu,\n"
	    "\t\t\"talfiles\": [\n",
	    hn, tbuf, (long long)st->elapsed_time.tv_sec,
	    (long long)st->user_time.tv_sec, (long long)st->system_time.tv_sec,
	    st->roas, st->roas_fail, st->roas_invalid,
	    st->brks, st->certs, st->certs_fail,
	    st->tals, talsz - st->tals) < 0)
		return -1;

	for (i = 0; i < talsz; i++) {
		if (fprintf(out,
		    "\t\t\t\"%s\"%s\n",
		    tals[i], i == talsz - 1 ? "" : ",") < 0)
			return -1;
	}

	if (fprintf(out,
	    "\t\t],\n"
	    "\t\t\"manifests\": %zu,\n"
	    "\t\t\"failedmanifests\": %zu,\n"
	    "\t\t\"stalemanifests\": %zu,\n"
	    "\t\t\"crls\": %zu,\n"
	    "\t\t\"gbrs\": %zu,\n"
	    "\t\t\"repositories\": %zu,\n"
	    "\t\t\"vrps\": %zu,\n"
	    "\t\t\"uniquevrps\": %zu,\n"
	    "\t\t\"cachedir_del_files\": %zu,\n"
	    "\t\t\"cachedir_superfluous_files\": %zu,\n"
	    "\t\t\"cachedir_del_dirs\": %zu\n"
	    "\t},\n\n",
	    st->mfts, st->mfts_fail, st->mfts_stale,
	    st->crls,
	    st->gbrs,
	    st->repos,
	    st->vrps, st->uniqs,
	    st->del_files, st->extra_files, st->del_dirs) < 0)
		return -1;
	return 0;
}

int
output_json(FILE *out, struct vrp_tree *vrps, struct brk_tree *brks,
    struct stats *st)
{
	char		 buf[64];
	struct vrp	*v;
	struct brk	*b;
	int		 first = 1;

	if (outputheader_json(out, st) < 0)
		return -1;

	if (fprintf(out, "\t\"roas\": [\n") < 0)
		return -1;

	RB_FOREACH(v, vrp_tree, vrps) {
		if (!first) {
			if (fprintf(out, ",\n") < 0)
				return -1;
		}
		first = 0;

		ip_addr_print(&v->addr, v->afi, buf, sizeof(buf));

		if (fprintf(out, "\t\t{ \"asn\": %u, \"prefix\": \"%s\", "
		    "\"maxLength\": %u, \"ta\": \"%s\", \"expires\": %lld }",
		    v->asid, buf, v->maxlength, taldescs[v->talid],
		    (long long)v->expires)
		    < 0)
			return -1;
	}

	if (fprintf(out, "\n\t],\n\n\t\"bgpsec_keys\": [\n") < 0)
		return -1;

	first = 1;
	RB_FOREACH(b, brk_tree, brks) {
		if (!first) {
			if (fprintf(out, ",\n") < 0)
				return -1;
		}
		first = 0;

		if (fprintf(out, "\t\t{ \"asn\": %u, \"ski\": \"%s\", "
		    "\"pubkey\": \"%s\", \"ta\": \"%s\", \"expires\": %lld }",
		    b->asid, b->ski, b->pubkey, taldescs[b->talid],
		    (long long)b->expires) < 0)
			return -1;
	}

	if (fprintf(out, "\n\t]\n}\n") < 0)
		return -1;
	return 0;
}
