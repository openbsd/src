/*	$OpenBSD: xhub.c,v 1.1 2008/04/07 22:47:40 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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

/*
 * IP27 Hub Widget
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbowdevs.h>

int	xhub_match(struct device *, void *, void *);
void	xhub_attach(struct device *, struct device *, void *);

struct cfattach xhub_ca = {
	sizeof(struct device), xhub_match, xhub_attach,
};

struct cfdriver xhub_cd = {
	NULL, "xhub", DV_DULL,
};

int
xhub_match(struct device *parent, void *match, void *aux)
{
	struct xbow_attach_args *xaa = aux;

	if (xaa->xaa_vendor == PCI_VENDOR_SGI4 &&
	    xaa->xaa_product == PCI_PRODUCT_SGI4_HUB)
		return xbow_intr_widget == 0 ? 20 : 1;

	return 0;
}

void
xhub_attach(struct device *parent, struct device *self, void *aux)
{
	struct xbow_attach_args *xaa = aux;

	printf(" revision %d\n", xaa->xaa_revision);

	/*
	 * If no other widget has claimed interrupts routing, do it now.
	 */
	if (xbow_intr_widget == 0) {
		xbow_intr_widget = xaa->xaa_widget;
	}

	/* initialize interrupt handling here */
}
