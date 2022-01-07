/*	$OpenBSD: dsa_meth.c,v 1.2 2022/01/07 09:35:36 tb Exp $	*/
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

#include <openssl/dsa.h>
#include <openssl/err.h>

#include "dsa_locl.h"

DSA_METHOD *
DSA_meth_new(const char *name, int flags)
{
	DSA_METHOD *meth;

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
DSA_meth_free(DSA_METHOD *meth)
{
	if (meth != NULL) {
		free((char *)meth->name);
		free(meth);
	}
}

DSA_METHOD *
DSA_meth_dup(const DSA_METHOD *meth)
{
	DSA_METHOD *copy;

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
DSA_meth_set_sign(DSA_METHOD *meth,
    DSA_SIG *(*sign)(const unsigned char *, int, DSA *))
{
	meth->dsa_do_sign = sign;
	return 1;
}

int
DSA_meth_set_finish(DSA_METHOD *meth, int (*finish)(DSA *))
{
	meth->finish = finish;
	return 1;
}
