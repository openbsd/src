/*	$OpenBSD: ualea.c,v 1.1 2015/04/16 08:55:21 mpi Exp $ */
/*
 * Copyright (c) 2006 Alexander Yurchenko <grange@openbsd.org>
 * Copyright (c) 2007 Marc Balmer <mbalmer@openbsd.org>
 * Copyright (C) 2015 Sean Levy <attila@stalphonsos.com>
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
 * Alea II TRNG.  Produces 100kbit/sec of entropy by black magic
 *
 * Product information in English can be found here:
 *     http://www.araneus.fi/products/alea2/en/
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdevs.h>

#include <dev/rndvar.h>

#define ALEA_IFACE		0
#define ALEA_MSECS		100
#define ALEA_READ_TIMEOUT	1000
#define ALEA_BUFSIZ		((1024/8)*100)	/* 100 kbits */

#define DEVNAME(_sc) ((_sc)->sc_dev.dv_xname)

struct ualea_softc {
	struct	device sc_dev;
	struct	usbd_device *sc_udev;
	struct	usbd_pipe *sc_pipe;
	struct	timeout sc_timeout;
	struct	usb_task sc_task;
	struct	usbd_xfer *sc_xfer;
	int	*sc_buf;
};

int ualea_match(struct device *, void *, void *);
void ualea_attach(struct device *, struct device *, void *);
int ualea_detach(struct device *, int);
void ualea_task(void *);
void ualea_timeout(void *);

struct cfdriver ualea_cd = {
	NULL, "ualea", DV_DULL
};

const struct cfattach ualea_ca = {
	sizeof(struct ualea_softc), ualea_match, ualea_attach, ualea_detach
};

int
ualea_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);
	if ((uaa->vendor == USB_VENDOR_ARANEUS) &&
	    (uaa->product == USB_PRODUCT_ARANEUS_ALEA) &&
	    (uaa->ifaceno == ALEA_IFACE))
		return (UMATCH_VENDOR_PRODUCT);
	return (UMATCH_NONE);
}

void
ualea_attach(struct device *parent, struct device *self, void *aux)
{
	struct ualea_softc *sc = (struct ualea_softc *)self;
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	int ep_ibulk = -1;
	usbd_status error;
	int i;

	sc->sc_udev = uaa->device;
	id = usbd_get_interface_descriptor(uaa->iface);
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(uaa->iface, i);
		if (ed == NULL) {
			printf("%s: failed to get endpoint %d descriptor\n",
			    DEVNAME(sc), i);
			return;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			ep_ibulk = ed->bEndpointAddress;
			break;
		}
	}
	if (ep_ibulk == -1) {
		printf("%s: missing endpoint\n", DEVNAME(sc));
		return;
	}
	error = usbd_open_pipe(uaa->iface, ep_ibulk, USBD_EXCLUSIVE_USE,
		    &sc->sc_pipe);
	if (error) {
		printf("%s: failed to open bulk-in pipe: %s\n",
		    DEVNAME(sc), usbd_errstr(error));
		return;
	}
	sc->sc_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_xfer == NULL) {
		printf("%s: could not alloc xfer\n", DEVNAME(sc));
		return;
	}
	sc->sc_buf = usbd_alloc_buffer(sc->sc_xfer, ALEA_BUFSIZ);
	if (sc->sc_buf == NULL) {
		printf("%s: could not alloc %d-byte buffer\n", DEVNAME(sc),
		    ALEA_BUFSIZ);
		return;
	}
	usb_init_task(&sc->sc_task, ualea_task, sc, USB_TASK_TYPE_GENERIC);
	timeout_set(&sc->sc_timeout, ualea_timeout, sc);
	usb_add_task(sc->sc_udev, &sc->sc_task);
}

int
ualea_detach(struct device *self, int flags)
{
	struct ualea_softc *sc = (struct ualea_softc *)self;

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	if (timeout_initialized(&sc->sc_timeout))
		timeout_del(&sc->sc_timeout);
	if (sc->sc_xfer)
		usbd_free_xfer(sc->sc_xfer);
	if (sc->sc_pipe != NULL)
		usbd_close_pipe(sc->sc_pipe);

	return (0);
}

void
ualea_task(void *arg)
{
	struct ualea_softc *sc = (struct ualea_softc *)arg;
	usbd_status error;
	u_int32_t len, i;

	usbd_setup_xfer(sc->sc_xfer, sc->sc_pipe, NULL, sc->sc_buf,
	    ALEA_BUFSIZ, USBD_SHORT_XFER_OK | USBD_SYNCHRONOUS,
	    ALEA_READ_TIMEOUT, NULL);
	error = usbd_transfer(sc->sc_xfer);
	if (error) {
		printf("%s: xfer failed: %s\n", DEVNAME(sc),
		    usbd_errstr(error));
		goto bail;
	}
	usbd_get_xfer_status(sc->sc_xfer, NULL, NULL, &len, NULL);
	if (len < sizeof(int)) {
		printf("%s: xfer too short (%u bytes) - dropping\n",
		    DEVNAME(sc), len);
		goto bail;
	}
	len /= sizeof(int);
	/*
	 * A random(|ness) koan:
	 *   children chug entropy like thirsty rhinos
	 *     surfing at the mall
	 *    privacy died in their hands
	 */
	for (i = 0; i < len; i++)
		add_true_randomness(sc->sc_buf[i]);
bail:
	timeout_add_msec(&sc->sc_timeout, ALEA_MSECS);
}

void
ualea_timeout(void *arg)
{
	struct ualea_softc *sc = arg;

	usb_add_task(sc->sc_udev, &sc->sc_task);
}
