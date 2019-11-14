/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <stdlib.h>
#include "fido.h"

static int
fido_dev_reset_tx(fido_dev_t *dev)
{
	const unsigned char	cbor[] = { CTAP_CBOR_RESET };
	const uint8_t		cmd = CTAP_FRAME_INIT | CTAP_CMD_CBOR;

	if (tx(dev, cmd, cbor, sizeof(cbor)) < 0) {
		log_debug("%s: tx", __func__);
		return (FIDO_ERR_TX);
	}

	return (FIDO_OK);
}

static int
fido_dev_reset_wait(fido_dev_t *dev, int ms)
{
	int r;

	if ((r = fido_dev_reset_tx(dev)) != FIDO_OK ||
	    (r = rx_cbor_status(dev, ms)) != FIDO_OK)
		return (r);

	return (FIDO_OK);
}

int
fido_dev_reset(fido_dev_t *dev)
{
	return (fido_dev_reset_wait(dev, -1));
}
