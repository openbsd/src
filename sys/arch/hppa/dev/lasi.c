/*	$OpenBSD: lasi.c,v 1.21 2004/09/15 01:10:06 mickey Exp $	*/

/*
 * Copyright (c) 1998-2003 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#undef LASIDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <machine/iomod.h>
#include <machine/reg.h>
#include <machine/autoconf.h>

#include <hppa/dev/cpudevs.h>

#include <hppa/gsc/gscbusvar.h>

#define	LASI_IOMASK	0xfff00000

struct lasi_hwr {
	u_int32_t lasi_power;
#define	LASI_BLINK	0x01
#define	LASI_OFF	0x02
	u_int32_t lasi_error;
	u_int32_t lasi_version;
	u_int32_t lasi_reset;
	u_int32_t lasi_arbmask;
};

struct lasi_trs {
	u_int32_t lasi_irr;	/* int requset register */
	u_int32_t lasi_imr;	/* int mask register */
	u_int32_t lasi_ipr;	/* int pending register */
	u_int32_t lasi_icr;	/* int command? register */
	u_int32_t lasi_iar;	/* int acquire? register */
};

struct lasi_softc {
	struct device sc_dev;
	struct gscbus_ic sc_ic;
	int sc_phantomassed;

	struct lasi_hwr volatile *sc_hw;
	struct lasi_trs volatile *sc_trs;
	struct gsc_attach_args ga;	/* for deferred attach */
};

int	lasimatch(struct device *, void *, void *);
void	lasiattach(struct device *, struct device *, void *);
void	lasi_mainbus_attach(struct device *, struct device *, void *);
void	lasi_phantomas_attach(struct device *, struct device *, void *);
void	lasi_gsc_attach(struct device *);

struct cfattach lasi_mainbus_ca = {
	sizeof(struct lasi_softc), lasimatch, lasi_mainbus_attach
};

struct cfattach lasi_phantomas_ca = {
	sizeof(struct lasi_softc), lasimatch, lasi_phantomas_attach
};

struct cfdriver lasi_cd = {
	NULL, "lasi", DV_DULL
};

void lasi_cold_hook(int on);

int
lasimatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	register struct confargs *ca = aux;
	/* register struct cfdata *cf = cfdata; */

	if (ca->ca_type.iodc_type != HPPA_TYPE_BHA ||
	    ca->ca_type.iodc_sv_model != HPPA_BHA_LASI)
		return 0;

	return 1;
}

void
lasi_mainbus_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	lasiattach(parent, self, aux);
}

void
lasi_phantomas_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct lasi_softc *sc = (struct lasi_softc *)self;

	sc->sc_phantomassed = 1;
	lasiattach(parent, self, aux);
}

void
lasiattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct lasi_softc *sc = (struct lasi_softc *)self;
	struct confargs *ca = aux;
	bus_space_handle_t ioh, ioh2;
	int s, in;

	if (bus_space_map(ca->ca_iot, ca->ca_hpa,
	    IOMOD_HPASIZE, 0, &ioh)) {
		printf(": can't map TRS space\n");
		return;
	}

	if (bus_space_map(ca->ca_iot, ca->ca_hpa + 0xc000,
	    IOMOD_HPASIZE, 0, &ioh2)) {
		bus_space_unmap(ca->ca_iot, ioh, IOMOD_HPASIZE);
		printf(": can't map IO space\n");
		return;
	}

	sc->sc_trs = (struct lasi_trs *)ca->ca_hpa;
	sc->sc_hw = (struct lasi_hwr *)(ca->ca_hpa + 0xc000);

	/* XXX should we reset the chip here? */

	printf(": rev %d.%d\n", (sc->sc_hw->lasi_version & 0xf0) >> 4,
	    sc->sc_hw->lasi_version & 0xf);

	/* interrupts guts */
	s = splhigh();
	sc->sc_trs->lasi_iar = cpu_gethpa(0) | (31 - ca->ca_irq);
	sc->sc_trs->lasi_icr = 0;
	sc->sc_trs->lasi_imr = ~0U;
	in = sc->sc_trs->lasi_irr;
	sc->sc_trs->lasi_imr = 0;
	splx(s);

	sc->sc_ic.gsc_type = gsc_lasi;
	sc->sc_ic.gsc_dv = sc;
	sc->sc_ic.gsc_base = sc->sc_trs;

#ifdef USELEDS
	/* figure out the leds address */
	switch (cpu_hvers) {
	case HPPA_BOARD_HP712_60:
	case HPPA_BOARD_HP712_80:
	case HPPA_BOARD_HP712_100:
	case HPPA_BOARD_HP743I_64:
	case HPPA_BOARD_HP743I_100:
	case HPPA_BOARD_HP712_120:
		break;	/* only has one led. works different */

	case HPPA_BOARD_HP715_64:
	case HPPA_BOARD_HP715_80:
	case HPPA_BOARD_HP715_100:
	case HPPA_BOARD_HP715_100XC:
	case HPPA_BOARD_HP725_100:
	case HPPA_BOARD_HP725_120:
		if (bus_space_map(ca->ca_iot, ca->ca_hpa - 0x20000,
		    4, 0, (bus_space_handle_t *)&machine_ledaddr))
			machine_ledaddr = NULL;
		machine_ledword = 1;
		break;

	case HPPA_BOARD_HP800_A180C:
	case HPPA_BOARD_HP778_B132L:
	case HPPA_BOARD_HP778_B132LP:
	case HPPA_BOARD_HP778_B160L:
	case HPPA_BOARD_HP778_B180L:
	case HPPA_BOARD_HP780_C100:
	case HPPA_BOARD_HP780_C110:
	case HPPA_BOARD_HP779_C132L:
	case HPPA_BOARD_HP779_C160L:
	case HPPA_BOARD_HP779_C180L:
	case HPPA_BOARD_HP779_C160L1:
		if (bus_space_map(ca->ca_iot, 0xf0190000,
		    4, 0, (bus_space_handle_t *)&machine_ledaddr))
			machine_ledaddr = NULL;
		machine_ledword = 1;
		break;

	default:
		machine_ledaddr = (u_int8_t *)sc->sc_hw;
		machine_ledword = 1;
		break;
	}
#endif

	sc->ga.ga_ca = *ca;	/* clone from us */
	if (!sc->sc_phantomassed) {
		sc->ga.ga_dp.dp_bc[0] = sc->ga.ga_dp.dp_bc[1];
		sc->ga.ga_dp.dp_bc[1] = sc->ga.ga_dp.dp_bc[2];
		sc->ga.ga_dp.dp_bc[2] = sc->ga.ga_dp.dp_bc[3];
		sc->ga.ga_dp.dp_bc[3] = sc->ga.ga_dp.dp_bc[4];
		sc->ga.ga_dp.dp_bc[4] = sc->ga.ga_dp.dp_bc[5];
		sc->ga.ga_dp.dp_bc[5] = sc->ga.ga_dp.dp_mod;
		sc->ga.ga_dp.dp_mod = 0;
	}
	if (sc->sc_dev.dv_unit)
		config_defer(self, lasi_gsc_attach);
	else {
		extern void (*cold_hook)(int);

		lasi_gsc_attach(self);
		/* could be already set by power(4) */
		if (!cold_hook)
			cold_hook = lasi_cold_hook;
	}
}

void
lasi_gsc_attach(self)
	struct device *self;
{
	struct lasi_softc *sc = (struct lasi_softc *)self;

	sc->ga.ga_name = "gsc";
	sc->ga.ga_hpamask = LASI_IOMASK;
	sc->ga.ga_ic = &sc->sc_ic;
	config_found(self, &sc->ga, gscprint);
}

void
lasi_cold_hook(on)
	int on;
{
	register struct lasi_softc *sc = lasi_cd.cd_devs[0];

	if (!sc)
		return;

	switch (on) {
	case HPPA_COLD_COLD:
		sc->sc_hw->lasi_power = LASI_BLINK;
		break;
	case HPPA_COLD_HOT:
		sc->sc_hw->lasi_power = 0;
		break;
	case HPPA_COLD_OFF:
		sc->sc_hw->lasi_power = LASI_OFF;
		break;
	}
}
