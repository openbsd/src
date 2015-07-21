/*	$OpenBSD: pvvar.h,v 1.1 2015/07/21 03:38:22 reyk Exp $	*/

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

struct pvbus_softc {
	struct device		 pvbus_dev;
	uint8_t			 pvbus_types;
};

struct pvbus_attach_args {
	const char		*pvba_busname;
};

struct pv_attach_args {
	const char		*pva_busname;
	uint8_t			 pva_type;	/* required hv type */
	uint8_t			 pva_types;	/* detected hv types */
};

extern int has_hv_cpuid;

#define PVBUS_KVM	0x01
#define PVBUS_HYPERV	0x02
#define PVBUS_VMWARE	0x04
#define PVBUS_XEN	0x08

int	 pvbus_probe(void);

#endif /* _DEV_PV_PVBUS_H_ */
