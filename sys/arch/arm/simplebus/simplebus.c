/* $OpenBSD: simplebus.c,v 1.7 2016/07/18 11:53:32 patrick Exp $ */
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

#include <dev/ofw/openfirm.h>

#include <arm/fdt.h>

int simplebus_match(struct device *, void *, void *);
void simplebus_attach(struct device *, struct device *, void *);

void simplebus_attach_node(struct device *, int);
int simplebus_bs_map(void *, bus_addr_t, bus_size_t, int, bus_space_handle_t *);

struct simplebus_softc {
	struct device		 sc_dev;
	int			 sc_node;
	bus_space_tag_t		 sc_iot;
	bus_dma_tag_t		 sc_dmat;
	int			 sc_acells;
	int			 sc_scells;
	int			 sc_pacells;
	int			 sc_pscells;
	struct bus_space	 sc_bus;
	int			*sc_ranges;
	int			 sc_rangeslen;
};

struct cfattach simplebus_ca = {
	sizeof(struct simplebus_softc), simplebus_match, simplebus_attach, NULL,
	config_activate_children
};

struct cfdriver simplebus_cd = {
	NULL, "simplebus", DV_DULL
};

/*
 * Simplebus is a generic bus with no special casings.
 */
int
simplebus_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *fa = (struct fdt_attach_args *)aux;

	if (fa->fa_node == 0)
		return (0);

	if (!OF_is_compatible(fa->fa_node, "simple-bus"))
		return (0);

	return (1);
}

void
simplebus_attach(struct device *parent, struct device *self, void *aux)
{
	struct simplebus_softc *sc = (struct simplebus_softc *)self;
	struct fdt_attach_args *fa = (struct fdt_attach_args *)aux;
	char name[32];
	int node;

	sc->sc_node = fa->fa_node;
	sc->sc_iot = fa->fa_iot;
	sc->sc_dmat = fa->fa_dmat;
	sc->sc_acells = OF_getpropint(sc->sc_node, "#address-cells",
	    fa->fa_acells);
	sc->sc_scells = OF_getpropint(sc->sc_node, "#size-cells",
	    fa->fa_scells);
	sc->sc_pacells = fa->fa_acells;
	sc->sc_pscells = fa->fa_scells;

	if (OF_getprop(sc->sc_node, "name", name, sizeof(name)) > 0) {
		name[sizeof(name) - 1] = 0;
		printf(": \"%s\"", name);
	}

	printf("\n");

	memcpy(&sc->sc_bus, sc->sc_iot, sizeof(sc->sc_bus));
	sc->sc_bus.bs_cookie = sc;
	sc->sc_bus.bs_map = simplebus_bs_map;

	sc->sc_rangeslen = OF_getproplen(sc->sc_node, "ranges");
	if (sc->sc_rangeslen > 0 && !(sc->sc_rangeslen % sizeof(uint32_t))) {
		sc->sc_ranges = malloc(sc->sc_rangeslen, M_TEMP, M_WAITOK);
		OF_getpropintarray(sc->sc_node, "ranges", sc->sc_ranges,
		    sc->sc_rangeslen);
	}

	/* Scan the whole tree. */
	for (node = OF_child(sc->sc_node);
	    node != 0;
	    node = OF_peer(node))
	{
		simplebus_attach_node(self, node);
	}
}

/*
 * Look for a driver that wants to be attached to this node.
 */
void
simplebus_attach_node(struct device *self, int node)
{
	struct simplebus_softc	*sc = (struct simplebus_softc *)self;
	struct fdt_attach_args	 fa;
	char			 buffer[128];
	int			 len;

	if (!OF_getprop(node, "compatible", buffer, sizeof(buffer)))
		return;

	if (OF_getprop(node, "status", buffer, sizeof(buffer)))
		if (!strcmp(buffer, "disabled"))
			return;

	memset(&fa, 0, sizeof(fa));
	fa.fa_name = "";
	fa.fa_node = node;
	fa.fa_iot = &sc->sc_bus;
	fa.fa_dmat = sc->sc_dmat;
	fa.fa_acells = sc->sc_acells;
	fa.fa_scells = sc->sc_scells;

	len = OF_getproplen(node, "reg");
	if (len > 0 && (len % sizeof(uint32_t)) == 0) {
		fa.fa_reg = malloc(len, M_DEVBUF, M_WAITOK);
		fa.fa_nreg = len / sizeof(uint32_t);

		OF_getpropintarray(node, "reg", fa.fa_reg, len);
	}

	len = OF_getproplen(node, "interrupts");
	if (len > 0 && (len % sizeof(uint32_t)) == 0) {
		fa.fa_intr = malloc(len, M_DEVBUF, M_WAITOK);
		fa.fa_nintr = len / sizeof(uint32_t);

		OF_getpropintarray(node, "interrupts", fa.fa_intr, len);
	}

	/* TODO: attach the device's clocks first? */

	config_found(self, &fa, NULL);

	free(fa.fa_reg, M_DEVBUF, fa.fa_nreg * sizeof(uint32_t));
	free(fa.fa_intr, M_DEVBUF, fa.fa_nintr * sizeof(uint32_t));
}

/*
 * Translate memory address if needed.
 */
int
simplebus_bs_map(void *t, bus_addr_t bpa, bus_size_t size,
    int flag, bus_space_handle_t *bshp)
{
	struct simplebus_softc *sc = (struct simplebus_softc *)t;
	uint64_t addr, rfrom, rto, rsize;
	uint32_t *range;
	int parent, rlen, rone;

	addr = bpa;
	parent = OF_parent(sc->sc_node);
	if (parent == 0)
		return bus_space_map(sc->sc_iot, addr, size, flag, bshp);

	if (sc->sc_rangeslen < 0)
		return EINVAL;
	if (sc->sc_rangeslen == 0)
		return bus_space_map(sc->sc_iot, addr, size, flag, bshp);

	rlen = sc->sc_rangeslen / sizeof(uint32_t);
	rone = sc->sc_pacells + sc->sc_acells + sc->sc_scells;

	/* For each range. */
	for (range = sc->sc_ranges; rlen >= rone; rlen -= rone, range += rone) {
		/* Extract from and size, so we can see if we fit. */
		rfrom = range[0];
		if (sc->sc_acells == 2)
			rfrom = (rfrom << 32) + range[1];
		rsize = range[sc->sc_acells + sc->sc_pacells];
		if (sc->sc_scells == 2)
			rsize = (rsize << 32) +
			    range[sc->sc_acells + sc->sc_pacells + 1];

		/* Try next, if we're not in the range. */
		if (addr < rfrom || (addr + size) > (rfrom + rsize))
			continue;

		/* All good, extract to address and translate. */
		rto = range[sc->sc_acells];
		if (sc->sc_pacells == 2)
			rto = (rto << 32) + range[sc->sc_acells + 1];

		addr -= rfrom;
		addr += rto;

		return bus_space_map(sc->sc_iot, addr, size, flag, bshp);
	}

	return ESRCH;
}
