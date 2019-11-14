/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <string.h>
#include "fido.h"

static int
parse_authkey(const cbor_item_t *key, const cbor_item_t *val, void *arg)
{
	es256_pk_t *authkey = arg;

	if (cbor_isa_uint(key) == false ||
	    cbor_int_get_width(key) != CBOR_INT_8 ||
	    cbor_get_uint8(key) != 1) {
		log_debug("%s: cbor type", __func__);
		return (0); /* ignore */
	}

	return (es256_pk_decode(val, authkey));
}

static int
fido_dev_authkey_tx(fido_dev_t *dev)
{
	fido_blob_t	 f;
	cbor_item_t	*argv[2];
	int		 r;

	log_debug("%s: dev=%p", __func__, (void *)dev);

	memset(&f, 0, sizeof(f));
	memset(argv, 0, sizeof(argv));

	/* add command parameters */
	if ((argv[0] = cbor_build_uint8(1)) == NULL ||
	    (argv[1] = cbor_build_uint8(2)) == NULL) {
		log_debug("%s: cbor_build", __func__);
		r = FIDO_ERR_INTERNAL;
		goto fail;
	}

	/* frame and transmit */
	if (cbor_build_frame(CTAP_CBOR_CLIENT_PIN, argv, 2, &f) < 0 ||
	    tx(dev, CTAP_FRAME_INIT | CTAP_CMD_CBOR, f.ptr, f.len) < 0) {
		log_debug("%s: tx", __func__);
		r = FIDO_ERR_TX;
		goto fail;
	}

	r = FIDO_OK;
fail:
	cbor_vector_free(argv, nitems(argv));
	free(f.ptr);

	return (r);
}

static int
fido_dev_authkey_rx(fido_dev_t *dev, es256_pk_t *authkey, int ms)
{
	const uint8_t	cmd = CTAP_FRAME_INIT | CTAP_CMD_CBOR;
	unsigned char	reply[2048];
	int		reply_len;

	log_debug("%s: dev=%p, authkey=%p, ms=%d", __func__, (void *)dev,
	    (void *)authkey, ms);

	memset(authkey, 0, sizeof(*authkey));

	if ((reply_len = rx(dev, cmd, &reply, sizeof(reply), ms)) < 0) {
		log_debug("%s: rx", __func__);
		return (FIDO_ERR_RX);
	}

	return (parse_cbor_reply(reply, (size_t)reply_len, authkey,
	    parse_authkey));
}

static int
fido_dev_authkey_wait(fido_dev_t *dev, es256_pk_t *authkey, int ms)
{
	int r;

	if ((r = fido_dev_authkey_tx(dev)) != FIDO_OK ||
	    (r = fido_dev_authkey_rx(dev, authkey, ms)) != FIDO_OK)
		return (r);

	return (FIDO_OK);
}

int
fido_dev_authkey(fido_dev_t *dev, es256_pk_t *authkey)
{
	return (fido_dev_authkey_wait(dev, authkey, -1));
}
