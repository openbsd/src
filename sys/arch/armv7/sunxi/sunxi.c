/* $OpenBSD: sunxi.c,v 1.1 2013/10/23 17:08:48 jasper Exp $ */
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
/* #include <machine/bus.h> */
#include <arch/arm/armv7/armv7var.h>
#include <armv7/sunxi/sunxivar.h>
#include <armv7/sunxi/sunxireg.h>

struct arm32_bus_dma_tag sunxi_bus_dma_tag = {
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

struct board_dev sun4i_devs[] = {
	{ "sxipio",	0 },
	{ "sxiccmu",	0 },
	{ "a1xintc",	0 },
	{ "sxitimer",	0 },
	{ "sxitimer",	1 },
	{ "sxitimer",	2 },
	{ "sxidog",	0 },
	{ "sxirtc",	0 },
	{ "sxiuart",	0 },
	{ "sxiuart",	1 },
	{ "sxiuart",	2 },
	{ "sxiuart",	3 },
	{ "sxiuart",	4 },
	{ "sxiuart",	5 },
	{ "sxiuart",	6 },
	{ "sxiuart",	7 },
	{ "sxie",	0 },
	{ "ahci",	0 },
	{ "ehci",	0 },
	{ "ehci",	1 },
#if 0
	{ "ohci",	0 },
	{ "ohci",	1 },
#endif
	{ NULL,		0 }
};

struct board_dev sun7i_devs[] = {
	{ "sxipio",	0 },
	{ "sxiccmu",	0 },
	{ "sxitimer",	0 },
	{ "sxitimer",	1 },
	{ "sxitimer",	2 },
	{ "sxidog",	0 },
	{ "sxirtc",	0 },
	{ "sxiuart",	0 },
	{ "sxiuart",	1 },
	{ "sxiuart",	2 },
	{ "sxiuart",	3 },
	{ "sxiuart",	4 },
	{ "sxiuart",	5 },
	{ "sxiuart",	6 },
	{ "sxiuart",	7 },
	{ "sxie",	0 },
	{ "ahci",	0 },
	{ "ehci",	0 },
	{ "ehci",	1 },
#if 0
	{ "ohci",	0 },
	{ "ohci",	1 },
#endif
	{ NULL,		0 }
};

struct board_dev *board_devs;
struct sxi_dev *sunxi_devs = NULL;

extern void sxia1x_init(void);
extern void sxia20_init(void);

struct sunxi_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;
};

int	sunxi_match(struct device *, void *, void *);
void	sunxi_attach(struct device *, struct device *, void *);
int	sxi_submatch(struct device *, void *, void *);

struct cfattach sunxi_ca = {
	sizeof(struct sunxi_softc), sunxi_match, sunxi_attach
};

struct cfdriver sunxi_cd = {
	NULL, "sunxi", DV_DULL
};

int
sunxi_match(struct device *parent, void *cfdata, void *aux)
{
	return 1;
}

void
sunxi_attach(struct device *parent, struct device *self, void *aux)
{
	struct sunxi_softc *sc = (struct sunxi_softc *)self;
	struct board_dev *bd;
	sc->sc_iot = &armv7_bs_tag;
	sc->sc_dmat = &sunxi_bus_dma_tag;

	switch (board_id) {
	case BOARD_ID_SUN4I_A10:
		printf(": A1X\n");
		sxia1x_init();
		board_devs = sun4i_devs;
		break;
	case BOARD_ID_SUN7I_A20:
		printf(": A20\n");
		sxia20_init();
		board_devs = sun7i_devs;
		break;
	default:
		printf("\n");
		panic("sunxi_attach: board type 0x%x unknown", board_id);
	}

#if 1
	/*
	 * XXX think of a better place to do this, as there might
	 * be need for access by other drivers later.
	 */
	if (bus_space_map(sc->sc_iot, SYSCTRL_ADDR, SYSCTRL_SIZE, 0,
	    &sc->sc_ioh))
		panic("sunxi_attach: bus_space_map failed!");
	/* map the part of SRAM dedicated to EMAC to EMAC */
	AWSET4(sc, 4, 5 << 2);
#endif

	/* Directly configure on-board devices (dev* in config file). */
	for (bd = board_devs; bd->name != NULL; bd++) {
		struct sxi_dev *sxid = sxi_find_dev(bd->name, bd->unit);
		struct sxi_attach_args sxi;

		if (sxid == NULL) {
			printf("%s: device %s unit %d not found\n",
			    self->dv_xname, bd->name, bd->unit);
			continue;
		}

		memset(&sxi, 0, sizeof(sxi));
		sxi.sxi_dev = sxid;
		sxi.sxi_iot = sc->sc_iot;
		sxi.sxi_dmat = sc->sc_dmat;

		if (config_found_sm(self, &sxi, NULL, sxi_submatch) == NULL)
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
sxi_submatch(struct device *parent, void *child, void *aux)
{
	struct cfdata *cf = child;
	struct sxi_attach_args *sxi = aux;

	if (strcmp(cf->cf_driver->cd_name, sxi->sxi_dev->name) == 0)
		return 1;

	return 0;
}

void
sxi_set_devs(struct sxi_dev *devs)
{
	sunxi_devs = devs;
}

struct sxi_dev *
sxi_find_dev(const char *name, int unit)
{
	struct sxi_dev *sxid;

	if (sunxi_devs == NULL)
		panic("sunxi_find_dev: sunxi_devs == NULL");

	for (sxid = sunxi_devs; sxid->name != NULL; sxid++)
		if (sxid->unit == unit && strcmp(sxid->name, name) == 0)
			return sxid;

	return NULL;
}
