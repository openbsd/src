/*	$OpenBSD: uhidev.c,v 1.67 2015/01/09 12:07:50 mpi Exp $	*/
/*	$NetBSD: uhidev.c,v 1.14 2003/03/11 16:44:00 augustss Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
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
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/signalvar.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/conf.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/hid.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/uhidev.h>

#ifndef SMALL_KERNEL
/* Replacement report descriptors for devices shipped with broken ones */
#include <dev/usb/uhid_rdesc.h>
int uhidev_use_rdesc(struct uhidev_softc *, int, int, void **, int *);
#endif /* !SMALL_KERNEL */

#define DEVNAME(sc)		((sc)->sc_dev.dv_xname)

#ifdef UHIDEV_DEBUG
#define DPRINTF(x)	do { if (uhidevdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uhidevdebug>(n)) printf x; } while (0)
int	uhidevdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct uhidev_async_info {
	void (*callback)(void *priv, int id, void *data, int len);
	void *priv;
	void *data;
	int id;
};

void uhidev_intr(struct usbd_xfer *, void *, usbd_status);

int uhidev_maxrepid(void *buf, int len);
int uhidevprint(void *aux, const char *pnp);
int uhidevsubmatch(struct device *parent, void *cf, void *aux);

int uhidev_match(struct device *, void *, void *);
void uhidev_attach(struct device *, struct device *, void *);
int uhidev_detach(struct device *, int);
int uhidev_activate(struct device *, int);

void uhidev_get_report_async_cb(struct usbd_xfer *xfer, void *priv,
    usbd_status status);

struct cfdriver uhidev_cd = {
	NULL, "uhidev", DV_DULL
};

const struct cfattach uhidev_ca = {
	sizeof(struct uhidev_softc), uhidev_match, uhidev_attach,
	uhidev_detach, uhidev_activate,
};

int
uhidev_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL)
		return (UMATCH_NONE);
#ifndef SMALL_KERNEL
	if (uaa->vendor == USB_VENDOR_MICROSOFT &&
	    uaa->product == USB_PRODUCT_MICROSOFT_XBOX360_CONTROLLER &&
	    id->bInterfaceNumber == 0)
		return (UMATCH_VENDOR_PRODUCT);
#endif /* !SMALL_KERNEL */
	if (id->bInterfaceClass != UICLASS_HID)
		return (UMATCH_NONE);
	if (usbd_get_quirks(uaa->device)->uq_flags & UQ_BAD_HID)
		return (UMATCH_NONE);

	return (UMATCH_IFACECLASS_GENERIC);
}

void
uhidev_attach(struct device *parent, struct device *self, void *aux)
{
	struct uhidev_softc *sc = (struct uhidev_softc *)self;
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	struct uhidev_attach_arg uha;
	int size, nrepid, repid, repsz;
	int i, repsizes[256];
	void *desc = NULL;
	struct device *dev;

	sc->sc_udev = uaa->device;
	sc->sc_iface = uaa->iface;
	sc->sc_ifaceno = uaa->ifaceno;
	id = usbd_get_interface_descriptor(sc->sc_iface);

	usbd_set_idle(sc->sc_udev, sc->sc_ifaceno, 0, 0);

	sc->sc_iep_addr = sc->sc_oep_addr = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface, i);
		if (ed == NULL) {
			printf("%s: could not read endpoint descriptor\n",
			    DEVNAME(sc));
			return;
		}

		DPRINTFN(10,("uhidev_attach: bLength=%d bDescriptorType=%d "
		    "bEndpointAddress=%d-%s bmAttributes=%d wMaxPacketSize=%d"
		    " bInterval=%d\n",
		    ed->bLength, ed->bDescriptorType,
		    ed->bEndpointAddress & UE_ADDR,
		    UE_GET_DIR(ed->bEndpointAddress)==UE_DIR_IN? "in" : "out",
		    ed->bmAttributes & UE_XFERTYPE,
		    UGETW(ed->wMaxPacketSize), ed->bInterval));

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			sc->sc_iep_addr = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			sc->sc_oep_addr = ed->bEndpointAddress;
		} else {
			printf("%s: unexpected endpoint\n", DEVNAME(sc));
			return;
		}
	}

	/*
	 * Check that we found an input interrupt endpoint.
	 * The output interrupt endpoint is optional
	 */
	if (sc->sc_iep_addr == -1) {
		printf("%s: no input interrupt endpoint\n", DEVNAME(sc));
		return;
	}

#ifndef SMALL_KERNEL
	if (uhidev_use_rdesc(sc, uaa->vendor, uaa->product, &desc, &size))
		return;
#endif /* !SMALL_KERNEL */

	if (desc == NULL) {
		struct usb_hid_descriptor *hid;

		hid = usbd_get_hid_descriptor(sc->sc_udev, id);
		if (hid == NULL) {
			printf("%s: no HID descriptor\n", DEVNAME(sc));
			return;
		}
		size = UGETW(hid->descrs[0].wDescriptorLength);
		desc = malloc(size, M_USBDEV, M_NOWAIT);
		if (desc == NULL) {
			printf("%s: no memory\n", DEVNAME(sc));
			return;
		}
		if (usbd_get_report_descriptor(sc->sc_udev, sc->sc_ifaceno,
		    desc, size)) {
			printf("%s: no report descriptor\n", DEVNAME(sc));
			free(desc, M_USBDEV, 0);
			return;
		}
	}

	sc->sc_repdesc = desc;
	sc->sc_repdesc_size = size;

	nrepid = uhidev_maxrepid(desc, size);
	if (nrepid < 0)
		return;
	printf("%s: iclass %d/%d", DEVNAME(sc), id->bInterfaceClass,
	    id->bInterfaceSubClass);
	if (nrepid > 0)
		printf(", %d report id%s", nrepid, nrepid > 1 ? "s" : "");
	printf("\n");
	nrepid++;
	sc->sc_subdevs = mallocarray(nrepid, sizeof(struct uhidev *),
	    M_USBDEV, M_NOWAIT | M_ZERO);
	if (sc->sc_subdevs == NULL) {
		printf("%s: no memory\n", DEVNAME(sc));
		return;
	}
	sc->sc_nrepid = nrepid;
	sc->sc_isize = 0;

	for (repid = 0; repid < nrepid; repid++) {
		repsz = hid_report_size(desc, size, hid_input, repid);
		DPRINTF(("uhidev_match: repid=%d, repsz=%d\n", repid, repsz));
		repsizes[repid] = repsz;
		if (repsz > sc->sc_isize)
			sc->sc_isize = repsz;
	}
	sc->sc_isize += (nrepid != 1);	/* one byte for the report ID */
	DPRINTF(("uhidev_attach: isize=%d\n", sc->sc_isize));

	uha.uaa = uaa;
	uha.parent = sc;
	uha.reportid = UHIDEV_CLAIM_ALLREPORTID;

	/* Look for a driver claiming all report IDs first. */
	dev = config_found_sm(self, &uha, NULL, uhidevsubmatch);
	if (dev != NULL) {
		for (repid = 0; repid < nrepid; repid++)
			sc->sc_subdevs[repid] = (struct uhidev *)dev;
		return;
	}

	for (repid = 0; repid < nrepid; repid++) {
		DPRINTF(("%s: try repid=%d\n", __func__, repid));
		if (hid_report_size(desc, size, hid_input, repid) == 0 &&
		    hid_report_size(desc, size, hid_output, repid) == 0 &&
		    hid_report_size(desc, size, hid_feature, repid) == 0)
			continue;

		uha.reportid = repid;
		dev = config_found_sm(self, &uha, uhidevprint, uhidevsubmatch);
		sc->sc_subdevs[repid] = (struct uhidev *)dev;
	}
}

#ifndef SMALL_KERNEL
int
uhidev_use_rdesc(struct uhidev_softc *sc, int vendor, int product,
    void **descp, int *sizep)
{
	static uByte reportbuf[] = {2, 2};
	const void *descptr = NULL;
	void *desc;
	int size;

	if (vendor == USB_VENDOR_WACOM) {
		/* The report descriptor for the Wacom Graphire is broken. */
		switch (product) {
		case USB_PRODUCT_WACOM_GRAPHIRE:
			size = sizeof(uhid_graphire_report_descr);
			descptr = uhid_graphire_report_descr;
			break;
		case USB_PRODUCT_WACOM_GRAPHIRE3_4X5:
		case USB_PRODUCT_WACOM_GRAPHIRE4_4X5:
			uhidev_set_report(sc, UHID_FEATURE_REPORT,
			    2, &reportbuf, sizeof(reportbuf));
			size = sizeof(uhid_graphire3_4x5_report_descr);
			descptr = uhid_graphire3_4x5_report_descr;
			break;
		default:
			break;
		}
	} else if (vendor == USB_VENDOR_MICROSOFT &&
	    product == USB_PRODUCT_MICROSOFT_XBOX360_CONTROLLER) {
		/* The Xbox 360 gamepad has no report descriptor. */
		size = sizeof(uhid_xb360gp_report_descr);
		descptr = uhid_xb360gp_report_descr;
	}

	if (descptr) {
		desc = malloc(size, M_USBDEV, M_NOWAIT);
		if (desc == NULL)
			return (ENOMEM);

		memcpy(desc, descptr, size);

		*descp = desc;
		*sizep = size;
	}

	return (0);
}
#endif /* !SMALL_KERNEL */

int
uhidev_maxrepid(void *buf, int len)
{
	struct hid_data *d;
	struct hid_item h;
	int maxid;

	maxid = -1;
	h.report_ID = 0;
	for (d = hid_start_parse(buf, len, hid_none); hid_get_item(d, &h); )
		if (h.report_ID > maxid)
			maxid = h.report_ID;
	hid_end_parse(d);
	return (maxid);
}

int
uhidevprint(void *aux, const char *pnp)
{
	struct uhidev_attach_arg *uha = aux;

	if (pnp)
		printf("uhid at %s", pnp);
	if (uha->reportid != 0 && uha->reportid != UHIDEV_CLAIM_ALLREPORTID)
		printf(" reportid %d", uha->reportid);
	return (UNCONF);
}

int uhidevsubmatch(struct device *parent, void *match, void *aux)
{
	struct uhidev_attach_arg *uha = aux;
        struct cfdata *cf = match;

	if (cf->uhidevcf_reportid != UHIDEV_UNK_REPORTID &&
	    cf->uhidevcf_reportid != uha->reportid)
		return (0);
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

int
uhidev_activate(struct device *self, int act)
{
	struct uhidev_softc *sc = (struct uhidev_softc *)self;
	int i, rv = 0, r;

	switch (act) {
	case DVACT_DEACTIVATE:
		for (i = 0; i < sc->sc_nrepid; i++)
			if (sc->sc_subdevs[i] != NULL) {
				r = config_deactivate(
				    &sc->sc_subdevs[i]->sc_dev);
				if (r && r != EOPNOTSUPP)
					rv = r;
			}
		usbd_deactivate(sc->sc_udev);
		break;
	}
	return (rv);
}

int
uhidev_detach(struct device *self, int flags)
{
	struct uhidev_softc *sc = (struct uhidev_softc *)self;
	int i, rv = 0;

	DPRINTF(("uhidev_detach: sc=%p flags=%d\n", sc, flags));

	if (sc->sc_opipe != NULL) {
		usbd_abort_pipe(sc->sc_opipe);
		usbd_close_pipe(sc->sc_opipe);
		sc->sc_opipe = NULL;
	}

	if (sc->sc_ipipe != NULL) {
		usbd_abort_pipe(sc->sc_ipipe);
		usbd_close_pipe(sc->sc_ipipe);
		sc->sc_ipipe = NULL;
	}

	if (sc->sc_repdesc != NULL)
		free(sc->sc_repdesc, M_USBDEV, 0);

	/*
	 * XXX Check if we have only one children claiming all the Report
	 * IDs, this is a hack since we need a dev -> Report ID mapping
	 * for uhidev_intr().
	 */
	if (sc->sc_nrepid > 1 && sc->sc_subdevs[0] != NULL &&
	    sc->sc_subdevs[0] == sc->sc_subdevs[1])
		return (config_detach(&sc->sc_subdevs[0]->sc_dev, flags));

	for (i = 0; i < sc->sc_nrepid; i++) {
		if (sc->sc_subdevs[i] != NULL) {
			rv |= config_detach(&sc->sc_subdevs[i]->sc_dev, flags);
			sc->sc_subdevs[i] = NULL;
		}
	}

	return (rv);
}

void
uhidev_intr(struct usbd_xfer *xfer, void *addr, usbd_status status)
{
	struct uhidev_softc *sc = addr;
	struct uhidev *scd;
	u_char *p;
	u_int rep;
	u_int32_t cc;

	if (usbd_is_dying(sc->sc_udev))
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &cc, NULL);

#ifdef UHIDEV_DEBUG
	if (uhidevdebug > 5) {
		u_int32_t i;

		DPRINTF(("uhidev_intr: status=%d cc=%d\n", status, cc));
		DPRINTF(("uhidev_intr: data ="));
		for (i = 0; i < cc; i++)
			DPRINTF((" %02x", sc->sc_ibuf[i]));
		DPRINTF(("\n"));
	}
#endif

	if (status == USBD_CANCELLED || status == USBD_IOERROR)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("%s: interrupt status=%d\n", DEVNAME(sc), status));
		usbd_clear_endpoint_stall_async(sc->sc_ipipe);
		return;
	}

	p = sc->sc_ibuf;
	if (sc->sc_nrepid != 1)
		rep = *p++, cc--;
	else
		rep = 0;
	if (rep >= sc->sc_nrepid) {
		printf("uhidev_intr: bad repid %d\n", rep);
		return;
	}
	scd = sc->sc_subdevs[rep];
	DPRINTFN(5,("uhidev_intr: rep=%d, scd=%p state=0x%x\n",
		    rep, scd, scd ? scd->sc_state : 0));
	if (scd == NULL || !(scd->sc_state & UHIDEV_OPEN))
		return;

	scd->sc_intr(scd, p, cc);
}

void
uhidev_get_report_desc(struct uhidev_softc *sc, void **desc, int *size)
{
	*desc = sc->sc_repdesc;
	*size = sc->sc_repdesc_size;
}

int
uhidev_open(struct uhidev *scd)
{
	struct uhidev_softc *sc = scd->sc_parent;
	usbd_status err;
	int error;

	DPRINTF(("uhidev_open: open pipe, state=%d refcnt=%d\n",
		 scd->sc_state, sc->sc_refcnt));

	if (scd->sc_state & UHIDEV_OPEN)
		return (EBUSY);
	scd->sc_state |= UHIDEV_OPEN;
	if (sc->sc_refcnt++)
		return (0);

	if (sc->sc_isize == 0)
		return (0);

	sc->sc_ibuf = malloc(sc->sc_isize, M_USBDEV, M_WAITOK);

	/* Set up input interrupt pipe. */
	DPRINTF(("uhidev_open: isize=%d, ep=0x%02x\n", sc->sc_isize,
	    sc->sc_iep_addr));

	err = usbd_open_pipe_intr(sc->sc_iface, sc->sc_iep_addr,
		  USBD_SHORT_XFER_OK, &sc->sc_ipipe, sc, sc->sc_ibuf,
		  sc->sc_isize, uhidev_intr, USBD_DEFAULT_INTERVAL);
	if (err != USBD_NORMAL_COMPLETION) {
		DPRINTF(("uhidopen: usbd_open_pipe_intr failed, "
		    "error=%d\n", err));
		error = EIO;
		goto out1;
	}

	DPRINTF(("uhidev_open: sc->sc_ipipe=%p\n", sc->sc_ipipe));

	sc->sc_ixfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_ixfer == NULL) {
		DPRINTF(("uhidev_open: couldn't allocate an xfer\n"));
		error = ENOMEM;
		goto out1; // xxxx
	}

	/*
	 * Set up output interrupt pipe if an output interrupt endpoint
	 * exists.
	 */
	if (sc->sc_oep_addr != -1) {
		DPRINTF(("uhidev_open: oep=0x%02x\n", sc->sc_oep_addr));

		err = usbd_open_pipe(sc->sc_iface, sc->sc_oep_addr,
		    0, &sc->sc_opipe);

		if (err != USBD_NORMAL_COMPLETION) {
			DPRINTF(("uhidev_open: usbd_open_pipe failed, "
			    "error=%d\n", err));
			error = EIO;
			goto out2;
		}
		DPRINTF(("uhidev_open: sc->sc_opipe=%p\n", sc->sc_opipe));

		sc->sc_oxfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_oxfer == NULL) {
			DPRINTF(("uhidev_open: couldn't allocate an xfer\n"));
			error = ENOMEM;
			goto out3;
		}

		sc->sc_owxfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_owxfer == NULL) {
			DPRINTF(("uhidev_open: couldn't allocate owxfer\n"));
			error = ENOMEM;
			goto out3;
		}
	}

	return (0);

out3:
	/* Abort output pipe */
	usbd_close_pipe(sc->sc_opipe);
out2:
	/* Abort input pipe */
	usbd_close_pipe(sc->sc_ipipe);
out1:
	DPRINTF(("uhidev_open: failed in someway"));
	free(sc->sc_ibuf, M_USBDEV, 0);
	scd->sc_state &= ~UHIDEV_OPEN;
	sc->sc_refcnt = 0;
	sc->sc_ipipe = NULL;
	sc->sc_opipe = NULL;
	if (sc->sc_oxfer != NULL) {
		usbd_free_xfer(sc->sc_oxfer);
		sc->sc_oxfer = NULL;
	}
	if (sc->sc_owxfer != NULL) {
		usbd_free_xfer(sc->sc_owxfer);
		sc->sc_owxfer = NULL;
	}
	if (sc->sc_ixfer != NULL) {
		usbd_free_xfer(sc->sc_ixfer);
		sc->sc_ixfer = NULL;
	}
	return (error);
}

void
uhidev_close(struct uhidev *scd)
{
	struct uhidev_softc *sc = scd->sc_parent;

	if (!(scd->sc_state & UHIDEV_OPEN))
		return;
	scd->sc_state &= ~UHIDEV_OPEN;
	if (--sc->sc_refcnt)
		return;
	DPRINTF(("uhidev_close: close pipe\n"));

	if (sc->sc_oxfer != NULL) {
		usbd_free_xfer(sc->sc_oxfer);
		sc->sc_oxfer = NULL;
	}

	if (sc->sc_owxfer != NULL) {
		usbd_free_xfer(sc->sc_owxfer);
		sc->sc_owxfer = NULL;
	}

	if (sc->sc_ixfer != NULL) {
		usbd_free_xfer(sc->sc_ixfer);
		sc->sc_ixfer = NULL;
	}

	/* Disable interrupts. */
	if (sc->sc_opipe != NULL) {
		usbd_abort_pipe(sc->sc_opipe);
		usbd_close_pipe(sc->sc_opipe);
		sc->sc_opipe = NULL;
	}

	if (sc->sc_ipipe != NULL) {
		usbd_abort_pipe(sc->sc_ipipe);
		usbd_close_pipe(sc->sc_ipipe);
		sc->sc_ipipe = NULL;
	}

	if (sc->sc_ibuf != NULL) {
		free(sc->sc_ibuf, M_USBDEV, 0);
		sc->sc_ibuf = NULL;
	}
}

int
uhidev_set_report(struct uhidev_softc *sc, int type, int id, void *data,
    int len)
{
	usb_device_request_t req;
	char *buf = data;
	int actlen = len;

	/* Prepend the reportID. */
	if (id > 0) {
		len++;
		buf = malloc(len, M_TEMP, M_WAITOK);
		buf[0] = id;
		memcpy(buf + 1, data, len - 1);
	}

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_REPORT;
	USETW2(req.wValue, type, id);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, len);

	if (usbd_do_request(sc->sc_udev, &req, buf))
		actlen = -1;

	if (id > 0)
		free(buf, M_TEMP, len);

	return (actlen);
}

int
uhidev_set_report_async(struct uhidev_softc *sc, int type, int id, void *data,
    int len)
{
	usb_device_request_t req;
	char *buf = data;
	int actlen = len;

	/* Prepend the reportID. */
	if (id > 0) {
		len++;
		buf = malloc(len, M_TEMP, M_NOWAIT);
		if (buf == NULL)
			return (USBD_NOMEM);
		buf[0] = id;
		memcpy(buf + 1, data, len - 1);
	}

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_REPORT;
	USETW2(req.wValue, type, id);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, len);

	if (usbd_do_request_async(sc->sc_udev, &req, buf, NULL, NULL))
		actlen = -1;

	/*
	 * Since report requests are write-only it is safe to free
	 * the buffer right after submitting the transfer because
	 * it won't be used afterward.
	 */
	if (id > 0)
		free(buf, M_TEMP, len);

	return (actlen);
}

int
uhidev_get_report(struct uhidev_softc *sc, int type, int id, void *data,
    int len)
{
	usb_device_request_t req;
	char *buf = data;
	usbd_status err;
	int actlen;

	if (id > 0) {
		len++;
		buf = malloc(len, M_TEMP, M_WAITOK|M_ZERO);
	}

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_REPORT;
	USETW2(req.wValue, type, id);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, len);

	err = usbd_do_request_flags(sc->sc_udev, &req, buf, 0, &actlen,
	    USBD_DEFAULT_TIMEOUT);
	if (err != USBD_NORMAL_COMPLETION && err != USBD_SHORT_XFER)
		actlen = -1;

	/* Skip the reportID. */
	if (id > 0) {
		memcpy(data, buf + 1, len - 1);
		free(buf, M_TEMP, len);
	}

	return (actlen);
}

void
uhidev_get_report_async_cb(struct usbd_xfer *xfer, void *priv, usbd_status err)
{
	struct uhidev_async_info *info = priv;
	int len = -1;

	if (err == USBD_NORMAL_COMPLETION || err == USBD_SHORT_XFER) {
		len = xfer->actlen;
		if (info->id > 0) {
			len--;
			memcpy(info->data, xfer->buffer + 1, len);
		}
	}
	info->callback(info->priv, info->id, info->data, len);
	if (info->id > 0)
		free(xfer->buffer, M_TEMP, xfer->length);
	free(info, M_TEMP, sizeof(*info));
	usbd_free_xfer(xfer);
}

int
uhidev_get_report_async(struct uhidev_softc *sc, int type, int id, void *data,
    int len, void *priv, void (*callback)(void *, int, void *, int))
{
	usb_device_request_t req;
	struct uhidev_async_info *info;
	char *buf = data;
	int actlen = len;

	info = malloc(sizeof(*info), M_TEMP, M_NOWAIT);
	if (info == NULL)
		return (-1);

	info->callback = callback;
	info->priv = priv;
	info->data = data;
	info->id = id;

	if (id > 0) {
		len++;
		buf = malloc(len, M_TEMP, M_NOWAIT|M_ZERO);
		if (buf == NULL) {
			free(info, M_TEMP, sizeof(*info));
			return (-1);
		}
	}

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_REPORT;
	USETW2(req.wValue, type, id);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, len);

	if (usbd_do_request_async(sc->sc_udev, &req, buf, priv,
	    uhidev_get_report_async_cb)) {
		free(info, M_TEMP, sizeof(*info));
		if (id > 0)
			free(buf, M_TEMP, len);
		actlen = -1;
	}
	return (actlen);
}

usbd_status
uhidev_write(struct uhidev_softc *sc, void *data, int len)
{
	usbd_status error;

	DPRINTF(("uhidev_write: data=%p, len=%d\n", data, len));

	if (sc->sc_opipe == NULL)
		return USBD_INVAL;

#ifdef UHIDEV_DEBUG
	if (uhidevdebug > 50) {

		u_int32_t i;
		u_int8_t *d = data;

		DPRINTF(("uhidev_write: data ="));
		for (i = 0; i < len; i++)
			DPRINTF((" %02x", d[i]));
		DPRINTF(("\n"));
	}
#endif
	usbd_setup_xfer(sc->sc_owxfer, sc->sc_opipe, 0, data, len,
	    USBD_SYNCHRONOUS | USBD_CATCH, 0, NULL);
	error = usbd_transfer(sc->sc_owxfer);
	if (error)
		usbd_clear_endpoint_stall(sc->sc_opipe);

	return (error);
}

int
uhidev_ioctl(struct uhidev *sc, u_long cmd, caddr_t addr, int flag,
    struct proc *p)
{
	struct usb_ctl_report_desc *rd;
	struct usb_ctl_report *re;
	int size;
	void *desc;

	switch (cmd) {
	case USB_GET_REPORT_DESC:
		uhidev_get_report_desc(sc->sc_parent, &desc, &size);
		rd = (struct usb_ctl_report_desc *)addr;
		size = min(size, sizeof rd->ucrd_data);
		rd->ucrd_size = size;
		memcpy(rd->ucrd_data, desc, size);
		break;
	case USB_GET_REPORT:
		re = (struct usb_ctl_report *)addr;
		switch (re->ucr_report) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			break;
		default:
			return EINVAL;
		}
		if (uhidev_get_report(sc->sc_parent, re->ucr_report,
		    sc->sc_report_id, re->ucr_data, size) != size)
			return EIO;
		break;
	case USB_SET_REPORT:
		re = (struct usb_ctl_report *)addr;
		switch (re->ucr_report) {
		case UHID_INPUT_REPORT:
			size = sc->sc_isize;
			break;
		case UHID_OUTPUT_REPORT:
			size = sc->sc_osize;
			break;
		case UHID_FEATURE_REPORT:
			size = sc->sc_fsize;
			break;
		default:
			return EINVAL;
		}
		if (uhidev_set_report(sc->sc_parent, re->ucr_report,
		    sc->sc_report_id, re->ucr_data, size) != size)
			return EIO;
		break;
	case USB_GET_REPORT_ID:
		*(int *)addr = sc->sc_report_id;
		break;
	default:
		return -1;
	}
	return 0;
}
