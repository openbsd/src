/*	$OpenBSD: gsckbd.c,v 1.3 2003/02/15 23:42:45 miod Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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

/*
 * Derived from /sys/dev/pckbc/pckbd.c under the following terms:
 * OpenBSD: pckbd.c,v 1.4 2002/03/14 01:27:00 millert Exp
 * NetBSD: pckbd.c,v 1.24 2000/06/05 22:20:57 sommerfeld Exp
 */
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)pccons.c	5.11 (Berkeley) 5/21/91
 */

/*
 * A pckbd-like driver for the GSC keyboards found on various HP workstations.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>

#include <machine/bus.h>

#include <dev/ic/pckbcvar.h>

#include <dev/pckbc/pckbdreg.h>
#include <hppa/gsc/gsckbdvar.h>
#include <hppa/gsc/gsckbdmap.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

struct gsckbd_internal {
	int t_isconsole;
	pckbc_tag_t t_kbctag;
	pckbc_slot_t t_kbcslot;

	int t_lastchar;
	int t_extended;
	int t_releasing;
	int t_extended1;

	struct gsckbd_softc *t_sc; /* back pointer */
};

struct gsckbd_softc {
        struct  device sc_dev;

	struct gsckbd_internal *id;
	int sc_enabled;

	int sc_ledstate;

	struct device *sc_wskbddev;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int rawkbd;
#endif
};

int gsckbd_is_console(pckbc_tag_t, pckbc_slot_t);

int gsckbdprobe(struct device *, void *, void *);
void gsckbdattach(struct device *, struct device *, void *);

struct cfdriver gsckbd_cd = {
	NULL, "gsckbd", DV_DULL
};

struct cfattach gsckbd_ca = {
	sizeof(struct gsckbd_softc), gsckbdprobe, gsckbdattach,
};

int	gsckbd_enable(void *, int);
void	gsckbd_set_leds(void *, int);
int	gsckbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

const struct wskbd_accessops gsckbd_accessops = {
	gsckbd_enable,
	gsckbd_set_leds,
	gsckbd_ioctl,
};

void	gsckbd_cngetc(void *, u_int *, int *);
void	gsckbd_cnpollc(void *, int);

const struct wskbd_consops gsckbd_consops = {
	gsckbd_cngetc,
	gsckbd_cnpollc,
	NULL
};

const struct wskbd_mapdata gsckbd_keymapdata = {
	gsckbd_keydesctab,	/* XXX */
#ifdef GSCKBD_LAYOUT
	GSCKBD_LAYOUT,
#else
	KB_US,
#endif
};

int	gsckbd_init(struct gsckbd_internal *, pckbc_tag_t, pckbc_slot_t,
			int);
void	gsckbd_input(void *, int);

static int	gsckbd_decode(struct gsckbd_internal *, int,
				  u_int *, int *);
static int	gsckbd_led_encode(int);
static int	gsckbd_led_decode(int);

struct gsckbd_internal gsckbd_consdata;

int
gsckbd_is_console(tag, slot)
	pckbc_tag_t tag;
	pckbc_slot_t slot;
{
	return (gsckbd_consdata.t_isconsole &&
		(tag == gsckbd_consdata.t_kbctag) &&
		(slot == gsckbd_consdata.t_kbcslot));
}

/*
 * these are both EXTREMELY bad jokes
 */
int
gsckbdprobe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct cfdata *cf = match;
	struct pckbc_attach_args *pa = aux;
	u_char cmd[1], resp[1];
	int res;

	/*
	 * XXX There are rumours that a keyboard can be connected
	 * to the aux port as well. For me, this didn't work.
	 * For further experiments, allow it if explicitly
	 * wired in the config file.
	 */
	if ((pa->pa_slot != PCKBC_KBD_SLOT) &&
	    (cf->cf_loc[PCKBCCF_SLOT] == PCKBCCF_SLOT_DEFAULT))
		return (0);

	/* Flush any garbage. */
	pckbc_flush(pa->pa_tag, pa->pa_slot);

	/* Reset the keyboard. */
	cmd[0] = KBC_RESET;
	res = pckbc_poll_cmd(pa->pa_tag, pa->pa_slot, cmd, 1, 1, resp, 1);
	if (res) {
#ifdef DEBUG
		printf("gsckbdprobe: reset error %d\n", res);
#endif
		/*
		 * There is probably no keyboard connected.
		 * Let the probe succeed if the keyboard is used
		 * as console input - it can be connected later.
		 */
		return (gsckbd_is_console(pa->pa_tag, pa->pa_slot) ? 1 : 0);
	}
	if (resp[0] != KBR_RSTDONE) {
		printf("gsckbdprobe: reset response 0x%x\n", resp[0]);
		return (0);
	}

	/*
	 * Some keyboards seem to leave a second ack byte after the reset.
	 * This is kind of stupid, but we account for them anyway by just
	 * flushing the buffer.
	 */
	pckbc_flush(pa->pa_tag, pa->pa_slot);

	return (2);
}

void
gsckbdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct gsckbd_softc *sc = (void *)self;
	struct pckbc_attach_args *pa = aux;
	int isconsole;
	struct wskbddev_attach_args a;

	printf("\n");

	isconsole = gsckbd_is_console(pa->pa_tag, pa->pa_slot);

	if (isconsole) {
		sc->id = &gsckbd_consdata;
		sc->sc_enabled = 1;
	} else {
		u_char cmd[1];

		sc->id = malloc(sizeof(struct gsckbd_internal),
				M_DEVBUF, M_WAITOK);
		(void) gsckbd_init(sc->id, pa->pa_tag, pa->pa_slot, 0);

		/* no interrupts until enabled */
		cmd[0] = KBC_DISABLE;
		(void) pckbc_poll_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
				      cmd, 1, 0, 0, 0);
		sc->sc_enabled = 0;
	}

	sc->id->t_sc = sc;

	pckbc_set_inputhandler(sc->id->t_kbctag, sc->id->t_kbcslot,
			       gsckbd_input, sc, sc->sc_dev.dv_xname);

	a.console = isconsole;

	a.keymap = &gsckbd_keymapdata;

	a.accessops = &gsckbd_accessops;
	a.accesscookie = sc;

	/*
	 * Attach the wskbd, saving a handle to it.
	 * XXX XXX XXX
	 */
	sc->sc_wskbddev = config_found(self, &a, wskbddevprint);
}

int
gsckbd_enable(v, on)
	void *v;
	int on;
{
	struct gsckbd_softc *sc = v;
	u_char cmd[1];
	int res;

	if (on) {
		if (sc->sc_enabled && !sc->id->t_isconsole)
			return (EBUSY);

		pckbc_slot_enable(sc->id->t_kbctag, sc->id->t_kbcslot, 1);

		cmd[0] = KBC_ENABLE;
		res = pckbc_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
					cmd, 1, 0, 1, 0);
		if (res) {
			printf("gsckbd_enable: command error\n");
			return (res);
		}

		sc->sc_enabled = 1;
	} else {
		if (sc->id->t_isconsole)
			return (EBUSY);

		cmd[0] = KBC_DISABLE;
		res = pckbc_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
					cmd, 1, 0, 1, 0);
		if (res) {
			printf("gsckbd_disable: command error\n");
			return (res);
		}

		pckbc_slot_enable(sc->id->t_kbctag, sc->id->t_kbcslot, 0);

		sc->sc_enabled = 0;
	}

	return (0);
}

static int
gsckbd_decode(id, datain, type, dataout)
	struct gsckbd_internal *id;
	int datain;
	u_int *type;
	int *dataout;
{
	int key;

	if (datain == KBR_BREAK) {
		id->t_releasing = 1;	/* next keycode is a release */
		return 0;
	}

	if (datain == KBR_EXTENDED0) {
		id->t_extended = 0x80;
		return 0;
	} else if (datain == KBR_EXTENDED1) {
		id->t_extended1 = 2;
		return 0;
	}

 	/*
	 * Map extended keys to codes 128-254
	 * Note that we do not use (datain & 0x7f) because function key
	 * F7 produces non-extended 0x83 code. Sucker.
	 */
	key = datain | id->t_extended;
	id->t_extended = 0;

	/*
	 * process BREAK sequence (EXT1 14 77):
	 * map to (unused) code 7F
	 */
	if (id->t_extended1 == 2 && datain == 0x14) {
		id->t_extended1 = 1;
		return 0;
	} else if (id->t_extended1 == 1 && datain == 0x77) {
		id->t_extended1 = 0;
		key = 0x7f;
	} else if (id->t_extended1 > 0) {
		id->t_extended1 = 0;
	}

	if (id->t_releasing) {
		id->t_releasing = 0;
		*type = WSCONS_EVENT_KEY_UP;
		*dataout = key;
		id->t_lastchar = 0;
	} else {
		/* Always ignore typematic keys */
		if (key == id->t_lastchar)
			return 0;
		*dataout = id->t_lastchar = key;
		*type = WSCONS_EVENT_KEY_DOWN;
	}

	return 1;
}

int
gsckbd_init(t, kbctag, kbcslot, console)
	struct gsckbd_internal *t;
	pckbc_tag_t kbctag;
	pckbc_slot_t kbcslot;
	int console;
{
	bzero(t, sizeof(struct gsckbd_internal));

	t->t_isconsole = console;
	t->t_kbctag = kbctag;
	t->t_kbcslot = kbcslot;

	return (0);
}

static int
gsckbd_led_encode(led)
	int led;
{
	int res;

	res = 0;

	if (led & WSKBD_LED_SCROLL)
		res |= 0x01;
	if (led & WSKBD_LED_NUM)
		res |= 0x02;
	if (led & WSKBD_LED_CAPS)
		res |= 0x04;
	return(res);
}

static int
gsckbd_led_decode(led)
	int led;
{
	int res;

	res = 0;
	if (led & 0x01)
		res |= WSKBD_LED_SCROLL;
	if (led & 0x02)
		res |= WSKBD_LED_NUM;
	if (led & 0x04)
		res |= WSKBD_LED_CAPS;
	return(res);
}

void
gsckbd_set_leds(v, leds)
	void *v;
	int leds;
{
	struct gsckbd_softc *sc = v;
	u_char cmd[2];

	cmd[0] = KBC_MODEIND;
	cmd[1] = gsckbd_led_encode(leds);
	sc->sc_ledstate = cmd[1];

	pckbc_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
	    cmd, 2, 0, 0, 0);
}

/*
 * Got a console receive interrupt -
 * the console processor wants to give us a character.
 */
void
gsckbd_input(vsc, data)
	void *vsc;
	int data;
{
	struct gsckbd_softc *sc = vsc;
	int type, key;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->rawkbd) {
		char d = data;
		wskbd_rawinput(sc->sc_wskbddev, &d, 1);
		return;
	}
#endif
	if (gsckbd_decode(sc->id, data, &type, &key))
		wskbd_input(sc->sc_wskbddev, type, key);
}

int
gsckbd_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct gsckbd_softc *sc = v;

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_PC_XT;
		return 0;
	case WSKBDIO_SETLEDS:
	{
		char cmd[2];
		int res;
		cmd[0] = KBC_MODEIND;
		cmd[1] = gsckbd_led_encode(*(int *)data);
		sc->sc_ledstate = cmd[1];
		res = pckbc_enqueue_cmd(sc->id->t_kbctag, sc->id->t_kbcslot,
		    cmd, 2, 0, 1, 0);
		return (res);
	}
	case WSKBDIO_GETLEDS:
		*(int *)data = gsckbd_led_decode(sc->sc_ledstate);
		return (0);
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->rawkbd = (*(int *)data == WSKBD_RAW);
		return (0);
#endif
	}
	return -1;
}

int
gsckbd_cnattach(kbctag, kbcslot)
	pckbc_tag_t kbctag;
	int kbcslot;
{
	char cmd[1];
	int res;

	res = gsckbd_init(&gsckbd_consdata, kbctag, kbcslot, 1);
#if 0 /* we allow the console to be attached if no keyboard is present */
	if (res)
		return (res);
#endif

	/* Just to be sure. */
	cmd[0] = KBC_ENABLE;
	res = pckbc_poll_cmd(kbctag, kbcslot, cmd, 1, 0, 0, 0);
#if 0
	if (res)
		return (res);
#endif

	wskbd_cnattach(&gsckbd_consops, &gsckbd_consdata, &gsckbd_keymapdata);

	return (0);
}

/* ARGSUSED */
void
gsckbd_cngetc(v, type, data)
	void *v;
	u_int *type;
	int *data;
{
        struct gsckbd_internal *t = v;
	int val;

	for (;;) {
		val = pckbc_poll_data(t->t_kbctag, t->t_kbcslot);
		if ((val != -1) && gsckbd_decode(t, val, type, data))
			return;
	}
}

void
gsckbd_cnpollc(v, on)
	void *v;
        int on;
{
	struct gsckbd_internal *t = v;

	pckbc_set_poll(t->t_kbctag, t->t_kbcslot, on);
}
