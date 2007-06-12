/*	$OpenBSD: uhid.c,v 1.40 2007/06/12 16:26:36 mbalmer Exp $ */
/*	$NetBSD: uhid.c,v 1.57 2003/03/11 16:44:00 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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

/*
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/hid.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/uhidev.h>

#ifdef UHID_DEBUG
#define DPRINTF(x)	do { if (uhiddebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uhiddebug>(n)) printf x; } while (0)
int	uhiddebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct uhid_softc {
	struct uhidev sc_hdev;

	int sc_isize;
	int sc_osize;
	int sc_fsize;

	u_char *sc_obuf;

	struct clist sc_q;
	struct selinfo sc_rsel;
	struct proc *sc_async;	/* process that wants SIGIO */
	u_char sc_state;	/* driver state */
#define	UHID_ASLP	0x01	/* waiting for device data */
#define UHID_IMMED	0x02	/* return read data immediately */

	int sc_refcnt;
	u_char sc_dying;
};

#define	UHIDUNIT(dev)	(minor(dev))
#define	UHID_CHUNK	128	/* chunk size for read */
#define	UHID_BSIZE	1020	/* buffer size */

void uhid_intr(struct uhidev *, void *, u_int len);

int uhid_do_read(struct uhid_softc *, struct uio *uio, int);
int uhid_do_write(struct uhid_softc *, struct uio *uio, int);
int uhid_do_ioctl(struct uhid_softc*, u_long, caddr_t, int,
			 struct proc *);

USB_DECLARE_DRIVER(uhid);

int
uhid_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;

	DPRINTF(("uhid_match: report=%d\n", uha->reportid));

	if (uha->matchlvl)
		return (uha->matchlvl);
	return (UMATCH_IFACECLASS_GENERIC);
}

void
uhid_attach(struct device *parent, struct device *self, void *aux)
{
	struct uhid_softc *sc = (struct uhid_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct uhidev_attach_arg *uha = (struct uhidev_attach_arg *)uaa;
	int size, repid;
	void *desc;

	sc->sc_hdev.sc_intr = uhid_intr;
	sc->sc_hdev.sc_parent = uha->parent;
	sc->sc_hdev.sc_report_id = uha->reportid;

	uhidev_get_report_desc(uha->parent, &desc, &size);
	repid = uha->reportid;
	sc->sc_isize = hid_report_size(desc, size, hid_input,   repid);
	sc->sc_osize = hid_report_size(desc, size, hid_output,  repid);
	sc->sc_fsize = hid_report_size(desc, size, hid_feature, repid);

	printf(": input=%d, output=%d, feature=%d\n",
	       sc->sc_isize, sc->sc_osize, sc->sc_fsize);
}

int
uhid_activate(struct device *self, enum devact act)
{
	struct uhid_softc *sc = (struct uhid_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}

int
uhid_detach(struct device *self, int flags)
{
	struct uhid_softc *sc = (struct uhid_softc *)self;
	int s;
	int maj, mn;

	DPRINTF(("uhid_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;

	if (sc->sc_hdev.sc_state & UHIDEV_OPEN) {
		s = splusb();
		if (--sc->sc_refcnt >= 0) {
			/* Wake everyone */
			wakeup(&sc->sc_q);
			/* Wait for processes to go away. */
			usb_detach_wait(&sc->sc_hdev.sc_dev);
		}
		splx(s);
	}

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == uhidopen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

#if 0
	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH,
			   sc->sc_hdev.sc_parent->sc_udev,
			   &sc->sc_hdev.sc_dev);
#endif

	return (0);
}

void
uhid_intr(struct uhidev *addr, void *data, u_int len)
{
	struct uhid_softc *sc = (struct uhid_softc *)addr;

#ifdef UHID_DEBUG
	if (uhiddebug > 5) {
		u_int32_t i;

		DPRINTF(("uhid_intr: data ="));
		for (i = 0; i < len; i++)
			DPRINTF((" %02x", ((u_char *)data)[i]));
		DPRINTF(("\n"));
	}
#endif

	(void)b_to_q(data, len, &sc->sc_q);

	if (sc->sc_state & UHID_ASLP) {
		sc->sc_state &= ~UHID_ASLP;
		DPRINTFN(5, ("uhid_intr: waking %p\n", &sc->sc_q));
		wakeup(&sc->sc_q);
	}
	selwakeup(&sc->sc_rsel);
	if (sc->sc_async != NULL) {
		DPRINTFN(3, ("uhid_intr: sending SIGIO %p\n", sc->sc_async));
		psignal(sc->sc_async, SIGIO);
	}
}

int
uhidopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct uhid_softc *sc;
	int error;

	if (UHIDUNIT(dev) >= uhid_cd.cd_ndevs)
		return (ENXIO);
	sc = uhid_cd.cd_devs[UHIDUNIT(dev)];
	if (sc == NULL)
		return (ENXIO);

	DPRINTF(("uhidopen: sc=%p\n", sc));

	if (sc->sc_dying)
		return (ENXIO);

	error = uhidev_open(&sc->sc_hdev);
	if (error)
		return (error);

	if (clalloc(&sc->sc_q, UHID_BSIZE, 0) == -1) {
		uhidev_close(&sc->sc_hdev);
		return (ENOMEM);
	}
	sc->sc_obuf = malloc(sc->sc_osize, M_USBDEV, M_WAITOK);
	sc->sc_state &= ~UHID_IMMED;
	sc->sc_async = NULL;

	return (0);
}

int
uhidclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct uhid_softc *sc;

	sc = uhid_cd.cd_devs[UHIDUNIT(dev)];

	DPRINTF(("uhidclose: sc=%p\n", sc));

	clfree(&sc->sc_q);
	free(sc->sc_obuf, M_USBDEV);
	sc->sc_async = NULL;
	uhidev_close(&sc->sc_hdev);

	return (0);
}

int
uhid_do_read(struct uhid_softc *sc, struct uio *uio, int flag)
{
	int s;
	int error = 0;
	int extra;
	size_t length;
	u_char buffer[UHID_CHUNK];
	usbd_status err;

	DPRINTFN(1, ("uhidread\n"));
	if (sc->sc_state & UHID_IMMED) {
		DPRINTFN(1, ("uhidread immed\n"));
		extra = sc->sc_hdev.sc_report_id != 0;
		err = uhidev_get_report(&sc->sc_hdev, UHID_INPUT_REPORT,
					buffer, sc->sc_isize + extra);
		if (err)
			return (EIO);
		return (uiomove(buffer+extra, sc->sc_isize, uio));
	}

	s = splusb();
	while (sc->sc_q.c_cc == 0) {
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		sc->sc_state |= UHID_ASLP;
		DPRINTFN(5, ("uhidread: sleep on %p\n", &sc->sc_q));
		error = tsleep(&sc->sc_q, PZERO | PCATCH, "uhidrea", 0);
		DPRINTFN(5, ("uhidread: woke, error=%d\n", error));
		if (sc->sc_dying)
			error = EIO;
		if (error) {
			sc->sc_state &= ~UHID_ASLP;
			break;
		}
	}
	splx(s);

	/* Transfer as many chunks as possible. */
	while (sc->sc_q.c_cc > 0 && uio->uio_resid > 0 && !error) {
		length = min(sc->sc_q.c_cc, uio->uio_resid);
		if (length > sizeof(buffer))
			length = sizeof(buffer);

		/* Remove a small chunk from the input queue. */
		(void) q_to_b(&sc->sc_q, buffer, length);
		DPRINTFN(5, ("uhidread: got %lu chars\n", (u_long)length));

		/* Copy the data to the user process. */
		if ((error = uiomove(buffer, length, uio)) != 0)
			break;
	}

	return (error);
}

int
uhidread(dev_t dev, struct uio *uio, int flag)
{
	struct uhid_softc *sc;
	int error;

	sc = uhid_cd.cd_devs[UHIDUNIT(dev)];

	sc->sc_refcnt++;
	error = uhid_do_read(sc, uio, flag);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_hdev.sc_dev);
	return (error);
}

int
uhid_do_write(struct uhid_softc *sc, struct uio *uio, int flag)
{
	int error;
	int size;
	usbd_status err;

	DPRINTFN(1, ("uhidwrite\n"));

	if (sc->sc_dying)
		return (EIO);

	size = sc->sc_osize;
	error = 0;
	if (uio->uio_resid != size)
		return (EINVAL);
	error = uiomove(sc->sc_obuf, size, uio);
	if (!error) {
		err = uhidev_set_report(&sc->sc_hdev, UHID_OUTPUT_REPORT,
					sc->sc_obuf, size);
		if (err)
			error = EIO;
	}

	return (error);
}

int
uhidwrite(dev_t dev, struct uio *uio, int flag)
{
	struct uhid_softc *sc;
	int error;

	sc = uhid_cd.cd_devs[UHIDUNIT(dev)];

	sc->sc_refcnt++;
	error = uhid_do_write(sc, uio, flag);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_hdev.sc_dev);
	return (error);
}

int
uhid_do_ioctl(struct uhid_softc *sc, u_long cmd, caddr_t addr,
	      int flag, struct proc *p)
{
	struct usb_ctl_report_desc *rd;
	struct usb_ctl_report *re;
	u_char buffer[UHID_CHUNK];
	int size, extra;
	usbd_status err;
	void *desc;

	DPRINTFN(2, ("uhidioctl: cmd=%lx\n", cmd));

	if (sc->sc_dying)
		return (EIO);

	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		break;

	case FIOASYNC:
		if (*(int *)addr) {
			if (sc->sc_async != NULL)
				return (EBUSY);
			sc->sc_async = p;
			DPRINTF(("uhid_do_ioctl: FIOASYNC %p\n", p));
		} else
			sc->sc_async = NULL;
		break;

	/* XXX this is not the most general solution. */
	case TIOCSPGRP:
		if (sc->sc_async == NULL)
			return (EINVAL);
		if (*(int *)addr != sc->sc_async->p_pgid)
			return (EPERM);
		break;

	case USB_GET_REPORT_DESC:
		uhidev_get_report_desc(sc->sc_hdev.sc_parent, &desc, &size);
		rd = (struct usb_ctl_report_desc *)addr;
		size = min(size, sizeof rd->ucrd_data);
		rd->ucrd_size = size;
		memcpy(rd->ucrd_data, desc, size);
		break;

	case USB_SET_IMMED:
		if (*(int *)addr) {
			extra = sc->sc_hdev.sc_report_id != 0;
			err = uhidev_get_report(&sc->sc_hdev, UHID_INPUT_REPORT,
						buffer, sc->sc_isize + extra);
			if (err)
				return (EOPNOTSUPP);

			sc->sc_state |=  UHID_IMMED;
		} else
			sc->sc_state &= ~UHID_IMMED;
		break;

	case USB_GET_REPORT:
		re = (struct usb_ctl_report *)addr;
		switch (re->ucr_report) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			break;
		default:
			return (EINVAL);
		}
		extra = sc->sc_hdev.sc_report_id != 0;
		err = uhidev_get_report(&sc->sc_hdev, re->ucr_report,
		    re->ucr_data, size + extra);
		if (extra)
			memcpy(re->ucr_data, re->ucr_data+1, size);
		if (err)
			return (EIO);
		break;

	case USB_SET_REPORT:
		re = (struct usb_ctl_report *)addr;
		switch (re->ucr_report) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			break;
		default:
			return (EINVAL);
		}
		err = uhidev_set_report(&sc->sc_hdev, re->ucr_report,
		    re->ucr_data, size);
		if (err)
			return (EIO);
		break;

	case USB_GET_REPORT_ID:
		*(int *)addr = sc->sc_hdev.sc_report_id;
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

int
uhidioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct uhid_softc *sc;
	int error;

	sc = uhid_cd.cd_devs[UHIDUNIT(dev)];

	sc->sc_refcnt++;
	error = uhid_do_ioctl(sc, cmd, addr, flag, p);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_hdev.sc_dev);
	return (error);
}

int
uhidpoll(dev_t dev, int events, struct proc *p)
{
	struct uhid_softc *sc;
	int revents = 0;
	int s;

	sc = uhid_cd.cd_devs[UHIDUNIT(dev)];

	if (sc->sc_dying)
		return (POLLERR);

	s = splusb();
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_q.c_cc > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &sc->sc_rsel);
	}

	splx(s);
	return (revents);
}

void filt_uhidrdetach(struct knote *);
int filt_uhidread(struct knote *, long);
int uhidkqfilter(dev_t, struct knote *);

void
filt_uhidrdetach(struct knote *kn)
{
	struct uhid_softc *sc = (void *)kn->kn_hook;
	int s;

	s = splusb();
	SLIST_REMOVE(&sc->sc_rsel.si_note, kn, knote, kn_selnext);
	splx(s);
}

int
filt_uhidread(struct knote *kn, long hint)
{
	struct uhid_softc *sc = (void *)kn->kn_hook;

	kn->kn_data = sc->sc_q.c_cc;
	return (kn->kn_data > 0);
}

struct filterops uhidread_filtops =
	{ 1, NULL, filt_uhidrdetach, filt_uhidread };

struct filterops uhid_seltrue_filtops =
	{ 1, NULL, filt_uhidrdetach, filt_seltrue };

int
uhidkqfilter(dev_t dev, struct knote *kn)
{
	struct uhid_softc *sc;
	struct klist *klist;
	int s;

	sc = uhid_cd.cd_devs[UHIDUNIT(dev)];

	if (sc->sc_dying)
		return (EIO);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->sc_rsel.si_note;
		kn->kn_fop = &uhidread_filtops;
		break;

	case EVFILT_WRITE:
		klist = &sc->sc_rsel.si_note;
		kn->kn_fop = &uhid_seltrue_filtops;
		break;

	default:
		return (1);
	}

	kn->kn_hook = (void *)sc;

	s = splusb();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}
