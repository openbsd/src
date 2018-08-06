/*	$OpenBSD: imxgpc.c,v 1.4 2018/08/06 10:52:30 patrick Exp $	*/
/*
 * Copyright (c) 2016 Mark Kettenis
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

#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>

struct imxgpc_softc {
	struct device	sc_dev;
	struct interrupt_controller sc_ic;
};

int	imxgpc_match(struct device *, void *, void *);
void	imxgpc_attach(struct device *, struct device *, void *);

struct cfattach imxgpc_ca = {
	sizeof(struct imxgpc_softc), imxgpc_match, imxgpc_attach
};

struct cfdriver imxgpc_cd = {
	NULL, "imxgpc", DV_DULL
};

int
imxgpc_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "fsl,imx6q-gpc") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx7d-gpc") ||
	    OF_is_compatible(faa->fa_node, "fsl,imx8mq-gpc"));
}

void
imxgpc_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct imxgpc_softc *sc = (struct imxgpc_softc *)self;

	sc->sc_ic.ic_node = faa->fa_node;
	sc->sc_ic.ic_cookie = &sc->sc_ic;
	sc->sc_ic.ic_establish = fdt_intr_parent_establish;
	sc->sc_ic.ic_disestablish = fdt_intr_parent_disestablish;
	fdt_intr_register(&sc->sc_ic);

	printf("\n");
}
