/* $OpenBSD: rt_isa.c,v 1.1 2002/08/28 21:20:48 mickey Exp $ */

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

/* ISA interface for AIMS Lab Radiotrack FM Radio Card device driver */

/*
 * Sanyo LM7000 Direct PLL Frequency Synthesizer
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/ic/lm700x.h>
#include <dev/isa/isavar.h>
#include <dev/isa/rtreg.h>
#include <dev/isa/rtvar.h>

int	rt_isa_probe(struct device *, void *, void *);
void	rt_isa_attach(struct device *, struct device *, void *);

struct cfattach rt_isa_ca = {
	sizeof(struct rt_softc), rt_isa_probe, rt_isa_attach
};

int
rt_isa_probe(struct device *parent, void *match, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;
	int iosize = 1;

	if (!RT_BASE_VALID(ia->ia_iobase)) {
		printf("rt: wrong iobase 0x%x\n", ia->ia_iobase);
		return (0);
	}

	if (bus_space_map(ia->ia_iot, ia->ia_iobase, iosize, 0, &ioh))
		return (0);

	/* This doesn't work yet */
	bus_space_unmap(ia->ia_iot, ioh, iosize);
	return (0);
#if 0
	ia->ia_iosize = iosize;
	return 1;
#endif /* 0 */
}

void
rt_isa_attach(struct device *parent, struct device *self, void *aux)
{
	struct rt_softc *sc = (void *) self;
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh;

	/* remap I/O */
	if (bus_space_map(ia->ia_iot, ia->ia_iobase, ia->ia_iosize, 0, &ioh)) {
		printf(": bus_space_map() failed\n");
		return;
	}

	printf(": AIMS Lab Radiotrack or compatible\n");

	sc->sc_ct = CARD_RADIOTRACK;
	sc->lm.iot = ia->ia_iot;
	sc->lm.ioh = ioh;

	rtattach(sc);
}
