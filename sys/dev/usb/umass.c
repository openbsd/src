/*	$OpenBSD: umass.c,v 1.45 2007/05/27 04:00:25 jsg Exp $ */
/*	$NetBSD: umass.c,v 1.116 2004/06/30 05:53:46 mycroft Exp $	*/

/*
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1999 MAEKAWA Masahide <bishop@rr.iij4u.or.jp>,
 *		      Nick Hibma <n_hibma@freebsd.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *     $FreeBSD: src/sys/dev/usb/umass.c,v 1.13 2000/03/26 01:39:12 n_hibma Exp $
 */

/*
 * Universal Serial Bus Mass Storage Class specs:
 * http://www.usb.org/developers/devclass_docs/usb_msc_overview_1.2.pdf
 * http://www.usb.org/developers/devclass_docs/usbmassbulk_10.pdf
 * http://www.usb.org/developers/devclass_docs/usb_msc_cbi_1.1.pdf
 * http://www.usb.org/developers/devclass_docs/usbmass-ufi10.pdf
 */

/*
 * Ported to NetBSD by Lennart Augustsson <augustss@NetBSD.org>.
 * Parts of the code written by Jason R. Thorpe <thorpej@shagadelic.org>.
 */

/*
 * The driver handles 3 Wire Protocols
 * - Command/Bulk/Interrupt (CBI)
 * - Command/Bulk/Interrupt with Command Completion Interrupt (CBI with CCI)
 * - Mass Storage Bulk-Only (BBB)
 *   (BBB refers Bulk/Bulk/Bulk for Command/Data/Status phases)
 *
 * Over these wire protocols it handles the following command protocols
 * - SCSI
 * - 8070 (ATA/ATAPI for rewritable removable media)
 * - UFI (USB Floppy Interface)
 *
 * 8070i is a transformed version of the SCSI command set. UFI is a transformed
 * version of the 8070i command set.  The sc->transform method is used to
 * convert the commands into the appropriate format (if at all necessary).
 * For example, ATAPI requires all commands to be 12 bytes in length amongst
 * other things.
 *
 * The source code below is marked and can be split into a number of pieces
 * (in this order):
 *
 * - probe/attach/detach
 * - generic transfer routines
 * - BBB
 * - CBI
 * - CBI_I (in addition to functions from CBI)
 * - CAM (Common Access Method)
 * - SCSI
 * - UFI
 * - 8070i
 *
 * The protocols are implemented using a state machine, for the transfers as
 * well as for the resets. The state machine is contained in umass_*_state.
 * The state machine is started through either umass_*_transfer or
 * umass_*_reset.
 *
 * The reason for doing this is a) CAM performs a lot better this way and b) it
 * avoids using tsleep from interrupt context (for example after a failed
 * transfer).
 */

/*
 * The SCSI related part of this driver has been derived from the
 * dev/ppbus/vpo.c driver, by Nicolas Souchu (nsouch@freebsd.org).
 *
 * The CAM layer uses so called actions which are messages sent to the host
 * adapter for completion. The actions come in through umass_cam_action. The
 * appropriate block of routines is called depending on the transport protocol
 * in use. When the transfer has finished, these routines call
 * umass_cam_cb again to complete the CAM command.
 */

#include "atapiscsi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#undef KASSERT
#define KASSERT(cond, msg)
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/clock.h>
#endif
#include <machine/bus.h>

#include <scsi/scsi_all.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/umassvar.h>
#include <dev/usb/umass_quirks.h>
#include <dev/usb/umass_scsi.h>


#ifdef UMASS_DEBUG
int umassdebug = 0;

char *states[TSTATE_STATES+1] = {
	/* should be kept in sync with the list at transfer_state */
	"Idle",
	"BBB CBW",
	"BBB Data",
	"BBB Data bulk-in/-out clear stall",
	"BBB CSW, 1st attempt",
	"BBB CSW bulk-in clear stall",
	"BBB CSW, 2nd attempt",
	"BBB Reset",
	"BBB bulk-in clear stall",
	"BBB bulk-out clear stall",
	"CBI Command",
	"CBI Data",
	"CBI Status",
	"CBI Data bulk-in/-out clear stall",
	"CBI Status intr-in clear stall",
	"CBI Reset",
	"CBI bulk-in clear stall",
	"CBI bulk-out clear stall",
	NULL
};
#endif

/* USB device probe/attach/detach functions */
USB_DECLARE_DRIVER(umass);
Static void umass_disco(struct umass_softc *sc);

/* generic transfer functions */
Static usbd_status umass_polled_transfer(struct umass_softc *sc,
				usbd_xfer_handle xfer);
Static usbd_status umass_setup_transfer(struct umass_softc *sc,
				usbd_pipe_handle pipe,
				void *buffer, int buflen, int flags,
				usbd_xfer_handle xfer);
Static usbd_status umass_setup_ctrl_transfer(struct umass_softc *sc,
				usb_device_request_t *req,
				void *buffer, int buflen, int flags,
				usbd_xfer_handle xfer);
Static void umass_clear_endpoint_stall(struct umass_softc *sc, int endpt,
				usbd_xfer_handle xfer);
Static void umass_adjust_transfer(struct umass_softc *);
#if 0
Static void umass_reset(struct umass_softc *sc,	transfer_cb_f cb, void *priv);
#endif

/* Bulk-Only related functions */
Static void umass_bbb_transfer(struct umass_softc *, int, void *, int, void *,
			       int, int, u_int, umass_callback, void *);
Static void umass_bbb_reset(struct umass_softc *, int);
Static void umass_bbb_state(usbd_xfer_handle, usbd_private_handle, usbd_status);

usbd_status umass_bbb_get_max_lun(struct umass_softc *, u_int8_t *);

/* CBI related functions */
Static void umass_cbi_transfer(struct umass_softc *, int, void *, int, void *,
			       int, int, u_int, umass_callback, void *);
Static void umass_cbi_reset(struct umass_softc *, int);
Static void umass_cbi_state(usbd_xfer_handle, usbd_private_handle, usbd_status);

Static int umass_cbi_adsc(struct umass_softc *, char *, int, usbd_xfer_handle);

const struct umass_wire_methods umass_bbb_methods = {
	umass_bbb_transfer,
	umass_bbb_reset,
	umass_bbb_state
};

const struct umass_wire_methods umass_cbi_methods = {
	umass_cbi_transfer,
	umass_cbi_reset,
	umass_cbi_state
};

#ifdef UMASS_DEBUG
/* General debugging functions */
Static void umass_bbb_dump_cbw(struct umass_softc *sc,
				umass_bbb_cbw_t *cbw);
Static void umass_bbb_dump_csw(struct umass_softc *sc,
				umass_bbb_csw_t *csw);
Static void umass_dump_buffer(struct umass_softc *sc, u_int8_t *buffer,
				int buflen, int printlen);
#endif


/*
 * USB device probe/attach/detach
 */

int
umass_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	const struct umass_quirk *quirk;
	usb_interface_descriptor_t *id;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);

	quirk = umass_lookup(uaa->vendor, uaa->product);
	if (quirk != NULL)
		return (quirk->uq_match);

	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL || id->bInterfaceClass != UICLASS_MASS)
		return (UMATCH_NONE);

	switch (id->bInterfaceSubClass) {
	case UISUBCLASS_RBC:
	case UISUBCLASS_SFF8020I:
	case UISUBCLASS_QIC157:
	case UISUBCLASS_UFI:
	case UISUBCLASS_SFF8070I:
	case UISUBCLASS_SCSI:
		break;
	default:
		return (UMATCH_IFACECLASS);
	}

	switch (id->bInterfaceProtocol) {
	case UIPROTO_MASS_CBI_I:
	case UIPROTO_MASS_CBI:
	case UIPROTO_MASS_BBB_OLD:
	case UIPROTO_MASS_BBB:
		break;
	default:
		return (UMATCH_IFACECLASS_IFACESUBCLASS);
	}

	return (UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO);
}

void
umass_attach(struct device *parent, struct device *self, void *aux)
{
	struct umass_softc *sc = (struct umass_softc *)self;
	struct usb_attach_arg *uaa = aux;
	const struct umass_quirk *quirk;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	const char *sWire, *sCommand;
	char *devinfop;
	usbd_status err;
	int i, bno, error;

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	printf("\n%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
	usbd_devinfo_free(devinfop);

	sc->sc_udev = uaa->device;
	sc->sc_iface = uaa->iface;
	sc->sc_ifaceno = uaa->ifaceno;

	quirk = umass_lookup(uaa->vendor, uaa->product);
	if (quirk != NULL) {
		sc->sc_wire = quirk->uq_wire;
		sc->sc_cmd = quirk->uq_cmd;
		sc->sc_quirks = quirk->uq_flags;
		sc->sc_busquirks = quirk->uq_busquirks;

		if (quirk->uq_fixup != NULL)
			(*quirk->uq_fixup)(sc);
	} else {
		sc->sc_wire = UMASS_WPROTO_UNSPEC;
		sc->sc_cmd = UMASS_CPROTO_UNSPEC;
		sc->sc_quirks = 0;
		sc->sc_busquirks = 0;
	}

	id = usbd_get_interface_descriptor(sc->sc_iface);
	if (id == NULL)
		return;

	if (sc->sc_wire == UMASS_WPROTO_UNSPEC) {
		switch (id->bInterfaceProtocol) {
		case UIPROTO_MASS_CBI:
			sc->sc_wire = UMASS_WPROTO_CBI;
			break;
		case UIPROTO_MASS_CBI_I:
			sc->sc_wire = UMASS_WPROTO_CBI_I;
			break;
		case UIPROTO_MASS_BBB:
		case UIPROTO_MASS_BBB_OLD:
			sc->sc_wire = UMASS_WPROTO_BBB;
			break;
		default:
			DPRINTF(UDMASS_GEN,
				("%s: Unsupported wire protocol %u\n",
				USBDEVNAME(sc->sc_dev),
				id->bInterfaceProtocol));
			return;
		}
	}

	if (sc->sc_cmd == UMASS_CPROTO_UNSPEC) {
		switch (id->bInterfaceSubClass) {
		case UISUBCLASS_SCSI:
			sc->sc_cmd = UMASS_CPROTO_SCSI;
			break;
		case UISUBCLASS_UFI:
			sc->sc_cmd = UMASS_CPROTO_UFI;
			break;
		case UISUBCLASS_SFF8020I:
		case UISUBCLASS_SFF8070I:
		case UISUBCLASS_QIC157:
			sc->sc_cmd = UMASS_CPROTO_ATAPI;
			break;
		case UISUBCLASS_RBC:
			sc->sc_cmd = UMASS_CPROTO_RBC;
			break;
		default:
			DPRINTF(UDMASS_GEN,
				("%s: Unsupported command protocol %u\n",
				USBDEVNAME(sc->sc_dev),
				id->bInterfaceSubClass));
			return;
		}
	}

	switch (sc->sc_wire) {
	case UMASS_WPROTO_CBI:
		sWire = "CBI";
		break;
	case UMASS_WPROTO_CBI_I:
		sWire = "CBI with CCI";
		break;
	case UMASS_WPROTO_BBB:
		sWire = "Bulk-Only";
		break;
	default:
		sWire = "unknown";
		break;
	}

	switch (sc->sc_cmd) {
	case UMASS_CPROTO_RBC:
		sCommand = "RBC";
		break;
	case UMASS_CPROTO_SCSI:
		sCommand = "SCSI";
		break;
	case UMASS_CPROTO_UFI:
		sCommand = "UFI";
		break;
	case UMASS_CPROTO_ATAPI:
		sCommand = "ATAPI";
		break;
	case UMASS_CPROTO_ISD_ATA:
		sCommand = "ISD-ATA";
		break;
	default:
		sCommand = "unknown";
		break;
	}

	printf("%s: using %s over %s\n", USBDEVNAME(sc->sc_dev), sCommand,
	       sWire);

	if (quirk != NULL && quirk->uq_init != NULL) {
		err = (*quirk->uq_init)(sc);
		if (err) {
			umass_disco(sc);
			return;
		}
	}

	/*
	 * In addition to the Control endpoint the following endpoints
	 * are required:
	 * a) bulk-in endpoint.
	 * b) bulk-out endpoint.
	 * and for Control/Bulk/Interrupt with CCI (CBI_I)
	 * c) intr-in
	 *
	 * The endpoint addresses are not fixed, so we have to read them
	 * from the device descriptors of the current interface.
	 */
	for (i = 0 ; i < id->bNumEndpoints ; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			printf("%s: could not read endpoint descriptor\n",
			       USBDEVNAME(sc->sc_dev));
			return;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->sc_epaddr[UMASS_BULKIN] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->sc_epaddr[UMASS_BULKOUT] = ed->bEndpointAddress;
		} else if (sc->sc_wire == UMASS_WPROTO_CBI_I
		    && UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN
		    && (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			sc->sc_epaddr[UMASS_INTRIN] = ed->bEndpointAddress;
#ifdef UMASS_DEBUG
			if (UGETW(ed->wMaxPacketSize) > 2) {
				DPRINTF(UDMASS_CBI, ("%s: intr size is %d\n",
					USBDEVNAME(sc->sc_dev),
					UGETW(ed->wMaxPacketSize)));
			}
#endif
		}
	}

	/* check whether we found all the endpoints we need */
	if (!sc->sc_epaddr[UMASS_BULKIN] || !sc->sc_epaddr[UMASS_BULKOUT] ||
	    (sc->sc_wire == UMASS_WPROTO_CBI_I &&
	     !sc->sc_epaddr[UMASS_INTRIN])) {
		DPRINTF(UDMASS_USB, ("%s: endpoint not found %u/%u/%u\n",
			USBDEVNAME(sc->sc_dev), sc->sc_epaddr[UMASS_BULKIN],
			sc->sc_epaddr[UMASS_BULKOUT],
			sc->sc_epaddr[UMASS_INTRIN]));
		return;
	}

	/*
	 * Get the maximum LUN supported by the device.
	 */
	if (sc->sc_wire == UMASS_WPROTO_BBB) {
		err = umass_bbb_get_max_lun(sc, &sc->maxlun);
		if (err) {
			printf("%s: unable to get Max Lun: %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			return;
		}
	} else {
		sc->maxlun = 0;
	}

	/* Open the bulk-in and -out pipe */
	DPRINTF(UDMASS_USB, ("%s: opening iface %p epaddr %d for BULKOUT\n",
	    USBDEVNAME(sc->sc_dev), sc->sc_iface,
	    sc->sc_epaddr[UMASS_BULKOUT]));
	err = usbd_open_pipe(sc->sc_iface, sc->sc_epaddr[UMASS_BULKOUT],
				USBD_EXCLUSIVE_USE,
				&sc->sc_pipe[UMASS_BULKOUT]);
	if (err) {
		DPRINTF(UDMASS_USB, ("%s: cannot open %u-out pipe (bulk)\n",
			USBDEVNAME(sc->sc_dev), sc->sc_epaddr[UMASS_BULKOUT]));
		umass_disco(sc);
		return;
	}
	DPRINTF(UDMASS_USB, ("%s: opening iface %p epaddr %d for BULKIN\n",
	    USBDEVNAME(sc->sc_dev), sc->sc_iface,
	    sc->sc_epaddr[UMASS_BULKIN]));
	err = usbd_open_pipe(sc->sc_iface, sc->sc_epaddr[UMASS_BULKIN],
				USBD_EXCLUSIVE_USE, &sc->sc_pipe[UMASS_BULKIN]);
	if (err) {
		DPRINTF(UDMASS_USB, ("%s: could not open %u-in pipe (bulk)\n",
			USBDEVNAME(sc->sc_dev), sc->sc_epaddr[UMASS_BULKIN]));
		umass_disco(sc);
		return;
	}
	/*
	 * Open the intr-in pipe if the protocol is CBI with CCI.
	 * Note: early versions of the Zip drive do have an interrupt pipe, but
	 * this pipe is unused
	 *
	 * We do not open the interrupt pipe as an interrupt pipe, but as a
	 * normal bulk endpoint. We send an IN transfer down the wire at the
	 * appropriate time, because we know exactly when to expect data on
	 * that endpoint. This saves bandwidth, but more important, makes the
	 * code for handling the data on that endpoint simpler. No data
	 * arriving concurrently.
	 */
	if (sc->sc_wire == UMASS_WPROTO_CBI_I) {
		DPRINTF(UDMASS_USB, ("%s: opening iface %p epaddr %d for INTRIN\n",
		    USBDEVNAME(sc->sc_dev), sc->sc_iface,
		    sc->sc_epaddr[UMASS_INTRIN]));
		err = usbd_open_pipe(sc->sc_iface, sc->sc_epaddr[UMASS_INTRIN],
				USBD_EXCLUSIVE_USE, &sc->sc_pipe[UMASS_INTRIN]);
		if (err) {
			DPRINTF(UDMASS_USB, ("%s: couldn't open %u-in (intr)\n",
				USBDEVNAME(sc->sc_dev),
				sc->sc_epaddr[UMASS_INTRIN]));
			umass_disco(sc);
			return;
		}
	}

	/* initialisation of generic part */
	sc->transfer_state = TSTATE_IDLE;

	/* request a sufficient number of xfer handles */
	for (i = 0; i < XFER_NR; i++) {
		sc->transfer_xfer[i] = usbd_alloc_xfer(uaa->device);
		if (sc->transfer_xfer[i] == NULL) {
			DPRINTF(UDMASS_USB, ("%s: Out of memory\n",
				USBDEVNAME(sc->sc_dev)));
			umass_disco(sc);
			return;
		}
	}
	/* Allocate buffer for data transfer (it's huge). */
	switch (sc->sc_wire) {
	case UMASS_WPROTO_BBB:
		bno = XFER_BBB_DATA;
		goto dalloc;
	case UMASS_WPROTO_CBI:
		bno = XFER_CBI_DATA;
		goto dalloc;
	case UMASS_WPROTO_CBI_I:
		bno = XFER_CBI_DATA;
	dalloc:
		sc->data_buffer = usbd_alloc_buffer(sc->transfer_xfer[bno],
						    UMASS_MAX_TRANSFER_SIZE);
		if (sc->data_buffer == NULL) {
			umass_disco(sc);
			return;
		}
		break;
	default:
		break;
	}

	/* Initialise the wire protocol specific methods */
	switch (sc->sc_wire) {
	case UMASS_WPROTO_BBB:
		sc->sc_methods = &umass_bbb_methods;
		break;
	case UMASS_WPROTO_CBI:
	case UMASS_WPROTO_CBI_I:
		sc->sc_methods = &umass_cbi_methods;
		break;
	default:
		umass_disco(sc);
		return;
	}

	error = 0;
	switch (sc->sc_cmd) {
	case UMASS_CPROTO_RBC:
	case UMASS_CPROTO_SCSI:
#if defined(__OpenBSD__) || NSCSIBUS > 0
		error = umass_scsi_attach(sc);
#else
		printf("%s: scsibus not configured\n", USBDEVNAME(sc->sc_dev));
#endif
		break;

	case UMASS_CPROTO_UFI:
	case UMASS_CPROTO_ATAPI:
#if (NATAPIBUS > 0) || (NATAPISCSI > 0)
		error = umass_atapi_attach(sc);
#else
		printf("%s: "UMASS_ATAPISTR" not configured\n",
		       USBDEVNAME(sc->sc_dev));
#endif
		break;

	case UMASS_CPROTO_ISD_ATA:
#if defined (__NetBSD__) && NWD > 0
		error = umass_isdata_attach(sc);
#else
		printf("%s: isdata not configured\n", USBDEVNAME(sc->sc_dev));
#endif
		break;

	default:
		printf("%s: command protocol=0x%x not supported\n",
		       USBDEVNAME(sc->sc_dev), sc->sc_cmd);
		umass_disco(sc);
		return;
	}
	if (error) {
		printf("%s: bus attach failed\n", USBDEVNAME(sc->sc_dev));
		umass_disco(sc);
		return;
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	DPRINTF(UDMASS_GEN, ("%s: Attach finished\n", USBDEVNAME(sc->sc_dev)));
}

int
umass_detach(struct device *self, int flags)
{
	struct umass_softc *sc = (struct umass_softc *)self;
	struct umassbus_softc *scbus;
	int rv = 0, i, s;

	DPRINTF(UDMASS_USB, ("%s: detached\n", USBDEVNAME(sc->sc_dev)));

	/* Abort the pipes to wake up any waiting processes. */
	for (i = 0 ; i < UMASS_NEP ; i++) {
		if (sc->sc_pipe[i] != NULL) {
			usbd_abort_pipe(sc->sc_pipe[i]);
			sc->sc_pipe[i] = NULL;
		}
	}

	/* Do we really need reference counting?  Perhaps in ioctl() */
	s = splusb();
	if (--sc->sc_refcnt >= 0) {
#ifdef DIAGNOSTIC
		printf("%s: waiting for refcnt\n", USBDEVNAME(sc->sc_dev));
#endif
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->sc_dev));
	}
	splx(s);

	scbus = sc->bus;
	if (scbus != NULL) {
		if (scbus->sc_child != NULL)
			rv = config_detach(scbus->sc_child, flags);
		free(scbus, M_DEVBUF);
		sc->bus = NULL;
	}

	if (rv != 0)
		return (rv);

	umass_disco(sc);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	return (rv);
}

int
umass_activate(struct device *dev, enum devact act)
{
	struct umass_softc *sc = (struct umass_softc *)dev;
	struct umassbus_softc *scbus = sc->bus;
	int rv = 0;

	DPRINTF(UDMASS_USB, ("%s: umass_activate: %d\n",
	    USBDEVNAME(sc->sc_dev), act));

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if (scbus == NULL || scbus->sc_child == NULL)
			break;
		rv = config_deactivate(scbus->sc_child);
		DPRINTF(UDMASS_USB, ("%s: umass_activate: child "
		    "returned %d\n", USBDEVNAME(sc->sc_dev), rv));
		break;
	}
	return (rv);
}

Static void
umass_disco(struct umass_softc *sc)
{
	int i;

	DPRINTF(UDMASS_GEN, ("umass_disco\n"));

	/* Free the xfers. */
	for (i = 0; i < XFER_NR; i++)
		if (sc->transfer_xfer[i] != NULL) {
			usbd_free_xfer(sc->transfer_xfer[i]);
			sc->transfer_xfer[i] = NULL;
		}

	/* Remove all the pipes. */
	for (i = 0 ; i < UMASS_NEP ; i++) {
		if (sc->sc_pipe[i] != NULL) {
			usbd_close_pipe(sc->sc_pipe[i]);
			sc->sc_pipe[i] = NULL;
		}
	}
}

/*
 * Generic functions to handle transfers
 */

Static usbd_status
umass_polled_transfer(struct umass_softc *sc, usbd_xfer_handle xfer)
{
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	/*
	 * If a polled transfer is already in progress, preserve the new
	 * usbd_xfer_handle and run it after the running one completes.
	 * This converts the recursive calls into the umass_*_state callbacks
	 * into iteration, preventing us from running out of stack under
	 * error conditions.
	 */
	if (sc->polling_depth) {
		if (sc->next_polled_xfer)
			panic("%s: got polled xfer %p, but %p already "
			    "pending\n", USBDEVNAME(sc->sc_dev), xfer,
			    sc->next_polled_xfer);

		DPRINTF(UDMASS_XFER, ("%s: saving polled xfer %p\n",
		    USBDEVNAME(sc->sc_dev), xfer));
		sc->next_polled_xfer = xfer;

		return (USBD_IN_PROGRESS);
	}

	sc->polling_depth++;

start_next_xfer:
	DPRINTF(UDMASS_XFER, ("%s: start polled xfer %p\n",
	    USBDEVNAME(sc->sc_dev), xfer));
	err = usbd_transfer(xfer);
	if (err && err != USBD_IN_PROGRESS && sc->next_polled_xfer == NULL) {
		DPRINTF(UDMASS_BBB, ("%s: failed to setup transfer, %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(err)));
		sc->polling_depth--;
		return (err);
	}

	if (err && err != USBD_IN_PROGRESS) {
		DPRINTF(UDMASS_XFER, ("umass_polled_xfer %p has error %s\n",
		    xfer, usbd_errstr(err)));
	}

	if (sc->next_polled_xfer != NULL) {
		DPRINTF(UDMASS_XFER, ("umass_polled_xfer running next "
		    "transaction %p\n", sc->next_polled_xfer));
		xfer = sc->next_polled_xfer;
		sc->next_polled_xfer = NULL;
		goto start_next_xfer;
	}

	sc->polling_depth--;

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
umass_setup_transfer(struct umass_softc *sc, usbd_pipe_handle pipe,
			void *buffer, int buflen, int flags,
			usbd_xfer_handle xfer)
{
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	/* Initialise a USB transfer and then schedule it */

	usbd_setup_xfer(xfer, pipe, (void *)sc, buffer, buflen,
	    flags | sc->sc_xfer_flags, sc->timeout, sc->sc_methods->wire_state);

	if (sc->sc_udev->bus->use_polling) {
		DPRINTF(UDMASS_XFER,("%s: start polled xfer buffer=%p "
		    "buflen=%d flags=0x%x timeout=%d\n", USBDEVNAME(sc->sc_dev),
		    buffer, buflen, flags | sc->sc_xfer_flags, sc->timeout));
		err = umass_polled_transfer(sc, xfer);
	} else {
		err = usbd_transfer(xfer);
		DPRINTF(UDMASS_XFER,("%s: start xfer buffer=%p buflen=%d "
		    "flags=0x%x timeout=%d\n", USBDEVNAME(sc->sc_dev),
		    buffer, buflen, flags | sc->sc_xfer_flags, sc->timeout));
	}
	if (err && err != USBD_IN_PROGRESS) {
		DPRINTF(UDMASS_BBB, ("%s: failed to setup transfer, %s\n",
			USBDEVNAME(sc->sc_dev), usbd_errstr(err)));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}


Static usbd_status
umass_setup_ctrl_transfer(struct umass_softc *sc, usb_device_request_t *req,
	 void *buffer, int buflen, int flags, usbd_xfer_handle xfer)
{
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	/* Initialise a USB control transfer and then schedule it */

	usbd_setup_default_xfer(xfer, sc->sc_udev, (void *) sc,
	    USBD_DEFAULT_TIMEOUT, req, buffer, buflen, flags,
	    sc->sc_methods->wire_state);

	if (sc->sc_udev->bus->use_polling) {
		DPRINTF(UDMASS_XFER,("%s: start polled ctrl xfer buffer=%p "
		    "buflen=%d flags=0x%x\n", USBDEVNAME(sc->sc_dev), buffer,
		    buflen, flags));
		err = umass_polled_transfer(sc, xfer);
	} else {
		DPRINTF(UDMASS_XFER,("%s: start ctrl xfer buffer=%p buflen=%d "
		    "flags=0x%x\n", USBDEVNAME(sc->sc_dev), buffer, buflen,
		    flags));
		err = usbd_transfer(xfer);
	}
	if (err && err != USBD_IN_PROGRESS) {
		DPRINTF(UDMASS_BBB, ("%s: failed to setup ctrl transfer, %s\n",
			 USBDEVNAME(sc->sc_dev), usbd_errstr(err)));

		/* do not reset, as this would make us loop */
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static void
umass_adjust_transfer(struct umass_softc *sc)
{
	switch (sc->sc_cmd) {
	case UMASS_CPROTO_UFI:
		sc->cbw.bCDBLength = UFI_COMMAND_LENGTH; 
		/* Adjust the length field in certain scsi commands. */
		switch (sc->cbw.CBWCDB[0]) {
		case INQUIRY:
			if (sc->transfer_datalen > 36) {
				sc->transfer_datalen = 36;
				sc->cbw.CBWCDB[4] = 36;
			}
			break;
		case MODE_SENSE_BIG:
			if (sc->transfer_datalen > 8) {
				sc->transfer_datalen = 8;
				sc->cbw.CBWCDB[7] = 0;
				sc->cbw.CBWCDB[8] = 8;
			}
			break;
		case REQUEST_SENSE:
			if (sc->transfer_datalen > 18) {
				sc->transfer_datalen = 18;
				sc->cbw.CBWCDB[4] = 18;
			}
			break;
		}
		break;
	case UMASS_CPROTO_ATAPI:
		sc->cbw.bCDBLength = UFI_COMMAND_LENGTH; 
		break;
	}
}

Static void
umass_clear_endpoint_stall(struct umass_softc *sc, int endpt,
	usbd_xfer_handle xfer)
{
	if (sc->sc_dying)
		return;

	DPRINTF(UDMASS_BBB, ("%s: Clear endpoint 0x%02x stall\n",
		USBDEVNAME(sc->sc_dev), sc->sc_epaddr[endpt]));

	usbd_clear_endpoint_toggle(sc->sc_pipe[endpt]);

	sc->sc_req.bmRequestType = UT_WRITE_ENDPOINT;
	sc->sc_req.bRequest = UR_CLEAR_FEATURE;
	USETW(sc->sc_req.wValue, UF_ENDPOINT_HALT);
	USETW(sc->sc_req.wIndex, sc->sc_epaddr[endpt]);
	USETW(sc->sc_req.wLength, 0);
	umass_setup_ctrl_transfer(sc, &sc->sc_req, NULL, 0, 0, xfer);
}

#if 0
Static void
umass_reset(struct umass_softc *sc, transfer_cb_f cb, void *priv)
{
	sc->transfer_cb = cb;
	sc->transfer_priv = priv;

	/* The reset is a forced reset, so no error (yet) */
	sc->reset(sc, STATUS_CMD_OK);
}
#endif

/*
 * Bulk protocol specific functions
 */

Static void
umass_bbb_reset(struct umass_softc *sc, int status)
{
	KASSERT(sc->sc_wire & UMASS_WPROTO_BBB,
		("sc->sc_wire == 0x%02x wrong for umass_bbb_reset\n",
		sc->sc_wire));

	if (sc->sc_dying)
		return;

	/*
	 * Reset recovery (5.3.4 in Universal Serial Bus Mass Storage Class)
	 *
	 * For Reset Recovery the host shall issue in the following order:
	 * a) a Bulk-Only Mass Storage Reset
	 * b) a Clear Feature HALT to the Bulk-In endpoint
	 * c) a Clear Feature HALT to the Bulk-Out endpoint
	 *
	 * This is done in 3 steps, states:
	 * TSTATE_BBB_RESET1
	 * TSTATE_BBB_RESET2
	 * TSTATE_BBB_RESET3
	 *
	 * If the reset doesn't succeed, the device should be port reset.
	 */

	DPRINTF(UDMASS_BBB, ("%s: Bulk Reset\n",
		USBDEVNAME(sc->sc_dev)));

	sc->transfer_state = TSTATE_BBB_RESET1;
	sc->transfer_status = status;

	/* reset is a class specific interface write */
	sc->sc_req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	sc->sc_req.bRequest = UR_BBB_RESET;
	USETW(sc->sc_req.wValue, 0);
	USETW(sc->sc_req.wIndex, sc->sc_ifaceno);
	USETW(sc->sc_req.wLength, 0);
	umass_setup_ctrl_transfer(sc, &sc->sc_req, NULL, 0, 0,
				  sc->transfer_xfer[XFER_BBB_RESET1]);
}

Static void
umass_bbb_transfer(struct umass_softc *sc, int lun, void *cmd, int cmdlen,
		   void *data, int datalen, int dir, u_int timeout,
		   umass_callback cb, void *priv)
{
	static int dCBWtag = 42;	/* unique for CBW of transfer */
	usbd_status err;

	DPRINTF(UDMASS_BBB,("%s: umass_bbb_transfer cmd=0x%02x\n",
		USBDEVNAME(sc->sc_dev), *(u_char *)cmd));

	KASSERT(sc->sc_wire & UMASS_WPROTO_BBB,
		("sc->sc_wire == 0x%02x wrong for umass_bbb_transfer\n",
		sc->sc_wire));

	if (sc->sc_dying) {
		sc->polled_xfer_status = USBD_IOERROR;
		return;
	}

	/* Be a little generous. */
	sc->timeout = timeout + USBD_DEFAULT_TIMEOUT;

	/*
	 * Do a Bulk-Only transfer with cmdlen bytes from cmd, possibly
	 * a data phase of datalen bytes from/to the device and finally a
	 * csw read phase.
	 * If the data direction was inbound a maximum of datalen bytes
	 * is stored in the buffer pointed to by data.
	 *
	 * umass_bbb_transfer initialises the transfer and lets the state
	 * machine in umass_bbb_state handle the completion. It uses the
	 * following states:
	 * TSTATE_BBB_COMMAND
	 *   -> TSTATE_BBB_DATA
	 *   -> TSTATE_BBB_STATUS
	 *   -> TSTATE_BBB_STATUS2
	 *   -> TSTATE_BBB_IDLE
	 *
	 * An error in any of those states will invoke
	 * umass_bbb_reset.
	 */

	/* check the given arguments */
	KASSERT(datalen == 0 || data != NULL,
		("%s: datalen > 0, but no buffer",USBDEVNAME(sc->sc_dev)));
	KASSERT(cmdlen <= CBWCDBLENGTH,
		("%s: cmdlen exceeds CDB length in CBW (%d > %d)",
			USBDEVNAME(sc->sc_dev), cmdlen, CBWCDBLENGTH));
	KASSERT(dir == DIR_NONE || datalen > 0,
		("%s: datalen == 0 while direction is not NONE\n",
			USBDEVNAME(sc->sc_dev)));
	KASSERT(datalen == 0 || dir != DIR_NONE,
		("%s: direction is NONE while datalen is not zero\n",
			USBDEVNAME(sc->sc_dev)));
	KASSERT(sizeof(umass_bbb_cbw_t) == UMASS_BBB_CBW_SIZE,
		("%s: CBW struct does not have the right size (%d vs. %d)\n",
			USBDEVNAME(sc->sc_dev),
			sizeof(umass_bbb_cbw_t), UMASS_BBB_CBW_SIZE));
	KASSERT(sizeof(umass_bbb_csw_t) == UMASS_BBB_CSW_SIZE,
		("%s: CSW struct does not have the right size (%d vs. %d)\n",
			USBDEVNAME(sc->sc_dev),
			sizeof(umass_bbb_csw_t), UMASS_BBB_CSW_SIZE));

	/*
	 * Determine the direction of the data transfer and the length.
	 *
	 * dCBWDataTransferLength (datalen) :
	 *   This field indicates the number of bytes of data that the host
	 *   intends to transfer on the IN or OUT Bulk endpoint(as indicated by
	 *   the Direction bit) during the execution of this command. If this
	 *   field is set to 0, the device will expect that no data will be
	 *   transferred IN or OUT during this command, regardless of the value
	 *   of the Direction bit defined in dCBWFlags.
	 *
	 * dCBWFlags (dir) :
	 *   The bits of the Flags field are defined as follows:
	 *     Bits 0-6	 reserved
	 *     Bit  7	 Direction - this bit shall be ignored if the
	 *			     dCBWDataTransferLength field is zero.
	 *		 0 = data Out from host to device
	 *		 1 = data In from device to host
	 */

	/* Fill in the Command Block Wrapper */
	USETDW(sc->cbw.dCBWSignature, CBWSIGNATURE);
	USETDW(sc->cbw.dCBWTag, dCBWtag);
	dCBWtag++;	/* cannot be done in macro (it will be done 4 times) */
	USETDW(sc->cbw.dCBWDataTransferLength, datalen);
	/* DIR_NONE is treated as DIR_OUT (0x00) */
	sc->cbw.bCBWFlags = (dir == DIR_IN? CBWFLAGS_IN:CBWFLAGS_OUT);
	sc->cbw.bCBWLUN = lun;
	sc->cbw.bCDBLength = cmdlen;
	bzero(sc->cbw.CBWCDB, sizeof(sc->cbw.CBWCDB));
	memcpy(sc->cbw.CBWCDB, cmd, cmdlen);

	DIF(UDMASS_BBB, umass_bbb_dump_cbw(sc, &sc->cbw));

	/* store the details for the data transfer phase */
	sc->transfer_dir = dir;
	sc->transfer_data = data;
	sc->transfer_datalen = datalen;
	sc->transfer_actlen = 0;
	sc->transfer_cb = cb;
	sc->transfer_priv = priv;
	sc->transfer_status = STATUS_CMD_OK;

	/* move from idle to the command state */
	sc->transfer_state = TSTATE_BBB_COMMAND;

	/* Send the CBW from host to device via bulk-out endpoint. */
	umass_adjust_transfer(sc);
	if ((err = umass_setup_transfer(sc, sc->sc_pipe[UMASS_BULKOUT],
			&sc->cbw, UMASS_BBB_CBW_SIZE, 0,
			sc->transfer_xfer[XFER_BBB_CBW])))
		umass_bbb_reset(sc, STATUS_WIRE_FAILED);

	if (sc->sc_udev->bus->use_polling)
		sc->polled_xfer_status = err;
}

Static void
umass_bbb_state(usbd_xfer_handle xfer, usbd_private_handle priv,
		usbd_status err)
{
	struct umass_softc *sc = (struct umass_softc *) priv;
	usbd_xfer_handle next_xfer;

	KASSERT(sc->sc_wire & UMASS_WPROTO_BBB,
		("sc->sc_wire == 0x%02x wrong for umass_bbb_state\n",
		sc->sc_wire));

	if (sc->sc_dying)
		return;

	/*
	 * State handling for BBB transfers.
	 *
	 * The subroutine is rather long. It steps through the states given in
	 * Annex A of the Bulk-Only specification.
	 * Each state first does the error handling of the previous transfer
	 * and then prepares the next transfer.
	 * Each transfer is done asynchronously so after the request/transfer
	 * has been submitted you will find a 'return;'.
	 */

	DPRINTF(UDMASS_BBB, ("%s: Handling BBB state %d (%s), xfer=%p, %s\n",
		USBDEVNAME(sc->sc_dev), sc->transfer_state,
		states[sc->transfer_state], xfer, usbd_errstr(err)));

	switch (sc->transfer_state) {

	/***** Bulk Transfer *****/
	case TSTATE_BBB_COMMAND:
		/* Command transport phase, error handling */
		if (err) {
			DPRINTF(UDMASS_BBB, ("%s: failed to send CBW\n",
				USBDEVNAME(sc->sc_dev)));
			/* If the device detects that the CBW is invalid, then
			 * the device may STALL both bulk endpoints and require
			 * a Bulk-Reset
			 */
			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		}

		/* Data transport phase, setup transfer */
		sc->transfer_state = TSTATE_BBB_DATA;
		if (sc->transfer_dir == DIR_IN) {
			if (umass_setup_transfer(sc, sc->sc_pipe[UMASS_BULKIN],
					sc->data_buffer, sc->transfer_datalen,
					USBD_SHORT_XFER_OK | USBD_NO_COPY,
					sc->transfer_xfer[XFER_BBB_DATA]))
				umass_bbb_reset(sc, STATUS_WIRE_FAILED);

			return;
		} else if (sc->transfer_dir == DIR_OUT) {
			memcpy(sc->data_buffer, sc->transfer_data,
			       sc->transfer_datalen);
			if (umass_setup_transfer(sc, sc->sc_pipe[UMASS_BULKOUT],
					sc->data_buffer, sc->transfer_datalen,
					USBD_NO_COPY,/* fixed length transfer */
					sc->transfer_xfer[XFER_BBB_DATA]))
				umass_bbb_reset(sc, STATUS_WIRE_FAILED);

			return;
		} else {
			DPRINTF(UDMASS_BBB, ("%s: no data phase\n",
				USBDEVNAME(sc->sc_dev)));
		}

		/* FALLTHROUGH if no data phase, err == 0 */
	case TSTATE_BBB_DATA:
		/* Command transport phase error handling (ignored if no data
		 * phase (fallthrough from previous state)) */
		if (sc->transfer_dir != DIR_NONE) {
			/* retrieve the length of the transfer that was done */
			usbd_get_xfer_status(xfer, NULL, NULL,
			     &sc->transfer_actlen, NULL);
			DPRINTF(UDMASS_BBB, ("%s: BBB_DATA actlen=%d\n",
				USBDEVNAME(sc->sc_dev), sc->transfer_actlen));

			if (err) {
				DPRINTF(UDMASS_BBB, ("%s: Data-%s %d failed, "
					"%s\n", USBDEVNAME(sc->sc_dev),
					(sc->transfer_dir == DIR_IN?"in":"out"),
					sc->transfer_datalen,usbd_errstr(err)));

				if (err == USBD_STALLED) {
					sc->transfer_state = TSTATE_BBB_DCLEAR;
					umass_clear_endpoint_stall(sc,
					  (sc->transfer_dir == DIR_IN?
					    UMASS_BULKIN:UMASS_BULKOUT),
					  sc->transfer_xfer[XFER_BBB_DCLEAR]);
				} else {
					/* Unless the error is a pipe stall the
					 * error is fatal.
					 */
					umass_bbb_reset(sc,STATUS_WIRE_FAILED);
				}
				return;
			}
		}

		/* FALLTHROUGH, err == 0 (no data phase or successful) */
	case TSTATE_BBB_DCLEAR: /* stall clear after data phase */
		if (sc->transfer_dir == DIR_IN)
			memcpy(sc->transfer_data, sc->data_buffer,
			       sc->transfer_actlen);

		DIF(UDMASS_BBB, if (sc->transfer_dir == DIR_IN)
					umass_dump_buffer(sc, sc->transfer_data,
						sc->transfer_datalen, 48));

		/* FALLTHROUGH, err == 0 (no data phase or successful) */
	case TSTATE_BBB_SCLEAR: /* stall clear after status phase */
		/* Reading of CSW after bulk stall condition in data phase
		 * (TSTATE_BBB_DATA2) or bulk-in stall condition after
		 * reading CSW (TSTATE_BBB_SCLEAR).
		 * In the case of no data phase or successful data phase,
		 * err == 0 and the following if block is passed.
		 */
		if (err) {	/* should not occur */
			printf("%s: BBB bulk-%s stall clear failed, %s\n",
			    USBDEVNAME(sc->sc_dev),
			    (sc->transfer_dir == DIR_IN? "in":"out"),
			    usbd_errstr(err));
			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		}

		/* Status transport phase, setup transfer */
		if (sc->transfer_state == TSTATE_BBB_COMMAND ||
		    sc->transfer_state == TSTATE_BBB_DATA ||
		    sc->transfer_state == TSTATE_BBB_DCLEAR) {
			/* After no data phase, successful data phase and
			 * after clearing bulk-in/-out stall condition
			 */
			sc->transfer_state = TSTATE_BBB_STATUS1;
			next_xfer = sc->transfer_xfer[XFER_BBB_CSW1];
		} else {
			/* After first attempt of fetching CSW */
			sc->transfer_state = TSTATE_BBB_STATUS2;
			next_xfer = sc->transfer_xfer[XFER_BBB_CSW2];
		}

		/* Read the Command Status Wrapper via bulk-in endpoint. */
		if (umass_setup_transfer(sc, sc->sc_pipe[UMASS_BULKIN],
			&sc->csw, UMASS_BBB_CSW_SIZE, 0, next_xfer)) {
			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		}

		return;
	case TSTATE_BBB_STATUS1:	/* first attempt */
	case TSTATE_BBB_STATUS2:	/* second attempt */
		/* Status transfer, error handling */
		if (err) {
			DPRINTF(UDMASS_BBB, ("%s: Failed to read CSW, %s%s\n",
				USBDEVNAME(sc->sc_dev), usbd_errstr(err),
				(sc->transfer_state == TSTATE_BBB_STATUS1?
					", retrying":"")));

			/* If this was the first attempt at fetching the CSW
			 * retry it, otherwise fail.
			 */
			if (sc->transfer_state == TSTATE_BBB_STATUS1) {
				sc->transfer_state = TSTATE_BBB_SCLEAR;
				umass_clear_endpoint_stall(sc, UMASS_BULKIN,
				    sc->transfer_xfer[XFER_BBB_SCLEAR]);
				return;
			} else {
				umass_bbb_reset(sc, STATUS_WIRE_FAILED);
				return;
			}
		}

		DIF(UDMASS_BBB, umass_bbb_dump_csw(sc, &sc->csw));

		/* Translate weird command-status signatures. */
		if ((sc->sc_quirks & UMASS_QUIRK_WRONG_CSWSIG) &&
		    UGETDW(sc->csw.dCSWSignature) == CSWSIGNATURE_OLYMPUS_C1)
			USETDW(sc->csw.dCSWSignature, CSWSIGNATURE);

		/* Translate invalid command-status tags */
		if (sc->sc_quirks & UMASS_QUIRK_WRONG_CSWTAG)
			USETDW(sc->csw.dCSWTag, UGETDW(sc->cbw.dCBWTag));

		/* Check CSW and handle any error */
		if (UGETDW(sc->csw.dCSWSignature) != CSWSIGNATURE) {
			/* Invalid CSW: Wrong signature or wrong tag might
			 * indicate that the device is confused -> reset it.
			 */
			printf("%s: Invalid CSW: sig 0x%08x should be 0x%08x\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(sc->csw.dCSWSignature),
				CSWSIGNATURE);

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		} else if (UGETDW(sc->csw.dCSWTag)
				!= UGETDW(sc->cbw.dCBWTag)) {
			printf("%s: Invalid CSW: tag %d should be %d\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(sc->csw.dCSWTag),
				UGETDW(sc->cbw.dCBWTag));

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;

		/* CSW is valid here */
		} else if (sc->csw.bCSWStatus > CSWSTATUS_PHASE) {
			printf("%s: Invalid CSW: status %d > %d\n",
				USBDEVNAME(sc->sc_dev),
				sc->csw.bCSWStatus,
				CSWSTATUS_PHASE);

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
		} else if (sc->csw.bCSWStatus == CSWSTATUS_PHASE) {
			printf("%s: Phase Error, residue = %d\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(sc->csw.dCSWDataResidue));

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;

		} else if (sc->transfer_actlen > sc->transfer_datalen) {
			/* Buffer overrun! Don't let this go by unnoticed */
			panic("%s: transferred %d bytes instead of %d bytes",
				USBDEVNAME(sc->sc_dev),
				sc->transfer_actlen, sc->transfer_datalen);
#if 0
		} else if (sc->transfer_datalen - sc->transfer_actlen
			   != UGETDW(sc->csw.dCSWDataResidue)) {
			DPRINTF(UDMASS_BBB, ("%s: actlen=%d != residue=%d\n",
				USBDEVNAME(sc->sc_dev),
				sc->transfer_datalen - sc->transfer_actlen,
				UGETDW(sc->csw.dCSWDataResidue)));

			umass_bbb_reset(sc, STATUS_WIRE_FAILED);
			return;
#endif
		} else if (sc->csw.bCSWStatus == CSWSTATUS_FAILED) {
			DPRINTF(UDMASS_BBB, ("%s: Command Failed, res = %d\n",
				USBDEVNAME(sc->sc_dev),
				UGETDW(sc->csw.dCSWDataResidue)));

			/* SCSI command failed but transfer was successful */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
					UGETDW(sc->csw.dCSWDataResidue),
					STATUS_CMD_FAILED);

			return;

		} else {	/* success */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
					UGETDW(sc->csw.dCSWDataResidue),
					STATUS_CMD_OK);

			return;
		}

	/***** Bulk Reset *****/
	case TSTATE_BBB_RESET1:
		if (err)
			printf("%s: BBB reset failed, %s\n",
				USBDEVNAME(sc->sc_dev), usbd_errstr(err));

		sc->transfer_state = TSTATE_BBB_RESET2;
		umass_clear_endpoint_stall(sc, UMASS_BULKIN,
			sc->transfer_xfer[XFER_BBB_RESET2]);

		return;
	case TSTATE_BBB_RESET2:
		if (err)	/* should not occur */
			printf("%s: BBB bulk-in clear stall failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		sc->transfer_state = TSTATE_BBB_RESET3;
		umass_clear_endpoint_stall(sc, UMASS_BULKOUT,
			sc->transfer_xfer[XFER_BBB_RESET3]);

		return;
	case TSTATE_BBB_RESET3:
		if (err)	/* should not occur */
			printf("%s: BBB bulk-out clear stall failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		sc->transfer_state = TSTATE_IDLE;
		if (sc->transfer_priv) {
			sc->transfer_cb(sc, sc->transfer_priv,
					sc->transfer_datalen,
					sc->transfer_status);
		}

		return;

	/***** Default *****/
	default:
		panic("%s: Unknown state %d",
		      USBDEVNAME(sc->sc_dev), sc->transfer_state);
	}
}

/*
 * Command/Bulk/Interrupt (CBI) specific functions
 */

Static int
umass_cbi_adsc(struct umass_softc *sc, char *buffer, int buflen,
	       usbd_xfer_handle xfer)
{
	KASSERT(sc->sc_wire & (UMASS_WPROTO_CBI|UMASS_WPROTO_CBI_I),
		("sc->sc_wire == 0x%02x wrong for umass_cbi_adsc\n",
		sc->sc_wire));

	sc->sc_req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	sc->sc_req.bRequest = UR_CBI_ADSC;
	USETW(sc->sc_req.wValue, 0);
	USETW(sc->sc_req.wIndex, sc->sc_ifaceno);
	USETW(sc->sc_req.wLength, buflen);
	return umass_setup_ctrl_transfer(sc, &sc->sc_req, buffer,
					 buflen, 0, xfer);
}


Static void
umass_cbi_reset(struct umass_softc *sc, int status)
{
	int i;
#	define SEND_DIAGNOSTIC_CMDLEN	12

	KASSERT(sc->sc_wire & (UMASS_WPROTO_CBI|UMASS_WPROTO_CBI_I),
		("sc->sc_wire == 0x%02x wrong for umass_cbi_reset\n",
		sc->sc_wire));

	if (sc->sc_dying)
		return;

	/*
	 * Command Block Reset Protocol
	 *
	 * First send a reset request to the device. Then clear
	 * any possibly stalled bulk endpoints.

	 * This is done in 3 steps, states:
	 * TSTATE_CBI_RESET1
	 * TSTATE_CBI_RESET2
	 * TSTATE_CBI_RESET3
	 *
	 * If the reset doesn't succeed, the device should be port reset.
	 */

	DPRINTF(UDMASS_CBI, ("%s: CBI Reset\n",
		USBDEVNAME(sc->sc_dev)));

	KASSERT(sizeof(sc->cbl) >= SEND_DIAGNOSTIC_CMDLEN,
		("%s: CBL struct is too small (%d < %d)\n",
			USBDEVNAME(sc->sc_dev),
			sizeof(sc->cbl), SEND_DIAGNOSTIC_CMDLEN));

	sc->transfer_state = TSTATE_CBI_RESET1;
	sc->transfer_status = status;

	/* The 0x1d code is the SEND DIAGNOSTIC command. To distinguish between
	 * the two the last 10 bytes of the cbl is filled with 0xff (section
	 * 2.2 of the CBI spec).
	 */
	sc->cbl[0] = 0x1d;	/* Command Block Reset */
	sc->cbl[1] = 0x04;
	for (i = 2; i < SEND_DIAGNOSTIC_CMDLEN; i++)
		sc->cbl[i] = 0xff;

	umass_cbi_adsc(sc, sc->cbl, SEND_DIAGNOSTIC_CMDLEN,
		       sc->transfer_xfer[XFER_CBI_RESET1]);
	/* XXX if the command fails we should reset the port on the bub */
}

Static void
umass_cbi_transfer(struct umass_softc *sc, int lun,
		   void *cmd, int cmdlen, void *data, int datalen, int dir,
		   u_int timeout, umass_callback cb, void *priv)
{
	usbd_status err;

	DPRINTF(UDMASS_CBI,("%s: umass_cbi_transfer cmd=0x%02x, len=%d\n",
		USBDEVNAME(sc->sc_dev), *(u_char *)cmd, datalen));

	KASSERT(sc->sc_wire & (UMASS_WPROTO_CBI|UMASS_WPROTO_CBI_I),
		("sc->sc_wire == 0x%02x wrong for umass_cbi_transfer\n",
		sc->sc_wire));

	if (sc->sc_dying) {
		sc->polled_xfer_status = USBD_IOERROR;
		return;
	}

	/* Be a little generous. */
	sc->timeout = timeout + USBD_DEFAULT_TIMEOUT;

	/*
	 * Do a CBI transfer with cmdlen bytes from cmd, possibly
	 * a data phase of datalen bytes from/to the device and finally a
	 * csw read phase.
	 * If the data direction was inbound a maximum of datalen bytes
	 * is stored in the buffer pointed to by data.
	 *
	 * umass_cbi_transfer initialises the transfer and lets the state
	 * machine in umass_cbi_state handle the completion. It uses the
	 * following states:
	 * TSTATE_CBI_COMMAND
	 *   -> XXX fill in
	 *
	 * An error in any of those states will invoke
	 * umass_cbi_reset.
	 */

	/* check the given arguments */
	KASSERT(datalen == 0 || data != NULL,
		("%s: datalen > 0, but no buffer",USBDEVNAME(sc->sc_dev)));
	KASSERT(datalen == 0 || dir != DIR_NONE,
		("%s: direction is NONE while datalen is not zero\n",
			USBDEVNAME(sc->sc_dev)));

	/* store the details for the data transfer phase */
	sc->transfer_dir = dir;
	sc->transfer_data = data;
	sc->transfer_datalen = datalen;
	sc->transfer_actlen = 0;
	sc->transfer_cb = cb;
	sc->transfer_priv = priv;
	sc->transfer_status = STATUS_CMD_OK;

	/* move from idle to the command state */
	sc->transfer_state = TSTATE_CBI_COMMAND;

	/* Send the Command Block from host to device via control endpoint. */
	sc->cbw.bCDBLength = cmdlen;
	bzero(sc->cbw.CBWCDB, sizeof(sc->cbw.CBWCDB));
	memcpy(sc->cbw.CBWCDB, cmd, cmdlen);
	umass_adjust_transfer(sc);
	if ((err = umass_cbi_adsc(sc, (void *)sc->cbw.CBWCDB, sc->cbw.bCDBLength,
	    sc->transfer_xfer[XFER_CBI_CB])))
		umass_cbi_reset(sc, STATUS_WIRE_FAILED);

	if (sc->sc_udev->bus->use_polling)
		sc->polled_xfer_status = err;
}

Static void
umass_cbi_state(usbd_xfer_handle xfer, usbd_private_handle priv,
		usbd_status err)
{
	struct umass_softc *sc = (struct umass_softc *) priv;

	KASSERT(sc->sc_wire & (UMASS_WPROTO_CBI|UMASS_WPROTO_CBI_I),
		("sc->sc_wire == 0x%02x wrong for umass_cbi_state\n",
		sc->sc_wire));

	if (sc->sc_dying)
		return;

	/*
	 * State handling for CBI transfers.
	 */

	DPRINTF(UDMASS_CBI, ("%s: Handling CBI state %d (%s), xfer=%p, %s\n",
		USBDEVNAME(sc->sc_dev), sc->transfer_state,
		states[sc->transfer_state], xfer, usbd_errstr(err)));

	switch (sc->transfer_state) {

	/***** CBI Transfer *****/
	case TSTATE_CBI_COMMAND:
		if (err == USBD_STALLED) {
			DPRINTF(UDMASS_CBI, ("%s: Command Transport failed\n",
				USBDEVNAME(sc->sc_dev)));
			/* Status transport by control pipe (section 2.3.2.1).
			 * The command contained in the command block failed.
			 *
			 * The control pipe has already been unstalled by the
			 * USB stack.
			 * Section 2.4.3.1.1 states that the bulk in endpoints
			 * should not stalled at this point.
			 */

			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
					sc->transfer_datalen,
					STATUS_CMD_FAILED);

			return;
		} else if (err) {
			DPRINTF(UDMASS_CBI, ("%s: failed to send ADSC\n",
				USBDEVNAME(sc->sc_dev)));
			umass_cbi_reset(sc, STATUS_WIRE_FAILED);
			return;
		}

		/* Data transport phase, setup transfer */
		sc->transfer_state = TSTATE_CBI_DATA;
		if (sc->transfer_dir == DIR_IN) {
			if (umass_setup_transfer(sc, sc->sc_pipe[UMASS_BULKIN],
					sc->data_buffer, sc->transfer_datalen,
					USBD_SHORT_XFER_OK | USBD_NO_COPY,
					sc->transfer_xfer[XFER_CBI_DATA]))
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);

			return;
		} else if (sc->transfer_dir == DIR_OUT) {
			memcpy(sc->data_buffer, sc->transfer_data,
			       sc->transfer_datalen);
			if (umass_setup_transfer(sc, sc->sc_pipe[UMASS_BULKOUT],
					sc->data_buffer, sc->transfer_datalen,
					USBD_NO_COPY,/* fixed length transfer */
					sc->transfer_xfer[XFER_CBI_DATA]))
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);

			return;
		} else {
			DPRINTF(UDMASS_CBI, ("%s: no data phase\n",
				USBDEVNAME(sc->sc_dev)));
		}

		/* FALLTHROUGH if no data phase, err == 0 */
	case TSTATE_CBI_DATA:
		/* Command transport phase error handling (ignored if no data
		 * phase (fallthrough from previous state)) */
		if (sc->transfer_dir != DIR_NONE) {
			/* retrieve the length of the transfer that was done */
			usbd_get_xfer_status(xfer, NULL, NULL,
			    &sc->transfer_actlen, NULL);
			DPRINTF(UDMASS_CBI, ("%s: CBI_DATA actlen=%d\n",
				USBDEVNAME(sc->sc_dev), sc->transfer_actlen));

			if (err) {
				DPRINTF(UDMASS_CBI, ("%s: Data-%s %d failed, "
					"%s\n", USBDEVNAME(sc->sc_dev),
					(sc->transfer_dir == DIR_IN?"in":"out"),
					sc->transfer_datalen,usbd_errstr(err)));

				if (err == USBD_STALLED) {
					sc->transfer_state = TSTATE_CBI_DCLEAR;
					umass_clear_endpoint_stall(sc,
					  (sc->transfer_dir == DIR_IN?
					    UMASS_BULKIN:UMASS_BULKOUT),
					sc->transfer_xfer[XFER_CBI_DCLEAR]);
				} else {
					/* Unless the error is a pipe stall the
					 * error is fatal.
					 */
					umass_cbi_reset(sc, STATUS_WIRE_FAILED);
				}
				return;
			}
		}

		if (sc->transfer_dir == DIR_IN)
			memcpy(sc->transfer_data, sc->data_buffer,
			       sc->transfer_actlen);

		DIF(UDMASS_CBI, if (sc->transfer_dir == DIR_IN)
					umass_dump_buffer(sc, sc->transfer_data,
						sc->transfer_actlen, 48));

		/* Status phase */
		if (sc->sc_wire == UMASS_WPROTO_CBI_I) {
			sc->transfer_state = TSTATE_CBI_STATUS;
			memset(&sc->sbl, 0, sizeof(sc->sbl));
			if (umass_setup_transfer(sc, sc->sc_pipe[UMASS_INTRIN],
				    &sc->sbl, sizeof(sc->sbl),
				    0,	/* fixed length transfer */
				    sc->transfer_xfer[XFER_CBI_STATUS]))
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);
		} else {
			/* No command completion interrupt. Request
			 * sense to get status of command.
			 */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
				sc->transfer_datalen - sc->transfer_actlen,
				STATUS_CMD_UNKNOWN);
		}
		return;

	case TSTATE_CBI_STATUS:
		if (err) {
			DPRINTF(UDMASS_CBI, ("%s: Status Transport failed\n",
				USBDEVNAME(sc->sc_dev)));
			/* Status transport by interrupt pipe (section 2.3.2.2).
			 */

			if (err == USBD_STALLED) {
				sc->transfer_state = TSTATE_CBI_SCLEAR;
				umass_clear_endpoint_stall(sc, UMASS_INTRIN,
					sc->transfer_xfer[XFER_CBI_SCLEAR]);
			} else {
				umass_cbi_reset(sc, STATUS_WIRE_FAILED);
			}
			return;
		}

		/* Dissect the information in the buffer */

		{
			u_int32_t actlen;
			usbd_get_xfer_status(xfer, NULL, NULL, &actlen, NULL);
			DPRINTF(UDMASS_CBI, ("%s: CBI_STATUS actlen=%d\n",
			    USBDEVNAME(sc->sc_dev), actlen));
			if (actlen != 2)
				break;
		}

		if (sc->sc_cmd == UMASS_CPROTO_UFI) {
			int status;

			/* Section 3.4.3.1.3 specifies that the UFI command
			 * protocol returns an ASC and ASCQ in the interrupt
			 * data block.
			 */

			DPRINTF(UDMASS_CBI, ("%s: UFI CCI, ASC = 0x%02x, "
				"ASCQ = 0x%02x\n",
				USBDEVNAME(sc->sc_dev),
				sc->sbl.ufi.asc, sc->sbl.ufi.ascq));

			if ((sc->sbl.ufi.asc == 0 && sc->sbl.ufi.ascq == 0) ||
			    sc->sc_sense)
				status = STATUS_CMD_OK;
			else
				status = STATUS_CMD_FAILED;

			/* No autosense, command successful */
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
			    sc->transfer_datalen - sc->transfer_actlen, status);
		} else {
			int status;

			/* Command Interrupt Data Block */

			DPRINTF(UDMASS_CBI, ("%s: type=0x%02x, value=0x%02x\n",
				USBDEVNAME(sc->sc_dev),
				sc->sbl.common.type, sc->sbl.common.value));

			if (sc->sbl.common.type == IDB_TYPE_CCI) {
				switch (sc->sbl.common.value &
				    IDB_VALUE_STATUS_MASK) {
				case IDB_VALUE_PASS:
					status = STATUS_CMD_OK;
					break;
				case IDB_VALUE_FAIL:
				case IDB_VALUE_PERSISTENT:
					status = STATUS_CMD_FAILED;
					break;
				case IDB_VALUE_PHASE:
					status = STATUS_WIRE_FAILED;
					break;
 				}

				sc->transfer_state = TSTATE_IDLE;
				sc->transfer_cb(sc, sc->transfer_priv,
				    sc->transfer_datalen - sc->transfer_actlen,
				    status);
			}
		}
		return;

	case TSTATE_CBI_DCLEAR:
		if (err) {	/* should not occur */
			printf("%s: CBI bulk-in/out stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			umass_cbi_reset(sc, STATUS_WIRE_FAILED);
		} else {
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
			    sc->transfer_datalen, STATUS_CMD_FAILED);
		}
		return;

	case TSTATE_CBI_SCLEAR:
		if (err) {	/* should not occur */
			printf("%s: CBI intr-in stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			umass_cbi_reset(sc, STATUS_WIRE_FAILED);
		} else {
			sc->transfer_state = TSTATE_IDLE;
			sc->transfer_cb(sc, sc->transfer_priv,
			    sc->transfer_datalen, STATUS_CMD_FAILED);
		}
		return;

	/***** CBI Reset *****/
	case TSTATE_CBI_RESET1:
		if (err)
			printf("%s: CBI reset failed, %s\n",
				USBDEVNAME(sc->sc_dev), usbd_errstr(err));

		sc->transfer_state = TSTATE_CBI_RESET2;
		umass_clear_endpoint_stall(sc, UMASS_BULKIN,
			sc->transfer_xfer[XFER_CBI_RESET2]);

		return;
	case TSTATE_CBI_RESET2:
		if (err)	/* should not occur */
			printf("%s: CBI bulk-in stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		sc->transfer_state = TSTATE_CBI_RESET3;
		umass_clear_endpoint_stall(sc, UMASS_BULKOUT,
			sc->transfer_xfer[XFER_CBI_RESET3]);

		return;
	case TSTATE_CBI_RESET3:
		if (err)	/* should not occur */
			printf("%s: CBI bulk-out stall clear failed, %s\n",
			       USBDEVNAME(sc->sc_dev), usbd_errstr(err));
			/* no error recovery, otherwise we end up in a loop */

		sc->transfer_state = TSTATE_IDLE;
		if (sc->transfer_priv) {
			sc->transfer_cb(sc, sc->transfer_priv,
					sc->transfer_datalen,
					sc->transfer_status);
		}

		return;


	/***** Default *****/
	default:
		panic("%s: Unknown state %d",
		      USBDEVNAME(sc->sc_dev), sc->transfer_state);
	}
}

usbd_status
umass_bbb_get_max_lun(struct umass_softc *sc, u_int8_t *maxlun)
{
	usb_device_request_t req;
	usbd_status err;

	*maxlun = 0;		/* Default to 0. */

	DPRINTF(UDMASS_BBB, ("%s: Get Max Lun\n", USBDEVNAME(sc->sc_dev)));

	/* The Get Max Lun command is a class-specific request. */
	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_BBB_GET_MAX_LUN;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, 1);

	err = usbd_do_request_flags(sc->sc_udev, &req, maxlun,
	    USBD_SHORT_XFER_OK, 0, USBD_DEFAULT_TIMEOUT);
	switch (err) {
	case USBD_NORMAL_COMPLETION:
		DPRINTF(UDMASS_BBB, ("%s: Max Lun %d\n",
		    USBDEVNAME(sc->sc_dev), *maxlun));
		break;

	case USBD_STALLED:
		/*
		 * Device doesn't support Get Max Lun request.
		 */
		err = USBD_NORMAL_COMPLETION;
		DPRINTF(UDMASS_BBB, ("%s: Get Max Lun not supported\n",
		    USBDEVNAME(sc->sc_dev)));
		break;

	case USBD_SHORT_XFER:
		/*
		 * XXX This must mean Get Max Lun is not supported, too!
		 */
		err = USBD_NORMAL_COMPLETION;
		DPRINTF(UDMASS_BBB, ("%s: Get Max Lun SHORT_XFER\n",
		    USBDEVNAME(sc->sc_dev)));
		break;

	default:
		printf("%s: Get Max Lun failed: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(err));
		/* XXX Should we port_reset the device? */
		break;
	}

	return (err);
}




#ifdef UMASS_DEBUG
Static void
umass_bbb_dump_cbw(struct umass_softc *sc, umass_bbb_cbw_t *cbw)
{
	int clen = cbw->bCDBLength;
	int dlen = UGETDW(cbw->dCBWDataTransferLength);
	u_int8_t *c = cbw->CBWCDB;
	int tag = UGETDW(cbw->dCBWTag);
	int flags = cbw->bCBWFlags;

	DPRINTF(UDMASS_BBB, ("%s: CBW %d: cmdlen=%d "
		"(0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%s), "
		"data = %d bytes, dir = %s\n",
		USBDEVNAME(sc->sc_dev), tag, clen,
		c[0], c[1], c[2], c[3], c[4], c[5],
		c[6], c[7], c[8], c[9],
		(clen > 10? "...":""),
		dlen, (flags == CBWFLAGS_IN? "in":
		       (flags == CBWFLAGS_OUT? "out":"<invalid>"))));
}

Static void
umass_bbb_dump_csw(struct umass_softc *sc, umass_bbb_csw_t *csw)
{
	int sig = UGETDW(csw->dCSWSignature);
	int tag = UGETDW(csw->dCSWTag);
	int res = UGETDW(csw->dCSWDataResidue);
	int status = csw->bCSWStatus;

	DPRINTF(UDMASS_BBB, ("%s: CSW %d: sig = 0x%08x (%s), tag = %d, "
		"res = %d, status = 0x%02x (%s)\n", USBDEVNAME(sc->sc_dev),
		tag, sig, (sig == CSWSIGNATURE?	 "valid":"invalid"),
		tag, res,
		status, (status == CSWSTATUS_GOOD? "good":
			 (status == CSWSTATUS_FAILED? "failed":
			  (status == CSWSTATUS_PHASE? "phase":"<invalid>")))));
}

Static void
umass_dump_buffer(struct umass_softc *sc, u_int8_t *buffer, int buflen,
		  int printlen)
{
	int i, j;
	char s1[40];
	char s2[40];
	char s3[5];

	s1[0] = '\0';
	s3[0] = '\0';

	snprintf(s2, sizeof s2, " buffer=%p, buflen=%d", buffer, buflen);
	for (i = 0; i < buflen && i < printlen; i++) {
		j = i % 16;
		if (j == 0 && i != 0) {
			DPRINTF(UDMASS_GEN, ("%s: 0x %s%s\n",
				USBDEVNAME(sc->sc_dev), s1, s2));
			s2[0] = '\0';
		}
		snprintf(&s1[j*2], sizeof s1 - j*2, "%02x", buffer[i] & 0xff);
	}
	if (buflen > printlen)
		snprintf(s3, sizeof s3, " ...");
	DPRINTF(UDMASS_GEN, ("%s: 0x %s%s%s\n",
		USBDEVNAME(sc->sc_dev), s1, s2, s3));
}
#endif
