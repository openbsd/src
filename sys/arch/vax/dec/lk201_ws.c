/*	$OpenBSD: lk201_ws.c,v 1.10 2006/08/27 16:50:43 miod Exp $	*/
/* $NetBSD: lk201_ws.c,v 1.2 1998/10/22 17:55:20 drochner Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
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
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/timeout.h>

#include <dev/wscons/wsconsio.h>

#include <vax/dec/lk201reg.h>
#include <vax/dec/lk201var.h>
#include <vax/dec/wskbdmap_lk201.h> /* for {MIN,MAX}_LK201_KEY */

struct	cfdriver lkkbd_cd = {
	NULL, "lkkbd", DV_DULL
};

void	lk201_identify(void *);

static const char *lkkbd_descr[] = {
	"no keyboard",
	"LK-201 keyboard",
	"LK-401 keyboard"
};

#define	send(lks, c) ((*((lks)->attmt.sendchar))((lks)->attmt.cookie, c))

void
lk201_init(struct lk201_state *lks)
{
	int i;

	lks->waitack = 0;

	send(lks, LK_LED_ENABLE);
	send(lks, LK_LED_ALL);

	/*
	 * set all keys to updown mode; autorepeat is
	 * done by wskbd software
	 */
	for (i = 1; i <= 14; i++)
		send(lks, LK_CMD_MODE(LK_UPDOWN, i));

	send(lks, LK_CL_DISABLE);
	lks->kcvol = 0;

	lks->bellvol = -1; /* not yet set */

	for (i = 0; i < LK_KLL; i++)
		lks->down_keys_list[i] = -1;
	send(lks, LK_KBD_ENABLE);

	send(lks, LK_LED_DISABLE);
	send(lks, LK_LED_ALL);
	lks->leds_state = 0;

	/*
	 * Note that, when attaching lkkbd initially, this timeout will
	 * be scheduled but will not run until interrupts are enabled.
	 * This is not a problem, since lk201_identify() relies upon
	 * interrupts being enabled.
	 */
	timeout_set(&lks->probetmo, lk201_identify, lks);
	timeout_add(&lks->probetmo, 0);
}

void
lk201_identify(void *v)
{
	struct lk201_state *lks = v;
	int i;

	/*
	 * Swallow all the keyboard acknowledges from lk201_init().
	 * There should be 14 of them - one per LK_CMD_MODE command.
	 */
	for(;;) {
		lks->waitack = 1;
		for (i = 100; i != 0; i--) {
			DELAY(1000);
			if (lks->waitack == 0)
				break;
		}
		if (i == 0)
			break;
	}

	/*
	 * Try to set the keyboard in LK-401 mode.
	 * If we receive an error, this is an LK-201 keyboard.
	 */
	lks->waitack = 1;
	send(lks, LK_ENABLE_401);
	for (i = 100; i != 0; i--) {
		DELAY(1000);
		if (lks->waitack == 0)
			break;
	}
	if (lks->waitack != 0)
		lks->kbdtype = KBD_NONE;
	else {
		if (lks->ackdata == LK_INPUT_ERROR)
			lks->kbdtype = KBD_LK201;
		else
			lks->kbdtype = KBD_LK401;
	}
	lks->waitack = 0;

	printf("%s: %s\n", lks->device->dv_xname, lkkbd_descr[lks->kbdtype]);
}

int
lk201_decode(struct lk201_state *lks, int active, int wantmulti, int datain,
    u_int *type, int *dataout)
{
	int i, freeslot;

	if (lks->waitack != 0) {
		lks->ackdata = datain;
		lks->waitack = 0;
		return (LKD_NODATA);
	}

	switch (datain) {
	case LK_POWER_UP:
#ifdef DEBUG
		printf("lk201_decode: powerup detected\n");
#endif
		lk201_init(lks);
		return (LKD_NODATA);
	case LK_KDOWN_ERROR:
	case LK_POWER_ERROR:
	case LK_OUTPUT_ERROR:
	case LK_INPUT_ERROR:
		printf("lk201_decode: error %x\n", datain);
		/* FALLTHROUGH */
	case LK_KEY_REPEAT: /* autorepeat handled by wskbd */
	case LK_MODE_CHANGE: /* ignore silently */
		return (LKD_NODATA);
	}

	if (active == 0)
		return (LKD_NODATA);	/* no need to decode */

	if (datain == LK_KEY_UP) {
		if (wantmulti) {
			for (i = 0; i < LK_KLL; i++)
				if (lks->down_keys_list[i] != -1) {
					*type = WSCONS_EVENT_KEY_UP;
					*dataout = lks->down_keys_list[i] -
					    MIN_LK201_KEY;
					lks->down_keys_list[i] = -1;
					return (LKD_MORE);
				}
			return (LKD_NODATA);
		} else {
			for (i = 0; i < LK_KLL; i++)
				lks->down_keys_list[i] = -1;
			*type = WSCONS_EVENT_ALL_KEYS_UP;
			return (LKD_COMPLETE);
		}
	} else if (datain < MIN_LK201_KEY || datain > MAX_LK201_KEY) {
#ifdef DEBUG
		/* this can happen while hotplugging the keyboard */
		printf("lk201_decode: %x\n", datain);
#endif
		return (LKD_NODATA);
	}

	/*
	 * The LK-201 keyboard has a compose key (to the left of the spacebar),
	 * but no alt/meta key at all. The LK-401 keyboard fixes this and has
	 * two compose keys and two alt keys.
	 *
	 * If the keyboard is an LK-201, translate the left compose key
	 * scancode to a specific key code, which will map as a left alt key,
	 * and compose key when shifted), so that the user can have both
	 * an alt and a compose key available.
	 */
	if (lks->kbdtype == KBD_LK201 && datain == 177)
		datain = 252;

	*dataout = datain - MIN_LK201_KEY;

	freeslot = -1;
	for (i = 0; i < LK_KLL; i++) {
		if (lks->down_keys_list[i] == datain) {
			*type = WSCONS_EVENT_KEY_UP;
			lks->down_keys_list[i] = -1;
			return (LKD_COMPLETE);
		}
		if (lks->down_keys_list[i] == -1 && freeslot == -1)
			freeslot = i;
	}

	if (freeslot == -1) {
		printf("lk201_decode: down(%d) no free slot\n", datain);
		return (LKD_NODATA);
	}

	*type = WSCONS_EVENT_KEY_DOWN;
	lks->down_keys_list[freeslot] = datain;
	return (LKD_COMPLETE);
}

void
lk201_bell(struct lk201_state *lks, struct wskbd_bell_data *bell)
{
	unsigned int vol;

	if (bell->which & WSKBD_BELL_DOVOLUME) {
		vol = 8 - bell->volume * 8 / 100;
		if (vol > 7)
			vol = 7;
	} else
		vol = 3;

	if (vol != lks->bellvol) {
		send(lks, LK_BELL_ENABLE);
		send(lks, LK_PARAM_VOLUME(vol));
		lks->bellvol = vol;
	}
	send(lks, LK_RING_BELL);
}

int
lk201_get_leds(struct lk201_state *lks)
{
	return (lks->leds_state);
}

int
lk201_get_type(struct lk201_state *lks)
{
	/*
	 * Note that we report LK201 even if no keyboard is
	 * plugged to avoid confusing wsconsctl.
	 */
	if (lks->kbdtype == KBD_LK401)
		return (WSKBD_TYPE_LK401);
	else
		return (WSKBD_TYPE_LK201);
}

void
lk201_set_keyclick(struct lk201_state *lks, int vol)
{
	unsigned int newvol;

	if (vol == 0)
		send(lks, LK_CL_DISABLE);
	else {
		newvol = 8 - vol * 8 / 100;
		if (newvol > 7)
			newvol = 7;

		send(lks, LK_CL_ENABLE);
		send(lks, LK_PARAM_VOLUME(newvol));
	}

	lks->kcvol = vol;
}

void
lk201_set_leds(struct lk201_state *lks, int leds)
{
	int newleds;

	newleds = 0;
	if (leds & WSKBD_LED_SCROLL)
		newleds |= LK_LED_WAIT;
	if (leds & WSKBD_LED_CAPS)
		newleds |= LK_LED_LOCK;

	send(lks, LK_LED_DISABLE);
	send(lks, (0x80 | (~newleds & 0x0f)));

	send(lks, LK_LED_ENABLE);
	send(lks, (0x80 | (newleds & 0x0f)));

	lks->leds_state = leds;
}
