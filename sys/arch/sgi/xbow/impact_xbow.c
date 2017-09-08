/*	$OpenBSD: impact_xbow.c,v 1.2 2017/09/08 05:36:52 deraadt Exp $	*/

/*
 * Copyright (c) 2010, 2012 Miodrag Vallat.
 * Copyright (c) 2009, 2010 Joel Sing <jsing@openbsd.org>
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
 * Driver for the SGI ImpactSR graphics board (XBow attachment).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>

#include <mips64/arcbios.h>

#include <sgi/dev/impactvar.h>
#include <sgi/xbow/widget.h>
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbowdevs.h>

#define	IMPACTSR_REG_OFFSET		0x00000000
#define	IMPACTSR_REG_SIZE		0x00200000

struct impact_xbow_softc {
	struct impact_softc	sc_base;

	struct mips_bus_space	iot_store;
};

int	impact_xbow_match(struct device *, void *, void *);
void	impact_xbow_attach(struct device *, struct device *, void *);

const struct cfattach impact_xbow_ca = {
	sizeof(struct impact_xbow_softc), impact_xbow_match, impact_xbow_attach
};

int	impact_xbow_is_console(struct xbow_attach_args *);

int
impact_xbow_match(struct device *parent, void *match, void *aux)
{
	struct xbow_attach_args *xaa = aux;

	if (xaa->xaa_vendor == XBOW_VENDOR_SGI5 &&
	    xaa->xaa_product == XBOW_PRODUCT_SGI5_IMPACT)
		return 1;

	return 0;
}

void
impact_xbow_attach(struct device *parent, struct device *self, void *aux)
{
	struct xbow_attach_args *xaa = aux;
	struct impact_xbow_softc *sc = (struct impact_xbow_softc *)self;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int console;

	if (strncmp(bios_graphics, "alive", 5) != 0) {
		printf(" device has not been setup by firmware!\n");
		return;
	}

	printf(" revision %d\n", xaa->xaa_revision);

	console = impact_xbow_is_console(xaa);

	if (console != 0) {
		iot = NULL;
		ioh = 0;
	} else {
		/*
		 * Create a copy of the bus space tag.
		 */
		bcopy(xaa->xaa_iot, &sc->iot_store,
		    sizeof(struct mips_bus_space));
		iot = &sc->iot_store;

		/* Setup bus space mappings. */
		if (bus_space_map(iot, IMPACTSR_REG_OFFSET,
		    IMPACTSR_REG_SIZE, 0, &ioh)) {
			printf("failed to map registers\n");
			return;
		}
	}

	if (impact_attach_common(&sc->sc_base, iot, ioh, console, 1) != 0) {
		if (console == 0)
			bus_space_unmap(iot, ioh, IMPACTSR_REG_SIZE);
	}
}

/*
 * Console support.
 */

int
impact_xbow_cnprobe()
{
	u_int32_t wid, vendor, product;

	if (xbow_widget_id(console_output.nasid, console_output.widget,
	    &wid) != 0)
		return 0;

	vendor = WIDGET_ID_VENDOR(wid);
	product = WIDGET_ID_PRODUCT(wid);

	if (vendor != XBOW_VENDOR_SGI5 || product != XBOW_PRODUCT_SGI5_IMPACT)
		return 0;

	if (strncmp(bios_graphics, "alive", 5) != 0)
		return 0;

	return 1;
}

int
impact_xbow_cnattach()
{
	static struct mips_bus_space impact_iot_store;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int rc;

	/* Build bus space accessor. */
	xbow_build_bus_space(&impact_iot_store, console_output.nasid,
	    console_output.widget);
	iot = &impact_iot_store;

	rc = bus_space_map(iot, IMPACTSR_REG_OFFSET, IMPACTSR_REG_SIZE,
	    0, &ioh);
	if (rc != 0)
		return rc;

	rc = impact_cnattach_common(iot, ioh, 1);
	if (rc != 0) {
		bus_space_unmap(iot, ioh, IMPACTSR_REG_SIZE);
		return rc;
	}

	return 0;
}

int
impact_xbow_is_console(struct xbow_attach_args *xaa)
{
	return xaa->xaa_nasid == console_output.nasid &&
	    xaa->xaa_widget == console_output.widget;
}
