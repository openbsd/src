/*	$OpenBSD: qcuart_fdt.c,v 1.1 2026/01/29 11:23:35 kettenis Exp $	*/
/*
 * Copyright (c) 2026 Mark Kettenis <kettenis@openbsd.org>
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

#include <dev/ic/qcuartvar.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

int	qcuart_fdt_match(struct device *, void *, void *);
void	qcuart_fdt_attach(struct device *, struct device *, void *);

const struct cfattach qcuart_fdt_ca = {
	sizeof(struct qcuart_softc), qcuart_fdt_match, qcuart_fdt_attach
};

void
qcuart_init_cons(void)
{
	struct fdt_reg reg;
	void *node;

	if ((node = fdt_find_cons("qcom,geni-uart")) == NULL &&
	    (node = fdt_find_cons("qcom,geni-debug-uart")) == NULL)
		return;
	if (fdt_get_reg(node, 0, &reg))
		return;

	qcuartcnattach(fdt_cons_bs_tag, reg.addr);
}

int
qcuart_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "qcom,geni-debug-uart");
}

void
qcuart_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct qcuart_softc *sc = (struct qcuart_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	if (faa->fa_node == stdout_node)
		sc->sc_conspeed = stdout_speed;

	sc->sc_ih = fdt_intr_establish_idx(faa->fa_node, 0, IPL_TTY,
	    qcuart_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish hard interrupt\n");
		return;
	}

	qcuart_attach_common(sc, faa->fa_node == stdout_node);
}
