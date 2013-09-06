/* $OpenBSD: imx.c,v 1.1 2013/09/06 20:45:53 patrick Exp $ */
/*
 * Copyright (c) 2005,2008 Dale Rahn <drahn@openbsd.com>
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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
#include <armv7/imx/imxvar.h>

struct arm32_bus_dma_tag imx_bus_dma_tag = {
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

struct board_dev phyflex_imx6_devs[] = {
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxdog",	0 },
	{ "imxocotp",	0 },
	{ "imxuart",	3 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "imxesdhc",	1 },
	{ "imxesdhc",	2 },
	{ "ehci",	0 },
	{ "imxenet",	0 },
	{ "ahci",	0 },
	{ NULL,		0 }
};

struct board_dev sabrelite_devs[] = {
	{ "imxccm",	0 },
	{ "imxiomuxc",	0 },
	{ "imxdog",	0 },
	{ "imxocotp",	0 },
	{ "imxuart",	1 },
	{ "imxgpio",	0 },
	{ "imxgpio",	1 },
	{ "imxgpio",	2 },
	{ "imxgpio",	3 },
	{ "imxgpio",	4 },
	{ "imxgpio",	5 },
	{ "imxgpio",	6 },
	{ "imxesdhc",	2 },
	{ "imxesdhc",	3 },
	{ "ehci",	0 },
	{ "imxenet",	0 },
	{ "ahci",	0 },
	{ NULL,		0 }
};

struct board_dev *board_devs;

struct imx_dev *imx_devs = NULL;

struct imx_softc {
	struct device sc_dv;
};

int	imx_match(struct device *, void *, void *);
void	imx_attach(struct device *, struct device *, void *);
int	imx_submatch(struct device *, void *, void *);

struct cfattach imx_ca = {
	sizeof(struct imx_softc), imx_match, imx_attach, NULL,
	config_activate_children
};

struct cfdriver imx_cd = {
	NULL, "imx", DV_DULL
};

int
imx_match(struct device *parent, void *cfdata, void *aux)
{
	return (1);
}

void
imx_attach(struct device *parent, struct device *self, void *aux)
{
	struct board_dev *bd;

	switch (board_id) {
	case BOARD_ID_IMX6_PHYFLEX:
		printf(": PhyFLEX-i.MX6\n");
		imx6_init();
		board_devs = phyflex_imx6_devs;
		break;
	case BOARD_ID_IMX6_SABRELITE:
		printf(": i.MX6 SABRE Lite\n");
		imx6_init();
		board_devs = sabrelite_devs;
		break;
	default:
		printf("\n");
		panic("%s: board type 0x%x unknown", __func__, board_id);
	}

	/* Directly configure on-board devices (dev* in config file). */
	for (bd = board_devs; bd->name != NULL; bd++) {
		struct imx_dev *id = imx_find_dev(bd->name, bd->unit);
		struct imx_attach_args ia;

		if (id == NULL)
			printf("%s: device %s unit %d not found\n",
			    self->dv_xname, bd->name, bd->unit);

		memset(&ia, 0, sizeof(ia));
		ia.ia_dev = id;
		ia.ia_iot = &armv7_bs_tag;
		ia.ia_dmat = &imx_bus_dma_tag;

		if (config_found_sm(self, &ia, NULL, imx_submatch) == NULL)
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
imx_submatch(struct device *parent, void *child, void *aux)
{
	struct cfdata *cf = child;
	struct imx_attach_args *ia = aux;

	if (strcmp(cf->cf_driver->cd_name, ia->ia_dev->name) == 0)
		return (1);

	/* "These are not the droids you are looking for." */
	return (0);
}

void
imx_set_devs(struct imx_dev *devs)
{
	imx_devs = devs;
}

struct imx_dev *
imx_find_dev(const char *name, int unit)
{
	struct imx_dev *id;

	if (imx_devs == NULL)
		panic("%s: imx_devs == NULL", __func__);

	for (id = imx_devs; id->name != NULL; id++) {
		if (id->unit == unit && strcmp(id->name, name) == 0)
			return (id);
	}

	return (NULL);
}
