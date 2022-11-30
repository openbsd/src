/*	$OpenBSD: crl.c,v 1.21 2022/11/30 09:03:44 job Exp $ */
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

#include "extern.h"

struct crl *
crl_parse(const char *fn, const unsigned char *der, size_t len)
{
	struct crl	*crl;
	const ASN1_TIME	*at;
	int		 rc = 0;

	/* just fail for empty buffers, the warning was printed elsewhere */
	if (der == NULL)
		return NULL;

	if ((crl = calloc(1, sizeof(*crl))) == NULL)
		err(1, NULL);

	if ((crl->x509_crl = d2i_X509_CRL(NULL, &der, len)) == NULL) {
		cryptowarnx("%s: d2i_X509_CRL", fn);
		goto out;
	}

	if ((crl->aki = x509_crl_get_aki(crl->x509_crl, fn)) == NULL) {
		warnx("x509_crl_get_aki failed");
		goto out;
	}

	at = X509_CRL_get0_lastUpdate(crl->x509_crl);
	if (at == NULL) {
		warnx("%s: X509_CRL_get0_lastUpdate failed", fn);
		goto out;
	}
	if (!x509_get_time(at, &crl->issued)) {
		warnx("%s: ASN1_time_parse failed", fn);
		goto out;
	}

	at = X509_CRL_get0_nextUpdate(crl->x509_crl);
	if (at == NULL) {
		warnx("%s: X509_CRL_get0_nextUpdate failed", fn);
		goto out;
	}
	if (!x509_get_time(at, &crl->expires)) {
		warnx("%s: ASN1_time_parse failed", fn);
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
