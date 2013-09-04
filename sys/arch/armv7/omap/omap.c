/* $OpenBSD: omap.c,v 1.1 2013/09/04 14:38:30 patrick Exp $ */
/*
 * Copyright (c) 2005,2008 Dale Rahn <drahn@openbsd.com>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/reboot.h>
#define _ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <arch/arm/armv7/armv7var.h>
#include <armv7/omap/omapvar.h>

struct arm32_bus_dma_tag omap_bus_dma_tag = {
	0,
	0,
	NULL,
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	_bus_dmamap_sync,
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

struct board_dev {
	char *name;
	int unit;
};

struct board_dev beagleboard_devs[] = {
	{ "prcm",	0 },
	{ "intc",	0 },
	{ "gptimer",	0 },
	{ "gptimer",	1 },
	{ "omdog",	0 },
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "omgpio",	4 },
	{ "omgpio",	5 },
	{ "ommmc",	0 },		/* HSMMC1 */
	{ "com",	2 },		/* UART3 */
	{ NULL,		0 }
};

struct board_dev beaglebone_devs[] = {
	{ "prcm",	0 },
	{ "sitaracm",	0 },
	{ "intc",	0 },
	{ "dmtimer",	0 },
	{ "dmtimer",	1 },
	{ "omdog",	0 },
	{ "ommmc",	0 },		/* HSMMC0 */
	{ "com",	0 },		/* UART0 */
	{ "cpsw",	0 },
	{ NULL,		0 }
};

struct board_dev overo_devs[] = {
	{ "prcm",	0 },
	{ "intc",	0 },
	{ "gptimer",	0 },
	{ "gptimer",	1 },
	{ "omdog",	0 },
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "omgpio",	4 },
	{ "omgpio",	5 },
	{ "ommmc",	0 },		/* HSMMC1 */
	{ "com",	2 },		/* UART3 */
	{ NULL,		0 }
};

struct board_dev pandaboard_devs[] = {
	{ "omapid",	0 },
	{ "prcm",	0 },
	{ "omdog",	0 },
	{ "omgpio",	0 },
	{ "omgpio",	1 },
	{ "omgpio",	2 },
	{ "omgpio",	3 },
	{ "omgpio",	4 },
	{ "omgpio",	5 },
	{ "ommmc",	0 },		/* HSMMC1 */
	{ "com",	2 },		/* UART3 */
	{ "ehci",	0 },
	{ NULL,		0 }
};

struct board_dev *board_devs;

struct omap_dev *omap_devs = NULL;

struct omap_softc {
	struct device sc_dv;
};

int	omap_match(struct device *, void *, void *);
void	omap_attach(struct device *, struct device *, void *);
int	omap_submatch(struct device *, void *, void *);

struct cfattach omap_ca = {
	sizeof(struct omap_softc), omap_match, omap_attach
};

struct cfdriver omap_cd = {
	NULL, "omap", DV_DULL
};

int
omap_match(struct device *parent, void *cfdata, void *aux)
{
	return (1);
}

void
omap_attach(struct device *parent, struct device *self, void *aux)
{
	struct board_dev *bd;

	switch (board_id) {
	case BOARD_ID_OMAP3_BEAGLE:
		printf(": BeagleBoard\n");
		omap3_init();
		board_devs = beagleboard_devs;
		break;
	case BOARD_ID_AM335X_BEAGLEBONE:
		printf(": BeagleBone\n");
		am335x_init();
		board_devs = beaglebone_devs;
		break;
	case BOARD_ID_OMAP3_OVERO:
		printf(": Gumstix Overo\n");
		omap3_init();
		board_devs = overo_devs;
		break;
	case BOARD_ID_OMAP4_PANDA:
		printf(": PandaBoard\n");
		omap4_init();
		board_devs = pandaboard_devs;
		break;
	default:
		printf("\n");
		panic("%s: board type 0x%x unknown", __func__, board_id);
	}

	/* Directly configure on-board devices (dev* in config file). */
	for (bd = board_devs; bd->name != NULL; bd++) {
		struct omap_dev *od = omap_find_dev(bd->name, bd->unit);
		struct omap_attach_args oa;

		if (od == NULL) {
			printf("%s: device %s unit %d not found\n",
			    self->dv_xname, bd->name, bd->unit);
			continue;
		}

		memset(&oa, 0, sizeof(oa));
		oa.oa_dev = od;
		oa.oa_iot = &armv7_bs_tag;
		oa.oa_dmat = &omap_bus_dma_tag;

		if (config_found_sm(self, &oa, NULL, omap_submatch) == NULL)
			printf("%s: device %s unit %d not configured\n",
			    self->dv_xname, bd->name, bd->unit);
	}
}

/*
 * We do direct configuration of devices on this SoC "bus", so we
 * never call the child device's match function at all (it can be
 * NULL in the struct cfattach).
 */
int
omap_submatch(struct device *parent, void *child, void *aux)
{
	struct cfdata *cf = child;
	struct omap_attach_args *oa = aux;

	if (strcmp(cf->cf_driver->cd_name, oa->oa_dev->name) == 0)
		return (1);

	/* "These are not the droids you are looking for." */
	return (0);
}

void
omap_set_devs(struct omap_dev *devs)
{
	omap_devs = devs;
}

struct omap_dev *
omap_find_dev(const char *name, int unit)
{
	struct omap_dev *od;

	if (omap_devs == NULL)
		panic("%s: omap_devs == NULL", __func__);

	for (od = omap_devs; od->name != NULL; od++) {
		if (od->unit == unit && strcmp(od->name, name) == 0)
			return (od);
	}

	return (NULL);
}
