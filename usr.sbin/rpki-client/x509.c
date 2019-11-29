/*	$OpenBSD: x509.c,v 1.13 2019/11/29 05:00:24 benno Exp $ */
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

#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include "extern.h"

/*
 * Wrapper around ASN1_get_object() that preserves the current start
 * state and returns a more meaningful value.
 * Return zero on failure, non-zero on success.
 */
static int
ASN1_frame(const char *fn, size_t sz,
    const unsigned char **cnt, long *cntsz, int *tag)
{
	int	 ret, pcls;

	assert(cnt != NULL && *cnt != NULL);
	assert(sz > 0);
	ret = ASN1_get_object(cnt, cntsz, tag, &pcls, sz);
	if ((ret & 0x80)) {
		cryptowarnx("%s: ASN1_get_object", fn);
		return 0;
	}
	return ASN1_object_size((ret & 0x01) ? 2 : 0, *cntsz, *tag);
}

/*
 * Parse X509v3 authority key identifier (AKI), RFC 6487 sec. 4.8.3.
 * Returns the AKI or NULL if it could not be parsed.
 * The AKI is formatted as aa:bb:cc:dd, with each being a hex value.
 */
char *
x509_get_aki_ext(X509_EXTENSION *ext, const char *fn)
{
	const unsigned char	*d;
	const ASN1_TYPE		*t;
	const ASN1_OCTET_STRING	*os = NULL;
	ASN1_SEQUENCE_ANY	*seq = NULL;
	int			 dsz, ptag;
	long			 i, plen;
	char			 buf[4];
	char			*res = NULL;

	assert(NID_authority_key_identifier ==
	    OBJ_obj2nid(X509_EXTENSION_get_object(ext)));
	os = X509_EXTENSION_get_data(ext);
	assert(os != NULL);

	d = os->data;
	dsz = os->length;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "failed ASN.1 sub-sequence parse", fn);
		goto out;
	}
	if (sk_ASN1_TYPE_num(seq) != 1) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "want 1 element, have %d", fn, sk_ASN1_TYPE_num(seq));
		goto out;
	}

	t = sk_ASN1_TYPE_value(seq, 0);
	if (t->type != V_ASN1_OTHER) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "want ASN.1 external, have %s (NID %d)",
		    fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}

	d = t->value.asn1_string->data;
	dsz = t->value.asn1_string->length;

	if (!ASN1_frame(fn, dsz, &d, &plen, &ptag))
		goto out;

	/* Make room for [hex1, hex2, ":"]*, NUL. */

	if ((res = calloc(plen * 3 + 1, 1)) == NULL)
		err(1, NULL);

	for (i = 0; i < plen; i++) {
		snprintf(buf, sizeof(buf), "%02X:", d[i]);
		strlcat(res, buf, plen * 3 + 1);
	}
	res[plen * 3 - 1] = '\0';
out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return res;
}

/*
 * Parse X509v3 subject key identifier (SKI), RFC 6487 sec. 4.8.2.
 * Returns the SKI or NULL if it could not be parsed.
 * The SKI is formatted as aa:bb:cc:dd, with each being a hex value.
 */
char *
x509_get_ski_ext(X509_EXTENSION *ext, const char *fn)
{
	const unsigned char	*d;
	const ASN1_OCTET_STRING	*os;
	ASN1_OCTET_STRING	*oss = NULL;
	int			 i, dsz;
	char			 buf[4];
	char			*res = NULL;

	assert(NID_subject_key_identifier ==
	    OBJ_obj2nid(X509_EXTENSION_get_object(ext)));

	os = X509_EXTENSION_get_data(ext);
	assert(os != NULL);
	d = os->data;
	dsz = os->length;

	if ((oss = d2i_ASN1_OCTET_STRING(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6487 section 4.8.2: SKI: "
		    "failed ASN.1 octet string parse", fn);
		goto out;
	}

	d = oss->data;
	dsz = oss->length;

	if (dsz != 20) {
		warnx("%s: RFC 6487 section 4.8.2: SKI: "
		    "want 20 B SHA1 hash, have %d B", fn, dsz);
		goto out;
	}

	/* Make room for [hex1, hex2, ":"]*, NUL. */

	if ((res = calloc(dsz * 3 + 1, 1)) == NULL)
		err(1, NULL);

	for (i = 0; i < dsz; i++) {
		snprintf(buf, sizeof(buf), "%02X:", d[i]);
		strlcat(res, buf, dsz * 3 + 1);
	}
	res[dsz * 3 - 1] = '\0';
out:
	ASN1_OCTET_STRING_free(oss);
	return res;
}

/*
 * Wraps around x509_get_ski_ext and x509_get_aki_ext.
 * Returns zero on failure (out pointers are NULL) or non-zero on
 * success (out pointers must be freed).
 */
int
x509_get_ski_aki(X509 *x, const char *fn, char **ski, char **aki)
{
	X509_EXTENSION		*ext = NULL;
	const ASN1_OBJECT	*obj;
	int			 extsz, i;

	*ski = *aki = NULL;

	if ((extsz = X509_get_ext_count(x)) < 0)
		cryptoerrx("X509_get_ext_count");

	for (i = 0; i < extsz; i++) {
		ext = X509_get_ext(x, i);
		assert(ext != NULL);
		obj = X509_EXTENSION_get_object(ext);
		assert(obj != NULL);
		switch (OBJ_obj2nid(obj)) {
		case NID_subject_key_identifier:
			free(*ski);
			*ski = x509_get_ski_ext(ext, fn);
			break;
		case NID_authority_key_identifier:
			free(*aki);
			*aki = x509_get_aki_ext(ext, fn);
			break;
		}
	}

	if (*aki == NULL) {
		cryptowarnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "missing AKI X509 extension", fn);
		free(*ski);
		*ski = NULL;
		return 0;
	}
	if (*ski == NULL) {
		cryptowarnx("%s: RFC 6487 section 4.8.2: AKI: "
		    "missing SKI X509 extension", fn);
		free(*aki);
		*aki = NULL;
		return 0;
	}

	return 1;
}

/*
 * Parse the very specific subset of information in the CRL distribution
 * point extension.
 * See RFC 6487, sectoin 4.8.6 for details.
 * Returns NULL on failure, the crl URI on success which has to be freed
 * after use.
 */
char *
x509_get_crl(X509 *x, const char *fn)
{
	STACK_OF(DIST_POINT)	*crldp;
	DIST_POINT		*dp;
	GENERAL_NAME		*name;
	char			*crl;

	crldp = X509_get_ext_d2i(x, NID_crl_distribution_points, NULL, NULL);
	if (crldp == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "no CRL distribution point extension", fn);
		return NULL;
	}

	if (sk_DIST_POINT_num(crldp) != 1) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "want 1 element, have %d", fn,
		    sk_DIST_POINT_num(crldp));
		return NULL;
	}

	dp = sk_DIST_POINT_value(crldp, 0);
	if (dp->distpoint == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "no distribution point name", fn);
		return NULL;
	}
	if (dp->distpoint->type != 0) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "expected GEN_OTHERNAME, have %d", fn, dp->distpoint->type);
		return NULL;
	}

	if (sk_GENERAL_NAME_num(dp->distpoint->name.fullname) != 1) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "want 1 full name, have %d", fn,
		    sk_GENERAL_NAME_num(dp->distpoint->name.fullname));
		return NULL;
	}

	name = sk_GENERAL_NAME_value(dp->distpoint->name.fullname, 0);
	if (name->type != GEN_URI) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "want URI type, have %d", fn, name->type);
		return NULL;
	}

	crl = strndup(ASN1_STRING_get0_data(name->d.uniformResourceIdentifier),
	    ASN1_STRING_length(name->d.uniformResourceIdentifier));
	if (crl == NULL)
		err(1, NULL);

	return crl;
}

char *
x509_crl_get_aki(X509_CRL *crl)
{
	X509_EXTENSION *ext;
	int loc;

	loc = X509_CRL_get_ext_by_NID(crl, NID_authority_key_identifier, -1);
	if (loc == -1) {
		warnx("%s: CRL without AKI extension", __func__);
		return NULL;
	}
	ext = X509_CRL_get_ext(crl, loc);

	return x509_get_aki_ext(ext, "x509_crl_get_aki");
}
