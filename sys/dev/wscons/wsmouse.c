/* $OpenBSD: wsmouse.c,v 1.30 2016/06/06 22:32:47 bru Exp $ */
/* $NetBSD: wsmouse.c,v 1.35 2005/02/27 00:27:52 perry Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Copyright (c) 2015, 2016 Ulf Brosziewski
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
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
 * Mouse driver.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/device.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/malloc.h>

#include <dev/wscons/wscons_features.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>
#include <dev/wscons/wsmouseinput.h>
#include <dev/wscons/wseventvar.h>
#include <dev/rndvar.h>

#include "wsmux.h"
#include "wsdisplay.h"
#include "wskbd.h"

#include <dev/wscons/wsmuxvar.h>

#if defined(WSMUX_DEBUG) && NWSMUX > 0
#define	DPRINTF(x)	if (wsmuxdebug) printf x
#define	DPRINTFN(n,x)	if (wsmuxdebug > (n)) printf x
extern int wsmuxdebug;
#else
#define	DPRINTF(x)
#define	DPRINTFN(n,x)
#endif

struct wsmouse_softc {
	struct wsevsrc	sc_base;

	const struct wsmouse_accessops *sc_accessops;
	void		*sc_accesscookie;

	struct wsmouseinput input;

	int		sc_refcnt;
	u_char		sc_dying;	/* device is being detached */
};

int	wsmouse_match(struct device *, void *, void *);
void	wsmouse_attach(struct device *, struct device *, void *);
int	wsmouse_detach(struct device *, int);
int	wsmouse_activate(struct device *, int);

int	wsmouse_do_ioctl(struct wsmouse_softc *, u_long, caddr_t, 
			      int, struct proc *);

#if NWSMUX > 0
int	wsmouse_mux_open(struct wsevsrc *, struct wseventvar *);
int	wsmouse_mux_close(struct wsevsrc *);
#endif

int	wsmousedoioctl(struct device *, u_long, caddr_t, int, 
			    struct proc *);
int	wsmousedoopen(struct wsmouse_softc *, struct wseventvar *);

struct cfdriver wsmouse_cd = {
	NULL, "wsmouse", DV_TTY
};

struct cfattach wsmouse_ca = {
	sizeof (struct wsmouse_softc), wsmouse_match, wsmouse_attach,
	wsmouse_detach, wsmouse_activate
};

#if NWSMUX > 0
struct wssrcops wsmouse_srcops = {
	WSMUX_MOUSE,
	wsmouse_mux_open, wsmouse_mux_close, wsmousedoioctl, NULL, NULL
};
#endif

/*
 * Print function (for parent devices).
 */
int
wsmousedevprint(void *aux, const char *pnp)
{

	if (pnp)
		printf("wsmouse at %s", pnp);
	return (UNCONF);
}

int
wsmouse_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
wsmouse_attach(struct device *parent, struct device *self, void *aux)
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)self;
	struct wsmousedev_attach_args *ap = aux;
#if NWSMUX > 0
	int mux, error;
#endif

	sc->sc_accessops = ap->accessops;
	sc->sc_accesscookie = ap->accesscookie;

	wsmouse_input_init(&sc->input, &sc->sc_base.me_evp);

#if NWSMUX > 0
	sc->sc_base.me_ops = &wsmouse_srcops;
	mux = sc->sc_base.me_dv.dv_cfdata->wsmousedevcf_mux;
	if (mux >= 0) {
		error = wsmux_attach_sc(wsmux_getmux(mux), &sc->sc_base);
		if (error)
			printf(" attach error=%d", error);
		else
			printf(" mux %d", mux);
	}
#else
#if 0	/* not worth keeping, especially since the default value is not -1... */
	if (sc->sc_base.me_dv.dv_cfdata->wsmousedevcf_mux >= 0)
		printf(" (mux ignored)");
#endif
#endif	/* NWSMUX > 0 */

	printf("\n");
}

int
wsmouse_activate(struct device *self, int act)
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)self;

	if (act == DVACT_DEACTIVATE)
		sc->sc_dying = 1;
	return (0);
}

/*
 * Detach a mouse.  To keep track of users of the softc we keep
 * a reference count that's incremented while inside, e.g., read.
 * If the mouse is active and the reference count is > 0 (0 is the
 * normal state) we post an event and then wait for the process
 * that had the reference to wake us up again.  Then we blow away the
 * vnode and return (which will deallocate the softc).
 */
int
wsmouse_detach(struct device *self, int flags)
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)self;
	struct wseventvar *evar;
	int maj, mn;
	int s;

#if NWSMUX > 0
	/* Tell parent mux we're leaving. */
	if (sc->sc_base.me_parent != NULL) {
		DPRINTF(("wsmouse_detach:\n"));
		wsmux_detach_sc(&sc->sc_base);
	}
#endif

	/* If we're open ... */
	evar = sc->sc_base.me_evp;
	if (evar != NULL && evar->io != NULL) {
		s = spltty();
		if (--sc->sc_refcnt >= 0) {
			/* Wake everyone by generating a dummy event. */
			if (++evar->put >= WSEVENT_QSIZE)
				evar->put = 0;
			WSEVENT_WAKEUP(evar);
			/* Wait for processes to go away. */
			if (tsleep(sc, PZERO, "wsmdet", hz * 60))
				printf("wsmouse_detach: %s didn't detach\n",
				       sc->sc_base.me_dv.dv_xname);
		}
		splx(s);
	}

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == wsmouseopen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	wsmouse_input_cleanup(&sc->input);

	return (0);
}

int
wsmouseopen(dev_t dev, int flags, int mode, struct proc *p)
{
	struct wsmouse_softc *sc;
	struct wseventvar *evar;
	int error, unit;

	unit = minor(dev);
	if (unit >= wsmouse_cd.cd_ndevs ||	/* make sure it was attached */
	    (sc = wsmouse_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

#if NWSMUX > 0
	DPRINTF(("wsmouseopen: %s mux=%p p=%p\n", sc->sc_base.me_dv.dv_xname,
		 sc->sc_base.me_parent, p));
#endif

	if (sc->sc_dying)
		return (EIO);

	if ((flags & (FREAD | FWRITE)) == FWRITE)
		return (0);			/* always allow open for write
						   so ioctl() is possible. */

#if NWSMUX > 0
	if (sc->sc_base.me_parent != NULL) {
		/* Grab the mouse out of the greedy hands of the mux. */
		DPRINTF(("wsmouseopen: detach\n"));
		wsmux_detach_sc(&sc->sc_base);
	}
#endif

	if (sc->sc_base.me_evp != NULL)
		return (EBUSY);

	evar = &sc->sc_base.me_evar;
	wsevent_init(evar);
	evar->io = p->p_p;

	error = wsmousedoopen(sc, evar);
	if (error) {
		DPRINTF(("wsmouseopen: %s open failed\n",
			 sc->sc_base.me_dv.dv_xname));
		sc->sc_base.me_evp = NULL;
		wsevent_fini(evar);
	}
	return (error);
}

int
wsmouseclose(dev_t dev, int flags, int mode, struct proc *p)
{
	struct wsmouse_softc *sc =
	    (struct wsmouse_softc *)wsmouse_cd.cd_devs[minor(dev)];
	struct wseventvar *evar = sc->sc_base.me_evp;

	if ((flags & (FREAD | FWRITE)) == FWRITE)
		return (0);			/* see wsmouseopen() */

	if (evar == NULL)
		/* not open for read */
		return (0);
	sc->sc_base.me_evp = NULL;
	(*sc->sc_accessops->disable)(sc->sc_accesscookie);
	wsevent_fini(evar);

#if NWSMUX > 0
	if (sc->sc_base.me_parent == NULL) {
		int mux, error;

		DPRINTF(("wsmouseclose: attach\n"));
		mux = sc->sc_base.me_dv.dv_cfdata->wsmousedevcf_mux;
		if (mux >= 0) {
			error = wsmux_attach_sc(wsmux_getmux(mux), &sc->sc_base);
			if (error)
				printf("%s: can't attach mux (error=%d)\n",
				    sc->sc_base.me_dv.dv_xname, error);
		}
	}
#endif

	return (0);
}

int
wsmousedoopen(struct wsmouse_softc *sc, struct wseventvar *evp)
{
	sc->sc_base.me_evp = evp;

	/* enable the device, and punt if that's not possible */
	return (*sc->sc_accessops->enable)(sc->sc_accesscookie);
}

int
wsmouseread(dev_t dev, struct uio *uio, int flags)
{
	struct wsmouse_softc *sc = wsmouse_cd.cd_devs[minor(dev)];
	int error;

	if (sc->sc_dying)
		return (EIO);

#ifdef DIAGNOSTIC
	if (sc->sc_base.me_evp == NULL) {
		printf("wsmouseread: evp == NULL\n");
		return (EINVAL);
	}
#endif

	sc->sc_refcnt++;
	error = wsevent_read(sc->sc_base.me_evp, uio, flags);
	if (--sc->sc_refcnt < 0) {
		wakeup(sc);
		error = EIO;
	}
	return (error);
}

int
wsmouseioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	return (wsmousedoioctl(wsmouse_cd.cd_devs[minor(dev)],
	    cmd, data, flag, p));
}

/* A wrapper around the ioctl() workhorse to make reference counting easy. */
int
wsmousedoioctl(struct device *dv, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)dv;
	int error;

	sc->sc_refcnt++;
	error = wsmouse_do_ioctl(sc, cmd, data, flag, p);
	if (--sc->sc_refcnt < 0)
		wakeup(sc);
	return (error);
}

int
wsmouse_do_ioctl(struct wsmouse_softc *sc, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
	int error;

	if (sc->sc_dying)
		return (EIO);

	/*
	 * Try the generic ioctls that the wsmouse interface supports.
	 */

	switch (cmd) {
	case FIOASYNC:
	case FIOSETOWN:
	case TIOCSPGRP:
		if ((flag & FWRITE) == 0)
			return (EACCES);
	}

	switch (cmd) {
	case FIONBIO:		/* we will remove this someday (soon???) */
		return (0);

	case FIOASYNC:
		if (sc->sc_base.me_evp == NULL)
			return (EINVAL);
		sc->sc_base.me_evp->async = *(int *)data != 0;
		return (0);

	case FIOSETOWN:
		if (sc->sc_base.me_evp == NULL)
			return (EINVAL);
		if (-*(int *)data != sc->sc_base.me_evp->io->ps_pgid
		    && *(int *)data != sc->sc_base.me_evp->io->ps_pid)
			return (EPERM);
		return (0);

	case TIOCSPGRP:
		if (sc->sc_base.me_evp == NULL)
			return (EINVAL);
		if (*(int *)data != sc->sc_base.me_evp->io->ps_pgid)
			return (EPERM);
		return (0);
	}

	/*
	 * Try the mouse driver for WSMOUSEIO ioctls.  It returns -1
	 * if it didn't recognize the request.
	 */
	error = (*sc->sc_accessops->ioctl)(sc->sc_accesscookie, cmd,
	    data, flag, p);
	return (error != -1 ? error : ENOTTY);
}

int
wsmousepoll(dev_t dev, int events, struct proc *p)
{
	struct wsmouse_softc *sc = wsmouse_cd.cd_devs[minor(dev)];

	if (sc->sc_base.me_evp == NULL)
		return (POLLERR);
	return (wsevent_poll(sc->sc_base.me_evp, events, p));
}

int
wsmousekqfilter(dev_t dev, struct knote *kn)
{
	struct wsmouse_softc *sc = wsmouse_cd.cd_devs[minor(dev)];

	if (sc->sc_base.me_evp == NULL)
		return (ENXIO);
	return (wsevent_kqfilter(sc->sc_base.me_evp, kn));
}

#if NWSMUX > 0
int
wsmouse_mux_open(struct wsevsrc *me, struct wseventvar *evp)
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)me;

	if (sc->sc_base.me_evp != NULL)
		return (EBUSY);

	return wsmousedoopen(sc, evp);
}

int
wsmouse_mux_close(struct wsevsrc *me)
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)me;

	sc->sc_base.me_evp = NULL;
	(*sc->sc_accessops->disable)(sc->sc_accesscookie);

	return (0);
}

int
wsmouse_add_mux(int unit, struct wsmux_softc *muxsc)
{
	struct wsmouse_softc *sc;

	if (unit < 0 || unit >= wsmouse_cd.cd_ndevs ||
	    (sc = wsmouse_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	if (sc->sc_base.me_parent != NULL || sc->sc_base.me_evp != NULL)
		return (EBUSY);

	return (wsmux_attach_sc(muxsc, &sc->sc_base));
}
#endif	/* NWSMUX > 0 */

void
wsmouse_buttons(struct device *sc, u_int buttons)
{
	struct btn_state *btn =
	    &((struct wsmouse_softc *) sc)->input.btn;

	if (btn->sync)
		/* Restore the old state. */
		btn->buttons ^= btn->sync;

	btn->sync = btn->buttons ^ buttons;
	btn->buttons = buttons;
}

void
wsmouse_motion(struct device *sc, int dx, int dy, int dz, int dw)
{
	struct motion_state *motion =
	    &((struct wsmouse_softc *) sc)->input.motion;

	motion->dx = dx;
	motion->dy = dy;
	motion->dz = dz;
	motion->dw = dw;
	if (dx || dy || dz || dw)
		motion->sync |= SYNC_DELTAS;
}

/*
 * Handle absolute coordinates.
 *
 * x_delta/y_delta are used by touchpad code. The values are only
 * valid if the SYNC-flags are set, and will be cleared by update- or
 * conversion-functions if a touch shouldn't trigger pointer motion.
 */
void
wsmouse_position(struct device *sc, int x, int y)
{
	struct motion_state *motion =
	    &((struct wsmouse_softc *) sc)->input.motion;
	int delta;

	delta = x - motion->x;
	if (delta) {
		motion->x = x;
		motion->sync |= SYNC_X;
		motion->x_delta = delta;
	}
	delta = y - motion->y;
	if (delta) {
		motion->y = y;
		motion->sync |= SYNC_Y;
		motion->y_delta = delta;
	}
}

static __inline int
normalized_pressure(struct wsmouseinput *input, int pressure)
{
	int limit = imax(input->touch.min_pressure, 1);

	if (pressure >= limit)
		return pressure;
	else
		return (pressure < 0 ? limit : 0);
}

void
wsmouse_touch(struct device *sc, int pressure, int contacts)
{
	struct wsmouseinput *input = &((struct wsmouse_softc *) sc)->input;
	struct touch_state *touch = &input->touch;

	pressure = normalized_pressure(input, pressure);
	contacts = (pressure ? imax(contacts, 1) : 0);

	if (pressure == 0 || pressure != touch->pressure) {
		/*
		 * pressure == 0: Drivers may report possibly arbitrary
		 * coordinates in this case; touch_update will correct them.
		 */
		touch->pressure = pressure;
		touch->sync |= SYNC_PRESSURE;
	}
	if (contacts != touch->contacts) {
		touch->contacts = contacts;
		touch->sync |= SYNC_CONTACTS;
	}
}

void
wsmouse_mtstate(struct device *sc, int slot, int x, int y, int pressure)
{
	struct wsmouseinput *input = &((struct wsmouse_softc *) sc)->input;
	struct mt_state *mt = &input->mt;
	struct mt_slot *mts;
	u_int bit;
	int initial;

	if (slot < 0 || slot >= mt->num_slots)
		return;

	bit = (1 << slot);
	mt->frame |= bit;

	/* Is this a new touch? */
	initial = ((mt->touches & bit) == (mt->sync[MTS_TOUCH] & bit));

	mts = &mt->slots[slot];
	if (x != mts->x || initial) {
		mts->x = x;
		mt->sync[MTS_X] |= bit;
	}
	if (y != mts->y || initial) {
		mts->y = y;
		mt->sync[MTS_Y] |= bit;
	}
	pressure = normalized_pressure(input, pressure);
	if (pressure != mts->pressure || initial) {
		mts->pressure = pressure;
		mt->sync[MTS_PRESSURE] |= bit;

		if (pressure) {
			if ((mt->touches & bit) == 0) {
				mt->num_touches++;
				mt->touches |= bit;
				mt->sync[MTS_TOUCH] |= bit;
			}
		} else if (mt->touches & bit) {
			mt->num_touches--;
			mt->touches ^= bit;
			mt->sync[MTS_TOUCH] |= bit;
		}
	}
}

void
wsmouse_set(struct device *sc, enum wsmouseval type, int value, int aux)
{
	struct wsmouseinput *input = &((struct wsmouse_softc *) sc)->input;
	struct mt_slot *mts;

	if (WSMOUSE_IS_MT_CODE(type)) {
		if (aux < 0 || aux >= input->mt.num_slots)
			return;
		mts = &input->mt.slots[aux];
	}

	switch (type) {
	case WSMOUSE_REL_X:
		value += input->motion.x; /* fall through */
	case WSMOUSE_ABS_X:
		wsmouse_position(sc, value, input->motion.y);
		return;
	case WSMOUSE_REL_Y:
		value += input->motion.y;
	case WSMOUSE_ABS_Y:
		wsmouse_position(sc, input->motion.x, value);
		return;
	case WSMOUSE_PRESSURE:
		wsmouse_touch(sc, value, input->touch.contacts);
		return;
	case WSMOUSE_CONTACTS:
		/* Contact counts can be overridden by wsmouse_touch. */
		if (value != input->touch.contacts) {
			input->touch.contacts = value;
			input->touch.sync |= SYNC_CONTACTS;
		}
		return;
	case WSMOUSE_TOUCH_WIDTH:
		if (value != input->touch.width) {
			input->touch.width = value;
			input->touch.sync |= SYNC_TOUCH_WIDTH;
		}
		return;
	case WSMOUSE_MT_REL_X:
		value += mts->x; /* fall through */
	case WSMOUSE_MT_ABS_X:
		wsmouse_mtstate(sc, aux, value, mts->y, mts->pressure);
		return;
	case WSMOUSE_MT_REL_Y:
		value += mts->y;
	case WSMOUSE_MT_ABS_Y:
		wsmouse_mtstate(sc, aux, mts->x, value, mts->pressure);
		return;
	case WSMOUSE_MT_PRESSURE:
		wsmouse_mtstate(sc, aux, mts->x, mts->y, value);
		return;
	}
}

/* Make touch and motion state consistent. */
void
wsmouse_touch_update(struct wsmouseinput *input)
{
	struct motion_state *motion = &input->motion;
	struct touch_state *touch = &input->touch;

	if (touch->pressure == 0) {
		/* Restore valid coordinates. */
		if (motion->sync & SYNC_X)
			motion->x -= motion->x_delta;
		if (motion->sync & SYNC_Y)
			motion->y -= motion->y_delta;
		/* Don't generate motion/position events. */
		motion->sync &= ~SYNC_POSITION;
	}
	if (touch->sync & SYNC_CONTACTS)
		/* Suppress pointer motion. */
		motion->x_delta = motion->y_delta = 0;

	if ((touch->sync & SYNC_PRESSURE) && touch->min_pressure) {
		if (touch->pressure >= input->params.pressure_hi)
			touch->min_pressure = input->params.pressure_lo;
		else if (touch->pressure < input->params.pressure_lo)
			touch->min_pressure = input->params.pressure_hi;
	}
}

/* Normalize multitouch state. */
void
wsmouse_mt_update(struct wsmouseinput *input)
{
	int i;

	/*
	 * The same as above: There may be arbitrary coordinates if
	 * (pressure == 0). Clear the sync flags for touches that have
	 * been released.
	 */
	if (input->mt.sync[MTS_TOUCH] & ~input->mt.touches) {
		for (i = MTS_X; i < MTS_SIZE; i++)
			input->mt.sync[i] &= input->mt.touches;
	}
}

/*
 * Select the pointer-controlling MT slot.
 *
 * Pointer-control is assigned to slots with non-zero motion deltas if
 * at least one such slot exists. This function doesn't impose any
 * restrictions on the way drivers use wsmouse_mtstate(), it covers
 * partial, unordered, and "delta-filtered" input.
 *
 * The "cycle" is the set of slots with X/Y updates in previous sync
 * operations; it will be cleared and rebuilt whenever a slot that is
 * being updated is already a member. If a cycle ends that doesn't
 * contain the pointer-controlling slot, a new slot will be selected.
 */
void
wsmouse_ptr_ctrl(struct mt_state *mt)
{
	u_int updates;
	int select, slot;

	mt->prev_ptr = mt->ptr;

	if (mt->num_touches <= 1) {
		mt->ptr = mt->touches;
		mt->ptr_cycle = mt->ptr;
		return;
	}

	/*
	 * If there is no pointer-controlling slot or it is inactive,
	 * select a new one.
	 */
	select = ((mt->ptr & mt->touches) == 0);

	/* Remove slots without X/Y deltas from the cycle. */
	updates = (mt->sync[MTS_X] | mt->sync[MTS_Y]) & ~mt->sync[MTS_TOUCH];
	mt->ptr_cycle &= ~(mt->frame ^ updates);

	if (mt->ptr_cycle & updates) {
		select |= ((mt->ptr_cycle & mt->ptr) == 0);
		mt->ptr_cycle = updates;
	} else {
		mt->ptr_cycle |= updates;
	}
	if (select) {
		slot = (mt->ptr_cycle
		    ? ffs(mt->ptr_cycle) - 1 : ffs(mt->touches) - 1);
		mt->ptr = (1 << slot);
	}
}

/* Derive touch and motion state from MT state. */
void
wsmouse_mt_convert(struct device *sc)
{
	struct wsmouseinput *input = &((struct wsmouse_softc *) sc)->input;
	struct mt_state *mt = &input->mt;
	struct mt_slot *mts;
	int slot, pressure;

	wsmouse_ptr_ctrl(mt);

	if (mt->ptr) {
		slot = ffs(mt->ptr) - 1;
		mts = &mt->slots[slot];
		wsmouse_position(sc, mts->x, mts->y);
		if (mt->ptr != mt->prev_ptr)
			/* Suppress pointer motion. */
			input->motion.x_delta = input->motion.y_delta = 0;
		pressure = mts->pressure;
	} else {
		pressure = 0;
	}

	wsmouse_touch(sc, pressure, mt->num_touches);
}

void
wsmouse_evq_put(struct evq_access *evq, int ev_type, int ev_value)
{
	struct wscons_event *ev;
	int space;

	space = evq->evar->get - evq->put;
	if (space != 1 && space != 1 - WSEVENT_QSIZE) {
		ev = &evq->evar->q[evq->put++];
		evq->put %= WSEVENT_QSIZE;
		ev->type = ev_type;
		ev->value = ev_value;
		memcpy(&ev->time, &evq->ts, sizeof(struct timespec));
		evq->result |= EVQ_RESULT_SUCCESS;
	} else {
		evq->result = EVQ_RESULT_OVERFLOW;
	}
}


void
wsmouse_btn_sync(struct wsmouseinput *input, struct evq_access *evq)
{
	struct btn_state *btn = &input->btn;
	int button, ev_type;
	u_int bit, sync;

	for (sync = btn->sync; sync; sync ^= bit) {
		button = ffs(sync) - 1;
		bit = (1 << button);
		ev_type = (btn->buttons & bit) ? BTN_DOWN_EV : BTN_UP_EV;
		wsmouse_evq_put(evq, ev_type, button);
	}
}

/*
 * Scale with a [*.12] fixed-point factor and a remainder:
 */
static __inline int
scale(int val, int factor, int *rmdr)
{
	val = val * factor + *rmdr;
	if (val >= 0) {
		*rmdr = val & 0xfff;
		return (val >> 12);
	} else {
		*rmdr = -(-val & 0xfff);
		return -(-val >> 12);
	}
}

void
wsmouse_motion_sync(struct wsmouseinput *input, struct evq_access *evq)
{
	struct motion_state *motion = &input->motion;
	struct wsmouseparams *params = &input->params;
	struct axis_filter *fltr;
	int x, y, dx, dy;

	if (motion->sync & SYNC_DELTAS) {
		dx = params->x_inv ? -motion->dx : motion->dx;
		dy = params->y_inv ? -motion->dy : motion->dy;
		if (input->flags & SCALE_DELTAS) {
			fltr = &input->fltr.h;
			dx = scale(dx, fltr->scale, &fltr->rmdr);
			fltr = &input->fltr.v;
			dy = scale(dy, fltr->scale, &fltr->rmdr);
		}
		if (dx)
			wsmouse_evq_put(evq, DELTA_X_EV(input->flags), dx);
		if (dy)
			wsmouse_evq_put(evq, DELTA_Y_EV(input->flags), dy);
		if (motion->dz)
			wsmouse_evq_put(evq, DELTA_Z_EV, motion->dz);
		if (motion->dw)
			wsmouse_evq_put(evq, DELTA_W_EV, motion->dw);
	}
	if (motion->sync & SYNC_POSITION) {
		if (motion->sync & SYNC_X) {
			x = (params->x_inv
			    ? params->x_inv - motion->x : motion->x);
			wsmouse_evq_put(evq, ABS_X_EV(input->flags), x);
		}
		if (motion->sync & SYNC_Y) {
			y = (params->y_inv
			    ? params->y_inv - motion->y : motion->y);
			wsmouse_evq_put(evq, ABS_Y_EV(input->flags), y);
		}
		if (motion->x_delta == 0 && motion->y_delta == 0
		    && (input->flags & TPAD_NATIVE_MODE))
			/* Suppress pointer motion. */
			wsmouse_evq_put(evq, WSCONS_EVENT_TOUCH_RESET, 0);
	}
}

void
wsmouse_touch_sync(struct wsmouseinput *input, struct evq_access *evq)
{
	struct touch_state *touch = &input->touch;

	if (touch->sync & SYNC_PRESSURE)
		wsmouse_evq_put(evq, ABS_Z_EV, touch->pressure);
	if (touch->sync & SYNC_CONTACTS)
		wsmouse_evq_put(evq, ABS_W_EV, touch->contacts);
	if ((touch->sync & SYNC_TOUCH_WIDTH)
	    && (input->flags & TPAD_NATIVE_MODE))
		wsmouse_evq_put(evq, WSCONS_EVENT_TOUCH_WIDTH, touch->width);
}

/*
 * Convert absolute touchpad input (compatibility mode).
 */
void
wsmouse_compat_convert(struct device *sc, struct evq_access *evq)
{
	struct wsmouseinput *input = &((struct wsmouse_softc *) sc)->input;
	struct wsmouseparams *params = &input->params;
	int dx, dy, dz, dw;

	dx = (input->motion.sync & SYNC_X) ? input->motion.x_delta : 0;
	dy = (input->motion.sync & SYNC_Y) ? input->motion.y_delta : 0;
	dz = (input->motion.sync & SYNC_DELTAS) ? input->motion.dz : 0;
	dw = (input->motion.sync & SYNC_DELTAS) ? input->motion.dw : 0;

	if ((params->dx_max && abs(dx) > params->dx_max)
	    || (params->dy_max && abs(dy) > params->dy_max)) {

		dx = dy = 0;
	}

	wsmouse_motion(sc, dx, dy, dz, dw);

	input->motion.sync &= ~SYNC_POSITION;
	input->touch.sync = 0;
}

static __inline void
clear_sync_flags(struct wsmouseinput *input)
{
	int i;

	input->btn.sync = 0;
	input->motion.sync = 0;
	input->touch.sync = 0;
	if (input->mt.frame) {
		input->mt.frame = 0;
		for (i = 0; i < MTS_SIZE; i++)
			input->mt.sync[i] = 0;
	}
}

void
wsmouse_input_sync(struct device *sc)
{
	struct wsmouseinput *input = &((struct wsmouse_softc *) sc)->input;
	struct evq_access evq;

	evq.evar = *input->evar;
	if (evq.evar == NULL)
		return;
	evq.put = evq.evar->put;
	evq.result = EVQ_RESULT_NONE;
	getnanotime(&evq.ts);

	add_mouse_randomness(input->btn.buttons
	    ^ input->motion.dx ^ input->motion.dy
	    ^ input->motion.x ^ input->motion.y
	    ^ input->motion.dz ^ input->motion.dw);

	if (input->mt.frame) {
		wsmouse_mt_update(input);
		wsmouse_mt_convert(sc);
	}
	if (input->touch.sync)
		wsmouse_touch_update(input);

	if (input->flags & TPAD_COMPAT_MODE)
		wsmouse_compat_convert(sc, &evq);

	if (input->flags & RESYNC) {
		input->flags &= ~RESYNC;
		input->motion.sync &= SYNC_POSITION;
		input->motion.x_delta = input->motion.y_delta = 0;
	}

	if (input->btn.sync)
		wsmouse_btn_sync(input, &evq);
	if (input->motion.sync)
		wsmouse_motion_sync(input, &evq);
	if (input->touch.sync)
		wsmouse_touch_sync(input, &evq);
	/* No MT events are generated yet. */

	if (evq.result == EVQ_RESULT_SUCCESS) {
		wsmouse_evq_put(&evq, WSCONS_EVENT_SYNC, 0);
		if (evq.result == EVQ_RESULT_SUCCESS) {
			evq.evar->put = evq.put;
			WSEVENT_WAKEUP(evq.evar);
		}
	}

	if (evq.result != EVQ_RESULT_OVERFLOW)
		clear_sync_flags(input);
	else
		input->flags |= RESYNC;
}

int
wsmouse_id_to_slot(struct device *sc, int id)
{
	struct wsmouseinput *input = &((struct wsmouse_softc *) sc)->input;
	struct mt_state *mt = &input->mt;
	int slot;

	if (mt->num_slots == 0)
		return (-1);

	FOREACHBIT(mt->touches, slot) {
		if (mt->slots[slot].id == id)
			return slot;
	}
	slot = ffs(~(mt->touches | mt->frame)) - 1;
	if (slot >= 0 && slot < mt->num_slots) {
		mt->frame |= 1 << slot;
		mt->slots[slot].id = id;
		return (slot);
	} else {
		return (-1);
	}
}

/*
 * Find a minimum-weight matching for an m-by-n matrix.
 *
 * m must be greater than or equal to n. The size of the buffer must be
 * at least 4m + 3n.
 *
 * On return, the first m elements of the buffer contain the row-to-
 * column mappings, i.e., buffer[i] is the column index for row i, or -1
 * if there is no assignment for that row (which may happen if n < m).
 *
 * Wrong results because of overflows will not occur with input values
 * in the range of 0 to INT_MAX / 2 inclusive.
 *
 * The function applies the Dinic-Kronrod algorithm. It is not modern or
 * popular, but it seems to be a good choice for small matrices at least.
 * The original form of the algorithm is modified as follows: There is no
 * initial search for row minima, the initial assignments are in a
 * "virtual" column with the index -1 and zero values. This permits inputs
 * with n < m, and it simplifies the reassignments.
 */
void
wsmouse_matching(int *matrix, int m, int n, int *buffer)
{
	int i, j, k, d, e, row, col, delta;
	int *p;
	int *r2c = buffer;	/* row-to-column assignments */
	int *red = r2c + m;	/* reduced values of the assignments */
	int *alt = red + m;	/* alternative assignments */
	int *mc = alt + m;	/* row-wise minimal elements of cs */
	int *cs = mc + m;	/* the column set */
	int *c2r = cs + n;	/* column-to-row assignments in cs */
	int *cd = c2r + n;	/* column deltas (reduction) */

	for (p = r2c; p < red; *p++ = -1) {}
	for (; p < alt; *p++ = 0) {}
	for (col = 0; col < n; col++) {
		delta = INT_MAX;
		for (i = 0, p = matrix + col; i < m; i++, p += n)
			if ((d = *p - red[i]) <= delta) {
				delta = d;
				row = i;
			}
		cd[col] = delta;
		if (r2c[row] < 0) {
			r2c[row] = col;
			continue;
		}
		for (p = alt; p < mc; *p++ = -1) {}
		for (; p < cs; *p++ = col) {}
		for (k = 0; (j = r2c[row]) >= 0;) {
			cs[k++] = j;
			c2r[j] = row;
			alt[row] = mc[row];
			delta = INT_MAX;
			for (i = 0, p = matrix; i < m; i++, p += n)
				if (alt[i] < 0) {
					d = p[mc[i]] - cd[mc[i]];
					e = p[j] - cd[j];
					if (e < d) {
						d = e;
						mc[i] = j;
					}
					d -= red[i];
					if (d <= delta) {
						delta = d;
						row = i;
					}
				}
			cd[col] += delta;
			for (i = 0; i < k; i++) {
				cd[cs[i]] += delta;
				red[c2r[cs[i]]] -= delta;
			}
		}
		for (j = mc[row]; (r2c[row] = j) != col;) {
			row = c2r[j];
			j = alt[row];
		}
	}
}

void
wsmouse_mtframe(struct device *sc, struct mtpoint *pt, int size)
{
	struct wsmouseinput *input = &((struct wsmouse_softc *) sc)->input;
	struct mt_state *mt = &input->mt;
	int i, j, m, n, dx, dy, slot, maxdist;
	int *p, *r2c, *c2r;
	u_int touches;

	if (mt->num_slots == 0 || mt->matrix == NULL)
		return;

	size = imax(0, imin(size, mt->num_slots));
	p = mt->matrix;
	touches = mt->touches;
	if (mt->num_touches >= size) {
		FOREACHBIT(touches, slot)
			for (i = 0; i < size; i++) {
				dx = pt[i].x - mt->slots[slot].x;
				dy = pt[i].y - mt->slots[slot].y;
				*p++ = dx * dx + dy * dy;
			}
		m = mt->num_touches;
		n = size;
	} else {
		for (i = 0; i < size; i++)
			FOREACHBIT(touches, slot) {
				dx = pt[i].x - mt->slots[slot].x;
				dy = pt[i].y - mt->slots[slot].y;
				*p++ = dx * dx + dy * dy;
			}
		m = size;
		n = mt->num_touches;
	}
	wsmouse_matching(mt->matrix, m, n, p);

	r2c = p;
	c2r = p + m;
	maxdist = input->params.tracking_maxdist;
	maxdist = (maxdist ? maxdist * maxdist : INT_MAX);
	for (i = 0, p = mt->matrix; i < m; i++, p += n)
		if ((j = r2c[i]) >= 0) {
			if (p[j] <= maxdist)
				c2r[j] = i;
			else
				c2r[j] = r2c[i] = -1;
		}

	p = (n == size ? c2r : r2c);
	for (i = 0; i < size; i++)
		if (*p++ < 0) {
			slot = ffs(~(mt->touches | mt->frame)) - 1;
			if (slot < 0 || slot >= mt->num_slots)
				break;
			wsmouse_mtstate(sc, slot,
			    pt[i].x, pt[i].y, pt[i].pressure);
			pt[i].slot = slot;
		}

	p = (n == size ? r2c : c2r);
	FOREACHBIT(touches, slot)
		if ((i = *p++) >= 0) {
			wsmouse_mtstate(sc, slot,
			    pt[i].x, pt[i].y, pt[i].pressure);
			pt[i].slot = slot;
		} else {
			wsmouse_mtstate(sc, slot, 0, 0, 0);
		}
}

static __inline void
free_mt_slots(struct wsmouseinput *input)
{
	int n, size;

	if ((n = input->mt.num_slots)) {
		size = n * sizeof(struct mt_slot);
		if (input->flags & MT_TRACKING)
			size += MATRIX_SIZE(n);
		input->mt.num_slots = 0;
		free(input->mt.slots, M_DEVBUF, size);
		input->mt.slots = NULL;
		input->mt.matrix = NULL;
	}
}

/* Allocate the MT slots and, if necessary, the buffers for MT tracking. */
int
wsmouse_mt_init(struct device *sc, int num_slots, int tracking)
{
	struct wsmouseinput *input =
	    &((struct wsmouse_softc *) sc)->input;
	int n, size;

	if (num_slots == input->mt.num_slots
	    && (!tracking == ((input->flags & MT_TRACKING) == 0)))
		return (0);

	free_mt_slots(input);

	if (tracking)
		input->flags |= MT_TRACKING;
	else
		input->flags &= ~MT_TRACKING;
	n = imin(imax(num_slots, 0), WSMOUSE_MT_SLOTS_MAX);
	if (n) {
		size = n * sizeof(struct mt_slot);
		if (input->flags & MT_TRACKING)
			size += MATRIX_SIZE(n);
		input->mt.slots = malloc(size, M_DEVBUF, M_WAITOK | M_ZERO);
		if (input->mt.slots != NULL) {
			if (input->flags & MT_TRACKING)
				input->mt.matrix = (int *)
				    (input->mt.slots + n);
			input->mt.num_slots = n;
			return (0);
		}
	}
	return (-1);
}

void
wsmouse_init_scaling(struct wsmouseinput *input)
{
	struct wsmouseparams *params = &input->params;
	int m, n;

	if (params->dx_mul || params->dx_div
	    || params->dy_mul || params->dy_div) {
		/* Scale factors have a [*.12] fixed point format. */
		m = (params->dx_mul ? abs(params->dx_mul) : 1);
		n = (params->dx_div ? abs(params->dx_div) : 1);
		input->fltr.h.scale = (m << 12) / n;
		input->fltr.h.rmdr = 0;
		m = (params->dy_mul ? abs(params->dy_mul) : 1);
		n = (params->dy_div ? abs(params->dy_div): 1);
		input->fltr.v.scale = (m << 12) / n;
		input->fltr.v.rmdr = 0;
		input->flags |= SCALE_DELTAS;
	} else {
		input->flags &= ~SCALE_DELTAS;
	}
}

void
wsmouse_set_param(struct device *sc, size_t param, int value)
{
	struct wsmouseinput *input =
	    &((struct wsmouse_softc *) sc)->input;
	struct wsmouseparams *params = &input->params;
	int *p;

	if (param < 0 || param > WSMPARAM_LASTFIELD) {
		printf("wsmouse_set_param: invalid parameter type\n");
		return;
	}

	p = (int *) (((void *) params) + param);
	*p = value;

	if (IS_WSMFLTR_PARAM(param)) {
		wsmouse_init_scaling(input);
	} else if (param == WSMPARAM_SWAPXY) {
		if (value)
			input->flags |= SWAPXY;
		else
			input->flags &= ~SWAPXY;
	} else if (param == WSMPARAM_PRESSURE_LO) {
		params->pressure_hi =
		    imax(params->pressure_lo, params->pressure_hi);
		input->touch.min_pressure = params->pressure_hi;
	} else if (param == WSMPARAM_PRESSURE_HI
	    && params->pressure_lo == 0) {
		params->pressure_lo = params->pressure_hi;
		input->touch.min_pressure = params->pressure_hi;
	}
}

int
wsmouse_set_mode(struct device *sc, int mode)
{
	struct wsmouseinput *input =
	    &((struct wsmouse_softc *) sc)->input;

	if (mode == WSMOUSE_COMPAT) {
		input->flags &= ~TPAD_NATIVE_MODE;
		input->flags |= TPAD_COMPAT_MODE;
		return (0);
	} else if (mode == WSMOUSE_NATIVE) {
		input->flags &= ~TPAD_COMPAT_MODE;
		input->flags |= TPAD_NATIVE_MODE;
		return (0);
	}
	return (-1);
}

void
wsmouse_input_init(struct wsmouseinput *input, struct wseventvar **evar)
{
	input->evar = evar;
}

void
wsmouse_input_cleanup(struct wsmouseinput *input)
{
	free_mt_slots(input);
}
