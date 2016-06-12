/* $OpenBSD: simplebus.c,v 1.5 2016/06/12 13:10:06 kettenis Exp $ */
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

struct simplebus_softc {
	struct device		 sc_dev;
	int			 sc_node;
	bus_space_tag_t		 sc_iot;
	bus_dma_tag_t		 sc_dmat;
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

	if (OF_getprop(sc->sc_node, "name", name, sizeof(name)) > 0) {
		name[sizeof(name) - 1] = 0;
		printf(": \"%s\"", name);
	}

	printf("\n");

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
	fa.fa_iot = sc->sc_iot;
	fa.fa_dmat = sc->sc_dmat;

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
