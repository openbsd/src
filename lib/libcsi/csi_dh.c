/* $OpenBSD: csi_dh.c,v 1.2 2018/06/02 17:43:14 jsing Exp $ */
/*
 * Copyright (c) 2000, 2001, 2015 Markus Friedl <markus@openbsd.org>
 * Copyright (c) 2006, 2016 Damien Miller <djm@openbsd.org>
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

#include <limits.h>
#include <string.h>

#include <openssl/ec.h>
#include <openssl/ecdh.h>

#include <csi.h>

#include "csi_internal.h"

struct csi_dh *
csi_dh_new()
{
	return calloc(1, sizeof(struct csi_dh));
}

static void
csi_dh_reset(struct csi_dh *cdh)
{
	DH_free(cdh->dh);
	cdh->dh = NULL;

	BN_free(cdh->peer_pubkey);
	cdh->peer_pubkey = NULL;

	csi_err_clear(&cdh->err);
}

void
csi_dh_free(struct csi_dh *cdh)
{
	if (cdh == NULL)
		return;

	csi_dh_reset(cdh);

	freezero(cdh, sizeof(*cdh));
}

const char *
csi_dh_error(struct csi_dh *cdh)
{
	return cdh->err.msg;
}

int
csi_dh_error_code(struct csi_dh *cdh)
{
	return cdh->err.code;
}

static int
csi_dh_init(struct csi_dh *cdh)
{
	csi_dh_reset(cdh);

	if ((cdh->dh = DH_new()) == NULL) {
		csi_err_setx(&cdh->err, CSI_ERR_MEM, "out of memory");
		return -1;
	}

	return 0;
}

struct csi_dh_params *
csi_dh_params_dup(struct csi_dh_params *cdhp)
{
	struct csi_dh_params *ncdhp = NULL;

	if ((ncdhp = calloc(1, sizeof(*ncdhp))) == NULL)
		goto err;

	if ((ncdhp->p.data = malloc(cdhp->p.len)) == NULL)
		goto err;
	ncdhp->p.len = cdhp->p.len;
	memcpy((uint8_t *)ncdhp->p.data, cdhp->p.data, cdhp->p.len);

	if ((ncdhp->g.data = malloc(cdhp->g.len)) == NULL)
		goto err;
	ncdhp->g.len = cdhp->g.len;
	memcpy((uint8_t *)ncdhp->g.data, cdhp->g.data, cdhp->g.len);

	return ncdhp;

 err:
	csi_dh_params_free(ncdhp);

	return NULL;
}

void
csi_dh_params_free(struct csi_dh_params *cdhp)
{
	if (cdhp == NULL)
		return;

	free((uint8_t *)cdhp->p.data);
	free((uint8_t *)cdhp->g.data);
	free(cdhp);
}

void
csi_dh_public_free(struct csi_dh_public *cdhp)
{
	if (cdhp == NULL)
		return;

	free((uint8_t *)cdhp->key.data);
	free(cdhp);
}

void
csi_dh_shared_free(struct csi_dh_shared *cdhs)
{
	if (cdhs == NULL)
		return;

	freezero((uint8_t *)cdhs->key.data, cdhs->key.len);
	freezero(cdhs, sizeof(*cdhs));
}

int
csi_dh_set_params(struct csi_dh *cdh, struct csi_dh_params *params)
{
	if (csi_dh_init(cdh) == -1)
		goto err;

	if (csi_integer_to_bn(&cdh->err, "p", &params->p,
	    &cdh->dh->p) == -1)
		goto err;
	if (csi_integer_to_bn(&cdh->err, "g", &params->g,
	    &cdh->dh->g) == -1)
		goto err;

	return 0;

 err:
	return -1;
}

int
csi_dh_public_is_valid(struct csi_dh *cdh, BIGNUM *pubkey)
{
	BIGNUM *tmp = NULL;
        int bits_set = 0;
	int rv = 0;
	int i;

	if ((tmp = BN_new()) == NULL) {
		csi_err_setx(&cdh->err, CSI_ERR_MEM, "out of memory");
		goto bad;
	}

	if (BN_is_negative(pubkey)) {
		csi_err_setx(&cdh->err, CSI_ERR_INVAL,
		    "invalid DH public key value (negative)");
		goto bad;
	}

	if (BN_cmp(pubkey, BN_value_one()) != 1) {
		csi_err_setx(&cdh->err, CSI_ERR_INVAL,
		    "invalid DH public key value (<= 1)");
		goto bad;
	}

	if (!BN_sub(tmp, cdh->dh->p, BN_value_one()) ||
	    BN_cmp(pubkey, tmp) != -1) {
		csi_err_setx(&cdh->err, CSI_ERR_INVAL,
		    "invalid DH public key value (>= p-1)");
		goto bad;
	}

	/*
	 * If g == 2 and bits_set == 1, then computing log_g(pubkey) is trivial.
	 */
	for (i = 0; i <= BN_num_bits(pubkey); i++) {
		if (BN_is_bit_set(pubkey, i))
			bits_set++;
	}
	if (bits_set < 4) {
		csi_err_setx(&cdh->err, CSI_ERR_INVAL,
		    "invalid DH public key value (%d/%d bits)",
		    bits_set, BN_num_bits(cdh->dh->p));
		goto bad;
	}

	rv = 1;

 bad:
	BN_clear_free(tmp);

	return rv;
}

int
csi_dh_set_peer_public(struct csi_dh *cdh, struct csi_dh_public *peer)
{
	BIGNUM *ppk = NULL;

	if (cdh->dh == NULL) {
		csi_err_setx(&cdh->err, CSI_ERR_INVAL, "no params set");
		goto err;
	}

	if (csi_integer_to_bn(&cdh->err, "key", &peer->key, &ppk) == -1)
		goto err;
	if (!csi_dh_public_is_valid(cdh, ppk))
		goto err;

	cdh->peer_pubkey = ppk;

	return 0;

 err:
	BN_clear_free(ppk);

	return -1;
}

struct csi_dh_params *
csi_dh_params(struct csi_dh *cdh)
{
	struct csi_dh_params *cdhp;

	if ((cdhp = calloc(1, sizeof(*cdhp))) == NULL)
		goto errmem;
	if (csi_bn_to_integer(&cdh->err, cdh->dh->p, &cdhp->p) != 0)
		goto err;
	if (csi_bn_to_integer(&cdh->err, cdh->dh->g, &cdhp->g) != 0)
		goto err;

	return cdhp;

 errmem:
	csi_err_setx(&cdh->err, CSI_ERR_MEM, "out of memory");
 err:
	csi_dh_params_free(cdhp);

	return NULL;
}

struct csi_dh_public *
csi_dh_public_key(struct csi_dh *cdh)
{
	struct csi_dh_public *cdhp;

	if ((cdhp = calloc(1, sizeof(*cdhp))) == NULL)
		goto errmem;
	if (csi_bn_to_integer(&cdh->err, cdh->dh->pub_key, &cdhp->key) != 0)
		goto err;

	return cdhp;

 errmem:
	csi_err_setx(&cdh->err, CSI_ERR_MEM, "out of memory");
 err:
	csi_dh_public_free(cdhp);

	return NULL;
}

struct csi_dh_public *
csi_dh_peer_public_key(struct csi_dh *cdh)
{
	struct csi_dh_public *cdhp;

	if ((cdhp = calloc(1, sizeof(*cdhp))) == NULL)
		goto errmem;
	if (csi_bn_to_integer(&cdh->err, cdh->peer_pubkey, &cdhp->key) != 0)
		goto err;

	return cdhp;

 errmem:
	csi_err_setx(&cdh->err, CSI_ERR_MEM, "out of memory");
 err:
	csi_dh_public_free(cdhp);

	return NULL;
}

int
csi_dh_generate_keys(struct csi_dh *cdh, size_t length,
    struct csi_dh_public **public)
{
	int pbits;

	if (cdh->dh == NULL) {
		csi_err_setx(&cdh->err, CSI_ERR_INVAL, "no params set");
		goto err;
	}

	if (length > 0) {
		if (length > INT_MAX / 2) {
			csi_err_setx(&cdh->err, CSI_ERR_INVAL,
			    "length too large");
			goto err;
		}
		if (length < CSI_MIN_DH_LENGTH)
			length = CSI_MIN_DH_LENGTH;

		/*
		 * Pollard Rho, Big Step/Little Step attacks are O(sqrt(n)),
		 * so double requested length.
		 */
		length *= 2;

		if ((pbits = BN_num_bits(cdh->dh->p)) <= 0) {
			csi_err_setx(&cdh->err, CSI_ERR_CRYPTO,
			    "invalid p bignum");
			goto err;
		}
		if ((int)length > pbits) {
			csi_err_setx(&cdh->err, CSI_ERR_INVAL,
			    "length too large");
			goto err;
		}

		cdh->dh->length = MINIMUM((int)length, pbits - 1);
	}

	if (!DH_generate_key(cdh->dh)) {
		csi_err_setx(&cdh->err, CSI_ERR_CRYPTO, "dh generation failed");
		goto err;
	}

	if (!csi_dh_public_is_valid(cdh, cdh->dh->pub_key))
		goto err;

	if (public != NULL) {
		csi_dh_public_free(*public);
		if ((*public = csi_dh_public_key(cdh)) == NULL)
			goto err;
	}

	return 0;

 err:
	return -1;
}

int
csi_dh_derive_shared_key(struct csi_dh *cdh, struct csi_dh_shared **cdhs)
{
	struct csi_dh_shared *dhs = NULL;
	uint8_t *key = NULL;
	size_t key_len = 0;
	int len;

	csi_dh_shared_free(*cdhs);
	*cdhs = NULL;

	if (cdh->dh == NULL) {
		csi_err_setx(&cdh->err, CSI_ERR_INVAL, "no params set");
		goto err;
	}

	if ((len = DH_size(cdh->dh)) <= 0) {
		csi_err_setx(&cdh->err, CSI_ERR_INVAL, "invalid dh size %i", len);
		goto err;
	}
	key_len = (size_t)len;
	if ((key = calloc(1, key_len)) == NULL)
		goto errmem;
	if (DH_compute_key(key, cdh->peer_pubkey, cdh->dh) != len) {
		csi_err_setx(&cdh->err, CSI_ERR_CRYPTO, "failed to derive key");
		goto err;
	}

	if ((dhs = calloc(1, sizeof(*dhs))) == NULL)
		goto errmem;
	dhs->key.data = key;
	dhs->key.len = key_len;

	*cdhs = dhs;

	return 0;

 errmem:
	csi_err_setx(&cdh->err, CSI_ERR_MEM, "out of memory");
 err:
	return -1;
}
