/*	$OpenBSD: mvicu.c,v 1.3 2018/08/06 10:52:30 patrick Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

/* Registers. */
#define ICU_SETSPI_NSR_AL	0x10
#define ICU_SETSPI_NSR_AH	0x14
#define ICU_CLRSPI_NSR_AL	0x18
#define ICU_CLRSPI_NSR_AH	0x1c
#define ICU_INT_CFG(x)	(0x100 + (x) * 4)
#define  ICU_INT_ENABLE		(1 << 24)
#define  ICU_INT_EDGE		(1 << 28)
#define  ICU_INT_GROUP_SHIFT	29
#define  ICU_INT_MASK		0x3ff

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))

struct mvicu_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	uint32_t		sc_spi_ranges[4];

	struct interrupt_controller sc_ic;
	struct interrupt_controller *sc_parent_ic;
};

int mvicu_match(struct device *, void *, void *);
void mvicu_attach(struct device *, struct device *, void *);

struct cfattach	mvicu_ca = {
	sizeof (struct mvicu_softc), mvicu_match, mvicu_attach
};

struct cfdriver mvicu_cd = {
	NULL, "mvicu", DV_DULL
};

void	*mvicu_intr_establish(void *, int *, int, int (*)(void *),
	    void *, char *);
void	mvicu_intr_disestablish(void *);

int
mvicu_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "marvell,cp110-icu");
}

void
mvicu_attach(struct device *parent, struct device *self, void *aux)
{
	struct mvicu_softc *sc = (struct mvicu_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct interrupt_controller *ic;
	bus_addr_t low, high, setspi_addr, clrspi_addr;
	uint32_t phandle;
	int node;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	phandle = OF_getpropint(faa->fa_node, "msi-parent", 0);
	node = OF_getnodebyphandle(phandle);
	if (node == 0) {
		printf(": GICP not found\n");
		return;
	}
	OF_getpropintarray(node, "marvell,spi-ranges", sc->sc_spi_ranges,
	    sizeof(sc->sc_spi_ranges));

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	/*
	 * This driver assumes that the ICU has been configured by the
	 * firmware.  Do some (minimal) checks to verify that has
	 * indeed been done.
	 */
	low = HREAD4(sc, ICU_SETSPI_NSR_AL);
	high = HREAD4(sc, ICU_SETSPI_NSR_AH);
	setspi_addr = (high << 32) | low;
	low = HREAD4(sc, ICU_CLRSPI_NSR_AL);
	high = HREAD4(sc, ICU_CLRSPI_NSR_AH);
	clrspi_addr = (high << 32) | low;
	if (setspi_addr == 0 || clrspi_addr == 0) {
		printf(": not configured by firmware\n");
		return;
	}

	printf("\n");

	extern uint32_t fdt_intr_get_parent(int);
	phandle = fdt_intr_get_parent(node);
	extern LIST_HEAD(, interrupt_controller) interrupt_controllers;
	LIST_FOREACH(ic, &interrupt_controllers, ic_list) {
		if (ic->ic_phandle == phandle)
			break;
	}
	sc->sc_parent_ic = ic;

	sc->sc_ic.ic_node = faa->fa_node;
	sc->sc_ic.ic_cookie = sc;
	sc->sc_ic.ic_establish = mvicu_intr_establish;
	sc->sc_ic.ic_disestablish = mvicu_intr_disestablish;
	fdt_intr_register(&sc->sc_ic);
}

void *
mvicu_intr_establish(void *cookie, int *cell, int level,
    int (*func)(void *), void *arg, char *name)
{
	struct mvicu_softc *sc = cookie;
	struct interrupt_controller *ic = sc->sc_parent_ic;
	uint32_t group = cell[0];
	uint32_t idx = cell[1];
	uint32_t interrupt[3];
	uint32_t reg;
	int i;

	if (ic == NULL)
		return NULL;

	reg = HREAD4(sc, ICU_INT_CFG(idx));
	if ((reg & ICU_INT_ENABLE) == 0 ||
	    (reg >> ICU_INT_GROUP_SHIFT) != group)
		return NULL;

	/* Convert to GIC interrupt source. */
	idx = reg & ICU_INT_MASK;
	for (i = 0; i < nitems(sc->sc_spi_ranges); i += 2) {
		if (idx < sc->sc_spi_ranges[i + 1]) {
			idx += sc->sc_spi_ranges[i];
			break;
		}
		idx -= sc->sc_spi_ranges[i];
	}
	if (i == nitems(sc->sc_spi_ranges))
		return NULL;
	interrupt[0] = 0;
	interrupt[1] = idx - 32;
	interrupt[2] = cell[2];
	return ic->ic_establish(ic->ic_cookie, interrupt, level,
	    func, arg, name);
}

void
mvicu_intr_disestablish(void *cookie)
{
	panic("%s", __func__);
}
