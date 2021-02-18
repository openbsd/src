/*	$OpenBSD: x509.c,v 1.16 2021/02/18 16:23:17 claudio Exp $ */
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

#include <openssl/x509v3.h>

#include "extern.h"

/*
 * Parse X509v3 authority key identifier (AKI), RFC 6487 sec. 4.8.3.
 * Returns the AKI or NULL if it could not be parsed.
 * The AKI is formatted as aa:bb:cc:dd, with each being a hex value.
 */
char *
x509_get_aki(X509 *x, int ta, const char *fn)
{
	const unsigned char	*d;
	AUTHORITY_KEYID		*akid;
	ASN1_OCTET_STRING	*os;
	int			 i, dsz, crit;
	char			 buf[4];
	char			*res = NULL;

	akid = X509_get_ext_d2i(x, NID_authority_key_identifier, &crit, NULL);
	if (akid == NULL) {
		if (!ta)
			warnx("%s: RFC 6487 section 4.8.3: AKI: "
			    "extension missing", fn);
		return NULL;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.3: "
		    "AKI: extension not non-critical", fn);
		goto out;
	}
	if (akid->issuer != NULL || akid->serial != NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "authorityCertIssuer or authorityCertSerialNumber present",
		    fn);
		goto out;
	}

	os = akid->keyid;
	if (os == NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "Key Identifier missing", fn);
		goto out;
	}

	d = os->data;
	dsz = os->length;

	if (dsz != SHA_DIGEST_LENGTH) {
		warnx("%s: RFC 6487 section 4.8.2: AKI: "
		    "want %d bytes SHA1 hash, have %d bytes",
		    fn, SHA_DIGEST_LENGTH, dsz);
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
	AUTHORITY_KEYID_free(akid);
	return res;
}

/*
 * Parse X509v3 subject key identifier (SKI), RFC 6487 sec. 4.8.2.
 * Returns the SKI or NULL if it could not be parsed.
 * The SKI is formatted as aa:bb:cc:dd, with each being a hex value.
 */
char *
x509_get_ski(X509 *x, const char *fn)
{
	const unsigned char	*d;
	ASN1_OCTET_STRING	*os;
	int			 i, dsz, crit;
	char			 buf[4];
	char			*res = NULL;

	os = X509_get_ext_d2i(x, NID_subject_key_identifier, &crit, NULL);
	if (os == NULL) {
		warnx("%s: RFC 6487 section 4.8.2: SKI: extension missing", fn);
		return NULL;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.2: "
		    "SKI: extension not non-critical", fn);
		goto out;
	}

	d = os->data;
	dsz = os->length;

	if (dsz != SHA_DIGEST_LENGTH) {
		warnx("%s: RFC 6487 section 4.8.2: SKI: "
		    "want %d bytes SHA1 hash, have %d bytes",
		    fn, SHA_DIGEST_LENGTH, dsz);
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
	ASN1_OCTET_STRING_free(os);
	return res;
}

/*
 * Parse the Authority Information Access (AIA) extension
 * See RFC 6487, section 4.8.7 for details.
 * Returns NULL on failure, on success returns the AIA URI
 * (which has to be freed after use).
 */
char *
x509_get_aia(X509 *x, const char *fn)
{
	ACCESS_DESCRIPTION		*ad;
	AUTHORITY_INFO_ACCESS		*info;
	char				*aia = NULL;
	int				 crit;

	info = X509_get_ext_d2i(x, NID_info_access, &crit, NULL);
	if (info == NULL) {
		warnx("%s: RFC 6487 section 4.8.7: AIA: extension missing", fn);
		return NULL;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.7: "
		    "AIA: extension not non-critical", fn);
		goto out;
	}
	if (sk_ACCESS_DESCRIPTION_num(info) != 1) {
		warnx("%s: RFC 6487 section 4.8.7: AIA: "
		    "want 1 element, have %d", fn,
		    sk_ACCESS_DESCRIPTION_num(info));
		goto out;
	}

	ad = sk_ACCESS_DESCRIPTION_value(info, 0);
	if (OBJ_obj2nid(ad->method) != NID_ad_ca_issuers) {
		warnx("%s: RFC 6487 section 4.8.7: AIA: "
		    "expected caIssuers, have %d", fn, OBJ_obj2nid(ad->method));
		goto out;
	}
	if (ad->location->type != GEN_URI) {
		warnx("%s: RFC 6487 section 4.8.7: AIA: "
		    "want GEN_URI type, have %d", fn, ad->location->type);
		goto out;
	}

	aia = strndup(
	    ASN1_STRING_get0_data(ad->location->d.uniformResourceIdentifier),
	    ASN1_STRING_length(ad->location->d.uniformResourceIdentifier));
	if (aia == NULL)
		err(1, NULL);

out:
	AUTHORITY_INFO_ACCESS_free(info);
	return aia;
}

/*
 * Wraps around x509_get_aia, x509_get_aki, and x509_get_ski.
 * Returns zero on failure (out pointers are NULL) or non-zero on
 * success (out pointers must be freed).
 */
int
x509_get_extensions(X509 *x, const char *fn, char **aia, char **aki, char **ski)
{
	*aia = *aki = *ski = NULL;

	*aia = x509_get_aia(x, fn);
	*aki = x509_get_aki(x, 0, fn);
	*ski = x509_get_ski(x, fn);

	if (*aia == NULL || *aki == NULL || *ski == NULL) {
		warnx("%s: RFC 6487 section 4.8: "
		    "missing AIA, AKI or SKI X509 extension", fn);
		free(*aia);
		free(*aki);
		free(*ski);
		*aia = *aki = *ski = NULL;
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
	CRL_DIST_POINTS		*crldp;
	DIST_POINT		*dp;
	GENERAL_NAME		*name;
	char			*crl = NULL;
	int			 crit;

	crldp = X509_get_ext_d2i(x, NID_crl_distribution_points, &crit, NULL);
	if (crldp == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "no CRL distribution point extension", fn);
		return NULL;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.6: "
		    "CRL distribution point: extension not non-critical", fn);
		goto out;
	}

	if (sk_DIST_POINT_num(crldp) != 1) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "want 1 element, have %d", fn,
		    sk_DIST_POINT_num(crldp));
		goto out;
	}

	dp = sk_DIST_POINT_value(crldp, 0);
	if (dp->distpoint == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "no distribution point name", fn);
		goto out;
	}
	if (dp->distpoint->type != 0) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "expected GEN_OTHERNAME, have %d", fn, dp->distpoint->type);
		goto out;
	}

	if (sk_GENERAL_NAME_num(dp->distpoint->name.fullname) != 1) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "want 1 full name, have %d", fn,
		    sk_GENERAL_NAME_num(dp->distpoint->name.fullname));
		goto out;
	}

	name = sk_GENERAL_NAME_value(dp->distpoint->name.fullname, 0);
	if (name->type != GEN_URI) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "want URI type, have %d", fn, name->type);
		goto out;
	}

	crl = strndup(ASN1_STRING_get0_data(name->d.uniformResourceIdentifier),
	    ASN1_STRING_length(name->d.uniformResourceIdentifier));
	if (crl == NULL)
		err(1, NULL);

out:
	CRL_DIST_POINTS_free(crldp);
	return crl;
}

char *
x509_crl_get_aki(X509_CRL *crl, const char *fn)
{
	const unsigned char	*d;
	AUTHORITY_KEYID		*akid;
	ASN1_OCTET_STRING	*os;
	int			 i, dsz, crit;
	char			 buf[4];
	char			*res = NULL;

	akid = X509_CRL_get_ext_d2i(crl, NID_authority_key_identifier, &crit,
	    NULL);
	if (akid == NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: extension missing", fn);
		return NULL;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.3: "
		    "AKI: extension not non-critical", fn);
		goto out;
	}
	if (akid->issuer != NULL || akid->serial != NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "authorityCertIssuer or authorityCertSerialNumber present",
		    fn);
		goto out;
	}

	os = akid->keyid;
	if (os == NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "Key Identifier missing", fn);
		goto out;
	}

	d = os->data;
	dsz = os->length;

	if (dsz != SHA_DIGEST_LENGTH) {
		warnx("%s: RFC 6487 section 4.8.2: AKI: "
		    "want %d bytes SHA1 hash, have %d bytes",
		    fn, SHA_DIGEST_LENGTH, dsz);
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
	AUTHORITY_KEYID_free(akid);
	return res;
}
