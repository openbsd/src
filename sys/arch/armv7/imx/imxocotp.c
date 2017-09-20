/* $OpenBSD: imxocotp.c,v 1.5 2017/09/20 11:21:58 kettenis Exp $ */
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/socket.h>
#include <sys/timeout.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <armv7/imx/imxocotpvar.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

/* registers */
#define OCOTP_ANA0	0x4d0
#define OCOTP_ANA1	0x4e0
#define OCOTP_ANA2	0x4f0
#define OCOTP_MAC0	0x620
#define OCOTP_MAC1	0x630

struct imxocotp_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct imxocotp_softc *imxocotp_sc;

int	imxocotp_match(struct device *, void *, void *);
void	imxocotp_attach(struct device *, struct device *, void *);

struct cfattach imxocotp_ca = {
	sizeof (struct imxocotp_softc), imxocotp_match, imxocotp_attach
};

struct cfdriver imxocotp_cd = {
	NULL, "imxocotp", DV_DULL
};

int
imxocotp_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	if (OF_is_compatible(faa->fa_node, "fsl,imx6q-ocotp"))
		return 10;	/* Must beat syscon(4). */

	return 0;
}

void
imxocotp_attach(struct device *parent, struct device *self, void *aux)
{
	struct imxocotp_softc *sc = (struct imxocotp_softc *)self;
	struct fdt_attach_args *faa = aux;

	KASSERT(faa->fa_nreg >= 1);

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh))
		panic("%s: bus_space_map failed!", __func__);

	imxocotp_sc = sc;
	printf("\n");
}

void
imxocotp_get_ethernet_address(u_int8_t* mac)
{
	uint32_t value;

	value = bus_space_read_4(imxocotp_sc->sc_iot, imxocotp_sc->sc_ioh, OCOTP_MAC0);
	mac[5] = value & 0xff;
	mac[4] = (value >> 8) & 0xff;
	mac[3] = (value >> 16) & 0xff;
	mac[2] = (value >> 24) & 0xff;
	value = bus_space_read_4(imxocotp_sc->sc_iot, imxocotp_sc->sc_ioh, OCOTP_MAC1);
	mac[1] = value & 0xff;
	mac[0] = (value >> 8) & 0xff;
}

uint32_t
imxocotp_get_temperature_calibration(void)
{
	return bus_space_read_4(imxocotp_sc->sc_iot, imxocotp_sc->sc_ioh, OCOTP_ANA1);
}
