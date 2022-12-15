/*	$OpenBSD: output-json.c,v 1.30 2022/12/15 12:02:29 claudio Exp $ */
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
	    "\t\t\"roas\": %u,\n"
	    "\t\t\"failedroas\": %u,\n"
	    "\t\t\"invalidroas\": %u,\n"
	    "\t\t\"aspas\": %u,\n"
	    "\t\t\"failedaspas\": %u,\n"
	    "\t\t\"invalidaspas\": %u,\n"
	    "\t\t\"bgpsec_pubkeys\": %u,\n"
	    "\t\t\"certificates\": %u,\n"
	    "\t\t\"invalidcertificates\": %u,\n"
	    "\t\t\"taks\": %u,\n"
	    "\t\t\"tals\": %u,\n"
	    "\t\t\"invalidtals\": %u,\n"
	    "\t\t\"talfiles\": [\n",
	    hn, tbuf, (long long)st->elapsed_time.tv_sec,
	    (long long)st->user_time.tv_sec, (long long)st->system_time.tv_sec,
	    st->repo_stats.roas,
	    st->repo_stats.roas_fail,
	    st->repo_stats.roas_invalid,
	    st->repo_stats.aspas,
	    st->repo_stats.aspas_fail,
	    st->repo_stats.aspas_invalid,
	    st->repo_stats.brks,
	    st->repo_stats.certs,
	    st->repo_stats.certs_fail,
	    st->repo_stats.taks,
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
	    "\t\t\"manifests\": %u,\n"
	    "\t\t\"failedmanifests\": %u,\n"
	    "\t\t\"stalemanifests\": %u,\n"
	    "\t\t\"crls\": %u,\n"
	    "\t\t\"gbrs\": %u,\n"
	    "\t\t\"repositories\": %u,\n"
	    "\t\t\"vrps\": %u,\n"
	    "\t\t\"uniquevrps\": %u,\n"
	    "\t\t\"vaps\": %u,\n"
	    "\t\t\"uniquevaps\": %u,\n"
	    "\t\t\"cachedir_del_files\": %u,\n"
	    "\t\t\"cachedir_superfluous_files\": %u,\n"
	    "\t\t\"cachedir_del_dirs\": %u\n"
	    "\t},\n\n",
	    st->repo_stats.mfts,
	    st->repo_stats.mfts_fail,
	    st->repo_stats.mfts_stale,
	    st->repo_stats.crls,
	    st->repo_stats.gbrs,
	    st->repos,
	    st->repo_stats.vrps,
	    st->repo_stats.vrps_uniqs,
	    st->repo_stats.vaps,
	    st->repo_stats.vaps_uniqs,
	    st->del_files,
	    st->extra_files,
	    st->del_dirs) < 0)
		return -1;
	return 0;
}

static int
print_vap(FILE *out, struct vap *v)
{
	size_t i;

	if (fprintf(out, "\t\t\t{ \"customer_asid\": %u, \"providers\": [",
	    v->custasid) < 0)
		return -1;
	for (i = 0; i < v->providersz; i++) {
		if (fprintf(out, "%u", v->providers[i]) < 0)
			return -1;
		if (i + 1 < v->providersz)
			if (fprintf(out, ", ") < 0)
				return -1;
	}
	if (fprintf(out, "], \"expires\": %lld }", (long long)v->expires) < 0)
		return -1;

	return 0;
}

static int
output_aspa(FILE *out, struct vap_tree *vaps)
{
	struct vap	*v;
	int		 first;

	if (fprintf(out, "\n\t],\n\n\t\"provider_authorizations\": {\n"
	    "\t\t\"ipv4\": [\n") < 0)
		return -1;

	first = 1;
	RB_FOREACH(v, vap_tree, vaps)
		if (v->afi == AFI_IPV4) {
			if (!first) {
				if (fprintf(out, ",\n") < 0)
					return -1;
			}
			first = 0;
			if (print_vap(out, v))
				return -1;
		}

	if (fprintf(out, "\n\t\t],\n\t\t\"ipv6\": [\n") < 0)
		return -1;

	first = 1;
	RB_FOREACH(v, vap_tree, vaps) {
		if (v->afi == AFI_IPV6) {
			if (!first) {
				if (fprintf(out, ",\n") < 0)
					return -1;
			}
			first = 0;
			if (print_vap(out, v))
				return -1;
		}
	}

	if (fprintf(out, "\n\t\t]\n\t}\n") < 0)
		return -1;

	return 0;
}


int
output_json(FILE *out, struct vrp_tree *vrps, struct brk_tree *brks,
    struct vap_tree *vaps, struct stats *st)
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

	if (output_aspa(out, vaps) < 0)
		return -1;

	if (fprintf(out, "\n}\n") < 0)
		return -1;

	return 0;
}
