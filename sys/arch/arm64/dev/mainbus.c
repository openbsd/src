/* $OpenBSD: mainbus.c,v 1.3 2017/02/22 22:55:27 patrick Exp $ */
/*
 * Copyright (c) 2016 Patrick Wildt <patrick@blueri.se>
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

void mainbus_attach_node(struct device *, int);

struct mainbus_softc {
	struct device		 sc_dev;
	bus_space_tag_t		 sc_iot;
	bus_dma_tag_t		 sc_dmat;
	int			 sc_acells;
	int			 sc_scells;
	int			*sc_ranges;
	int			 sc_rangeslen;
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
	char buffer[128];
	int node, len;

	if ((node = OF_peer(0)) == 0)
		panic("mainbus: no device tree");

	arm_intr_init_fdt();
	agtimer_init();

	sc->sc_iot = &arm64_bs_tag;
	sc->sc_dmat = &mainbus_dma_tag;
	sc->sc_acells = OF_getpropint(OF_peer(0), "#address-cells", 1);
	sc->sc_scells = OF_getpropint(OF_peer(0), "#size-cells", 1);

	if ((len = OF_getprop(node, "model", buffer, sizeof(buffer))) > 0) {
		printf(": %s\n", buffer);
		hw_prod = malloc(len, M_DEVBUF, M_NOWAIT);
		if (hw_prod)
			strlcpy(hw_prod, buffer, len);
	} else
		printf(": unknown model\n");

	/* Attach CPU first. */
	mainbus_legacy_found(self, "cpu");

	/* TODO: Scan for interrupt controllers and attach them first? */

	sc->sc_rangeslen = OF_getproplen(OF_peer(0), "ranges");
	if (sc->sc_rangeslen > 0 && !(sc->sc_rangeslen % sizeof(uint32_t))) {
		sc->sc_ranges = malloc(sc->sc_rangeslen, M_TEMP, M_WAITOK);
		OF_getpropintarray(OF_peer(0), "ranges", sc->sc_ranges,
		    sc->sc_rangeslen);
	}

	/* Scan the whole tree. */
	for (node = OF_child(node);
	    node != 0;
	    node = OF_peer(node))
	{
		mainbus_attach_node(self, node);
	}
}

/*
 * Look for a driver that wants to be attached to this node.
 */
void
mainbus_attach_node(struct device *self, int node)
{
	struct mainbus_softc	*sc = (struct mainbus_softc *)self;
	struct fdt_attach_args	 fa;
	char			 buffer[128];
	int			 i, len, line;
	uint32_t		*cell, *reg;

	if (!OF_getprop(node, "compatible", buffer, sizeof(buffer)))
		return;

	if (OF_getprop(node, "status", buffer, sizeof(buffer)))
		if (!strcmp(buffer, "disabled"))
			return;

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

	/* TODO: attach the device's clocks first? */

	config_found(self, &fa, NULL);

	free(fa.fa_reg, M_DEVBUF, fa.fa_nreg * sizeof(struct fdt_reg));
	free(fa.fa_intr, M_DEVBUF, fa.fa_nintr * sizeof(uint32_t));
}

/*
 * Legacy support for SoCs that do not use FDT.
 */
void
mainbus_legacy_found(struct device *self, char *name)
{
	union mainbus_attach_args ma;

	memset(&ma, 0, sizeof(ma));
	ma.ma_name = name;

	config_found(self, &ma, NULL);
}
