/*	$OpenBSD: opl_yds.c,v 1.8 2010/04/08 00:23:53 tedu Exp $	*/
/*	$NetBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/selinfo.h>
#include <sys/audioio.h>
#include <sys/midiio.h>

#include <machine/bus.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/ic/oplreg.h>
#include <dev/ic/oplvar.h>
#include <dev/ic/ac97.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/ydsreg.h>
#include <dev/pci/ydsvar.h>

int	opl_yds_match(struct device *, void *, void *);
void	opl_yds_attach(struct device *, struct device *, void *);

struct cfdriver opl_yds_cd = {
	NULL, "opl_yds", DV_DULL
};

struct cfattach opl_yds_ca = {
	sizeof (struct opl_softc), opl_yds_match, opl_yds_attach
};

int
opl_yds_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct audio_attach_args *aa = (struct audio_attach_args *)aux;
	struct yds_softc *ssc = (struct yds_softc *)parent;
	struct opl_attach_arg oaa;

	if (aa->type != AUDIODEV_TYPE_OPL)
		return (0);
	memset(&oaa, 0, sizeof oaa);
	oaa.iot = ssc->sc_opl_iot;
	oaa.ioh = ssc->sc_opl_ioh;
	return (opl_find(&oaa));
}

void
opl_yds_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct yds_softc *ssc = (struct yds_softc *)parent;
	struct opl_softc *sc = (struct opl_softc *)self;

	sc->ioh = ssc->sc_opl_ioh;
	sc->iot = ssc->sc_opl_iot;
	sc->offs = 0;
	strlcpy(sc->syn.name, "DS-1 integrated ", sizeof sc->syn.name);

	opl_attach(sc);
}
