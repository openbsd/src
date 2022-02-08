/*	$OpenBSD: crl.c,v 1.12 2022/02/08 11:51:51 tb Exp $ */
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

#include <sys/socket.h>

#include <arpa/inet.h>
#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

struct crl *
crl_parse(const char *fn, const unsigned char *der, size_t len)
{
	struct crl	*crl;
	const ASN1_TIME	*at;
	struct tm	 expires_tm;
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

	/* extract expire time for later use */
	at = X509_CRL_get0_nextUpdate(crl->x509_crl);
	if (at == NULL) {
		warnx("%s: X509_CRL_get0_nextUpdate failed", fn);
		goto out;
	}
	memset(&expires_tm, 0, sizeof(expires_tm));
	if (ASN1_time_parse(at->data, at->length, &expires_tm, 0) == -1) {
		warnx("%s: ASN1_time_parse failed", fn);
		goto out;
	}
	if ((crl->expires = mktime(&expires_tm)) == -1)
		errx(1, "%s: mktime failed", fn);

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

RB_GENERATE(crl_tree, crl, entry, crlcmp);

void
crl_free(struct crl *crl)
{
	if (crl == NULL)
		return;
	free(crl->aki);
	X509_CRL_free(crl->x509_crl);
	free(crl);
}
