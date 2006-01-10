/* $NetBSD: lunaws.c,v 1.6 2002/03/17 19:40:42 atatat Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tohru Nishimura.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include "wsmouse.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>
#include <dev/wscons/wsmousevar.h>

#include <luna88k/dev/sioreg.h>
#include <luna88k/dev/siovar.h>

static const u_int8_t ch1_regs[6] = {
	WR0_RSTINT,				/* Reset E/S Interrupt */
	WR1_RXALLS,				/* Rx per char, No Tx */
	0,					/* */
	WR3_RX8BIT | WR3_RXENBL,		/* Rx */
	WR4_BAUD96 | WR4_STOP1 | WR4_NPARITY,	/* Tx/Rx */
	WR5_TX8BIT | WR5_TXENBL,		/* Tx */
};

struct ws_softc {
	struct device	sc_dv;
	struct sioreg	*sc_ctl;
	u_int8_t	sc_wr[6];
	struct device	*sc_wskbddev;
#if NWSMOUSE > 0
	struct device	*sc_wsmousedev;
	int		sc_msreport;
	int		buttons, dx, dy;
#endif
};

void omkbd_input(void *, int);
void omkbd_decode(void *, int, u_int *, int *);
int  omkbd_enable(void *, int);
void omkbd_set_leds(void *, int);
int  omkbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

struct wscons_keydesc omkbd_keydesctab[];

const struct wskbd_mapdata omkbd_keymapdata = {
	omkbd_keydesctab,
#ifdef	OMKBD_LAYOUT
	OMKBD_LAYOUT,
#else
	KB_JP,
#endif
};

const struct wskbd_accessops omkbd_accessops = {
	omkbd_enable,
	omkbd_set_leds,
	omkbd_ioctl,
};

void	ws_cnattach(void);
void ws_cngetc(void *, u_int *, int *);
void ws_cnpollc(void *, int);
const struct wskbd_consops ws_consops = {
	ws_cngetc,
	ws_cnpollc,
	NULL	/* bell */
};

#if NWSMOUSE > 0
int  omms_enable(void *);
int  omms_ioctl(void *, u_long, caddr_t, int, struct proc *);
void omms_disable(void *);

const struct wsmouse_accessops omms_accessops = {
	omms_enable,
	omms_ioctl,
	omms_disable,
};
#endif

void wsintr(int);

int  wsmatch(struct device *, void *, void *);
void wsattach(struct device *, struct device *, void *);
int  ws_submatch_kbd(struct device *, void *, void *);
#if NWSMOUSE > 0
int  ws_submatch_mouse(struct device *, void *, void *);
#endif

const struct cfattach ws_ca = {
	sizeof(struct ws_softc), wsmatch, wsattach
};

struct cfdriver ws_cd = {
        NULL, "ws", DV_TTY
};

extern int  syscngetc(dev_t);
extern void syscnputc(dev_t, int);

int
wsmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct sio_attach_args *args = aux;

	if (args->channel != 1)
		return 0;
	return 1;
}

void
wsattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ws_softc *sc = (struct ws_softc *)self;
	struct sio_softc *scp = (struct sio_softc *)parent;
	struct sio_attach_args *args = aux;
	struct wskbddev_attach_args a;

	sc->sc_ctl = (struct sioreg *)scp->scp_ctl + 1;
	bcopy(ch1_regs, sc->sc_wr, sizeof(ch1_regs));
	scp->scp_intr[1] = wsintr;
	
	setsioreg(sc->sc_ctl, WR0, sc->sc_wr[WR0]);
	setsioreg(sc->sc_ctl, WR4, sc->sc_wr[WR4]);
	setsioreg(sc->sc_ctl, WR3, sc->sc_wr[WR3]);
	setsioreg(sc->sc_ctl, WR5, sc->sc_wr[WR5]);
	setsioreg(sc->sc_ctl, WR0, sc->sc_wr[WR0]);
	setsioreg(sc->sc_ctl, WR1, sc->sc_wr[WR1]);

	syscnputc((dev_t)1, 0x20); /* keep quiet mouse */

	printf("\n");

	a.console = (args->hwflags == 1);
	a.keymap = &omkbd_keymapdata;
	a.accessops = &omkbd_accessops;
	a.accesscookie = (void *)sc;
	sc->sc_wskbddev = config_found_sm(self, &a, wskbddevprint,
					ws_submatch_kbd);

#if NWSMOUSE > 0
	{
	struct wsmousedev_attach_args b;
	b.accessops = &omms_accessops;
	b.accesscookie = (void *)sc;	
	sc->sc_wsmousedev = config_found_sm(self, &b, wsmousedevprint,
					ws_submatch_mouse);
	sc->sc_msreport = 0;
	}
#endif
}

int
ws_submatch_kbd(parent, match, aux)
        struct device *parent;
        void *match, *aux;
{
	struct cfdata *cf = match;

        if (strcmp(cf->cf_driver->cd_name, "wskbd"))
                return (0);
        return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

#if NWSMOUSE > 0

int
ws_submatch_mouse(parent, match, aux)
        struct device *parent;
        void *match, *aux;
{
	struct cfdata *cf = match;

        if (strcmp(cf->cf_driver->cd_name, "wsmouse"))
                return (0);
        return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

#endif

/*ARGSUSED*/
void
wsintr(chan)
	int chan;
{
	struct ws_softc *sc = ws_cd.cd_devs[0];
	struct sioreg *sio = sc->sc_ctl;
	u_int code;
	int rr;

	rr = getsiocsr(sio);
	if (rr & RR_RXRDY) {
		do {
			code = sio->sio_data;
			if (rr & (RR_FRAMING | RR_OVERRUN | RR_PARITY)) {
				sio->sio_cmd = WR0_ERRRST;
				continue;
			}
#if NWSMOUSE > 0
			/*
			 * if (code >= 0x80 && code <= 0x87), then
			 * it's the first byte of 3 byte long mouse report
			 * 	code[0] & 07 -> LMR button condition
			 *	code[1], [2] -> x,y delta
			 * otherwise, key press or release event.
			 */
			if (sc->sc_msreport == 0) {
				if (code < 0x80 || code > 0x87) {
					omkbd_input(sc, code);
					continue;
				}
				code = (code & 07) ^ 07;
				/* LMR->RML: wsevent counts 0 for leftmost */
				sc->buttons = (code & 02);
				if (code & 01)
					sc->buttons |= 04;
				if (code & 04)
					sc->buttons |= 01;
				sc->sc_msreport = 1;
			}
			else if (sc->sc_msreport == 1) {
				sc->dx = (signed char)code;
				sc->sc_msreport = 2;
			}
			else if (sc->sc_msreport == 2) {
				sc->dy = (signed char)code;
				if (sc->sc_wsmousedev != NULL)
					wsmouse_input(sc->sc_wsmousedev,
					    sc->buttons, sc->dx, sc->dy, 0, 0);
				sc->sc_msreport = 0;
			}
#else
			omkbd_input(sc, code);
#endif
		} while ((rr = getsiocsr(sio)) & RR_RXRDY);
	}
	if (rr && RR_TXRDY)
		sio->sio_cmd = WR0_RSTPEND;
	/* not capable of transmit, yet */
}

void
omkbd_input(v, data)
	void *v;
	int data;
{
	struct ws_softc *sc = v;
	u_int type;
	int key;

	omkbd_decode(v, data, &type, &key);
	if (sc->sc_wskbddev != NULL)
		wskbd_input(sc->sc_wskbddev, type, key);	
}

void
omkbd_decode(v, datain, type, dataout)
	void *v;
	int datain;
	u_int *type;
	int *dataout;
{
	*type = (datain & 0x80) ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN;
	*dataout = datain & 0x7f;
}

#define KC(n) KS_KEYCODE(n)

static const keysym_t omkbd_keydesc_1[] = {
/*  pos      command		normal		shifted */
    KC(0x9), 			KS_Tab,
    KC(0xa),  			KS_Control_L,
    KC(0xb),			KS_Mode_switch,	/* Kana */
    KC(0xc),			KS_Shift_R,
    KC(0xd),			KS_Shift_L,
    KC(0xe),			KS_Caps_Lock,
    KC(0xf),			KS_Meta_L,	/* Zenmen */
    KC(0x10),			KS_Escape,
    KC(0x11),			KS_BackSpace,
    KC(0x12),			KS_Return,
    KC(0x14),			KS_space,
    KC(0x15),			KS_Delete,
    KC(0x16),			KS_Alt_L,	/* Henkan */
    KC(0x17),			KS_Alt_R,	/* Kakutei */
    KC(0x18),			KS_f11,		/* Shokyo */
    KC(0x19),			KS_f12,		/* Yobidashi */
    KC(0x1a),			KS_f13,		/* Bunsetsu L */
    KC(0x1b),			KS_f14,		/* Bunsetsu R */
    KC(0x1c),			KS_KP_Up,
    KC(0x1d),			KS_KP_Left,
    KC(0x1e),			KS_KP_Right,
    KC(0x1f),			KS_KP_Down,
 /* KC(0x20),			KS_f11, */
 /* KC(0x21),			KS_f12, */
    KC(0x22),  			KS_1,		KS_exclam,
    KC(0x23),			KS_2,		KS_quotedbl,
    KC(0x24),			KS_3,		KS_numbersign,
    KC(0x25),			KS_4,		KS_dollar,
    KC(0x26),			KS_5,		KS_percent,
    KC(0x27),			KS_6,		KS_ampersand,
    KC(0x28),			KS_7,		KS_apostrophe,
    KC(0x29),			KS_8,		KS_parenleft,
    KC(0x2a),			KS_9,		KS_parenright,
    KC(0x2b),			KS_0,
    KC(0x2c),			KS_minus,	KS_equal,
    KC(0x2d),			KS_asciicircum,	KS_asciitilde,
    KC(0x2e),			KS_backslash,	KS_bar,
 /* KC(0x30),			KS_f13, */
 /* KC(0x31),			KS_f14, */
    KC(0x32),			KS_q,
    KC(0x33),			KS_w,
    KC(0x34),			KS_e,
    KC(0x35),			KS_r,
    KC(0x36),			KS_t,
    KC(0x37),			KS_y,
    KC(0x38),			KS_u,
    KC(0x39),			KS_i,
    KC(0x3a),			KS_o,
    KC(0x3b),			KS_p,
    KC(0x3c),			KS_at,		KS_grave,
    KC(0x3d),			KS_bracketleft,	KS_braceleft,
    KC(0x42),			KS_a,
    KC(0x43),			KS_s,
    KC(0x44),			KS_d,
    KC(0x45),			KS_f,
    KC(0x46),			KS_g,
    KC(0x47),			KS_h,
    KC(0x48),			KS_j,
    KC(0x49),			KS_k,
    KC(0x4a),			KS_l,
    KC(0x4b),			KS_semicolon,	KS_plus,
    KC(0x4c),			KS_colon,	KS_asterisk,
    KC(0x4d),			KS_bracketright, KS_braceright,
    KC(0x52),			KS_z,
    KC(0x53),			KS_x,
    KC(0x54),			KS_c,
    KC(0x55),			KS_v,
    KC(0x56),			KS_b,
    KC(0x57),			KS_n,
    KC(0x58),			KS_m,
    KC(0x59),			KS_comma,	KS_less,
    KC(0x5a),			KS_period,	KS_greater,
    KC(0x5b),			KS_slash,	KS_question,
    KC(0x5c),			KS_underscore,
    KC(0x60),			KS_KP_Delete,
    KC(0x61),			KS_KP_Add,
    KC(0x62),			KS_KP_Subtract,
    KC(0x63),			KS_KP_7,
    KC(0x64),			KS_KP_8,
    KC(0x65),			KS_KP_9,
    KC(0x66),			KS_KP_4,
    KC(0x67),			KS_KP_5,
    KC(0x68),			KS_KP_6,
    KC(0x69),			KS_KP_1,
    KC(0x6a),			KS_KP_2,
    KC(0x6b),			KS_KP_3,
    KC(0x6c),			KS_KP_0,
    KC(0x6d),			KS_KP_Decimal,
    KC(0x6e),			KS_KP_Enter,
    KC(0x72),			KS_f1,
    KC(0x73),			KS_f2,
    KC(0x74),			KS_f3,
    KC(0x75),			KS_f4,
    KC(0x76),			KS_f5,
    KC(0x77),			KS_f6,
    KC(0x78),			KS_f7,
    KC(0x79),			KS_f8,
    KC(0x7a),			KS_f9,
    KC(0x7b),			KS_f10,
    KC(0x7c),			KS_KP_Multiply,
    KC(0x7d),			KS_KP_Divide,
    KC(0x7e),			KS_KP_Equal,
    KC(0x7f),			KS_KP_Separator,
};

#define	SIZE(map) (sizeof(map)/sizeof(keysym_t))

struct wscons_keydesc omkbd_keydesctab[] = {
	{ KB_JP, 0, SIZE(omkbd_keydesc_1), omkbd_keydesc_1, },
	{ 0, 0, 0, 0 },
};

void
ws_cngetc(v, type, data)
	void *v;
	u_int *type;
	int *data;
{
	int code;

	code = syscngetc((dev_t)1);
	omkbd_decode(v, code, type, data);
}

void
ws_cnpollc(v, on)
	void *v;
        int on;
{
}

/* EXPORT */ void
ws_cnattach()
{
	static int voidfill;

	/* XXX need CH.B initialization XXX */

	wskbd_cnattach(&ws_consops, &voidfill, &omkbd_keymapdata);
}

int
omkbd_enable(v, on)
	void *v;
	int on;
{
	return 0;
}

void
omkbd_set_leds(v, leds)
	void *v;
	int leds;
{
#if 0
	syscnputc((dev_t)1, 0x10); /* kana LED on */
	syscnputc((dev_t)1, 0x00); /* kana LED off */
	syscnputc((dev_t)1, 0x11); /* caps LED on */
	syscnputc((dev_t)1, 0x01); /* caps LED off */
#endif
}

int
omkbd_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
#if 0
	struct ws_softc *sc = v;
#endif

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_LUNA;
		return 0;
	case WSKBDIO_SETLEDS:
	case WSKBDIO_GETLEDS:
	case WSKBDIO_COMPLEXBELL:	/* XXX capable of complex bell */
		return -1;
	}
	return -1;
}

#if NWSMOUSE > 0

int
omms_enable(v)
	void *v;
{
	struct ws_softc *sc = v;

	syscnputc((dev_t)1, 0x60); /* enable 3 byte long mouse reporting */
	sc->sc_msreport = 0;
	return 0;
}

/*ARGUSED*/
int
omms_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
#if 0
	struct ws_softc *sc = v;
#endif

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_LUNA;
		return 0;
	}

	return -1;
}

void
omms_disable(v)
	void *v;
{
	struct ws_softc *sc = v;

	syscnputc((dev_t)1, 0x20); /* quiet mouse */
	sc->sc_msreport = 0;
}
#endif
