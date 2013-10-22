/* $OpenBSD: allwinner.c,v 1.1 2013/10/22 13:22:18 jasper Exp $ */
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
#include <armv7/allwinner/allwinnervar.h>
#include <armv7/allwinner/allwinnerreg.h>

struct arm32_bus_dma_tag allwinner_bus_dma_tag = {
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

struct board_dev cubieboard_devs[] = {
	{ "awpio",	0 },
	{ "awccmu",	0 },
	{ "a1xintc",	0 },
	{ "awtimer",	0 },
	{ "awtimer",	1 },
	{ "awtimer",	2 },
	{ "awdog",	0 },
	{ "awrtc",	0 },
	{ "awuart",	0 },
	{ "awuart",	1 },
	{ "awuart",	2 },
	{ "awuart",	3 },
	{ "awuart",	4 },
	{ "awuart",	5 },
	{ "awuart",	6 },
	{ "awuart",	7 },
	{ "awe",	0 },
	{ "ahci",	0 },
	{ "ehci",	0 },
	{ "ehci",	1 },
#if 0
	{ "ohci",	0 },
	{ "ohci",	1 },
#endif
	{ NULL,		0 }
};

struct board_dev cubieboard2_devs[] = {
	{ "awpio",	0 },
	{ "awccmu",	0 },
	{ "awtimer",	0 },
	{ "awtimer",	1 },
	{ "awtimer",	2 },
	{ "awdog",	0 },
	{ "awrtc",	0 },
	{ "awuart",	0 },
	{ "awuart",	1 },
	{ "awuart",	2 },
	{ "awuart",	3 },
	{ "awuart",	4 },
	{ "awuart",	5 },
	{ "awuart",	6 },
	{ "awuart",	7 },
	{ "awe",	0 },
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
struct aw_dev *allwinner_devs = NULL;

extern void awa1x_init(void);
extern void awa20_init(void);

struct allwinner_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;
};

int	allwinner_match(struct device *, void *, void *);
void	allwinner_attach(struct device *, struct device *, void *);
int	aw_submatch(struct device *, void *, void *);

struct cfattach allwinner_ca = {
	sizeof(struct allwinner_softc), allwinner_match, allwinner_attach
};

struct cfdriver allwinner_cd = {
	NULL, "allwinner", DV_DULL
};

int
allwinner_match(struct device *parent, void *cfdata, void *aux)
{
	return 1;
}

void
allwinner_attach(struct device *parent, struct device *self, void *aux)
{
	struct allwinner_softc *sc = (struct allwinner_softc *)self;
	struct board_dev *bd;
	sc->sc_iot = &armv7_bs_tag;
	sc->sc_dmat = &allwinner_bus_dma_tag;

	switch (board_id) {
	case BOARD_ID_A10_CUBIE:
		printf(": A1X\n");
		awa1x_init();
		board_devs = cubieboard_devs;
		break;
	case BOARD_ID_A20_CUBIE:
		printf(": A20\n");
		awa20_init();
		board_devs = cubieboard2_devs;
		break;
	default:
		printf("\n");
		panic("allwinner_attach: board type 0x%x unknown", board_id);
	}

#if 1
	/*
	 * XXX think of a better place to do this, as there might
	 * be need for access by other drivers later.
	 */
	if (bus_space_map(sc->sc_iot, SYSCTRL_ADDR, SYSCTRL_SIZE, 0,
	    &sc->sc_ioh))
		panic("allwinner_attach: bus_space_map failed!");
	/* map the part of SRAM dedicated to EMAC to EMAC */
	AWSET4(sc, 4, 5 << 2);
#endif

	/* Directly configure on-board devices (dev* in config file). */
	for (bd = board_devs; bd->name != NULL; bd++) {
		struct aw_dev *awd = aw_find_dev(bd->name, bd->unit);
		struct aw_attach_args aw;

		if (awd == NULL) {
			printf("%s: device %s unit %d not found\n",
			    self->dv_xname, bd->name, bd->unit);
			continue;
		}

		memset(&aw, 0, sizeof(aw));
		aw.aw_dev = awd;
		aw.aw_iot = sc->sc_iot;
		aw.aw_dmat = sc->sc_dmat;

		if (config_found_sm(self, &aw, NULL, aw_submatch) == NULL)
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
aw_submatch(struct device *parent, void *child, void *aux)
{
	struct cfdata *cf = child;
	struct aw_attach_args *aw = aux;

	if (strcmp(cf->cf_driver->cd_name, aw->aw_dev->name) == 0)
		return 1;

	return 0;
}

void
aw_set_devs(struct aw_dev *devs)
{
	allwinner_devs = devs;
}

struct aw_dev *
aw_find_dev(const char *name, int unit)
{
	struct aw_dev *awd;

	if (allwinner_devs == NULL)
		panic("allwinner_find_dev: allwinner_devs == NULL");

	for (awd = allwinner_devs; awd->name != NULL; awd++)
		if (awd->unit == unit && strcmp(awd->name, name) == 0)
			return awd;

	return NULL;
}
