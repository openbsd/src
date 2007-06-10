/*	$OpenBSD: uscanner.c,v 1.29 2007/06/10 14:49:01 mbalmer Exp $ */
/*	$NetBSD: uscanner.c,v 1.40 2003/01/27 00:32:44 wiz Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology
 * and Nick Hibma (n_hibma@qubesoft.com).
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


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/conf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/usbdevs.h>

#ifdef USCANNER_DEBUG
#define DPRINTF(x)	do { if (uscannerdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uscannerdebug>(n)) printf x; } while (0)
int	uscannerdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct uscan_info {
	struct usb_devno devno;
	u_int flags;
#define USC_KEEP_OPEN 1
};

/* Table of scanners that may work with this driver. */
static const struct uscan_info uscanner_devs[] = {
  /* Acer Peripherals */
 {{ USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_320U }, 0 },
 {{ USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_640U }, 0 },
 {{ USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_620U }, 0 },
 {{ USB_VENDOR_ACERP, USB_PRODUCT_ACERP_ACERSCAN_C310U }, 0 },

  /* AGFA */
 {{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCAN1236U }, 0 },
 {{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCAN1212U }, 0 },
 {{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCAN1212U2 }, 0 },
 {{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANTOUCH }, 0 },
 {{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE40 }, 0 },
 {{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE50 }, 0 },
 {{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE20 }, 0 },
 {{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE25 }, 0 },
 {{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE26 }, 0 },
 {{ USB_VENDOR_AGFA, USB_PRODUCT_AGFA_SNAPSCANE52 }, 0 },

  /* Avision */
 {{ USB_VENDOR_AVISION, USB_PRODUCT_AVISION_1200U }, 0 },

  /* Canon */
 {{ USB_VENDOR_CANON, USB_PRODUCT_CANON_N656U }, 0 },
 {{ USB_VENDOR_CANON, USB_PRODUCT_CANON_N670U }, 0 },
 {{ USB_VENDOR_CANON, USB_PRODUCT_CANON_N1220U }, 0 },
 {{ USB_VENDOR_CANON, USB_PRODUCT_CANON_N1240U }, 0 },

  /* Kye */
 {{ USB_VENDOR_KYE, USB_PRODUCT_KYE_VIVIDPRO }, 0 },

  /* HP */
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_2200C }, 0 },
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_3300C }, 0 },
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_3400CSE }, 0 },
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_4100C }, 0 },
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_4200C }, 0 },
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_4300C }, 0 },
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_S20 }, 0 },
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_5200C }, 0 },
#if 0
  /* Handled by usscanner */
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_5300C }, 0 },
#endif
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_6200C }, 0 },
 {{ USB_VENDOR_HP, USB_PRODUCT_HP_6300C }, 0 },

#if 0
  /* XXX Should be handled by usscanner */
  /* Microtek */
 {{ USB_VENDOR_SCANLOGIC, USB_PRODUCT_SCANLOGIC_336CX }, 0 },
 {{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_X6U }, 0 },
 {{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_336CX }, 0 },
 {{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_336CX2 }, 0 },
 {{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_C6 }, 0 },
 {{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_V6USL }, 0 },
 {{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_V6USL2 }, 0 },
 {{ USB_VENDOR_MICROTEK, USB_PRODUCT_MICROTEK_V6UL }, 0 },
#endif

  /* Mustek */
 {{ USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200CU }, 0 },
 {{ USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_BEARPAW1200F }, 0 },
 {{ USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_600USB }, 0 },
 {{ USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_600CU }, 0 },
 {{ USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200USB }, 0 },
 {{ USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200UB }, 0 },
 {{ USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200USBPLUS }, 0 },
 {{ USB_VENDOR_MUSTEK, USB_PRODUCT_MUSTEK_1200CUPLUS }, 0 },

  /* National */
 {{ USB_VENDOR_NATIONAL, USB_PRODUCT_NATIONAL_BEARPAW1200 }, 0 },
 {{ USB_VENDOR_NATIONAL, USB_PRODUCT_NATIONAL_BEARPAW2400 }, 0 },

  /* Primax */
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2X300 }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2E300 }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2300 }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2E3002 }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_9600 }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_600U }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_6200 }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_19200 }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_1200U }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G600 }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_636I }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2600 }, 0 },
 {{ USB_VENDOR_PRIMAX, USB_PRODUCT_PRIMAX_G2E600 }, 0 },

  /* Epson */
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_636 }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_610 }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1200 }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1240 }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1260 }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1600 }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1640 }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1660 }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_640U }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_1650 }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_2400 }, 0 },
 {{ USB_VENDOR_EPSON, USB_PRODUCT_EPSON_GT9700F }, USC_KEEP_OPEN },

  /* UMAX */
 {{ USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA1220U }, 0 },
 {{ USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA1236U }, 0 },
 {{ USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA2000U }, 0 },
 {{ USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA2200U }, 0 },
 {{ USB_VENDOR_UMAX, USB_PRODUCT_UMAX_ASTRA3400 }, 0 },

  /* Visioneer */
 {{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_3000 }, 0 },
 {{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_5300 }, 0 },
 {{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_7600 }, 0 },
 {{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_6100 }, 0 },
 {{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_6200 }, 0 },
 {{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_8100 }, 0 },
 {{ USB_VENDOR_VISIONEER, USB_PRODUCT_VISIONEER_8600 }, 0 },

  /* Ultima */
 {{ USB_VENDOR_ULTIMA, USB_PRODUCT_ULTIMA_1200UBPLUS }, 0 },

};
#define uscanner_lookup(v, p) ((const struct uscan_info *)usb_lookup(uscanner_devs, v, p))

#define	USCANNER_BUFFERSIZE	1024

struct uscanner_softc {
	struct device		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;

	u_int			sc_dev_flags;

	usbd_pipe_handle	sc_bulkin_pipe;
	int			sc_bulkin;
	usbd_xfer_handle	sc_bulkin_xfer;
	void 			*sc_bulkin_buffer;
	int			sc_bulkin_bufferlen;
	int			sc_bulkin_datalen;

	usbd_pipe_handle	sc_bulkout_pipe;
	int			sc_bulkout;
	usbd_xfer_handle	sc_bulkout_xfer;
	void 			*sc_bulkout_buffer;
	int			sc_bulkout_bufferlen;
	int			sc_bulkout_datalen;

	struct selinfo		sc_selq;

	u_char			sc_state;
#define USCANNER_OPEN		0x01	/* opened */

	int			sc_refcnt;
	u_char			sc_dying;
};

int uscanner_do_read(struct uscanner_softc *, struct uio *, int);
int uscanner_do_write(struct uscanner_softc *, struct uio *, int);
void uscanner_do_close(struct uscanner_softc *);

#define USCANNERUNIT(n) (minor(n))

USB_DECLARE_DRIVER(uscanner);

int
uscanner_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	return (uscanner_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

void
uscanner_attach(struct device *parent, struct device *self, void *aux)
{
	struct uscanner_softc *sc = (struct uscanner_softc *)self;
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id = 0;
	usb_endpoint_descriptor_t *ed, *ed_bulkin = NULL, *ed_bulkout = NULL;
	char *devinfop;
	int i;
	usbd_status err;

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	printf("\n%s: %s\n", sc->sc_dev.dv_xname, devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_dev_flags = uscanner_lookup(uaa->vendor, uaa->product)->flags;

	sc->sc_udev = uaa->device;

	err = usbd_set_config_no(uaa->device, 1, 1); /* XXX */
	if (err) {
		printf("%s: setting config no failed\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/* XXX We only check the first interface */
	err = usbd_device2interface_handle(sc->sc_udev, 0, &sc->sc_iface);
	if (!err && sc->sc_iface)
	    id = usbd_get_interface_descriptor(sc->sc_iface);
	if (err || id == 0) {
		printf("%s: could not get interface descriptor, err=%d,id=%p\n",
		       sc->sc_dev.dv_xname, err, id);
		return;
	}

	/* Find the two first bulk endpoints */
	for (i = 0 ; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == 0) {
			printf("%s: could not read endpoint descriptor\n",
			       sc->sc_dev.dv_xname);
			return;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			ed_bulkin = ed;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
		        ed_bulkout = ed;
		}

		if (ed_bulkin && ed_bulkout)	/* found all we need */
			break;
	}

	/* Verify that we goething sensible */
	if (ed_bulkin == NULL || ed_bulkout == NULL) {
		printf("%s: bulk-in and/or bulk-out endpoint not found\n",
			sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_bulkin = ed_bulkin->bEndpointAddress;
	sc->sc_bulkout = ed_bulkout->bEndpointAddress;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   &sc->sc_dev);
}

int
uscanneropen(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
	struct uscanner_softc *sc;
	int unit = USCANNERUNIT(dev);
	usbd_status err;

	USB_GET_SC_OPEN(uscanner, unit, sc);

 	DPRINTFN(5, ("uscanneropen: flag=%d, mode=%d, unit=%d\n",
		     flag, mode, unit));

	if (sc->sc_dying)
		return (ENXIO);

	if (sc->sc_state & USCANNER_OPEN)
		return (EBUSY);

	sc->sc_state |= USCANNER_OPEN;

	sc->sc_bulkin_buffer = malloc(USCANNER_BUFFERSIZE, M_USBDEV, M_WAITOK);
	sc->sc_bulkout_buffer = malloc(USCANNER_BUFFERSIZE, M_USBDEV, M_WAITOK);
	/* No need to check buffers for NULL since we have WAITOK */

	sc->sc_bulkin_bufferlen = USCANNER_BUFFERSIZE;
	sc->sc_bulkout_bufferlen = USCANNER_BUFFERSIZE;

	/* We have decided on which endpoints to use, now open the pipes */
	if (sc->sc_bulkin_pipe == NULL) {
		err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkin,
				     USBD_EXCLUSIVE_USE, &sc->sc_bulkin_pipe);
		if (err) {
			printf("%s: cannot open bulk-in pipe (addr %d)\n",
			       sc->sc_dev.dv_xname, sc->sc_bulkin);
			uscanner_do_close(sc);
			return (EIO);
		}
	}
	if (sc->sc_bulkout_pipe == NULL) {
		err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkout,
				     USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
		if (err) {
			printf("%s: cannot open bulk-out pipe (addr %d)\n",
			       sc->sc_dev.dv_xname, sc->sc_bulkout);
			uscanner_do_close(sc);
			return (EIO);
		}
	}

	sc->sc_bulkin_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_bulkin_xfer == NULL) {
		uscanner_do_close(sc);
		return (ENOMEM);
	}
	sc->sc_bulkout_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_bulkout_xfer == NULL) {
		uscanner_do_close(sc);
		return (ENOMEM);
	}

	return (0);	/* success */
}

int
uscannerclose(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
	struct uscanner_softc *sc;

	USB_GET_SC(uscanner, USCANNERUNIT(dev), sc);

	DPRINTFN(5, ("uscannerclose: flag=%d, mode=%d, unit=%d\n",
		     flag, mode, USCANNERUNIT(dev)));

#ifdef DIAGNOSTIC
	if (!(sc->sc_state & USCANNER_OPEN)) {
		printf("uscannerclose: not open\n");
		return (EINVAL);
	}
#endif

	uscanner_do_close(sc);

	return (0);
}

void
uscanner_do_close(struct uscanner_softc *sc)
{
	if (sc->sc_bulkin_xfer) {
		usbd_free_xfer(sc->sc_bulkin_xfer);
		sc->sc_bulkin_xfer = NULL;
	}
	if (sc->sc_bulkout_xfer) {
		usbd_free_xfer(sc->sc_bulkout_xfer);
		sc->sc_bulkout_xfer = NULL;
	}

	if (!(sc->sc_dev_flags & USC_KEEP_OPEN)) {
		if (sc->sc_bulkin_pipe != NULL) {
			usbd_abort_pipe(sc->sc_bulkin_pipe);
			usbd_close_pipe(sc->sc_bulkin_pipe);
			sc->sc_bulkin_pipe = NULL;
		}
		if (sc->sc_bulkout_pipe != NULL) {
			usbd_abort_pipe(sc->sc_bulkout_pipe);
			usbd_close_pipe(sc->sc_bulkout_pipe);
			sc->sc_bulkout_pipe = NULL;
		}
	}

	if (sc->sc_bulkin_buffer) {
		free(sc->sc_bulkin_buffer, M_USBDEV);
		sc->sc_bulkin_buffer = NULL;
	}
	if (sc->sc_bulkout_buffer) {
		free(sc->sc_bulkout_buffer, M_USBDEV);
		sc->sc_bulkout_buffer = NULL;
	}

	sc->sc_state &= ~USCANNER_OPEN;
}

int
uscanner_do_read(struct uscanner_softc *sc, struct uio *uio, int flag)
{
	u_int32_t n, tn;
	usbd_status err;
	int error = 0;

	DPRINTFN(5, ("%s: uscannerread\n", sc->sc_dev.dv_xname));

	if (sc->sc_dying)
		return (EIO);

	while ((n = min(sc->sc_bulkin_bufferlen, uio->uio_resid)) != 0) {
		DPRINTFN(1, ("uscannerread: start transfer %d bytes\n",n));
		tn = n;

		err = usbd_bulk_transfer(
			sc->sc_bulkin_xfer, sc->sc_bulkin_pipe,
			USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT,
			sc->sc_bulkin_buffer, &tn,
			"uscnrb");
		if (err) {
			if (err == USBD_INTERRUPTED)
				error = EINTR;
			else if (err == USBD_TIMEOUT)
				error = ETIMEDOUT;
			else
				error = EIO;
			break;
		}
		DPRINTFN(1, ("uscannerread: got %d bytes\n", tn));
		error = uiomove(sc->sc_bulkin_buffer, tn, uio);
		if (error || tn < n)
			break;
	}

	return (error);
}

int
uscannerread(dev_t dev, struct uio *uio, int flag)
{
	struct uscanner_softc *sc;
	int error;

	USB_GET_SC(uscanner, USCANNERUNIT(dev), sc);

	sc->sc_refcnt++;
	error = uscanner_do_read(sc, uio, flag);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_dev);

	return (error);
}

int
uscanner_do_write(struct uscanner_softc *sc, struct uio *uio, int flag)
{
	u_int32_t n;
	int error = 0;
	usbd_status err;

	DPRINTFN(5, ("%s: uscanner_do_write\n", sc->sc_dev.dv_xname));

	if (sc->sc_dying)
		return (EIO);

	while ((n = min(sc->sc_bulkout_bufferlen, uio->uio_resid)) != 0) {
		error = uiomove(sc->sc_bulkout_buffer, n, uio);
		if (error)
			break;
		DPRINTFN(1, ("uscanner_do_write: transfer %d bytes\n", n));
		err = usbd_bulk_transfer(
			sc->sc_bulkout_xfer, sc->sc_bulkout_pipe,
			0, USBD_NO_TIMEOUT,
			sc->sc_bulkout_buffer, &n,
			"uscnwb");
		if (err) {
			if (err == USBD_INTERRUPTED)
				error = EINTR;
			else
				error = EIO;
			break;
		}
	}

	return (error);
}

int
uscannerwrite(dev_t dev, struct uio *uio, int flag)
{
	struct uscanner_softc *sc;
	int error;

	USB_GET_SC(uscanner, USCANNERUNIT(dev), sc);

	sc->sc_refcnt++;
	error = uscanner_do_write(sc, uio, flag);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_dev);
	return (error);
}

int
uscanner_activate(device_ptr_t self, enum devact act)
{
	struct uscanner_softc *sc = (struct uscanner_softc *)self;

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
uscanner_detach(struct device *self, int flags)
{
	struct uscanner_softc *sc = (struct uscanner_softc *)self;
	int s;
	int maj, mn;

	DPRINTF(("uscanner_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	sc->sc_dev_flags = 0;	/* make close really close device */

	/* Abort all pipes.  Causes processes waiting for transfer to wake. */
	if (sc->sc_bulkin_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulkin_pipe);
	if (sc->sc_bulkout_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulkout_pipe);

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(&sc->sc_dev);
	}
	splx(s);

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == uscanneropen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit * USB_MAX_ENDPOINTS;
	vdevgone(maj, mn, mn + USB_MAX_ENDPOINTS - 1, VCHR);
	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   &sc->sc_dev);

	return (0);
}

int
uscannerpoll(dev_t dev, int events, usb_proc_ptr p)
{
	struct uscanner_softc *sc;
	int revents = 0;

	USB_GET_SC(uscanner, USCANNERUNIT(dev), sc);

	if (sc->sc_dying)
		return (POLLERR);

	/*
	 * We have no easy way of determining if a read will
	 * yield any data or a write will happen.
	 * Pretend they will.
	 */
	revents |= events &
		   (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM);

	return (revents);
}

int
uscannerioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, usb_proc_ptr p)
{
	return (EINVAL);
}

void filt_uscannerdetach(struct knote *);
int uscannerkqfilter(dev_t, struct knote *);

void
filt_uscannerdetach(struct knote *kn)
{
	struct uscanner_softc *sc = (void *)kn->kn_hook;

	SLIST_REMOVE(&sc->sc_selq.sel_klist, kn, knote, kn_selnext);
}

struct filterops uscanner_seltrue_filtops =
	{ 1, NULL, filt_uscannerdetach, filt_seltrue };

int
uscannerkqfilter(dev_t dev, struct knote *kn)
{
	struct uscanner_softc *sc;
	struct klist *klist;

	USB_GET_SC(uscanner, USCANNERUNIT(dev), sc);

	if (sc->sc_dying)
		return (1);

	switch (kn->kn_filter) {
	case EVFILT_READ:
	case EVFILT_WRITE:
		/* 
		 * We have no easy way of determining if a read will
		 * yield any data or a write will happen.
		 * Pretend they will.
		 */
		klist = &sc->sc_selq.sel_klist;
		kn->kn_fop = &uscanner_seltrue_filtops;
		break;

	default:
		return (1);
	}

	kn->kn_hook = (void *)sc;

	SLIST_INSERT_HEAD(klist, kn, kn_selnext);

	return (0);
}
