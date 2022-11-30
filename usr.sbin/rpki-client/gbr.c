/*	$OpenBSD: gbr.c,v 1.20 2022/11/30 09:12:50 job Exp $ */
/*
 * Copyright (c) 2020 Claudio Jeker <claudio@openbsd.org>
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

/*
 * Parse results and data of the manifest file.
 */
struct	parse {
	const char	 *fn; /* manifest file name */
	struct gbr	 *res; /* results */
};

extern ASN1_OBJECT	*gbr_oid;

/*
 * Parse a full RFC 6493 file and signed by the certificate "cacert"
 * (the latter is optional and may be passed as NULL to disable).
 * Returns the payload or NULL if the document was malformed.
 */
struct gbr *
gbr_parse(X509 **x509, const char *fn, const unsigned char *der, size_t len)
{
	struct parse	 p;
	size_t		 cmsz;
	unsigned char	*cms;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	cms = cms_parse_validate(x509, fn, der, len, gbr_oid, &cmsz);
	if (cms == NULL)
		return NULL;

	if ((p.res = calloc(1, sizeof(*p.res))) == NULL)
		err(1, NULL);
	if ((p.res->vcard = strndup(cms, cmsz)) == NULL)
		err(1, NULL);
	free(cms);

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
		    "missing AIA, AKI, SIA or SKI X509 extension", fn);
		goto out;
	}

	if (!x509_inherits(*x509)) {
		warnx("%s: RFC 3779 extension not set to inherit", fn);
		goto out;
	}

	return p.res;

 out:
	gbr_free(p.res);
	X509_free(*x509);
	*x509 = NULL;
	return NULL;
}

/*
 * Free a GBR pointer.
 * Safe to call with NULL.
 */
void
gbr_free(struct gbr *p)
{

	if (p == NULL)
		return;
	free(p->aia);
	free(p->aki);
	free(p->sia);
	free(p->ski);
	free(p->vcard);
	free(p);
}
