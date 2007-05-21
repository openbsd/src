/*	$OpenBSD: usb.c,v 1.39 2007/05/21 05:40:28 jsg Exp $	*/
/*	$NetBSD: usb.c,v 1.77 2003/01/01 00:10:26 thorpej Exp $	*/

/*
 * Copyright (c) 1998, 2002 The NetBSD Foundation, Inc.
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
 * USB specifications and other documentation can be found at
 * http://www.usb.org/developers/docs/ and
 * http://www.usb.org/developers/devclass_docs/
 */

#include "ohci.h"
#include "uhci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#define USB_DEV_MINOR 255

#include <machine/bus.h>

#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	do { if (usbdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (usbdebug>(n)) printf x; } while (0)
int	usbdebug = 0;
#if defined(UHCI_DEBUG) && NUHCI > 0
extern int	uhcidebug;
#endif
#if defined(OHCI_DEBUG) && NOHCI > 0
extern int	ohcidebug;
#endif
/*
 * 0  - do usual exploration
 * 1  - do not use timeout exploration
 * >1 - do no exploration
 */
int	usb_noexplore = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct usb_softc {
	USBBASEDEVICE	sc_dev;		/* base device */
	usbd_bus_handle sc_bus;		/* USB controller */
	struct usbd_port sc_port;	/* dummy port for root hub */

	usb_proc_ptr	sc_event_thread;

	char		sc_dying;
};

TAILQ_HEAD(, usb_task) usb_all_tasks;

#if defined(__NetBSD__)
dev_type_open(usbopen);
dev_type_close(usbclose);
dev_type_read(usbread);
dev_type_ioctl(usbioctl);
dev_type_poll(usbpoll);
dev_type_kqfilter(usbkqfilter);

const struct cdevsw usb_cdevsw = {
	usbopen, usbclose, usbread, nowrite, usbioctl,
	nostop, notty, usbpoll, nommap, usbkqfilter,
};
#endif

Static volatile int threads_pending = 0;

Static void	usb_discover(void *);
Static void	usb_create_event_thread(void *);
Static void	usb_event_thread(void *);
Static void	usb_task_thread(void *);
Static usb_proc_ptr usb_task_thread_proc = NULL;

#define USB_MAX_EVENTS 100
struct usb_event_q {
	struct usb_event ue;
	SIMPLEQ_ENTRY(usb_event_q) next;
};
Static SIMPLEQ_HEAD(, usb_event_q) usb_events =
	SIMPLEQ_HEAD_INITIALIZER(usb_events);
Static int usb_nevents = 0;
Static struct selinfo usb_selevent;
Static usb_proc_ptr usb_async_proc;  /* process that wants USB SIGIO */
Static int usb_dev_open = 0;
Static void usb_add_event(int, struct usb_event *);

Static int usb_get_next_event(struct usb_event *);

Static const char *usbrev_str[] = USBREV_STR;

USB_DECLARE_DRIVER(usb);

USB_MATCH(usb)
{
	DPRINTF(("usbd_match\n"));
	return (UMATCH_GENERIC);
}

USB_ATTACH(usb)
{
	struct usb_softc *sc = (struct usb_softc *)self;
	usbd_device_handle dev;
	usbd_status err;
	int usbrev;
	int speed;
	struct usb_event ue;

	DPRINTF(("usbd_attach\n"));

	usbd_init();
	sc->sc_bus = aux;
	sc->sc_bus->usbctl = sc;
	sc->sc_port.power = USB_MAX_POWER;

	usbrev = sc->sc_bus->usbrev;
	printf(": USB revision %s", usbrev_str[usbrev]);
	switch (usbrev) {
	case USBREV_1_0:
	case USBREV_1_1:
		speed = USB_SPEED_FULL;
		break;
	case USBREV_2_0:
		speed = USB_SPEED_HIGH;
		break;
	default:
		printf(", not supported\n");
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}
	printf("\n");

	/* Make sure not to use tsleep() if we are cold booting. */
	if (cold)
		sc->sc_bus->use_polling++;

	ue.u.ue_ctrlr.ue_bus = USBDEVUNIT(sc->sc_dev);
	usb_add_event(USB_EVENT_CTRLR_ATTACH, &ue);

#ifdef USB_USE_SOFTINTR
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	/* XXX we should have our own level */
	sc->sc_bus->soft = softintr_establish(IPL_SOFTNET,
	    sc->sc_bus->methods->soft_intr, sc->sc_bus);
	if (sc->sc_bus->soft == NULL) {
		printf("%s: can't register softintr\n", USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}
#else
	usb_callout_init(sc->sc_bus->softi);
#endif
#endif

	err = usbd_new_device(USBDEV(sc->sc_dev), sc->sc_bus, 0, speed, 0,
		  &sc->sc_port);
	if (!err) {
		dev = sc->sc_port.device;
		if (dev->hub == NULL) {
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
		if (cold && (sc->sc_dev.dv_cfdata->cf_flags & 1))
			dev->hub->explore(sc->sc_bus->root_hub);
#endif
	} else {
		printf("%s: root hub problem, error=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		sc->sc_dying = 1;
	}
	if (cold)
		sc->sc_bus->use_polling--;

	config_pending_incr();
	kthread_create_deferred(usb_create_event_thread, sc);

	USB_ATTACH_SUCCESS_RETURN;
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
void
usb_create_event_thread(void *arg)
{
	struct usb_softc *sc = arg;
	static int created = 0;

	if (sc->sc_bus->usbrev == USBREV_2_0)
		threads_pending++;

	if (kthread_create(usb_event_thread, sc, &sc->sc_event_thread,
	    "%s", sc->sc_dev.dv_xname))
		panic("unable to create event thread for %s",
		    sc->sc_dev.dv_xname);

	if (!created) {
		created = 1;
		TAILQ_INIT(&usb_all_tasks);
		if (kthread_create(usb_task_thread, NULL,
		    &usb_task_thread_proc, "usbtask"))
			panic("unable to create usb task thread");
	}
}

/*
 * Add a task to be performed by the task thread.  This function can be
 * called from any context and the task will be executed in a process
 * context ASAP.
 */
void
usb_add_task(usbd_device_handle dev, struct usb_task *task)
{
	int s;

	s = splusb();
	if (!task->onqueue) {
		DPRINTFN(2,("usb_add_task: task=%p\n", task));
		TAILQ_INSERT_TAIL(&usb_all_tasks, task, next);
		task->onqueue = 1;
	} else {
		DPRINTFN(3,("usb_add_task: task=%p on q\n", task));
	}
	wakeup(&usb_all_tasks);
	splx(s);
}

void
usb_rem_task(usbd_device_handle dev, struct usb_task *task)
{
	int s;

	s = splusb();
	if (task->onqueue) {
		TAILQ_REMOVE(&usb_all_tasks, task, next);
		task->onqueue = 0;
	}
	splx(s);
}

void
usb_event_thread(void *arg)
{
	struct usb_softc *sc = arg;
	int pwrdly;

	DPRINTF(("usb_event_thread: start\n"));

	/* Wait for power to come good. */
	pwrdly = sc->sc_bus->root_hub->hub->hubdesc.bPwrOn2PwrGood * 
	    UHD_PWRON_FACTOR + USB_EXTRA_POWER_UP_TIME;
	usb_delay_ms(sc->sc_bus, pwrdly);

	/* USB1 threads wait for USB2 threads to finish their first probe. */
	while (sc->sc_bus->usbrev != USBREV_2_0 && threads_pending)
		(void)tsleep((void *)&threads_pending, PWAIT, "config", 0);

	/* Make sure first discover does something. */
	sc->sc_bus->needs_explore = 1;
	usb_discover(sc);
	config_pending_decr();

	/* Wake up any companions waiting for handover before their probes. */
	if (sc->sc_bus->usbrev == USBREV_2_0) {
		threads_pending--;
		wakeup((void *)&threads_pending);
	}

	while (!sc->sc_dying) {
#ifdef USB_DEBUG
		if (usb_noexplore < 2)
#endif
		usb_discover(sc);
#ifdef USB_DEBUG
		(void)tsleep(&sc->sc_bus->needs_explore, PWAIT, "usbevt",
		    usb_noexplore ? 0 : hz * 60);
#else
		(void)tsleep(&sc->sc_bus->needs_explore, PWAIT, "usbevt",
		    hz * 60);
#endif
		DPRINTFN(2,("usb_event_thread: woke up\n"));
	}
	sc->sc_event_thread = NULL;

	/* In case parent is waiting for us to exit. */
	wakeup(sc);

	DPRINTF(("usb_event_thread: exit\n"));
	kthread_exit(0);
}

void
usb_task_thread(void *arg)
{
	struct usb_task *task;
	int s;

	DPRINTF(("usb_task_thread: start\n"));

	s = splusb();
	for (;;) {
		task = TAILQ_FIRST(&usb_all_tasks);
		if (task == NULL) {
			tsleep(&usb_all_tasks, PWAIT, "usbtsk", 0);
			task = TAILQ_FIRST(&usb_all_tasks);
		}
		DPRINTFN(2,("usb_task_thread: woke up task=%p\n", task));
		if (task != NULL) {
			TAILQ_REMOVE(&usb_all_tasks, task, next);
			task->onqueue = 0;
			splx(s);
			task->fun(task->arg);
			s = splusb();
		}
	}
}

int
usbctlprint(void *aux, const char *pnp)
{
	/* only "usb"es can attach to host controllers */
	if (pnp)
		printf("usb at %s", pnp);

	return (UNCONF);
}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

int
usbopen(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
	int unit = minor(dev);
	struct usb_softc *sc;

	if (unit == USB_DEV_MINOR) {
		if (usb_dev_open)
			return (EBUSY);
		usb_dev_open = 1;
		usb_async_proc = 0;
		return (0);
	}

	USB_GET_SC_OPEN(usb, unit, sc);

	if (sc->sc_dying)
		return (EIO);

	return (0);
}

int
usbread(dev_t dev, struct uio *uio, int flag)
{
	struct usb_event ue;
	int s, error, n;

	if (minor(dev) != USB_DEV_MINOR)
		return (ENXIO);

	if (uio->uio_resid != sizeof(struct usb_event))
		return (EINVAL);

	error = 0;
	s = splusb();
	for (;;) {
		n = usb_get_next_event(&ue);
		if (n != 0)
			break;
		if (flag & IO_NDELAY) {
			error = EWOULDBLOCK;
			break;
		}
		error = tsleep(&usb_events, PZERO | PCATCH, "usbrea", 0);
		if (error)
			break;
	}
	splx(s);
	if (!error)
		error = uiomove((void *)&ue, uio->uio_resid, uio);

	return (error);
}

int
usbclose(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
	int unit = minor(dev);

	if (unit == USB_DEV_MINOR) {
		usb_async_proc = 0;
		usb_dev_open = 0;
	}

	return (0);
}

int
usbioctl(dev_t devt, u_long cmd, caddr_t data, int flag, usb_proc_ptr p)
{
	struct usb_softc *sc;
	int unit = minor(devt);

	if (unit == USB_DEV_MINOR) {
		switch (cmd) {
		case FIONBIO:
			/* All handled in the upper FS layer. */
			return (0);

		case FIOASYNC:
			if (*(int *)data)
				usb_async_proc = p;
			else
				usb_async_proc = 0;
			return (0);

		default:
			return (EINVAL);
		}
	}

	USB_GET_SC(usb, unit, sc);

	if (sc->sc_dying)
		return (EIO);

	switch (cmd) {
#ifdef USB_DEBUG
	case USB_SETDEBUG:
		if (!(flag & FWRITE))
			return (EBADF);
		usbdebug  = ((*(int *)data) & 0x000000ff);
#if defined(UHCI_DEBUG) && NUHCI > 0
		uhcidebug = ((*(int *)data) & 0x0000ff00) >> 8;
#endif
#if defined(OHCI_DEBUG) && NOHCI > 0
		ohcidebug = ((*(int *)data) & 0x00ff0000) >> 16;
#endif
		break;
#endif /* USB_DEBUG */
	case USB_REQUEST:
	{
		struct usb_ctl_request *ur = (void *)data;
		int len = UGETW(ur->ucr_request.wLength);
		struct iovec iov;
		struct uio uio;
		void *ptr = 0;
		int addr = ur->ucr_addr;
		usbd_status err;
		int error = 0;

		if (!(flag & FWRITE))
			return (EBADF);

		DPRINTF(("usbioctl: USB_REQUEST addr=%d len=%d\n", addr, len));
		if (len < 0 || len > 32768)
			return (EINVAL);
		if (addr < 0 || addr >= USB_MAX_DEVICES ||
		    sc->sc_bus->devices[addr] == 0)
			return (EINVAL);
		if (len != 0) {
			iov.iov_base = (caddr_t)ur->ucr_data;
			iov.iov_len = len;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_resid = len;
			uio.uio_offset = 0;
			uio.uio_segflg = UIO_USERSPACE;
			uio.uio_rw =
				ur->ucr_request.bmRequestType & UT_READ ?
				UIO_READ : UIO_WRITE;
			uio.uio_procp = p;
			ptr = malloc(len, M_TEMP, M_WAITOK);
			if (uio.uio_rw == UIO_WRITE) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
		err = usbd_do_request_flags(sc->sc_bus->devices[addr],
			  &ur->ucr_request, ptr, ur->ucr_flags,
			  &ur->ucr_actlen, USBD_DEFAULT_TIMEOUT);
		if (err) {
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
		int addr = di->udi_addr;
		usbd_device_handle dev;

		if (addr < 1 || addr >= USB_MAX_DEVICES)
			return (EINVAL);
		dev = sc->sc_bus->devices[addr];
		if (dev == NULL)
			return (ENXIO);
		usbd_fill_deviceinfo(dev, di, 1);
		break;
	}

	case USB_DEVICESTATS:
		*(struct usb_device_stats *)data = sc->sc_bus->stats;
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

int
usbpoll(dev_t dev, int events, usb_proc_ptr p)
{
	int revents, mask, s;

	if (minor(dev) == USB_DEV_MINOR) {
		revents = 0;
		mask = POLLIN | POLLRDNORM;

		s = splusb();
		if (events & mask && usb_nevents > 0)
			revents |= events & mask;
		if (revents == 0 && events & mask)
			selrecord(p, &usb_selevent);
		splx(s);

		return (revents);
	} else {
		return (POLLERR);
	}
}

Static void filt_usbrdetach(struct knote *);
Static int filt_usbread(struct knote *, long);
int usbkqfilter(dev_t, struct knote *);

Static void
filt_usbrdetach(struct knote *kn)
{
	int s;

	s = splusb();
	SLIST_REMOVE(&usb_selevent.sel_klist, kn, knote, kn_selnext);
	splx(s);
}

Static int
filt_usbread(struct knote *kn, long hint)
{

	if (usb_nevents == 0)
		return (0);

	kn->kn_data = sizeof(struct usb_event);
	return (1);
}

Static struct filterops usbread_filtops =
	{ 1, NULL, filt_usbrdetach, filt_usbread };

int
usbkqfilter(dev_t dev, struct knote *kn)
{
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		if (minor(dev) != USB_DEV_MINOR)
			return (1);
		klist = &usb_selevent.sel_klist;
		kn->kn_fop = &usbread_filtops;
		break;

	default:
		return (1);
	}

	kn->kn_hook = NULL;

	s = splusb();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}

/* Explore device tree from the root. */
Static void
usb_discover(void *v)
{
	struct usb_softc *sc = v;

	DPRINTFN(2,("usb_discover\n"));
#ifdef USB_DEBUG
	if (usb_noexplore > 1)
		return;
#endif
	/*
	 * We need mutual exclusion while traversing the device tree,
	 * but this is guaranteed since this function is only called
	 * from the event thread for the controller.
	 */
	while (sc->sc_bus->needs_explore && !sc->sc_dying) {
		sc->sc_bus->needs_explore = 0;
		sc->sc_bus->root_hub->hub->explore(sc->sc_bus->root_hub);
	}
}

void
usb_needs_explore(usbd_device_handle dev)
{
	DPRINTFN(2,("usb_needs_explore\n"));
	dev->bus->needs_explore = 1;
	wakeup(&dev->bus->needs_explore);
}

void
usb_needs_reattach(usbd_device_handle dev)
{
	DPRINTFN(2,("usb_needs_reattach\n"));
	dev->powersrc->reattach = 1;
	dev->bus->needs_explore = 1;
	wakeup(&dev->bus->needs_explore);
}

/* Called at splusb() */
int
usb_get_next_event(struct usb_event *ue)
{
	struct usb_event_q *ueq;

	if (usb_nevents <= 0)
		return (0);
	ueq = SIMPLEQ_FIRST(&usb_events);
#ifdef DIAGNOSTIC
	if (ueq == NULL) {
		printf("usb: usb_nevents got out of sync! %d\n", usb_nevents);
		usb_nevents = 0;
		return (0);
	}
#endif
	*ue = ueq->ue;
	SIMPLEQ_REMOVE_HEAD(&usb_events, next);
	free(ueq, M_USBDEV);
	usb_nevents--;
	return (1);
}

void
usbd_add_dev_event(int type, usbd_device_handle udev)
{
	struct usb_event ue;

	usbd_fill_deviceinfo(udev, &ue.u.ue_device, USB_EVENT_IS_ATTACH(type));
	usb_add_event(type, &ue);
}

void
usbd_add_drv_event(int type, usbd_device_handle udev, device_ptr_t dev)
{
	struct usb_event ue;

	ue.u.ue_driver.ue_cookie = udev->cookie;
	strncpy(ue.u.ue_driver.ue_devname, USBDEVPTRNAME(dev),
	    sizeof ue.u.ue_driver.ue_devname);
	usb_add_event(type, &ue);
}

Static void
usb_add_event(int type, struct usb_event *uep)
{
	struct usb_event_q *ueq;
	struct usb_event ue;
	struct timespec thetime;
	int s;

	nanotime(&thetime);
	/* Don't want to wait here inside splusb() */
	ueq = malloc(sizeof *ueq, M_USBDEV, M_WAITOK);
	ueq->ue = *uep;
	ueq->ue.ue_type = type;
	ueq->ue.ue_time = thetime;

	s = splusb();
	if (++usb_nevents >= USB_MAX_EVENTS) {
		/* Too many queued events, drop an old one. */
		DPRINTFN(-1,("usb: event dropped\n"));
		(void)usb_get_next_event(&ue);
	}
	SIMPLEQ_INSERT_TAIL(&usb_events, ueq, next);
	wakeup(&usb_events);
	selwakeup(&usb_selevent);
	if (usb_async_proc != NULL)
		psignal(usb_async_proc, SIGIO);
	splx(s);
}

void
usb_schedsoftintr(usbd_bus_handle bus)
{
	DPRINTFN(10,("usb_schedsoftintr: polling=%d\n", bus->use_polling));
#ifdef USB_USE_SOFTINTR
	if (bus->use_polling) {
		bus->methods->soft_intr(bus);
	} else {
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
		softintr_schedule(bus->soft);
#else
		if (!usb_callout_pending(bus->softi))
			usb_callout(bus->softi, 0, bus->methods->soft_intr,
				    bus);
#endif /* __HAVE_GENERIC_SOFT_INTERRUPTS */
	}
#else
	bus->methods->soft_intr(bus);
#endif /* USB_USE_SOFTINTR */
}

int
usb_activate(device_ptr_t self, enum devact act)
{
	struct usb_softc *sc = (struct usb_softc *)self;
	usbd_device_handle dev = sc->sc_port.device;
	int i, rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if (dev != NULL && dev->cdesc != NULL &&
		    dev->subdevs != NULL) {
			for (i = 0; dev->subdevs[i]; i++)
				rv |= config_deactivate(dev->subdevs[i]);
		}
		break;
	}
	return (rv);
}

int
usb_detach(device_ptr_t self, int flags)
{
	struct usb_softc *sc = (struct usb_softc *)self;
	struct usb_event ue;

	DPRINTF(("usb_detach: start\n"));

	sc->sc_dying = 1;

	/* Make all devices disconnect. */
	if (sc->sc_port.device != NULL)
		usb_disconnect_port(&sc->sc_port, self);

	/* Kill off event thread. */
	if (sc->sc_event_thread != NULL) {
		wakeup(&sc->sc_bus->needs_explore);
		if (tsleep(sc, PWAIT, "usbdet", hz * 60))
			printf("%s: event thread didn't die\n",
			       USBDEVNAME(sc->sc_dev));
		DPRINTF(("usb_detach: event thread dead\n"));
	}

	usbd_finish();

#ifdef USB_USE_SOFTINTR
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	if (sc->sc_bus->soft != NULL) {
		softintr_disestablish(sc->sc_bus->soft);
		sc->sc_bus->soft = NULL;
	}
#else
	usb_uncallout(sc->sc_bus->softi, bus->methods->soft_intr, bus);
#endif
#endif

	ue.u.ue_ctrlr.ue_bus = USBDEVUNIT(sc->sc_dev);
	usb_add_event(USB_EVENT_CTRLR_DETACH, &ue);

	return (0);
}
