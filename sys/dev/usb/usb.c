/*	$OpenBSD: usb.c,v 1.5 1999/09/27 18:03:56 fgsch Exp $	*/
/*	$NetBSD: usb.c,v 1.24 1999/09/15 21:10:11 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
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
 * USB specifications and other documentation can be found at
 * http://www.usb.org/developers/data/ and
 * http://www.usb.org/developers/index.html .
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#include <sys/kthread.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/uio.h>
#include <sys/conf.h>
#endif
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/select.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#if defined(__FreeBSD__)
MALLOC_DEFINE(M_USB, "USB", "USB");
MALLOC_DEFINE(M_USBDEV, "USBdev", "USB device");
MALLOC_DEFINE(M_USBHC, "USBHC", "USB host controller");

#include "usb_if.h"
#endif /* defined(__FreeBSD__) */

#include <machine/bus.h>

#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
int	usbdebug = 0;
int	uhcidebug;
int	ohcidebug;
int	usb_noexplore = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

int usb_nbus = 0;

#define USBUNIT(dev) (minor(dev))

struct usb_softc {
	USBBASEDEVICE	sc_dev;		/* base device */
	usbd_bus_handle sc_bus;		/* USB controller */
	struct usbd_port sc_port;	/* dummy port for root hub */

	struct selinfo	sc_consel;	/* waiting for connect change */
	struct proc    *sc_event_thread;

	char		sc_dying;
};

#if defined(__NetBSD__) || defined(__OpenBSD__)
cdev_decl(usb);
#elif defined(__FreeBSD__)
d_open_t  usbopen; 
d_close_t usbclose;
d_ioctl_t usbioctl;
int usbpoll __P((dev_t, int, struct proc *));

struct cdevsw usb_cdevsw = {
	usbopen,     usbclose,    noread,         nowrite,
	usbioctl,    nullstop,    nullreset,      nodevtotty,
	usbpoll,     nommap,      nostrat,
	"usb",        NULL,   -1
};
#endif

usbd_status usb_discover __P((struct usb_softc *));
void	usb_create_event_thread __P((void *));
void	usb_event_thread __P((void *));

/* Flag to see if we are in the cold boot process. */
extern int cold;

USB_DECLARE_DRIVER_INIT(usb, DEVMETHOD(bus_print_child, usbd_print_child));

USB_MATCH(usb)
{
	DPRINTF(("usbd_match\n"));
	return (UMATCH_GENERIC);
}

USB_ATTACH(usb)
{
#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct usb_softc *sc = (struct usb_softc *)self;
#elif defined(__FreeBSD__)
	struct usb_softc *sc = device_get_softc(self);
	void *aux = device_get_ivars(self);
#endif
	usbd_device_handle dev;
	usbd_status r;
	
#if defined(__NetBSD__) || defined(__OpenBSD__)
	printf("\n");
#elif defined(__FreeBSD__)
	sc->sc_dev = self;
#endif

	DPRINTF(("usbd_attach\n"));
	usbd_init();
	sc->sc_bus = aux;
	sc->sc_bus->usbctl = sc;
	sc->sc_port.power = USB_MAX_POWER;
	r = usbd_new_device(USBDEV(sc->sc_dev), sc->sc_bus, 0, 0, 0,
			    &sc->sc_port);

	if (r == USBD_NORMAL_COMPLETION) {
		dev = sc->sc_port.device;
		if (!dev->hub) {
			sc->sc_dying = 1;
			printf("%s: root device is not a hub\n", 
			       USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}
		sc->sc_bus->root_hub = dev;
#if 1
		/* 
		 * Turning this code off will delay attachment of USB devices
		 * until the USB event thread is running, which means that
		 * the keyboard will not work until after cold boot.
		 */
		if (cold) {
			sc->sc_bus->use_polling++;
			dev->hub->explore(sc->sc_bus->root_hub);
			sc->sc_bus->use_polling--;
		}
#endif
	} else {
		printf("%s: root hub problem, error=%d\n", 
		       USBDEVNAME(sc->sc_dev), r); 
		sc->sc_dying = 1;
	}

#if defined(__OpenBSD__)
	kthread_create_deferred(usb_create_event_thread, sc);
#else
	kthread_create(usb_create_event_thread, sc);
#endif

	usb_nbus++;
	USB_ATTACH_SUCCESS_RETURN;
}

void
usb_create_event_thread(arg)
	void *arg;
{
	struct usb_softc *sc = arg;

#if !defined(__OpenBSD__)
	if (kthread_create1(usb_event_thread, sc, &sc->sc_event_thread,
#else
	if (kthread_create(usb_event_thread, sc, &sc->sc_event_thread,
#endif
			   "%s", sc->sc_dev.dv_xname)) {
		printf("%s: unable to create event thread for\n",
		       sc->sc_dev.dv_xname);
		panic("usb_create_event_thread");
	}
}

void
usb_event_thread(arg)
	void *arg;
{
	struct usb_softc *sc = arg;

	while (!sc->sc_dying) {
#ifdef USB_DEBUG
		if (!usb_noexplore)
#endif
		usb_discover(sc);
		(void)tsleep(&sc->sc_bus->needs_explore, 
			     PWAIT, "usbevt", hz*60);
		DPRINTFN(2,("usb_event_thread: woke up\n"));
	}
	sc->sc_event_thread = 0;

	/* In case parent is waiting for us to exit. */
	wakeup(sc);

	kthread_exit(0);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
usbctlprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	/* only "usb"es can attach to host controllers */
	if (pnp)
		printf("usb at %s", pnp);

	return (UNCONF);
}
#endif

int
usbopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	USB_GET_SC_OPEN(usb, USBUNIT(dev), sc);

	if (sc == 0)
		return (ENXIO);
	if (sc->sc_dying)
		return (EIO);

	return (0);
}

int
usbclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	return (0);
}

int
usbioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	USB_GET_SC(usb, USBUNIT(dev), sc);

	if (sc->sc_dying)
		return (EIO);

	switch (cmd) {
#ifdef USB_DEBUG
	case USB_SETDEBUG:
		usbdebug = uhcidebug = ohcidebug = *(int *)data;
		break;
#endif
	case USB_REQUEST:
	{
		struct usb_ctl_request *ur = (void *)data;
		int len = UGETW(ur->request.wLength);
		struct iovec iov;
		struct uio uio;
		void *ptr = 0;
		int addr = ur->addr;
		usbd_status r;
		int error = 0;

		DPRINTF(("usbioctl: USB_REQUEST addr=%d len=%d\n", addr, len));
		if (len < 0 || len > 32768)
			return (EINVAL);
		if (addr < 0 || addr >= USB_MAX_DEVICES || 
		    sc->sc_bus->devices[addr] == 0)
			return (EINVAL);
		if (len != 0) {
			iov.iov_base = (caddr_t)ur->data;
			iov.iov_len = len;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_resid = len;
			uio.uio_offset = 0;
			uio.uio_segflg = UIO_USERSPACE;
			uio.uio_rw =
				ur->request.bmRequestType & UT_READ ? 
				UIO_READ : UIO_WRITE;
			uio.uio_procp = p;
			ptr = malloc(len, M_TEMP, M_WAITOK);
			if (uio.uio_rw == UIO_WRITE) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
		r = usbd_do_request_flags(sc->sc_bus->devices[addr],
					  &ur->request, ptr, 
					  ur->flags, &ur->actlen);
		if (r != USBD_NORMAL_COMPLETION) {
			error = EIO;
			goto ret;
		}
		if (len != 0) {
			if (uio.uio_rw == UIO_READ) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
	ret:
		if (ptr)
			free(ptr, M_TEMP);
		return (error);
	}

	case USB_DEVICEINFO:
	{
		struct usb_device_info *di = (void *)data;
		int addr = di->addr;
		usbd_device_handle dev;

		if (addr < 1 || addr >= USB_MAX_DEVICES)
			return (EINVAL);
		dev = sc->sc_bus->devices[addr];
		if (dev == 0)
			return (ENXIO);
		usbd_fill_deviceinfo(dev, di);
		break;
	}

	case USB_DEVICESTATS:
		*(struct usb_device_stats *)data = sc->sc_bus->stats;
		break;

	default:
		return (ENXIO);
	}
	return (0);
}

int
usbpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	int revents, s;
	USB_GET_SC(usb, USBUNIT(dev), sc);

	if (sc->sc_dying)
		return (EIO);

	DPRINTFN(2, ("usbpoll: sc=%p events=0x%x\n", sc, events));
	s = splusb();
	revents = 0;
	if (events & (POLLOUT | POLLWRNORM))
		if (sc->sc_bus->needs_explore)
			revents |= events & (POLLOUT | POLLWRNORM);
	DPRINTFN(2, ("usbpoll: revents=0x%x\n", revents));
	if (revents == 0) {
		if (events & (POLLOUT | POLLWRNORM)) {
			DPRINTFN(2, ("usbpoll: selrecord\n"));
			selrecord(p, &sc->sc_consel);
		}
	}
	splx(s);
	return (revents);
}

/* Explore device tree from the root. */
usbd_status
usb_discover(sc)
	struct usb_softc *sc;
{
	/* 
	 * We need mutual exclusion while traversing the device tree,
	 * but this is guaranteed since this function is only called
	 * from the event thread for the controller.
	 */
	do {
		sc->sc_bus->needs_explore = 0;
		sc->sc_bus->root_hub->hub->explore(sc->sc_bus->root_hub);
	} while (sc->sc_bus->needs_explore && !sc->sc_dying);
	return (USBD_NORMAL_COMPLETION);
}

void
usb_needs_explore(bus)
	usbd_bus_handle bus;
{
	bus->needs_explore = 1;
	selwakeup(&bus->usbctl->sc_consel);
	wakeup(&bus->needs_explore);
}

int
usb_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct usb_softc *sc = (struct usb_softc *)self;
	usbd_device_handle dev = sc->sc_port.device;
	int i, rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if (dev && dev->cdesc && dev->subdevs) {
			for (i = 0; dev->subdevs[i]; i++)
				rv |= config_deactivate(dev->subdevs[i]);
		}
		break;
	}
	return (rv);
}

int
usb_detach(self, flags)
	device_ptr_t self;
	int flags;
{
	struct usb_softc *sc = (struct usb_softc *)self;

	sc->sc_dying = 1;

	/* Make all devices disconnect. */
	if (sc->sc_port.device)
		usb_disconnect_port(&sc->sc_port);

	/* Kill off event thread. */
	if (sc->sc_event_thread) {
		wakeup(&sc->sc_bus->needs_explore);
		if (tsleep(sc, PWAIT, "usbdet", hz * 60))
			printf("%s: event thread didn't die\n",
			       USBDEVNAME(sc->sc_dev));
	}

	usb_nbus--;
	return (0);
}

int
usbread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	/* XXX */
	return (0);
}

#if defined(__FreeBSD__)
DRIVER_MODULE(usb, root, usb_driver, usb_devclass, 0, 0);
#endif
