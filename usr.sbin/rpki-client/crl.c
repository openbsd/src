/*	$OpenBSD: crl.c,v 1.11 2021/10/26 10:52:49 claudio Exp $ */
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

X509_CRL *
crl_parse(const char *fn, const unsigned char *der, size_t len)
{
	int		 rc = 0;
	X509_CRL	*x = NULL;

	/* just fail for empty buffers, the warning was printed elsewhere */
	if (der == NULL)
		return NULL;

	if ((x = d2i_X509_CRL(NULL, &der, len)) == NULL) {
		cryptowarnx("%s: d2i_X509_CRL", fn);
		goto out;
	}

	rc = 1;
out:
	if (rc == 0) {
		X509_CRL_free(x);
		x = NULL;
	}
	return x;
}

static inline int
crlcmp(struct crl *a, struct crl *b)
{
	return strcmp(a->aki, b->aki);
}

RB_GENERATE(crl_tree, crl, entry, crlcmp);

void
free_crl(struct crl *crl)
{
	free(crl->aki);
	X509_CRL_free(crl->x509_crl);
	free(crl);
}
