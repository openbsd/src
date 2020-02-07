/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_RANDOM_H
#include <sys/random.h>
#endif

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "fido.h"

#if defined(_WIN32)
#include <windows.h>

#include <winternl.h>
#include <winerror.h>
#include <stdio.h>
#include <bcrypt.h>
#include <sal.h>

static int
obtain_nonce(uint64_t *nonce)
{
	NTSTATUS status;

	status = BCryptGenRandom(NULL, (unsigned char *)nonce, sizeof(*nonce),
	    BCRYPT_USE_SYSTEM_PREFERRED_RNG);

	if (!NT_SUCCESS(status))
		return (-1);

	return (0);
}
#elif defined(HAVE_ARC4RANDOM_BUF)
static int
obtain_nonce(uint64_t *nonce)
{
	arc4random_buf(nonce, sizeof(*nonce));
	return (0);
}
#elif defined(HAVE_GETRANDOM)
static int
obtain_nonce(uint64_t *nonce)
{
	if (getrandom(nonce, sizeof(*nonce), 0) < 0)
		return (-1);
	return (0);
}
#elif defined(HAVE_DEV_URANDOM)
static int
obtain_nonce(uint64_t *nonce)
{
	int	fd = -1;
	int	ok = -1;
	ssize_t	r;

	if ((fd = open(FIDO_RANDOM_DEV, O_RDONLY)) < 0)
		goto fail;
	if ((r = read(fd, nonce, sizeof(*nonce))) < 0 ||
	    (size_t)r != sizeof(*nonce))
		goto fail;

	ok = 0;
fail:
	if (fd != -1)
		close(fd);

	return (ok);
}
#else
#error "please provide an implementation of obtain_nonce() for your platform"
#endif /* _WIN32 */

#ifndef TLS
#define TLS
#endif

typedef struct dev_manifest_func_node {
	dev_manifest_func_t manifest_func;
	struct dev_manifest_func_node *next;
} dev_manifest_func_node_t;

static TLS dev_manifest_func_node_t *manifest_funcs = NULL;

static void
find_manifest_func_node(dev_manifest_func_t f, dev_manifest_func_node_t **curr,
    dev_manifest_func_node_t **prev)
{
	*prev = NULL;
	*curr = manifest_funcs;

	while (*curr != NULL && (*curr)->manifest_func != f) {
		*prev = *curr;
		*curr = (*curr)->next;
	}
}

static int
fido_dev_open_tx(fido_dev_t *dev, const char *path)
{
	const uint8_t cmd = CTAP_CMD_INIT;

	if (dev->io_handle != NULL) {
		fido_log_debug("%s: handle=%p", __func__, dev->io_handle);
		return (FIDO_ERR_INVALID_ARGUMENT);
	}

	if (dev->io.open == NULL || dev->io.close == NULL) {
		fido_log_debug("%s: NULL open/close", __func__);
		return (FIDO_ERR_INVALID_ARGUMENT);
	}

	if (obtain_nonce(&dev->nonce) < 0) {
		fido_log_debug("%s: obtain_nonce", __func__);
		return (FIDO_ERR_INTERNAL);
	}

	if ((dev->io_handle = dev->io.open(path)) == NULL) {
		fido_log_debug("%s: dev->io.open", __func__);
		return (FIDO_ERR_INTERNAL);
	}

	if (fido_tx(dev, cmd, &dev->nonce, sizeof(dev->nonce)) < 0) {
		fido_log_debug("%s: fido_tx", __func__);
		dev->io.close(dev->io_handle);
		dev->io_handle = NULL;
		return (FIDO_ERR_TX);
	}

	return (FIDO_OK);
}

static int
fido_dev_open_rx(fido_dev_t *dev, int ms)
{
	fido_cbor_info_t	*info = NULL;
	int			 reply_len;
	int			 r;

	if ((reply_len = fido_rx(dev, CTAP_CMD_INIT, &dev->attr,
	    sizeof(dev->attr), ms)) < 0) {
		fido_log_debug("%s: fido_rx", __func__);
		r = FIDO_ERR_RX;
		goto fail;
	}

#ifdef FIDO_FUZZ
	dev->attr.nonce = dev->nonce;
#endif

	if ((size_t)reply_len != sizeof(dev->attr) ||
	    dev->attr.nonce != dev->nonce) {
		fido_log_debug("%s: invalid nonce", __func__);
		r = FIDO_ERR_RX;
		goto fail;
	}

	dev->cid = dev->attr.cid;

	if (fido_dev_is_fido2(dev)) {
		if ((info = fido_cbor_info_new()) == NULL) {
			fido_log_debug("%s: fido_cbor_info_new", __func__);
			r = FIDO_ERR_INTERNAL;
			goto fail;
		}
		if (fido_dev_get_cbor_info_wait(dev, info, ms) != FIDO_OK) {
			fido_log_debug("%s: falling back to u2f", __func__);
			fido_dev_force_u2f(dev);
		}
	}

	if (fido_dev_is_fido2(dev) && info != NULL) {
		fido_log_debug("%s: FIDO_MAXMSG=%d, maxmsgsiz=%lu", __func__,
		    FIDO_MAXMSG, (unsigned long)fido_cbor_info_maxmsgsiz(info));
	}

	r = FIDO_OK;
fail:
	fido_cbor_info_free(&info);

	if (r != FIDO_OK) {
		dev->io.close(dev->io_handle);
		dev->io_handle = NULL;
	}

	return (r);
}

static int
fido_dev_open_wait(fido_dev_t *dev, const char *path, int ms)
{
	int r;

	if ((r = fido_dev_open_tx(dev, path)) != FIDO_OK ||
	    (r = fido_dev_open_rx(dev, ms)) != FIDO_OK)
		return (r);

	return (FIDO_OK);
}

int
fido_dev_register_manifest_func(const dev_manifest_func_t f)
{
	dev_manifest_func_node_t *prev, *curr, *n;

	find_manifest_func_node(f, &curr, &prev);
	if (curr != NULL)
		return (FIDO_OK);

	if ((n = calloc(1, sizeof(*n))) == NULL) {
		fido_log_debug("%s: calloc", __func__);
		return (FIDO_ERR_INTERNAL);
	}

	n->manifest_func = f;
	n->next = manifest_funcs;
	manifest_funcs = n;

	return (FIDO_OK);
}

void
fido_dev_unregister_manifest_func(const dev_manifest_func_t f)
{
	dev_manifest_func_node_t *prev, *curr;

	find_manifest_func_node(f, &curr, &prev);
	if (curr == NULL)
		return;
	if (prev != NULL)
		prev->next = curr->next;
	else
		manifest_funcs = curr->next;

	free(curr);
}

int
fido_dev_info_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	dev_manifest_func_node_t	*curr = NULL;
	dev_manifest_func_t		 m_func;
	size_t				 curr_olen;
	int				 r;

	*olen = 0;

	if (fido_dev_register_manifest_func(fido_hid_manifest) != FIDO_OK)
		return (FIDO_ERR_INTERNAL);

	for (curr = manifest_funcs; curr != NULL; curr = curr->next) {
		curr_olen = 0;
		m_func = curr->manifest_func;
		r = m_func(devlist + *olen, ilen - *olen, &curr_olen);
		if (r != FIDO_OK)
			return (r);
		*olen += curr_olen;
		if (*olen == ilen)
			break;
	}

	return (FIDO_OK);
}

int
fido_dev_open_with_info(fido_dev_t *dev)
{
	if (dev->path == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	return (fido_dev_open_wait(dev, dev->path, -1));
}

int
fido_dev_open(fido_dev_t *dev, const char *path)
{
	return (fido_dev_open_wait(dev, path, -1));
}

int
fido_dev_close(fido_dev_t *dev)
{
	if (dev->io_handle == NULL || dev->io.close == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	dev->io.close(dev->io_handle);
	dev->io_handle = NULL;

	return (FIDO_OK);
}

int
fido_dev_cancel(fido_dev_t *dev)
{
	if (fido_tx(dev, CTAP_CMD_CANCEL, NULL, 0) < 0)
		return (FIDO_ERR_TX);

	return (FIDO_OK);
}

int
fido_dev_set_io_functions(fido_dev_t *dev, const fido_dev_io_t *io)
{
	if (dev->io_handle != NULL) {
		fido_log_debug("%s: NULL handle", __func__);
		return (FIDO_ERR_INVALID_ARGUMENT);
	}

	if (io == NULL || io->open == NULL || io->close == NULL ||
	    io->read == NULL || io->write == NULL) {
		fido_log_debug("%s: NULL function", __func__);
		return (FIDO_ERR_INVALID_ARGUMENT);
	}

	dev->io = *io;

	return (FIDO_OK);
}

void
fido_init(int flags)
{
	if (flags & FIDO_DEBUG || getenv("FIDO_DEBUG") != NULL)
		fido_log_init();
}

fido_dev_t *
fido_dev_new(void)
{
	fido_dev_t *dev;

	if ((dev = calloc(1, sizeof(*dev))) == NULL)
		return (NULL);

	dev->cid = CTAP_CID_BROADCAST;
	dev->io = (fido_dev_io_t) {
		&fido_hid_open,
		&fido_hid_close,
		&fido_hid_read,
		&fido_hid_write,
		NULL,
		NULL,
	};

	return (dev);
}

fido_dev_t *
fido_dev_new_with_info(const fido_dev_info_t *di)
{
	fido_dev_t *dev;

	if ((dev = calloc(1, sizeof(*dev))) == NULL)
		return (NULL);

	dev->cid = CTAP_CID_BROADCAST;

	if (di->io.open == NULL || di->io.close == NULL ||
	    di->io.read == NULL || di->io.write == NULL) {
		fido_log_debug("%s: NULL function", __func__);
		fido_dev_free(&dev);
		return (NULL);
	}

	dev->io = di->io;
	if ((dev->path = strdup(di->path)) == NULL) {
		fido_log_debug("%s: strdup", __func__);
		fido_dev_free(&dev);
		return (NULL);
	}

	return (dev);
}

void
fido_dev_free(fido_dev_t **dev_p)
{
	fido_dev_t *dev;

	if (dev_p == NULL || (dev = *dev_p) == NULL)
		return;

	free(dev->path);
	free(dev);

	*dev_p = NULL;
}

uint8_t
fido_dev_protocol(const fido_dev_t *dev)
{
	return (dev->attr.protocol);
}

uint8_t
fido_dev_major(const fido_dev_t *dev)
{
	return (dev->attr.major);
}

uint8_t
fido_dev_minor(const fido_dev_t *dev)
{
	return (dev->attr.minor);
}

uint8_t
fido_dev_build(const fido_dev_t *dev)
{
	return (dev->attr.build);
}

uint8_t
fido_dev_flags(const fido_dev_t *dev)
{
	return (dev->attr.flags);
}

bool
fido_dev_is_fido2(const fido_dev_t *dev)
{
	return (dev->attr.flags & FIDO_CAP_CBOR);
}

void
fido_dev_force_u2f(fido_dev_t *dev)
{
	dev->attr.flags &= ~FIDO_CAP_CBOR;
}

void
fido_dev_force_fido2(fido_dev_t *dev)
{
	dev->attr.flags |= FIDO_CAP_CBOR;
}
