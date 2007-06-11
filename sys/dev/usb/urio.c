/*	$OpenBSD: urio.c,v 1.29 2007/06/11 12:36:52 mbalmer Exp $	*/
/*	$NetBSD: urio.c,v 1.15 2002/10/23 09:14:02 jdolecek Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
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
 * The inspiration and information for this driver comes from the
 * FreeBSD driver written by Iwasa Kazmi.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/urio.h>

#ifdef URIO_DEBUG
#define DPRINTF(x)	do { if (uriodebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uriodebug>(n)) printf x; } while (0)
int	uriodebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define URIO_CONFIG_NO		1
#define URIO_IFACE_IDX		0

#define	URIO_BSIZE	4096

struct urio_softc {
 	struct device		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;

	int			sc_in_addr;
	usbd_pipe_handle	sc_in_pipe;
	int			sc_out_addr;
	usbd_pipe_handle	sc_out_pipe;

	int			sc_refcnt;
	char			sc_dying;
};

#define URIOUNIT(n) (minor(n))

#define URIO_RW_TIMEOUT 4000	/* ms */

static const struct usb_devno urio_devs[] = {
	{ USB_VENDOR_DIAMOND, USB_PRODUCT_DIAMOND_RIO500USB},
	{ USB_VENDOR_DIAMOND2, USB_PRODUCT_DIAMOND2_RIO600USB},
	{ USB_VENDOR_DIAMOND2, USB_PRODUCT_DIAMOND2_RIO800USB},
	{ USB_VENDOR_DIAMOND2, USB_PRODUCT_DIAMOND2_PSAPLAY120},
};
#define urio_lookup(v, p) usb_lookup(urio_devs, v, p)

USB_DECLARE_DRIVER(urio);

int
urio_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg	*uaa = aux;

	DPRINTFN(50,("urio_match\n"));

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	return (urio_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void
urio_attach(struct device *parent, struct device *self, void *aux)
{
	struct urio_softc	*sc = (struct urio_softc *)self;
	struct usb_attach_arg	*uaa = aux;
	usbd_device_handle	dev = uaa->device;
	usbd_interface_handle	iface;
	char			*devinfop;
	usbd_status		err;
	usb_endpoint_descriptor_t *ed;
	u_int8_t		epcount;
	int			i;

	DPRINTFN(10,("urio_attach: sc=%p\n", sc));

	devinfop = usbd_devinfo_alloc(dev, 0);
	printf("\n%s: %s\n", sc->sc_dev.dv_xname, devinfop);
	usbd_devinfo_free(devinfop);

	err = usbd_set_config_no(dev, URIO_CONFIG_NO, 1);
	if (err) {
		printf("%s: setting config no failed\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	err = usbd_device2interface_handle(dev, URIO_IFACE_IDX, &iface);
	if (err) {
		printf("%s: getting interface handle failed\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_udev = dev;
	sc->sc_iface = iface;

	epcount = 0;
	(void)usbd_endpoint_count(iface, &epcount);

	sc->sc_in_addr = -1;
	sc->sc_out_addr = -1;
	for (i = 0; i < epcount; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: couldn't get ep %d\n",
			    sc->sc_dev.dv_xname, i);
			return;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_in_addr = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_out_addr = ed->bEndpointAddress;
		}
	}
	if (sc->sc_in_addr == -1 || sc->sc_out_addr == -1) {
		printf("%s: missing endpoint\n", sc->sc_dev.dv_xname);
		return;
	}

	DPRINTFN(10, ("urio_attach: %p\n", sc->sc_udev));

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   &sc->sc_dev);
}

int
urio_detach(struct device *self, int flags)
{
	struct urio_softc *sc = (struct urio_softc *)self;
	int s;
	int maj, mn;

	DPRINTF(("urio_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	/* Abort all pipes.  Causes processes waiting for transfer to wake. */
	if (sc->sc_in_pipe != NULL) {
		usbd_abort_pipe(sc->sc_in_pipe);
		usbd_close_pipe(sc->sc_in_pipe);
		sc->sc_in_pipe = NULL;
	}
	if (sc->sc_out_pipe != NULL) {
		usbd_abort_pipe(sc->sc_out_pipe);
		usbd_close_pipe(sc->sc_out_pipe);
		sc->sc_out_pipe = NULL;
	}

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(&sc->sc_dev);
	}
	splx(s);

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == urioopen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   &sc->sc_dev);

	return (0);
}

int
urio_activate(device_ptr_t self, enum devact act)
{
	struct urio_softc *sc = (struct urio_softc *)self;

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
urioopen(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
	struct urio_softc *sc;
	usbd_status err;

	if (URIOUNIT(dev) >= urio_cd.cd_ndevs)
		return (ENXIO);
	sc = urio_cd.cd_devs[URIOUNIT(dev)];
	if (sc == NULL)
		return (ENXIO);

	DPRINTFN(5, ("urioopen: flag=%d, mode=%d, unit=%d\n",
		     flag, mode, URIOUNIT(dev)));

	if (sc->sc_dying)
		return (EIO);

	if (sc->sc_in_pipe != NULL)
		return (EBUSY);

	if ((flag & (FWRITE|FREAD)) != (FWRITE|FREAD))
		return (EACCES);

	err = usbd_open_pipe(sc->sc_iface, sc->sc_in_addr, 0, &sc->sc_in_pipe);
	if (err)
		return (EIO);
	err = usbd_open_pipe(sc->sc_iface, sc->sc_out_addr,0,&sc->sc_out_pipe);
	if (err) {
		usbd_close_pipe(sc->sc_in_pipe);
		sc->sc_in_pipe = NULL;
		return (EIO);
	}

	return (0);
}

int
urioclose(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
	struct urio_softc *sc;
	sc = urio_cd.cd_devs[URIOUNIT(dev)];

	DPRINTFN(5, ("urioclose: flag=%d, mode=%d, unit=%d\n",
		     flag, mode, URIOUNIT(dev)));

	if (sc->sc_in_pipe != NULL) {
		usbd_abort_pipe(sc->sc_in_pipe);
		usbd_close_pipe(sc->sc_in_pipe);
		sc->sc_in_pipe = NULL;
	}
	if (sc->sc_out_pipe != NULL) {
		usbd_abort_pipe(sc->sc_out_pipe);
		usbd_close_pipe(sc->sc_out_pipe);
		sc->sc_out_pipe = NULL;
	}

	return (0);
}

int
urioread(dev_t dev, struct uio *uio, int flag)
{
	struct urio_softc *sc;
	usbd_xfer_handle xfer;
	usbd_status err;
	void *bufp;
	u_int32_t n, tn;
	int error = 0;

	sc = urio_cd.cd_devs[URIOUNIT(dev)];

	DPRINTFN(5, ("urioread: %d\n", URIOUNIT(dev)));

	if (sc->sc_dying)
		return (EIO);

	xfer = usbd_alloc_xfer(sc->sc_udev);
	if (xfer == NULL)
		return (ENOMEM);
	bufp = usbd_alloc_buffer(xfer, URIO_BSIZE);
	if (bufp == NULL) {
		usbd_free_xfer(xfer);
		return (ENOMEM);
	}

	sc->sc_refcnt++;

	while ((n = min(URIO_BSIZE, uio->uio_resid)) != 0) {
		DPRINTFN(1, ("urioread: start transfer %d bytes\n", n));
		tn = n;
		err = usbd_bulk_transfer(xfer, sc->sc_in_pipe, USBD_NO_COPY,
			  URIO_RW_TIMEOUT, bufp, &tn, "uriors");
		if (err) {
			if (err == USBD_INTERRUPTED)
				error = EINTR;
			else if (err == USBD_TIMEOUT)
				error = ETIMEDOUT;
			else
				error = EIO;
			break;
		}

		DPRINTFN(1, ("urioread: got %d bytes\n", tn));

		error = uiomove(bufp, tn, uio);
		if (error || tn < n)
			break;
	}
	usbd_free_xfer(xfer);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_dev);

	return (error);
}

int
uriowrite(dev_t dev, struct uio *uio, int flag)
{
	struct urio_softc *sc;
	usbd_xfer_handle xfer;
	usbd_status err;
	void *bufp;
	u_int32_t n;
	int error = 0;

	sc = urio_cd.cd_devs[URIOUNIT(dev)];

	DPRINTFN(5, ("uriowrite: unit=%d, len=%ld\n", URIOUNIT(dev),
		     (long)uio->uio_resid));

	if (sc->sc_dying)
		return (EIO);

	xfer = usbd_alloc_xfer(sc->sc_udev);
	if (xfer == NULL)
		return (ENOMEM);
	bufp = usbd_alloc_buffer(xfer, URIO_BSIZE);
	if (bufp == NULL) {
		usbd_free_xfer(xfer);
		return (ENOMEM);
	}

	sc->sc_refcnt++;

	while ((n = min(URIO_BSIZE, uio->uio_resid)) != 0) {
		error = uiomove(bufp, n, uio);
		if (error)
			break;

		DPRINTFN(1, ("uriowrite: transfer %d bytes\n", n));

		err = usbd_bulk_transfer(xfer, sc->sc_out_pipe, USBD_NO_COPY,
			  URIO_RW_TIMEOUT, bufp, &n, "uriowr");
		DPRINTFN(2, ("uriowrite: err=%d\n", err));
		if (err) {
			if (err == USBD_INTERRUPTED)
				error = EINTR;
			else if (err == USBD_TIMEOUT)
				error = ETIMEDOUT;
			else
				error = EIO;
			break;
		}
	}

	usbd_free_xfer(xfer);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_dev);

	DPRINTFN(5, ("uriowrite: done unit=%d, error=%d\n", URIOUNIT(dev),
		     error));

	return (error);
}


int
urioioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, usb_proc_ptr p)
{
	struct urio_softc * sc;
	int unit = URIOUNIT(dev);
	struct urio_command *rcmd;
	int requesttype, len;
	struct iovec iov;
	struct uio uio;
	usb_device_request_t req;
	usbd_status err;
	int req_flags = 0;
	u_int32_t req_actlen = 0;
	void *ptr = NULL;
	int error = 0;

	sc = urio_cd.cd_devs[unit];

	if (sc->sc_dying)
		return (EIO);

	rcmd = (struct urio_command *)addr;

	switch (cmd) {
	case URIO_RECV_COMMAND:
		requesttype = rcmd->requesttype | UT_READ_VENDOR_DEVICE;
		break;

	case URIO_SEND_COMMAND:
		requesttype = rcmd->requesttype | UT_WRITE_VENDOR_DEVICE;
		break;

	default:
		return (EINVAL);
		break;
	}

	if (!(flag & FWRITE))
		return (EPERM);
	len = rcmd->length;

	DPRINTFN(1,("urio_ioctl: cmd=0x%08lx reqtype=0x%0x req=0x%0x "
		    "value=0x%0x index=0x%0x len=0x%0x\n",
		    cmd, requesttype, rcmd->request, rcmd->value,
		    rcmd->index, len));

	/* Send rio control message */
	req.bmRequestType = requesttype;
	req.bRequest = rcmd->request;
	USETW(req.wValue, rcmd->value);
	USETW(req.wIndex, rcmd->index);
	USETW(req.wLength, len);

	if (len < 0 || len > 32767)
		return (EINVAL);
	if (len != 0) {
		iov.iov_base = (caddr_t)rcmd->buffer;
		iov.iov_len = len;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_resid = len;
		uio.uio_offset = 0;
		uio.uio_segflg = UIO_USERSPACE;
		uio.uio_rw = req.bmRequestType & UT_READ ?
			     UIO_READ : UIO_WRITE;
		uio.uio_procp = p;
		ptr = malloc(len, M_TEMP, M_WAITOK);
		if (uio.uio_rw == UIO_WRITE) {
			error = uiomove(ptr, len, &uio);
			if (error)
				goto ret;
		}
	}

	sc->sc_refcnt++;

	err = usbd_do_request_flags(sc->sc_udev, &req, ptr, req_flags,
		  &req_actlen, USBD_DEFAULT_TIMEOUT);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_dev);

	if (err) {
		error = EIO;
	} else {
		if (len != 0 && uio.uio_rw == UIO_READ)
			error = uiomove(ptr, len, &uio);
	}

ret:
	if (ptr != NULL)
		free(ptr, M_TEMP);
	return (error);
}

int
uriopoll(dev_t dev, int events, usb_proc_ptr p)
{
	return (0);
}
