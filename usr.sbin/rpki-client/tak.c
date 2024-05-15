/*	$OpenBSD: tak.c,v 1.20 2024/05/15 09:01:36 tb Exp $ */
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

extern ASN1_OBJECT	*tak_oid;

/*
 * ASN.1 templates for Trust Anchor Keys (draft-ietf-sidrops-signed-tal-12)
 */

ASN1_ITEM_EXP TAKey_it;
ASN1_ITEM_EXP TAK_it;

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
	X509_PUBKEY		*pubkey;
	struct takey		*res = NULL;
	unsigned char		*der = NULL;
	size_t			 i;
	int			 der_len;

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
		goto err;
	}
	if ((res->uris = calloc(res->urisz, sizeof(char *))) == NULL)
		err(1, NULL);

	for (i = 0; i < res->urisz; i++) {
		certURI = sk_ASN1_IA5STRING_value(takey->certificateURIs, i);
		if (!valid_uri(certURI->data, certURI->length, NULL)) {
			warnx("%s: invalid TA URI", fn);
			goto err;
		}

		/* XXX: enforce that protocol is rsync or https. */

		res->uris[i] = strndup(certURI->data, certURI->length);
		if (res->uris[i] == NULL)
			err(1, NULL);
	}

	pubkey = takey->subjectPublicKeyInfo;
	if ((res->ski = x509_pubkey_get_ski(pubkey, fn)) == NULL)
		goto err;

	if ((der_len = i2d_X509_PUBKEY(pubkey, &der)) <= 0) {
		warnx("%s: i2d_X509_PUBKEY failed", fn);
		goto err;
	}
	res->pubkey = der;
	res->pubkeysz = der_len;

	return res;

 err:
	takey_free(res);
	return NULL;
}

/*
 * Parses the eContent segment of an TAK file
 * Returns zero on failure, non-zero on success.
 */
static int
tak_parse_econtent(const char *fn, struct tak *tak, const unsigned char *d,
    size_t dsz)
{
	const unsigned char	*oder;
	TAK			*tak_asn1;
	int			 rc = 0;

	oder = d;
	if ((tak_asn1 = d2i_TAK(NULL, &d, dsz)) == NULL) {
		warnx("%s: failed to parse Trust Anchor Key", fn);
		goto out;
	}
	if (d != oder + dsz) {
		warnx("%s: %td bytes trailing garbage in eContent", fn,
		    oder + dsz - d);
		goto out;
	}

	if (!valid_econtent_version(fn, tak_asn1->version, 0))
		goto out;

	tak->current = parse_takey(fn, tak_asn1->current);
	if (tak->current == NULL)
		goto out;

	if (tak_asn1->predecessor != NULL) {
		tak->predecessor = parse_takey(fn, tak_asn1->predecessor);
		if (tak->predecessor == NULL)
			goto out;
	}

	if (tak_asn1->successor != NULL) {
		tak->successor = parse_takey(fn, tak_asn1->successor);
		if (tak->successor == NULL)
			goto out;
	}

	rc = 1;
 out:
	TAK_free(tak_asn1);
	return rc;
}

/*
 * Parse a full draft-ietf-sidrops-signed-tal file.
 * Returns the TAK or NULL if the object was malformed.
 */
struct tak *
tak_parse(X509 **x509, const char *fn, int talid, const unsigned char *der,
    size_t len)
{
	struct tak		*tak;
	struct cert		*cert = NULL;
	unsigned char		*cms;
	size_t			 cmsz;
	time_t			 signtime = 0;
	int			 rc = 0;

	cms = cms_parse_validate(x509, fn, der, len, tak_oid, &cmsz, &signtime);
	if (cms == NULL)
		return NULL;

	if ((tak = calloc(1, sizeof(struct tak))) == NULL)
		err(1, NULL);
	tak->signtime = signtime;

	if (!x509_get_aia(*x509, fn, &tak->aia))
		goto out;
	if (!x509_get_aki(*x509, fn, &tak->aki))
		goto out;
	if (!x509_get_sia(*x509, fn, &tak->sia))
		goto out;
	if (!x509_get_ski(*x509, fn, &tak->ski))
		goto out;
	if (tak->aia == NULL || tak->aki == NULL || tak->sia == NULL ||
	    tak->ski == NULL) {
		warnx("%s: RFC 6487 section 4.8: "
		    "missing AIA, AKI, SIA, or SKI X509 extension", fn);
		goto out;
	}

	if (!x509_get_notbefore(*x509, fn, &tak->notbefore))
		goto out;
	if (!x509_get_notafter(*x509, fn, &tak->notafter))
		goto out;

	if (!x509_inherits(*x509)) {
		warnx("%s: RFC 3779 extension not set to inherit", fn);
		goto out;
	}

	if (!tak_parse_econtent(fn, tak, cms, cmsz))
		goto out;

	if ((cert = cert_parse_ee_cert(fn, talid, *x509)) == NULL)
		goto out;

	if (strcmp(tak->aki, tak->current->ski) != 0) {
		warnx("%s: current TAKey's SKI does not match EE AKI", fn);
		goto out;
	}

	rc = 1;
 out:
	if (rc == 0) {
		tak_free(tak);
		tak = NULL;
		X509_free(*x509);
		*x509 = NULL;
	}
	cert_free(cert);
	free(cms);
	return tak;
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
	free(t->sia);
	free(t->ski);
	free(t);
}
