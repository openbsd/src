/*	$OpenBSD: asp.c,v 1.1 1998/11/23 02:55:43 mickey Exp $	*/

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

struct asp_softc {
	struct  device sc_dv;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	bus_space_handle_t sc_cioh;

	struct gscbus_ic sc_ic;

	u_int8_t sc_leds;
};

/* ASP "Primary Controller" definitions */
#define	ASP_CHPA	0xF0800000
#define	ASP_IRR		0x000
#define	ASP_IMR		0x004
#define	ASP_IPR		0x008
#define	ASP_LEDS	0x020
#define		ASP_LED_DATA	0x01
#define		ASP_LED_STROBE	0x02
#define		ASP_LED_PULSE	0x08

/* ASP registers definitions */
#define	ASP_RESET	0x000
#define	ASP_VERSION	0x020
#define	ASP_DSYNC	0x030
#define	ASP_ERROR	0x040

int	aspmatch __P((struct device *, void *, void *));
void	aspattach __P((struct device *, struct device *, void *));

struct cfattach asp_ca = {
	sizeof(struct asp_softc), aspmatch, aspattach
};

struct cfdriver asp_cd = {
	NULL, "asp", DV_DULL
};

void asp_intr_attach __P((void *v, u_int in));
void asp_intr_establish __P((void *v, u_int32_t mask));
void asp_intr_disestablish __P((void *v, u_int32_t mask));
u_int32_t asp_intr_check __P((void *v));
void asp_intr_ack __P((void *v, u_int32_t mask));

int
aspmatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;
	/* struct cfdata *cf = cfdata; */
	bus_space_handle_t ioh;

	if (ca->ca_type.iodc_type != HPPA_TYPE_BHA ||
	    ca->ca_type.iodc_sv_model != HPPA_BHA_ASP)
		return 0;

	if (bus_space_map(ca->ca_iot, ca->ca_hpa, IOMOD_HPASIZE, 0, &ioh))
		return 0;

	bus_space_unmap(ca->ca_iot, ioh, IOMOD_HPASIZE);

	return 1;
}

void
aspattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct confargs *ca = aux;
	register struct asp_softc *sc = (struct asp_softc *)self;
	struct gsc_attach_args ga;
	u_int ver;

	sc->sc_leds = 0;
	sc->sc_iot = ca->ca_iot;
	if (bus_space_map(sc->sc_iot, ca->ca_hpa, IOMOD_HPASIZE, 0,
			  &sc->sc_ioh))
		panic("aspattach: unable to map bus space");

	if (bus_space_map(sc->sc_iot, ASP_CHPA, IOMOD_HPASIZE, 0,
			  &sc->sc_cioh))
		panic("aspattach: unable to map bus space");

	/* reset ASP */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, ASP_RESET, 1);

	ver = bus_space_read_4(sc->sc_iot, sc->sc_ioh, ASP_VERSION);
	printf(": hpa 0x%x, rev %d\n", ca->ca_hpa,
	       (ver & 0xf0) >> 4, ver & 0xf);

	sc->sc_ic.gsc_type = gsc_asp;
	sc->sc_ic.gsc_dv = sc;
	sc->sc_ic.gsc_intr_attach = asp_intr_attach;
	sc->sc_ic.gsc_intr_establish = asp_intr_establish;
	sc->sc_ic.gsc_intr_disestablish = asp_intr_disestablish;
	sc->sc_ic.gsc_intr_check = asp_intr_check;
	sc->sc_ic.gsc_intr_ack = asp_intr_ack;

	ga.ga_ca = *ca;	/* clone from us */
	ga.ga_name = "gsc";
	ga.ga_ic = &sc->sc_ic;
	config_found(self, &ga, gscprint);
}

#ifdef USELEDS
void
heartbeat(int on)
{
	register struct asp_softc *sc;

	sc = asp_cd.cd_devs[0];
	if (sc) {
		register u_int8_t r = sc->sc_leds ^= ASP_LED_PULSE, b;
		for (b = 0x80; b; b >>= 1) {
			bus_space_write_1(sc->sc_iot, sc->sc_cioh, ASP_LEDS,
					  (r & b)? 1 : 0);
			bus_space_write_1(sc->sc_iot, sc->sc_cioh, ASP_LEDS,
					  ASP_LED_STROBE | ((r & b)? 1 : 0));
		}
	}
}
#endif

void
asp_intr_attach(v, irq)
	void *v;
	u_int irq;
{
	register struct asp_softc *sc = v;
	int s;

	s = splhigh();
	cpu_setintrwnd(1 << irq);

	bus_space_write_4(sc->sc_iot, sc->sc_cioh, ASP_IMR, ~0);
	bus_space_read_4 (sc->sc_iot, sc->sc_cioh, ASP_IRR);
	bus_space_write_4(sc->sc_iot, sc->sc_cioh, ASP_IMR, 0);
	splx(s);
}

void
asp_intr_establish(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct asp_softc *sc = v;

	mask |= bus_space_read_4(sc->sc_iot, sc->sc_cioh, ASP_IMR);
	bus_space_write_4(sc->sc_iot, sc->sc_cioh, ASP_IMR, mask);
}

void
asp_intr_disestablish(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct asp_softc *sc = v;

	mask &= ~bus_space_read_4(sc->sc_iot, sc->sc_cioh, ASP_IMR);
	bus_space_write_4(sc->sc_iot, sc->sc_cioh, ASP_IMR, mask);
}

u_int32_t
asp_intr_check(v)
	void *v;
{
	register struct asp_softc *sc = v;
	register u_int32_t mask, imr;

	imr = bus_space_read_4(sc->sc_iot, sc->sc_cioh, ASP_IMR);
	mask = bus_space_read_4(sc->sc_iot, sc->sc_cioh, ASP_IRR);
	bus_space_write_4(sc->sc_iot, sc->sc_cioh, ASP_IMR, imr & ~mask);

	return mask;
}

void
asp_intr_ack(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct asp_softc *sc = v;

	mask |= bus_space_read_4(sc->sc_iot, sc->sc_cioh, ASP_IMR);
	bus_space_write_4(sc->sc_iot, sc->sc_cioh, ASP_IMR, mask);
}
