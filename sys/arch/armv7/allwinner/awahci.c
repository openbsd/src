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

#include <armv7/allwinner/allwinnervar.h>
#include <armv7/allwinner/allwinnerreg.h>
#include <armv7/allwinner/awccmuvar.h>
#include <armv7/allwinner/awpiovar.h>

#define	AWAHCI_CAP	0x0000
#define	AWAHCI_GHC	0x0004
#define	AWAHCI_PI	0x000c
#define	AWAHCI_PHYCS0	0x00c0
#define	AWAHCI_PHYCS1	0x00c4
#define	AWAHCI_PHYCS2	0x00c8
#define	AWAHCI_TIMER1MS	0x00e0
#define	AWAHCI_RWC	0x00fc
#define	AWAHCI_TIMEOUT	0x100000
#define AWAHCI_PWRPIN	40

void	awahci_attach(struct device *, struct device *, void *);
int	awahci_detach(struct device *, int);
int	awahci_activate(struct device *, int);

extern int ahci_intr(void *);
extern u_int32_t ahci_read(struct ahci_softc *, bus_size_t);
extern void ahci_write(struct ahci_softc *, bus_size_t, u_int32_t);

struct awahci_softc {
	struct ahci_softc	sc;

};

struct cfattach awahci_ca = {
	sizeof(struct awahci_softc),
	NULL,
	awahci_attach,
	awahci_detach,
	awahci_activate
};

struct cfdriver awahci_cd = {
	NULL, "ahci", DV_DULL
};

void
awahci_attach(struct device *parent, struct device *self, void *args)
{
	struct aw_attach_args *aw = args;
	struct awahci_softc *awsc = (struct awahci_softc *)self;
	struct ahci_softc *sc = &awsc->sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	uint32_t timo;

	sc->sc_iot = iot = aw->aw_iot;
	sc->sc_ios = aw->aw_dev->mem[0].size;
	sc->sc_dmat = aw->aw_dmat;

	if (bus_space_map(sc->sc_iot, aw->aw_dev->mem[0].addr,
	    aw->aw_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("awahci_attach: bus_space_map failed!");
	ioh = sc->sc_ioh;

	/* enable clock */
	awccmu_enablemodule(CCMU_AHCI);
	delay(5000);

	/* XXX setup magix */
	AWWRITE4(sc, AWAHCI_RWC, 0);
	delay(10);

	AWSET4(sc, AWAHCI_PHYCS1, 1 << 19);
	delay(10);

	AWCMS4(sc, AWAHCI_PHYCS0, 1 << 25,
	    1 << 23 | 1 << 24 | 1 << 18 | 1 << 26);
	delay(10);

	AWCMS4(sc, AWAHCI_PHYCS1,
	    1 << 16 | 1 << 12 | 1 << 11 | 1 << 8 | 1 << 6,
	    1 << 17 | 1 << 10 | 1 << 9 | 1 << 7);
	delay(10);

	AWSET4(sc, AWAHCI_PHYCS1, 1 << 28 | 1 << 15); 
	delay(10);

	AWCLR4(sc, AWAHCI_PHYCS1, 1 << 19); 
	delay(10);

	AWCMS4(sc, AWAHCI_PHYCS0, 1 << 21 | 1 << 20, 1 << 22);
	delay(10);

	AWCMS4(sc, AWAHCI_PHYCS2, 1 << 7 | 1 << 6,
	    1 << 9 | 1 << 8 | 1 << 5);
	delay(5000);

	AWSET4(sc, AWAHCI_PHYCS0, 1 << 19);
	delay(20);

	timo = AWAHCI_TIMEOUT;
	while ((AWREAD4(sc, AWAHCI_PHYCS0) >> 28 & 3) != 2 && --timo)
		delay(10);
	if (!timo)
		printf("awahci_attach: AHCI phy power up failed.\n");

	AWSET4(sc, AWAHCI_PHYCS2, 1 << 24);

	timo = AWAHCI_TIMEOUT;
	while ((AWREAD4(sc, AWAHCI_PHYCS2) & (1 << 24)) && --timo)
		delay(10);
	if (!timo)
		printf("awahci_attach: AHCI phy calibration failed.\n");

	delay(15000);
	AWWRITE4(sc, AWAHCI_RWC, 7);

	/* power up phy */
	awpio_setcfg(AWAHCI_PWRPIN, AWPIO_OUTPUT);
	awpio_setpin(AWAHCI_PWRPIN);

	sc->sc_ih = arm_intr_establish(aw->aw_dev->irq[0], IPL_BIO,
	    ahci_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		goto unmap;
	}

	AWWRITE4(sc, AWAHCI_PI, 1);
	AWCLR4(sc, AWAHCI_CAP, AHCI_REG_CAP_SPM);
	sc->sc_flags |= AHCI_F_NO_PMP; /* XXX enough? */
	if (ahci_attach(sc) != 0) {
		/* error printed by ahci_attach */
		goto irq;
	}

	return;
irq:
	arm_intr_disestablish(sc->sc_ih);
unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
}

int
awahci_detach(struct device *self, int flags)
{
	struct awahci_softc *awsc = (struct awahci_softc *) self;
	struct ahci_softc *sc = &awsc->sc;

	ahci_detach(sc, flags);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	return 0;
}

int
awahci_activate(struct device *self, int act)
{
	struct awahci_softc *awsc = (struct awahci_softc *) self;
	struct ahci_softc *sc = &awsc->sc;

	return ahci_activate((struct device *)sc, act);
}
