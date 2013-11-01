/*	$OpenBSD	*/
/*
 * Copyright (c) 2013 Patrick Wildt <patrick@blueri.se>
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
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <dev/ic/ahcivar.h>
#include <dev/ic/ahcireg.h>

#include <armv7/sunxi/sunxivar.h>
#include <armv7/sunxi/sunxireg.h>
#include <armv7/sunxi/sxiccmuvar.h>
#include <armv7/sunxi/sxipiovar.h>

#define	SXIAHCI_CAP	0x0000
#define	SXIAHCI_GHC	0x0004
#define	SXIAHCI_PI	0x000c
#define	SXIAHCI_PHYCS0	0x00c0
#define	SXIAHCI_PHYCS1	0x00c4
#define	SXIAHCI_PHYCS2	0x00c8
#define	SXIAHCI_TIMER1MS	0x00e0
#define	SXIAHCI_RWC	0x00fc
#define	SXIAHCI_TIMEOUT	0x100000
#define SXIAHCI_PWRPIN	40

void	sxiahci_attach(struct device *, struct device *, void *);
int	sxiahci_detach(struct device *, int);
int	sxiahci_activate(struct device *, int);

extern int ahci_intr(void *);
extern u_int32_t ahci_read(struct ahci_softc *, bus_size_t);
extern void ahci_write(struct ahci_softc *, bus_size_t, u_int32_t);

struct sxiahci_softc {
	struct ahci_softc	sc;

};

struct cfattach sxiahci_ca = {
	sizeof(struct sxiahci_softc),
	NULL,
	sxiahci_attach,
	sxiahci_detach,
	sxiahci_activate
};

struct cfdriver sxiahci_cd = {
	NULL, "ahci", DV_DULL
};

void
sxiahci_attach(struct device *parent, struct device *self, void *args)
{
	struct sxi_attach_args *sxi = args;
	struct sxiahci_softc *sxisc = (struct sxiahci_softc *)self;
	struct ahci_softc *sc = &sxisc->sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint32_t timo;

	sc->sc_iot = iot = sxi->sxi_iot;
	sc->sc_ios = sxi->sxi_dev->mem[0].size;
	sc->sc_dmat = sxi->sxi_dmat;

	if (bus_space_map(sc->sc_iot, sxi->sxi_dev->mem[0].addr,
	    sxi->sxi_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("sxiahci_attach: bus_space_map failed!");
	ioh = sc->sc_ioh;

	/* enable clock */
	sxiccmu_enablemodule(CCMU_AHCI);
	delay(5000);

	/* XXX setup magix */
	SXIWRITE4(sc, SXIAHCI_RWC, 0);
	delay(10);

	SXISET4(sc, SXIAHCI_PHYCS1, 1 << 19);
	delay(10);

	SXICMS4(sc, SXIAHCI_PHYCS0, 1 << 25,
	    1 << 23 | 1 << 24 | 1 << 18 | 1 << 26);
	delay(10);

	SXICMS4(sc, SXIAHCI_PHYCS1,
	    1 << 16 | 1 << 12 | 1 << 11 | 1 << 8 | 1 << 6,
	    1 << 17 | 1 << 10 | 1 << 9 | 1 << 7);
	delay(10);

	SXISET4(sc, SXIAHCI_PHYCS1, 1 << 28 | 1 << 15); 
	delay(10);

	SXICLR4(sc, SXIAHCI_PHYCS1, 1 << 19); 
	delay(10);

	SXICMS4(sc, SXIAHCI_PHYCS0, 1 << 21 | 1 << 20, 1 << 22);
	delay(10);

	SXICMS4(sc, SXIAHCI_PHYCS2, 1 << 7 | 1 << 6,
	    1 << 9 | 1 << 8 | 1 << 5);
	delay(5000);

	SXISET4(sc, SXIAHCI_PHYCS0, 1 << 19);
	delay(20);

	timo = SXIAHCI_TIMEOUT;
	while ((SXIREAD4(sc, SXIAHCI_PHYCS0) >> 28 & 3) != 2 && --timo)
		delay(10);
	if (!timo) {
		printf(": AHCI phy power up failed.\n");
		goto dismod;
	}

	SXISET4(sc, SXIAHCI_PHYCS2, 1 << 24);

	timo = SXIAHCI_TIMEOUT;
	while ((SXIREAD4(sc, SXIAHCI_PHYCS2) & (1 << 24)) && --timo)
		delay(10);
	if (!timo) {
		printf(": AHCI phy calibration failed.\n");
		goto dismod;
	}

	delay(15000);
	SXIWRITE4(sc, SXIAHCI_RWC, 7);

	/* power up phy */
	sxipio_setcfg(SXIAHCI_PWRPIN, SXIPIO_OUTPUT);
	sxipio_setpin(SXIAHCI_PWRPIN);

	sc->sc_ih = arm_intr_establish(sxi->sxi_dev->irq[0], IPL_BIO,
	    ahci_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		goto clrpwr;
	}

	SXIWRITE4(sc, SXIAHCI_PI, 1);
	SXICLR4(sc, SXIAHCI_CAP, AHCI_REG_CAP_SPM);
	sc->sc_flags |= AHCI_F_NO_PMP; /* XXX enough? */
	if (ahci_attach(sc) != 0) {
		/* error printed by ahci_attach */
		goto irq;
	}

	return;
irq:
	arm_intr_disestablish(sc->sc_ih);
clrpwr:
	sxipio_clrpin(SXIAHCI_PWRPIN);
dismod:
	sxiccmu_disablemodule(CCMU_AHCI);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
}

int
sxiahci_detach(struct device *self, int flags)
{
	struct sxiahci_softc *sxisc = (struct sxiahci_softc *) self;
	struct ahci_softc *sc = &sxisc->sc;

	ahci_detach(sc, flags);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	return 0;
}

int
sxiahci_activate(struct device *self, int act)
{
	struct sxiahci_softc *sxisc = (struct sxiahci_softc *) self;
	struct ahci_softc *sc = &sxisc->sc;

	return ahci_activate((struct device *)sc, act);
}
