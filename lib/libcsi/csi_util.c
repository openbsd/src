/* $OpenBSD: csi_util.c,v 1.1 2018/06/02 17:40:33 jsing Exp $ */
/*
 * Copyright (c) 2018 Joel Sing <jsing@openbsd.org>
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

#include <openssl/bn.h>

#include "csi.h"
#include "csi_internal.h"

int
csi_integer_to_bn(struct csi_err *err, const char *field,
    struct csi_integer *integer, BIGNUM **bn)
{
	BN_clear_free(*bn);
	*bn = NULL;

	if (integer->len > CSI_MAX_BIGNUM_BYTES) {
		csi_err_setx(err, CSI_ERR_INVAL, "%s too large", field);
                goto err;
	}
	if ((*bn = BN_bin2bn(integer->data, integer->len, NULL)) == NULL) {
		csi_err_setx(err, CSI_ERR_MEM, "out of memory");
		goto err;
	}
	return 0;

 err:
	return -1;
}

int
csi_bn_to_integer(struct csi_err *err, BIGNUM *bn, struct csi_integer *integer)
{
	uint8_t *b = NULL;
	int len = 0;

	freezero((uint8_t *)integer->data, integer->len);
	integer->data = NULL;
	integer->len = 0;

	len = BN_num_bytes(bn);
        if (len < 0 || len > CSI_MAX_BIGNUM_BYTES) {
		csi_err_setx(err, CSI_ERR_INVAL,
		    "invalid bignum length %i", len);
                goto err;
	}
	/* XXX - prepend zero to avoid interpretation as negative? */
	if ((b = calloc(1, len)) == NULL)
		goto errmem;
        if (BN_bn2bin(bn, b) != len)
                goto errmem;

	integer->data = b;
	integer->len = (size_t)len;

	return 0;

 errmem:
	csi_err_setx(err, CSI_ERR_MEM, "out of memory");
 err:
	freezero(b, len);

	return -1;
}
