/*	$OpenBSD: ichwdt.c,v 1.1 2005/05/02 17:26:00 grange Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
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

/*
 * Intel 6300ESB ICH watchdog timer driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/ichreg.h>

#define ICHWDT_DEBUG

#ifdef ICHWDT_DEBUG
#define DPRINTF(fmt, args...) printf(fmt, ##args)
#else
#define DPRINTF(fmt, args...)
#endif

struct ichwdt_softc {
	struct device sc_dev;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_tag;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	int sc_enabled;
	int sc_divisor;
};

int	ichwdt_match(struct device *, void *, void *);
void	ichwdt_attach(struct device *, struct device *, void *);

int	ichwdt_cb(void *, int);

struct cfattach ichwdt_ca = {
	sizeof(struct ichwdt_softc),
	ichwdt_match,
	ichwdt_attach
};

struct cfdriver ichwdt_cd = {
	NULL, "ichwdt", DV_DULL
};

const struct pci_matchid ichwdt_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_6300ESB_WDT }
};

static __inline void
ichwdt_unlock_write(struct ichwdt_softc *sc, int reg, u_int32_t val)
{
	/* Register unlocking sequence */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ICH_WDT_RELOAD, 0x80);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, ICH_WDT_RELOAD, 0x86);

	/* Now it's possible to write to the register */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, reg, val);
}

int
ichwdt_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, ichwdt_devices,
	    sizeof(ichwdt_devices) / sizeof(ichwdt_devices[0])));
}

void
ichwdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct ichwdt_softc *sc = (struct ichwdt_softc *)self;
	struct pci_attach_args *pa = aux;
	u_int32_t reg;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;

	/* Map memory space */
	sc->sc_iot = pa->pa_iot;
	if (pci_mapreg_map(pa, ICH_WDT_BASE, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, NULL, 0)) {
		printf(": failed to map memory space\n");
		return;
	}

	reg = pci_conf_read(sc->sc_pc, sc->sc_tag, ICH_WDT_CONF);
	DPRINTF(": conf 0x%x", reg);

	/* Disable interrupts for now */
	reg &= ~ICH_WDT_CONF_INT_MASK;
	reg |= ICH_WDT_CONF_INT_DIS;

	sc->sc_divisor = (reg & ICH_WDT_CONF_PRE ? 2 ^ 5 : 2 ^ 15);

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, ICH_WDT_RELOAD);
	if (reg & ICH_WDT_RELOAD_TIMEOUT) {
		printf(": reboot on timeout");
		ichwdt_unlock_write(sc, ICH_WDT_RELOAD, reg);
	}

	printf("\n");

	/* Register new watchdog */
	wdog_register(sc, ichwdt_cb);
}

int
ichwdt_cb(void *arg, int period)
{
	struct ichwdt_softc *sc = arg;
	pcireg_t reg;

	if (period == 0) {
		/* Disable watchdog timer */
		reg = pci_conf_read(sc->sc_pc, sc->sc_tag, ICH_WDT_LOCK);
		reg &= ~ICH_WDT_LOCK_ENABLED;
		pci_conf_write(sc->sc_pc, sc->sc_tag, ICH_WDT_LOCK, reg);
	} else {
		/* Reset watchdog timer */
		ichwdt_unlock_write(sc, ICH_WDT_PRE1, 1);
		ichwdt_unlock_write(sc, ICH_WDT_PRE2, 1);

		reg = pci_conf_read(sc->sc_pc, sc->sc_tag, ICH_WDT_LOCK);
		reg &= ~ICH_WDT_LOCK_FREERUN;
		reg |= ICH_WDT_LOCK_ENABLED;
		pci_conf_write(sc->sc_pc, sc->sc_tag, ICH_WDT_LOCK, reg);
	}

	return (period);
}
