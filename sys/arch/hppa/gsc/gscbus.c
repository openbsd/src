/*	$OpenBSD: gscbus.c,v 1.30 2010/11/28 20:09:40 miod Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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

/* #define GSCDEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>

#include <machine/iomod.h>
#include <machine/autoconf.h>
#include <machine/cpufunc.h>

#include <hppa/gsc/gscbusvar.h>

int	gscmatch(struct device *, void *, void *);
void	gscattach(struct device *, struct device *, void *);

struct cfattach gsc_ca = {
	sizeof(struct gsc_softc), gscmatch, gscattach
};

struct cfdriver gsc_cd = {
	NULL, "gsc", DV_DULL
};

int
gscmatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;

	return !strcmp(ca->ca_name, "gsc");
}

void
gscattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct gsc_softc *sc = (struct gsc_softc *)self;
	struct gsc_attach_args *ga = aux;

	sc->sc_iot = ga->ga_iot;
	sc->sc_ic = ga->ga_ic;

#ifdef USELEDS
	if (machine_ledaddr)
		printf(": %sleds", machine_ledword? "word" : "");
#endif
	printf ("\n");

	sc->sc_ih = cpu_intr_establish(IPL_NESTED, ga->ga_irq,
	    gsc_intr, (void *)sc->sc_ic->gsc_base, sc->sc_dev.dv_xname);

	pdc_scanbus(self, &ga->ga_ca, MAXMODBUS, 0, 0);
}

int
gscprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct gsc_attach_args *ga = aux;

	if (pnp)
		printf("%s at %s", ga->ga_name, pnp);
	return (UNCONF);
}

void *
gsc_intr_establish(sc, irq, pri, handler, arg, name)
	struct gsc_softc *sc;
	int pri;
	int irq;
	int (*handler)(void *v);
	void *arg;
	const char *name;
{
	volatile u_int32_t *r = sc->sc_ic->gsc_base;
	void *iv;

	if ((iv = cpu_intr_map(sc->sc_ih, pri, irq, handler, arg, name)))
		r[1] |= (1 << irq);
	else {
#ifdef GSCDEBUG
		printf("%s: attaching irq %d, already occupied\n",
		       sc->sc_dev.dv_xname, irq);
#endif
	}

	return (iv);
}

void
gsc_intr_disestablish(sc, v)
	struct gsc_softc *sc;
	void *v;
{
#if notyet
	volatile u_int32_t *r = sc->sc_ic->gsc_base;

	r[1] &= ~(1 << irq);

	cpu_intr_unmap(sc->sc_ih, v);
#endif
}
