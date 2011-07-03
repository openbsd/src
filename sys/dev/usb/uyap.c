/*	$OpenBSD: uyap.c,v 1.19 2011/07/03 15:47:18 matthew Exp $ */
/*	$NetBSD: uyap.c,v 1.6 2002/07/11 21:14:37 augustss Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by  Lennart Augustsson <lennart@augustsson.net>.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/tty.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/ezload.h>

struct uyap_softc {
	struct device		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;
};

int uyap_match(struct device *, void *, void *); 
void uyap_attach(struct device *, struct device *, void *); 
int uyap_detach(struct device *, int); 
int uyap_activate(struct device *, int); 

struct cfdriver uyap_cd = { 
	NULL, "uyap", DV_DULL 
}; 

const struct cfattach uyap_ca = { 
	sizeof(struct uyap_softc), 
	uyap_match, 
	uyap_attach, 
	uyap_detach, 
	uyap_activate, 
};
void uyap_attachhook(void *);

int
uyap_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	/* Match the boot device. */
	if (uaa->vendor == USB_VENDOR_SILICONPORTALS &&
	    uaa->product == USB_PRODUCT_SILICONPORTALS_YAPPH_NF)
		return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

void
uyap_attachhook(void *xsc)
{
	char *firmwares[] = { "uyap", NULL };
	struct uyap_softc *sc = xsc;
	int err;

	err = ezload_downloads_and_reset(sc->sc_udev, firmwares);
	if (err) {
		printf("%s: download ezdata format firmware error: %s\n",
		    sc->sc_dev.dv_xname, usbd_errstr(err));
		return;
	}

	printf("%s: firmware download complete, disconnecting.\n",
	    sc->sc_dev.dv_xname);
}

void
uyap_attach(struct device *parent, struct device *self, void *aux)
{
	struct uyap_softc *sc = (struct uyap_softc *)self;
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;

	printf("%s: downloading firmware\n", sc->sc_dev.dv_xname);

	sc->sc_udev = dev;
	if (rootvp == NULL)
		mountroothook_establish(uyap_attachhook, sc);
	else
		uyap_attachhook(sc);
}

int
uyap_detach(struct device *self, int flags)
{
	return (0);
}

int
uyap_activate(struct device *self, int act)
{
	struct uyap_softc *sc = (struct uyap_softc *)self;

	switch (act) {
	case DVACT_DEACTIVATE:
		usbd_deactivate(sc->sc_udev);
		break;
	}

	return 0;
}
