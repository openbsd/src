/*	$OpenBSD: ym_isapnp.c,v 1.4 1999/03/08 11:16:49 deraadt Exp $ */


/*
 * Copyright (c) 1998 Constantine Sapuntzakis. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Driver for the Yamaha OPL3-SA3 chipset. This is found on many laptops
 *  and Pentium (II) motherboards.
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

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/isa/ad1848var.h>

#include <dev/ic/cs4231reg.h>
#include <dev/isa/cs4231var.h>

#include <dev/isa/wssreg.h>
#include <dev/isa/ymvar.h>

int	ym_isapnp_match __P((struct device *, void *, void *));
void	ym_isapnp_attach __P((struct device *, struct device *, void *));

struct cfattach ym_isapnp_ca = {
	sizeof(struct ym_softc), ym_isapnp_match, ym_isapnp_attach
};


/*
 * Probe / attach routines.
 */

/*
 * Probe for the soundblaster hardware.
 */
int
ym_isapnp_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{

	return 1;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver.
 */
void
ym_isapnp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ym_softc *sc = (struct ym_softc *)self;
	struct isa_attach_args *ia = aux;

	if (ia->ipa_nio < 5) {
		printf ("Insufficient I/O ports... not really attached\n");
		return;
	}

	sc->sc_iot = ia->ia_iot;
	sc->sc_ioh = ia->ipa_io[1].h;
	sc->sc_ic = ia->ia_ic;

	sc->ym_irq = ia->ipa_irq[0].num;
	sc->ym_drq = ia->ipa_drq[0].num;
	sc->ym_recdrq = ia->ipa_drq[1].num;

	sc->sc_controlioh = ia->ipa_io[4].h; 

	sc->sc_ad1848.sc_ioh = sc->sc_ioh;
	sc->sc_ad1848.sc_isa = parent->dv_parent;
	sc->sc_ad1848.sc_iot = sc->sc_iot;
	sc->sc_ad1848.sc_iooffs = WSS_CODEC;
	sc->sc_ad1848.mode = 2;
	sc->sc_ad1848.MCE_bit = MODE_CHANGE_ENABLE;

	ym_attach(sc);
}

