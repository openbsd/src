/*	$OpenBSD: drm_linux.c,v 1.2 2015/04/06 12:25:10 jsg Exp $	*/
/*
 * Copyright (c) 2013 Jonathan Gray <jsg@openbsd.org>
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

#include <dev/pci/drm/drmP.h>

struct timespec
ns_to_timespec(const int64_t nsec)
{
	struct timespec ts;
	int32_t rem;

	if (nsec == 0) {
		ts.tv_sec = 0;
		ts.tv_nsec = 0;
		return (ts);
	}

	ts.tv_sec = nsec / NSEC_PER_SEC;
	rem = nsec % NSEC_PER_SEC;
	if (rem < 0) {
		ts.tv_sec--;
		rem += NSEC_PER_SEC;
	}
	ts.tv_nsec = rem;
	return (ts);
}

int64_t
timeval_to_ns(const struct timeval *tv)
{
	return ((int64_t)tv->tv_sec * NSEC_PER_SEC) +
		tv->tv_usec * NSEC_PER_USEC;
}

struct timeval
ns_to_timeval(const int64_t nsec)
{
	struct timeval tv;
	int32_t rem;

	if (nsec == 0) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		return (tv);
	}

	tv.tv_sec = nsec / NSEC_PER_SEC;
	rem = nsec % NSEC_PER_SEC;
	if (rem < 0) {
		tv.tv_sec--;
		rem += NSEC_PER_SEC;
	}
	tv.tv_usec = rem / 1000;
	return (tv);
}

extern char *hw_vendor, *hw_prod;

static bool
dmi_found(const struct dmi_system_id *dsi)
{
	int i, slot;

	for (i = 0; i < nitems(dsi->matches); i++) {
		slot = dsi->matches[i].slot;
		switch (slot) {
		case DMI_NONE:
			break;
		case DMI_SYS_VENDOR:
		case DMI_BOARD_VENDOR:
			if (hw_vendor != NULL &&
			    !strcmp(hw_vendor, dsi->matches[i].substr))
				break;
			else
				return false;
		case DMI_PRODUCT_NAME:
		case DMI_BOARD_NAME:
			if (hw_prod != NULL &&
			    !strcmp(hw_prod, dsi->matches[i].substr))
				break;
			else
				return false;
		default:
			return false;
		}
	}

	return true;
}

int
dmi_check_system(const struct dmi_system_id *sysid)
{
	const struct dmi_system_id *dsi;
	int num = 0;

	for (dsi = sysid; dsi->matches[0].slot != 0 ; dsi++) {
		if (dmi_found(dsi)) {
			num++;
			if (dsi->callback && dsi->callback(dsi))
				break;
		}
	}
	return (num);
}
