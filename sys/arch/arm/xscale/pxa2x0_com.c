/*	$OpenBSD: pxa2x0_com.c,v 1.2 2005/01/02 19:52:36 drahn Exp $ */
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

#include <sys/cdefs.h>
/*
__KERNEL_RCSID(0, "$NetBSD: pxa2x0_com.c,v 1.4 2003/07/15 00:24:55 lukem Exp $");
*/

#ifndef COM_PXA2X0
#error "You must use options COM_PXA2X0 to get PXA2x0 serial port support"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <arm/xscale/pxacomreg.h>
#include <arm/xscale/pxacomvar.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>

static int	pxauart_match(struct device *, void *, void *);
static void	pxauart_attach(struct device *, struct device *, void *);

#ifdef __NetBSD__
CFATTACH_DECL(pxauart, sizeof(struct com_softc),
    pxauart_match, pxauart_attach, NULL, NULL);
#else
struct cfattach pxauart_ca = {
        sizeof (struct com_softc), pxauart_match, pxauart_attach
};
	 
struct cfdriver pxauart_cd = {
	NULL, "pxauart", DV_DULL
};
#endif

static int
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

static void
pxauart_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (struct com_softc *)self;
	struct pxaip_attach_args *pxa = aux;

	sc->sc_iot = &pxa2x0_a4x_bs_tag;	/* XXX: This sucks */
	sc->sc_iobase = pxa->pxa_addr;
	sc->sc_frequency = PXA2X0_COM_FREQ;

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

	pxa2x0_intr_establish(pxa->pxa_intr, IPL_SERIAL, comintr, sc);
}
