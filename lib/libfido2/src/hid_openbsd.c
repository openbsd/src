/*
 * Copyright (c) 2019 Google LLC. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <sys/types.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <usbhid.h>
#include <poll.h>

#include "fido.h"

#define MAX_UHID	64
#define MAX_U2FHID_LEN	64

struct hid_openbsd {
	int fd;
	size_t report_in_len;
	size_t report_out_len;
};

int
fido_hid_manifest(fido_dev_info_t *devlist, size_t ilen, size_t *olen)
{
	size_t i;
	char path[64];
	int fd;
	struct usb_device_info udi;
	fido_dev_info_t *di;

	if (ilen == 0)
		return (FIDO_OK); /* nothing to do */

	if (devlist == NULL || olen == NULL)
		return (FIDO_ERR_INVALID_ARGUMENT);

	for (i = *olen = 0; i < MAX_UHID && *olen < ilen; i++) {
		snprintf(path, sizeof(path), "/dev/fido/%zu", i);
		if ((fd = open(path, O_RDWR)) == -1) {
			if (errno != ENOENT && errno != ENXIO) {
				fido_log_debug("%s: open %s: %s", __func__,
				    path, strerror(errno));
			}
			continue;
		}
		close(fd);

		memset(&udi, 0, sizeof(udi));
		strlcpy(udi.udi_vendor, "OpenBSD", sizeof(udi.udi_vendor));
		strlcpy(udi.udi_product, "fido(4)", sizeof(udi.udi_product));
		udi.udi_vendorNo = 0x0b5d; /* stolen from PCI_VENDOR_OPENBSD */

		fido_log_debug("%s: %s: vendor = \"%s\", product = \"%s\"",
		    __func__, path, udi.udi_vendor, udi.udi_product);

		di = &devlist[*olen];
		memset(di, 0, sizeof(*di));
		if ((di->path = strdup(path)) == NULL ||
		    (di->manufacturer = strdup(udi.udi_vendor)) == NULL ||
		    (di->product = strdup(udi.udi_product)) == NULL) {
			free(di->path);
			free(di->manufacturer);
			free(di->product);
			explicit_bzero(di, sizeof(*di));
			return FIDO_ERR_INTERNAL;
		}
		di->vendor_id = udi.udi_vendorNo;
		di->product_id = udi.udi_productNo;
		di->io = (fido_dev_io_t) {
			fido_hid_open,
			fido_hid_close,
			fido_hid_read,
			fido_hid_write,
		};
		(*olen)++;
	}

	return FIDO_OK;
}

void *
fido_hid_open(const char *path)
{
	struct hid_openbsd *ret = NULL;

	if ((ret = calloc(1, sizeof(*ret))) == NULL ||
	    (ret->fd = open(path, O_RDWR)) < 0) {
		free(ret);
		return (NULL);
	}
	ret->report_in_len = ret->report_out_len = MAX_U2FHID_LEN;
	fido_log_debug("%s: inlen = %zu outlen = %zu", __func__,
	    ret->report_in_len, ret->report_out_len);

	return (ret);
}

void
fido_hid_close(void *handle)
{
	struct hid_openbsd *ctx = (struct hid_openbsd *)handle;

	close(ctx->fd);
	free(ctx);
}

int
waitfd(int fd, int ms)
{
	struct timespec ts_start, ts_now, ts_delta;
	struct pollfd pfd;
	int ms_remain, r;

	if (ms < 0)
		return 0;
	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = fd;
	pfd.events = POLLIN;
	if (clock_gettime(CLOCK_MONOTONIC, &ts_start) != 0) {
		fido_log_debug("%s: clock_gettime: %s",
		    __func__, strerror(errno));
		return -1;
	}
	for (ms_remain = ms; ms_remain > 0;) {
		if ((r = poll(&pfd, 1, ms_remain)) > 0)
			return 0;
		else if (r == 0)
			break;
		else if (errno != EINTR) {
			fido_log_debug("%s: poll: %s",
			    __func__, strerror(errno));
			return -1;
		}
		/* poll interrupted - subtract time already waited */
		if (clock_gettime(CLOCK_MONOTONIC, &ts_now) != 0) {
			fido_log_debug("%s: clock_gettime: %s",
			    __func__, strerror(errno));
			return -1;
		}
		timespecsub(&ts_now, &ts_start, &ts_delta);
		ms_remain = ms - ((ts_delta.tv_sec * 1000) +
		    (ts_delta.tv_nsec / 1000000));
	}
	return -1;
}

int
fido_hid_read(void *handle, unsigned char *buf, size_t len, int ms)
{
	struct hid_openbsd *ctx = (struct hid_openbsd *)handle;
	ssize_t r;

	fido_log_debug("%s: %zu timeout %d", __func__, len, ms);
	if (len != ctx->report_in_len) {
		fido_log_debug("%s: invalid len: got %zu, want %zu", __func__,
		    len, ctx->report_in_len);
		return (-1);
	}
	if (waitfd(ctx->fd, ms) != 0) {
		fido_log_debug("%s: fd not ready", __func__);
		return (-1);
	}
	if ((r = read(ctx->fd, buf, len)) == -1 || (size_t)r != len) {
		fido_log_debug("%s: read: %s", __func__, strerror(errno));
		return (-1);
	}
	return ((int)len);
}

int
fido_hid_write(void *handle, const unsigned char *buf, size_t len)
{
	struct hid_openbsd *ctx = (struct hid_openbsd *)handle;
	ssize_t r;

	if (len != ctx->report_out_len + 1) {
		fido_log_debug("%s: invalid len: got %zu, want %zu", __func__,
		    len, ctx->report_out_len);
		return (-1);
	}
	if ((r = write(ctx->fd, buf + 1, len - 1)) == -1 ||
	    (size_t)r != len - 1) {
		fido_log_debug("%s: write: %s", __func__, strerror(errno));
		return (-1);
	}
	return ((int)len);
}

size_t
fido_hid_report_in_len(void *handle)
{
	struct hid_openbsd *ctx = handle;

	return (ctx->report_in_len);
}

size_t
fido_hid_report_out_len(void *handle)
{
	struct hid_openbsd *ctx = handle;

	return (ctx->report_out_len);
}
