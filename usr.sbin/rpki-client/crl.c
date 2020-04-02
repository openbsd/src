/*	$OpenBSD: crl.c,v 1.8 2020/04/02 09:16:43 claudio Exp $ */
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

#include <openssl/ssl.h>

#include "extern.h"

X509_CRL *
crl_parse(const char *fn, const unsigned char *dgst)
{
	int		 rc = 0, sz;
	X509_CRL	*x = NULL;
	BIO		*bio = NULL, *shamd;
	FILE		*f;
	EVP_MD		*md;
	char		 mdbuf[EVP_MAX_MD_SIZE];

	if ((f = fopen(fn, "rb")) == NULL) {
		warn("%s", fn);
		return NULL;
	}

	if ((bio = BIO_new_fp(f, BIO_CLOSE)) == NULL) {
		if (verbose > 0)
			cryptowarnx("%s: BIO_new_file", fn);
		return NULL;
	}

	/*
	 * If we have a digest specified, create an MD chain that will
	 * automatically compute a digest during the X509 creation.
	 */

	if (dgst != NULL) {
		if ((shamd = BIO_new(BIO_f_md())) == NULL)
			cryptoerrx("BIO_new");
		if (!BIO_set_md(shamd, EVP_sha256()))
			cryptoerrx("BIO_set_md");
		if ((bio = BIO_push(shamd, bio)) == NULL)
			cryptoerrx("BIO_push");
	}

	if ((x = d2i_X509_CRL_bio(bio, NULL)) == NULL) {
		cryptowarnx("%s: d2i_X509_CRL_bio", fn);
		goto out;
	}

	/*
	 * If we have a digest, find it in the chain (we'll already have
	 * made it, so assert otherwise) and verify it.
	 */

	if (dgst != NULL) {
		shamd = BIO_find_type(bio, BIO_TYPE_MD);
		assert(shamd != NULL);

		if (!BIO_get_md(shamd, &md))
			cryptoerrx("BIO_get_md");
		assert(EVP_MD_type(md) == NID_sha256);

		if ((sz = BIO_gets(shamd, mdbuf, EVP_MAX_MD_SIZE)) < 0)
			cryptoerrx("BIO_gets");
		assert(sz == SHA256_DIGEST_LENGTH);

		if (memcmp(mdbuf, dgst, SHA256_DIGEST_LENGTH)) {
			if (verbose > 0)
				warnx("%s: bad message digest", fn);
			goto out;
		}
	}

	rc = 1;
out:
	BIO_free_all(bio);
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
