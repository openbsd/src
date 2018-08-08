/* $OpenBSD: mainbus.c,v 1.10 2018/08/08 20:57:53 kettenis Exp $ */
/*
 * Copyright (c) 2016 Patrick Wildt <patrick@blueri.se>
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/fdt.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#include <arm64/arm64/arm64var.h>
#include <arm64/dev/mainbus.h>

int mainbus_match(struct device *, void *, void *);
void mainbus_attach(struct device *, struct device *, void *);

void mainbus_attach_node(struct device *, int, cfmatch_t);
int mainbus_match_status(struct device *, void *, void *);
void mainbus_attach_cpus(struct device *, cfmatch_t);
int mainbus_match_primary(struct device *, void *, void *);
int mainbus_match_secondary(struct device *, void *, void *);
void mainbus_attach_efi(struct device *);
void mainbus_attach_framebuffer(struct device *);

struct mainbus_softc {
	struct device		 sc_dev;
	int			 sc_node;
	bus_space_tag_t		 sc_iot;
	bus_dma_tag_t		 sc_dmat;
	int			 sc_acells;
	int			 sc_scells;
	int			*sc_ranges;
	int			 sc_rangeslen;
	int			 sc_early;
};

struct cfattach mainbus_ca = {
	sizeof(struct mainbus_softc), mainbus_match, mainbus_attach, NULL,
	config_activate_children
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

struct machine_bus_dma_tag mainbus_dma_tag = {
	NULL,
	0,
	_dmamap_create,
	_dmamap_destroy,
	_dmamap_load,
	_dmamap_load_mbuf,
	_dmamap_load_uio,
	_dmamap_load_raw,
	_dmamap_load_buffer,
	_dmamap_unload,
	_dmamap_sync,
	_dmamem_alloc,
	_dmamem_free,
	_dmamem_map,
	_dmamem_unmap,
	_dmamem_mmap,
};

/*
 * Mainbus takes care of FDT and non-FDT machines, so we
 * always attach.
 */
int
mainbus_match(struct device *parent, void *cfdata, void *aux)
{
	return (1);
}

extern char *hw_prod;
void agtimer_init(void);

void
mainbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct mainbus_softc *sc = (struct mainbus_softc *)self;
	char model[128];
	int node, len;

	arm_intr_init_fdt();
	agtimer_init();

	sc->sc_node = OF_peer(0);
	sc->sc_iot = &arm64_bs_tag;
	sc->sc_dmat = &mainbus_dma_tag;
	sc->sc_acells = OF_getpropint(OF_peer(0), "#address-cells", 1);
	sc->sc_scells = OF_getpropint(OF_peer(0), "#size-cells", 1);

	len = OF_getprop(sc->sc_node, "model", model, sizeof(model));
	if (len > 0) {
		printf(": %s\n", model);
		hw_prod = malloc(len, M_DEVBUF, M_NOWAIT);
		if (hw_prod)
			strlcpy(hw_prod, model, len);
	} else
		printf(": unknown model\n");

	/* Attach primary CPU first. */
	mainbus_attach_cpus(self, mainbus_match_primary);

	mainbus_attach_efi(self);

	sc->sc_rangeslen = OF_getproplen(OF_peer(0), "ranges");
	if (sc->sc_rangeslen > 0 && !(sc->sc_rangeslen % sizeof(uint32_t))) {
		sc->sc_ranges = malloc(sc->sc_rangeslen, M_TEMP, M_WAITOK);
		OF_getpropintarray(OF_peer(0), "ranges", sc->sc_ranges,
		    sc->sc_rangeslen);
	}

	/* Scan the whole tree. */
	sc->sc_early = 1;
	for (node = OF_child(sc->sc_node); node != 0; node = OF_peer(node))
		mainbus_attach_node(self, node, NULL);

	sc->sc_early = 0;
	for (node = OF_child(sc->sc_node); node != 0; node = OF_peer(node))
		mainbus_attach_node(self, node, NULL);
	
	mainbus_attach_framebuffer(self);

	/* Attach secondary CPUs. */
	mainbus_attach_cpus(self, mainbus_match_secondary);
}

/*
 * Look for a driver that wants to be attached to this node.
 */
void
mainbus_attach_node(struct device *self, int node, cfmatch_t submatch)
{
	struct mainbus_softc	*sc = (struct mainbus_softc *)self;
	struct fdt_attach_args	 fa;
	int			 i, len, line;
	uint32_t		*cell, *reg;

	memset(&fa, 0, sizeof(fa));
	fa.fa_name = "";
	fa.fa_node = node;
	fa.fa_iot = sc->sc_iot;
	fa.fa_dmat = sc->sc_dmat;
	fa.fa_acells = sc->sc_acells;
	fa.fa_scells = sc->sc_scells;

	len = OF_getproplen(node, "reg");
	line = (sc->sc_acells + sc->sc_scells) * sizeof(uint32_t);
	if (len > 0 && (len % line) == 0) {
		reg = malloc(len, M_TEMP, M_WAITOK);
		OF_getpropintarray(node, "reg", reg, len);

		fa.fa_reg = malloc((len / line) * sizeof(struct fdt_reg),
		    M_DEVBUF, M_WAITOK);
		fa.fa_nreg = (len / line);

		for (i = 0, cell = reg; i < len / line; i++) {
			if (sc->sc_acells >= 1)
				fa.fa_reg[i].addr = cell[0];
			if (sc->sc_acells == 2) {
				fa.fa_reg[i].addr <<= 32;
				fa.fa_reg[i].addr |= cell[1];
			}
			cell += sc->sc_acells;
			if (sc->sc_scells >= 1)
				fa.fa_reg[i].size = cell[0];
			if (sc->sc_scells == 2) {
				fa.fa_reg[i].size <<= 32;
				fa.fa_reg[i].size |= cell[1];
			}
			cell += sc->sc_scells;
		}

		free(reg, M_TEMP, len);
	}

	len = OF_getproplen(node, "interrupts");
	if (len > 0 && (len % sizeof(uint32_t)) == 0) {
		fa.fa_intr = malloc(len, M_DEVBUF, M_WAITOK);
		fa.fa_nintr = len / sizeof(uint32_t);

		OF_getpropintarray(node, "interrupts", fa.fa_intr, len);
	}

	if (OF_getproplen(node, "dma-coherent") >= 0) {
		fa.fa_dmat = malloc(sizeof(*sc->sc_dmat),
		    M_DEVBUF, M_WAITOK | M_ZERO);
		memcpy(fa.fa_dmat, sc->sc_dmat, sizeof(*sc->sc_dmat));
		fa.fa_dmat->_flags |= BUS_DMA_COHERENT;
	}

	if (submatch == NULL)
		submatch = mainbus_match_status;
	config_found_sm(self, &fa, NULL, submatch);

	free(fa.fa_reg, M_DEVBUF, fa.fa_nreg * sizeof(struct fdt_reg));
	free(fa.fa_intr, M_DEVBUF, fa.fa_nintr * sizeof(uint32_t));
}

int
mainbus_match_status(struct device *parent, void *match, void *aux)
{
	struct mainbus_softc *sc = (struct mainbus_softc *)parent;
	struct fdt_attach_args *fa = aux;
	struct cfdata *cf = match;
	char buf[32];

	if (fa->fa_node == 0)
		return 0;

	if (OF_getprop(fa->fa_node, "status", buf, sizeof(buf)) > 0 &&
	    strcmp(buf, "disabled") == 0)
		return 0;

	if (cf->cf_loc[0] == sc->sc_early)
		return (*cf->cf_attach->ca_match)(parent, match, aux);
	return 0;
}

void
mainbus_attach_cpus(struct device *self, cfmatch_t match)
{
	struct mainbus_softc *sc = (struct mainbus_softc *)self;
	int node = OF_finddevice("/cpus");
	int acells, scells;
	char buf[32];

	if (node == 0)
		return;

	acells = sc->sc_acells;
	scells = sc->sc_scells;
	sc->sc_acells = OF_getpropint(node, "#address-cells", 2);
	sc->sc_scells = OF_getpropint(node, "#size-cells", 0);

	ncpusfound = 0;
	for (node = OF_child(node); node != 0; node = OF_peer(node)) {
		if (OF_getprop(node, "device_type", buf, sizeof(buf)) > 0 &&
		    strcmp(buf, "cpu") == 0)
			ncpusfound++;

		mainbus_attach_node(self, node, match);
	}

	sc->sc_acells = acells;
	sc->sc_scells = scells;
}

int
mainbus_match_primary(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *fa = aux;
	struct cfdata *cf = match;
	uint64_t mpidr = READ_SPECIALREG(mpidr_el1);

	if (fa->fa_nreg < 1 || fa->fa_reg[0].addr != (mpidr & MPIDR_AFF))
		return 0;

	return (*cf->cf_attach->ca_match)(parent, match, aux);
}

int
mainbus_match_secondary(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *fa = aux;
	struct cfdata *cf = match;
	uint64_t mpidr = READ_SPECIALREG(mpidr_el1);

	if (fa->fa_nreg < 1 || fa->fa_reg[0].addr == (mpidr & MPIDR_AFF))
		return 0;

	return (*cf->cf_attach->ca_match)(parent, match, aux);
}

void
mainbus_attach_efi(struct device *self)
{
	struct mainbus_softc *sc = (struct mainbus_softc *)self;
	struct fdt_attach_args fa;
	int node = OF_finddevice("/chosen");

	if (node == 0 || OF_getproplen(node, "openbsd,uefi-system-table") <= 0)
		return;

	memset(&fa, 0, sizeof(fa));
	fa.fa_name = "efi";
	fa.fa_iot = sc->sc_iot;
	fa.fa_dmat = sc->sc_dmat;
	config_found(self, &fa, NULL);
}

void
mainbus_attach_framebuffer(struct device *self)
{
	int node = OF_finddevice("/chosen");

	if (node == 0)
		return;

	for (node = OF_child(node); node != 0; node = OF_peer(node))
		mainbus_attach_node(self, node, NULL);
}
