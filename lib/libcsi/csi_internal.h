/* $OpenBSD: csi_internal.h,v 1.2 2018/06/02 17:43:14 jsing Exp $ */
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

#ifndef HEADER_CSI_INTERNAL_H
#define HEADER_CSI_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include <openssl/bn.h>
#include <openssl/dh.h>

__BEGIN_HIDDEN_DECLS

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

#define CSI_MAX_BIGNUM_BYTES		(16384 / 8)
#define CSI_MIN_DH_LENGTH		256

struct csi_err {
	u_int code;
	int errnum;
	char *msg;
};

struct csi_dh{
	struct csi_err err;
	DH *dh;
	BIGNUM *peer_pubkey;
};

void csi_err_clear(struct csi_err *_err);
int csi_err_set(struct csi_err *_err, u_int _code, const char *_fmt, ...);
int csi_err_setx(struct csi_err *_err, u_int _code, const char *_fmt, ...);

int csi_integer_to_bn(struct csi_err *_err, const char *_field,
    struct csi_integer *_value, BIGNUM **_bn);
int csi_bn_to_integer(struct csi_err *_err, BIGNUM *_bn,
    struct csi_integer *_integer);

struct csi_dh_params *csi_dh_params_dup(struct csi_dh_params *_cdhp);
int csi_dh_public_is_valid(struct csi_dh *_cdh, BIGNUM *_pubkey);

__END_HIDDEN_DECLS

#endif /* HEADER_CSI_INTERNAL_H */
