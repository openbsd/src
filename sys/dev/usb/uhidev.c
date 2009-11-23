/*	$OpenBSD: uhidev.c,v 1.37 2009/11/23 19:26:54 yuo Exp $	*/
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

#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/hid.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/uhidev.h>

/* Report descriptor for broken Wacom Graphire */
#include <dev/usb/ugraphire_rdesc.h>

#ifdef UHIDEV_DEBUG
#define DPRINTF(x)	do { if (uhidevdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uhidevdebug>(n)) printf x; } while (0)
int	uhidevdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

void uhidev_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

int uhidev_maxrepid(void *buf, int len);
int uhidevprint(void *aux, const char *pnp);
int uhidevsubmatch(struct device *parent, void *cf, void *aux);

int uhidev_match(struct device *, void *, void *); 
void uhidev_attach(struct device *, struct device *, void *); 
int uhidev_detach(struct device *, int); 
int uhidev_activate(struct device *, int); 

struct cfdriver uhidev_cd = { 
	NULL, "uhidev", DV_DULL 
}; 

const struct cfattach uhidev_ca = { 
	sizeof(struct uhidev_softc), 
	uhidev_match, 
	uhidev_attach, 
	uhidev_detach, 
	uhidev_activate, 
};

int
uhidev_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL || id->bInterfaceClass != UICLASS_HID)
		return (UMATCH_NONE);
	if (usbd_get_quirks(uaa->device)->uq_flags & UQ_BAD_HID)
		return (UMATCH_NONE);
	if (uaa->matchlvl)
		return (uaa->matchlvl);

	return (UMATCH_IFACECLASS_GENERIC);
}

void
uhidev_attach(struct device *parent, struct device *self, void *aux)
{
	struct uhidev_softc *sc = (struct uhidev_softc *)self;
	struct usb_attach_arg *uaa = aux;
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	struct uhidev_attach_arg uha;
	struct uhidev *dev;
	int size, nrepid, repid, repsz;
	int repsizes[256];
	int i;
	void *desc;
	const void *descptr;
	usbd_status err;

	sc->sc_udev = uaa->device;
	sc->sc_iface = iface;
	id = usbd_get_interface_descriptor(iface);

	(void)usbd_set_idle(iface, 0, 0);
#if 0

	qflags = usbd_get_quirks(sc->sc_udev)->uq_flags;
	if ((qflags & UQ_NO_SET_PROTO) == 0 &&
	    id->bInterfaceSubClass != UISUBCLASS_BOOT)
		(void)usbd_set_protocol(iface, 1);
#endif

	sc->sc_iep_addr = sc->sc_oep_addr = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: could not read endpoint descriptor\n",
			    sc->sc_dev.dv_xname);
			sc->sc_dying = 1;
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
			printf("%s: unexpected endpoint\n", sc->sc_dev.dv_xname);
			sc->sc_dying = 1;
			return;
		}
	}

	/*
	 * Check that we found an input interrupt endpoint. The output interrupt
	 * endpoint is optional
	 */
	if (sc->sc_iep_addr == -1) {
		printf("%s: no input interrupt endpoint\n", sc->sc_dev.dv_xname);
		sc->sc_dying = 1;
		return;
	}

	/* XXX need to extend this */
	descptr = NULL;
	if (uaa->vendor == USB_VENDOR_WACOM) {
		static uByte reportbuf[] = {2, 2, 2};

		/* The report descriptor for the Wacom Graphire is broken. */
		switch (uaa->product) {
		case USB_PRODUCT_WACOM_GRAPHIRE:
			size = sizeof uhid_graphire_report_descr;
			descptr = uhid_graphire_report_descr;
			break;
		case USB_PRODUCT_WACOM_GRAPHIRE3_4X5:
		case USB_PRODUCT_WACOM_GRAPHIRE4_4X5:
			usbd_set_report(uaa->iface, UHID_FEATURE_REPORT, 2,
			    &reportbuf, sizeof reportbuf);
			size = sizeof uhid_graphire3_4x5_report_descr;
			descptr = uhid_graphire3_4x5_report_descr;
			break;
		default:
			/* Keep descriptor */
			break;
		}
	}

	if (descptr) {
		desc = malloc(size, M_USBDEV, M_NOWAIT);
		if (desc == NULL)
			err = USBD_NOMEM;
		else {
			err = USBD_NORMAL_COMPLETION;
			memcpy(desc, descptr, size);
		}
	} else {
		desc = NULL;
		err = usbd_read_report_desc(uaa->iface, &desc, &size, M_USBDEV);
	}
	if (err) {
		printf("%s: no report descriptor\n", sc->sc_dev.dv_xname);
		sc->sc_dying = 1;
		return;
	}

	sc->sc_repdesc = desc;
	sc->sc_repdesc_size = size;

	uha.uaa = uaa;
	nrepid = uhidev_maxrepid(desc, size);
	if (nrepid < 0)
		return;
	printf("%s: iclass %d/%d", sc->sc_dev.dv_xname,
	    id->bInterfaceClass, id->bInterfaceSubClass);
	if (nrepid > 0)
		printf(", %d report id%s", nrepid,
		    nrepid > 1 ? "s" : "");
	printf("\n");
	nrepid++;
	sc->sc_subdevs = malloc(nrepid * sizeof(struct device *),
	    M_USBDEV, M_NOWAIT | M_ZERO);
	if (sc->sc_subdevs == NULL) {
		printf("%s: no memory\n", sc->sc_dev.dv_xname);
		return;
	}
	sc->sc_nrepid = nrepid;
	sc->sc_isize = 0;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   &sc->sc_dev);

	for (repid = 0; repid < nrepid; repid++) {
		repsz = hid_report_size(desc, size, hid_input, repid);
		DPRINTF(("uhidev_match: repid=%d, repsz=%d\n", repid, repsz));
		repsizes[repid] = repsz;
		if (repsz > 0) {
			if (repsz > sc->sc_isize)
				sc->sc_isize = repsz;
		}
	}
	sc->sc_isize += nrepid != 1;	/* space for report ID */
	DPRINTF(("uhidev_attach: isize=%d\n", sc->sc_isize));

	uha.parent = sc;
	for (repid = 0; repid < nrepid; repid++) {
		DPRINTF(("uhidev_match: try repid=%d\n", repid));
		if (hid_report_size(desc, size, hid_input, repid) == 0 &&
		    hid_report_size(desc, size, hid_output, repid) == 0 &&
		    hid_report_size(desc, size, hid_feature, repid) == 0) {
			;	/* already NULL in sc->sc_subdevs[repid] */
		} else {
			uha.reportid = repid;
			dev = (struct uhidev *)config_found_sm(self, &uha,
			                           uhidevprint, uhidevsubmatch);
			sc->sc_subdevs[repid] = dev;
			if (dev != NULL) {
				dev->sc_in_rep_size = repsizes[repid];
#ifdef DIAGNOSTIC
				DPRINTF(("uhidev_match: repid=%d dev=%p\n",
					 repid, dev));
				if (dev->sc_intr == NULL) {
					DPRINTF(("%s: sc_intr == NULL\n",
					       sc->sc_dev.dv_xname));
					return;
				}
#endif
			}
		}
	}
}

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
	if (uha->reportid != 0)
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
	if (cf->uhidevcf_reportid == uha->reportid)
		uha->matchlvl = UMATCH_VENDOR_PRODUCT;
	else
		uha->matchlvl = 0;
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}

int
uhidev_activate(struct device *self, int act)
{
	struct uhidev_softc *sc = (struct uhidev_softc *)self;
	int i, rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		for (i = 0; i < sc->sc_nrepid; i++)
			if (sc->sc_subdevs[i] != NULL)
				rv |= config_deactivate(
					&sc->sc_subdevs[i]->sc_dev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

int
uhidev_detach(struct device *self, int flags)
{
	struct uhidev_softc *sc = (struct uhidev_softc *)self;
	int i, rv;

	DPRINTF(("uhidev_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	if (sc->sc_ipipe != NULL)
		usbd_abort_pipe(sc->sc_ipipe);

	if (sc->sc_repdesc != NULL)
		free(sc->sc_repdesc, M_USBDEV);

	rv = 0;
	for (i = 0; i < sc->sc_nrepid; i++) {
		if (sc->sc_subdevs[i] != NULL) {
			rv |= config_detach(&sc->sc_subdevs[i]->sc_dev, flags);
			sc->sc_subdevs[i] = NULL;
		}
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   &sc->sc_dev);

	return (rv);
}

void
uhidev_intr(usbd_xfer_handle xfer, usbd_private_handle addr, usbd_status status)
{
	struct uhidev_softc *sc = addr;
	struct uhidev *scd;
	u_char *p;
	u_int rep;
	u_int32_t cc;

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

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("%s: interrupt status=%d\n", sc->sc_dev.dv_xname,
			 status));
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
#ifdef UHIDEV_DEBUG
	if (scd->sc_in_rep_size != cc)
		printf("%s: bad input length %d != %d\n",sc->sc_dev.dv_xname,
		       scd->sc_in_rep_size, cc);
#endif
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
	free(sc->sc_ibuf, M_USBDEV);
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

	if (sc->sc_oxfer != NULL)
		usbd_free_xfer(sc->sc_oxfer);

	if (sc->sc_owxfer != NULL)
		usbd_free_xfer(sc->sc_owxfer);

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
		free(sc->sc_ibuf, M_USBDEV);
		sc->sc_ibuf = NULL;
	}
}

usbd_status
uhidev_set_report(struct uhidev *scd, int type, void *data, int len)
{
	char *buf;
	usbd_status retstat;

	if (scd->sc_report_id == 0)
		return usbd_set_report(scd->sc_parent->sc_iface, type,
				       scd->sc_report_id, data, len);

	buf = malloc(len + 1, M_TEMP, M_WAITOK);
	buf[0] = scd->sc_report_id;
	memcpy(buf+1, data, len);

	retstat = usbd_set_report(scd->sc_parent->sc_iface, type,
				  scd->sc_report_id, buf, len + 1);

	free(buf, M_TEMP);

	return retstat;
}

void
uhidev_set_report_async(struct uhidev *scd, int type, void *data, int len)
{
	/* XXX */
	char buf[100];
	if (scd->sc_report_id) {
		buf[0] = scd->sc_report_id;
		if ((uint)len > sizeof(buf) - 1) {
#ifdef DIAGNOSTIC
			printf("%s: report length too large (%d)\n",
			    scd->sc_dev.dv_xname, len);
#endif
			return;
		}
		memcpy(buf+1, data, len);
		len++;
		data = buf;
	}

	usbd_set_report_async(scd->sc_parent->sc_iface, type,
			      scd->sc_report_id, data, len);
}

usbd_status
uhidev_get_report(struct uhidev *scd, int type, void *data, int len)
{
	return usbd_get_report(scd->sc_parent->sc_iface, type,
			       scd->sc_report_id, data, len);
}

usbd_status
uhidev_write(struct uhidev_softc *sc, void *data, int len)
{

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
	return usbd_intr_transfer(sc->sc_owxfer, sc->sc_opipe, 0,
	    USBD_NO_TIMEOUT, data, &len, "uhidevwi");
}
