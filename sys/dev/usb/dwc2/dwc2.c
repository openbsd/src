/*	$OpenBSD: dwc2.c,v 1.20 2015/02/12 06:46:23 uebayasi Exp $	*/
/*	$NetBSD: dwc2.c,v 1.32 2014/09/02 23:26:20 macallan Exp $	*/

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Nick Hudson
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

#if 0
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dwc2.c,v 1.32 2014/09/02 23:26:20 macallan Exp $");
#endif

#if 0
#include "opt_usb.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/queue.h>
#if 0
#include <sys/cpu.h>
#endif

#include <machine/bus.h>
#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#if 0
#include <dev/usb/usbroothub_subr.h>
#endif

#include <dev/usb/dwc2/dwc2.h>
#include <dev/usb/dwc2/dwc2var.h>

#include <dev/usb/dwc2/dwc2_core.h>
#include <dev/usb/dwc2/dwc2_hcd.h>

#ifdef DWC2_COUNTERS
#define	DWC2_EVCNT_ADD(a,b)	((void)((a).ev_count += (b)))
#else
#define	DWC2_EVCNT_ADD(a,b)	do { } while (/*CONSTCOND*/0)
#endif
#define	DWC2_EVCNT_INCR(a)	DWC2_EVCNT_ADD((a), 1)

#ifdef DWC2_DEBUG
#define	DPRINTFN(n,fmt,...) do {			\
	if (dwc2debug >= (n)) {			\
		printf("%s: " fmt,			\
		__FUNCTION__,## __VA_ARGS__);		\
	}						\
} while (0)
#define	DPRINTF(...)	DPRINTFN(1, __VA_ARGS__)
int dwc2debug = 0;
#else
#define	DPRINTF(...) do { } while (0)
#define	DPRINTFN(...) do { } while (0)
#endif

STATIC usbd_status	dwc2_open(struct usbd_pipe *);
STATIC void		dwc2_poll(struct usbd_bus *);
STATIC void		dwc2_softintr(void *);
STATIC void		dwc2_waitintr(struct dwc2_softc *, struct usbd_xfer *);

#if 0
STATIC usbd_status	dwc2_allocm(struct usbd_bus *, struct usb_dma *, uint32_t);
STATIC void		dwc2_freem(struct usbd_bus *, struct usb_dma *);
#endif

STATIC struct usbd_xfer	*dwc2_allocx(struct usbd_bus *);
STATIC void		dwc2_freex(struct usbd_bus *, struct usbd_xfer *);
#if 0
STATIC void		dwc2_get_lock(struct usbd_bus *, struct mutex **);
#endif

STATIC usbd_status	dwc2_root_ctrl_transfer(struct usbd_xfer *);
STATIC usbd_status	dwc2_root_ctrl_start(struct usbd_xfer *);
STATIC void		dwc2_root_ctrl_abort(struct usbd_xfer *);
STATIC void		dwc2_root_ctrl_close(struct usbd_pipe *);
STATIC void		dwc2_root_ctrl_done(struct usbd_xfer *);

STATIC usbd_status	dwc2_root_intr_transfer(struct usbd_xfer *);
STATIC usbd_status	dwc2_root_intr_start(struct usbd_xfer *);
STATIC void		dwc2_root_intr_abort(struct usbd_xfer *);
STATIC void		dwc2_root_intr_close(struct usbd_pipe *);
STATIC void		dwc2_root_intr_done(struct usbd_xfer *);

STATIC usbd_status	dwc2_device_ctrl_transfer(struct usbd_xfer *);
STATIC usbd_status	dwc2_device_ctrl_start(struct usbd_xfer *);
STATIC void		dwc2_device_ctrl_abort(struct usbd_xfer *);
STATIC void		dwc2_device_ctrl_close(struct usbd_pipe *);
STATIC void		dwc2_device_ctrl_done(struct usbd_xfer *);

STATIC usbd_status	dwc2_device_bulk_transfer(struct usbd_xfer *);
STATIC usbd_status	dwc2_device_bulk_start(struct usbd_xfer *);
STATIC void		dwc2_device_bulk_abort(struct usbd_xfer *);
STATIC void		dwc2_device_bulk_close(struct usbd_pipe *);
STATIC void		dwc2_device_bulk_done(struct usbd_xfer *);

STATIC usbd_status	dwc2_device_intr_transfer(struct usbd_xfer *);
STATIC usbd_status	dwc2_device_intr_start(struct usbd_xfer *);
STATIC void		dwc2_device_intr_abort(struct usbd_xfer *);
STATIC void		dwc2_device_intr_close(struct usbd_pipe *);
STATIC void		dwc2_device_intr_done(struct usbd_xfer *);

STATIC usbd_status	dwc2_device_isoc_transfer(struct usbd_xfer *);
STATIC usbd_status	dwc2_device_isoc_start(struct usbd_xfer *);
STATIC void		dwc2_device_isoc_abort(struct usbd_xfer *);
STATIC void		dwc2_device_isoc_close(struct usbd_pipe *);
STATIC void		dwc2_device_isoc_done(struct usbd_xfer *);

STATIC usbd_status	dwc2_device_start(struct usbd_xfer *);

STATIC void		dwc2_close_pipe(struct usbd_pipe *);
STATIC void		dwc2_abort_xfer(struct usbd_xfer *, usbd_status);

STATIC void		dwc2_device_clear_toggle(struct usbd_pipe *);
STATIC void		dwc2_noop(struct usbd_pipe *pipe);

STATIC int		dwc2_interrupt(struct dwc2_softc *);
STATIC void		dwc2_rhc(void *);

STATIC void		dwc2_timeout(void *);
STATIC void		dwc2_timeout_task(void *);

STATIC_INLINE void
dwc2_allocate_bus_bandwidth(struct dwc2_hsotg *hsotg, u16 bw,
			    struct usbd_xfer *xfer)
{
}

STATIC_INLINE void
dwc2_free_bus_bandwidth(struct dwc2_hsotg *hsotg, u16 bw,
			struct usbd_xfer *xfer)
{
}

#define DWC2_INTR_ENDPT 1

STATIC struct usbd_bus_methods dwc2_bus_methods = {
	.open_pipe =	dwc2_open,
	.soft_intr =	dwc2_softintr,
	.do_poll =	dwc2_poll,
#if 0
	.allocm =	dwc2_allocm,
	.freem =	dwc2_freem,
#endif
	.allocx =	dwc2_allocx,
	.freex =	dwc2_freex,
#if 0
	.get_lock =	dwc2_get_lock,
#endif
};

STATIC struct usbd_pipe_methods dwc2_root_ctrl_methods = {
	.transfer =	dwc2_root_ctrl_transfer,
	.start =	dwc2_root_ctrl_start,
	.abort =	dwc2_root_ctrl_abort,
	.close =	dwc2_root_ctrl_close,
	.cleartoggle =	dwc2_noop,
	.done =		dwc2_root_ctrl_done,
};

STATIC struct usbd_pipe_methods dwc2_root_intr_methods = {
	.transfer =	dwc2_root_intr_transfer,
	.start =	dwc2_root_intr_start,
	.abort =	dwc2_root_intr_abort,
	.close =	dwc2_root_intr_close,
	.cleartoggle =	dwc2_noop,
	.done =		dwc2_root_intr_done,
};

STATIC struct usbd_pipe_methods dwc2_device_ctrl_methods = {
	.transfer =	dwc2_device_ctrl_transfer,
	.start =	dwc2_device_ctrl_start,
	.abort =	dwc2_device_ctrl_abort,
	.close =	dwc2_device_ctrl_close,
	.cleartoggle =	dwc2_noop,
	.done =		dwc2_device_ctrl_done,
};

STATIC struct usbd_pipe_methods dwc2_device_intr_methods = {
	.transfer =	dwc2_device_intr_transfer,
	.start =	dwc2_device_intr_start,
	.abort =	dwc2_device_intr_abort,
	.close =	dwc2_device_intr_close,
	.cleartoggle =	dwc2_device_clear_toggle,
	.done =		dwc2_device_intr_done,
};

STATIC struct usbd_pipe_methods dwc2_device_bulk_methods = {
	.transfer =	dwc2_device_bulk_transfer,
	.start =	dwc2_device_bulk_start,
	.abort =	dwc2_device_bulk_abort,
	.close =	dwc2_device_bulk_close,
	.cleartoggle =	dwc2_device_clear_toggle,
	.done =		dwc2_device_bulk_done,
};

STATIC struct usbd_pipe_methods dwc2_device_isoc_methods = {
	.transfer =	dwc2_device_isoc_transfer,
	.start =	dwc2_device_isoc_start,
	.abort =	dwc2_device_isoc_abort,
	.close =	dwc2_device_isoc_close,
	.cleartoggle =	dwc2_noop,
	.done =		dwc2_device_isoc_done,
};

#if 0
STATIC usbd_status
dwc2_allocm(struct usbd_bus *bus, struct usb_dma *dma, uint32_t size)
{
	struct dwc2_softc *sc = DWC2_BUS2SC(bus);
	usbd_status status;

	DPRINTFN(10, "\n");

	status = usb_allocmem(&sc->sc_bus, size, 0, dma);
	if (status == USBD_NOMEM)
		status = usb_reserve_allocm(&sc->sc_dma_reserve, dma, size);
	return status;
}

STATIC void
dwc2_freem(struct usbd_bus *bus, struct usb_dma *dma)
{
	struct dwc2_softc *sc = DWC2_BUS2SC(bus);

	DPRINTFN(10, "\n");

	if (dma->block->flags & USB_DMA_RESERVE) {
		usb_reserve_freem(&sc->sc_dma_reserve, dma);
		return;
	}
	usb_freemem(&sc->sc_bus, dma);
}
#endif

struct usbd_xfer *
dwc2_allocx(struct usbd_bus *bus)
{
	struct dwc2_softc *sc = DWC2_BUS2SC(bus);
	struct dwc2_xfer *dxfer;

	DPRINTFN(10, "\n");

	DWC2_EVCNT_INCR(sc->sc_ev_xferpoolget);
	dxfer = pool_get(&sc->sc_xferpool, PR_NOWAIT);
	if (dxfer != NULL) {
		memset(dxfer, 0, sizeof(*dxfer));

		dxfer->urb = dwc2_hcd_urb_alloc(sc->sc_hsotg,
		    DWC2_MAXISOCPACKETS, GFP_KERNEL);

#ifdef DWC2_DEBUG
		dxfer->xfer.busy_free = XFER_ONQU;
#endif
	}
	return (struct usbd_xfer *)dxfer;
}

void
dwc2_freex(struct usbd_bus *bus, struct usbd_xfer *xfer)
{
	struct dwc2_xfer *dxfer = DWC2_XFER2DXFER(xfer);
	struct dwc2_softc *sc = DWC2_BUS2SC(bus);

	DPRINTFN(10, "\n");

#ifdef DWC2_DEBUG
	if (xfer->busy_free != XFER_ONQU) {
		DPRINTF("xfer=%p not busy, 0x%08x\n", xfer, xfer->busy_free);
	}
	xfer->busy_free = XFER_FREE;
#endif
	DWC2_EVCNT_INCR(sc->sc_ev_xferpoolput);
	dwc2_hcd_urb_free(sc->sc_hsotg, dxfer->urb, DWC2_MAXISOCPACKETS);
	pool_put(&sc->sc_xferpool, xfer);
}

#if 0
STATIC void
dwc2_get_lock(struct usbd_bus *bus, struct mutex **lock)
{
	struct dwc2_softc *sc = DWC2_BUS2SC(bus);

	*lock = &sc->sc_lock;
}
#endif

STATIC void
dwc2_rhc(void *addr)
{
	struct dwc2_softc *sc = addr;
	struct usbd_xfer *xfer;
	u_char *p;

	DPRINTF("\n");
	mtx_enter(&sc->sc_lock);
	xfer = sc->sc_intrxfer;

	if (xfer == NULL) {
		/* Just ignore the change. */
		mtx_leave(&sc->sc_lock);
		return;

	}
	/* set port bit */
	p = KERNADDR(&xfer->dmabuf, 0);

	p[0] = 0x02;	/* we only have one port (1 << 1) */

	xfer->actlen = xfer->length;
	xfer->status = USBD_NORMAL_COMPLETION;

	usb_transfer_complete(xfer);
	mtx_leave(&sc->sc_lock);
}

STATIC void
dwc2_softintr(void *v)
{
	struct usbd_bus *bus = v;
	struct dwc2_softc *sc = DWC2_BUS2SC(bus);
	struct dwc2_hsotg *hsotg = sc->sc_hsotg;
	struct dwc2_xfer *dxfer;

	KASSERT(sc->sc_bus.use_polling || mtx_owned(&sc->sc_lock));

	mtx_enter(&hsotg->lock);
	while ((dxfer = TAILQ_FIRST(&sc->sc_complete)) != NULL) {

		KASSERTMSG(!timeout_pending(&dxfer->xfer.timeout_handle), 
		    "xfer %p pipe %p\n", dxfer, dxfer->xfer.pipe);

		/*
		 * dwc2_abort_xfer will remove this transfer from the
		 * sc_complete queue
		 */
		/*XXXNH not tested */
		if (dxfer->flags & DWC2_XFER_ABORTING) {
			wakeup(&dxfer->flags);
			continue;
		}

		TAILQ_REMOVE(&sc->sc_complete, dxfer, xnext);

		mtx_leave(&hsotg->lock);
		usb_transfer_complete(&dxfer->xfer);
		mtx_enter(&hsotg->lock);
	}
	mtx_leave(&hsotg->lock);
}

STATIC void
dwc2_waitintr(struct dwc2_softc *sc, struct usbd_xfer *xfer)
{
	struct dwc2_hsotg *hsotg = sc->sc_hsotg;
	uint32_t intrs;
	int timo;

	xfer->status = USBD_IN_PROGRESS;
	for (timo = xfer->timeout; timo >= 0; timo--) {
		usb_delay_ms(&sc->sc_bus, 1);
		if (sc->sc_dying)
			break;
		intrs = dwc2_read_core_intr(hsotg);

		DPRINTFN(15, "0x%08x\n", intrs);

		if (intrs) {
			mtx_enter(&hsotg->lock);
			dwc2_interrupt(sc);
			mtx_leave(&hsotg->lock);
			if (xfer->status != USBD_IN_PROGRESS)
				return;
		}
	}

	/* Timeout */
	DPRINTF("timeout\n");

	mtx_enter(&sc->sc_lock);
	xfer->status = USBD_TIMEOUT;
	usb_transfer_complete(xfer);
	mtx_leave(&sc->sc_lock);
}

STATIC void
dwc2_timeout(void *addr)
{
	struct usbd_xfer *xfer = addr;
	struct dwc2_xfer *dxfer = DWC2_XFER2DXFER(xfer);
// 	struct dwc2_pipe *dpipe = DWC2_XFER2DPIPE(xfer);
 	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);

	DPRINTF("dxfer=%p\n", dxfer);

	if (sc->sc_dying) {
		mtx_enter(&sc->sc_lock);
		dwc2_abort_xfer(&dxfer->xfer, USBD_TIMEOUT);
		mtx_leave(&sc->sc_lock);
		return;
	}

	/* Execute the abort in a process context. */
	usb_init_task(&dxfer->abort_task, dwc2_timeout_task, addr,
	    USB_TASK_TYPE_GENERIC);
	usb_add_task(dxfer->xfer.pipe->device, &dxfer->abort_task);
}

STATIC void
dwc2_timeout_task(void *addr)
{
	struct usbd_xfer *xfer = addr;
 	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);

	DPRINTF("xfer=%p\n", xfer);

	mtx_enter(&sc->sc_lock);
	dwc2_abort_xfer(xfer, USBD_TIMEOUT);
	mtx_leave(&sc->sc_lock);
}

usbd_status
dwc2_open(struct usbd_pipe *pipe)
{
	struct usbd_device *dev = pipe->device;
	struct dwc2_softc *sc = DWC2_PIPE2SC(pipe);
	struct dwc2_pipe *dpipe = DWC2_PIPE2DPIPE(pipe);
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	uint8_t addr = dev->address;
	uint8_t xfertype = UE_GET_XFERTYPE(ed->bmAttributes);
	usbd_status err;

	DPRINTF("pipe %p addr %d xfertype %d dir %s\n", pipe, addr, xfertype,
	    UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN ? "in" : "out");

	if (sc->sc_dying) {
		return USBD_IOERROR;
	}

	if (addr == sc->sc_addr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &dwc2_root_ctrl_methods;
			break;
		case UE_DIR_IN | DWC2_INTR_ENDPT:
			pipe->methods = &dwc2_root_intr_methods;
			break;
		default:
			DPRINTF("bad bEndpointAddress 0x%02x\n",
			    ed->bEndpointAddress);
			return USBD_INVAL;
		}
		DPRINTF("root hub pipe open\n");
		return USBD_NORMAL_COMPLETION;
	}

	switch (xfertype) {
	case UE_CONTROL:
		pipe->methods = &dwc2_device_ctrl_methods;
		err = usb_allocmem(&sc->sc_bus, sizeof(usb_device_request_t),
		    0, &dpipe->req_dma);
		if (err)
			return err;
		break;
	case UE_INTERRUPT:
		pipe->methods = &dwc2_device_intr_methods;
		break;
	case UE_ISOCHRONOUS:
		pipe->methods = &dwc2_device_isoc_methods;
		break;
	case UE_BULK:
		pipe->methods = &dwc2_device_bulk_methods;
		break;
	default:
		DPRINTF("bad xfer type %d\n", xfertype);
		return USBD_INVAL;
	}

	dpipe->priv = NULL;	/* QH */

	return USBD_NORMAL_COMPLETION;
}

STATIC void
dwc2_poll(struct usbd_bus *bus)
{
	struct dwc2_softc *sc = DWC2_BUS2SC(bus);
	struct dwc2_hsotg *hsotg = sc->sc_hsotg;

	mtx_enter(&hsotg->lock);
	dwc2_interrupt(sc);
	mtx_leave(&hsotg->lock);
}

/*
 * Close a reqular pipe.
 * Assumes that there are no pending transactions.
 */
STATIC void
dwc2_close_pipe(struct usbd_pipe *pipe)
{
#ifdef DIAGNOSTIC
	struct dwc2_softc *sc = DWC2_PIPE2SC(pipe);
#endif

	KASSERT(mtx_owned(&sc->sc_lock));
}

/*
 * Abort a device request.
 */
STATIC void
dwc2_abort_xfer(struct usbd_xfer *xfer, usbd_status status)
{
	struct dwc2_xfer *dxfer = DWC2_XFER2DXFER(xfer);
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	struct dwc2_hsotg *hsotg = sc->sc_hsotg;
	struct dwc2_xfer *d, *tmp;
	bool wake;
	int err;

	DPRINTF("xfer=%p\n", xfer);

	KASSERT(mtx_owned(&sc->sc_lock));
	//KASSERT(!cpu_intr_p() && !cpu_softintr_p());

	if (sc->sc_dying) {
		xfer->status = status;
		timeout_del(&xfer->timeout_handle);
		usb_transfer_complete(xfer);
		return;
	}

	/*
	 * If an abort is already in progress then just wait for it to
	 * complete and return.
	 */
	if (dxfer->flags & DWC2_XFER_ABORTING) {
		xfer->status = status;
		dxfer->flags |= DWC2_XFER_ABORTWAIT;
		while (dxfer->flags & DWC2_XFER_ABORTING)
			tsleep(&dxfer->flags, PZERO, "dwc2xfer", 0);
		return;
	}

	/*
	 * Step 1: Make the stack ignore it and stop the timeout.
	 */
	mtx_enter(&hsotg->lock);
	dxfer->flags |= DWC2_XFER_ABORTING;

	xfer->status = status;	/* make software ignore it */
	timeout_del(&xfer->timeout_handle);

	/* XXXNH suboptimal */
	TAILQ_FOREACH_SAFE(d, &sc->sc_complete, xnext, tmp) {
		if (d == dxfer) {
			TAILQ_REMOVE(&sc->sc_complete, dxfer, xnext);
		}
	}

	err = dwc2_hcd_urb_dequeue(hsotg, dxfer->urb);
	if (err) {
		DPRINTF("dwc2_hcd_urb_dequeue failed\n");
	}

	mtx_leave(&hsotg->lock);

	/*
	 * Step 2: Execute callback.
	 */
	wake = dxfer->flags & DWC2_XFER_ABORTWAIT;
	dxfer->flags &= ~(DWC2_XFER_ABORTING | DWC2_XFER_ABORTWAIT);

	usb_transfer_complete(xfer);
	if (wake) {
		wakeup(&dxfer->flags);
	}
}

STATIC void
dwc2_noop(struct usbd_pipe *pipe)
{

}

STATIC void
dwc2_device_clear_toggle(struct usbd_pipe *pipe)
{

	DPRINTF("toggle %d -> 0", pipe->endpoint->savedtoggle);
}

/***********************************************************************/

/*
 * Data structures and routines to emulate the root hub.
 */

STATIC const usb_device_descriptor_t dwc2_devd = {
	.bLength = sizeof(usb_device_descriptor_t),
	.bDescriptorType = UDESC_DEVICE,
	.bcdUSB = {0x00, 0x02},
	.bDeviceClass = UDCLASS_HUB,
	.bDeviceSubClass = UDSUBCLASS_HUB,
	.bDeviceProtocol = UDPROTO_HSHUBSTT,
	.bMaxPacketSize = 64,
	.bcdDevice = {0x00, 0x01},
	.iManufacturer = 1,
	.iProduct = 2,
	.bNumConfigurations = 1,
};

struct dwc2_config_desc {
	usb_config_descriptor_t confd;
	usb_interface_descriptor_t ifcd;
	usb_endpoint_descriptor_t endpd;
} __packed;

STATIC const struct dwc2_config_desc dwc2_confd = {
	.confd = {
		.bLength = USB_CONFIG_DESCRIPTOR_SIZE,
		.bDescriptorType = UDESC_CONFIG,
		.wTotalLength[0] = sizeof(dwc2_confd),
		.bNumInterface = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = UC_SELF_POWERED,
		.bMaxPower = 0,
	},
	.ifcd = {
		.bLength = USB_INTERFACE_DESCRIPTOR_SIZE,
		.bDescriptorType = UDESC_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 1,
		.bInterfaceClass = UICLASS_HUB,
		.bInterfaceSubClass = UISUBCLASS_HUB,
		.bInterfaceProtocol = UIPROTO_HSHUBSTT,
		.iInterface = 0
	},
	.endpd = {
		.bLength = USB_ENDPOINT_DESCRIPTOR_SIZE,
		.bDescriptorType = UDESC_ENDPOINT,
		.bEndpointAddress = UE_DIR_IN | DWC2_INTR_ENDPT,
		.bmAttributes = UE_INTERRUPT,
		.wMaxPacketSize = {8, 0},			/* max packet */
		.bInterval = 255,
	},
};

#define	HSETW(ptr, val) ptr = { (uint8_t)(val), (uint8_t)((val) >> 8) }
#if 0
/* appears to be unused */
STATIC const usb_hub_descriptor_t dwc2_hubd = {
	.bDescLength = USB_HUB_DESCRIPTOR_SIZE,
	.bDescriptorType = UDESC_HUB,
	.bNbrPorts = 1,
	HSETW(.wHubCharacteristics, (UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL)),
	.bPwrOn2PwrGood = 50,
	.bHubContrCurrent = 0,
	.DeviceRemovable = {0},		/* port is removable */
};
#endif

STATIC usbd_status
dwc2_root_ctrl_transfer(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	usbd_status err;

	mtx_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mtx_leave(&sc->sc_lock);
	if (err)
		return err;

	return dwc2_root_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue));
}

STATIC usbd_status
dwc2_root_ctrl_start(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	usb_device_request_t *req;
	uint8_t *buf;
	uint16_t len;
	int value, index, l, totlen;
	usbd_status err = USBD_IOERROR;

	if (sc->sc_dying)
		return USBD_IOERROR;

	req = &xfer->request;

	DPRINTFN(4, "type=0x%02x request=%02x\n",
	    req->bmRequestType, req->bRequest);

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	buf = len ? KERNADDR(&xfer->dmabuf, 0) : NULL;

	totlen = 0;

#define C(x,y) ((x) | ((y) << 8))
	switch (C(req->bRequest, req->bmRequestType)) {
	case C(UR_CLEAR_FEATURE, UT_WRITE_DEVICE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		/*
		 * DEVICE_REMOTE_WAKEUP and ENDPOINT_HALT are no-ops
		 * for the integrated root hub.
		 */
		break;
	case C(UR_GET_CONFIG, UT_READ_DEVICE):
		if (len > 0) {
			*buf = sc->sc_conf;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		DPRINTFN(8, "wValue=0x%04x\n", value);

		if (len == 0)
			break;
		switch (value) {
		case C(0, UDESC_DEVICE):
			l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
// 			USETW(dwc2_devd.idVendor, sc->sc_id_vendor);
			memcpy(buf, &dwc2_devd, l);
			buf += l;
			len -= l;
			totlen += l;

			break;
		case C(0, UDESC_CONFIG):
			l = min(len, sizeof(dwc2_confd));
			memcpy(buf, &dwc2_confd, l);
			buf += l;
			len -= l;
			totlen += l;

			break;
#define sd ((usb_string_descriptor_t *)buf)
		case C(0, UDESC_STRING):
			totlen = usbd_str(sd, len, "\001");
			break;
		case C(1, UDESC_STRING):
			totlen = usbd_str(sd, len, sc->sc_vendor);
			break;
		case C(2, UDESC_STRING):
			totlen = usbd_str(sd, len, "DWC2 root hub");
			break;
#undef sd
		default:
			goto fail;
		}
		break;
	case C(UR_GET_INTERFACE, UT_READ_INTERFACE):
		if (len > 0) {
			*buf = 0;
			totlen = 1;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_DEVICE):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus,UDS_SELF_POWERED);
			totlen = 2;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_INTERFACE):
	case C(UR_GET_STATUS, UT_READ_ENDPOINT):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus, 0);
			totlen = 2;
		}
		break;
	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		DPRINTF("UR_SET_ADDRESS, UT_WRITE_DEVICE: addr %d\n",
		    value);
		if (value >= USB_MAX_DEVICES)
			goto fail;

		sc->sc_addr = value;
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1)
			goto fail;

		sc->sc_conf = value;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		err = USBD_IOERROR;
		goto fail;
	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		break;
	case C(UR_SYNCH_FRAME, UT_WRITE_ENDPOINT):
		break;
	default:
		/* Hub requests - XXXNH len check? */
		err = dwc2_hcd_hub_control(sc->sc_hsotg,
		    C(req->bRequest, req->bmRequestType), value, index,
		    buf, len);
		if (err) {
			err = USBD_IOERROR;
			goto fail;
		}
		totlen = len;
	}
	xfer->actlen = totlen;
	err = USBD_NORMAL_COMPLETION;

fail:
	mtx_enter(&sc->sc_lock);
	xfer->status = err;
	usb_transfer_complete(xfer);
	mtx_leave(&sc->sc_lock);

	return USBD_IN_PROGRESS;
}

STATIC void
dwc2_root_ctrl_abort(struct usbd_xfer *xfer)
{
	DPRINTFN(10, "\n");

	/* Nothing to do, all transfers are synchronous. */
}

STATIC void
dwc2_root_ctrl_close(struct usbd_pipe *pipe)
{
	DPRINTFN(10, "\n");

	/* Nothing to do. */
}

STATIC void
dwc2_root_ctrl_done(struct usbd_xfer *xfer)
{
	DPRINTFN(10, "\n");

	/* Nothing to do. */
}

STATIC usbd_status
dwc2_root_intr_transfer(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	usbd_status err;

	DPRINTF("\n");

	/* Insert last in queue. */
	mtx_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mtx_leave(&sc->sc_lock);
	if (err)
		return err;

	/* Pipe isn't running, start first */
	return dwc2_root_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue));
}

STATIC usbd_status
dwc2_root_intr_start(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);

	DPRINTF("\n");

	if (sc->sc_dying)
		return USBD_IOERROR;

	mtx_enter(&sc->sc_lock);
	KASSERT(sc->sc_intrxfer == NULL);
	sc->sc_intrxfer = xfer;
	mtx_leave(&sc->sc_lock);

	return USBD_IN_PROGRESS;
}

/* Abort a root interrupt request. */
STATIC void
dwc2_root_intr_abort(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);

	DPRINTF("xfer=%p\n", xfer);

	KASSERT(mtx_owned(&sc->sc_lock));
	KASSERT(xfer->pipe->intrxfer == xfer);

	sc->sc_intrxfer = NULL;

	xfer->status = USBD_CANCELLED;
	usb_transfer_complete(xfer);
}

STATIC void
dwc2_root_intr_close(struct usbd_pipe *pipe)
{
	struct dwc2_softc *sc = DWC2_PIPE2SC(pipe);

	DPRINTF("\n");

	KASSERT(mtx_owned(&sc->sc_lock));

	sc->sc_intrxfer = NULL;
}

STATIC void
dwc2_root_intr_done(struct usbd_xfer *xfer)
{

	DPRINTF("\n");
}

/***********************************************************************/

STATIC usbd_status
dwc2_device_ctrl_transfer(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	usbd_status err;

	DPRINTF("\n");

	/* Insert last in queue. */
	mtx_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mtx_leave(&sc->sc_lock);
	if (err)
		return err;

	/* Pipe isn't running, start first */
	return dwc2_device_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue));
}

STATIC usbd_status
dwc2_device_ctrl_start(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	usbd_status err;

	DPRINTF("\n");

	mtx_enter(&sc->sc_lock);
	xfer->status = USBD_IN_PROGRESS;
	err = dwc2_device_start(xfer);
	mtx_leave(&sc->sc_lock);

	if (err)
		return err;

	if (sc->sc_bus.use_polling)
		dwc2_waitintr(sc, xfer);

	return USBD_IN_PROGRESS;
}

STATIC void
dwc2_device_ctrl_abort(struct usbd_xfer *xfer)
{
#ifdef DWC2_DEBUG
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
#endif
	KASSERT(mtx_owned(&sc->sc_lock));

	DPRINTF("xfer=%p\n", xfer);
	dwc2_abort_xfer(xfer, USBD_CANCELLED);
}

STATIC void
dwc2_device_ctrl_close(struct usbd_pipe *pipe)
{

	DPRINTF("pipe=%p\n", pipe);
	dwc2_close_pipe(pipe);
}

STATIC void
dwc2_device_ctrl_done(struct usbd_xfer *xfer)
{

	DPRINTF("xfer=%p\n", xfer);
}

/***********************************************************************/

STATIC usbd_status
dwc2_device_bulk_transfer(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	usbd_status err;

	DPRINTF("xfer=%p\n", xfer);

	/* Insert last in queue. */
	mtx_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mtx_leave(&sc->sc_lock);
	if (err)
		return err;

	/* Pipe isn't running, start first */
	return dwc2_device_bulk_start(SIMPLEQ_FIRST(&xfer->pipe->queue));
}

STATIC usbd_status
dwc2_device_bulk_start(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	usbd_status err;

	DPRINTF("xfer=%p\n", xfer);
	mtx_enter(&sc->sc_lock);
	xfer->status = USBD_IN_PROGRESS;
	err = dwc2_device_start(xfer);
	mtx_leave(&sc->sc_lock);

	return err;
}

STATIC void
dwc2_device_bulk_abort(struct usbd_xfer *xfer)
{
#ifdef DWC2_DEBUG
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
#endif
	KASSERT(mtx_owned(&sc->sc_lock));

	DPRINTF("xfer=%p\n", xfer);
	dwc2_abort_xfer(xfer, USBD_CANCELLED);
}

STATIC void
dwc2_device_bulk_close(struct usbd_pipe *pipe)
{

	DPRINTF("pipe=%p\n", pipe);

	dwc2_close_pipe(pipe);
}

STATIC void
dwc2_device_bulk_done(struct usbd_xfer *xfer)
{

	DPRINTF("xfer=%p\n", xfer);
}

/***********************************************************************/

STATIC usbd_status
dwc2_device_intr_transfer(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	usbd_status err;

	DPRINTF("xfer=%p\n", xfer);

	/* Insert last in queue. */
	mtx_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mtx_leave(&sc->sc_lock);
	if (err)
		return err;

	/* Pipe isn't running, start first */
	return dwc2_device_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue));
}

STATIC usbd_status
dwc2_device_intr_start(struct usbd_xfer *xfer)
{
	struct dwc2_pipe *dpipe = DWC2_XFER2DPIPE(xfer)
	struct dwc2_softc *sc = DWC2_DPIPE2SC(dpipe);
	usbd_status err;

	mtx_enter(&sc->sc_lock);
	xfer->status = USBD_IN_PROGRESS;
	err = dwc2_device_start(xfer);
	mtx_leave(&sc->sc_lock);

	if (err)
		return err;

	if (sc->sc_bus.use_polling)
		dwc2_waitintr(sc, xfer);

	return USBD_IN_PROGRESS;
}

/* Abort a device interrupt request. */
STATIC void
dwc2_device_intr_abort(struct usbd_xfer *xfer)
{
#ifdef DWC2_DEBUG
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
#endif

	KASSERT(mtx_owned(&sc->sc_lock));
	KASSERT(xfer->pipe->intrxfer == xfer);

	DPRINTF("xfer=%p\n", xfer);

	dwc2_abort_xfer(xfer, USBD_CANCELLED);
}

STATIC void
dwc2_device_intr_close(struct usbd_pipe *pipe)
{

	DPRINTF("pipe=%p\n", pipe);

	dwc2_close_pipe(pipe);
}

STATIC void
dwc2_device_intr_done(struct usbd_xfer *xfer)
{

	DPRINTF("\n");

	if (xfer->pipe->repeat) {
		xfer->status = USBD_IN_PROGRESS;
		dwc2_device_start(xfer);
	}
}

/***********************************************************************/

usbd_status
dwc2_device_isoc_transfer(struct usbd_xfer *xfer)
{
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	usbd_status err;

	DPRINTF("xfer=%p\n", xfer);

	/* Insert last in queue. */
	mtx_enter(&sc->sc_lock);
	err = usb_insert_transfer(xfer);
	mtx_leave(&sc->sc_lock);
	if (err)
		return err;

	/* Pipe isn't running, start first */
	return dwc2_device_isoc_start(SIMPLEQ_FIRST(&xfer->pipe->queue));
}

usbd_status
dwc2_device_isoc_start(struct usbd_xfer *xfer)
{
	struct dwc2_pipe *dpipe = DWC2_XFER2DPIPE(xfer);
	struct dwc2_softc *sc = DWC2_DPIPE2SC(dpipe);
	usbd_status err;

	mtx_enter(&sc->sc_lock);
	xfer->status = USBD_IN_PROGRESS;
	err = dwc2_device_start(xfer);
	mtx_leave(&sc->sc_lock);

	if (sc->sc_bus.use_polling)
		dwc2_waitintr(sc, xfer);

	return err;
}

void
dwc2_device_isoc_abort(struct usbd_xfer *xfer)
{
#ifdef DWC2_DEBUG
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
#endif
	KASSERT(mtx_owned(&sc->sc_lock));

	DPRINTF("xfer=%p\n", xfer);
	dwc2_abort_xfer(xfer, USBD_CANCELLED);
}

void
dwc2_device_isoc_close(struct usbd_pipe *pipe)
{
	DPRINTF("\n");

	dwc2_close_pipe(pipe);
}

void
dwc2_device_isoc_done(struct usbd_xfer *xfer)
{

	DPRINTF("\n");
}


usbd_status
dwc2_device_start(struct usbd_xfer *xfer)
{
 	struct dwc2_xfer *dxfer = DWC2_XFER2DXFER(xfer);
	struct dwc2_pipe *dpipe = DWC2_XFER2DPIPE(xfer);
	struct dwc2_softc *sc = DWC2_XFER2SC(xfer);
	struct dwc2_hsotg *hsotg = sc->sc_hsotg;
	struct dwc2_hcd_urb *dwc2_urb;

	struct usbd_device *dev = xfer->pipe->device;
	usb_endpoint_descriptor_t *ed = xfer->pipe->endpoint->edesc;
	uint8_t addr = dev->address;
	uint8_t xfertype = UE_GET_XFERTYPE(ed->bmAttributes);
	uint8_t epnum = UE_GET_ADDR(ed->bEndpointAddress);
	uint8_t dir = UE_GET_DIR(ed->bEndpointAddress);
	uint16_t mps = UE_GET_SIZE(UGETW(ed->wMaxPacketSize));
	uint32_t len;

	uint32_t flags = 0;
	uint32_t off = 0;
	int retval, err = USBD_IN_PROGRESS;
	int alloc_bandwidth = 0;
	int i;

	DPRINTFN(1, "xfer=%p pipe=%p\n", xfer, xfer->pipe);

	if (xfertype == UE_ISOCHRONOUS ||
	    xfertype == UE_INTERRUPT) {
		mtx_enter(&hsotg->lock);
		if (!dwc2_hcd_is_bandwidth_allocated(hsotg, xfer))
			alloc_bandwidth = 1;
		mtx_leave(&hsotg->lock);
	}

	/*
	 * For Control pipe the direction is from the request, all other
	 * transfers have been set correctly at pipe open time.
	 */
	if (xfertype == UE_CONTROL) {
		usb_device_request_t *req = &xfer->request;

		DPRINTFN(3, "xfer=%p type=0x%02x request=0x%02x wValue=0x%04x "
		    "wIndex=0x%04x len=%d addr=%d endpt=%d dir=%s speed=%d "
		    "mps=%d\n",
		    xfer, req->bmRequestType, req->bRequest, UGETW(req->wValue),
		    UGETW(req->wIndex), UGETW(req->wLength), dev->address,
		    epnum, dir == UT_READ ? "in" :"out", dev->speed, mps);

		/* Copy request packet to our DMA buffer */
		memcpy(KERNADDR(&dpipe->req_dma, 0), req, sizeof(*req));
		usb_syncmem(&dpipe->req_dma, 0, sizeof(*req),
			    BUS_DMASYNC_PREWRITE);
		len = UGETW(req->wLength);
		if ((req->bmRequestType & UT_READ) == UT_READ) {
			dir = UE_DIR_IN;
		} else {
			dir = UE_DIR_OUT;
		}

		DPRINTFN(3, "req = %p dma = %llx len %d dir %s\n",
		    KERNADDR(&dpipe->req_dma, 0),
		    (long long)DMAADDR(&dpipe->req_dma, 0),
		    len, dir == UE_DIR_IN ? "in" : "out");
	} else {
		DPRINTFN(3, "xfer=%p len=%d flags=%d addr=%d endpt=%d,"
		    " mps=%d dir %s\n", xfer, xfer->length, xfer->flags, addr,
		    epnum, mps, dir == UT_READ ? "in" :"out");

		len = xfer->length;
	}

	dwc2_urb = dxfer->urb;
	if (!dwc2_urb)
		return USBD_NOMEM;

	memset(dwc2_urb, 0, sizeof(*dwc2_urb) +
	    sizeof(dwc2_urb->iso_descs[0]) * DWC2_MAXISOCPACKETS);

	dwc2_hcd_urb_set_pipeinfo(hsotg, dwc2_urb, addr, epnum, xfertype, dir,
				  mps);

	if (xfertype == UE_CONTROL) {
		dwc2_urb->setup_usbdma = &dpipe->req_dma;
		dwc2_urb->setup_packet = KERNADDR(&dpipe->req_dma, 0);
		dwc2_urb->setup_dma = DMAADDR(&dpipe->req_dma, 0);
	} else {
		/* XXXNH - % mps required? */
		if ((xfer->flags & USBD_FORCE_SHORT_XFER) && (len % mps) == 0)
			flags |= URB_SEND_ZERO_PACKET;
	}
	flags |= URB_GIVEBACK_ASAP;

	/*
	 * control transfers with no data phase don't touch usbdma, but
	 * everything else does.
	 */
	if (!(xfertype == UE_CONTROL && len == 0)) {
		dwc2_urb->usbdma = &xfer->dmabuf;
		dwc2_urb->buf = KERNADDR(dwc2_urb->usbdma, 0);
		dwc2_urb->dma = DMAADDR(dwc2_urb->usbdma, 0);
 	}
	dwc2_urb->length = len;
 	dwc2_urb->flags = flags;
	dwc2_urb->status = -EINPROGRESS;
	dwc2_urb->packet_count = xfer->nframes;

	if (xfertype == UE_INTERRUPT ||
	    xfertype == UE_ISOCHRONOUS) {
		uint16_t ival;

		if (xfertype == UE_INTERRUPT &&
		    dpipe->pipe.interval != USBD_DEFAULT_INTERVAL) {
			ival = dpipe->pipe.interval;
		} else {
			ival = ed->bInterval;
		}

		if (ival < 1) {
			retval = -ENODEV;
			goto fail;
		}
		if (dev->speed == USB_SPEED_HIGH ||
		   (dev->speed == USB_SPEED_FULL && xfertype == UE_ISOCHRONOUS)) {
			if (ival > 16) {
				/*
				 * illegal with HS/FS, but there were
				 * documentation bugs in the spec
				 */
				ival = 256;
			} else {
				ival = (1 << (ival - 1));
			}
		} else {
			if (xfertype == UE_INTERRUPT && ival < 10)
				ival = 10;
		}
		dwc2_urb->interval = ival;
	}

	/* XXXNH bring down from callers?? */
// 	mtx_enter(&sc->sc_lock);

	xfer->actlen = 0;

	KASSERT(xfertype != UE_ISOCHRONOUS ||
	    xfer->nframes < DWC2_MAXISOCPACKETS);
	KASSERTMSG(xfer->nframes == 0 || xfertype == UE_ISOCHRONOUS,
	    "nframes %d xfertype %d\n", xfer->nframes, xfertype);

	for (off = i = 0; i < xfer->nframes; ++i) {
		DPRINTFN(3, "xfer=%p frame=%d offset=%d length=%d\n", xfer, i,
		    off, xfer->frlengths[i]);

		dwc2_hcd_urb_set_iso_desc_params(dwc2_urb, i, off,
		    xfer->frlengths[i]);
		off += xfer->frlengths[i];
	}

	/* might need to check cpu_intr_p */
	mtx_enter(&hsotg->lock);

	if (xfer->timeout && !sc->sc_bus.use_polling) {
		timeout_reset(&xfer->timeout_handle, mstohz(xfer->timeout),
		    dwc2_timeout, xfer);
	}

	dwc2_urb->priv = xfer;
	retval = dwc2_hcd_urb_enqueue(hsotg, dwc2_urb, &dpipe->priv, 0);
	if (retval)
		goto fail;

	if (alloc_bandwidth) {
		dwc2_allocate_bus_bandwidth(hsotg,
				dwc2_hcd_get_ep_bandwidth(hsotg, dpipe),
				xfer);
	}

fail:
	mtx_leave(&hsotg->lock);

// 	mtx_leave(&sc->sc_lock);

	switch (retval) {
	case 0:
		break;
	case -ENODEV:
		err = USBD_INVAL;
		break;
	case -ENOMEM:
		err = USBD_NOMEM;
		break;
	default:
		err = USBD_IOERROR;
	}

	return err;

}

void
dwc2_worker(struct task *wk, void *priv)
{
	struct dwc2_softc *sc = priv;
	struct dwc2_hsotg *hsotg = sc->sc_hsotg;

Debugger();
#if 0
	struct usbd_xfer *xfer = dwork->xfer;
	struct dwc2_xfer *dxfer = DWC2_XFER2DXFER(xfer);

	dwc2_hcd_endpoint_disable(sc->dwc_dev.hcd, dpipe->priv, 250);
	dwc_free(NULL, dpipe->urb);
#endif

	mtx_enter(&sc->sc_lock);
	if (wk == &hsotg->wf_otg) {
		dwc2_conn_id_status_change(wk);
	} else if (wk == &hsotg->start_work.work) {
		dwc2_hcd_start_func(wk);
	} else if (wk == &hsotg->reset_work.work) {
		dwc2_hcd_reset_func(wk);
	} else {
#if 0
		KASSERT(dwork->xfer != NULL);
		KASSERT(dxfer->queued == true);

		if (!(dxfer->flags & DWC2_XFER_ABORTING)) {
			dwc2_start_standard_chain(xfer);
		}
		dxfer->queued = false;
		wakeup(&dxfer->flags);
#endif
	}
	mtx_leave(&sc->sc_lock);
}

int dwc2_intr(void *p)
{
	struct dwc2_softc *sc = p;
	struct dwc2_hsotg *hsotg;
	int ret = 0;

	if (sc == NULL)
		return 0;

	hsotg = sc->sc_hsotg;
	mtx_enter(&hsotg->lock);

	if (sc->sc_dying)
		goto done;

	if (sc->sc_bus.use_polling) {
		uint32_t intrs;

		intrs = dwc2_read_core_intr(hsotg);
		DWC2_WRITE_4(hsotg, GINTSTS, intrs);
	} else {
		ret = dwc2_interrupt(sc);
	}

done:
	mtx_leave(&hsotg->lock);

	return ret;
}

int
dwc2_interrupt(struct dwc2_softc *sc)
{
	int ret = 0;

	if (sc->sc_hcdenabled) {
		ret |= dwc2_handle_hcd_intr(sc->sc_hsotg);
	}

	ret |= dwc2_handle_common_intr(sc->sc_hsotg);

	return ret;
}

/***********************************************************************/

int
dwc2_detach(struct dwc2_softc *sc, int flags)
{
	int rv = 0;

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);

	return rv;
}

bool
dwc2_shutdown(struct device *self, int flags)
{
	struct dwc2_softc *sc = (void *)self;

	sc = sc;

	return true;
}

void
dwc2_childdet(struct device *self, struct device *child)
{
	struct dwc2_softc *sc = (void *)self;

	sc = sc;
}

int
dwc2_activate(struct device *self, int act)
{
	struct dwc2_softc *sc = (void *)self;

	sc = sc;

	return 0;
}

#if 0
bool
dwc2_resume(struct device *dv, const pmf_qual_t *qual)
{
	struct dwc2_softc *sc = (void *)dv;

	sc = sc;

	return true;
}

bool
dwc2_suspend(struct device *dv, const pmf_qual_t *qual)
{
	struct dwc2_softc *sc = (void *)dv;

	sc = sc;

	return true;
}
#endif

/***********************************************************************/
int
dwc2_init(struct dwc2_softc *sc)
{
	int err = 0;

	sc->sc_bus.usbrev = USBREV_2_0;
	sc->sc_bus.methods = &dwc2_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct dwc2_pipe);
	sc->sc_hcdenabled = false;

	mtx_init(&sc->sc_lock, IPL_SOFTUSB);

	TAILQ_INIT(&sc->sc_complete);

	sc->sc_rhc_si = softintr_establish(IPL_SOFTUSB, dwc2_rhc, sc);

#if 0
	usb_setup_reserve(&sc->sc_bus, &sc->sc_dma_reserve, sc->sc_bus.dmatag,
	    USB_MEM_RESERVE);
#endif

	pool_init(&sc->sc_xferpool, sizeof(struct dwc2_xfer), 0, 0, 0,
	    "dwc2xfer", NULL);
	pool_setipl(&sc->sc_xferpool, IPL_USB);
	pool_init(&sc->sc_qhpool, sizeof(struct dwc2_qh), 0, 0, 0,
	    "dwc2qh", NULL);
	pool_setipl(&sc->sc_qhpool, IPL_USB);
	pool_init(&sc->sc_qtdpool, sizeof(struct dwc2_qtd), 0, 0, 0,
	    "dwc2qtd", NULL);
	pool_setipl(&sc->sc_qtdpool, IPL_USB);

	sc->sc_hsotg = malloc(sizeof(struct dwc2_hsotg), M_DEVBUF,
	    M_ZERO | M_WAITOK);
	if (sc->sc_hsotg == NULL) {
		err = ENOMEM;
		goto fail1;
	}

	sc->sc_hsotg->hsotg_sc = sc;
	sc->sc_hsotg->dev = &sc->sc_bus.bdev;
	sc->sc_hcdenabled = true;

	err = dwc2_hcd_init(sc->sc_hsotg, sc->sc_params);
	if (err) {
		err = -err;
		goto fail2;
	}

	return 0;

fail2:
	free(sc->sc_hsotg, M_DEVBUF, sizeof(struct dwc2_hsotg));
fail1:
	softintr_disestablish(sc->sc_rhc_si);

	return err;
}

#if 0
/*
 * curmode is a mode indication bit 0 = device, 1 = host
 */
STATIC const char * const intnames[32] = {
	"curmode",	"modemis",	"otgint",	"sof",
	"rxflvl",	"nptxfemp",	"ginnakeff",	"goutnakeff",
	"ulpickint",	"i2cint",	"erlysusp",	"usbsusp",
	"usbrst",	"enumdone",	"isooutdrop",	"eopf",
	"restore_done",	"epmis",	"iepint",	"oepint",
	"incompisoin",	"incomplp",	"fetsusp",	"resetdet",
	"prtint",	"hchint",	"ptxfemp",	"lpm",
	"conidstschng",	"disconnint",	"sessreqint",	"wkupint"
};


/***********************************************************************/

#endif


void
dw_timeout(void *arg)
{
	struct delayed_work *dw = arg;

	task_set(&dw->work, dw->dw_fn, arg);
	task_add(dw->dw_wq, &dw->work);
}

void dwc2_host_hub_info(struct dwc2_hsotg *hsotg, void *context, int *hub_addr,
			int *hub_port)
{
	struct usbd_xfer *xfer = context;
	struct dwc2_pipe *dpipe = DWC2_XFER2DPIPE(xfer);
	struct usbd_device *dev = dpipe->pipe.device;

	if (dev->myhsport != NULL) {
		*hub_addr = dev->myhsport->parent->address;
 		*hub_port = dev->myhsport->portno;
	}
}

int dwc2_host_get_speed(struct dwc2_hsotg *hsotg, void *context)
{
	struct usbd_xfer *xfer = context;
	struct dwc2_pipe *dpipe = DWC2_XFER2DPIPE(xfer);
	struct usbd_device *dev = dpipe->pipe.device;

	return dev->speed;
}

/*
 * Sets the final status of an URB and returns it to the upper layer. Any
 * required cleanup of the URB is performed.
 *
 * Must be called with interrupt disabled and spinlock held
 */
void dwc2_host_complete(struct dwc2_hsotg *hsotg, struct dwc2_qtd *qtd,
			int status)
{
	struct usbd_xfer *xfer;
	struct dwc2_xfer *dxfer;
	struct dwc2_softc *sc;
	usb_endpoint_descriptor_t *ed;
	uint8_t xfertype;

	if (!qtd) {
		dev_dbg(hsotg->dev, "## %s: qtd is NULL ##\n", __func__);
		return;
	}

	if (!qtd->urb) {
		dev_dbg(hsotg->dev, "## %s: qtd->urb is NULL ##\n", __func__);
		return;
	}

	xfer = qtd->urb->priv;
	if (!xfer) {
		dev_dbg(hsotg->dev, "## %s: urb->priv is NULL ##\n", __func__);
		return;
	}

	dxfer = DWC2_XFER2DXFER(xfer);
	sc = DWC2_XFER2SC(xfer);
	ed = xfer->pipe->endpoint->edesc;
	xfertype = UE_GET_XFERTYPE(ed->bmAttributes);

	xfer->actlen = dwc2_hcd_urb_get_actual_length(qtd->urb);

	DPRINTFN(3, "xfer=%p actlen=%d\n", xfer, xfer->actlen);

	if (xfertype == UE_ISOCHRONOUS && dbg_perio()) {
		int i;

		for (i = 0; i < xfer->nframes; i++)
			dev_vdbg(hsotg->dev, " ISO Desc %d status %d\n",
				 i, qtd->urb->iso_descs[i].status);
	}

	if (xfertype == UE_ISOCHRONOUS) {
		int i;

		xfer->actlen = 0;
		for (i = 0; i < xfer->nframes; ++i) {
			xfer->frlengths[i] =
				dwc2_hcd_urb_get_iso_desc_actual_length(
						qtd->urb, i);
			xfer->actlen += xfer->frlengths[i];
		}
	}

	if (!status) {
		if (!(xfer->flags & USBD_SHORT_XFER_OK) &&
		    xfer->actlen < xfer->length)
			status = -EIO;
	}

	switch (status) {
	case 0:
		xfer->status = USBD_NORMAL_COMPLETION;
		break;
	case -EPIPE:
		xfer->status = USBD_STALLED;
		break;
	case -ETIMEDOUT:
		xfer->status = USBD_TIMEOUT;
		break;
	case -EPROTO:
		xfer->status = USBD_INVAL;
		break;
	case -EIO:
		xfer->status = USBD_IOERROR;
		break;
	case -EOVERFLOW:
		xfer->status = USBD_IOERROR;
		break;
	default:
		printf("%s: unknown error status %d\n", __func__, status);
	}

	if (xfertype == UE_ISOCHRONOUS ||
	    xfertype == UE_INTERRUPT) {
		struct dwc2_pipe *dpipe = DWC2_XFER2DPIPE(xfer);

		dwc2_free_bus_bandwidth(hsotg,
					dwc2_hcd_get_ep_bandwidth(hsotg, dpipe),
					xfer);
	}

	qtd->urb = NULL;
	timeout_del(&xfer->timeout_handle);

	KASSERT(mtx_owned(&hsotg->lock));

	TAILQ_INSERT_TAIL(&sc->sc_complete, dxfer, xnext);

	mtx_leave(&hsotg->lock);
	usb_schedsoftintr(&sc->sc_bus);
	mtx_enter(&hsotg->lock);
}


int
_dwc2_hcd_start(struct dwc2_hsotg *hsotg)
{
	dev_dbg(hsotg->dev, "DWC OTG HCD START\n");

	mtx_enter(&hsotg->lock);

	hsotg->op_state = OTG_STATE_A_HOST;

	dwc2_hcd_reinit(hsotg);

	/*XXXNH*/
	delay(50);

	mtx_leave(&hsotg->lock);
	return 0;
}

int dwc2_host_is_b_hnp_enabled(struct dwc2_hsotg *hsotg)
{

	return false;
}
