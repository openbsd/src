/*	$OpenBSD: lasi.c,v 1.1 1998/11/23 02:55:43 mickey Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hppa/dev/cpudevs.h>

#include <hppa/gsc/gscbusvar.h>

struct lasi_softc {
	struct  device sc_dv;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	struct gscbus_ic sc_ic;
};

/* LASI registers definition */
#define	LASI_IRR	0x000
#define	LASI_IMR	0x004
#define	LASI_IPR	0x008
#define	LASI_ICR	0x00c
#define	LASI_IAR	0x010
#define	LASI_POWER	0x800
#define	LASI_ERROR	0x804
#define	LASI_VERSION	0x808
#define	LASI_RESET	0x80c
#define	LASI_ARBMASK	0x810


int	lasimatch __P((struct device *, void *, void *));
void	lasiattach __P((struct device *, struct device *, void *));

struct cfattach lasi_ca = {
	sizeof(struct lasi_softc), lasimatch, lasiattach
};

struct cfdriver lasi_cd = {
	NULL, "lasi", DV_DULL
};

void lasi_intr_attach __P((void *v, u_int in));
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
	struct confargs *ca = aux;
	/* struct cfdata *cf = cfdata; */
	bus_space_handle_t ioh;

	if (ca->ca_type.iodc_type != HPPA_TYPE_BHA ||
	    ca->ca_type.iodc_sv_model != HPPA_BHA_LASI)
		return 0;

	if (bus_space_map(ca->ca_iot, ca->ca_hpa, IOMOD_HPASIZE, 0, &ioh))
		return 0;

	bus_space_unmap(ca->ca_iot, ioh, IOMOD_HPASIZE);

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
	u_int ver;

	sc->sc_iot = ca->ca_iot;
	if (bus_space_map(sc->sc_iot, ca->ca_hpa, IOMOD_HPASIZE, 0,
			  &sc->sc_ioh))
		panic("lasiattach: unable to map bus space");

	/* XXX should we reset the chip here? */

	ver = bus_space_read_4(sc->sc_iot, sc->sc_ioh, LASI_VERSION);
	printf (": hpa 0x%x, rev %x\n", ca->ca_hpa, ver & 0xff);

	sc->sc_ic.gsc_type = gsc_lasi;
	sc->sc_ic.gsc_dv = sc;
	sc->sc_ic.gsc_intr_attach = lasi_intr_attach;
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
lasi_intr_attach(v, in)
	void *v;
	u_int in;
{
	register struct lasi_softc *sc = v;
	hppa_hpa_t cpu_hpa;

	cpu_hpa = cpu_gethpa(0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LASI_IAR, cpu_hpa | in);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LASI_ICR, 0);
}

void
lasi_intr_establish(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct lasi_softc *sc = v;

	mask |= bus_space_read_4(sc->sc_iot, sc->sc_ioh, LASI_IMR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LASI_IMR, mask);
}

void
lasi_intr_disestablish(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct lasi_softc *sc = v;

	mask &= ~bus_space_read_4(sc->sc_iot, sc->sc_ioh, LASI_IMR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LASI_IMR, mask);
}

u_int32_t
lasi_intr_check(v)
	void *v;
{
	register struct lasi_softc *sc = v;
	register u_int32_t mask, imr;

	imr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, LASI_IMR);
	mask = bus_space_read_4(sc->sc_iot, sc->sc_ioh, LASI_IRR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LASI_IMR, imr & ~mask);

	return mask;
}

void
lasi_intr_ack(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct lasi_softc *sc = v;

	mask |= bus_space_read_4(sc->sc_iot, sc->sc_ioh, LASI_IMR);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, LASI_IMR, mask);
}
