/*	$OpenBSD: rsa_meth.c,v 1.2 2018/09/12 06:35:38 djm Exp $	*/
/*
 * Copyright (c) 2018 Theo Buehler <tb@openbsd.org>
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

#include <stdlib.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/rsa.h>

RSA_METHOD *
RSA_meth_new(const char *name, int flags)
{
	RSA_METHOD *meth;

	if ((meth = calloc(1, sizeof(*meth))) == NULL)
		return NULL;
	if ((meth->name = strdup(name)) == NULL) {
		free(meth);
		return NULL;
	}
	meth->flags = flags;

	return meth;
}

void
RSA_meth_free(RSA_METHOD *meth)
{
	if (meth != NULL) {
		free((char *)meth->name);
		free(meth);
	}
}

RSA_METHOD *
RSA_meth_dup(const RSA_METHOD *meth)
{
	RSA_METHOD *copy;

	if ((copy = calloc(1, sizeof(*copy))) == NULL)
		return NULL;
	memcpy(copy, meth, sizeof(*copy));
	if ((copy->name = strdup(meth->name)) == NULL) {
		free(copy);
		return NULL;
	}

	return copy;
}

int
RSA_meth_set1_name(RSA_METHOD *meth, const char *name)
{
	char *copy;

	if ((copy = strdup(name)) == NULL)
		return 0;
	free((char *)meth->name);
	meth->name = copy;
	return 1;
}

int
(*RSA_meth_get_finish(const RSA_METHOD *meth))(RSA *rsa)
{
	return meth->finish;
}

int
RSA_meth_set_priv_enc(RSA_METHOD *meth, int (*priv_enc)(int flen,
    const unsigned char *from, unsigned char *to, RSA *rsa, int padding))
{
	meth->rsa_priv_enc = priv_enc;
	return 1;
}

int
RSA_meth_set_priv_dec(RSA_METHOD *meth, int (*priv_dec)(int flen,
    const unsigned char *from, unsigned char *to, RSA *rsa, int padding))
{
	meth->rsa_priv_dec = priv_dec;
	return 1;
}

int
RSA_meth_set_finish(RSA_METHOD *meth, int (*finish)(RSA *rsa))
{
	meth->finish = finish;
	return 1;
}
