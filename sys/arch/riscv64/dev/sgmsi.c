/*	$OpenBSD: sgmsi.c,v 1.1 2025/12/02 19:57:29 kettenis Exp $	*/
/*
 * Copyright (c) 2025 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

struct sgmsi_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_clr_ioh;

	uint32_t		sc_msi_mask;
	bus_addr_t		sc_msi_addr;
	uint32_t		sc_msi_range[4];
	struct interrupt_controller sc_msi_ic;
};

struct sgmsi_intrhand {
	struct sgmsi_softc	*ih_sc;
	int			(*ih_func)(void *);
	void			*ih_arg;
	int			ih_msi;
	struct interrupt_controller *ih_ic;
	void			*ih_cookie;
};

int	sgmsi_match(struct device *, void *, void *);
void	sgmsi_attach(struct device *, struct device *, void *);

const struct cfattach sgmsi_ca = {
	sizeof (struct sgmsi_softc), sgmsi_match, sgmsi_attach
};

struct cfdriver sgmsi_cd = {
	NULL, "sgmsi", DV_DULL
};

void	*sgmsi_intr_establish_msi(void *, uint64_t *, uint64_t *,
	    int, struct cpu_info *, int (*)(void *), void *, char *);
void	sgmsi_intr_disestablish_msi(void *);
void	sgmsi_barrier(void *);

int
sgmsi_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "sophgo,sg2042-msi");
}

void
sgmsi_attach(struct device *parent, struct device *self, void *aux)
{
	struct sgmsi_softc *sc = (struct sgmsi_softc *)self;
	struct fdt_attach_args *faa = aux;
	int idx;

	if (OF_getpropintarray(faa->fa_node, "msi-ranges", sc->sc_msi_range,
	    sizeof(sc->sc_msi_range)) != sizeof(sc->sc_msi_range)) {
		printf(": invalid msi-ranges property\n");
		return;
	}
	sc->sc_msi_mask = (1UL << sc->sc_msi_range[3]) - 1;

	idx = OF_getindex(faa->fa_node, "doorbell", "reg-names");
	if (idx < 0 || idx >= faa->fa_nreg) {
		printf(": no doorbell registers\n");
		return;
	}
	sc->sc_msi_addr = faa->fa_reg[idx].addr;

	sc->sc_iot = faa->fa_iot;
	idx = OF_getindex(faa->fa_node, "clr", "reg-names");
	if (idx < 0 || idx >= faa->fa_nreg) {
		printf(": no clr registers\n");
		return;
	}
	if (bus_space_map(sc->sc_iot, faa->fa_reg[idx].addr,
	    faa->fa_reg[idx].size, 0, &sc->sc_clr_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	printf("\n");

	sc->sc_msi_ic.ic_node = faa->fa_node;
	sc->sc_msi_ic.ic_cookie = sc;
	sc->sc_msi_ic.ic_establish_msi = sgmsi_intr_establish_msi;
	sc->sc_msi_ic.ic_disestablish = sgmsi_intr_disestablish_msi;
	sc->sc_msi_ic.ic_barrier = sgmsi_barrier;
	fdt_intr_register(&sc->sc_msi_ic);
}

int
sgmsi_intr(void *arg)
{
	struct sgmsi_intrhand *ih = arg;
	struct sgmsi_softc *sc = ih->ih_sc;

	/* ACK the interrupt. */
	bus_space_write_4(sc->sc_iot, sc->sc_clr_ioh, 0, (1U << ih->ih_msi));

	return ih->ih_func(ih->ih_arg);
}

extern LIST_HEAD(, interrupt_controller) interrupt_controllers;

void *
sgmsi_intr_establish_msi(void *cookie, uint64_t *addr, uint64_t *data,
    int level, struct cpu_info *ci, int (*func)(void *), void *arg, char *name)
{
	struct sgmsi_softc *sc = cookie;
	struct sgmsi_intrhand *ih;
	struct interrupt_controller *ic;
	uint32_t cells[2];
	int msi;

	msi = ffs(sc->sc_msi_mask) - 1;
	if (msi == -1)
		return NULL;
	cells[0] = sc->sc_msi_range[1] + msi;
	cells[1] = sc->sc_msi_range[2];

	/* Lookup the parent interrupt controller. */
	LIST_FOREACH(ic, &interrupt_controllers, ic_list) {
		if (ic->ic_phandle == sc->sc_msi_range[0])
			break;
	}
	if (ic == NULL)
		return NULL;

	ih = malloc(sizeof(struct sgmsi_intrhand), M_DEVBUF, M_WAITOK);
	ih->ih_sc = sc;
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_msi = msi;
	ih->ih_ic = ic;
	ih->ih_cookie = ic->ic_establish(ic->ic_cookie, cells, level,
	    ci, sgmsi_intr, ih, name);
	if (ih->ih_cookie == NULL) {
		free(ih, M_DEVBUF, sizeof(*ih));
		return NULL;
	}
	sc->sc_msi_mask &= ~(1U << ih->ih_msi);

	/* ACK the interrupt. */
	bus_space_write_4(sc->sc_iot, sc->sc_clr_ioh, 0, (1U << ih->ih_msi));

	*addr = sc->sc_msi_addr;
	*data = (1U << ih->ih_msi);
	return ih;
}

void
sgmsi_intr_disestablish_msi(void *cookie)
{
	struct sgmsi_intrhand *ih = cookie;

	ih->ih_ic->ic_disestablish(ih->ih_cookie);
	ih->ih_sc->sc_msi_mask |= (1U << ih->ih_msi);
	free(ih, M_DEVBUF, sizeof(*ih));
}

void
sgmsi_barrier(void *cookie)
{
	struct sgmsi_intrhand *ih = cookie;

	ih->ih_ic->ic_barrier(ih->ih_cookie);
}
