/*	$OpenBSD: asp.c,v 1.5 2000/02/09 05:04:22 mickey Exp $	*/

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
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF MIND,
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * References:
 *
 * 1. Cobra/Coral I/O Subsystem External Reference Specification
 *    Hewlett-Packard
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>
#include <machine/cpufunc.h>

#include <hppa/dev/cpudevs.h>
#include <hppa/dev/viper.h>

#include <hppa/gsc/gscbusvar.h>

struct asp_hwr {
	u_int8_t asp_reset;
	u_int8_t asp_resv[31];
	u_int8_t asp_version;
	u_int8_t asp_resv1[15];
	u_int8_t asp_scsidsync;
	u_int8_t asp_resv2[15];
	u_int8_t asp_error;
};

struct asp_trs {
	u_int32_t asp_irr;
	u_int32_t asp_imr;
	u_int32_t asp_ipr;
	u_int32_t asp_icr;
	u_int32_t asp_iar;
	u_int32_t asp_resv[3];
	u_int8_t  asp_cled;
	u_int8_t  asp_resv1[3];
	struct {
		u_int		:20,
			asp_spu	: 3,	/* SPU ID board jumper */
#define	ASP_SPUCOBRA	0
#define	ASP_SPUCORAL	1
#define	ASP_SPUBUSH	2
#define	ASP_SPUHARDBALL	3
#define	ASP_SPUSCORPIO	4
#define	ASP_SPUCORAL2	5
			asp_sw	: 1,	/* front switch is normal */
			asp_clk : 1,	/* SCSI clock is doubled */
			asp_lan : 2,	/* LAN iface selector */
#define	ASP_LANINVAL	0
#define	ASP_LANAUI	1
#define	ASP_LANTHIN	2
#define	ASP_LANMISS	3
			asp_lanf: 1,	/* LAN AUI fuse is ok */
			asp_spwr: 1,	/* SCSI power ok */
			asp_scsi: 3;	/* SCSI ctrl ID */
	} _asp_ios;
#define	asp_spu		_asp_ios.asp_spu
#define	asp_sw		_asp_ios.asp_sw
#define	asp_clk		_asp_ios.asp_clk
#define	asp_lan		_asp_ios.asp_lan
#define	asp_lanf	_asp_ios.asp_lanf
#define	asp_spwr	_asp_ios.asp_spwr
#define	asp_scsi	_asp_ios.asp_scsi
};

const struct asp_spus_tag {
	char	name[12];
	int	ledword;
} asp_spus[] = {
	{ "Cobra", 0 },
	{ "Coral", 0 },
	{ "Bushmaster", 0 },
	{ "Hardball", 1 },
	{ "Scorpio", 0 },
	{ "Coral II", 1 },
	{ "#6", 0 },
	{ "#7", 0 }
};

struct asp_softc {
	struct  device sc_dev;
	struct gscbus_ic sc_ic;

	volatile struct asp_hwr *sc_hw;
	volatile struct asp_trs *sc_trs;
};

/* ASP "Primary Controller" HPA */
#define	ASP_CHPA	0xF0800000

int	aspmatch __P((struct device *, void *, void *));
void	aspattach __P((struct device *, struct device *, void *));

struct cfattach asp_ca = {
	sizeof(struct asp_softc), aspmatch, aspattach
};

struct cfdriver asp_cd = {
	NULL, "asp", DV_DULL
};

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

	if (ca->ca_type.iodc_type != HPPA_TYPE_BHA ||
	    ca->ca_type.iodc_sv_model != HPPA_BHA_ASP)
		return 0;

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
	bus_space_handle_t ioh;
	register u_int32_t irr;
	register int s;

	if (bus_space_map(ca->ca_iot, ca->ca_hpa, IOMOD_HPASIZE, 0, &ioh)) {
#ifdef DEBUG
		printf("aspattach: can't map IO space\n");
#endif
		return;
	}

	sc->sc_trs = (struct asp_trs *)ASP_CHPA;
	sc->sc_hw = (struct asp_hwr *)ca->ca_hpa;

	machine_ledaddr = &sc->sc_trs->asp_cled;
	machine_ledword = asp_spus[sc->sc_trs->asp_spu].ledword;

	/* reset ASP */
	/* sc->sc_hw->asp_reset = 1; */
	/* delay(400000); */

	s = splhigh();
	viper_setintrwnd(1 << ca->ca_irq);

	sc->sc_trs->asp_imr = ~0;
	irr = sc->sc_trs->asp_irr;
	sc->sc_trs->asp_imr = 0;
	splx(s);

	printf (": %s rev %d, lan %d scsi %d\n",
	    asp_spus[sc->sc_trs->asp_spu].name, sc->sc_hw->asp_version,
	    sc->sc_trs->asp_lan, sc->sc_trs->asp_scsi);

	sc->sc_ic.gsc_type = gsc_asp;
	sc->sc_ic.gsc_dv = sc;
	sc->sc_ic.gsc_intr_establish = asp_intr_establish;
	sc->sc_ic.gsc_intr_disestablish = asp_intr_disestablish;
	sc->sc_ic.gsc_intr_check = asp_intr_check;
	sc->sc_ic.gsc_intr_ack = asp_intr_ack;

	ga.ga_ca = *ca;	/* clone from us */
	ga.ga_name = "gsc";
	ga.ga_ic = &sc->sc_ic;
	config_found(self, &ga, gscprint);
}

void
asp_intr_establish(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct asp_softc *sc = v;

	sc->sc_trs->asp_imr |= mask;
}

void
asp_intr_disestablish(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct asp_softc *sc = v;

	sc->sc_trs->asp_imr &= ~mask;
}

u_int32_t
asp_intr_check(v)
	void *v;
{
	register struct asp_softc *sc = v;
	register u_int32_t irr, imr;

	imr = sc->sc_trs->asp_imr;
	irr = sc->sc_trs->asp_irr;
	sc->sc_trs->asp_imr = imr & ~irr;

	return irr;
}

void
asp_intr_ack(v, mask)
	void *v;
	u_int32_t mask;
{
	register struct asp_softc *sc = v;

	sc->sc_trs->asp_imr |= mask;
}
