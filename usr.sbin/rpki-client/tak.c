/*	$OpenBSD: tak.c,v 1.2 2022/11/04 09:43:13 job Exp $ */
/*
 * Copyright (c) 2022 Job Snijders <job@fastly.com>
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
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

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/safestack.h>
#include <openssl/stack.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "extern.h"

/*
 * Parse results and data of the Trust Anchor Key file.
 */
struct parse {
	const char	*fn; /* TAK file name */
	struct tak	*res; /* results */
};

extern ASN1_OBJECT	*tak_oid;

/*
 * ASN.1 templates for Trust Anchor Keys (draft-ietf-sidrops-signed-tal-12)
 */

DECLARE_STACK_OF(ASN1_IA5STRING);

#ifndef DEFINE_STACK_OF
#define sk_ASN1_IA5STRING_num(st) SKM_sk_num(ASN1_IA5STRING, (st))
#define sk_ASN1_IA5STRING_value(st, i) SKM_sk_value(ASN1_IA5STRING, (st), (i))
#endif

typedef struct {
	STACK_OF(ASN1_UTF8STRING)	*comments;
	STACK_OF(ASN1_IA5STRING)	*certificateURIs;
	X509_PUBKEY			*subjectPublicKeyInfo;
} TAKey;

typedef struct {
	ASN1_INTEGER			*version;
	TAKey				*current;
	TAKey				*predecessor;
	TAKey				*successor;
} TAK;

ASN1_SEQUENCE(TAKey) = {
	ASN1_SEQUENCE_OF(TAKey, comments, ASN1_UTF8STRING),
	ASN1_SEQUENCE_OF(TAKey, certificateURIs, ASN1_IA5STRING),
	ASN1_SIMPLE(TAKey, subjectPublicKeyInfo, X509_PUBKEY),
} ASN1_SEQUENCE_END(TAKey);

ASN1_SEQUENCE(TAK) = {
	ASN1_EXP_OPT(TAK, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(TAK, current, TAKey),
	ASN1_EXP_OPT(TAK, predecessor, TAKey, 0),
	ASN1_EXP_OPT(TAK, successor, TAKey, 1),
} ASN1_SEQUENCE_END(TAK);

DECLARE_ASN1_FUNCTIONS(TAK);
IMPLEMENT_ASN1_FUNCTIONS(TAK);

/*
 * On success return pointer to allocated & valid takey structure,
 * on failure return NULL.
 */
static struct takey *
parse_takey(const char *fn, const TAKey *takey)
{
	const ASN1_UTF8STRING	*comment;
	const ASN1_IA5STRING	*certURI;
	EVP_PKEY		*pkey;
	RSA			*r;
	struct takey		*res = NULL;
	unsigned char		*der = NULL, *rder = NULL;
	unsigned char		 md[SHA_DIGEST_LENGTH];
	size_t			 i;
	int			 rdersz, rc = 0;

	if ((res = calloc(1, sizeof(struct takey))) == NULL)
		err(1, NULL);

	res->commentsz = sk_ASN1_UTF8STRING_num(takey->comments);
	if (res->commentsz > 0) {
		res->comments = calloc(res->commentsz, sizeof(char *));
		if (res->comments == NULL)
			err(1, NULL);

		for (i = 0; i < res->commentsz; i++) {
			comment = sk_ASN1_UTF8STRING_value(takey->comments, i);
			res->comments[i] = strndup(comment->data, comment->length);
			if (res->comments[i] == NULL)
				err(1, NULL);
		}
	}

	res->urisz = sk_ASN1_IA5STRING_num(takey->certificateURIs);
	if (res->urisz == 0) {
		warnx("%s: Signed TAL requires at least 1 CertificateURI", fn);
		goto out;
	}
	if ((res->uris = calloc(res->urisz, sizeof(char *))) == NULL)
		err(1, NULL);

	for (i = 0; i < res->urisz; i++) {
		certURI = sk_ASN1_IA5STRING_value(takey->certificateURIs, i);
		if (!valid_uri(certURI->data, certURI->length, NULL)) {
			warnx("%s: invalid TA URI", fn);
			goto out;
		}

		/* XXX: enforce that protocol is rsync or https. */

		res->uris[i] = strndup(certURI->data, certURI->length);
		if (res->uris[i] == NULL)
			err(1, NULL);
	}

	if ((pkey = X509_PUBKEY_get0(takey->subjectPublicKeyInfo)) == NULL) {
		warnx("%s: X509_PUBKEY_get0 failed", fn);
		goto out;
	}

	if ((r = EVP_PKEY_get0_RSA(pkey)) == NULL) {
		warnx("%s: EVP_PKEY_get0_RSA failed", fn);
		goto out;
	}

	if ((rdersz = i2d_RSAPublicKey(r, &rder)) <= 0) {
		warnx("%s: i2d_RSAPublicKey failed", fn);
		goto out;
	}

	if (!EVP_Digest(rder, rdersz, md, NULL, EVP_sha1(), NULL)) {
		warnx("%s: EVP_Digest failed", fn);
		goto out;
	}
	res->ski = hex_encode(md, SHA_DIGEST_LENGTH);

	if ((res->pubkeysz = i2d_PUBKEY(pkey, &der)) <= 0) {
		warnx("%s: i2d_PUBKEY failed", fn);
		goto out;
	}

	res->pubkey = der;
	der = NULL;

	rc = 1;
 out:
	if (rc == 0) {
		takey_free(res);
		res = NULL;
	}
	free(der);
	free(rder);
	return res;
}

/*
 * Parses the eContent segment of an TAK file
 * Returns zero on failure, non-zero on success.
 */
static int
tak_parse_econtent(const unsigned char *d, size_t dsz, struct parse *p)
{
	TAK		*tak;
	const char	*fn;
	int		 rc = 0;

	fn = p->fn;

	if ((tak = d2i_TAK(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: failed to parse Trust Anchor Key", fn);
		goto out;
	}

	if (!valid_econtent_version(fn, tak->version))
		goto out;

	p->res->current = parse_takey(fn, tak->current);
	if (p->res->current == NULL)
		goto out;

	if (tak->predecessor != NULL) {
		p->res->predecessor = parse_takey(fn, tak->predecessor);
		if (p->res->predecessor == NULL)
			goto out;
	}

	if (tak->successor != NULL) {
		p->res->successor = parse_takey(fn, tak->successor);
		if (p->res->successor == NULL)
			goto out;
	}

	rc = 1;
 out:
	TAK_free(tak);
	return rc;
}

/*
 * Parse a full draft-ietf-sidrops-signed-tal file.
 * Returns the TAK or NULL if the object was malformed.
 */
struct tak *
tak_parse(X509 **x509, const char *fn, const unsigned char *der, size_t len)
{
	struct parse		 p;
	unsigned char		*cms;
	size_t			 cmsz;
	const ASN1_TIME		*at;
	int			 rc = 0;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	cms = cms_parse_validate(x509, fn, der, len, tak_oid, &cmsz);
	if (cms == NULL)
		return NULL;

	if ((p.res = calloc(1, sizeof(struct tak))) == NULL)
		err(1, NULL);

	if (!x509_get_aia(*x509, fn, &p.res->aia))
		goto out;
	if (!x509_get_aki(*x509, fn, &p.res->aki))
		goto out;
	if (!x509_get_sia(*x509, fn, &p.res->sia))
		goto out;
	if (!x509_get_ski(*x509, fn, &p.res->ski))
		goto out;
	if (p.res->aia == NULL || p.res->aki == NULL || p.res->sia == NULL ||
	    p.res->ski == NULL) {
		warnx("%s: RFC 6487 section 4.8: "
		    "missing AIA, AKI, SIA, or SKI X509 extension", fn);
		goto out;
	}

	at = X509_get0_notAfter(*x509);
	if (at == NULL) {
		warnx("%s: X509_get0_notAfter failed", fn);
		goto out;
	}
	if (!x509_get_time(at, &p.res->expires)) {
		warnx("%s: ASN1_time_parse failed", fn);
		goto out;
	}

	if (!x509_inherits(*x509)) {
		warnx("%s: RFC 3779 extension not set to inherit", fn);
		goto out;
	}

	if (!tak_parse_econtent(cms, cmsz, &p))
		goto out;

	if (strcmp(p.res->aki, p.res->current->ski) != 0) {
		warnx("%s: current TAKey's SKI does not match EE AKI", fn);
		goto out;
	}

	rc = 1;
 out:
	if (rc == 0) {
		tak_free(p.res);
		p.res = NULL;
		X509_free(*x509);
		*x509 = NULL;
	}
	free(cms);
	return p.res;
}

/*
 * Free TAKey pointer.
 */
void
takey_free(struct takey *t)
{
	size_t	i;

	if (t == NULL)
		return;

	for (i = 0; i < t->commentsz; i++)
		free(t->comments[i]);

	for (i = 0; i < t->urisz; i++)
		free(t->uris[i]);

	free(t->comments);
	free(t->uris);
	free(t->ski);
	free(t->pubkey);
	free(t);
}

/*
 * Free an TAK pointer.
 * Safe to call with NULL.
 */
void
tak_free(struct tak *t)
{
	if (t == NULL)
		return;

	takey_free(t->current);
	takey_free(t->predecessor);
	takey_free(t->successor);

	free(t->aia);
	free(t->aki);
	free(t->ski);
	free(t);
}
