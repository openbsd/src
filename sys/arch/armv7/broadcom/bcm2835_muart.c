/* $OpenBSD: bcm2835_muart.c,v 1.1 2016/08/07 17:46:36 kettenis Exp $ */
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
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <armv7/armv7/armv7_machdep.h>
#include <arm/armv7/armv7var.h>

#include <dev/ic/comvar.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

int	 bcmmuart_match(struct device *, void *, void *);
void	 bcmmuart_attach(struct device *, struct device *, void *);

void	 bcmmuart_init_cons(void);

struct cfdriver bcmmuart_cd = {
	NULL, "bcmmuart", DV_DULL
};

struct cfattach bcmmuart_ca = {
	sizeof (struct com_softc), bcmmuart_match, bcmmuart_attach,
};

int
bcmmuart_match(struct device *parent, void *self, void *aux)
{
	struct fdt_attach_args *faa = aux;

	if (OF_is_compatible(faa->fa_node, "brcm,bcm2835-aux-uart"))
		return 1;

	return 0;
}

void
bcmmuart_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (struct com_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1)
		return;

	sc->sc_frequency = 500000000;
	sc->sc_uarttype = COM_UART_16550;

	if (faa->fa_reg[0].addr == comconsaddr + 0x3f000000) {
		sc->sc_iobase = comconsaddr;
		sc->sc_iot = comconsiot;
		sc->sc_ioh = comconsioh;
	} else {
		sc->sc_iot = faa->fa_iot; /* XXX */
		sc->sc_iobase = faa->fa_reg[0].addr;
		if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
		    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
			printf("%s: bus_space_map failed\n", __func__);
			return;
		}
	}

	com_attach_subr(sc);

	arm_intr_establish_fdt(faa->fa_node, IPL_TTY, comintr, sc,
	    sc->sc_dev.dv_xname);

	extern struct cfdriver com_cd;
	com_cd.cd_devs = bcmmuart_cd.cd_devs;
	com_cd.cd_ndevs = bcmmuart_cd.cd_ndevs;
}

extern int comcnspeed;
extern int comcnmode;

void
bcmmuart_init_cons(void)
{
	struct fdt_reg reg;
	void *node;

	if ((node = fdt_find_cons("brcm,bcm2835-aux-uart")) == NULL)
		return;
	if (fdt_get_reg(node, 0, &reg))
		return;

	comcnattach(&armv7_a4x_bs_tag, reg.addr, comcnspeed, 500000000,
	    comcnmode);
}
