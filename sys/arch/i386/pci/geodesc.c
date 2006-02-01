/*	$OpenBSD: geodesc.c,v 1.5 2006/02/01 13:08:12 mickey Exp $	*/

/*
 * Copyright (c) 2003 Markus Friedl <markus@openbsd.org>
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
 * Geode SC1100 Information Appliance On a Chip
 * http://www.national.com/ds.cgi/SC/SC1100.pdf
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#ifdef __HAVE_TIMECOUNTER
#include <sys/timetc.h>
#endif

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <arch/i386/pci/geodescreg.h>

struct geodesc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

int	geodesc_match(struct device *, void *, void *);
void	geodesc_attach(struct device *, struct device *, void *);
int	geodesc_wdogctl_cb(void *, int);

struct cfattach geodesc_ca = {
	sizeof(struct geodesc_softc), geodesc_match, geodesc_attach
};

struct cfdriver geodesc_cd = {
	NULL, "geodesc", DV_DULL
};

#ifdef __HAVE_TIMECOUNTER
u_int   geodesc_get_timecount(struct timecounter *tc);

struct timecounter geodesc_timecounter = {
	geodesc_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffffff,		/* counter_mask */
	27000000,		/* frequency */
	"GEOTSC",		/* name */
	2000			/* quality */
};
#endif	/* __HAVE_TIMECOUNTER */

int
geodesc_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NS &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NS_SC1100_XBUS ||
	     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NS_SCx200_XBUS))
		return (1);
	return (0);
}

#define WDSTSBITS "\20\x04WDRST\x03WDSMI\x02WDINT\x01WDOVF"

void
geodesc_attach(struct device *parent, struct device *self, void *aux)
{
	struct geodesc_softc *sc = (void *) self;
	struct pci_attach_args *pa = aux;
	uint16_t cnfg, cba;
	uint8_t sts, rev, iid;
	pcireg_t reg;

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, SC1100_F5_SCRATCHPAD);
	sc->sc_iot = pa->pa_iot;
	if (bus_space_map(sc->sc_iot, reg, 64, 0, &sc->sc_ioh)) {
		printf(": unable to map registers at 0x%x\n", reg);
		return;
	}
	cba = bus_space_read_2(sc->sc_iot, sc->sc_ioh, GCB_CBA);
	if (cba != reg) {
		printf(": cba mismatch: cba 0x%x != reg 0x%x\n", cba, reg);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, 64);
		return;
	}
	sts = bus_space_read_1(sc->sc_iot, sc->sc_ioh, GCB_WDSTS);
	cnfg = bus_space_read_2(sc->sc_iot, sc->sc_ioh, GCB_WDCNFG);
	iid = bus_space_read_1(sc->sc_iot, sc->sc_ioh, GCB_IID);
	rev = bus_space_read_1(sc->sc_iot, sc->sc_ioh, GCB_REV);

	printf(": iid %d revision %d wdstatus %b\n", iid, rev, sts, WDSTSBITS);

	/* setup and register watchdog */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, GCB_WDTO, 0);
	sts |= WDOVF_CLEAR;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, GCB_WDSTS, sts);
	cnfg &= ~WDCNFG_MASK;
	cnfg |= WDTYPE1_RESET|WDPRES_DIV_512;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, GCB_WDCNFG, cnfg);

	wdog_register(sc, geodesc_wdogctl_cb);
#ifdef __HAVE_TIMECOUNTER
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GCB_TSCNFG, TSC_ENABLE);
	/* Hook into the kern_tc */
	geodesc_timecounter.tc_priv = sc;
	tc_init(&geodesc_timecounter);  
#endif /* __HAVE_TIMECOUNTER */
}

int
geodesc_wdogctl_cb(void *self, int period)
{
	struct geodesc_softc *sc = self;

	if (period > 0x03ff)
		period = 0x03ff;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, GCB_WDTO, period * 64);
	return (period);
}

#ifdef __HAVE_TIMECOUNTER
u_int
geodesc_get_timecount(struct timecounter *tc)
{
	struct geodesc_softc *sc = tc->tc_priv;

	return (bus_space_read_4(sc->sc_iot, sc->sc_ioh, GCB_TSC));
}
#endif /* __HAVE_TIMECOUNTER */
