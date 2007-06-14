/*	$OpenBSD: pxa2x0_ohci.c,v 1.21 2007/06/14 19:18:49 deraadt Exp $ */

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
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
#include <sys/kernel.h>
#include <sys/timeout.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ohcireg.h>
#include <dev/usb/ohcivar.h>

int	pxaohci_match(struct device *, void *, void *);
void	pxaohci_attach(struct device *, struct device *, void *);
int	pxaohci_detach(struct device *, int);
void	pxaohci_power(int, void *);

struct pxaohci_softc {
	ohci_softc_t	sc;
	void		*sc_ih;
};

void	pxaohci_enable(struct pxaohci_softc *);
void	pxaohci_disable(struct pxaohci_softc *);

struct cfattach pxaohci_ca = {
        sizeof (struct pxaohci_softc), pxaohci_match, pxaohci_attach,
	pxaohci_detach, ohci_activate
};

int
pxaohci_match(struct device *parent, void *match, void *aux)
{
	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) != CPU_ID_PXA27X)
		return (0);

	return (1);
}

void
pxaohci_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxaohci_softc		*sc = (struct pxaohci_softc *)self;
	struct pxaip_attach_args	*pxa = aux;
	usbd_status			r;

	sc->sc.iot = pxa->pxa_iot;
	sc->sc.sc_bus.dmatag = pxa->pxa_dmat;
	sc->sc_ih = NULL;
	sc->sc.sc_size = 0;

	/* Map I/O space */
	if (bus_space_map(sc->sc.iot, PXA2X0_USBHC_BASE, PXA2X0_USBHC_SIZE, 0,
	    &sc->sc.ioh)) {
		printf(": cannot map mem space\n");
		return;
	}
	sc->sc.sc_size = PXA2X0_USBHC_SIZE;

	/* XXX copied from ohci_pci.c. needed? */
	bus_space_barrier(sc->sc.iot, sc->sc.ioh, 0, sc->sc.sc_size,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	/* start the usb clock */
	pxa2x0_clkman_config(CKEN_USBHC, 1);
	pxaohci_enable(sc);

	/* Disable interrupts, so we don't get any spurious ones. */
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, OHCI_INTERRUPT_DISABLE,
	    OHCI_MIE);

	sc->sc_ih = pxa2x0_intr_establish(PXA2X0_INT_USBH1, IPL_USB,
	    ohci_intr, &sc->sc, sc->sc.sc_bus.bdev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		pxaohci_disable(sc);
		pxa2x0_clkman_config(CKEN_USBHC, 0);
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
		return;
	}

	strlcpy(sc->sc.sc_vendor, "PXA27x", sizeof(sc->sc.sc_vendor));

	if (ohci_checkrev(&sc->sc) != USBD_NORMAL_COMPLETION)
		goto unsupported;

	r = ohci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n",
		    sc->sc.sc_bus.bdev.dv_xname, r);
unsupported:
		pxa2x0_intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
		pxaohci_disable(sc);
		pxa2x0_clkman_config(CKEN_USBHC, 0);
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
		return;
	}

	sc->sc.sc_powerhook = powerhook_establish(pxaohci_power, sc);
	if (sc->sc.sc_powerhook == NULL)
		printf("%s: cannot establish powerhook\n",
		    sc->sc.sc_bus.bdev.dv_xname);

	sc->sc.sc_child = config_found((void *)sc, &sc->sc.sc_bus,
	    usbctlprint);
}

int
pxaohci_detach(struct device *self, int flags)
{
	struct pxaohci_softc		*sc = (struct pxaohci_softc *)self;
	int				rv;

	rv = ohci_detach(&sc->sc, flags);
	if (rv)
		return (rv);

	if (sc->sc.sc_powerhook != NULL) {
		powerhook_disestablish(sc->sc.sc_powerhook);
		sc->sc.sc_powerhook = NULL;
	}

	if (sc->sc_ih != NULL) {
		pxa2x0_intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}

	pxaohci_disable(sc);

	/* stop clock */
	pxa2x0_clkman_config(CKEN_USBHC, 0);

	if (sc->sc.sc_size) {
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}

	return (0);
}


void
pxaohci_power(int why, void *arg)
{
	struct pxaohci_softc		*sc = (struct pxaohci_softc *)arg;
	int				s;

	s = splhardusb();
	sc->sc.sc_bus.use_polling++;
	switch (why) {
	case PWR_STANDBY:
	case PWR_SUSPEND:
		ohci_power(why, &sc->sc);
		pxa2x0_clkman_config(CKEN_USBHC, 0);
		break;

	case PWR_RESUME:
		pxa2x0_clkman_config(CKEN_USBHC, 1);
		pxaohci_enable(sc);
		ohci_power(why, &sc->sc);
		break;
	}
	sc->sc.sc_bus.use_polling--;
	splx(s);
}

void
pxaohci_enable(struct pxaohci_softc *sc)
{
	u_int32_t			hr;

	/* Full host reset */
	hr = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USBHC_HR);
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USBHC_HR,
	    (hr & USBHC_HR_MASK) | USBHC_HR_FHR);

	DELAY(USBHC_RST_WAIT);

	hr = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USBHC_HR);
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USBHC_HR,
	    (hr & USBHC_HR_MASK) & ~(USBHC_HR_FHR));

	/* Force system bus interface reset */
	hr = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USBHC_HR);
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USBHC_HR,
	    (hr & USBHC_HR_MASK) | USBHC_HR_FSBIR);

	while (bus_space_read_4(sc->sc.iot, sc->sc.ioh, USBHC_HR) & \
	    USBHC_HR_FSBIR)
		DELAY(3);

	/* Enable the ports (physically only one, only enable that one?) */
	hr = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USBHC_HR);
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USBHC_HR,
	    (hr & USBHC_HR_MASK) & ~(USBHC_HR_SSE));
	hr = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USBHC_HR);
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USBHC_HR,
	    (hr & USBHC_HR_MASK) & ~(USBHC_HR_SSEP2));
}

void
pxaohci_disable(struct pxaohci_softc *sc)
{
	u_int32_t			hr;

	/* Full host reset */
	hr = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USBHC_HR);
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USBHC_HR,
	    (hr & USBHC_HR_MASK) | USBHC_HR_FHR);

	DELAY(USBHC_RST_WAIT);

	hr = bus_space_read_4(sc->sc.iot, sc->sc.ioh, USBHC_HR);
	bus_space_write_4(sc->sc.iot, sc->sc.ioh, USBHC_HR,
	    (hr & USBHC_HR_MASK) & ~(USBHC_HR_FHR));
}
