/*	$OpenBSD: pxa2x0_com.c,v 1.9 2008/11/22 17:05:35 drahn Exp $ */
/*	$NetBSD: pxa2x0_com.c,v 1.4 2003/07/15 00:24:55 lukem Exp $	*/

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

#ifndef COM_PXA2X0
#error "You must use options COM_PXA2X0 to get PXA2x0 serial port support"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#define com_isr 8
#define ISR_RECV	(ISR_RXPL | ISR_XMODE | ISR_RCVEIR)

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>

#ifdef __zaurus__
#include <zaurus/dev/zaurus_scoopvar.h>
#endif

int	pxauart_match(struct device *, void *, void *);
void	pxauart_attach(struct device *, struct device *, void *);
void	pxauart_power(int why, void *);

struct cfattach com_pxaip_ca = {
        sizeof (struct com_softc), pxauart_match, pxauart_attach
};

int
pxauart_match(struct device *parent, void *cf, void *aux)
{
	struct pxaip_attach_args *pxa = aux;
	bus_space_tag_t bt = &pxa2x0_a4x_bs_tag;	/* XXX: This sucks */
	bus_space_handle_t bh;
	int rv;

	switch (pxa->pxa_addr) {
	case PXA2X0_FFUART_BASE:
		if (pxa->pxa_intr != PXA2X0_INT_FFUART)
			return (0);
		break;

	case PXA2X0_STUART_BASE:
		if (pxa->pxa_intr != PXA2X0_INT_STUART)
			return (0);
		break;

	case PXA2X0_BTUART_BASE:	/* XXX: Config file option ... */
		if (pxa->pxa_intr != PXA2X0_INT_BTUART)
			return (0);
		break;

	default:
		return (0);
	}

	pxa->pxa_size = 0x20;

	{
		extern bus_addr_t comconsaddr;

		if (comconsaddr == pxa->pxa_addr)
			return (1);
	}

	if (bus_space_map(bt, pxa->pxa_addr, pxa->pxa_size, 0, &bh))
		return (0);

	/* Make sure the UART is enabled */
	bus_space_write_1(bt, bh, com_ier, IER_EUART);

	rv = comprobe1(bt, bh);
	bus_space_unmap(bt, bh, pxa->pxa_size);

	return (rv);
}

void
pxauart_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (struct com_softc *)self;
	struct pxaip_attach_args *pxa = aux;

	sc->sc_iot = &pxa2x0_a4x_bs_tag;	/* XXX: This sucks */
	sc->sc_iobase = pxa->pxa_addr;
	sc->sc_frequency = PXA2X0_COM_FREQ;
	sc->sc_uarttype = COM_UART_PXA2X0;

#if 0
	if (com_is_console(sc->sc_iot, sc->sc_iobase, &sc->sc_ioh) == 0 &&
	    bus_space_map(sc->sc_iot, sc->sc_iobase, pxa->pxa_size, 0,
			  &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}
#endif
	bus_space_map(sc->sc_iot, sc->sc_iobase, pxa->pxa_size, 0, &sc->sc_ioh);

	com_attach_subr(sc);

	(void)pxa2x0_intr_establish(pxa->pxa_intr, IPL_TTY, comintr,
	    sc, sc->sc_dev.dv_xname);

	(void)powerhook_establish(&pxauart_power, sc);
}

void
pxauart_power(int why, void *arg)
{
	struct com_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp = sc->sc_tty;

	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
#ifdef __zaurus__
		if (sc->enabled && ISSET(sc->sc_hwflags, COM_HW_SIR))
			scoop_set_irled(0);
#endif
		break;
	case PWR_RESUME:
		if (sc->enabled) {
			sc->sc_initialize = 1;
			comparam(tp, &tp->t_termios);
			bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);

			if (ISSET(sc->sc_hwflags, COM_HW_SIR)) {
#ifdef __zaurus__
				scoop_set_irled(1);
#endif
				bus_space_write_1(iot, ioh, com_isr,
				    ISR_RECV);
			}
		}
		break;
	}
}
