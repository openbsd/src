/*	$OpenBSD: wax.c,v 1.10 2004/11/08 20:53:25 miod Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hppa/dev/cpudevs.h>

#include <hppa/gsc/gscbusvar.h>

#define	WAX_IOMASK	0xfff00000

struct wax_regs {
	u_int32_t wax_irr;	/* int request register */
	u_int32_t wax_imr;	/* int mask register */
	u_int32_t wax_ipr;	/* int pending register */
	u_int32_t wax_icr;	/* int command? register */
	u_int32_t wax_iar;	/* int acquire? register */
};

struct wax_softc {
	struct device sc_dv;
	struct gscbus_ic sc_ic;

	struct wax_regs volatile *sc_regs;
};

int	waxmatch(struct device *, void *, void *);
void	waxattach(struct device *, struct device *, void *);
void	wax_gsc_attach(struct device *);

struct cfattach wax_ca = {
	sizeof(struct wax_softc), waxmatch, waxattach
};

struct cfdriver wax_cd = {
	NULL, "wax", DV_DULL
};

int
waxmatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;
	struct cfdata *cf = cfdata;

	/* there will be only one */
	if (cf->cf_unit > 0 ||
	    ca->ca_type.iodc_type != HPPA_TYPE_BHA ||
	    ca->ca_type.iodc_sv_model != HPPA_BHA_WAX)
		return 0;

	return 1;
}

void
waxattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct wax_softc *sc = (struct wax_softc *)self;
	struct confargs *ca = aux;
	struct gsc_attach_args ga;
	bus_space_handle_t ioh;
	int s, in;

	if (bus_space_map(ca->ca_iot, ca->ca_hpa, IOMOD_HPASIZE, 0, &ioh)) {
		printf(": can't map IO space\n");
		return;
	}

	sc->sc_regs = (struct wax_regs *)ca->ca_hpa;

	printf("\n");

	/* interrupts guts */
	s = splhigh();
	sc->sc_regs->wax_iar = cpu_gethpa(0) | (31 - ca->ca_irq);
	sc->sc_regs->wax_icr = 0;
	sc->sc_regs->wax_imr = ~0U;
	in = sc->sc_regs->wax_irr;
	sc->sc_regs->wax_imr = 0;
	splx(s);

	sc->sc_ic.gsc_type = gsc_wax;
	sc->sc_ic.gsc_dv = sc;
	sc->sc_ic.gsc_base = sc->sc_regs;

	ga.ga_ca = *ca;	/* clone from us */
	if (!strcmp(parent->dv_xname, "mainbus0")) {
		ga.ga_dp.dp_bc[0] = ga.ga_dp.dp_bc[1];
		ga.ga_dp.dp_bc[1] = ga.ga_dp.dp_bc[2];
		ga.ga_dp.dp_bc[2] = ga.ga_dp.dp_bc[3];
		ga.ga_dp.dp_bc[3] = ga.ga_dp.dp_bc[4];
		ga.ga_dp.dp_bc[4] = ga.ga_dp.dp_bc[5];
		ga.ga_dp.dp_bc[5] = ga.ga_dp.dp_mod;
		ga.ga_dp.dp_mod = 0;
	}

	ga.ga_name = "gsc";
	ga.ga_hpamask = WAX_IOMASK;
	ga.ga_ic = &sc->sc_ic;
	config_found(self, &ga, gscprint);
}
