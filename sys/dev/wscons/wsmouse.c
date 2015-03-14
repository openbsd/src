/* $OpenBSD: wsmouse.c,v 1.27 2015/03/14 03:38:50 jsg Exp $ */
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

#include <dev/wscons/wscons_features.h>
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>
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

#define	INVALID_X	INT_MAX
#define	INVALID_Y	INT_MAX
#define	INVALID_Z	INT_MAX
#define	INVALID_W	INT_MAX

struct wsmouse_softc {
	struct wsevsrc	sc_base;

	const struct wsmouse_accessops *sc_accessops;
	void		*sc_accesscookie;

	u_int		sc_mb;		/* mouse button state */
	u_int		sc_ub;		/* user button state */
	int		sc_dx;		/* delta-x */
	int		sc_dy;		/* delta-y */
	int		sc_dz;		/* delta-z */
	int		sc_dw;		/* delta-w */
	int		sc_x;		/* absolute-x */
	int		sc_y;		/* absolute-y */
	int		sc_z;		/* absolute-z */
	int		sc_w;		/* absolute-w */

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

	return (0);
}

void
wsmouse_input(struct device *wsmousedev, u_int btns, /* 0 is up */
    int x, int y, int z, int w, u_int flags)
{
	struct wsmouse_softc *sc = (struct wsmouse_softc *)wsmousedev;
	struct wscons_event *ev;
	struct wseventvar *evar;
	int mb, ub, d, get, put, any;

	add_mouse_randomness(x ^ y ^ z ^ w ^ btns);

	/*
	 * Discard input if not ready.
	 */
	evar = sc->sc_base.me_evp;
	if (evar == NULL)
		return;

#ifdef DIAGNOSTIC
	if (evar->q == NULL) {
		printf("wsmouse_input: evar->q=NULL\n");
		return;
	}
#endif

#if NWSMUX > 0
	DPRINTFN(5,("wsmouse_input: %s mux=%p, evar=%p\n",
		    sc->sc_base.me_dv.dv_xname, sc->sc_base.me_parent, evar));
#endif

	sc->sc_mb = btns;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_X))
		sc->sc_dx += x;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_Y))
		sc->sc_dy += y;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_Z))
		sc->sc_dz += z;
	if (!(flags & WSMOUSE_INPUT_ABSOLUTE_W))
		sc->sc_dw += w;

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
		getnanotime(&ev->time);					\
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
	if (flags & WSMOUSE_INPUT_ABSOLUTE_W) {
		if (sc->sc_w != w) {
			NEXT;
			ev->type = WSCONS_EVENT_MOUSE_ABSOLUTE_W;
			ev->value = w;
			TIMESTAMP;
			ADVANCE;
			sc->sc_w = w;
		}
	} else {
		if (sc->sc_dw) {
			NEXT;
			ev->type = WSCONS_EVENT_MOUSE_DELTA_W;
			ev->value = sc->sc_dw;
			TIMESTAMP;
			ADVANCE;
			sc->sc_dw = 0;
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

	NEXT;
	ev->type = WSCONS_EVENT_SYNC;
	ev->value = 0;
	TIMESTAMP;
	ADVANCE;

#undef	TIMESTAMP
#undef	ADVANCE
#undef	NEXT

out:
	if (any) {
		sc->sc_ub = ub;
		evar->put = put;
		WSEVENT_WAKEUP(evar);
#ifdef HAVE_BURNER_SUPPORT
		/* wsdisplay_burn(sc->sc_displaydv, WSDISPLAY_BURN_MOUSE); */
#endif
#if NWSMUX > 0
		DPRINTFN(5,("wsmouse_input: %s wakeup evar=%p\n",
			    sc->sc_base.me_dv.dv_xname, evar));
#endif
	}
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
	sc->sc_x = INVALID_X;
	sc->sc_y = INVALID_Y;
	sc->sc_z = INVALID_Z;
	sc->sc_w = INVALID_W;

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
