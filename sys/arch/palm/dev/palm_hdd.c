/*	$OpenBSD: palm_hdd.c,v 1.3 2011/05/09 22:33:54 matthew Exp $	*/

/*
 * Copyright (c) 2009 Marek Vasut <marex@openbsd.org>
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
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/palm_var.h>

#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

#include <arch/arm/xscale/pxa2x0_gpio.h>
#include <arch/arm/xscale/pxa2x0var.h>

struct palm_hdd_softc {
	struct wdc_softc	sc_wdcdev;
	struct channel_softc	sc_channel;
	void			*sc_ih;
};

int	palm_hdd_match(struct device *, void *, void *);
void	palm_hdd_attach(struct device *, struct device *, void *);

struct cfattach palm_hdd_ca = {
	sizeof(struct palm_hdd_softc),
	palm_hdd_match,
	palm_hdd_attach,
};

int palm_hdd_match(struct device *parent, void *match, void *aux)
{
	return mach_is_palmld;
}

void palm_hdd_attach(struct device *parent, struct device *self, void *aux)
{
	struct palm_hdd_softc *sc = (void *)self;
	struct channel_softc *chp = &sc->sc_channel;
	struct pxaip_attach_args *pxa = aux;
	int ret;

	chp->cmd_iot = pxa->pxa_iot;
	chp->ctl_iot = pxa->pxa_iot;

	ret = bus_space_map(chp->cmd_iot, 0x20000010, WDC_NREG,
		0, &chp->cmd_ioh);
	if (ret) {
		printf(": Failed mapping CMD register\n");
		return;
	}

	ret = bus_space_map(chp->ctl_iot, 0x2000000e, 2, 0, &chp->ctl_ioh);
	if (ret) {
		printf(": Failed mapping CTL register\n");
		return;
	}

	sc->sc_ih = pxa2x0_gpio_intr_establish(95, IST_EDGE_BOTH, IPL_BIO, wdcintr, chp, self->dv_xname);

	pxa2x0_gpio_set_bit(115);	/* PWEN */
	pxa2x0_gpio_clear_bit(98);	/* RESET */
	delay(50);
	pxa2x0_gpio_set_bit(98);	/* RESET */
	delay(50);

	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_DMA | WDC_CAPABILITY_SINGLE_DRIVE/* | WDC_CAPABILITY_IRQACK*/;
	sc->sc_wdcdev.PIO_cap = 4;
	sc->sc_wdcdev.channels = &chp;
	sc->sc_wdcdev.nchannels = 1;

	chp->channel = 0;
	chp->wdc = &sc->sc_wdcdev;
	chp->ch_queue = wdc_alloc_queue();

	printf("\n");

	wdcattach(chp);
	wdc_print_current_modes(chp);
}
