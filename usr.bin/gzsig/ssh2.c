/* $OpenBSD: ssh2.c,v 1.3 2009/07/12 18:04:03 jsg Exp $ */
/*
 * ssh2.c
 *
 * Copyright (c) 2005 Marius Eriksen <marius@openbsd.org>
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
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <openssl/ssl.h>
#include <openssl/des.h>
#include <openssl/md5.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <resolv.h>
#include <err.h>

#include "key.h"
#include "ssh2.h"

#define GET_32BIT(cp) (((u_long)(u_char)(cp)[0] << 24) | \
		       ((u_long)(u_char)(cp)[1] << 16) | \
		       ((u_long)(u_char)(cp)[2] << 8) | \
		       ((u_long)(u_char)(cp)[3]))

/* From OpenSSH */
static int
_uudecode(const char *src, u_char *target, size_t targsize)
{
	int len;
	char *encoded, *p;

	/* copy the 'readonly' source */
	if ((encoded = strdup(src)) == NULL)
		err(1, "strdup");
	/* skip whitespace and data */
	for (p = encoded; *p == ' ' || *p == '\t'; p++)
		;
	for (; *p != '\0' && *p != ' ' && *p != '\t'; p++)
		;
	/* and remove trailing whitespace because __b64_pton needs this */
	*p = '\0';
	len = __b64_pton(encoded, target, targsize);

	free(encoded);

	return len;
}

/*
 * Small compatibility layer for the OpenSSH buffers.  Only what we
 * need here.
 */

static int
_keyfromstr(char *str, int len)
{
	if (strncmp(str, "rsa", len) == 0 ||
	    strncmp(str, "ssh-rsa", len) == 0)
		return KEY_RSA;
	else if (strncmp(str, "dsa", len) == 0 ||
	    strncmp(str, "ssh-dss", len) == 0)
		return KEY_DSA;

	return (-1);
}

static int
_read_int(struct iovec *iov, int *ival)
{
	iov->iov_len -= 4;
	if (iov->iov_len < 0)
		return (-1);
	*ival = GET_32BIT((u_char *)iov->iov_base);
	iov->iov_base = (u_char*)iov->iov_base + 4;

	return (0);
}

static int
_read_opaque(struct iovec *iov, u_char **buf, int *len)
{
	if (_read_int(iov, len) < 0 || *len < 0)
		return (-1);

	iov->iov_len -= *len;
	if (iov->iov_len < 0)
		return (-1);

	*buf = iov->iov_base;
	iov->iov_base = (u_char*)iov->iov_base + *len;

	return (0);
}

static int
_read_bignum(struct iovec *iov, BIGNUM *bn)
{
	u_char *bp;
	int blen;

	if (_read_opaque(iov, &bp, &blen) < 0)
		return (-1);

	if ((blen > 0 && bp[0] & 0x80) ||  /* No negative values */
	    (blen > 8*1024))		   /* Too large */
		return (-1);

	BN_bin2bn(bp, blen, bn);

	return (0);
}

int
ssh2_load_public(struct key *k, struct iovec *iovp)
{
	int len, keytype, error = 0;
	u_char *bp;
	struct iovec iov;
	/* iov->iov_base is NULL terminated */
	char *cp0, *savep = NULL, *cp = iovp->iov_base;

	if ((cp0 = strchr(cp, ' ')) == NULL)
		return (-1);

	len = cp0 - cp;

	if ((keytype = _keyfromstr(cp, len)) < 0)
		return (-1);

	/* cp0 is a space (' '), so we have at least one more */
	cp = cp0 + 1;

	len = 2*strlen(cp);
	if ((savep = iov.iov_base = malloc(len)) == NULL)
		err(1, "malloc(%d)", len);
	iov.iov_len = _uudecode(cp, iov.iov_base, len);

	if (_read_opaque(&iov, &bp, &len) < 0 ||
	    keytype != _keyfromstr(bp, len)) {
		error = -1;
		goto out;
	}

	k->type = keytype;
	switch (keytype) {
	case KEY_RSA: {
		RSA *rsa;

		if ((rsa = RSA_new()) == NULL ||
		    (rsa->e = BN_new()) == NULL ||
		    (rsa->n = BN_new()) == NULL)
			errx(1, "BN_new");

		if (_read_bignum(&iov, rsa->e) < 0 ||
		    _read_bignum(&iov, rsa->n) < 0) {
			error = -1;
			RSA_free(rsa);
			goto out;
		}

		k->data = (void *)rsa;

		break;
	}
	case KEY_DSA: {
		DSA *dsa;

		if ((dsa = DSA_new()) == NULL ||
		    (dsa->p = BN_new()) == NULL ||
		    (dsa->q = BN_new()) == NULL ||
		    (dsa->g = BN_new()) == NULL ||
		    (dsa->pub_key = BN_new()) == NULL)
			errx(1, "BN_new");

		if (_read_bignum(&iov, dsa->p) < 0 ||
		    _read_bignum(&iov, dsa->q) < 0 ||
		    _read_bignum(&iov, dsa->g) < 0 ||
		    _read_bignum(&iov, dsa->pub_key) < 0) {
			error = -1;
			DSA_free(dsa);
			goto out;
		}

		k->data = (void *)dsa;

		break;
	}
	default:
		error = -1;
	}

#if 0
	if (iov->iov_len != 0)
		/* Sanity check. */
		return (-1);
#endif


out:
	if (savep != NULL)
		free(savep);
	return (error);
}
