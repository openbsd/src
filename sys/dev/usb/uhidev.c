/*	$OpenBSD: uhidev.c,v 1.12 2006/01/06 03:36:32 brad Exp $	*/
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
#define DPRINTF(x)	do { if (uhidevdebug) logprintf x; } while (0)
#define DPRINTFN(n,x)	do { if (uhidevdebug>(n)) logprintf x; } while (0)
int	uhidevdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

Static void uhidev_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

Static int uhidev_maxrepid(void *buf, int len);
Static int uhidevprint(void *aux, const char *pnp);
#if defined(__NetBSD__)
Static int uhidevsubmatch(struct device *parent, struct cfdata *cf, void *aux);
#else
Static int uhidevsubmatch(struct device *parent, void *cf, void *aux);
#endif

USB_DECLARE_DRIVER(uhidev);

USB_MATCH(uhidev)
{
	USB_MATCH_START(uhidev, uaa);
	usb_interface_descriptor_t *id;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);
	id = usbd_get_interface_descriptor(uaa->iface);
	if (id == NULL || id->bInterfaceClass != UICLASS_HID)
		return (UMATCH_NONE);
	if (uaa->matchlvl)
		return (uaa->matchlvl);
	return (UMATCH_IFACECLASS_GENERIC);
}

USB_ATTACH(uhidev)
{
	USB_ATTACH_START(uhidev, sc, uaa);
	usbd_interface_handle iface = uaa->iface;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	struct uhidev_attach_arg uha;
	struct uhidev *dev;
	int size, nrepid, repid, repsz;
	int repsizes[256];
	void *desc;
	const void *descptr;
	usbd_status err;
	char *devinfop;

	sc->sc_udev = uaa->device;
	sc->sc_iface = iface;
	id = usbd_get_interface_descriptor(iface);

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	USB_ATTACH_SETUP;
	printf("%s: %s, iclass %d/%d\n", USBDEVNAME(sc->sc_dev),
	       devinfop, id->bInterfaceClass, id->bInterfaceSubClass);
	usbd_devinfo_free(devinfop);

	(void)usbd_set_idle(iface, 0, 0);
#if 0

	qflags = usbd_get_quirks(sc->sc_udev)->uq_flags;
	if ((qflags & UQ_NO_SET_PROTO) == 0 &&
	    id->bInterfaceSubClass != UISUBCLASS_BOOT)
		(void)usbd_set_protocol(iface, 1);
#endif

	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (ed == NULL) {
		printf("%s: could not read endpoint descriptor\n",
		       USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}

	DPRINTFN(10,("uhidev_attach: bLength=%d bDescriptorType=%d "
		     "bEndpointAddress=%d-%s bmAttributes=%d wMaxPacketSize=%d"
		     " bInterval=%d\n",
		     ed->bLength, ed->bDescriptorType,
		     ed->bEndpointAddress & UE_ADDR,
		     UE_GET_DIR(ed->bEndpointAddress)==UE_DIR_IN? "in" : "out",
		     ed->bmAttributes & UE_XFERTYPE,
		     UGETW(ed->wMaxPacketSize), ed->bInterval));

	if (UE_GET_DIR(ed->bEndpointAddress) != UE_DIR_IN ||
	    (ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		printf("%s: unexpected endpoint\n", USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}

	sc->sc_ep_addr = ed->bEndpointAddress;

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
		printf("%s: no report descriptor\n", USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}

	sc->sc_repdesc = desc;
	sc->sc_repdesc_size = size;

	uha.uaa = uaa;
	nrepid = uhidev_maxrepid(desc, size);
	if (nrepid < 0)
		USB_ATTACH_SUCCESS_RETURN;
	if (nrepid > 0)
		printf("%s: %d report ids\n", USBDEVNAME(sc->sc_dev), nrepid);
	nrepid++;
	sc->sc_subdevs = malloc(nrepid * sizeof(device_ptr_t),
				M_USBDEV, M_NOWAIT);
	bzero(sc->sc_subdevs, nrepid * sizeof(device_ptr_t));
	if (sc->sc_subdevs == NULL) {
		printf("%s: no memory\n", USBDEVNAME(sc->sc_dev));
		USB_ATTACH_ERROR_RETURN;
	}
	sc->sc_nrepid = nrepid;
	sc->sc_isize = 0;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

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
					printf("%s: sc_intr == NULL\n",
					       USBDEVNAME(sc->sc_dev));
					USB_ATTACH_ERROR_RETURN;
				}
#endif
#if defined(__NetBSD__) && NRND > 0
				rnd_attach_source(&dev->rnd_source, 
						  USBDEVNAME(dev->sc_dev), 
						  RND_TYPE_TTY, 0);
#endif
			}
		}
	}

	USB_ATTACH_SUCCESS_RETURN;
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

#if defined(__NetBSD__)
Static int uhidevsubmatch(struct device *parent, struct cfdata *cf, void *aux)
#else
Static int uhidevsubmatch(struct device *parent, void *match, void *aux)
#endif
{
	struct uhidev_attach_arg *uha = aux;
#if defined(__OpenBSD__)
        struct cfdata *cf = match;
#endif

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
uhidev_activate(device_ptr_t self, enum devact act)
{
	struct uhidev_softc *sc = (struct uhidev_softc *)self;
	int i, rv;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);

	case DVACT_DEACTIVATE:
		rv = 0;
		for (i = 0; i < sc->sc_nrepid; i++)
			if (sc->sc_subdevs[i] != NULL)
				rv |= config_deactivate(
					&sc->sc_subdevs[i]->sc_dev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}

USB_DETACH(uhidev)
{
	USB_DETACH_START(uhidev, sc);
	int i, rv;

	DPRINTF(("uhidev_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	if (sc->sc_intrpipe != NULL)
		usbd_abort_pipe(sc->sc_intrpipe);

	if (sc->sc_repdesc != NULL)
		free(sc->sc_repdesc, M_USBDEV);

	rv = 0;
	for (i = 0; i < sc->sc_nrepid; i++) {
		if (sc->sc_subdevs[i] != NULL) {
#if defined(__NetBSD__) && NRND > 0
			rnd_detach_source(&sc->sc_subdevs[i]->rnd_source);
#endif
			rv |= config_detach(&sc->sc_subdevs[i]->sc_dev, flags);
			sc->sc_subdevs[i] = NULL;
		}
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

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
		DPRINTF(("%s: interrupt status=%d\n", USBDEVNAME(sc->sc_dev),
			 status));
		usbd_clear_endpoint_stall_async(sc->sc_intrpipe);
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
#ifdef DIAGNOSTIC
	if (scd->sc_in_rep_size != cc)
		printf("%s: bad input length %d != %d\n",USBDEVNAME(sc->sc_dev),
		       scd->sc_in_rep_size, cc);
#endif
#if defined(__NetBSD__) && NRND > 0
	rnd_add_uint32(&scd->rnd_source, (uintptr_t)(sc->sc_ibuf));
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

	/* Set up interrupt pipe. */
	DPRINTF(("uhidev_open: isize=%d, ep=0x%02x\n", sc->sc_isize,
		 sc->sc_ep_addr));
	err = usbd_open_pipe_intr(sc->sc_iface, sc->sc_ep_addr,
		  USBD_SHORT_XFER_OK, &sc->sc_intrpipe, sc, sc->sc_ibuf,
		  sc->sc_isize, uhidev_intr, USBD_DEFAULT_INTERVAL);
	if (err) {
		DPRINTF(("uhidopen: usbd_open_pipe_intr failed, "
			 "error=%d\n",err));
		free(sc->sc_ibuf, M_USBDEV);
		scd->sc_state &= ~UHIDEV_OPEN;
		sc->sc_refcnt = 0;
		sc->sc_intrpipe = NULL;
		return (EIO);
	}
	return (0);
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

	/* Disable interrupts. */
	if (sc->sc_intrpipe != NULL) {
		usbd_abort_pipe(sc->sc_intrpipe);
		usbd_close_pipe(sc->sc_intrpipe);
		sc->sc_intrpipe = NULL;
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
				  scd->sc_report_id, data, len + 1);

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
