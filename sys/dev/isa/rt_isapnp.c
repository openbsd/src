/* $OpenBSD: rt_isapnp.c,v 1.1 2002/08/28 21:20:48 mickey Exp $ */

/*
 * Copyright (c) 2001, 2002 Maxim Tsyplakov <tm@oganer.net>,
 *                    Vladimir Popov <jumbo@narod.ru>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* ISAPnP interface for AIMS Lab Radiotrack FM Radio Card device driver */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/ic/lm700x.h>
#include <dev/isa/isavar.h>
#include <dev/isa/rtreg.h>
#include <dev/isa/rtvar.h>

int	rt_isapnp_probe(struct device *, void *, void *);
void	rt_isapnp_attach(struct device *, struct device *, void *);

struct cfattach rt_isapnp_ca = {
	sizeof(struct rt_softc), rt_isapnp_probe, rt_isapnp_attach
};

int
rt_isapnp_probe(struct device *parent, void *match, void *aux)
{
	/* Always there */
	return (1);
}

void
rt_isapnp_attach(struct device *parent, struct device *self, void *aux)
{
	struct rt_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;

	printf(": SF16-FMI\n");

	sc->sc_ct = CARD_SF16FMI;
	sc->lm.iot = ia->ia_iot;
	sc->lm.ioh = ia->ipa_io[0].h; /* ia_ioh */

	rtattach(sc);
}
