/*	$OpenBSD: lasi.c,v 1.10 2002/12/17 21:54:20 mickey Exp $	*/

/*
 * Copyright (c) 1998-2002 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF MIND,
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

	struct lasi_hwr volatile *sc_hw;
	struct lasi_trs volatile *sc_trs;
	struct gsc_attach_args ga;	/* for deferred attach */
};

int	lasimatch(struct device *, void *, void *);
void	lasiattach(struct device *, struct device *, void *);
void	lasi_gsc_attach(struct device *);

struct cfattach lasi_ca = {
	sizeof(struct lasi_softc), lasimatch, lasiattach
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
lasiattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct confargs *ca = aux;
	register struct lasi_softc *sc = (struct lasi_softc *)self;
	bus_space_handle_t ioh;
	int s, in;

	if (bus_space_map(ca->ca_iot, ca->ca_hpa + 0xc000,
			  IOMOD_HPASIZE, 0, &ioh)) {
#ifdef DEBUG
		printf("lasiattach: can't map IO space\n");
#endif
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

	sc->ga.ga_ca = *ca;	/* clone from us */
	if (sc->sc_dev.dv_unit)
		config_defer(self, lasi_gsc_attach);
	else {
		extern void (*cold_hook)(int);

		lasi_gsc_attach(self);
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
