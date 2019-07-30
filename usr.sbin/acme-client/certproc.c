/*	$Id: certproc.c,v 1.10 2017/01/24 13:32:55 jsing Exp $ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>

#include "extern.h"

#define MARKER "-----BEGIN CERTIFICATE-----"

/*
 * Convert an X509 certificate to a buffer of "sz".
 * We don't guarantee that it's NUL-terminated.
 * Returns NULL on failure.
 */
static char *
x509buf(X509 *x, size_t *sz)
{
	BIO	*bio;
	char	*p;
	int	 ssz;

	/* Convert X509 to PEM in BIO. */

	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		warnx("BIO_new");
		return NULL;
	} else if (!PEM_write_bio_X509(bio, x)) {
		warnx("PEM_write_bio_X509");
		BIO_free(bio);
		return NULL;
	}

	/*
	 * Now convert bio to string.
	 * Make into NUL-terminated, just in case.
	 */

	if ((p = calloc(1, bio->num_write + 1)) == NULL) {
		warn("calloc");
		BIO_free(bio);
		return NULL;
	}

	ssz = BIO_read(bio, p, bio->num_write);
	if (ssz < 0 || (unsigned)ssz != bio->num_write) {
		warnx("BIO_read");
		BIO_free(bio);
		return NULL;
	}

	*sz = ssz;
	BIO_free(bio);
	return p;
}

int
certproc(int netsock, int filesock)
{
	char		*csr = NULL, *chain = NULL, *url = NULL;
	unsigned char	*csrcp, *chaincp;
	size_t		 csrsz, chainsz;
	int		 i, rc = 0, idx = -1, cc;
	enum certop	 op;
	long		 lval;
	X509		*x = NULL, *chainx = NULL;
	X509_EXTENSION	*ext = NULL;
	X509V3_EXT_METHOD *method = NULL;
	void		*entries;
	STACK_OF(CONF_VALUE) *val;
	CONF_VALUE	*nval;

	/* File-system and sandbox jailing. */

	ERR_load_crypto_strings();

	if (pledge("stdio", NULL) == -1) {
		warn("pledge");
		goto out;
	}

	/* Read what the netproc wants us to do. */

	op = CERT__MAX;
	if ((lval = readop(netsock, COMM_CSR_OP)) == 0)
		op = CERT_STOP;
	else if (lval == CERT_REVOKE || lval == CERT_UPDATE)
		op = lval;

	if (CERT_STOP == op) {
		rc = 1;
		goto out;
	} else if (CERT__MAX == op) {
		warnx("unknown operation from netproc");
		goto out;
	}

	/*
	 * Pass revocation right through to fileproc.
	 * If the reader is terminated, ignore it.
	 */

	if (CERT_REVOKE == op) {
		if (writeop(filesock, COMM_CHAIN_OP, FILE_REMOVE) >= 0)
			rc = 1;
		goto out;
	}

	/*
	 * Wait until we receive the DER encoded (signed) certificate
	 * from the network process.
	 * Then convert the DER encoding into an X509 certificate.
	 */

	if ((csr = readbuf(netsock, COMM_CSR, &csrsz)) == NULL)
		goto out;

	csrcp = (u_char *)csr;
	x = d2i_X509(NULL, (const u_char **)&csrcp, csrsz);
	if (x == NULL) {
		warnx("d2i_X509");
		goto out;
	}

	/*
	 * Extract the CA Issuers from its NID.
	 * TODO: I have no idea what I'm doing.
	 */

	idx = X509_get_ext_by_NID(x, NID_info_access, idx);
	if (idx >= 0 && (ext = X509_get_ext(x, idx)) != NULL)
		method = (X509V3_EXT_METHOD *)X509V3_EXT_get(ext);

	entries = X509_get_ext_d2i(x, NID_info_access, 0, 0);
	if (method != NULL && entries != NULL) {
		val = method->i2v(method, entries, 0);
		for (i = 0; i < sk_CONF_VALUE_num(val); i++) {
			nval = sk_CONF_VALUE_value(val, i);
			if (strcmp(nval->name, "CA Issuers - URI"))
				continue;
			url = strdup(nval->value);
			if (url == NULL) {
				warn("strdup");
				goto out;
			}
			break;
		}
	}

	if (url == NULL) {
		warnx("no CA issuer registered with certificate");
		goto out;
	}

	/* Write the CA issuer to the netsock. */

	if (writestr(netsock, COMM_ISSUER, url) <= 0)
		goto out;

	/* Read the full-chain back from the netsock. */

	if ((chain = readbuf(netsock, COMM_CHAIN, &chainsz)) == NULL)
		goto out;

	/*
	 * Then check if the chain is PEM-encoded by looking to see if
	 * it begins with the PEM marker.
	 * If so, ship it as-is; otherwise, convert to a PEM encoded
	 * buffer and ship that.
	 * FIXME: if PEM, re-parse it.
	 */

	if (chainsz <= strlen(MARKER) ||
	    strncmp(chain, MARKER, strlen(MARKER))) {
		chaincp = (u_char *)chain;
		chainx = d2i_X509(NULL, (const u_char **)&chaincp, chainsz);
		if (chainx == NULL) {
			warnx("d2i_X509");
			goto out;
		}
		free(chain);
		if ((chain = x509buf(chainx, &chainsz)) == NULL)
			goto out;
	}

	/* Allow reader termination to just push us out. */

	if ((cc = writeop(filesock, COMM_CHAIN_OP, FILE_CREATE)) == 0)
		rc = 1;
	if (cc <= 0)
		goto out;
	if ((cc = writebuf(filesock, COMM_CHAIN, chain, chainsz)) == 0)
		rc = 1;
	if (cc <= 0)
		goto out;

	/*
	 * Next, convert the X509 to a buffer and send that.
	 * Reader failure doesn't change anything.
	 */

	free(chain);
	if ((chain = x509buf(x, &chainsz)) == NULL)
		goto out;
	if (writebuf(filesock, COMM_CSR, chain, chainsz) < 0)
		goto out;

	rc = 1;
out:
	close(netsock);
	close(filesock);
	if (x != NULL)
		X509_free(x);
	if (chainx != NULL)
		X509_free(chainx);
	free(csr);
	free(url);
	free(chain);
	ERR_print_errors_fp(stderr);
	ERR_free_strings();
	return rc;
}
