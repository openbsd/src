/*	$OpenBSD: qscms.c,v 1.2 2008/08/22 21:05:07 miod Exp $	*/
/*	from OpenBSD: qscms.c,v 1.6 2006/07/31 18:50:13 miod Exp	*/
/*
 * Copyright (c) 2006 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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
 * VSXXX mouse or tablet, attached to line D of the SC26C94
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

#include <vax/vxt/qscvar.h>
#include <vax/dec/vsmsvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

struct qscms_softc {
	struct lkms_softc	sc_base;
	u_int			sc_line;
};

int	qscms_match(struct device *, void *, void *);
void	qscms_attach(struct device *, struct device *, void *);

struct cfattach qscms_ca = {
	sizeof(struct qscms_softc), qscms_match, qscms_attach,
};

int	qscms_enable(void *);
void	qscms_disable(void *);

const struct wsmouse_accessops qscms_accessops = {
	qscms_enable,
	lkms_ioctl,
	qscms_disable,
};

int
qscms_match(struct device *parent, void *vcf, void *aux)
{
	struct qsc_attach_args *qa = aux;
	struct cfdata *cf = vcf;

	if (cf->cf_loc[0] == qa->qa_line)
		return 1;

	return 0;
}

void
qscms_attach(struct device *parent, struct device *self, void *aux)
{
	struct qscms_softc *qscms = (void *)self;
	struct lkms_softc *sc = (void *)self;
	struct qsc_attach_args *qa = aux;
	struct wsmousedev_attach_args a;

	qa->qa_hook->fn = lkms_input;
	qa->qa_hook->arg = self;
	qscms->sc_line = qa->qa_line;

	printf("\n");

	a.accessops = &qscms_accessops;
	a.accesscookie = qscms;

	sc->sc_flags = 0;
	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
}

int
qscms_enable(void *v)
{
	struct qscms_softc *qscms = v;
	struct lkms_softc *sc = v;

	if (ISSET(sc->sc_flags, MS_ENABLED))
		return EBUSY;

	SET(sc->sc_flags, MS_SELFTEST);
	qscputc(qscms->sc_line, VS_SELF_TEST);
	/* selftest is supposed to complete within 500ms */
	(void)tsleep(&sc->sc_flags, TTIPRI, "qscmsopen", hz / 2);
	if (ISSET(sc->sc_flags, MS_SELFTEST)) {
		CLR(sc->sc_flags, MS_SELFTEST);
		return ENXIO;
	}
	DELAY(150);
	qscputc(qscms->sc_line, VS_INCREMENTAL);
	SET(sc->sc_flags, MS_ENABLED);
	return 0;
}

void
qscms_disable(void *v)
{
	struct lkms_softc *sc = v;

	CLR(sc->sc_flags, MS_ENABLED);
}
