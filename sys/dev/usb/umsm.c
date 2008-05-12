/*	$OpenBSD: umsm.c,v 1.25 2008/05/12 12:24:43 jsg Exp $	*/

/*
 * Copyright (c) 2006 Jonathan Gray <jsg@openbsd.org>
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

/* Driver for Qualcomm MSM EVDO and UMTS communication devices */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/ucomvar.h>

#ifdef USB_DEBUG
#define UMSM_DEBUG
#endif

#ifdef UMSM_DEBUG
int     umsmdebug = 1;
#define DPRINTFN(n, x)  do { if (umsmdebug > (n)) printf x; } while (0)
#else
#define DPRINTFN(n, x)
#endif

#define DPRINTF(x) DPRINTFN(0, x)


#define UMSMBUFSZ	4096
#define	UMSM_INTR_INTERVAL	100	/* ms */
#define E220_MODE_CHANGE_REQUEST 0x2

int umsm_match(struct device *, void *, void *); 
void umsm_attach(struct device *, struct device *, void *); 
int umsm_detach(struct device *, int); 
int umsm_activate(struct device *, enum devact); 

int umsm_open(void *, int);
void umsm_close(void *, int);
void umsm_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
void umsm_get_status(void *, int, u_char *, u_char *);

usbd_status umsm_e220_changemode(usbd_device_handle);

struct umsm_softc {
	struct device		 sc_dev;
	usbd_device_handle	 sc_udev;
	usbd_interface_handle	 sc_iface;
	struct device		*sc_subdev;
	u_char			 sc_dying;

	/* interrupt ep */
	int			 sc_intr_number;
	usbd_pipe_handle	 sc_intr_pipe;
	u_char			*sc_intr_buf;
	int			 sc_isize;

	u_char			 sc_lsr;	/* Local status register */
	u_char			 sc_msr;	/* status register */
};

struct ucom_methods umsm_methods = {
	umsm_get_status,
	NULL,
	NULL,
	NULL,
	umsm_open,
	umsm_close,
	NULL,
	NULL,
};

static const struct usb_devno umsm_devs[] = {
	{ USB_VENDOR_AIRPRIME,	USB_PRODUCT_AIRPRIME_PC5220 },
	{ USB_VENDOR_ANYDATA,	USB_PRODUCT_ANYDATA_A2502 },
	{ USB_VENDOR_ANYDATA,	USB_PRODUCT_ANYDATA_ADU_500A },
	{ USB_VENDOR_DELL,	USB_PRODUCT_DELL_W5500 },
	{ USB_VENDOR_HUAWEI,	USB_PRODUCT_HUAWEI_E220 },
	{ USB_VENDOR_KYOCERA2,	USB_PRODUCT_KYOCERA2_KPC650 },
	{ USB_VENDOR_NOVATEL1,	USB_PRODUCT_NOVATEL1_FLEXPACKGPS },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_EXPRESSCARD },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_MERLINV620 },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_S720 },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_U720 },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_XU870 },
	{ USB_VENDOR_NOVATEL,	USB_PRODUCT_NOVATEL_ES620 },
	{ USB_VENDOR_QUALCOMM,	USB_PRODUCT_QUALCOMM_MSM_DRIVER },
	{ USB_VENDOR_QUALCOMM,	USB_PRODUCT_QUALCOMM_MSM_HSDPA },
	{ USB_VENDOR_QUALCOMM,	USB_PRODUCT_QUALCOMM_MSM_HSDPA2 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_EM5625 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_AIRCARD_580 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_AIRCARD_595 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_AIRCARD_875 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC5720 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC5725 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC8755 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC8755_2 },
	{ USB_VENDOR_SIERRA,    USB_PRODUCT_SIERRA_MC8755_3 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC8765 },
	{ USB_VENDOR_SIERRA,	USB_PRODUCT_SIERRA_MC8775 },
};


struct cfdriver umsm_cd = { 
	NULL, "umsm", DV_DULL 
}; 

const struct cfattach umsm_ca = { 
	sizeof(struct umsm_softc), 
	umsm_match, 
	umsm_attach, 
	umsm_detach, 
	umsm_activate, 
};

int
umsm_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;

	if (uaa->iface == NULL)
		return UMATCH_NONE;

	/*
	 * Some devices(eg Huawei E220) have multiple interfaces and some
	 * of them are of class umass. Don't claim ownership in such case.
	 */
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL || id->bInterfaceClass == UICLASS_MASS) {
		/*
		 * E220 is a dual mode device, so we have to deal with
		 * it differently.
		 */
		if (uaa->vendor  == USB_VENDOR_HUAWEI &&
                    uaa->product == USB_PRODUCT_HUAWEI_E220) {
			if  (uaa->ifaceno != 2) 
				return UMATCH_VENDOR_IFACESUBCLASS;
			else
				return UMATCH_NONE;
		} else
			return UMATCH_NONE;
	}

	return (usb_lookup(umsm_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_IFACESUBCLASS : UMATCH_NONE;
}

void
umsm_attach(struct device *parent, struct device *self, void *aux)
{
	struct umsm_softc *sc = (struct umsm_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct ucom_attach_args uca;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int i;

	bzero(&uca, sizeof(uca));
	sc->sc_udev = uaa->device;
	sc->sc_iface = uaa->iface;

	id = usbd_get_interface_descriptor(sc->sc_iface);

	if (id == NULL || id->bInterfaceClass == UICLASS_MASS) {
		/* Huawei E220 require special command to change its mode to modem */
		if (uaa->vendor  == USB_VENDOR_HUAWEI &&
                    uaa->product == USB_PRODUCT_HUAWEI_E220 && uaa->ifaceno == 0) {
                        umsm_e220_changemode(uaa->device);
			/*
			 * the device will reset its own bus from device side, therefore
			 * we only return from this device probe process. 
			 */
			printf("%s: umass only mode. need to reattach\n", 
				sc->sc_dev.dv_xname);
			return;
		}
	}

	uca.bulkin = uca.bulkout = -1;
	sc->sc_intr_number = sc->sc_isize = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor found for %d\n",
			    sc->sc_dev.dv_xname, i);
			sc->sc_dying = 1;
			return;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_intr_number = ed->bEndpointAddress;
			sc->sc_isize = UGETW(ed->wMaxPacketSize);
			DPRINTF(("%s: find interrupt endpoint for %s\n", 
				__func__, sc->sc_dev.dv_xname));
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			uca.bulkin = ed->bEndpointAddress;
		else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			uca.bulkout = ed->bEndpointAddress;
	}
	if (uca.bulkin == -1 || uca.bulkout == -1) {
		printf("%s: missing endpoint\n", sc->sc_dev.dv_xname);
		sc->sc_dying = 1;
		return;
	}

	/* We need to force size as some devices lie */
	uca.ibufsize = UMSMBUFSZ;
	uca.obufsize = UMSMBUFSZ;
	uca.ibufsizepad = UMSMBUFSZ;
	uca.opkthdrlen = 0;
	uca.device = sc->sc_udev;
	uca.iface = sc->sc_iface;
	uca.methods = &umsm_methods;
	uca.arg = sc;
	uca.info = NULL;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    &sc->sc_dev);
	
	sc->sc_subdev = config_found_sm(self, &uca, ucomprint, ucomsubmatch);
}

int
umsm_detach(struct device *self, int flags)
{
	struct umsm_softc *sc = (struct umsm_softc *)self;
	int rv = 0;

	sc->sc_dying = 1;
	if (sc->sc_subdev != NULL) {
		rv = config_detach(sc->sc_subdev, flags);
		sc->sc_subdev = NULL;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   &sc->sc_dev);

	return (rv);
}

int
umsm_activate(struct device *self, enum devact act)
{
	struct umsm_softc *sc = (struct umsm_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_subdev != NULL)
			rv = config_deactivate(sc->sc_subdev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

int
umsm_open(void *addr, int portno)
{
	struct umsm_softc *sc = addr;
	int err;

	if (sc->sc_dying)
		return (ENXIO);

	if (sc->sc_intr_number != -1 && sc->sc_intr_pipe == NULL) {
		sc->sc_intr_buf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);
		err = usbd_open_pipe_intr(sc->sc_iface,
		    sc->sc_intr_number,
		    USBD_SHORT_XFER_OK,
		    &sc->sc_intr_pipe,
		    sc,
		    sc->sc_intr_buf,
		    sc->sc_isize,
		    umsm_intr,
		    UMSM_INTR_INTERVAL);
		if (err) {
			printf("%s: cannot open interrupt pipe (addr %d)\n",
			    sc->sc_dev.dv_xname,
			    sc->sc_intr_number);
			return (EIO);
		}
	}

	return (0);
}

void
umsm_close(void *addr, int portno)
{
	struct umsm_softc *sc = addr;
	int err;

	if (sc->sc_dying)
		return;

	if (sc->sc_intr_pipe != NULL) {
		err = usbd_abort_pipe(sc->sc_intr_pipe);
       		if (err)
			printf("%s: abort interrupt pipe failed: %s\n",
			    sc->sc_dev.dv_xname,
			    usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_intr_pipe);
		if (err)
			printf("%s: close interrupt pipe failed: %s\n",
			    sc->sc_dev.dv_xname,
			    usbd_errstr(err));
		free(sc->sc_intr_buf, M_USBDEV);
		sc->sc_intr_pipe = NULL;
	}

}

void
umsm_intr(usbd_xfer_handle xfer, usbd_private_handle priv,
	usbd_status status)
{
	struct umsm_softc *sc = priv;
	u_char *buf;

	buf = sc->sc_intr_buf;
	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		DPRINTF(("%s: umsm_intr: abnormal status: %s\n",
		    sc->sc_dev.dv_xname, usbd_errstr(status)));
		usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);
		return;
	}

	/* XXX */
	sc->sc_lsr = buf[2];
	sc->sc_msr = buf[3];

	ucom_status_change((struct ucom_softc *)sc->sc_subdev);
}

void
umsm_get_status(void *addr, int portno, u_char *lsr, u_char *msr)
{
	struct umsm_softc *sc = addr;

	if (lsr != NULL)
		*lsr = sc->sc_lsr;
	if (msr != NULL)
		*msr = sc->sc_msr;
}

usbd_status
umsm_e220_changemode(usbd_device_handle dev)
{
	usb_device_request_t req;
	usbd_status err;

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, UF_DEVICE_REMOTE_WAKEUP);
	USETW(req.wIndex, E220_MODE_CHANGE_REQUEST);
	USETW(req.wLength, 0);

	err = usbd_do_request(dev, &req, 0);
	if (err) 
		return (EIO);

	return (0);
}
