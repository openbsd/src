/*	$OpenBSD: dart_syscon.c,v 1.1.1.1 2006/05/09 18:13:37 miod Exp $	*/
/*
 * Copyright (c) 2006, Miodrag Vallat
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

#include <machine/av400.h>

#include <aviion/dev/sysconreg.h>
#include <aviion/dev/dartvar.h>

int	dart_syscon_match(struct device *parent, void *self, void *aux);
void	dart_syscon_attach(struct device *parent, struct device *self, void *aux);

struct cfattach dart_syscon_ca = {
	sizeof(struct dartsoftc), dart_syscon_match, dart_syscon_attach
};

int
dart_syscon_match(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;
	bus_space_handle_t ioh;
	int rc;

	/*
	 * We do not accept empty locators here...
	 */
	if (ca->ca_paddr == (paddr_t)-1)
		return (0);

	if (bus_space_map(ca->ca_iot, ca->ca_paddr, DART_SIZE, 0, &ioh) != 0)
		return (0);
	rc = badaddr((vaddr_t)bus_space_vaddr(ca->ca_iot, ioh), 4);
	bus_space_unmap(ca->ca_iot, ca->ca_paddr, DART_SIZE);

	return (rc == 0);
}

void
dart_syscon_attach(struct device *parent, struct device *self, void *aux)
{
	struct dartsoftc *sc = (struct dartsoftc *)self;
	struct confargs *ca = aux;
	bus_space_handle_t ioh;
	u_int vec;

	if (ca->ca_ipl < 0)
		ca->ca_ipl = IPL_TTY;

	sc->sc_iot = ca->ca_iot;
	if (bus_space_map(sc->sc_iot, ca->ca_paddr, DART_SIZE, 0, &ioh) != 0) {
		printf(": can't map registers!\n");
		return;
	}
	sc->sc_ioh = ioh;

	if (ca->ca_paddr == DART_BASE) {
		vec = SYSCV_SCC;
		sc->sc_console = 1;	/* XXX for now */
		printf(": console");
	} else {
		vec = SYSCV_SCC2;
		sc->sc_console = 0;
	}

	/* enable interrupts */
	sc->sc_ih.ih_fn = dartintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_wantframe = 0;
	sc->sc_ih.ih_ipl = ca->ca_ipl;

	sysconintr_establish(vec, &sc->sc_ih, self->dv_xname);

	dart_common_attach(sc);
}
