/*	$OpenBSD: lasi.c,v 1.2 1999/02/25 23:15:31 mickey Exp $	*/

/*
 * Copyright (c) 1998,1999 Michael Shalayeff
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
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define LASIDEBUG 9

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hppa/dev/cpudevs.h>

#include <hppa/gsc/gscbusvar.h>

struct lasi_hwr {
	u_int32_t lasi_power;
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
};

int	lasimatch __P((struct device *, void *, void *));
void	lasiattach __P((struct device *, struct device *, void *));

struct cfattach lasi_ca = {
	sizeof(struct lasi_softc), lasimatch, lasiattach
};

struct cfdriver lasi_cd = {
	NULL, "lasi", DV_DULL
};

void lasi_intr_establish __P((void *v, u_int32_t mask));
void lasi_intr_disestablish __P((void *v, u_int32_t mask));
u_int32_t lasi_intr_check __P((void *v));
void lasi_intr_ack __P((void *v, u_int32_t mask));


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
	struct gsc_attach_args ga;
	int s, in;

	sc->sc_trs = (struct lasi_trs *)ca->ca_hpa;
	sc->sc_hw = (struct lasi_hwr *)(ca->ca_hpa + 0xc000);

	/* XXX should we reset the chip here? */

	printf (": rev %d.%d\n", (sc->sc_hw->lasi_version & 0xf0) >> 4,
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
	sc->sc_ic.gsc_intr_establish = lasi_intr_establish;
	sc->sc_ic.gsc_intr_disestablish = lasi_intr_disestablish;
	sc->sc_ic.gsc_intr_check = lasi_intr_check;
	sc->sc_ic.gsc_intr_ack = lasi_intr_ack;

	ga.ga_ca = *ca;	/* clone from us */
	ga.ga_name = "gsc";
	ga.ga_ic = &sc->sc_ic;
	config_found(self, &ga, gscprint);
}

void
lasi_intr_establish(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct lasi_softc *sc = v;

	sc->sc_trs->lasi_imr |= mask;
}

void
lasi_intr_disestablish(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct lasi_softc *sc = v;

	sc->sc_trs->lasi_imr &= ~mask;
}

u_int32_t
lasi_intr_check(v)
	void *v;
{
	register struct lasi_softc *sc = v;
	register u_int32_t irr, imr, ipr;

	imr = sc->sc_trs->lasi_imr;
	ipr = sc->sc_trs->lasi_ipr;
	irr = sc->sc_trs->lasi_irr;
	sc->sc_trs->lasi_imr = 0;
	sc->sc_trs->lasi_imr = imr &= ~irr;

#ifdef LASIDEBUG
	printf ("%s: imr=0x%x, irr=0x%x, ipr=0x%x, iar=0x%x, icr=0x%x\n",
		sc->sc_dev.dv_xname, imr, irr, ipr,
		sc->sc_trs->lasi_iar, sc->sc_trs->lasi_icr);
#endif

	return irr;
}

void
lasi_intr_ack(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct lasi_softc *sc = v;

	sc->sc_trs->lasi_imr |= mask;
}
