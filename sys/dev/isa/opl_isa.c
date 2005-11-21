/*	$OpenBSD: opl_isa.c,v 1.4 2005/11/21 18:16:40 millert Exp $	*/
/*	$NetBSD: opl_isa.c,v 1.1 1998/08/26 13:33:59 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/selinfo.h>
#include <sys/audioio.h>
#include <sys/midiio.h>

#include <machine/bus.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>

#include <dev/ic/oplreg.h>
#include <dev/ic/oplvar.h>

#include <dev/isa/isavar.h>

#define OPL_SIZE 4

int	opl_isa_match(struct device *, void *, void *);
void	opl_isa_attach(struct device *, struct device *, void *);

struct cfattach opl_isa_ca = {
	sizeof (struct opl_softc), opl_isa_match, opl_isa_attach
};

int
opl_isa_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct opl_softc sc;
	int r;

	memset(&sc, 0, sizeof sc);
	sc.iot = ia->ia_iot;
	if (bus_space_map(sc.iot, ia->ia_iobase, OPL_SIZE, 0, &sc.ioh))
		return (0);
	r = opl_find(&sc);
        bus_space_unmap(sc.iot, sc.ioh, OPL_SIZE);
	return (r);
}

void
opl_isa_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct opl_softc *sc = (struct opl_softc *)self;
	struct isa_attach_args *ia = aux;

	if (bus_space_map(sc->iot, ia->ia_iobase, OPL_SIZE, 0, &sc->ioh)) {
		printf("opl_isa_attach: bus_space_map failed\n");
		return;
	}
	sc->offs = 0;

	opl_attach(sc);
}
