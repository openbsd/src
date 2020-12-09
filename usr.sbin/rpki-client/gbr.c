/*	$OpenBSD: gbr.c,v 1.1 2020/12/09 11:29:04 claudio Exp $ */
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

#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * Parse results and data of the manifest file.
 */
struct	parse {
	const char	 *fn; /* manifest file name */
	struct gbr	 *res; /* results */
};

/*
 * Parse a full RFC 6493 file and signed by the certificate "cacert"
 * (the latter is optional and may be passed as NULL to disable).
 * Returns the payload or NULL if the document was malformed.
 */
struct gbr *
gbr_parse(X509 **x509, const char *fn)
{
	struct parse	 p;
	size_t		 cmsz;
	unsigned char	*cms;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	/* OID from section 9.1, RFC 6493. */

	cms = cms_parse_validate(x509, fn,
	    "1.2.840.113549.1.9.16.1.35", NULL, &cmsz);
	if (cms == NULL)
		return NULL;

	if ((p.res = calloc(1, sizeof(struct roa))) == NULL)
		err(1, NULL);
	if ((p.res->vcard = strndup(cms, cmsz)) == NULL)
		err(1, NULL);
	if (!x509_get_ski_aki(*x509, fn, &p.res->ski, &p.res->aki)) {
		gbr_free(p.res);
		X509_free(*x509);
		*x509 = NULL;
		return NULL;
	}

	free(cms);
	return p.res;
}

/*
 * Free an GBR pointer.
 * Safe to call with NULL.
 */
void
gbr_free(struct gbr *p)
{

	if (p == NULL)
		return;
	free(p->aki);
	free(p->ski);
	free(p->vcard);
	free(p);
}
