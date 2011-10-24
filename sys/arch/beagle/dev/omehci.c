/*	$OpenBSD: omehci.c,v 1.7 2011/10/24 22:49:07 drahn Exp $ */

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
#include <sys/rwlock.h>
#include <sys/timeout.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <arch/beagle/beagle/ahb.h>
#include <beagle/dev/prcmvar.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#define EHCI_HCPCAPBASE 0x48064800

int	omehci_match(struct device *, void *, void *);
void	omehci_attach(struct device *, struct device *, void *);
int	omehci_detach(struct device *, int);
int	omehci_activate(struct device *, int);

struct omehci_softc {
	ehci_softc_t	sc;
	void		*sc_ih;
};

void	omehci_enable(struct omehci_softc *);
void	omehci_disable(struct omehci_softc *);

struct cfattach omehci_ca = {
        sizeof (struct omehci_softc), omehci_match, omehci_attach,
	omehci_detach, omehci_activate
};

int
omehci_match(struct device *parent, void *match, void *aux)
{
	struct ahb_attach_args	*aa = aux;

	switch (board_id) {
	case BOARD_ID_OMAP3_BEAGLE:
		break; /* continue trying */
	case BOARD_ID_OMAP4_PANDA:
		return 0; /* not ported yet ??? - different */
	default:
		return 0; /* unknown */
	}
	if (aa->aa_addr != EHCI_HCPCAPBASE)
		return 0;

	return (1);
}

void
omehci_attach(struct device *parent, struct device *self, void *aux)
{
	struct omehci_softc	*sc = (struct omehci_softc *)self;
        struct ahb_attach_args	*aa = aux;
	usbd_status		r;
	char *devname = sc->sc.sc_bus.bdev.dv_xname;


	sc->sc.iot = aa->aa_iot;
	sc->sc.sc_bus.dmatag = aa->aa_dmat;
	sc->sc.sc_size = 0;

	/* Map I/O space */
	if (bus_space_map(sc->sc.iot, aa->aa_addr, aa->aa_size, 0,
	    &sc->sc.ioh)) {
		printf(": cannot map mem space\n");
		return;
	}
	sc->sc.sc_size = aa->aa_size;

	/* XXX copied from ohci_pci.c. needed? */
	bus_space_barrier(sc->sc.iot, sc->sc.ioh, 0, sc->sc.sc_size,
	    BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE);

	prcm_enableclock(PRCM_CLK_EN_USB);

#if 0
	omehci_enable(sc);
#endif

	/* Disable interrupts, so we don't get any spurious ones. */
	sc->sc.sc_offs = EREAD1(&sc->sc, EHCI_CAPLENGTH);
	EOWRITE2(&sc->sc, EHCI_USBINTR, 0);

	sc->sc_ih = arm_intr_establish(aa->aa_intr, IPL_USB,
	    ehci_intr, &sc->sc, devname);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
#if 0
		omehci_disable(sc);
#endif

#if 0
		prcm_disableclock(PRCM_CLK_EN_USB);
#endif

		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
		return;
	}

	strlcpy(sc->sc.sc_vendor, "OMAP3[45]xx", sizeof(sc->sc.sc_vendor));
	r = ehci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n", devname);

		arm_intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;

		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);

#if 0
		prcm_disableclock(PRCM_CLK_EN_USB);
#endif
		sc->sc.sc_size = 0;
		return;
	}

	sc->sc.sc_shutdownhook = shutdownhook_establish(ehci_shutdown, &sc->sc);

	sc->sc.sc_child = config_found((void *)sc, &sc->sc.sc_bus,
	    usbctlprint);
}

int
omehci_detach(struct device *self, int flags)
{
	struct omehci_softc		*sc = (struct omehci_softc *)self;
	int				rv;

	rv = ehci_detach(&sc->sc, flags);
	if (rv)
		return (rv);

	if (sc->sc_ih != NULL) {
		arm_intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (sc->sc.sc_size) {
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}

	/* stop clock */
#if 0
	prcm_disableclock(PRCM_CLK_EN_USB);
#endif

	return (0);
}

int
omehci_activate(struct device *self, int act)
{
	struct omehci_softc *sc = (struct omehci_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		sc->sc.sc_bus.use_polling++;
#if 0
		ohci_activate(&sc->sc, act);
		prcm_disableclock(PRCM_CLK_EN_USB);
#endif
		sc->sc.sc_bus.use_polling--;
		break;
	case DVACT_RESUME:
		sc->sc.sc_bus.use_polling++;
		prcm_enableclock(PRCM_CLK_EN_USB);
#if 0
		omehci_enable(sc);
		ohci_activate(&sc->sc, act);
#endif
		sc->sc.sc_bus.use_polling--;
		break;
	}
	return 0;
}

#if 0
void
omehci_enable(struct omehci_softc *sc)
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
omehci_disable(struct omehci_softc *sc)
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
#endif
