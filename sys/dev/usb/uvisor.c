/*	$OpenBSD: uvisor.c,v 1.12 2002/07/25 04:07:33 nate Exp $	*/
/*	$NetBSD: uvisor.c,v 1.16 2002/07/11 21:14:36 augustss Exp $	*/

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
 * Handspring Visor (Palmpilot compatible PDA) driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/ucomvar.h>

#ifdef UVISOR_DEBUG
#define DPRINTF(x)	if (uvisordebug) printf x
#define DPRINTFN(n,x)	if (uvisordebug>(n)) printf x
int uvisordebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define UVISOR_CONFIG_INDEX	0
#define UVISOR_IFACE_INDEX	0

/* From the Linux driver */
/*
 * UVISOR_REQUEST_BYTES_AVAILABLE asks the visor for the number of bytes that
 * are available to be transfered to the host for the specified endpoint.
 * Currently this is not used, and always returns 0x0001
 */
#define UVISOR_REQUEST_BYTES_AVAILABLE		0x01

/*
 * UVISOR_CLOSE_NOTIFICATION is set to the device to notify it that the host
 * is now closing the pipe. An empty packet is sent in response.
 */
#define UVISOR_CLOSE_NOTIFICATION		0x02

/*
 * UVISOR_GET_CONNECTION_INFORMATION is sent by the host during enumeration to
 * get the endpoints used by the connection.
 */
#define UVISOR_GET_CONNECTION_INFORMATION	0x03


/*
 * UVISOR_GET_CONNECTION_INFORMATION returns data in the following format
 */
#define UVISOR_MAX_CONN 8
struct uvisor_connection_info {
	uWord	num_ports;
	struct {
		uByte	port_function_id;
		uByte	port;
	} connections[UVISOR_MAX_CONN];
};
#define UVISOR_CONNECTION_INFO_SIZE 18

/* struct uvisor_connection_info.connection[x].port_function_id defines: */
#define UVISOR_FUNCTION_GENERIC		0x00
#define UVISOR_FUNCTION_DEBUGGER	0x01
#define UVISOR_FUNCTION_HOTSYNC		0x02
#define UVISOR_FUNCTION_CONSOLE		0x03
#define UVISOR_FUNCTION_REMOTE_FILE_SYS	0x04

/*
 * Unknown PalmOS stuff.
 */
#define UVISOR_GET_PALM_INFORMATION		0x04
#define UVISOR_GET_PALM_INFORMATION_LEN		0x14


#define UVISORIBUFSIZE 1024
#define UVISOROBUFSIZE 1024

struct uvisor_softc {
	USBBASEDEVICE		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* device */
	usbd_interface_handle	sc_iface;	/* interface */

	device_ptr_t		sc_subdevs[UVISOR_MAX_CONN];
	int			sc_numcon;

	u_int16_t		sc_flags;

	u_char			sc_dying;
};

Static usbd_status uvisor_init(struct uvisor_softc *,
			       struct uvisor_connection_info *);

Static void uvisor_close(void *, int);


struct ucom_methods uvisor_methods = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	uvisor_close,
	NULL,
	NULL,
};

struct uvisor_type {
	struct usb_devno	uv_dev;
	u_int16_t		uv_flags;
#define PALM4	0x0001
};
static const struct uvisor_type uvisor_devs[] = {
	{{ USB_VENDOR_HANDSPRING, USB_PRODUCT_HANDSPRING_VISOR }, 0 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_M500 }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_M505 }, PALM4 },
	{{ USB_VENDOR_PALM, USB_PRODUCT_PALM_M125 }, PALM4 },
	{{ USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_40 }, PALM4 },
	{{ USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_41 }, 0 },
/*	{{ USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_25 }, PALM4 },*/
};
#define uvisor_lookup(v, p) ((struct uvisor_type *)usb_lookup(uvisor_devs, v, p))

USB_DECLARE_DRIVER(uvisor);

USB_MATCH(uvisor)
{
	USB_MATCH_START(uvisor, uaa);

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	DPRINTFN(20,("uvisor: vendor=0x%x, product=0x%x\n",
		     uaa->vendor, uaa->product));

	return (uvisor_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

USB_ATTACH(uvisor)
{
	USB_ATTACH_START(uvisor, sc, uaa);
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	struct uvisor_connection_info coninfo;
	usb_endpoint_descriptor_t *ed;
	char devinfo[1024];
	char *devname = USBDEVNAME(sc->sc_dev);
	int i, j, hasin, hasout, port;
	usbd_status err;
	struct ucom_attach_args uca;

	DPRINTFN(10,("\nuvisor_attach: sc=%p\n", sc));

	/* Move the device into the configured state. */
	err = usbd_set_config_index(dev, UVISOR_CONFIG_INDEX, 1);
	if (err) {
		printf("\n%s: failed to set configuration, err=%s\n",
		       devname, usbd_errstr(err));
		goto bad;
	}

	err = usbd_device2interface_handle(dev, UVISOR_IFACE_INDEX, &iface);
	if (err) {
		printf("\n%s: failed to get interface, err=%s\n",
		       devname, usbd_errstr(err));
		goto bad;
	}

	usbd_devinfo(dev, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", devname, devinfo);

	sc->sc_flags = uvisor_lookup(uaa->vendor, uaa->product)->uv_flags;

	id = usbd_get_interface_descriptor(iface);

	sc->sc_udev = dev;
	sc->sc_iface = iface;

	uca.ibufsize = UVISORIBUFSIZE;
	uca.obufsize = UVISOROBUFSIZE;
	uca.ibufsizepad = UVISORIBUFSIZE;
	uca.opkthdrlen = 0;
	uca.device = dev;
	uca.iface = iface;
	uca.methods = &uvisor_methods;
	uca.arg = sc;

	err = uvisor_init(sc, &coninfo);
	if (err) {
		printf("%s: init failed, %s\n", USBDEVNAME(sc->sc_dev),
		       usbd_errstr(err));
		goto bad;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	sc->sc_numcon = UGETW(coninfo.num_ports);
	if (sc->sc_numcon > UVISOR_MAX_CONN)
		sc->sc_numcon = UVISOR_MAX_CONN;

	/* Attach a ucom for each connection. */
	for (i = 0; i < sc->sc_numcon; ++i) {
		switch (coninfo.connections[i].port_function_id) {
		case UVISOR_FUNCTION_GENERIC:
			uca.info = "Generic";
			break;
		case UVISOR_FUNCTION_DEBUGGER:
			uca.info = "Debugger";
			break;
		case UVISOR_FUNCTION_HOTSYNC:
			uca.info = "HotSync";
			break;
		case UVISOR_FUNCTION_REMOTE_FILE_SYS:
			uca.info = "Remote File System";
			break;
		default:
			uca.info = "unknown";
			break;
		}
		port = coninfo.connections[i].port;
		uca.portno = port;
		uca.bulkin = port | UE_DIR_IN;
		uca.bulkout = port | UE_DIR_OUT;
		/* Verify that endpoints exist. */
		for (hasin = hasout = j = 0; j < id->bNumEndpoints; j++) {
			ed = usbd_interface2endpoint_descriptor(iface, j);
			if (ed == NULL)
				break;
			if (UE_GET_ADDR(ed->bEndpointAddress) == port &&
			    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
				if (UE_GET_DIR(ed->bEndpointAddress)
				    == UE_DIR_IN)
					hasin++;
				else
					hasout++;
			}
		}
		if (hasin == 1 && hasout == 1)
			sc->sc_subdevs[i] = config_found_sm(self, &uca,
			    ucomprint, ucomsubmatch);
		else
			printf("%s: no proper endpoints for port %d (%d,%d)\n",
			    USBDEVNAME(sc->sc_dev), port, hasin, hasout);
	}

	USB_ATTACH_SUCCESS_RETURN;

bad:
	DPRINTF(("uvisor_attach: ATTACH ERROR\n"));
	sc->sc_dying = 1;
	USB_ATTACH_ERROR_RETURN;
}

int
uvisor_activate(device_ptr_t self, enum devact act)
{
	struct uvisor_softc *sc = (struct uvisor_softc *)self;
	int rv = 0;
	int i;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		for (i = 0; i < sc->sc_numcon; i++)
			if (sc->sc_subdevs[i] != NULL)
				rv = config_deactivate(sc->sc_subdevs[i]);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

int
uvisor_detach(device_ptr_t self, int flags)
{
	struct uvisor_softc *sc = (struct uvisor_softc *)self;
	int rv = 0;
	int i;

	DPRINTF(("uvisor_detach: sc=%p flags=%d\n", sc, flags));
	sc->sc_dying = 1;
	for (i = 0; i < sc->sc_numcon; i++) {
		if (sc->sc_subdevs[i] != NULL) {
			rv |= config_detach(sc->sc_subdevs[i], flags);
			sc->sc_subdevs[i] = NULL;
		}
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	return (rv);
}

usbd_status
uvisor_init(struct uvisor_softc *sc, struct uvisor_connection_info *ci)
{
	usbd_status err;
	usb_device_request_t req;
	int actlen;
	uWord avail;
	char buffer[256];

	DPRINTF(("uvisor_init: getting connection info\n"));
	req.bmRequestType = UT_READ_VENDOR_ENDPOINT;
	req.bRequest = UVISOR_GET_CONNECTION_INFORMATION;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, UVISOR_CONNECTION_INFO_SIZE);
	err = usbd_do_request_flags(sc->sc_udev, &req, ci,
		  USBD_SHORT_XFER_OK, &actlen, USBD_DEFAULT_TIMEOUT);
	if (err)
		return (err);

	if (sc->sc_flags & PALM4) {
		/* Palm OS 4.0 Hack */
		req.bmRequestType = UT_READ_VENDOR_ENDPOINT;
		req.bRequest = UVISOR_GET_PALM_INFORMATION;
		USETW(req.wValue, 0);
		USETW(req.wIndex, 0);
		USETW(req.wLength, UVISOR_GET_PALM_INFORMATION_LEN);
		err = usbd_do_request(sc->sc_udev, &req, buffer);
		if (err)
			return (err);
		req.bmRequestType = UT_READ_VENDOR_ENDPOINT;
		req.bRequest = UVISOR_GET_PALM_INFORMATION;
		USETW(req.wValue, 0);
		USETW(req.wIndex, 0);
		USETW(req.wLength, UVISOR_GET_PALM_INFORMATION_LEN);
		err = usbd_do_request(sc->sc_udev, &req, buffer);
		if (err)
			return (err);
	}

	DPRINTF(("uvisor_init: getting available bytes\n"));
	req.bmRequestType = UT_READ_VENDOR_ENDPOINT;
	req.bRequest = UVISOR_REQUEST_BYTES_AVAILABLE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 5);
	USETW(req.wLength, sizeof avail);
	err = usbd_do_request(sc->sc_udev, &req, &avail);
	if (err)
		return (err);
	DPRINTF(("uvisor_init: avail=%d\n", UGETW(avail)));

	DPRINTF(("uvisor_init: done\n"));
	return (err);
}

void
uvisor_close(void *addr, int portno)
{
	struct uvisor_softc *sc = addr;
	usb_device_request_t req;
	struct uvisor_connection_info coninfo; /* XXX ? */
	int actlen;

	if (sc->sc_dying)
		return;

	req.bmRequestType = UT_READ_VENDOR_ENDPOINT; /* XXX read? */
	req.bRequest = UVISOR_CLOSE_NOTIFICATION;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, UVISOR_CONNECTION_INFO_SIZE);
	(void)usbd_do_request_flags(sc->sc_udev, &req, &coninfo,
		  USBD_SHORT_XFER_OK, &actlen, USBD_DEFAULT_TIMEOUT);
}
