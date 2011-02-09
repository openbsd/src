/*	$OpenBSD: usb.c,v 1.74 2011/02/09 04:25:32 jakemsr Exp $	*/
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
#include "ehci.h"

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
#include <sys/time.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

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
#if defined(EHCI_DEBUG) && NEHCI > 0
extern int	ehcidebug;
#endif
/*
 * 0  - do usual exploration
 * !0 - do no exploration
 */
int	usb_noexplore = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct usb_softc {
	struct device	 sc_dev;	/* base device */
	usbd_bus_handle  sc_bus;	/* USB controller */
	struct usbd_port sc_port;	/* dummy port for root hub */

	struct usb_task	 sc_explore_task;

	struct timeval	 sc_ptime;
};

TAILQ_HEAD(, usb_task) usb_abort_tasks;
TAILQ_HEAD(, usb_task) usb_explore_tasks;
TAILQ_HEAD(, usb_task) usb_generic_tasks;

int usb_run_tasks, usb_run_abort_tasks;
int explore_pending;

void	usb_explore(void *);
void	usb_first_explore(void *);
void	usb_create_task_threads(void *);
void	usb_task_thread(void *);
struct proc *usb_task_thread_proc = NULL;
void	usb_abort_task_thread(void *);
struct proc *usb_abort_task_thread_proc = NULL;

const char *usbrev_str[] = USBREV_STR;

int usb_match(struct device *, void *, void *); 
void usb_attach(struct device *, struct device *, void *); 
int usb_detach(struct device *, int); 
int usb_activate(struct device *, int); 

struct cfdriver usb_cd = { 
	NULL, "usb", DV_DULL 
}; 

const struct cfattach usb_ca = { 
	sizeof(struct usb_softc), 
	usb_match, 
	usb_attach, 
	usb_detach, 
	usb_activate, 
};

int
usb_match(struct device *parent, void *match, void *aux)
{
	DPRINTF(("usbd_match\n"));
	return (UMATCH_GENERIC);
}

void
usb_attach(struct device *parent, struct device *self, void *aux)
{
	struct usb_softc *sc = (struct usb_softc *)self;
	usbd_device_handle dev;
	usbd_status err;
	int usbrev;
	int speed;

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
		sc->sc_bus->dying = 1;
		return;
	}
	printf("\n");

	/* Make sure not to use tsleep() if we are cold booting. */
	if (cold)
		sc->sc_bus->use_polling++;

	/* Don't let hub interrupts cause explore until ready. */
	sc->sc_bus->flags |= USB_BUS_CONFIG_PENDING;

	/* explore task */
	usb_init_task(&sc->sc_explore_task, usb_explore, sc,
	    USB_TASK_TYPE_EXPLORE);

	/* XXX we should have our own level */
	sc->sc_bus->soft = softintr_establish(IPL_SOFTNET,
	    sc->sc_bus->methods->soft_intr, sc->sc_bus);
	if (sc->sc_bus->soft == NULL) {
		printf("%s: can't register softintr\n", sc->sc_dev.dv_xname);
		sc->sc_bus->dying = 1;
		return;
	}

	err = usbd_new_device(&sc->sc_dev, sc->sc_bus, 0, speed, 0,
		  &sc->sc_port);
	if (!err) {
		dev = sc->sc_port.device;
		if (dev->hub == NULL) {
			sc->sc_bus->dying = 1;
			printf("%s: root device is not a hub\n",
			       sc->sc_dev.dv_xname);
			return;
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
		       sc->sc_dev.dv_xname, err);
		sc->sc_bus->dying = 1;
	}
	if (cold)
		sc->sc_bus->use_polling--;

	if (!sc->sc_bus->dying) {
		getmicrouptime(&sc->sc_ptime);
		if (sc->sc_bus->usbrev == USBREV_2_0)
			explore_pending++;
		config_pending_incr();
		kthread_create_deferred(usb_first_explore, sc);
	}
}

/*
 * Called by usbd_init when first usb is attached.
 */
void
usb_begin_tasks(void)
{
	TAILQ_INIT(&usb_abort_tasks);
	TAILQ_INIT(&usb_explore_tasks);
	TAILQ_INIT(&usb_generic_tasks);
	usb_run_tasks = usb_run_abort_tasks = 1;
	kthread_create_deferred(usb_create_task_threads, NULL);
}

/*
 * Called by usbd_finish when last usb is detached.
 */
void
usb_end_tasks(void)
{
	usb_run_tasks = usb_run_abort_tasks = 0;
	wakeup(&usb_run_abort_tasks);
	wakeup(&usb_run_tasks);
}

void
usb_create_task_threads(void *arg)
{
	if (kthread_create(usb_abort_task_thread, NULL,
	    &usb_abort_task_thread_proc, "usbatsk"))
		panic("unable to create usb abort task thread");

	if (kthread_create(usb_task_thread, NULL,
	    &usb_task_thread_proc, "usbtask"))
		panic("unable to create usb task thread");
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

	DPRINTFN(2,("%s: task=%p onqueue=%d type=%d\n", __func__, task,
	    task->onqueue, task->type));

	/* Don't add task if the device's root hub is dying. */
	if (usbd_is_dying(dev))
		return;

	s = splusb();
	if (!task->onqueue) {
		switch (task->type) {
		case USB_TASK_TYPE_ABORT:
			TAILQ_INSERT_TAIL(&usb_abort_tasks, task, next);
			break;
		case USB_TASK_TYPE_EXPLORE:
			TAILQ_INSERT_TAIL(&usb_explore_tasks, task, next);
			break;
		case USB_TASK_TYPE_GENERIC:
			TAILQ_INSERT_TAIL(&usb_generic_tasks, task, next);
			break;
		}
		task->onqueue = 1;
		task->dev = dev;
	}
	if (task->type == USB_TASK_TYPE_ABORT)
		wakeup(&usb_run_abort_tasks);
	else
		wakeup(&usb_run_tasks);
	splx(s);
}

void
usb_rem_task(usbd_device_handle dev, struct usb_task *task)
{
	int s;

	DPRINTFN(2,("%s: task=%p onqueue=%d type=%d\n", __func__, task,
	    task->onqueue, task->type));

	s = splusb();
	if (task->onqueue) {
		switch (task->type) {
		case USB_TASK_TYPE_ABORT:
			TAILQ_REMOVE(&usb_abort_tasks, task, next);
			break;
		case USB_TASK_TYPE_EXPLORE:
			TAILQ_REMOVE(&usb_explore_tasks, task, next);
			break;
		case USB_TASK_TYPE_GENERIC:
			TAILQ_REMOVE(&usb_generic_tasks, task, next);
			break;
		}
		task->onqueue = 0;
	}
	splx(s);
}

void
usb_rem_wait_task(usbd_device_handle dev, struct usb_task *task)
{
	int s;

	DPRINTFN(2,("%s: task=%p onqueue=%d type=%d\n", __func__, task,
	    task->onqueue, task->type));

	s = splusb();
	usb_rem_task(dev, task);
	while (task->running) {
		DPRINTF(("%s: waiting for task to complete\n", __func__));
		tsleep(task, PWAIT, "endtask", 0);
	}
	splx(s);
}

void
usb_first_explore(void *arg)
{
	struct usb_softc *sc = arg;
	struct timeval now, waited;
	int pwrdly, waited_ms;

	getmicrouptime(&now);
	timersub(&now, &sc->sc_ptime, &waited);
	waited_ms = waited.tv_sec * 1000 + waited.tv_usec / 1000;

	/* Wait for power to come good. */
	pwrdly = sc->sc_bus->root_hub->hub->hubdesc.bPwrOn2PwrGood * 
	    UHD_PWRON_FACTOR + USB_EXTRA_POWER_UP_TIME;
	if (pwrdly > waited_ms)
		usb_delay_ms(sc->sc_bus, pwrdly - waited_ms);

	/*
	 * USB1 waits for USB2 to finish their first probe.
	 * We only really need to have "companion" USB1 controllers
	 * wait, but it's hard to determine what's a companion and
	 * what isn't.
	 */
	while (sc->sc_bus->usbrev != USBREV_2_0 && explore_pending)
		(void)tsleep((void *)&explore_pending, PWAIT, "config", 0);

	/*
	 * Add first explore task to the queue.  The tasks are run in order
	 * in a single thread, so adding tasks to the queue in the correct
	 * order means they will run in the correct order.
	 */
	usb_needs_explore(sc->sc_bus->root_hub, 1);

	/* Wake up any companions waiting for handover before their probes. */
	if (sc->sc_bus->usbrev == USBREV_2_0) {
		explore_pending--;
		wakeup((void *)&explore_pending);
 	}
}

void
usb_task_thread(void *arg)
{
	struct usb_task *task;
	int s;

	DPRINTF(("usb_task_thread: start\n"));

	s = splusb();
	while (usb_run_tasks) {
		if ((task = TAILQ_FIRST(&usb_explore_tasks)) != NULL)
			TAILQ_REMOVE(&usb_explore_tasks, task, next);
		else if ((task = TAILQ_FIRST(&usb_generic_tasks)) != NULL)
			TAILQ_REMOVE(&usb_generic_tasks, task, next);
		else {
			tsleep(&usb_run_tasks, PWAIT, "usbtsk", 0);
			continue;
		}
		task->onqueue = 0;
		/* Don't execute the task if the root hub is gone. */
		if (usbd_is_dying(task->dev))
			continue;
		task->running = 1;
		splx(s);
		task->fun(task->arg);
		s = splusb();
		task->running = 0;
		wakeup(task);
	}
	splx(s);

	kthread_exit(0);
}

/*
 * This thread is ONLY for the HCI drivers to be able to abort xfers.
 * Synchronous xfers sleep the task thread, so the aborts need to happen
 * in a different thread.
 */
void
usb_abort_task_thread(void *arg)
{
	struct usb_task *task;
	int s;

	DPRINTF(("usb_xfer_abort_thread: start\n"));

	s = splusb();
	while (usb_run_abort_tasks) {
		if ((task = TAILQ_FIRST(&usb_abort_tasks)) != NULL)
			TAILQ_REMOVE(&usb_abort_tasks, task, next);
		else {
			tsleep(&usb_run_abort_tasks, PWAIT, "usbatsk", 0);
			continue;
		}
		task->onqueue = 0;
		/* Don't execute the task if the root hub is gone. */
		if (usbd_is_dying(task->dev))
			continue;
		task->running = 1;
		splx(s);
		task->fun(task->arg);
		s = splusb();
		task->running = 0;
		wakeup(task);
	}
	splx(s);

	kthread_exit(0);
}

int
usbctlprint(void *aux, const char *pnp)
{
	/* only "usb"es can attach to host controllers */
	if (pnp)
		printf("usb at %s", pnp);

	return (UNCONF);
}

int
usbopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = minor(dev);
	struct usb_softc *sc;

	if (unit >= usb_cd.cd_ndevs)
		return (ENXIO);
	sc = usb_cd.cd_devs[unit];
	if (sc == NULL)
		return (ENXIO);

	if (sc->sc_bus->dying)
		return (EIO);

	return (0);
}

int
usbclose(dev_t dev, int flag, int mode, struct proc *p)
{
	return (0);
}

int
usbioctl(dev_t devt, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct usb_softc *sc;
	int unit = minor(devt);
	int error;

	sc = usb_cd.cd_devs[unit];

	if (sc->sc_bus->dying)
		return (EIO);

	error = 0;
	switch (cmd) {
#ifdef USB_DEBUG
	case USB_SETDEBUG:
		/* only root can access to these debug flags */
		if ((error = suser(curproc, 0)) != 0)
			return (error);
		if (!(flag & FWRITE))
			return (EBADF);
		usbdebug  = ((*(unsigned int *)data) & 0x000000ff);
#if defined(UHCI_DEBUG) && NUHCI > 0
		uhcidebug = ((*(unsigned int *)data) & 0x0000ff00) >> 8;
#endif
#if defined(OHCI_DEBUG) && NOHCI > 0
		ohcidebug = ((*(unsigned int *)data) & 0x00ff0000) >> 16;
#endif
#if defined(EHCI_DEBUG) && NEHCI > 0
		ehcidebug = ((*(unsigned int *)data) & 0xff000000) >> 24;
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

	case USB_DEVICEINFO_48:
	{
		struct usb_device_info_48 *di_48 = (void *)data;
		struct usb_device_info di_tmp;
		int addr = di_48->udi_addr;
		usbd_device_handle dev;

		if (addr < 1 || addr >= USB_MAX_DEVICES)
			return (EINVAL);
		dev = sc->sc_bus->devices[addr];
		if (dev == NULL)
			return (ENXIO);

		bzero(&di_tmp, sizeof(struct usb_device_info));
		bcopy(di_48, &di_tmp, sizeof(struct usb_device_info_48));
		usbd_fill_deviceinfo(dev, &di_tmp, 1);
		bcopy(&di_tmp, di_48, sizeof(struct usb_device_info_48));
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

/*
 * Explore device tree from the root.  We need mutual exclusion to this
 * hub while traversing the device tree, but this is guaranteed since this
 * function is only called from the task thread, with one exception:
 * usb_attach() calls this function, but there shouldn't be anything else
 * trying to explore this hub at that time.
 */
void
usb_explore(void *v)
{
	struct usb_softc *sc = v;

	DPRINTFN(2,("%s: %s\n", __func__, sc->sc_dev.dv_xname));
#ifdef USB_DEBUG
	if (usb_noexplore)
		return;
#endif

	if (!sc->sc_bus->dying)
		sc->sc_bus->root_hub->hub->explore(sc->sc_bus->root_hub);

	if (sc->sc_bus->flags & USB_BUS_CONFIG_PENDING) {
		DPRINTF(("%s: %s: first explore done\n", __func__,
		    sc->sc_dev.dv_xname));
		config_pending_decr();
		sc->sc_bus->flags &= ~(USB_BUS_CONFIG_PENDING);
	}
}

void
usb_needs_explore(usbd_device_handle dev, int first_explore)
{
	DPRINTFN(3,("%s: %s\n", dev->bus->usbctl->sc_dev.dv_xname, __func__));

	if (!first_explore &&
	    (dev->bus->flags & USB_BUS_CONFIG_PENDING)) {
		DPRINTF(("%s: %s: not exploring before first explore\n",
		    __func__, dev->bus->usbctl->sc_dev.dv_xname));
		return;
	}

	usb_add_task(dev, &dev->bus->usbctl->sc_explore_task);
}

void
usb_needs_reattach(usbd_device_handle dev)
{
	DPRINTFN(2,("usb_needs_reattach\n"));
	dev->powersrc->reattach = 1;
	usb_needs_explore(dev, 0);
}

void
usb_schedsoftintr(usbd_bus_handle bus)
{
	DPRINTFN(10,("usb_schedsoftintr: polling=%d\n", bus->use_polling));

	if (bus->use_polling) {
		bus->methods->soft_intr(bus);
	} else {
		softintr_schedule(bus->soft);
	}
}

int
usb_activate(struct device *self, int act)
{
	struct usb_softc *sc = (struct usb_softc *)self;
	usbd_device_handle dev = sc->sc_port.device;
	int i, rv = 0, r;

	switch (act) {
	case DVACT_ACTIVATE:
		break;
	case DVACT_DEACTIVATE:
		sc->sc_bus->dying = 1;
		if (dev != NULL && dev->cdesc != NULL &&
		    dev->subdevs != NULL) {
			for (i = 0; dev->subdevs[i]; i++) {
				r = config_deactivate(dev->subdevs[i]);
				if (r)
					rv = r;
			}
		}
		break;
	}
	return (rv);
}

int
usb_detach(struct device *self, int flags)
{
	struct usb_softc *sc = (struct usb_softc *)self;

	DPRINTF(("usb_detach: start\n"));

	sc->sc_bus->dying = 1;

	if (sc->sc_bus->root_hub != NULL) {
		/* Make all devices disconnect. */
		if (sc->sc_port.device != NULL)
			usb_disconnect_port(&sc->sc_port, self);

		usb_rem_wait_task(sc->sc_bus->root_hub, &sc->sc_explore_task);

		usbd_finish();
	}

	if (sc->sc_bus->soft != NULL) {
		softintr_disestablish(sc->sc_bus->soft);
		sc->sc_bus->soft = NULL;
	}

	return (0);
}
