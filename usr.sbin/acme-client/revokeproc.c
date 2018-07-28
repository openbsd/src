/*	$Id: revokeproc.c,v 1.14 2018/07/28 15:25:23 tb Exp $ */
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

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>

#include "extern.h"

#define	RENEW_ALLOW (30 * 24 * 60 * 60)

/*
 * Convert the X509's expiration time (which is in ASN1_TIME format)
 * into a time_t value.
 * There are lots of suggestions on the Internet on how to do this and
 * they're really, really unsafe.
 * Adapt those poor solutions to a safe one.
 */
static time_t
X509expires(X509 *x)
{
	ASN1_TIME	*atim;
	struct tm	 t;
	unsigned char	*str;
	size_t		 i = 0;

	atim = X509_get_notAfter(x);
	str = atim->data;
	memset(&t, 0, sizeof(t));

	/* Account for 2 and 4-digit time. */

	if (atim->type == V_ASN1_UTCTIME) {
		if (atim->length <= 2) {
			warnx("invalid ASN1_TIME");
			return (time_t)-1;
		}
		t.tm_year = (str[0] - '0') * 10 + (str[1] - '0');
		if (t.tm_year < 70)
			t.tm_year += 100;
		i = 2;
	} else if (atim->type == V_ASN1_GENERALIZEDTIME) {
		if (atim->length <= 4) {
			warnx("invalid ASN1_TIME");
			return (time_t)-1;
		}
		t.tm_year = (str[0] - '0') * 1000 + (str[1] - '0') * 100 +
		    (str[2] - '0') * 10 + (str[3] - '0');
		t.tm_year -= 1900;
		i = 4;
	}

	/* Now the post-year parts. */

	if (atim->length <= (int)i + 10) {
		warnx("invalid ASN1_TIME");
		return (time_t)-1;
	}

	t.tm_mon = ((str[i + 0] - '0') * 10 + (str[i + 1] - '0')) - 1;
	t.tm_mday = (str[i + 2] - '0') * 10 + (str[i + 3] - '0');
	t.tm_hour = (str[i + 4] - '0') * 10 + (str[i + 5] - '0');
	t.tm_min  = (str[i + 6] - '0') * 10 + (str[i + 7] - '0');
	t.tm_sec  = (str[i + 8] - '0') * 10 + (str[i + 9] - '0');

	return mktime(&t);
}

int
revokeproc(int fd, const char *certdir, const char *certfile, int force,
    int revocate, const char *const *alts, size_t altsz)
{
	char		*path = NULL, *der = NULL, *dercp, *der64 = NULL;
	char		*san = NULL, *str, *tok;
	int		 rc = 0, cc, i, extsz, ssz, len;
	size_t		*found = NULL;
	BIO		*bio = NULL;
	FILE		*f = NULL;
	X509		*x = NULL;
	long		 lval;
	enum revokeop	 op, rop;
	time_t		 t;
	X509_EXTENSION	*ex;
	ASN1_OBJECT	*obj;
	size_t		 j;

	/*
	 * First try to open the certificate before we drop privileges
	 * and jail ourselves.
	 * We allow "f" to be NULL IFF the cert doesn't exist yet.
	 */

	if (asprintf(&path, "%s/%s", certdir, certfile) == -1) {
		warn("asprintf");
		goto out;
	} else if ((f = fopen(path, "r")) == NULL && errno != ENOENT) {
		warn("%s", path);
		goto out;
	}

	/* File-system and sandbox jailing. */

	ERR_load_crypto_strings();

	if (pledge("stdio", NULL) == -1) {
		warn("pledge");
		goto out;
	}

	/*
	 * If we couldn't open the certificate, it doesn't exist so we
	 * haven't submitted it yet, so obviously we can mark that it
	 * has expired and we should renew it.
	 * If we're revoking, however, then that's an error!
	 * Ignore if the reader isn't reading in either case.
	 */

	if (f == NULL && revocate) {
		warnx("%s/%s: no certificate found", certdir, certfile);
		(void)writeop(fd, COMM_REVOKE_RESP, REVOKE_OK);
		goto out;
	} else if (f == NULL && !revocate) {
		if (writeop(fd, COMM_REVOKE_RESP, REVOKE_EXP) >= 0)
			rc = 1;
		goto out;
	}

	if ((x = PEM_read_X509(f, NULL, NULL, NULL)) == NULL) {
		warnx("PEM_read_X509");
		goto out;
	}

	/* Read out the expiration date. */

	if ((t = X509expires(x)) == (time_t)-1) {
		warnx("X509expires");
		goto out;
	}

	/*
	 * Next, the long process to make sure that the SAN entries
	 * listed with the certificate fully cover those passed on the
	 * command line.
	 */

	extsz = x->cert_info->extensions != NULL ?
		sk_X509_EXTENSION_num(x->cert_info->extensions) : 0;

	/* Scan til we find the SAN NID. */

	for (i = 0; i < extsz; i++) {
		ex = sk_X509_EXTENSION_value(x->cert_info->extensions, i);
		assert(ex != NULL);
		obj = X509_EXTENSION_get_object(ex);
		assert(obj != NULL);
		if (NID_subject_alt_name != OBJ_obj2nid(obj))
			continue;

		if (san != NULL) {
			warnx("%s/%s: two SAN entries", certdir, certfile);
			goto out;
		}

		bio = BIO_new(BIO_s_mem());
		if (bio == NULL) {
			warnx("BIO_new");
			goto out;
		} else if (!X509V3_EXT_print(bio, ex, 0, 0)) {
			warnx("X509V3_EXT_print");
			goto out;
		} else if ((san = calloc(1, bio->num_write + 1)) == NULL) {
			warn("calloc");
			goto out;
		}
		ssz = BIO_read(bio, san, bio->num_write);
		if (ssz < 0 || (unsigned)ssz != bio->num_write) {
			warnx("BIO_read");
			goto out;
		}
	}

	if (san == NULL) {
		warnx("%s/%s: does not have a SAN entry", certdir, certfile);
		goto out;
	}

	/* An array of buckets: the number of entries found. */

	if ((found = calloc(altsz, sizeof(size_t))) == NULL) {
		warn("calloc");
		goto out;
	}

	/*
	 * Parse the SAN line.
	 * Make sure that all of the domains are represented only once.
	 */

	str = san;
	while ((tok = strsep(&str, ",")) != NULL) {
		if (*tok == '\0')
			continue;
		while (isspace((int)*tok))
			tok++;
		if (strncmp(tok, "DNS:", 4))
			continue;
		tok += 4;
		for (j = 0; j < altsz; j++)
			if (strcmp(tok, alts[j]) == 0)
				break;
		if (j == altsz) {
			warnx("%s/%s: unknown SAN entry: %s",
			    certdir, certfile, tok);
			goto out;
		}
		if (found[j]++) {
			warnx("%s/%s: duplicate SAN entry: %s",
			    certdir, certfile, tok);
			goto out;
		}
	}

	for (j = 0; j < altsz; j++) {
		if (found[j])
			continue;
		warnx("%s/%s: domain not listed: %s",
		    certdir, certfile, alts[j]);
		goto out;
	}

	/*
	 * If we're going to revoke, write the certificate to the
	 * netproc in DER and base64-encoded format.
	 * Then exit: we have nothing left to do.
	 */

	if (revocate) {
		dodbg("%s/%s: revocation", certdir, certfile);

		/*
		 * First, tell netproc we're online.
		 * If they're down, then just exit without warning.
		 */

		cc = writeop(fd, COMM_REVOKE_RESP, REVOKE_EXP);
		if (cc == 0)
			rc = 1;
		if (cc <= 0)
			goto out;

		if ((len = i2d_X509(x, NULL)) < 0) {
			warnx("i2d_X509");
			goto out;
		} else if ((der = dercp = malloc(len)) == NULL) {
			warn("malloc");
			goto out;
		} else if (len != i2d_X509(x, (u_char **)&dercp)) {
			warnx("i2d_X509");
			goto out;
		} else if ((der64 = base64buf_url(der, len)) == NULL) {
			warnx("base64buf_url");
			goto out;
		} else if (writestr(fd, COMM_CSR, der64) >= 0)
			rc = 1;

		goto out;
	}

	rop = time(NULL) >= (t - RENEW_ALLOW) ? REVOKE_EXP : REVOKE_OK;

	if (rop == REVOKE_EXP)
		dodbg("%s/%s: certificate renewable: %lld days left",
		    certdir, certfile,
		    (long long)(t - time(NULL)) / 24 / 60 / 60);
	else
		dodbg("%s/%s: certificate valid: %lld days left",
		    certdir, certfile,
		    (long long)(t - time(NULL)) / 24 / 60 / 60);

	if (rop == REVOKE_OK && force) {
		warnx("%s/%s: forcing renewal", certdir, certfile);
		rop = REVOKE_EXP;
	}

	/*
	 * We can re-submit it given RENEW_ALLOW time before.
	 * If netproc is down, just exit.
	 */

	if ((cc = writeop(fd, COMM_REVOKE_RESP, rop)) == 0)
		rc = 1;
	if (cc <= 0)
		goto out;

	op = REVOKE__MAX;
	if ((lval = readop(fd, COMM_REVOKE_OP)) == 0)
		op = REVOKE_STOP;
	else if (lval == REVOKE_CHECK)
		op = lval;

	if (op == REVOKE__MAX) {
		warnx("unknown operation from netproc");
		goto out;
	} else if (op == REVOKE_STOP) {
		rc = 1;
		goto out;
	}

	rc = 1;
out:
	close(fd);
	if (f != NULL)
		fclose(f);
	X509_free(x);
	BIO_free(bio);
	free(san);
	free(path);
	free(der);
	free(found);
	free(der64);
	ERR_print_errors_fp(stderr);
	ERR_free_strings();
	return rc;
}
