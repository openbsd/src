/*	$OpenBSD: ichpcib.c,v 1.3 2004/06/06 17:34:37 grange Exp $	*/
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
 * Special driver for the Intel ICHx/ICHx-M LPC bridges that attaches
 * instead of pcib(4). In addition to the core pcib(4) functionality this
 * driver provides support for the Intel SpeedStep technology.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/ichreg.h>

struct ichpcib_softc {
	struct device sc_dev;

	bus_space_tag_t sc_pm_iot;
	bus_space_handle_t sc_pm_ioh;
};

int	ichpcib_match(struct device *, void *, void *);
void	ichpcib_attach(struct device *, struct device *, void *);

int	ichss_present(struct pci_attach_args *);
int	ichss_setperf(int);

/* arch/i386/pci/pcib.c */
void    pcibattach(struct device *, struct device *, void *);

struct cfattach ichpcib_ca = {
	sizeof(struct ichpcib_softc),
	ichpcib_match,
	ichpcib_attach
};

struct cfdriver ichpcib_cd = {
	NULL, "ichpcib", DV_DULL
};

#ifndef SMALL_KERNEL
static void *ichss_cookie;	/* XXX */
extern int setperf_prio;
#endif	/* !SMALL_KERNEL */

int
ichpcib_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_BRIDGE ||
	    PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_BRIDGE_ISA) {
		return (0);
	}

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL) {
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_INTEL_82801AA_LPC:	/* ICH */
		case PCI_PRODUCT_INTEL_82801AB_LPC:	/* ICH0 */
		case PCI_PRODUCT_INTEL_82801BA_LPC:	/* ICH2 */
		case PCI_PRODUCT_INTEL_82801BAM_LPC:	/* ICH2-M */
		case PCI_PRODUCT_INTEL_82801CA_LPC:	/* ICH3-S */
		case PCI_PRODUCT_INTEL_82801CAM_LPC:	/* ICH3-M */
		case PCI_PRODUCT_INTEL_82801DB_LPC:	/* ICH4 */
		case PCI_PRODUCT_INTEL_82801DBM_LPC:	/* ICH4-M */
		case PCI_PRODUCT_INTEL_82801EB_LPC:	/* ICH5 */
			return (2);	/* supersede pcib(4) */
		}
	}

	return (0);
}

void
ichpcib_attach(struct device *parent, struct device *self, void *aux)
{
#ifndef SMALL_KERNEL
	struct ichpcib_softc *sc = (struct ichpcib_softc *)self;
	struct pci_attach_args *pa = aux;
	pcireg_t pmbase;

	/* Map power management I/O space */
	sc->sc_pm_iot = pa->pa_iot;
	pmbase = pci_conf_read(pa->pa_pc, pa->pa_tag, ICH_PMBASE);
	if (bus_space_map(sc->sc_pm_iot, PCI_MAPREG_IO_ADDR(pmbase),
	    ICH_PMSIZE, 0, &sc->sc_pm_ioh) != 0) {
		printf(": failed to map I/O space");
		goto corepcib;
	}

	/* Check for SpeedStep */
	if (ichss_present(pa)) {
		printf(": SpeedStep");

		/* Enable SpeedStep */
		pci_conf_write(pa->pa_pc, pa->pa_tag, ICH_GEN_PMCON1,
		    pci_conf_read(pa->pa_pc, pa->pa_tag, ICH_GEN_PMCON1) |
		    ICH_GEN_PMCON1_SS_EN);

		/* Hook into hw.setperf sysctl */
		ichss_cookie = sc;
		cpu_setperf = ichss_setperf;
		setperf_prio = 2;
	}

corepcib:
#endif	/* !SMALL_KERNEL */
	/* Provide core pcib(4) functionality */
	pcibattach(parent, self, aux);
}

#ifndef SMALL_KERNEL
int
ichss_present(struct pci_attach_args *pa)
{
	pcitag_t br_tag;
	pcireg_t br_id, br_class;

	if (setperf_prio > 2)
		return (0);

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801DBM_LPC ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801CAM_LPC)
		return (1);
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82801BAM_LPC) {
		/*
		 * Old revisions of the 82815 hostbridge found on
		 * Dell Inspirons 8000 and 8100 don't support
		 * SpeedStep.
		 */
		/*
		 * XXX: dev 0 func 0 is not always a hostbridge,
		 * should be converted to use pchb(4) hook.
		 */
		br_tag = pci_make_tag(pa->pa_pc, pa->pa_bus, 0, 0);
		br_id = pci_conf_read(pa->pa_pc, br_tag, PCI_ID_REG);
		br_class = pci_conf_read(pa->pa_pc, br_tag, PCI_CLASS_REG);

		if (PCI_PRODUCT(br_id) == PCI_PRODUCT_INTEL_82815_FULL_HUB &&
		    PCI_REVISION(br_class) < 5)
			return (0);
		return (1);
	}

	return (0);
}

int
ichss_setperf(int level)
{
	struct ichpcib_softc *sc = ichss_cookie;
	u_int8_t state, ostate, cntl;
	int s;

#ifdef DIAGNOSTIC
	if (sc == NULL) {
		printf("%s: no cookie", __func__);
		return (EFAULT);
	}
#endif

	s = splhigh();
	state = bus_space_read_1(sc->sc_pm_iot, sc->sc_pm_ioh, ICH_PM_SS_CNTL);
	ostate = state;

	/* Only two states are available */
	if (level <= 50)
		state |= ICH_PM_SS_STATE_LOW;
	else
		state &= ~ICH_PM_SS_STATE_LOW;

	/*
	 * An Intel SpeedStep technology transition _always_ occur on
	 * writes to the ICH_PM_SS_CNTL register, even if the value
	 * written is the same as the previous value. So do the write
	 * only if the state has changed.
	 */
	if (state != ostate) {
		/* Disable bus mastering arbitration */
		cntl = bus_space_read_1(sc->sc_pm_iot, sc->sc_pm_ioh,
		    ICH_PM_CNTL);
		bus_space_write_1(sc->sc_pm_iot, sc->sc_pm_ioh, ICH_PM_CNTL,
		    cntl | ICH_PM_ARB_DIS);

		/* Do the transition */
		bus_space_write_1(sc->sc_pm_iot, sc->sc_pm_ioh, ICH_PM_SS_CNTL,
		    state);

		/* Restore bus mastering arbitration state */
		bus_space_write_1(sc->sc_pm_iot, sc->sc_pm_ioh, ICH_PM_CNTL,
		    cntl);

		if (update_cpuspeed != NULL)
			update_cpuspeed();
	}
	splx(s);

	return (0);
}
#endif	/* !SMALL_KERNEL */
