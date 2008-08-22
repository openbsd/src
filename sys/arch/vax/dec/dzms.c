/*	$OpenBSD: dzms.c,v 1.8 2008/08/22 21:05:07 miod Exp $	*/
/*	$NetBSD: dzms.c,v 1.1 2000/12/02 17:03:55 ragge Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
 */

/*
 * VSXXX mouse or tablet, attached to line 1 of the DZ*
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/tty.h>

#include <machine/bus.h>

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>

#include <vax/dec/dzkbdvar.h>
#include <vax/dec/vsmsvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

struct dzms_softc {		/* driver status information */
	struct lkms_softc	sc_base;
	struct dz_linestate *dzms_ls;
};

int	dzms_match(struct device *, struct cfdata *, void *);
void	dzms_attach(struct device *, struct device *, void *);

struct cfattach dzms_ca = {
	sizeof(struct dzms_softc), (cfmatch_t)dzms_match, dzms_attach,
};

int	dzms_enable(void *);
void	dzms_disable(void *);

const struct wsmouse_accessops dzms_accessops = {
	dzms_enable,
	lkms_ioctl,
	dzms_disable,
};

int
dzms_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct dzkm_attach_args *daa = aux;

#define DZCF_LINE 0
#define DZCF_LINE_DEFAULT 1

	/* Exact match is better than wildcard. */
	if (cf->cf_loc[DZCF_LINE] == daa->daa_line)
		return 2;

	/* This driver accepts wildcard. */
	if (cf->cf_loc[DZCF_LINE] == DZCF_LINE_DEFAULT)
		return 1;

	return 0;
}

void
dzms_attach(struct device *parent, struct device *self, void *aux)
{
	struct dz_softc *dz = (void *)parent;
	struct dzms_softc *dzms = (void *)self;
	struct lkms_softc *sc = (void *)self;
	struct dzkm_attach_args *daa = aux;
	struct dz_linestate *ls;
	struct wsmousedev_attach_args a;

	dz->sc_dz[daa->daa_line].dz_catch = lkms_input;
	dz->sc_dz[daa->daa_line].dz_private = dzms;
	ls = &dz->sc_dz[daa->daa_line];
	dzms->dzms_ls = ls;

	printf("\n");

	a.accessops = &dzms_accessops;
	a.accesscookie = dzms;

	sc->sc_flags = 0;
	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
}

int
dzms_enable(void *v)
{
	struct dzms_softc *dzms = v;
	struct lkms_softc *sc = v;

	if (ISSET(sc->sc_flags, MS_ENABLED))
		return EBUSY;

	SET(sc->sc_flags, MS_SELFTEST);
	dzputc(dzms->dzms_ls, VS_SELF_TEST);
	/* selftest is supposed to complete within 500ms */
	(void)tsleep(&sc->sc_flags, TTIPRI, "dzmsopen", hz / 2);
	if (ISSET(sc->sc_flags, MS_SELFTEST)) {
		CLR(sc->sc_flags, MS_SELFTEST);
		return ENXIO;
	}
	DELAY(150);
	dzputc(dzms->dzms_ls, VS_INCREMENTAL);
	SET(sc->sc_flags, MS_ENABLED);
	return 0;
}

void
dzms_disable(void *v)
{
	struct lkms_softc *sc = v;

	CLR(sc->sc_flags, MS_ENABLED);
}
