/* $OpenBSD: wsmouse.c,v 1.7 2001/10/04 19:37:52 mickey Exp $ */
/* $NetBSD: wsmouse.c,v 1.12 2000/05/01 07:36:58 takemura Exp $ */

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

#include <sys/cdefs.h>

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
 *	@(#)ms.c	8.1 (Berkeley) 6/11/93
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

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>
#include <dev/wscons/wseventvar.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rndvar.h>

#include "wsmouse.h"
#include "wsmux.h"
#include "wsdisplay.h"
#include "wskbd.h"

#if NWSMUX > 0
#include <dev/wscons/wsmuxvar.h>
#endif

#define	INVALID_X	INT_MAX
#define	INVALID_Y	INT_MAX
#define	INVALID_Z	INT_MAX

struct wsmouse_softc {
	struct device	sc_dv;

	const struct wsmouse_accessops *sc_accessops;
	void		*sc_accesscookie;

	int		sc_ready;	/* accepting events */
	struct wseventvar sc_events;	/* event queue state */

	u_int		sc_mb;		/* mouse button state */
	u_int		sc_ub;		/* user button state */
	int		sc_dx;		/* delta-x */
	int		sc_dy;		/* delta-y */
	int		sc_dz;		/* delta-z */
	int		sc_x;		/* absolute-x */
	int		sc_y;		/* absolute-y */
	int		sc_z;		/* absolute-z */

	int		sc_refcnt;
	u_char		sc_dying;	/* device is being detached */

#if NWSMUX > 0
	struct wsmux_softc *sc_mux;
#endif
};

int	wsmouse_match __P((struct device *, void *, void *));
void	wsmouse_attach __P((struct device *, struct device *, void *));
int	wsmouse_detach __P((struct device *, int));
int	wsmouse_activate __P((struct device *, enum devact));

int	wsmouse_do_ioctl __P((struct wsmouse_softc *, u_long, caddr_t, 
			      int, struct proc *));

int	wsmousedoclose __P((struct device *, int, int, struct proc *));
int	wsmousedoioctl __P((struct device *, u_long, caddr_t, int, 
			    struct proc *));

struct cfdriver wsmouse_cd = {
	NULL, "wsmouse", DV_TTY
};

struct cfattach wsmouse_ca = {
	sizeof (struct wsmouse_softc), wsmouse_match, wsmouse_attach,
	wsmouse_detach, wsmouse_activate
};

#if NWSMOUSE > 0
extern struct cfdriver wsmouse_cd;
#endif /* NWSMOUSE > 0 */

cdev_decl(wsmouse);

#if NWSMUX > 0
struct wsmuxops wsmouse_muxops = {
	wsmouseopen, wsmousedoclose, wsmousedoioctl, 0, 0, 0
};
#endif

/*
 * Print function (for parent devices).
 */
int
wsmousedevprint(aux, pnp)
	void *aux;
	const char *pnp;
{

	if (pnp)
		printf("wsmouse at %s", pnp);
	return (UNCONF);
}

int
wsmouse_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return (1);
}

void
wsmouse_attach(parent, self, aux)
        struct device *parent, *self;
	void *aux;
{
        struct wsmouse_softc *sc = (struct wsmouse_softc *)self;
	struct wsmousedev_attach_args *ap = aux;
#if NWSMUX > 0
	int mux;
#endif

	sc->sc_accessops = ap->accessops;
	sc->sc_accesscookie = ap->accesscookie;
	sc->sc_ready = 0;				/* sanity */

#if NWSMUX > 0
	mux = sc->sc_dv.dv_cfdata->wsmousedevcf_mux;
	if (mux != WSMOUSEDEVCF_MUX_DEFAULT) {
		wsmux_attach(mux, WSMUX_MOUSE, &sc->sc_dv, &sc->sc_events,
			     &sc->sc_mux, &wsmouse_muxops);
		printf(" mux %d", mux);
	}
#endif

	printf("\n");
}

int
wsmouse_activate(self, act)
	struct device *self;
	enum devact act;
{
	/* XXX should we do something more? */
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
wsmouse_detach(self, flags)
	struct device  *self;
	int flags;
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)self;
	struct wseventvar *evar;
	int maj, mn;
	int s;
#if NWSMUX > 0
	int mux;
#endif

	sc->sc_dying = 1;

#if NWSMUX > 0
	mux = sc->sc_dv.dv_cfdata->wsmousedevcf_mux;
	if (mux != WSMOUSEDEVCF_MUX_DEFAULT)
		wsmux_detach(mux, &sc->sc_dv);
#endif

	evar = &sc->sc_events;
	if (evar->io) {
		s = spltty();
		if (--sc->sc_refcnt >= 0) {
			/* Wake everyone by generating a dummy event. */
			if (++evar->put >= WSEVENT_QSIZE)
				evar->put = 0;
			WSEVENT_WAKEUP(evar);
			/* Wait for processes to go away. */
			if (tsleep(sc, PZERO, "wsmdet", hz * 60))
				printf("wsmouse_detach: %s didn't detach\n",
				       sc->sc_dv.dv_xname);
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

	return (0);
}

void
wsmouse_input(wsmousedev, btns, x, y, z, flags)
	struct device *wsmousedev;
	u_int btns;			/* 0 is up */
	int x, y, z;
	u_int flags;
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)wsmousedev;
	struct wscons_event *ev;
	struct wseventvar *evar;
	int mb, ub, d, get, put, any;

        /*
         * Discard input if not ready.
         */
	if (sc->sc_ready == 0)
		return;

	add_mouse_randomness(x ^ y ^ z ^ btns);

#if NWSMUX > 0
	if (sc->sc_mux)
		evar = &sc->sc_mux->sc_events;
	else
#endif
		evar = &sc->sc_events;

	sc->sc_mb = btns;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_X))
		sc->sc_dx += x;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_Y))
		sc->sc_dy += y;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_Z))
		sc->sc_dz += z;

	/*
	 * We have at least one event (mouse button, delta-X, or
	 * delta-Y; possibly all three, and possibly three separate
	 * button events).  Deliver these events until we are out
	 * of changes or out of room.  As events get delivered,
	 * mark them `unchanged'.
	 */
	ub = sc->sc_ub;
	any = 0;
	get = evar->get;
	put = evar->put;
	ev = &evar->q[put];

	/* NEXT prepares to put the next event, backing off if necessary */
#define	NEXT								\
	if ((++put) % WSEVENT_QSIZE == get) {				\
		put--;							\
		goto out;						\
	}
	/* ADVANCE completes the `put' of the event */
#define	ADVANCE								\
	ev++;								\
	if (put >= WSEVENT_QSIZE) {					\
		put = 0;						\
		ev = &evar->q[0];				\
	}								\
	any = 1
	/* TIMESTAMP sets `time' field of the event to the current time */
#define TIMESTAMP							\
	do {								\
		int s;							\
		s = splhigh();						\
		TIMEVAL_TO_TIMESPEC(&time, &ev->time);			\
		splx(s);						\
	} while (0)

	if (flags & WSMOUSE_INPUT_ABSOLUTE_X) {
		if (sc->sc_x != x) {
			NEXT;
			ev->type = WSCONS_EVENT_MOUSE_ABSOLUTE_X;
			ev->value = x;
			TIMESTAMP;
			ADVANCE;
			sc->sc_x = x;
		}
	} else {
		if (sc->sc_dx) {
			NEXT;
			ev->type = WSCONS_EVENT_MOUSE_DELTA_X;
			ev->value = sc->sc_dx;
			TIMESTAMP;
			ADVANCE;
			sc->sc_dx = 0;
		}
	}
	if (flags & WSMOUSE_INPUT_ABSOLUTE_Y) {
		if (sc->sc_y != y) {
			NEXT;
			ev->type = WSCONS_EVENT_MOUSE_ABSOLUTE_Y;
			ev->value = y;
			TIMESTAMP;
			ADVANCE;
			sc->sc_y = y;
		}
	} else {
		if (sc->sc_dy) {
			NEXT;
			ev->type = WSCONS_EVENT_MOUSE_DELTA_Y;
			ev->value = sc->sc_dy;
			TIMESTAMP;
			ADVANCE;
			sc->sc_dy = 0;
		}
	}
	if (flags & WSMOUSE_INPUT_ABSOLUTE_Z) {
		if (sc->sc_z != z) {
			NEXT;
			ev->type = WSCONS_EVENT_MOUSE_ABSOLUTE_Z;
			ev->value = z;
			TIMESTAMP;
			ADVANCE;
			sc->sc_z = z;
		}
	} else {
		if (sc->sc_dz) {
			NEXT;
			ev->type = WSCONS_EVENT_MOUSE_DELTA_Z;
			ev->value = sc->sc_dz;
			TIMESTAMP;
			ADVANCE;
			sc->sc_dz = 0;
		}
	}

	mb = sc->sc_mb;
	while ((d = mb ^ ub) != 0) {
		/*
		 * Mouse button change.  Find the first change and drop
		 * it into the event queue.
		 */
		NEXT;
		ev->value = ffs(d) - 1;

		KASSERT(ev->value >= 0);

		d = 1 << ev->value;
		ev->type =
		    (mb & d) ? WSCONS_EVENT_MOUSE_DOWN : WSCONS_EVENT_MOUSE_UP;
		TIMESTAMP;
		ADVANCE;
		ub ^= d;
	}
out:
	if (any) {
		sc->sc_ub = ub;
		evar->put = put;
		WSEVENT_WAKEUP(evar);
		/* wsdisplay_burn(sc->sc_displaydv, WSDISPLAY_BURN_MOUSE); */
	}
}

int
wsmouseopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
#if NWSMOUSE > 0
	struct wsmouse_softc *sc;
	int error, unit;

	unit = minor(dev);
	if (unit >= wsmouse_cd.cd_ndevs ||	/* make sure it was attached */
	    (sc = wsmouse_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	if (sc->sc_dying)
		return (EIO);

	if ((flags & (FREAD | FWRITE)) == FWRITE)
		return (0);			/* always allow open for write
						   so ioctl() is possible. */

#if NWSMUX > 0
	if (sc->sc_mux)
		return (EBUSY);
#endif

	if (sc->sc_events.io)			/* and that it's not in use */
		return (EBUSY);

	sc->sc_events.io = p;
	wsevent_init(&sc->sc_events);		/* may cause sleep */

	sc->sc_ready = 1;			/* start accepting events */
	sc->sc_x = INVALID_X;
	sc->sc_y = INVALID_Y;
	sc->sc_z = INVALID_Z;

	/* enable the device, and punt if that's not possible */
	error = (*sc->sc_accessops->enable)(sc->sc_accesscookie);
	if (error) {
		sc->sc_ready = 0;		/* stop accepting events */
		wsevent_fini(&sc->sc_events);
		sc->sc_events.io = NULL;
		return (error);
	}

	return (0);
#else
	return (ENXIO);
#endif /* NWSMOUSE > 0 */
}

int
wsmouseclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
#if NWSMOUSE > 0
	return (wsmousedoclose(wsmouse_cd.cd_devs[minor(dev)], 
			       flags, mode, p));
#else
	return (ENXIO);
#endif /* NWSMOUSE > 0 */
}

#if NWSMOUSE > 0
int
wsmousedoclose(dv, flags, mode, p)
	struct device *dv;
	int flags, mode;
	struct proc *p;
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)dv;

	if ((flags & (FREAD | FWRITE)) == FWRITE)
		return (0);			/* see wsmouseopen() */

	(*sc->sc_accessops->disable)(sc->sc_accesscookie);

	sc->sc_ready = 0;			/* stop accepting events */
	wsevent_fini(&sc->sc_events);
	sc->sc_events.io = NULL;
	return (0);
}
#endif /* NWSMOUSE > 0 */

int
wsmouseread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
#if NWSMOUSE > 0
	struct wsmouse_softc *sc = wsmouse_cd.cd_devs[minor(dev)];
	int error;

	if (sc->sc_dying)
		return (EIO);

	sc->sc_refcnt++;
	error = wsevent_read(&sc->sc_events, uio, flags);
	if (--sc->sc_refcnt < 0) {
		wakeup(sc);
		error = EIO;
	}
	return (error);
#else
	return (ENXIO);
#endif /* NWSMOUSE > 0 */
}

int
wsmouseioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
#if NWSMOUSE > 0
	return (wsmousedoioctl(wsmouse_cd.cd_devs[minor(dev)],
			       cmd, data, flag, p));
#else
	return (ENXIO);
#endif /* NWSMOUSE > 0 */
}

#if NWSMOUSE > 0
/* A wrapper around the ioctl() workhorse to make reference counting easy. */
int
wsmousedoioctl(dv, cmd, data, flag, p)
	struct device *dv;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
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
wsmouse_do_ioctl(sc, cmd, data, flag, p)
	struct wsmouse_softc *sc;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error;

	if (sc->sc_dying)
		return (EIO);

	/*
	 * Try the generic ioctls that the wsmouse interface supports.
	 */
	switch (cmd) {
	case FIONBIO:		/* we will remove this someday (soon???) */
		return (0);

	case FIOASYNC:
		sc->sc_events.async = *(int *)data != 0;
		return (0);

	case TIOCSPGRP:
		if (*(int *)data != sc->sc_events.io->p_pgid)
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
#endif /* NWSMOUSE > 0 */

int
wsmouseselect(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
#if NWSMOUSE > 0
	struct wsmouse_softc *sc = wsmouse_cd.cd_devs[minor(dev)];

	return (wsevent_poll(&sc->sc_events, events, p));
#else
	return (0);
#endif /* NWSMOUSE > 0 */
}

#if NWSMUX > 0
int
wsmouse_add_mux(unit, muxsc)
	int unit;
	struct wsmux_softc *muxsc;
{
	struct wsmouse_softc *sc;

	if (unit < 0 || unit >= wsmouse_cd.cd_ndevs ||
	    (sc = wsmouse_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	if (sc->sc_mux || sc->sc_events.io)
		return (EBUSY);

	return (wsmux_attach_sc(muxsc, WSMUX_MOUSE, &sc->sc_dv, &sc->sc_events, 
				&sc->sc_mux, &wsmouse_muxops));
}

int
wsmouse_rem_mux(unit, muxsc)
	int unit;
	struct wsmux_softc *muxsc;
{
	struct wsmouse_softc *sc;

	if (unit < 0 || unit >= wsmouse_cd.cd_ndevs ||
	    (sc = wsmouse_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	return (wsmux_detach_sc(muxsc, &sc->sc_dv));
}

#endif
