/*	$OpenBSD: sb_isa.c,v 1.1 1997/07/10 23:06:37 provos Exp $	*/
/*	$NetBSD: sb_isa.c,v 1.3 1997/03/20 11:03:11 mycroft Exp $	*/

/*
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/sbreg.h>
#include <dev/isa/sbvar.h>

#include <dev/isa/sbdspvar.h>

int	sb_isa_match __P((struct device *, void *, void *));
void	sb_isa_attach __P((struct device *, struct device *, void *));

struct cfattach sb_isa_ca = {
	sizeof(struct sbdsp_softc), sb_isa_match, sb_isa_attach
};

/*
 * Probe / attach routines.
 */

/*
 * Probe for the soundblaster hardware.
 */
int
sb_isa_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	/*
	 * Indirect brokedness!
	 */
	register struct sbdsp_softc *sc = match;
	register struct isa_attach_args *ia = aux;

	if (!SB_BASE_VALID(ia->ia_iobase)) {
		printf("sb: configured iobase 0x%x invalid\n", ia->ia_iobase);
		return 0;
	}

	sc->sc_iot = ia->ia_iot;

	/* Map i/o space [we map 24 ports which is the max of the sb and pro  */
	if (bus_space_map(sc->sc_iot, ia->ia_iobase, SBP_NPORT, 0,
	    &sc->sc_ioh)) {
		printf("sb: can't map i/o space 0x%x/%d in probe\n",
		    ia->ia_iobase, SBP_NPORT);
		return 0;
	}

	/*
	 * Indirect brokedness!
	 */
	sc->sc_iobase = ia->ia_iobase;
	sc->sc_irq = ia->ia_irq;
	sc->sc_drq8 = ia->ia_drq;
	sc->sc_drq16 = -1;	/* XXX */
	sc->sc_ic = ia->ia_ic;

	if (!sbmatch(sc)) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, SBP_NPORT);
		return 0;
	}

	if (ISSBPROCLASS(sc))
		ia->ia_iosize = SBP_NPORT;
	else
		ia->ia_iosize = SB_NPORT;

	ia->ia_irq = sc->sc_irq;

	return 1;
}


/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
sb_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	sbattach((struct sbdsp_softc *) self);
}
