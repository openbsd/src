/* $OpenBSD: omap_com.c,v 1.5 2011/03/22 17:46:02 deraadt Exp $ */
/*
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

/* pick up armv7_a4x_bs_tag */
#include <arch/arm/armv7/armv7var.h>

#include <arch/beagle/beagle/ahb.h>

#define com_isr 8
#define ISR_RECV	(ISR_RXPL | ISR_XMODE | ISR_RCVEIR)

int	omapuart_match(struct device *, void *, void *);
void	omapuart_attach(struct device *, struct device *, void *);
int	omapuart_activate(struct device *, int);

struct cfattach com_ahb_ca = {
        sizeof (struct com_softc), omapuart_match, omapuart_attach, NULL, 
	omapuart_activate
};

int
omapuart_match(struct device *parent, void *cf, void *aux)
{
        struct ahb_attach_args *aa = aux;
	bus_space_tag_t bt = &armv7_a4x_bs_tag;	/* XXX: This sucks */
	bus_space_handle_t bh;
	int rv;

	/* XXX */
	if (aa->aa_addr == 0x4806A000 && aa->aa_intr == 72)
		return 1;
	if (aa->aa_addr == 0x4806C000 && aa->aa_intr == 73)
		return 1;
	if (aa->aa_addr == 0x4806E000 && aa->aa_intr == 74)
		return 1;
	{
		extern bus_addr_t comconsaddr;

		if (comconsaddr == aa->aa_addr)
			return (1);
	}

	if (bus_space_map(bt, aa->aa_addr, aa->aa_size, 0, &bh))
		return (0);

	/* Make sure the UART is enabled - XXX */
	bus_space_write_1(bt, bh, com_ier, IER_EUART);

	rv = comprobe1(bt, bh);
	bus_space_unmap(bt, bh, aa->aa_size);

	return (rv);
}

void
omapuart_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (struct com_softc *)self;
        struct ahb_attach_args *aa = aux;

	sc->sc_iot = &armv7_a4x_bs_tag;	/* XXX: This sucks */
	sc->sc_iobase = aa->aa_addr;
	sc->sc_frequency = 48000000;
	sc->sc_uarttype = COM_UART_TI16750;

	bus_space_map(sc->sc_iot, sc->sc_iobase, aa->aa_size, 0, &sc->sc_ioh);

	com_attach_subr(sc);

	(void)intc_intr_establish(aa->aa_intr, IPL_TTY, comintr,
	    sc, sc->sc_dev.dv_xname);
}

int
omapuart_activate(struct device *self, int act)
{
	struct com_softc *sc = (struct com_softc *)self;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp = sc->sc_tty;

	switch (act) {
	case DVACT_SUSPEND:
		break;
	case DVACT_RESUME:
		if (sc->enabled) {
			sc->sc_initialize = 1;
			comparam(tp, &tp->t_termios);
			bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);

			if (ISSET(sc->sc_hwflags, COM_HW_SIR)) {
				bus_space_write_1(iot, ioh, com_isr,
				    ISR_RECV);
			}
		}
		break;
	}
	return 0;
}
