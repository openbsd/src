/*	$OpenBSD: uturn.c,v 1.1 2004/09/15 21:32:43 mickey Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff
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

/* TODO IOA programming */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hppa/dev/cpudevs.h>

struct uturn_regs {
	u_int64_t	resv0[2];
	u_int64_t	status;		/* 0x10: */
	u_int64_t	resv1[5];
	u_int64_t	debug;		/* 0x40: */
};

struct uturn_softc {
	struct device sc_dv;

	struct uturn_regs volatile *sc_regs;
};

int	uturnmatch(struct device *, void *, void *);
void	uturnattach(struct device *, struct device *, void *);

struct cfattach uturn_ca = {
	sizeof(struct uturn_softc), uturnmatch, uturnattach
};

struct cfdriver uturn_cd = {
	NULL, "uturn", DV_DULL
};

int
uturnmatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;
	/* struct cfdata *cf = cfdata; */

	/* there will be only one */
	if (ca->ca_type.iodc_type != HPPA_TYPE_IOA ||
	    ca->ca_type.iodc_sv_model != HPPA_IOA_UTURN)
		return 0;

	return 1;
}

void
uturnattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct confargs *ca = aux, nca;
	struct uturn_softc *sc = (struct uturn_softc *)self;
	bus_space_handle_t ioh;

	if (bus_space_map(ca->ca_iot, ca->ca_hpa, IOMOD_HPASIZE, 0, &ioh)) {
		printf(": can't map IO space\n");
		return;
	}
	sc->sc_regs = (struct uturn_regs *)ca->ca_hpa;

	printf(": %s rev %d\n",
	    ca->ca_type.iodc_revision < 0x10? "U2" : "UTurn",
	    ca->ca_type.iodc_revision & 0xf);

	/* keep it real */
	((struct iomod *)ioh)->io_control = 0x80;

	nca = *ca;	/* clone from us */
	nca.ca_hpamask = HPPA_IOSPACE;
	pdc_scanbus(self, &nca, MAXMODBUS, 0);
}
