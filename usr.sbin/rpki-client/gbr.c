/*	$OpenBSD: gbr.c,v 1.32 2025/06/12 16:59:48 tb Exp $ */
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
#include <vis.h>

#include <openssl/x509.h>

#include "extern.h"

extern ASN1_OBJECT	*gbr_oid;

#define VCARD_START	"BEGIN:VCARD\r\nVERSION:4.0\r\n"
#define VCARD_START_LEN	(sizeof(VCARD_START) - 1)

/*
 * Parse a full RFC 6493 file and signed by the certificate "cacert"
 * (the latter is optional and may be passed as NULL to disable).
 * Returns the payload or NULL if the document was malformed.
 */
struct gbr *
gbr_parse(X509 **x509, const char *fn, int talid, const unsigned char *der,
    size_t len)
{
	struct gbr	*gbr;
	struct cert	*cert = NULL;
	size_t		 cmsz;
	unsigned char	*cms;
	time_t		 signtime = 0;

	cms = cms_parse_validate(x509, fn, der, len, gbr_oid, &cmsz, &signtime);
	if (cms == NULL)
		return NULL;

	if ((gbr = calloc(1, sizeof(*gbr))) == NULL)
		err(1, NULL);
	gbr->signtime = signtime;

	if (cmsz < VCARD_START_LEN ||
	    strncmp(cms, VCARD_START, VCARD_START_LEN) != 0) {
		warnx("%s: Ghostbusters record with invalid vCard", fn);
		goto out;
	}
	if ((gbr->vcard = calloc(cmsz + 1, 4)) == NULL)
		err(1, NULL);
	(void)strvisx(gbr->vcard, cms, cmsz, VIS_SAFE);

	free(cms);
	cms = NULL;

	if (!x509_get_aia(*x509, fn, &gbr->aia))
		goto out;
	if (!x509_get_aki(*x509, fn, &gbr->aki))
		goto out;
	if (!x509_get_sia(*x509, fn, &gbr->sia))
		goto out;
	if (!x509_get_ski(*x509, fn, &gbr->ski))
		goto out;
	if (gbr->aia == NULL || gbr->aki == NULL || gbr->sia == NULL ||
	    gbr->ski == NULL) {
		warnx("%s: RFC 6487 section 4.8: "
		    "missing AIA, AKI, SIA or SKI X509 extension", fn);
		goto out;
	}

	if (!x509_get_notbefore(*x509, fn, &gbr->notbefore))
		goto out;
	if (!x509_get_notafter(*x509, fn, &gbr->notafter))
		goto out;

	if (!x509_inherits(*x509)) {
		warnx("%s: RFC 3779 extension not set to inherit", fn);
		goto out;
	}

	if ((cert = cert_parse_ee_cert(fn, talid, *x509)) == NULL)
		goto out;
	cert_free(cert);

	return gbr;

 out:
	free(cms);
	gbr_free(gbr);
	X509_free(*x509);
	*x509 = NULL;
	cert_free(cert);
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
