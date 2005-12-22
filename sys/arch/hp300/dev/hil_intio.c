/*	$OpenBSD: hil_intio.c,v 1.6 2005/12/22 07:09:49 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat.
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
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/cons.h>

#include <hp300/dev/intiovar.h>

#include <machine/hil_machdep.h>
#include <machine/bus.h>
#include <dev/hil/hilvar.h>

int	hil_intio_match(struct device *, void *, void *);
void	hil_intio_attach(struct device *, struct device *, void *);

struct cfattach hil_intio_ca = {
	sizeof(struct hil_softc), hil_intio_match, hil_intio_attach
};

int
hil_intio_match(struct device *parent, void *match, void *aux)
{
	struct intio_attach_args *ia = aux;
static	int hil_matched = 0;

	/* Allow only one instance. */
	if (hil_matched != 0)
		return (0);

	if (badaddr((caddr_t)IIOV(HILADDR)))	/* should not happen! */
		return (0);

	ia->ia_addr = (caddr_t)HILADDR;
	return (1);
}

struct isr hil_isr;
int hil_is_console = -1;	/* undecided */

void
hil_intio_attach(struct device *parent, struct device *self, void *aux)
{
	struct hil_softc *sc = (void *)self;
	extern struct consdev wsdisplay_cons;

	sc->sc_bst = HP300_BUS_TAG(HP300_BUS_INTIO, 0);
	if (bus_space_map(sc->sc_bst, HILADDR - INTIOBASE,
	    HILMAPSIZE, 0, &sc->sc_bsh) != 0) {
		printf(": couldn't map hil controller\n");
		return;
	}

	/*
	 * Check that the configured console device is a wsdisplay.
	 */
	if (cn_tab != &wsdisplay_cons)
		hil_is_console = 0;

	hil_isr.isr_func = hil_intr;
	hil_isr.isr_arg = sc;
	hil_isr.isr_ipl = 1;
	hil_isr.isr_priority = IPL_TTY;

	printf(" ipl %d", hil_isr.isr_ipl);

	hil_attach(sc, &hil_is_console);
	intr_establish(&hil_isr, self->dv_xname);

	startuphook_establish(hil_attach_deferred, sc);
}
