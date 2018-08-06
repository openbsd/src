/*	$OpenBSD: pluart_fdt.c,v 1.2 2018/08/06 10:52:30 patrick Exp $	*/
/*
 * Copyright (c) 2014 Patrick Wildt <patrick@blueri.se>
 * Copyright (c) 2005 Dale Rahn <drahn@dalerahn.com>
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
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ic/pluartvar.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

int	pluart_fdt_match(struct device *, void *, void *);
void	pluart_fdt_attach(struct device *, struct device *, void *);

struct cfattach pluart_fdt_ca = {
	sizeof(struct pluart_softc), pluart_fdt_match, pluart_fdt_attach
};

void
pluart_init_cons(void)
{
	struct fdt_reg reg;
	void *node;

	if ((node = fdt_find_cons("arm,pl011")) == NULL)
		return;
	if (fdt_get_reg(node, 0, &reg))
		return;

	pluartcnattach(fdt_cons_bs_tag, reg.addr, B115200, TTYDEF_CFLAG);
}

int
pluart_fdt_match(struct device *parent, void *self, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "arm,pl011");
}

void
pluart_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct pluart_softc *sc = (struct pluart_softc *) self;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_irq = fdt_intr_establish(faa->fa_node, IPL_TTY, pluart_intr,
	    sc, sc->sc_dev.dv_xname);

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr, faa->fa_reg[0].size,
	    0, &sc->sc_ioh))
		panic("pluartattach: bus_space_map failed!");

	pluart_attach_common(sc, stdout_node == faa->fa_node);
}
