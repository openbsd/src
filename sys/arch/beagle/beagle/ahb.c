/* $OpenBSD: ahb.c,v 1.1 2009/05/08 03:13:26 drahn Exp $ */
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
#include <arch/beagle/beagle/ahb.h>
#include <arch/arm/armv7/armv7var.h>

struct ahb_softc {
	struct device sc_dv;

};

int	ahbmatch(struct device *, void *, void *);
void	ahbattach(struct device *, struct device *, void *);
int	ahbprint(void *, const char *);
void	ahbscan(struct device *, void *);

struct arm32_bus_dma_tag ahb_bus_dma_tag = {
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


struct cfattach ahb_ca = {
	sizeof(struct ahb_softc), ahbmatch, ahbattach
};

struct cfdriver ahb_cd = {
	NULL, "ahb", DV_DULL, 1
};

int
ahbmatch(struct device *parent, void *cfdata, void *aux)
{
	return (1);
}

void
ahbattach(struct device *parent, struct device *self, void *aux)
{
	/*
	struct ahb_softc *sc = (struct ahb_softc *)self; 
	*/

	printf("\n");

	config_scan(ahbscan, self);
}

void
ahbscan(struct device *parent, void *match)
{
	/*
	struct ahb_softc *sc = (struct ahb_softc *)parent;
	*/
	struct device *dev = match;
	struct cfdata *cf = dev->dv_cfdata;
	struct ahb_attach_args aa;

	if (cf->cf_fstate == FSTATE_STAR) {
		printf("ahb does not deal with dev* entries\n");
		free(dev, M_DEVBUF);
		return;
	}

        if (autoconf_verbose)
		printf(">>> probing for %s%d\n", cf->cf_driver->cd_name,
		    cf->cf_unit);

	aa.aa_addr = cf->cf_loc[0];
	aa.aa_size = cf->cf_loc[1];
	aa.aa_intr = cf->cf_loc[2];
	aa.aa_iot = &armv7_bs_tag;
	aa.aa_dmat = &ahb_bus_dma_tag;
	if ((*cf->cf_attach->ca_match)(parent, dev, &aa) > 0) {
		if (autoconf_verbose)
			printf(">>> probing for %s%d succeeded\n",
			    cf->cf_driver->cd_name, cf->cf_unit);
		config_attach(parent, dev, &aa, ahbprint);
	} else {
		if (autoconf_verbose)
			printf(">>> probing for %s%d failed\n",
			    cf->cf_driver->cd_name, cf->cf_unit);
		free(dev, M_DEVBUF);
	}
}

int
ahbprint(void *aux, const char *str)
{
	struct ahb_attach_args *aa = aux;

	if (aa->aa_addr != -1)
		printf(" addr 0x%x", aa->aa_addr);
	if (aa->aa_size != 0)
		printf(" size 0x%x", aa->aa_size);
	if (aa->aa_intr != -1)
		printf(" intr %d", aa->aa_intr);
	return (QUIET);
}
