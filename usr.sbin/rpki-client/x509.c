/*	$Id: x509.c,v 1.2 2019/06/17 15:04:59 deraadt Exp $ */
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
	const unsigned char 	*d;
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
	} else if (sk_ASN1_TYPE_num(seq) != 1) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: want 1 "
			"element, have %d", fn, sk_ASN1_TYPE_num(seq));
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
		err(EXIT_FAILURE, NULL);

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
	const unsigned char 	*d;
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
		warnx("%s: RFC 6487 section 4.8.2: SKI: want 20 B "
			"SHA1 hash, have %d B", fn, dsz);
		goto out;
	}

	/* Make room for [hex1, hex2, ":"]*, NUL. */

	if ((res = calloc(dsz * 3 + 1, 1)) == NULL)
		err(EXIT_FAILURE, NULL);

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
		cryptowarnx("%s: RFC 6487 section 4.8.3: "
			"AKI: missing AKI X509 extension", fn);
		free(*ski);
		return 0;
	} else if (*ski == NULL) {
		cryptowarnx("%s: RFC 6487 section 4.8.2: "
			"AKI: missing SKI X509 extension", fn);
		free(*aki);
		return 0;
	}

	assert(*ski != NULL && *aki != NULL);
	return 1;
}
