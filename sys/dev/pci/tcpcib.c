/*	$OpenBSD: tcpcib.c,v 1.1 2012/05/27 12:24:33 jsg Exp $	*/

/*
 * Copyright (c) 2012 Matt Dainty <matt@bodgit-n-scarper.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Intel Atom E600 series LPC bridge also containing watchdog
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define	E600_LPC_SMBA		0x40		/* SMBus Base Address */
#define	E600_LPC_GBA		0x44		/* GPIO Base Address */
#define	E600_LPC_WDTBA		0x84		/* WDT Base Address */

#define	E600_WDT_SIZE		64		/* I/O region size */
#define	E600_WDT_PV1		0x00		/* Preload Value 1 Register */
#define	E600_WDT_PV2		0x04		/* Preload Value 2 Register */
#define	E600_WDT_RR0		0x0c		/* Reload Register 0 */
#define	E600_WDT_RR1		0x0d		/* Reload Register 1 */
#define	E600_WDT_RR1_RELOAD	(1 << 0)	/* WDT Reload Flag */
#define	E600_WDT_RR1_TIMEOUT	(1 << 1)	/* WDT Timeout Flag */
#define	E600_WDT_WDTCR		0x10		/* WDT Configuration Register */
#define	E600_WDT_WDTCR_PRE	(1 << 2)	/* WDT Prescalar Select */
#define	E600_WDT_WDTCR_RESET	(1 << 3)	/* WDT Reset Select */
#define	E600_WDT_WDTCR_ENABLE	(1 << 4)	/* WDT Reset Enable */
#define	E600_WDT_WDTCR_TIMEOUT	(1 << 5)	/* WDT Timeout Output Enable */
#define	E600_WDT_DCR		0x14		/* Down Counter Register */
#define	E600_WDT_WDTLR		0x18		/* WDT Lock Register */
#define	E600_WDT_WDTLR_LOCK	(1 << 0)	/* Watchdog Timer Lock */
#define	E600_WDT_WDTLR_ENABLE	(1 << 1)	/* Watchdog Timer Enable */
#define	E600_WDT_WDTLR_TIMEOUT	(1 << 2)	/* WDT Timeout Configuration */

struct tcpcib_softc {
	struct device sc_dev;

	/* Keep track of which parts of the hardware are active */
	int sc_active;
#define	E600_WDT_ACTIVE		(1 << 0)

	/* Watchdog interface */
	bus_space_tag_t sc_wdt_iot;
	bus_space_handle_t sc_wdt_ioh;

	int sc_wdt_period;
};

struct cfdriver tcpcib_cd = {
	NULL, "tcpcib", DV_DULL
};

int	 tcpcib_match(struct device *, void *, void *);
void	 tcpcib_attach(struct device *, struct device *, void *);
int	 tcpcib_activate(struct device *, int);

int	 tcpcib_wdt_cb(void *, int);
void	 tcpcib_wdt_init(struct tcpcib_softc *, int);
void	 tcpcib_wdt_start(struct tcpcib_softc *);
void	 tcpcib_wdt_stop(struct tcpcib_softc *);

struct cfattach tcpcib_ca = {
	sizeof(struct tcpcib_softc), tcpcib_match, tcpcib_attach,
	NULL, tcpcib_activate
};

/* from arch/<*>/pci/pcib.c */
void	pcibattach(struct device *parent, struct device *self, void *aux);

const struct pci_matchid tcpcib_devices[] = {
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_E600_LPC }
};

static __inline void
tcpcib_wdt_unlock(struct tcpcib_softc *sc)
{
	/* Register unlocking sequence */
	bus_space_write_1(sc->sc_wdt_iot, sc->sc_wdt_ioh, E600_WDT_RR0, 0x80);
	bus_space_write_1(sc->sc_wdt_iot, sc->sc_wdt_ioh, E600_WDT_RR0, 0x86);
}

void
tcpcib_wdt_init(struct tcpcib_softc *sc, int period)
{
	u_int32_t preload;

	/* Set new timeout */
	preload = (period * 33000000) >> 15;
	preload--;

	/*
	 * Set watchdog to perform a cold reset toggling the GPIO pin and the
	 * prescaler set to 1ms-10m resolution
	 */
	tcpcib_wdt_unlock(sc);
	bus_space_write_1(sc->sc_wdt_iot, sc->sc_wdt_ioh, E600_WDT_WDTCR,
	    E600_WDT_WDTCR_ENABLE);
	tcpcib_wdt_unlock(sc);
	bus_space_write_4(sc->sc_wdt_iot, sc->sc_wdt_ioh, E600_WDT_PV1, 0);
	tcpcib_wdt_unlock(sc);
	bus_space_write_4(sc->sc_wdt_iot, sc->sc_wdt_ioh, E600_WDT_PV2,
	    preload);
	tcpcib_wdt_unlock(sc);
	bus_space_write_1(sc->sc_wdt_iot, sc->sc_wdt_ioh, E600_WDT_RR1,
	    E600_WDT_RR1_RELOAD);
}

void
tcpcib_wdt_start(struct tcpcib_softc *sc)
{
	/* Enable watchdog */
	tcpcib_wdt_unlock(sc);
	bus_space_write_1(sc->sc_wdt_iot, sc->sc_wdt_ioh, E600_WDT_WDTLR,
	    E600_WDT_WDTLR_ENABLE);
}

void
tcpcib_wdt_stop(struct tcpcib_softc *sc)
{
	/* Disable watchdog, with a reload before for safety */
	tcpcib_wdt_unlock(sc);
	bus_space_write_1(sc->sc_wdt_iot, sc->sc_wdt_ioh, E600_WDT_RR1,
	    E600_WDT_RR1_RELOAD);
	tcpcib_wdt_unlock(sc);
	bus_space_write_1(sc->sc_wdt_iot, sc->sc_wdt_ioh, E600_WDT_WDTLR, 0);
}

int
tcpcib_match(struct device *parent, void *match, void *aux)
{
	if (pci_matchbyid((struct pci_attach_args *)aux, tcpcib_devices,
	    sizeof(tcpcib_devices) / sizeof(tcpcib_devices[0])))
		return (2);

	return (0);
}

void
tcpcib_attach(struct device *parent, struct device *self, void *aux)
{
	struct tcpcib_softc *sc = (struct tcpcib_softc *)self;
	struct pci_attach_args *pa = aux;
	u_int32_t reg, wdtbase;

	sc->sc_active = 0;

	/* Map Watchdog I/O space */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, E600_LPC_WDTBA);
	wdtbase = reg & 0xffff;
	sc->sc_wdt_iot = pa->pa_iot;
	if (reg & (1 << 31) && wdtbase) {
		if (PCI_MAPREG_IO_ADDR(wdtbase) == 0 ||
		    bus_space_map(sc->sc_wdt_iot, PCI_MAPREG_IO_ADDR(wdtbase),
		    E600_WDT_SIZE, 0, &sc->sc_wdt_ioh)) {
			printf(": can't map watchdog I/O space");
			goto corepcib;
		}
		printf(": watchdog");

		/* Check for reboot on timeout */
		reg = bus_space_read_1(sc->sc_wdt_iot, sc->sc_wdt_ioh,
		    E600_WDT_RR1);
		if (reg & E600_WDT_RR1_TIMEOUT) {
			printf(", reboot on timeout");

			/* Clear timeout bit */
			tcpcib_wdt_unlock(sc);
			bus_space_write_1(sc->sc_wdt_iot, sc->sc_wdt_ioh,
			    E600_WDT_RR1, E600_WDT_RR1_TIMEOUT);
		}

		/* Check it's not locked already */
		reg = bus_space_read_1(sc->sc_wdt_iot, sc->sc_wdt_ioh,
		    E600_WDT_WDTLR);
		if (reg & E600_WDT_WDTLR_LOCK) {
			printf(", locked");
			goto corepcib;
		}

		/* Disable watchdog */
		tcpcib_wdt_stop(sc);
		sc->sc_wdt_period = 0;

		sc->sc_active |= E600_WDT_ACTIVE;

		/* Register new watchdog */
		wdog_register(sc, tcpcib_wdt_cb);
	}

corepcib:
	/* Provide core pcib(4) functionality */
	pcibattach(parent, self, aux);
}

int
tcpcib_activate(struct device *self, int act)
{
	struct tcpcib_softc *sc = (struct tcpcib_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		/* Watchdog is running, disable it */
		if (sc->sc_active & E600_WDT_ACTIVE && sc->sc_wdt_period != 0)
			tcpcib_wdt_stop(sc);
		break;
	case DVACT_RESUME:
		if (sc->sc_active & E600_WDT_ACTIVE) {
			/*
			 * Watchdog was running prior to suspend so reenable
			 * it, otherwise make sure it stays disabled
			 */
			if (sc->sc_wdt_period != 0) {
				tcpcib_wdt_init(sc, sc->sc_wdt_period);
				tcpcib_wdt_start(sc);
			} else
				tcpcib_wdt_stop(sc);
		}
		break;
	}
	return (0);
}

int
tcpcib_wdt_cb(void *arg, int period)
{
	struct tcpcib_softc *sc = arg;

	if (period == 0) {
		if (sc->sc_wdt_period != 0)
			tcpcib_wdt_stop(sc);
	} else {
		/* 600 seconds is the maximum supported timeout value */
		if (period > 600)
			period = 600;
		if (sc->sc_wdt_period != period)
			tcpcib_wdt_init(sc, period);
		if (sc->sc_wdt_period == 0) {
			tcpcib_wdt_start(sc);
		} else {
			/* Reset timer */
			tcpcib_wdt_unlock(sc);
			bus_space_write_1(sc->sc_wdt_iot, sc->sc_wdt_ioh,
			    E600_WDT_RR1, E600_WDT_RR1_RELOAD);
		}
	}
	sc->sc_wdt_period = period;

	return (period);
}
