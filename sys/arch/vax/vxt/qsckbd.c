/*	$OpenBSD: qsckbd.c,v 1.1 2006/08/27 16:55:41 miod Exp $	*/
/*	from OpenBSD: dzkbd.c,v 1.11 2006/08/05 22:05:55 miod Exp */
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
 *	@(#)kbd.c	8.2 (Berkeley) 10/30/93
 */

/*
 * LK200/LK400 keyboard attached to line C of the SC26C94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/timeout.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#include <vax/dec/wskbdmap_lk201.h>

#include <machine/bus.h> 

#include <vax/dec/lk201reg.h>
#include <vax/dec/lk201var.h>

#include <vax/vxt/qscvar.h>

struct qsckbd_internal {
	u_int	dzi_line;
	struct lk201_state dzi_ks;
};

struct qsckbd_internal qsckbd_console_internal;

struct qsckbd_softc {
	struct device qsckbd_dev;	/* required first: base device */

	struct qsckbd_internal *sc_itl;
	int sc_enabled;
	struct device *sc_wskbddev;
};

int	qsckbd_match(struct device *, void *, void *);
void	qsckbd_attach(struct device *, struct device *, void *);

struct cfattach qsckbd_ca = {
	sizeof(struct qsckbd_softc), qsckbd_match, qsckbd_attach,
};

int	qsckbd_enable(void *, int);
void	qsckbd_set_leds(void *, int);
int	qsckbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wskbd_accessops qsckbd_accessops = {
	qsckbd_enable,
	qsckbd_set_leds,
	qsckbd_ioctl,
};

void	qsckbd_cngetc(void *, u_int *, int *);
void	qsckbd_cnpollc(void *, int);

const struct wskbd_consops qsckbd_consops = {
	qsckbd_cngetc,
	qsckbd_cnpollc,
};

const struct wskbd_mapdata qsckbd_keymapdata = {
	lkkbd_keydesctab,
#ifdef LKKBD_LAYOUT
	LKKBD_LAYOUT,
#else
	KB_US,
#endif
};

int	qsckbd_input(void *, int);
int	qsckbd_sendchar(void *, int);

int
qsckbd_match(struct device *parent, void *vcf, void *aux)
{
	struct qsc_attach_args *qa = aux;
	struct cfdata *cf = vcf;

	if (cf->cf_loc[0] == qa->qa_line)
		return 1;

	return 0;
}

void
qsckbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct qsckbd_softc *sc = (void *)self;
	struct qsc_attach_args *qa = aux;
	struct qsckbd_internal *dzi;
	struct wskbddev_attach_args a;
	int isconsole;

	qa->qa_hook->fn = qsckbd_input;
	qa->qa_hook->arg = self;

	isconsole = qa->qa_console;

	if (isconsole) {
		dzi = &qsckbd_console_internal;
		sc->sc_enabled = 1;
	} else {
		dzi = malloc(sizeof(struct qsckbd_internal), M_DEVBUF, M_NOWAIT);
		if (dzi == NULL) {
			printf(": out of memory\n");
			return;
		}
		dzi->dzi_ks.attmt.sendchar = qsckbd_sendchar;
		dzi->dzi_ks.attmt.cookie = (void *)qa->qa_line;
	}
	dzi->dzi_ks.device = self;
	dzi->dzi_line = qa->qa_line;
	sc->sc_itl = dzi;

	printf("\n");

	if (!isconsole)
		lk201_init(&dzi->dzi_ks);

	a.console = dzi == &qsckbd_console_internal;
	a.keymap = &qsckbd_keymapdata;
	a.accessops = &qsckbd_accessops;
	a.accesscookie = sc;

	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);
}

int
qsckbd_cnattach(u_int line)
{

	qsckbd_console_internal.dzi_ks.attmt.sendchar = qsckbd_sendchar;
	qsckbd_console_internal.dzi_ks.attmt.cookie = (void *)line;
	lk201_init(&qsckbd_console_internal.dzi_ks);
	qsckbd_console_internal.dzi_line = line;

	wskbd_cnattach(&qsckbd_consops, &qsckbd_console_internal,
	    &qsckbd_keymapdata);

	return 0;
}

int
qsckbd_enable(void *v, int on)
{
	struct qsckbd_softc *sc = v;

	sc->sc_enabled = on;
	return 0;
}

void
qsckbd_cngetc(void *v, u_int *type, int *data)
{
	struct qsckbd_internal *dzi = v;
	int c;

	do {
		c = qscgetc(dzi->dzi_line);
	} while (lk201_decode(&dzi->dzi_ks, 1, 0, c, type, data) == LKD_NODATA);
}

void
qsckbd_cnpollc(void *v, int on)
{
}

void
qsckbd_set_leds(void *v, int leds)
{
	struct qsckbd_softc *sc = (struct qsckbd_softc *)v;

	lk201_set_leds(&sc->sc_itl->dzi_ks, leds);
}

int
qsckbd_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct qsckbd_softc *sc = (struct qsckbd_softc *)v;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = lk201_get_type(&sc->sc_itl->dzi_ks);
		return 0;
	case WSKBDIO_SETLEDS:
		lk201_set_leds(&sc->sc_itl->dzi_ks, *(int *)data);
		return 0;
	case WSKBDIO_GETLEDS:
		*(int *)data = lk201_get_leds(&sc->sc_itl->dzi_ks);
		return 0;
	case WSKBDIO_COMPLEXBELL:
		lk201_bell(&sc->sc_itl->dzi_ks,
			   (struct wskbd_bell_data *)data);
		return 0;
	}
	return -1;
}

int
qsckbd_input(void *v, int data)
{
	struct qsckbd_softc *sc = (struct qsckbd_softc *)v;
	u_int type;
	int val;
	int decode;

	/*
	 * We want to run through lk201_decode always, so that a late plugged
	 * keyboard will get configured correctly.
	 */
	do {
		decode = lk201_decode(&sc->sc_itl->dzi_ks, sc->sc_enabled, 1,
		    data, &type, &val);
		if (decode != LKD_NODATA)
			wskbd_input(sc->sc_wskbddev, type, val);
	} while (decode == LKD_MORE);

	return(1);
}

int
qsckbd_sendchar(void *v, int c)
{
	qscputc((u_int)v, c);
	return (0);
}
