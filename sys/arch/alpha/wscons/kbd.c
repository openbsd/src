/*	$NetBSD: kbd.c,v 1.1 1996/04/12 02:00:46 cgd Exp $ */

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
 *	@(#)kbd.c	8.2 (Berkeley) 10/30/93
 */

/*
 * Keyboard driver (/dev/kbd -- note that we do not have minor numbers
 * [yet?]).  Translates incoming bytes to ASCII or to `firm_events' and
 * passes them up to the appropriate reader.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/tty.h>

#include <machine/autoconf.h>

#include <machine/vuid_event.h>
#include <machine/kbio.h>			/* XXX FOR KIOCSDIRECT */
#include <machine/wsconsio.h>			/* XXX for bell ioctls */
#include <alpha/wscons/event_var.h>
#include <alpha/wscons/wsconsvar.h>

struct kbd_softc {
	struct device *k_idev;		/* the input device */
	struct wscons_idev_spec k_ispec; /* the input device information */

	int	k_evmode;		/* set if we should produce events */
	struct	evvar k_events;		/* event queue state */
	char	*k_repeatcp;		/* repeated character (string) */
	int	k_repeating;		/* we've called timeout() */
	int	k_repeat_start;		/* how long (ms) until repeat */
	int	k_repeat_step;		/* how long (ms) until more repeats */

	struct wsconsio_bell_data k_belldata;
} kbd_softc;

void
kbdattach(idev, ispec)
	struct device *idev;
	struct wscons_idev_spec *ispec;
{
	register struct kbd_softc *k = &kbd_softc;

	/*
	 * It would be nice if the repeat rates were in ticks.
	 * However, if they were, we couldn't set them here, as
	 * hz might not be set up yet!
	 */
	k->k_repeat_start = 200;
	k->k_repeat_step = 50; 

	k->k_belldata.wbd_pitch = 1500;	/* 1500 Hz */
	k->k_belldata.wbd_period = 100;	/* 100 ms */
	k->k_belldata.wbd_volume = 50;		/* 50% volume */

	k->k_idev = idev;
	k->k_ispec = *ispec;
}

void
kbd_repeat(void *arg)
{
	struct kbd_softc *k = (struct kbd_softc *)arg;
	int s = spltty();

	if (k->k_repeating && k->k_repeatcp != NULL) {
		wscons_input(k->k_repeatcp);
		timeout(kbd_repeat, k, (hz * k->k_repeat_step) / 1000);
	}
	splx(s);
}

void
kbd_input(register int c)
{
	register struct kbd_softc *k = &kbd_softc;
	register struct firm_event *fe;
	register int put;
	char *cp;

	if (k->k_repeating) {
		k->k_repeating = 0;
		untimeout(kbd_repeat, k);
	}

	/*
	 * If /dev/kbd is not connected in event mode translate and
	 * send upstream.
	 */
	if (!k->k_evmode) {
		cp = (*k->k_ispec.wi_translate)(k->k_idev, c);
		if (cp != NULL) {
			wscons_input(cp);
			k->k_repeating = 1;
			k->k_repeatcp = cp;
			timeout(kbd_repeat, k, (hz * k->k_repeat_start) / 1000);
		}
		return;
	}

	/*
	 * Keyboard is generating events.  Turn this keystroke into an
	 * event and put it in the queue.  If the queue is full, the
	 * keystroke is lost (sorry!).
	 */
	put = k->k_events.ev_put;
	fe = &k->k_events.ev_q[put];
	put = (put + 1) % EV_QSIZE;
	if (put == k->k_events.ev_get) {
		log(LOG_WARNING, "keyboard event queue overflow\n"); /* ??? */
		return;
	}
	fe->id = c & k->k_ispec.wi_keymask;
	fe->value = (c & k->k_ispec.wi_keyupmask) != 0 ? VKEY_UP : VKEY_DOWN;
	microtime(&fe->time);
	k->k_events.ev_put = put;
	EV_WAKEUP(&k->k_events);
}

int
kbdopen(dev_t dev, int flags, int mode, struct proc *p)
{
	int s;
	struct tty *tp;

	if (kbd_softc.k_events.ev_io)
		return (EBUSY);
	kbd_softc.k_events.ev_io = p;
	ev_init(&kbd_softc.k_events);
	kbd_softc.k_evmode = 1;
	return (0);
}

int
kbdclose(dev_t dev, int flags, int mode, struct proc *p)
{

	/*
	 * Turn off event mode, dump the queue, and close the keyboard
	 * unless it is supplying console input.
	 */
	kbd_softc.k_evmode = 0;
	ev_fini(&kbd_softc.k_events);
	kbd_softc.k_events.ev_io = NULL;
	return (0);
}

int
kbdread(dev_t dev, struct uio *uio, int flags)
{

	return (ev_read(&kbd_softc.k_events, uio, flags));
}

/* this routine should not exist, but is convenient to write here for now */
int
kbdwrite(dev_t dev, struct uio *uio, int flags)
{

	return (EOPNOTSUPP);
}

int
kbdioctl(dev_t dev, u_long cmd, register caddr_t data, int flag, struct proc *p)
{
	struct kbd_softc *k = &kbd_softc;
	struct wsconsio_bell_data *wbd;
	int rv;

	rv = ENOTTY;
	switch (cmd) {
#if 0
	case KIOCSDIRECT:
		k->k_evmode = *(int *)data;
		return (0);
#endif

	case FIONBIO:		/* we will remove this someday (soon???) */
		return (0);

	case FIOASYNC:
		k->k_events.ev_async = *(int *)data != 0;
		return (0);

	case TIOCSPGRP:
		if (*(int *)data != k->k_events.ev_io->p_pgid)
			return (EPERM);
		return (0);

	case WSCONSIO_BELL:
		if (k->k_ispec.wi_bell != NULL)
			(*k->k_ispec.wi_bell)(k->k_idev, &k->k_belldata);
		return (0);

	case WSCONSIO_COMPLEXBELL:
		if (k->k_ispec.wi_bell != NULL) {
			wbd = (struct wsconsio_bell_data *)data;
			if ((wbd->wbd_flags & WSCONSIO_BELLDATA_PITCH) == 0)
				wbd->wbd_pitch = k->k_belldata.wbd_pitch;
			if ((wbd->wbd_flags & WSCONSIO_BELLDATA_PERIOD) == 0)
				wbd->wbd_period = k->k_belldata.wbd_period;
			if ((wbd->wbd_flags & WSCONSIO_BELLDATA_VOLUME) == 0)
				wbd->wbd_volume = k->k_belldata.wbd_volume;

			(*k->k_ispec.wi_bell)(k->k_idev, wbd);
		}
		return (0);

	case WSCONSIO_SETBELL:
		wbd = (struct wsconsio_bell_data *)data;
		if ((wbd->wbd_flags & WSCONSIO_BELLDATA_PITCH) != 0)
			k->k_belldata.wbd_pitch = wbd->wbd_pitch;
		if ((wbd->wbd_flags & WSCONSIO_BELLDATA_PERIOD) != 0)
			k->k_belldata.wbd_period = wbd->wbd_period;
		if ((wbd->wbd_flags & WSCONSIO_BELLDATA_VOLUME) != 0)
			k->k_belldata.wbd_volume = wbd->wbd_volume;
		return (0);

	case WSCONSIO_GETBELL:
		wbd = (struct wsconsio_bell_data *)data;
		wbd->wbd_flags = WSCONSIO_BELLDATA_PITCH |
		    WSCONSIO_BELLDATA_PERIOD | WSCONSIO_BELLDATA_VOLUME;
		wbd->wbd_pitch = k->k_belldata.wbd_pitch;
		wbd->wbd_period = k->k_belldata.wbd_period;
		wbd->wbd_volume = k->k_belldata.wbd_volume ;
		return (0);

#if 0 /* XXX */
	/* XXX KEY-REPEAT RATE SETTING */
#endif /* XXX */

	default:
		if (k->k_ispec.wi_ioctl != NULL)
			rv = (*k->k_ispec.wi_ioctl)(k->k_idev, cmd, data,
			    flag, p);
	}
	return (rv);
}

int
kbdselect(dev_t dev, int rw, struct proc *p)
{

	return (ev_select(&kbd_softc.k_events, rw, p));
}

/* Ring the console bell.  (For wscons terminal emulator and other code) */
void
wscons_kbd_bell()
{
	struct kbd_softc *k = &kbd_softc;

	if (k->k_ispec.wi_bell != NULL)
		(*k->k_ispec.wi_bell)(k->k_idev, &k->k_belldata);
}

/*
 * Console handling functions.
 */

int
kbd_cngetc(dev)
	dev_t dev;
{
	struct kbd_softc *k = &kbd_softc;

	if (kbd_softc.k_evmode)				/* XXX? */
		return 0;
	if (k->k_ispec.wi_getc != NULL)
		return (*k->k_ispec.wi_getc)(k->k_idev);
	else
		return 0;
}

void
kbd_cnpollc(dev, on)
	dev_t dev;
	int on;
{
	struct kbd_softc *k = &kbd_softc;

	if (kbd_softc.k_evmode)				/* XXX? */
		return;
	if (k->k_ispec.wi_pollc != NULL)
		(*k->k_ispec.wi_pollc)(k->k_idev, on);
}
