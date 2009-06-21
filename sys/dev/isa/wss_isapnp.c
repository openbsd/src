/*	$OpenBSD: wss_isapnp.c,v 1.8 2009/06/21 00:38:22 martynas Exp $	*/
/*	$NetBSD: wss_isapnp.c,v 1.5 1998/11/25 22:17:07 augustss Exp $	*/

/*
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
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
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/isapnpreg.h>

#include <dev/isa/ad1848var.h>
#include <dev/ic/ad1848reg.h>
#include <dev/isa/madreg.h>
#include <dev/isa/wssreg.h>
#include <dev/isa/wssvar.h>

int	wss_isapnp_match(struct device *, void *, void *);
void	wss_isapnp_attach(struct device *, struct device *, void *);

struct cfattach wss_isapnp_ca = {
	sizeof(struct wss_softc), wss_isapnp_match, wss_isapnp_attach
};


/*
 * Probe / attach routines.
 */

/*
 * Probe for the WSS hardware.
 */
int
wss_isapnp_match(parent, match, aux)
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
wss_isapnp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isapnp_softc *pnp = (struct isapnp_softc *)parent;
	struct wss_softc *sc = (struct wss_softc *)self;
	struct ad1848_softc *ac = &sc->sc_ad1848;
	struct isa_attach_args *ipa = aux;

	/* probably broken */
	isapnp_write_reg(pnp, ISAPNP_CONFIG_CONTROL, 0x02);

	sc->sc_iot = ipa->ia_iot;
	sc->sc_ioh = ipa->ipa_io[0].h;
	sc->mad_chip_type = MAD_NONE;

/* Set up AD1848 I/O handle. */ 
	ac->sc_iot = sc->sc_iot;
	ac->sc_isa = parent->dv_parent; 
	ac->sc_ioh = sc->sc_ioh;
	ac->mode = 2;
	ac->sc_iooffs = 0;

	sc->sc_ic  = ipa->ia_ic;
	sc->wss_irq = ipa->ipa_irq[0].num;
	sc->wss_drq = ipa->ipa_drq[0].num;
	sc->wss_recdrq = ipa->ipa_ndrq > 1 ? ipa->ipa_drq[1].num :
	    ipa->ipa_drq[0].num;

	if (ad1848_probe(&sc->sc_ad1848)==0) {
		printf("%s: probe failed\n", ac->sc_dev.dv_xname);
		return;
	}

	wssattach(sc);
}

