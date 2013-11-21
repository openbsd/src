/*	$OpenBSD: ca.c,v 1.3 2013/11/21 08:36:51 eric Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
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

#include <sys/types.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "log.h"

int	ca_X509_verify(X509 *, STACK_OF(X509) *, const char *, const char *, const char **);

static int
verify_cb(int ok, X509_STORE_CTX *ctx)
{
	switch (X509_STORE_CTX_get_error(ctx)) {
	case X509_V_OK:
		break;
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
		log_warnx("warn: unable to get issuer cert");
		break;
        case X509_V_ERR_CERT_NOT_YET_VALID:
        case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
		log_warnx("warn: certificate not yet valid");
		break;
        case X509_V_ERR_CERT_HAS_EXPIRED:
        case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
		log_warnx("warn: certificate has expired");
		break;
        case X509_V_ERR_NO_EXPLICIT_POLICY:
		log_warnx("warn: no explicit policy");
		break;
	}
	return ok;
}

int
ca_X509_verify(X509 *certificate, STACK_OF(X509) *chain, const char *CAfile,
    const char *CRLfile, const char **errstr)
{
	X509_STORE     *store = NULL;
	X509_STORE_CTX *xsc = NULL;
	int		ret = 0;

	if ((store = X509_STORE_new()) == NULL)
		goto end;

	if (! X509_STORE_load_locations(store, CAfile, NULL)) {
		log_warn("warn: unable to load CA file %s", CAfile);
		goto end;
	}
	X509_STORE_set_default_paths(store);

	if ((xsc = X509_STORE_CTX_new()) == NULL)
		goto end;

	if (X509_STORE_CTX_init(xsc, store, certificate, chain) != 1)
		goto end;

	X509_STORE_CTX_set_verify_cb(xsc, verify_cb);

	ret = X509_verify_cert(xsc);

end:
	*errstr = NULL;
	if (ret != 1) {
		if (xsc)
			*errstr = X509_verify_cert_error_string(xsc->error);
		else if (ERR_peek_last_error())
			*errstr = ERR_error_string(ERR_peek_last_error(), NULL);
	}

	if (xsc)
		X509_STORE_CTX_free(xsc);
	if (store)
		X509_STORE_free(store);

	return ret > 0 ? 1 : 0;
}
