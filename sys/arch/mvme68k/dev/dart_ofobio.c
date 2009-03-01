/*	$OpenBSD: dart_ofobio.c,v 1.1 2009/03/01 22:08:13 miod Exp $	*/
/*
 * Copyright (c) 2006, 2009, Miodrag Vallat
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <mvme68k/dev/ofobioreg.h>

#include <mvme68k/dev/dartreg.h>
#include <mvme68k/dev/dartvar.h>

int	dart_ofobio_match(struct device *, void *, void *);
void	dart_ofobio_attach(struct device *, struct device *, void *);

struct cfattach dartofobio_ca = {
	sizeof(struct dartsoftc), dart_ofobio_match, dart_ofobio_attach
};

int
dart_ofobio_match(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;
#if 0
	bus_space_handle_t ioh;
	int rc;
#endif

	if (cputyp != CPU_141 || ca->ca_paddr != MVME141_DART_BASE)
		return (0);

#if 0	/* overkill, this is the console so if we've run so far, it exists */
	if (bus_space_map(ca->ca_iot, ca->ca_paddr, DART_SIZE, 0, &ioh) != 0)
		return (0);
	rc = badvaddr((vaddr_t)bus_space_vaddr(ca->ca_iot, ioh) + 3, 1);
	bus_space_unmap(ca->ca_iot, ca->ca_paddr, DART_SIZE);

	return (rc == 0);
#else
	return (1);
#endif
}

void
dart_ofobio_attach(struct device *parent, struct device *self, void *aux)
{
	struct dartsoftc *sc = (struct dartsoftc *)self;
	struct confargs *ca = aux;
	bus_space_handle_t ioh;

	if (ca->ca_ipl < 0)
		ca->ca_ipl = IPL_TTY;

	sc->sc_iot = ca->ca_iot;
	if (bus_space_map(sc->sc_iot, ca->ca_paddr, DART_SIZE, 0, &ioh) != 0) {
		printf(": can't map registers!\n");
		return;
	}
	sc->sc_ioh = ioh;

	sc->sc_console = 1;	/* there can't be any other */
	printf(": console");

	/* enable interrupts */
	sc->sc_ih.ih_fn = dartintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_wantframe = 0;
	sc->sc_ih.ih_ipl = ca->ca_ipl;

	sc->sc_vec = OFOBIOVEC_DART;
	intr_establish(sc->sc_vec, &sc->sc_ih, self->dv_xname);
	sc->sc_stride = 0;
	dart_common_attach(sc);
}
