/*	$OpenBSD: fhc_mainbus.c,v 1.1 2004/09/22 21:44:45 jason Exp $	*/

/*
 * Copyright (c) 2004 Jason L. Wright (jason@thought.net).
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <sparc64/dev/fhcvar.h>

int	fhc_mainbus_match(struct device *, void *, void *);
void	fhc_mainbus_attach(struct device *, struct device *, void *);

struct cfattach fhc_mainbus_ca = {
	sizeof(struct fhc_softc), fhc_mainbus_match, fhc_mainbus_attach
};

int
fhc_mainbus_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, "fhc") == 0)
		return (1);
	return (0);
}

void
fhc_mainbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct fhc_softc *sc = (struct fhc_softc *)self;
	struct mainbus_attach_args *ma = aux;

	sc->sc_node = ma->ma_node;
	sc->sc_bt = ma->ma_bustag;
	fhc_attach(sc);
	return;
}
