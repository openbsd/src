/*	$OpenBSD: uvisor.c,v 1.1 2000/07/04 11:34:31 fgsch Exp $	*/
/*	$NetBSD: uvisor.c,v 1.7 2000/06/01 14:29:03 augustss Exp $	*/

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
struct uvisor_connection_info {
	uWord	num_ports;
	struct {
		uByte	port_function_id;
		uByte	port;
	} connections[8];
};
#define UVISOR_CONNECTION_INFO_SIZE 18


/* struct uvisor_connection_info.connection[x].port defines: */
#define UVISOR_ENDPOINT_1		0x01
#define UVISOR_ENDPOINT_2		0x02

/* struct uvisor_connection_info.connection[x].port_function_id defines: */
#define UVISOR_FUNCTION_GENERIC		0x00
#define UVISOR_FUNCTION_DEBUGGER	0x01
#define UVISOR_FUNCTION_HOTSYNC		0x02
#define UVISOR_FUNCTION_CONSOLE		0x03
#define UVISOR_FUNCTION_REMOTE_FILE_SYS	0x04


#define UVISORIBUFSIZE 1024
#define UVISOROBUFSIZE 1024

struct uvisor_softc {
	USBBASEDEVICE		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* device */
	usbd_interface_handle	sc_iface;	/* interface */

	device_ptr_t		sc_subdev;

	u_char			sc_dying;
};

Static usbd_status uvisor_init(struct uvisor_softc *);

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

USB_DECLARE_DRIVER(uvisor);

USB_MATCH(uvisor)
{
	USB_MATCH_START(uvisor, uaa);
	
	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	DPRINTFN(20,("uvisor: vendor=0x%x, product=0x%x\n",
		     uaa->vendor, uaa->product));

	if (uaa->vendor == USB_VENDOR_HANDSPRING &&
	    uaa->product == USB_PRODUCT_HANDSPRING_VISOR)
		return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

USB_ATTACH(uvisor)
{
	USB_ATTACH_START(uvisor, sc, uaa);
	usbd_device_handle dev = uaa->device;
	usbd_interface_handle iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	char devinfo[1024];
	char *devname = USBDEVNAME(sc->sc_dev);
	int i;
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

	id = usbd_get_interface_descriptor(iface);

	sc->sc_udev = dev;
	sc->sc_iface = iface;

	uca.bulkin = uca.bulkout = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		int addr, dir, attr;
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: could not read endpoint descriptor"
			       ": %s\n", devname, usbd_errstr(err));
			goto bad;
		}
		
		addr = ed->bEndpointAddress;
		dir = UE_GET_DIR(ed->bEndpointAddress);
		attr = ed->bmAttributes & UE_XFERTYPE;
		if (dir == UE_DIR_IN && attr == UE_BULK)
			uca.bulkin = addr;
		else if (dir == UE_DIR_OUT && attr == UE_BULK)
			uca.bulkout = addr;
		else {
			printf("%s: unexpected endpoint\n", devname);
			goto bad;
		}
	}
	if (uca.bulkin == -1) {
		printf("%s: Could not find data bulk in\n",
		       USBDEVNAME(sc->sc_dev));
		goto bad;
	}
	if (uca.bulkout == -1) {
		printf("%s: Could not find data bulk out\n",
		       USBDEVNAME(sc->sc_dev));
		goto bad;
	}
	
	uca.portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	uca.ibufsize = UVISORIBUFSIZE;
	uca.obufsize = UVISOROBUFSIZE;
	uca.ibufsizepad = UVISORIBUFSIZE;
	uca.obufsizepad = UVISOROBUFSIZE;
	uca.device = dev;
	uca.iface = iface;
	uca.methods = &uvisor_methods;
	uca.arg = sc;

	err = uvisor_init(sc);
	if (err) {
		printf("%s: init failed, %s\n", USBDEVNAME(sc->sc_dev),
		       usbd_errstr(err));
		goto bad;
	}

	DPRINTF(("uvisor: in=0x%x out=0x%x\n", uca.bulkin, uca.bulkout));
	sc->sc_subdev = config_found_sm(self, &uca, ucomprint, ucomsubmatch);

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

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
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
uvisor_detach(device_ptr_t self, int flags)
{
	struct uvisor_softc *sc = (struct uvisor_softc *)self;
	int rv = 0;

	DPRINTF(("uvisor_detach: sc=%p flags=%d\n", sc, flags));
	sc->sc_dying = 1;
	if (sc->sc_subdev != NULL) {
		rv = config_detach(sc->sc_subdev, flags);
		sc->sc_subdev = NULL;
	}
	return (0);
}

usbd_status
uvisor_init(struct uvisor_softc *sc)
{
	usbd_status err;
	usb_device_request_t req;
	struct uvisor_connection_info coninfo;
	int actlen;
	uWord avail;

	DPRINTF(("uvisor_init: getting connection info\n"));
	req.bmRequestType = UT_READ_VENDOR_ENDPOINT;
	req.bRequest = UVISOR_GET_CONNECTION_INFORMATION;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, UVISOR_CONNECTION_INFO_SIZE);
	err = usbd_do_request_flags(sc->sc_udev, &req, &coninfo, 
				    USBD_SHORT_XFER_OK, &actlen);
	if (err)
		return (err);

#ifdef UVISOR_DEBUG
	{
		int i, np;
		char *string;

		np = UGETW(coninfo.num_ports);
		printf("%s: Number of ports: %d\n", USBDEVNAME(sc->sc_dev), np);
		for (i = 0; i < np; ++i) {
			switch (coninfo.connections[i].port_function_id) {
			case UVISOR_FUNCTION_GENERIC:
				string = "Generic";
				break;
			case UVISOR_FUNCTION_DEBUGGER:
				string = "Debugger";
				break;
			case UVISOR_FUNCTION_HOTSYNC:
				string = "HotSync";
				break;
			case UVISOR_FUNCTION_REMOTE_FILE_SYS:
				string = "Remote File System";
				break;
			default:
				string = "unknown";
				break;	
			}
			printf("%s: port %d, is for %s\n",
			    USBDEVNAME(sc->sc_dev), coninfo.connections[i].port,
			    string);
		}
	}
#endif

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
				    USBD_SHORT_XFER_OK, &actlen);
}
