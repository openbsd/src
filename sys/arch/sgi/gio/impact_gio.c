/*	$OpenBSD: impact_gio.c,v 1.1 2012/04/18 17:28:24 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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
 * Driver for the SGI Impact graphics board (GIO attachment).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/types.h>

#include <machine/autoconf.h>
#include <mips64/archtype.h>

#include <mips64/arcbios.h>

#include <sgi/dev/impactreg.h>
#include <sgi/dev/impactvar.h>
#include <sgi/gio/giodevs.h>
#include <sgi/gio/gioreg.h>
#include <sgi/gio/giovar.h>
#include <sgi/sgi/ip22.h>

#define	IMPACT_REG_OFFSET		0x00000000
#define	IMPACT_REG_SIZE			0x00080000

int	impact_gio_match(struct device *, void *, void *);
void	impact_gio_attach(struct device *, struct device *, void *);

const struct cfattach impact_gio_ca = {
	sizeof(struct impact_softc), impact_gio_match, impact_gio_attach
};

int
impact_gio_match(struct device *parent, void *match, void *aux)
{
	struct gio_attach_args *ga = aux;

	/* Impact only exists on Indigo 2 systems */
	if (sys_config.system_type != SGI_IP22 ||
	    sys_config.system_subtype != IP22_INDIGO2)
		return 0;

	/* not looking for a frame buffer */
	if (ga->ga_slot != -1)
		return 0;

	/* EXP1 does not exist on Indigo2 */
	if (ga->ga_addr != GIO_ADDR_GFX && ga->ga_addr != GIO_ADDR_EXP0)
		return 0;

	/* Impact has a real GIO ID */
	if (ga->ga_product != GIO_PRODUCT_IMPACT)
		return 0;

	return 1;
}

void
impact_gio_attach(struct device *parent, struct device *self, void *aux)
{
	struct gio_attach_args *ga = aux;
	struct impact_softc *sc = (struct impact_softc *)self;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int console;

	if (strncmp(bios_graphics, "alive", 5) != 0) {
		printf(" device has not been setup by firmware!\n");
		return;
	}

	console = giofb_consaddr == ga->ga_addr;

	if (console != 0) {
		iot = NULL;
		ioh = 0;
	} else {
		iot = ga->ga_iot;

		/* Setup bus space mappings. */
		if (bus_space_map(iot, ga->ga_addr + IMPACT_REG_OFFSET,
		    IMPACT_REG_SIZE, 0, &ioh)) {
			printf("failed to map registers\n");
			return;
		}
	}

	if (impact_attach_common(sc, iot, ioh, console, 0) != 0) {
		if (console == 0)
			bus_space_unmap(iot, ioh, IMPACT_REG_SIZE);
	}
}

/*
 * Console support.
 */

int
impact_gio_cnprobe(struct gio_attach_args *ga)
{
	return impact_gio_match(NULL, NULL, ga);
}

int
impact_gio_cnattach(struct gio_attach_args *ga)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int rc;

	iot = ga->ga_iot;
	rc = bus_space_map(iot, ga->ga_addr + IMPACT_REG_OFFSET,
	    IMPACT_REG_SIZE, 0, &ioh);
	if (rc != 0)
		return rc;

	rc = impact_cnattach_common(iot, ioh, 0);
	if (rc != 0) {
		bus_space_unmap(iot, ioh, IMPACT_REG_SIZE);
		return rc;
	}

	return 0;
}
