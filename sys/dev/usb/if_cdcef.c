/*	$OpenBSD: if_cdcef.c,v 1.1 2006/11/25 18:10:29 uwe Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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

/*
 * USB Communication Device Class Ethernet Emulation Model function driver
 * (counterpart of the host-side cdce(4) driver)
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbf.h>
#include <dev/usb/usbcdc.h>

#define CDCEF_VENDOR_ID		0x0001
#define CDCEF_PRODUCT_ID	0x0001
#define CDCEF_DEVICE_CODE	0x0100
#define CDCEF_VENDOR_STRING	"OpenBSD.org"
#define CDCEF_PRODUCT_STRING	"CDC Ethernet Emulation"
#define CDCEF_SERIAL_STRING	"1.00"

#define CDCEF_BUFSZ		65536

struct cdcef_softc {
	struct usbf_function	sc_dev;
	usbf_config_handle	sc_config;
	usbf_interface_handle	sc_iface;
	usbf_endpoint_handle	sc_ep_in;
	usbf_endpoint_handle	sc_ep_out;
	usbf_pipe_handle	sc_pipe_in;
	usbf_pipe_handle	sc_pipe_out;
	usbf_xfer_handle	sc_xfer_in;
	usbf_xfer_handle	sc_xfer_out;
	void			*sc_buffer_in;
	void			*sc_buffer_out;
};

int		cdcef_match(struct device *, void *, void *);
void		cdcef_attach(struct device *, struct device *, void *);

usbf_status	cdcef_do_request(usbf_function_handle,
				 usb_device_request_t *, void **);

void		cdcef_start(struct ifnet *);

void		cdcef_txeof(usbf_xfer_handle, usbf_private_handle,
			    usbf_status);
void		cdcef_rxeof(usbf_xfer_handle, usbf_private_handle,
			    usbf_status);

struct cfattach cdcef_ca = {
	sizeof(struct cdcef_softc), cdcef_match, cdcef_attach
};

struct cfdriver cdcef_cd = {
	NULL, "cdcef", DV_DULL
};

struct usbf_function_methods cdcef_methods = {
	NULL,			/* set_config */
	cdcef_do_request
};

#ifndef CDCEF_DEBUG
#define DPRINTF(x)	do {} while (0)
#else
#define DPRINTF(x)	printf x
#endif

#define DEVNAME(sc)	USBDEVNAME((sc)->sc_dev.bdev)

/*
 * USB function match/attach/detach
 */

USB_MATCH(cdcef)
{
	return UMATCH_GENERIC;
}

USB_ATTACH(cdcef)
{
	struct cdcef_softc *sc = (struct cdcef_softc *)self;
	struct usbf_attach_arg *uaa = aux;
	usbf_device_handle dev = uaa->device;
	char *devinfop;
	usbf_status err;
	usb_cdc_union_descriptor_t udesc;

	/* Set the device identification according to the function. */
	usbf_devinfo_setup(dev, UDCLASS_IN_INTERFACE, 0, 0, CDCEF_VENDOR_ID,
	    CDCEF_PRODUCT_ID, CDCEF_DEVICE_CODE, CDCEF_VENDOR_STRING,
	    CDCEF_PRODUCT_STRING, CDCEF_SERIAL_STRING);

	devinfop = usbf_devinfo_alloc(dev);
	printf(": %s\n", devinfop);
	usbf_devinfo_free(devinfop);

	/* Fill in the fields needed by the parent device. */
	sc->sc_dev.methods = &cdcef_methods;

	/*
	 * Build descriptors according to the device class specification.
	 */
	err = usbf_add_config(dev, &sc->sc_config);
	if (err) {
		printf("%s: usbf_add_config failed\n", DEVNAME(sc));
		USB_ATTACH_ERROR_RETURN;
	}
	err = usbf_add_interface(sc->sc_config, UICLASS_CDC,
	    UISUBCLASS_ETHERNET_NETWORKING_CONTROL_MODEL, 0, NULL,
	    &sc->sc_iface);
	if (err) {
		printf("%s: usbf_add_interface failed\n", DEVNAME(sc));
		USB_ATTACH_ERROR_RETURN;
	}
	/* XXX don't use hard-coded values 128 and 16. */
	err = usbf_add_endpoint(sc->sc_iface, UE_DIR_IN, UE_BULK,
	    128, 16, &sc->sc_ep_in) ||
	    usbf_add_endpoint(sc->sc_iface, UE_DIR_OUT, UE_BULK,
	    128, 16, &sc->sc_ep_out);
	if (err) {
		printf("%s: usbf_add_endpoint failed\n", DEVNAME(sc));
		USB_ATTACH_ERROR_RETURN;
	}

	/* Append a CDC union descriptor. */
	bzero(&udesc, sizeof udesc);
	udesc.bLength = sizeof udesc;
	udesc.bDescriptorType = UDESC_CS_INTERFACE;
	udesc.bDescriptorSubtype = UDESCSUB_CDC_UNION;
	udesc.bSlaveInterface[0] = usbf_interface_number(sc->sc_iface);
	err = usbf_add_config_desc(sc->sc_config,
	    (usb_descriptor_t *)&udesc, NULL);
	if (err) {
		printf("%s: usbf_add_config_desc failed\n", DEVNAME(sc));
		USB_ATTACH_ERROR_RETURN;
	}

	/*
	 * Close the configuration and build permanent descriptors.
	 */
	err = usbf_end_config(sc->sc_config);
	if (err) {
		printf("%s: usbf_end_config failed\n", DEVNAME(sc));
		USB_ATTACH_ERROR_RETURN;
	}

	/* Preallocate xfers and data buffers. */
	sc->sc_xfer_in = usbf_alloc_xfer(dev);
	sc->sc_xfer_out = usbf_alloc_xfer(dev);
	sc->sc_buffer_in = usbf_alloc_buffer(sc->sc_xfer_in,
	    CDCEF_BUFSZ);
	sc->sc_buffer_out = usbf_alloc_buffer(sc->sc_xfer_out,
	    CDCEF_BUFSZ);
	if (sc->sc_buffer_in == NULL || sc->sc_buffer_out == NULL) {
		printf("%s: usbf_alloc_buffer failed\n", DEVNAME(sc));
		USB_ATTACH_ERROR_RETURN;
	}

	/* Open the bulk pipes. */
	err = usbf_open_pipe(sc->sc_iface,
	    usbf_endpoint_address(sc->sc_ep_in), &sc->sc_pipe_in) ||
	    usbf_open_pipe(sc->sc_iface,
	    usbf_endpoint_address(sc->sc_ep_out), &sc->sc_pipe_out);
	if (err) {
		printf("%s: usbf_open_pipe failed\n", DEVNAME(sc));
		USB_ATTACH_ERROR_RETURN;
	}

	/* Get ready to receive packets. */
	usbf_setup_xfer(sc->sc_xfer_out, sc->sc_pipe_out, (void *)sc,
	    sc->sc_buffer_out, CDCEF_BUFSZ, 0, 0, cdcef_rxeof);
	err = usbf_transfer(sc->sc_xfer_out);
	if (err && err != USBF_IN_PROGRESS) {
		printf("%s: usbf_transfer failed\n", DEVNAME(sc));
		USB_ATTACH_ERROR_RETURN;
	}

	USB_ATTACH_SUCCESS_RETURN;
}

usbf_status
cdcef_do_request(usbf_function_handle fun, usb_device_request_t *req,
    void **data)
{
	return USBF_STALLED;
}

void
cdcef_start(struct ifnet *ifp)
{
}

void
cdcef_txeof(usbf_xfer_handle xfer, usbf_private_handle priv,
    usbf_status err)
{
	struct cdcef_softc *sc = priv;

	printf("cdcef_txeof: xfer=%p, priv=%p, %s\n", xfer, priv,
	    usbf_errstr(err));

	/* Setup another xfer. */
	usbf_setup_xfer(xfer, sc->sc_pipe_in, (void *)sc,
	    sc->sc_buffer_in, CDCEF_BUFSZ, 0, 0, cdcef_rxeof);
	err = usbf_transfer(xfer);
	if (err && err != USBF_IN_PROGRESS) {
		printf("%s: usbf_transfer failed\n", DEVNAME(sc));
		USB_ATTACH_ERROR_RETURN;
	}
}

void
cdcef_rxeof(usbf_xfer_handle xfer, usbf_private_handle priv,
    usbf_status err)
{
	struct cdcef_softc *sc = priv;

	printf("cdcef_txeof: xfer=%p, priv=%p, %s\n", xfer, priv,
	    usbf_errstr(err));

	/* Setup another xfer. */
	usbf_setup_xfer(xfer, sc->sc_pipe_out, (void *)sc,
	    sc->sc_buffer_out, CDCEF_BUFSZ, 0, 0, cdcef_rxeof);
	err = usbf_transfer(xfer);
	if (err && err != USBF_IN_PROGRESS) {
		printf("%s: usbf_transfer failed\n", DEVNAME(sc));
		USB_ATTACH_ERROR_RETURN;
	}
}
