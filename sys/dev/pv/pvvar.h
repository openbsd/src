/*	$OpenBSD: pvvar.h,v 1.4 2015/07/28 09:48:52 reyk Exp $	*/

/*
 * Copyright (c) 2015 Reyk Floeter <reyk@openbsd.org>
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

#ifndef _DEV_PV_PVVAR_H_
#define _DEV_PV_PVVAR_H_

enum {
	PVBUS_KVM,
	PVBUS_HYPERV,
	PVBUS_VMWARE,
	PVBUS_XEN,
	PVBUS_BHYVE,

	PVBUS_MAX
};

struct pvbus_hv {
	uint32_t		 hv_base;
	uint32_t		 hv_features;
	uint32_t		 hv_version;
};

struct pvbus_softc {
	struct device		 pvbus_dev;
	struct pvbus_hv		 pvbus_hv[PVBUS_MAX];
};

struct pvbus_attach_args {
	const char		*pvba_busname;
};

struct pv_attach_args {
	const char		*pva_busname;
	struct pvbus_hv		*pva_hv;
};

extern int has_hv_cpuid;

int	 pvbus_probe(void);

#endif /* _DEV_PV_PVBUS_H_ */
