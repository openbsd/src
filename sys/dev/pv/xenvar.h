/*
 * Copyright (c) 2015 Mike Belopuhov
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

#ifndef _XENVAR_H_
#define _XENVAR_H_

#define XEN_DEBUG

#ifdef XEN_DEBUG
#define DPRINTF(x...)		printf(x)
#else
#define DPRINTF(x...)
#endif

struct xen_softc {
	struct device		 sc_dev;
	uint32_t		 sc_base;
	void			*sc_hc;
	uint32_t		 sc_features;
#define  XENFEAT_CBVEC		(1<<8)

	struct shared_info	*sc_ipg;	/* HYPERVISOR_shared_info */
};

extern struct xen_softc *xen_sc;

/*
 *  Hypercalls
 */
#define memory_op		12
#define xen_version		17
#define hvm_op			34

int	xen_hypercall(struct xen_softc *, int, int, ...);
int	xen_hypercallv(struct xen_softc *, int, int, ulong *);

#endif	/* _XENVAR_H_ */
