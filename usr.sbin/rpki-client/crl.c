/*	$OpenBSD: crl.c,v 1.30 2023/12/10 14:18:23 job Exp $ */
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

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/x509.h>

#include "extern.h"

struct crl *
crl_parse(const char *fn, const unsigned char *der, size_t len)
{
	const unsigned char	*oder;
	struct crl		*crl;
	const X509_ALGOR	*palg;
	const ASN1_OBJECT	*cobj;
	const ASN1_TIME		*at;
	int			 count, nid, rc = 0;

	/* just fail for empty buffers, the warning was printed elsewhere */
	if (der == NULL)
		return NULL;

	if ((crl = calloc(1, sizeof(*crl))) == NULL)
		err(1, NULL);

	oder = der;
	if ((crl->x509_crl = d2i_X509_CRL(NULL, &der, len)) == NULL) {
		warnx("%s: d2i_X509_CRL", fn);
		goto out;
	}
	if (der != oder + len) {
		warnx("%s: %td bytes trailing garbage", fn, oder + len - der);
		goto out;
	}

	if (X509_CRL_get_version(crl->x509_crl) != 1) {
		warnx("%s: RFC 6487 section 5: version 2 expected", fn);
		goto out;
	}

	X509_CRL_get0_signature(crl->x509_crl, NULL, &palg);
	if (palg == NULL) {
		warnx("%s: X509_CRL_get0_signature", fn);
		goto out;
	}
	X509_ALGOR_get0(&cobj, NULL, NULL, palg);
	nid = OBJ_obj2nid(cobj);
	if (nid == NID_ecdsa_with_SHA256) {
		if (verbose)
			warnx("%s: P-256 support is experimental", fn);
	} else if (nid != NID_sha256WithRSAEncryption) {
		warnx("%s: RFC 7935: wrong signature algorithm %s, want %s",
		    fn, OBJ_nid2ln(nid),
		    OBJ_nid2ln(NID_sha256WithRSAEncryption));
		goto out;
	}

	/*
	 * RFC 6487, section 5: AKI and crlNumber MUST be present, no other
	 * CRL extensions are allowed.
	 */
	if ((crl->aki = x509_crl_get_aki(crl->x509_crl, fn)) == NULL) {
		warnx("%s: x509_crl_get_aki failed", fn);
		goto out;
	}
	if ((crl->number = x509_crl_get_number(crl->x509_crl, fn)) == NULL) {
		warnx("%s: x509_crl_get_number failed", fn);
		goto out;
	}
	if ((count = X509_CRL_get_ext_count(crl->x509_crl)) != 2) {
		warnx("%s: RFC 6487 section 5: unexpected number of extensions "
		    "%d != 2", fn, count);
		goto out;
	}

	at = X509_CRL_get0_lastUpdate(crl->x509_crl);
	if (at == NULL) {
		warnx("%s: X509_CRL_get0_lastUpdate failed", fn);
		goto out;
	}
	if (!x509_get_time(at, &crl->lastupdate)) {
		warnx("%s: ASN1_TIME_to_tm failed", fn);
		goto out;
	}

	at = X509_CRL_get0_nextUpdate(crl->x509_crl);
	if (at == NULL) {
		warnx("%s: X509_CRL_get0_nextUpdate failed", fn);
		goto out;
	}
	if (!x509_get_time(at, &crl->nextupdate)) {
		warnx("%s: ASN1_TIME_to_tm failed", fn);
		goto out;
	}

	rc = 1;
 out:
	if (rc == 0) {
		crl_free(crl);
		crl = NULL;
	}
	return crl;
}

static inline int
crlcmp(struct crl *a, struct crl *b)
{
	return strcmp(a->aki, b->aki);
}

RB_GENERATE_STATIC(crl_tree, crl, entry, crlcmp);

/*
 * Find a CRL based on the auth SKI value.
 */
struct crl *
crl_get(struct crl_tree *crlt, const struct auth *a)
{
	struct crl	find;

	if (a == NULL)
		return NULL;
	find.aki = a->cert->ski;
	return RB_FIND(crl_tree, crlt, &find);
}

int
crl_insert(struct crl_tree *crlt, struct crl *crl)
{
	return RB_INSERT(crl_tree, crlt, crl) == NULL;
}

void
crl_free(struct crl *crl)
{
	if (crl == NULL)
		return;
	free(crl->aki);
	free(crl->number);
	X509_CRL_free(crl->x509_crl);
	free(crl);
}

void
crl_tree_free(struct crl_tree *crlt)
{
	struct crl	*crl, *tcrl;

	RB_FOREACH_SAFE(crl, crl_tree, crlt, tcrl) {
		RB_REMOVE(crl_tree, crlt, crl);
		crl_free(crl);
	}
}
