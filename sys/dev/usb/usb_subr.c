/*	$OpenBSD: usb_subr.c,v 1.117 2015/03/23 22:26:01 jsg Exp $ */
/*	$NetBSD: usb_subr.c,v 1.103 2003/01/10 11:19:13 augustss Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/usb_subr.c,v 1.18 1999/11/17 22:33:47 n_hibma Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/selinfo.h>
#include <sys/rwlock.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	do { if (usbdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (usbdebug>(n)) printf x; } while (0)
extern int usbdebug;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

usbd_status	usbd_set_config(struct usbd_device *, int);
void		usbd_devinfo(struct usbd_device *, int, char *, size_t);
void		usbd_devinfo_vp(struct usbd_device *, char *, size_t,
		    char *, size_t, int);
char		*usbd_get_string(struct usbd_device *, int, char *, size_t);
int		usbd_getnewaddr(struct usbd_bus *);
int		usbd_print(void *, const char *);
void		usbd_free_iface_data(struct usbd_device *, int);
usbd_status	usbd_probe_and_attach(struct device *,
		    struct usbd_device *, int, int);

int		usbd_printBCD(char *cp, size_t len, int bcd);
void		usb_free_device(struct usbd_device *);

#ifdef USBVERBOSE
#include <dev/usb/usbdevs_data.h>
#endif /* USBVERBOSE */

const char * const usbd_error_strs[] = {
	"NORMAL_COMPLETION",
	"IN_PROGRESS",
	"PENDING_REQUESTS",
	"NOT_STARTED",
	"INVAL",
	"NOMEM",
	"CANCELLED",
	"BAD_ADDRESS",
	"IN_USE",
	"NO_ADDR",
	"SET_ADDR_FAILED",
	"NO_POWER",
	"TOO_DEEP",
	"IOERROR",
	"NOT_CONFIGURED",
	"TIMEOUT",
	"SHORT_XFER",
	"STALLED",
	"INTERRUPTED",
	"XXX",
};

const char *
usbd_errstr(usbd_status err)
{
	static char buffer[5];

	if (err < USBD_ERROR_MAX)
		return (usbd_error_strs[err]);
	else {
		snprintf(buffer, sizeof(buffer), "%d", err);
		return (buffer);
	}
}

usbd_status
usbd_get_string_desc(struct usbd_device *dev, int sindex, int langid,
    usb_string_descriptor_t *sdesc, int *sizep)
{
	usb_device_request_t req;
	usbd_status err;
	int actlen;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_STRING, sindex);
	USETW(req.wIndex, langid);
	USETW(req.wLength, 2);	/* size and descriptor type first */
	err = usbd_do_request_flags(dev, &req, sdesc, USBD_SHORT_XFER_OK,
	    &actlen, USBD_DEFAULT_TIMEOUT);
	if (err)
		return (err);

	if (actlen < 2)
		return (USBD_SHORT_XFER);

	USETW(req.wLength, sdesc->bLength);	/* the whole string */
	err = usbd_do_request_flags(dev, &req, sdesc, USBD_SHORT_XFER_OK,
	    &actlen, USBD_DEFAULT_TIMEOUT);
	if (err)
		return (err);

	if (actlen != sdesc->bLength) {
		DPRINTFN(-1, ("usbd_get_string_desc: expected %d, got %d\n",
		    sdesc->bLength, actlen));
	}

	*sizep = actlen;
	return (USBD_NORMAL_COMPLETION);
}

char *
usbd_get_string(struct usbd_device *dev, int si, char *buf, size_t buflen)
{
	int swap = dev->quirks->uq_flags & UQ_SWAP_UNICODE;
	usb_string_descriptor_t us;
	char *s;
	int i, n;
	u_int16_t c;
	usbd_status err;
	int size;

	if (si == 0)
		return (0);
	if (dev->quirks->uq_flags & UQ_NO_STRINGS)
		return (0);
	if (dev->langid == USBD_NOLANG) {
		/* Set up default language */
		err = usbd_get_string_desc(dev, USB_LANGUAGE_TABLE, 0, &us,
		    &size);
		if (err || size < 4)
			dev->langid = 0; /* Well, just pick English then */
		else {
			/* Pick the first language as the default. */
			dev->langid = UGETW(us.bString[0]);
		}
	}
	err = usbd_get_string_desc(dev, si, dev->langid, &us, &size);
	if (err)
		return (0);
	s = buf;
	n = size / 2 - 1;
	for (i = 0; i < n && i < buflen ; i++) {
		c = UGETW(us.bString[i]);
		/* Convert from Unicode, handle buggy strings. */
		if ((c & 0xff00) == 0)
			*s++ = c;
		else if ((c & 0x00ff) == 0 && swap)
			*s++ = c >> 8;
		else
			*s++ = '?';
	}
	if (buflen > 0)
		*s++ = 0;
	return (buf);
}

static void
usbd_trim_spaces(char *p)
{
	char *q, *e;

	if (p == NULL)
		return;
	q = e = p;
	while (*q == ' ')	/* skip leading spaces */
		q++;
	while ((*p = *q++))	/* copy string */
		if (*p++ != ' ') /* remember last non-space */
			e = p;
	*e = 0;			/* kill trailing spaces */
}

void
usbd_devinfo_vp(struct usbd_device *dev, char *v, size_t vl,
    char *p, size_t pl, int usedev)
{
	usb_device_descriptor_t *udd = &dev->ddesc;
	char *vendor = NULL, *product = NULL;
#ifdef USBVERBOSE
	const struct usb_known_vendor *ukv;
	const struct usb_known_product *ukp;
#endif

	if (dev == NULL) {
		v[0] = p[0] = '\0';
		return;
	}

	if (usedev) {
		vendor = usbd_get_string(dev, udd->iManufacturer, v, vl);
		usbd_trim_spaces(vendor);
		product = usbd_get_string(dev, udd->iProduct, p, pl);
		usbd_trim_spaces(product);
	}
#ifdef USBVERBOSE
	if (vendor == NULL || product == NULL) {
		for (ukv = usb_known_vendors;
		    ukv->vendorname != NULL;
		    ukv++) {
			if (ukv->vendor == UGETW(udd->idVendor)) {
				vendor = ukv->vendorname;
				break;
			}
		}
		if (vendor != NULL) {
			for (ukp = usb_known_products;
			    ukp->productname != NULL;
			    ukp++) {
				if (ukp->vendor == UGETW(udd->idVendor) &&
				    (ukp->product == UGETW(udd->idProduct))) {
					product = ukp->productname;
					break;
				}
			}
		}
	}
#endif

	if (v == vendor)
		;
	else if (vendor != NULL && *vendor)
		strlcpy(v, vendor, vl);
	else
		snprintf(v, vl, "vendor 0x%04x", UGETW(udd->idVendor));

	if (p == product)
		;
	else if (product != NULL && *product)
		strlcpy(p, product, pl);
	else
		snprintf(p, pl, "product 0x%04x", UGETW(udd->idProduct));
}

int
usbd_printBCD(char *cp, size_t len, int bcd)
{
	int l;

	l = snprintf(cp, len, "%x.%02x", bcd >> 8, bcd & 0xff);
	if (l == -1 || len == 0)
		return (0);
	if (l >= len)
		return len - 1;
	return (l);
}

void
usbd_devinfo(struct usbd_device *dev, int showclass, char *base, size_t len)
{
	usb_device_descriptor_t *udd = &dev->ddesc;
	char vendor[USB_MAX_STRING_LEN];
	char product[USB_MAX_STRING_LEN];
	char *cp = base;
	int bcdDevice, bcdUSB;

	usbd_devinfo_vp(dev, vendor, sizeof vendor, product, sizeof product, 1);
	snprintf(cp, len, "\"%s %s\"", vendor, product);
	cp += strlen(cp);
	if (showclass) {
		snprintf(cp, base + len - cp, ", class %d/%d",
		    udd->bDeviceClass, udd->bDeviceSubClass);
		cp += strlen(cp);
	}
	bcdUSB = UGETW(udd->bcdUSB);
	bcdDevice = UGETW(udd->bcdDevice);
	snprintf(cp, base + len - cp, " rev ");
	cp += strlen(cp);
	usbd_printBCD(cp, base + len - cp, bcdUSB);
	cp += strlen(cp);
	snprintf(cp, base + len - cp, "/");
	cp += strlen(cp);
	usbd_printBCD(cp, base + len - cp, bcdDevice);
	cp += strlen(cp);
	snprintf(cp, base + len - cp, " addr %d", dev->address);
}

/* Delay for a certain number of ms */
void
usb_delay_ms(struct usbd_bus *bus, u_int ms)
{
	static int usb_delay_wchan;

	/* Wait at least two clock ticks so we know the time has passed. */
	if (bus->use_polling || cold)
		delay((ms+1) * 1000);
	else
		tsleep(&usb_delay_wchan, PRIBIO, "usbdly",
		    (ms*hz+999)/1000 + 1);
}

/* Delay given a device handle. */
void
usbd_delay_ms(struct usbd_device *dev, u_int ms)
{
	if (usbd_is_dying(dev))
		return;

	usb_delay_ms(dev->bus, ms);
}

usbd_status
usbd_port_disown_to_1_1(struct usbd_device *dev, int port)
{
	usb_port_status_t ps;
	usbd_status err;
	int n;

	err = usbd_set_port_feature(dev, port, UHF_PORT_DISOWN_TO_1_1);
	DPRINTF(("usbd_disown_to_1_1: port %d disown request done, error=%s\n",
	    port, usbd_errstr(err)));
	if (err)
		return (err);
	n = 10;
	do {
		/* Wait for device to recover from reset. */
		usbd_delay_ms(dev, USB_PORT_RESET_DELAY);
		err = usbd_get_port_status(dev, port, &ps);
		if (err) {
			DPRINTF(("%s: get status failed %d\n", __func__, err));
			return (err);
		}
		/* If the device disappeared, just give up. */
		if (!(UGETW(ps.wPortStatus) & UPS_CURRENT_CONNECT_STATUS))
			return (USBD_NORMAL_COMPLETION);
	} while ((UGETW(ps.wPortChange) & UPS_C_PORT_RESET) == 0 && --n > 0);
	if (n == 0)
		return (USBD_TIMEOUT);

	return (err);
}

int
usbd_reset_port(struct usbd_device *dev, int port)
{
	usb_port_status_t ps;
	int n;

	if (usbd_set_port_feature(dev, port, UHF_PORT_RESET))
		return (EIO);
	DPRINTF(("%s: port %d reset done\n", __func__, port));
	n = 10;
	do {
		/* Wait for device to recover from reset. */
		usbd_delay_ms(dev, USB_PORT_RESET_DELAY);
		if (usbd_get_port_status(dev, port, &ps)) {
			DPRINTF(("%s: get status failed\n", __func__));
			return (EIO);
		}
		/* If the device disappeared, just give up. */
		if (!(UGETW(ps.wPortStatus) & UPS_CURRENT_CONNECT_STATUS))
			return (0);
	} while ((UGETW(ps.wPortChange) & UPS_C_PORT_RESET) == 0 && --n > 0);

	/* Clear port reset even if a timeout occured. */
	if (usbd_clear_port_feature(dev, port, UHF_C_PORT_RESET)) {
		DPRINTF(("%s: clear port feature failed\n", __func__));
		return (EIO);
	}

	if (n == 0)
		return (ETIMEDOUT);

	/* Wait for the device to recover from reset. */
	usbd_delay_ms(dev, USB_PORT_RESET_RECOVERY);
	return (0);
}

usb_interface_descriptor_t *
usbd_find_idesc(usb_config_descriptor_t *cd, int ifaceidx, int altidx)
{
	char *p = (char *)cd;
	char *end = p + UGETW(cd->wTotalLength);
	usb_interface_descriptor_t *d;
	int curidx, lastidx, curaidx = 0;

	for (curidx = lastidx = -1; p < end; ) {
		d = (usb_interface_descriptor_t *)p;
		DPRINTFN(4,("usbd_find_idesc: idx=%d(%d) altidx=%d(%d) len=%d "
			    "type=%d\n",
			    ifaceidx, curidx, altidx, curaidx,
			    d->bLength, d->bDescriptorType));
		if (d->bLength == 0) /* bad descriptor */
			break;
		p += d->bLength;
		if (p <= end && d->bDescriptorType == UDESC_INTERFACE) {
			if (d->bInterfaceNumber != lastidx) {
				lastidx = d->bInterfaceNumber;
				curidx++;
				curaidx = 0;
			} else
				curaidx++;
			if (ifaceidx == curidx && altidx == curaidx)
				return (d);
		}
	}
	return (NULL);
}

usb_endpoint_descriptor_t *
usbd_find_edesc(usb_config_descriptor_t *cd, int ifaceidx, int altidx,
		int endptidx)
{
	char *p = (char *)cd;
	char *end = p + UGETW(cd->wTotalLength);
	usb_interface_descriptor_t *d;
	usb_endpoint_descriptor_t *e;
	int curidx;

	d = usbd_find_idesc(cd, ifaceidx, altidx);
	if (d == NULL)
		return (NULL);
	if (endptidx >= d->bNumEndpoints) /* quick exit */
		return (NULL);

	curidx = -1;
	for (p = (char *)d + d->bLength; p < end; ) {
		e = (usb_endpoint_descriptor_t *)p;
		if (e->bLength == 0) /* bad descriptor */
			break;
		p += e->bLength;
		if (p <= end && e->bDescriptorType == UDESC_INTERFACE)
			return (NULL);
		if (p <= end && e->bDescriptorType == UDESC_ENDPOINT) {
			curidx++;
			if (curidx == endptidx)
				return (e);
		}
	}
	return (NULL);
}

usbd_status
usbd_fill_iface_data(struct usbd_device *dev, int ifaceidx, int altidx)
{
	struct usbd_interface *ifc = &dev->ifaces[ifaceidx];
	usb_interface_descriptor_t *idesc;
	char *p, *end;
	int endpt, nendpt;

	DPRINTFN(4,("usbd_fill_iface_data: ifaceidx=%d altidx=%d\n",
		    ifaceidx, altidx));
	idesc = usbd_find_idesc(dev->cdesc, ifaceidx, altidx);
	if (idesc == NULL)
		return (USBD_INVAL);
	ifc->device = dev;
	ifc->idesc = idesc;
	ifc->index = ifaceidx;
	ifc->altindex = altidx;
	nendpt = ifc->idesc->bNumEndpoints;
	DPRINTFN(4,("usbd_fill_iface_data: found idesc nendpt=%d\n", nendpt));
	if (nendpt != 0) {
		ifc->endpoints = mallocarray(nendpt,
		    sizeof(struct usbd_endpoint), M_USB, M_NOWAIT);
		if (ifc->endpoints == NULL)
			return (USBD_NOMEM);
	} else
		ifc->endpoints = NULL;
	ifc->priv = NULL;
	p = (char *)ifc->idesc + ifc->idesc->bLength;
	end = (char *)dev->cdesc + UGETW(dev->cdesc->wTotalLength);
#define ed ((usb_endpoint_descriptor_t *)p)
	for (endpt = 0; endpt < nendpt; endpt++) {
		DPRINTFN(10,("usbd_fill_iface_data: endpt=%d\n", endpt));
		for (; p < end; p += ed->bLength) {
			DPRINTFN(10,("usbd_fill_iface_data: p=%p end=%p "
			    "len=%d type=%d\n", p, end, ed->bLength,
			    ed->bDescriptorType));
			if (p + ed->bLength <= end && ed->bLength != 0 &&
			    ed->bDescriptorType == UDESC_ENDPOINT)
				goto found;
			if (ed->bLength == 0 ||
			    ed->bDescriptorType == UDESC_INTERFACE)
				break;
		}
		/* passed end, or bad desc */
		printf("usbd_fill_iface_data: bad descriptor(s): %s\n",
		    ed->bLength == 0 ? "0 length" :
		    ed->bDescriptorType == UDESC_INTERFACE ? "iface desc" :
		    "out of data");
		goto bad;
	found:
		ifc->endpoints[endpt].edesc = ed;
		if (dev->speed == USB_SPEED_HIGH) {
			u_int mps;
			/* Control and bulk endpoints have max packet
			   limits. */
			switch (UE_GET_XFERTYPE(ed->bmAttributes)) {
			case UE_CONTROL:
				mps = USB_2_MAX_CTRL_PACKET;
				goto check;
			case UE_BULK:
				mps = USB_2_MAX_BULK_PACKET;
			check:
				if (UGETW(ed->wMaxPacketSize) != mps) {
					USETW(ed->wMaxPacketSize, mps);
#ifdef DIAGNOSTIC
					printf("usbd_fill_iface_data: bad max "
					    "packet size\n");
#endif
				}
				break;
			default:
				break;
			}
		}
		ifc->endpoints[endpt].refcnt = 0;
		ifc->endpoints[endpt].savedtoggle = 0;
		p += ed->bLength;
	}
#undef ed
	LIST_INIT(&ifc->pipes);
	return (USBD_NORMAL_COMPLETION);

 bad:
	if (ifc->endpoints != NULL) {
		free(ifc->endpoints, M_USB, 0);
		ifc->endpoints = NULL;
	}
	return (USBD_INVAL);
}

void
usbd_free_iface_data(struct usbd_device *dev, int ifcno)
{
	struct usbd_interface *ifc = &dev->ifaces[ifcno];
	if (ifc->endpoints)
		free(ifc->endpoints, M_USB, 0);
}

usbd_status
usbd_set_config(struct usbd_device *dev, int conf)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_CONFIG;
	USETW(req.wValue, conf);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usbd_do_request(dev, &req, 0));
}

usbd_status
usbd_set_config_no(struct usbd_device *dev, int no, int msg)
{
	int index;
	usb_config_descriptor_t cd;
	usbd_status err;

	DPRINTFN(5,("usbd_set_config_no: %d\n", no));
	/* Figure out what config index to use. */
	for (index = 0; index < dev->ddesc.bNumConfigurations; index++) {
		err = usbd_get_desc(dev, UDESC_CONFIG, index,
		    USB_CONFIG_DESCRIPTOR_SIZE, &cd);
		if (err || cd.bDescriptorType != UDESC_CONFIG)
			return (err);
		if (cd.bConfigurationValue == no)
			return (usbd_set_config_index(dev, index, msg));
	}
	return (USBD_INVAL);
}

usbd_status
usbd_set_config_index(struct usbd_device *dev, int index, int msg)
{
	usb_status_t ds;
	usb_config_descriptor_t cd, *cdp;
	usbd_status err;
	int i, ifcidx, nifc, len, selfpowered, power;

	DPRINTFN(5,("usbd_set_config_index: dev=%p index=%d\n", dev, index));

	/* XXX check that all interfaces are idle */
	if (dev->config != USB_UNCONFIG_NO) {
		DPRINTF(("usbd_set_config_index: free old config\n"));
		/* Free all configuration data structures. */
		nifc = dev->cdesc->bNumInterface;
		for (ifcidx = 0; ifcidx < nifc; ifcidx++)
			usbd_free_iface_data(dev, ifcidx);
		free(dev->ifaces, M_USB, 0);
		free(dev->cdesc, M_USB, 0);
		dev->ifaces = NULL;
		dev->cdesc = NULL;
		dev->config = USB_UNCONFIG_NO;
	}

	if (index == USB_UNCONFIG_INDEX) {
		/* We are unconfiguring the device, so leave unallocated. */
		DPRINTF(("usbd_set_config_index: set config 0\n"));
		err = usbd_set_config(dev, USB_UNCONFIG_NO);
		if (err)
			DPRINTF(("usbd_set_config_index: setting config=0 "
				 "failed, error=%s\n", usbd_errstr(err)));
		return (err);
	}

	/* Get the short descriptor. */
	err = usbd_get_desc(dev, UDESC_CONFIG, index,
	    USB_CONFIG_DESCRIPTOR_SIZE, &cd);
	if (err || cd.bDescriptorType != UDESC_CONFIG)
		return (err);
	len = UGETW(cd.wTotalLength);
	cdp = malloc(len, M_USB, M_NOWAIT);
	if (cdp == NULL)
		return (USBD_NOMEM);
	/* Get the full descriptor. */
	for (i = 0; i < 3; i++) {
		err = usbd_get_desc(dev, UDESC_CONFIG, index, len, cdp);
		if (!err)
			break;
		usbd_delay_ms(dev, 200);
	}
	if (err)
		goto bad;

	if (cdp->bDescriptorType != UDESC_CONFIG) {
		DPRINTFN(-1,("usbd_set_config_index: bad desc %d\n",
		    cdp->bDescriptorType));
		err = USBD_INVAL;
		goto bad;
	}

	/* Figure out if the device is self or bus powered. */
	selfpowered = 0;
	if (!(dev->quirks->uq_flags & UQ_BUS_POWERED) &&
	    (cdp->bmAttributes & UC_SELF_POWERED)) {
		/* May be self powered. */
		if (cdp->bmAttributes & UC_BUS_POWERED) {
			/* Must ask device. */
			if (dev->quirks->uq_flags & UQ_POWER_CLAIM) {
				/*
				 * Hub claims to be self powered, but isn't.
				 * It seems that the power status can be
				 * determined by the hub characteristics.
				 */
				usb_hub_descriptor_t hd;
				usb_device_request_t req;
				req.bmRequestType = UT_READ_CLASS_DEVICE;
				req.bRequest = UR_GET_DESCRIPTOR;
				USETW(req.wValue, 0);
				USETW(req.wIndex, 0);
				USETW(req.wLength, USB_HUB_DESCRIPTOR_SIZE);
				err = usbd_do_request(dev, &req, &hd);
				if (!err &&
				    (UGETW(hd.wHubCharacteristics) &
				     UHD_PWR_INDIVIDUAL))
					selfpowered = 1;
				DPRINTF(("usbd_set_config_index: charac=0x%04x"
				    ", error=%s\n",
				    UGETW(hd.wHubCharacteristics),
				    usbd_errstr(err)));
			} else {
				err = usbd_get_device_status(dev, &ds);
				if (!err &&
				    (UGETW(ds.wStatus) & UDS_SELF_POWERED))
					selfpowered = 1;
				DPRINTF(("usbd_set_config_index: status=0x%04x"
				    ", error=%s\n",
				    UGETW(ds.wStatus), usbd_errstr(err)));
			}
		} else
			selfpowered = 1;
	}
	DPRINTF(("usbd_set_config_index: (addr %d) cno=%d attr=0x%02x, "
		 "selfpowered=%d, power=%d\n", dev->address,
		 cdp->bConfigurationValue, cdp->bmAttributes,
		 selfpowered, cdp->bMaxPower * 2));

	/* Check if we have enough power. */
#ifdef USB_DEBUG
	if (dev->powersrc == NULL) {
		DPRINTF(("usbd_set_config_index: No power source?\n"));
		err = USBD_IOERROR;
		goto bad;
	}
#endif
	power = cdp->bMaxPower * 2;
	if (power > dev->powersrc->power) {
		DPRINTF(("power exceeded %d %d\n", power,dev->powersrc->power));
		/* XXX print nicer message. */
		if (msg)
			printf("%s: device addr %d (config %d) exceeds power "
			    "budget, %d mA > %d mA\n",
			    dev->bus->bdev.dv_xname, dev->address,
			    cdp->bConfigurationValue,
			    power, dev->powersrc->power);
		err = USBD_NO_POWER;
		goto bad;
	}
	dev->power = power;
	dev->self_powered = selfpowered;

	/* Set the actual configuration value. */
	DPRINTF(("usbd_set_config_index: set config %d\n",
	    cdp->bConfigurationValue));
	err = usbd_set_config(dev, cdp->bConfigurationValue);
	if (err) {
		DPRINTF(("usbd_set_config_index: setting config=%d failed, "
		    "error=%s\n", cdp->bConfigurationValue, usbd_errstr(err)));
		goto bad;
	}

	/* Allocate and fill interface data. */
	nifc = cdp->bNumInterface;
	dev->ifaces = mallocarray(nifc, sizeof(struct usbd_interface),
	    M_USB, M_NOWAIT | M_ZERO);
	if (dev->ifaces == NULL) {
		err = USBD_NOMEM;
		goto bad;
	}
	DPRINTFN(5,("usbd_set_config_index: dev=%p cdesc=%p\n", dev, cdp));
	dev->cdesc = cdp;
	dev->config = cdp->bConfigurationValue;
	for (ifcidx = 0; ifcidx < nifc; ifcidx++) {
		err = usbd_fill_iface_data(dev, ifcidx, 0);
		if (err) {
			while (--ifcidx >= 0)
				usbd_free_iface_data(dev, ifcidx);
			goto bad;
		}
	}

	return (USBD_NORMAL_COMPLETION);

 bad:
	free(cdp, M_USB, 0);
	return (err);
}

/* XXX add function for alternate settings */

usbd_status
usbd_setup_pipe(struct usbd_device *dev, struct usbd_interface *iface,
    struct usbd_endpoint *ep, int ival, struct usbd_pipe **pipe)
{
	struct usbd_pipe *p;
	usbd_status err;

	DPRINTF(("%s: dev=%p iface=%p ep=%p pipe=%p\n", __func__,
		    dev, iface, ep, pipe));
	p = malloc(dev->bus->pipe_size, M_USB, M_NOWAIT|M_ZERO);
	if (p == NULL)
		return (USBD_NOMEM);
	p->device = dev;
	p->iface = iface;
	p->endpoint = ep;
	ep->refcnt++;
	p->interval = ival;
	SIMPLEQ_INIT(&p->queue);
	err = dev->bus->methods->open_pipe(p);
	if (err) {
		DPRINTF(("%s: endpoint=0x%x failed, error=%s\n", __func__,
			 ep->edesc->bEndpointAddress, usbd_errstr(err)));
		free(p, M_USB, 0);
		return (err);
	}
	*pipe = p;
	return (USBD_NORMAL_COMPLETION);
}

int
usbd_set_address(struct usbd_device *dev, int addr)
{
	usb_device_request_t req;

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_ADDRESS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	if (usbd_do_request(dev, &req, 0))
		return (1);

	/* Allow device time to set new address */
	usbd_delay_ms(dev, USB_SET_ADDRESS_SETTLE);

	return (0);
}

int
usbd_getnewaddr(struct usbd_bus *bus)
{
	int addr;

	for (addr = 1; addr < USB_MAX_DEVICES; addr++)
		if (bus->devices[addr] == NULL)
			return (addr);
	return (-1);
}

usbd_status
usbd_probe_and_attach(struct device *parent, struct usbd_device *dev, int port,
    int addr)
{
	struct usb_attach_arg uaa;
	usb_device_descriptor_t *dd = &dev->ddesc;
	int i, confi, nifaces, len;
	usbd_status err;
	struct device *dv;
	struct usbd_interface **ifaces;
	extern struct rwlock usbpalock;

	rw_enter_write(&usbpalock);

	uaa.device = dev;
	uaa.iface = NULL;
	uaa.ifaces = NULL;
	uaa.nifaces = 0;
	uaa.usegeneric = 0;
	uaa.port = port;
	uaa.configno = UHUB_UNK_CONFIGURATION;
	uaa.ifaceno = UHUB_UNK_INTERFACE;
	uaa.vendor = UGETW(dd->idVendor);
	uaa.product = UGETW(dd->idProduct);
	uaa.release = UGETW(dd->bcdDevice);

	/* First try with device specific drivers. */
	DPRINTF(("usbd_probe_and_attach trying device specific drivers\n"));
	dv = config_found(parent, &uaa, usbd_print);
	if (dv) {
		dev->subdevs = malloc(2 * sizeof dv, M_USB, M_NOWAIT);
		if (dev->subdevs == NULL) {
			err = USBD_NOMEM;
			goto fail;
		}
		dev->subdevs[dev->ndevs++] = dv;
		dev->subdevs[dev->ndevs] = 0;
		err = USBD_NORMAL_COMPLETION;
		goto fail;
	}

	DPRINTF(("usbd_probe_and_attach: no device specific driver found\n"));

	DPRINTF(("usbd_probe_and_attach: looping over %d configurations\n",
		 dd->bNumConfigurations));
	/* Next try with interface drivers. */
	for (confi = 0; confi < dd->bNumConfigurations; confi++) {
		DPRINTFN(1,("usbd_probe_and_attach: trying config idx=%d\n",
			    confi));
		err = usbd_set_config_index(dev, confi, 1);
		if (err) {
#ifdef USB_DEBUG
			DPRINTF(("%s: port %d, set config at addr %d failed, "
				 "error=%s\n", parent->dv_xname, port,
				 addr, usbd_errstr(err)));
#else
			printf("%s: port %d, set config %d at addr %d failed\n",
			    parent->dv_xname, port, confi, addr);
#endif

 			goto fail;
		}
		nifaces = dev->cdesc->bNumInterface;
		uaa.configno = dev->cdesc->bConfigurationValue;
		ifaces = mallocarray(nifaces, sizeof(*ifaces), M_USB, M_NOWAIT);
		if (ifaces == NULL) {
			err = USBD_NOMEM;
			goto fail;
		}
		for (i = 0; i < nifaces; i++)
			ifaces[i] = &dev->ifaces[i];
		uaa.ifaces = ifaces;
		uaa.nifaces = nifaces;

		/* add 1 for possible ugen and 1 for NULL terminator */
		dev->subdevs = mallocarray(nifaces + 2, sizeof(dv), M_USB,
		    M_NOWAIT | M_ZERO);
		if (dev->subdevs == NULL) {
			free(ifaces, M_USB, 0);
			err = USBD_NOMEM;
			goto fail;
		}
		len = (nifaces + 2) * sizeof(dv);

		for (i = 0; i < nifaces; i++) {
			if (usbd_iface_claimed(dev, i))
				continue;
			uaa.iface = ifaces[i];
			uaa.ifaceno = ifaces[i]->idesc->bInterfaceNumber;
			dv = config_found(parent, &uaa, usbd_print);
			if (dv != NULL) {
				dev->subdevs[dev->ndevs++] = dv;
				usbd_claim_iface(dev, i);
			}
		}
		free(ifaces, M_USB, 0);

		if (dev->ndevs > 0) {
			for (i = 0; i < nifaces; i++) {
				if (!usbd_iface_claimed(dev, i))
					break;
			}
			if (i < nifaces)
				goto generic;
			 else
				goto fail;
		}

		free(dev->subdevs, M_USB, 0);
		dev->subdevs = NULL;
	}
	/* No interfaces were attached in any of the configurations. */

	if (dd->bNumConfigurations > 1) /* don't change if only 1 config */
		usbd_set_config_index(dev, 0, 0);

	DPRINTF(("usbd_probe_and_attach: no interface drivers found\n"));

generic:
	/* Finally try the generic driver. */
	uaa.iface = NULL;
	uaa.usegeneric = 1;
	uaa.configno = dev->ndevs == 0 ? UHUB_UNK_CONFIGURATION :
	    dev->cdesc->bConfigurationValue;
	uaa.ifaceno = UHUB_UNK_INTERFACE;
	dv = config_found(parent, &uaa, usbd_print);
	if (dv != NULL) {
		if (dev->ndevs == 0) {
			dev->subdevs = malloc(2 * sizeof dv, M_USB, M_NOWAIT);
			if (dev->subdevs == NULL) {
				err = USBD_NOMEM;
				goto fail;
			}
		}
		dev->subdevs[dev->ndevs++] = dv;
		dev->subdevs[dev->ndevs] = 0;
		err = USBD_NORMAL_COMPLETION;
		goto fail;
	}

	/*
	 * The generic attach failed, but leave the device as it is.
	 * We just did not find any drivers, that's all.  The device is
	 * fully operational and not harming anyone.
	 */
	DPRINTF(("usbd_probe_and_attach: generic attach failed\n"));
 	err = USBD_NORMAL_COMPLETION;
fail:
	rw_exit_write(&usbpalock);
	return (err);
}


/*
 * Called when a new device has been put in the powered state,
 * but not yet in the addressed state.
 * Get initial descriptor, set the address, get full descriptor,
 * and attach a driver.
 */
usbd_status
usbd_new_device(struct device *parent, struct usbd_bus *bus, int depth,
		int speed, int port, struct usbd_port *up)
{
	struct usbd_device *dev, *adev;
	struct usbd_device *hub;
	usb_device_descriptor_t *dd;
	usbd_status err;
	int addr;
	int i;
	int p;

	DPRINTF(("usbd_new_device bus=%p port=%d depth=%d speed=%d\n",
		 bus, port, depth, speed));
	addr = usbd_getnewaddr(bus);
	if (addr < 0) {
		printf("%s: No free USB addresses, new device ignored.\n",
		    bus->bdev.dv_xname);
		return (USBD_NO_ADDR);
	}

	dev = malloc(sizeof *dev, M_USB, M_NOWAIT | M_ZERO);
	if (dev == NULL)
		return (USBD_NOMEM);

	dev->bus = bus;

	/* Set up default endpoint handle. */
	dev->def_ep.edesc = &dev->def_ep_desc;

	/* Set up default endpoint descriptor. */
	dev->def_ep_desc.bLength = USB_ENDPOINT_DESCRIPTOR_SIZE;
	dev->def_ep_desc.bDescriptorType = UDESC_ENDPOINT;
	dev->def_ep_desc.bEndpointAddress = USB_CONTROL_ENDPOINT;
	dev->def_ep_desc.bmAttributes = UE_CONTROL;
	USETW(dev->def_ep_desc.wMaxPacketSize, USB_MAX_IPACKET);
	dev->def_ep_desc.bInterval = 0;

	dev->quirks = &usbd_no_quirk;
	dev->address = USB_START_ADDR;
	dev->ddesc.bMaxPacketSize = 0;
	dev->depth = depth;
	dev->powersrc = up;
	dev->myhub = up->parent;

	up->device = dev;

	/* Locate port on upstream high speed hub */
	for (adev = dev, hub = up->parent;
	    hub != NULL && hub->speed != USB_SPEED_HIGH;
	    adev = hub, hub = hub->myhub)
		;
	if (hub) {
		for (p = 0; p < hub->hub->nports; p++) {
			if (hub->hub->ports[p].device == adev) {
				dev->myhsport = &hub->hub->ports[p];
				goto found;
			}
		}
		panic("usbd_new_device: cannot find HS port");
	found:
		DPRINTFN(1,("usbd_new_device: high speed port %d\n", p));
	} else {
		dev->myhsport = NULL;
	}
	dev->speed = speed;
	dev->langid = USBD_NOLANG;

	/* Establish the default pipe. */
	err = usbd_setup_pipe(dev, 0, &dev->def_ep, USBD_DEFAULT_INTERVAL,
	    &dev->default_pipe);
	if (err) {
		usb_free_device(dev);
		up->device = NULL;
		return (err);
	}

	dd = &dev->ddesc;

	/* Try to get device descriptor */
	/* 
	 * some device will need small size query at first (XXX: out of spec)
	 * we will get full size descriptor later, just determin the maximum
	 * packet size of the control pipe at this moment.
	 */
	for (i = 0; i < 3; i++) {
		/* Get the first 8 bytes of the device descriptor. */
		/* 8 byte is magic size, some device only return 8 byte for 1st
		 * query (XXX: out of spec) */
		err = usbd_get_desc(dev, UDESC_DEVICE, 0, USB_MAX_IPACKET, dd);
		if (!err)
			break;
		usbd_delay_ms(dev, 100+50*i);
	}

	/* some device need actual size request for the query. try again */
	if (err) {
		USETW(dev->def_ep_desc.wMaxPacketSize,
			USB_DEVICE_DESCRIPTOR_SIZE);
		usbd_reset_port(up->parent, port);
		for (i = 0; i < 3; i++) {
			err = usbd_get_desc(dev, UDESC_DEVICE, 0, 
				USB_DEVICE_DESCRIPTOR_SIZE, dd);
			if (!err)
				break;
			usbd_delay_ms(dev, 100+50*i);
		}
	}

	/* XXX some devices need more time to wake up */
	if (err) {
		USETW(dev->def_ep_desc.wMaxPacketSize, USB_MAX_IPACKET);
		usbd_reset_port(up->parent, port);
		usbd_delay_ms(dev, 500);
		err = usbd_get_desc(dev, UDESC_DEVICE, 0, 
			USB_MAX_IPACKET, dd);
	}

	if (err) {
		usb_free_device(dev);
		up->device = NULL;
		return (err);
	}

	if (speed == USB_SPEED_HIGH) {
		/* Max packet size must be 64 (sec 5.5.3). */
		if (dd->bMaxPacketSize != USB_2_MAX_CTRL_PACKET) {
#ifdef DIAGNOSTIC
			printf("%s: addr=%d bad max packet size %d\n", __func__,
			    addr, dd->bMaxPacketSize);
#endif
			dd->bMaxPacketSize = USB_2_MAX_CTRL_PACKET;
		}
	}

	DPRINTF(("usbd_new_device: adding unit addr=%d, rev=%02x, class=%d, "
		 "subclass=%d, protocol=%d, maxpacket=%d, len=%d, speed=%d\n",
		 addr,UGETW(dd->bcdUSB), dd->bDeviceClass, dd->bDeviceSubClass,
		 dd->bDeviceProtocol, dd->bMaxPacketSize, dd->bLength,
		 dev->speed));

	if (dd->bDescriptorType != UDESC_DEVICE) {
		usb_free_device(dev);
		up->device = NULL;
		return (USBD_INVAL);
	}

	if (dd->bLength < USB_DEVICE_DESCRIPTOR_SIZE) {
		usb_free_device(dev);
		up->device = NULL;
		return (USBD_INVAL);
	}

	USETW(dev->def_ep_desc.wMaxPacketSize, dd->bMaxPacketSize);

	/* Set the address if the HC didn't do it already. */
	if (bus->methods->dev_setaddr != NULL &&
	    bus->methods->dev_setaddr(dev, addr)) {
		usb_free_device(dev);
		up->device = NULL;
		return (USBD_SET_ADDR_FAILED);
 	}

	/*
	 * If this device is attached to an xHCI controller, this
	 * address does not correspond to the hardware one.
	 */
	dev->address = addr;
	bus->devices[addr] = dev;

	err = usbd_reload_device_desc(dev);
	if (err) {
		usb_free_device(dev);
		up->device = NULL;
		return (err);
	}

	/* send disown request to handover 2.0 to 1.1. */
	if (dev->quirks->uq_flags & UQ_EHCI_NEEDTO_DISOWN) {
		/* only effective when the target device is on ehci */
		if (dev->bus->usbrev == USBREV_2_0) {
			DPRINTF(("%s: disown request issues to dev:%p on usb2.0 bus\n",
				__func__, dev));
			usbd_port_disown_to_1_1(dev->myhub, port);
			/* reset_port required to finish disown request */
			usbd_reset_port(dev->myhub, port);
  			return (USBD_NORMAL_COMPLETION);
		}
	}

	/* Assume 100mA bus powered for now. Changed when configured. */
	dev->power = USB_MIN_POWER;
	dev->self_powered = 0;

	DPRINTF(("usbd_new_device: new dev (addr %d), dev=%p, parent=%p\n",
		 addr, dev, parent));

	err = usbd_probe_and_attach(parent, dev, port, addr);
	if (err) {
		usb_free_device(dev);
		up->device = NULL;
		return (err);
  	}

  	return (USBD_NORMAL_COMPLETION);
}

usbd_status
usbd_reload_device_desc(struct usbd_device *dev)
{
	usbd_status err;

	/* Get the full device descriptor. */
	err = usbd_get_desc(dev, UDESC_DEVICE, 0,
		USB_DEVICE_DESCRIPTOR_SIZE, &dev->ddesc);
	if (err)
		return (err);

	/* Figure out what's wrong with this device. */
	dev->quirks = usbd_find_quirk(&dev->ddesc);

	return (USBD_NORMAL_COMPLETION);
}

int
usbd_print(void *aux, const char *pnp)
{
	struct usb_attach_arg *uaa = aux;
	char *devinfop;

	devinfop = malloc(DEVINFOSIZE, M_TEMP, M_WAITOK);
	usbd_devinfo(uaa->device, 0, devinfop, DEVINFOSIZE);

	DPRINTFN(15, ("usbd_print dev=%p\n", uaa->device));
	if (pnp) {
		if (!uaa->usegeneric) {
			free(devinfop, M_TEMP, 0);
			return (QUIET);
		}
		printf("%s at %s", devinfop, pnp);
	}
	if (uaa->port != 0)
		printf(" port %d", uaa->port);
	if (uaa->configno != UHUB_UNK_CONFIGURATION)
		printf(" configuration %d", uaa->configno);
	if (uaa->ifaceno != UHUB_UNK_INTERFACE)
		printf(" interface %d", uaa->ifaceno);

	if (!pnp)
		printf(" %s\n", devinfop);
	free(devinfop, M_TEMP, 0);
	return (UNCONF);
}

void
usbd_fill_deviceinfo(struct usbd_device *dev, struct usb_device_info *di,
    int usedev)
{
	struct usbd_port *p;
	int i, err, s;

	di->udi_bus = dev->bus->usbctl->dv_unit;
	di->udi_addr = dev->address;
	usbd_devinfo_vp(dev, di->udi_vendor, sizeof(di->udi_vendor),
	    di->udi_product, sizeof(di->udi_product), usedev);
	usbd_printBCD(di->udi_release, sizeof di->udi_release,
	    UGETW(dev->ddesc.bcdDevice));
	di->udi_vendorNo = UGETW(dev->ddesc.idVendor);
	di->udi_productNo = UGETW(dev->ddesc.idProduct);
	di->udi_releaseNo = UGETW(dev->ddesc.bcdDevice);
	di->udi_class = dev->ddesc.bDeviceClass;
	di->udi_subclass = dev->ddesc.bDeviceSubClass;
	di->udi_protocol = dev->ddesc.bDeviceProtocol;
	di->udi_config = dev->config;
	di->udi_power = dev->self_powered ? 0 : dev->power;
	di->udi_speed = dev->speed;

	if (dev->subdevs != NULL) {
		for (i = 0; dev->subdevs[i] && i < USB_MAX_DEVNAMES; i++) {
			strncpy(di->udi_devnames[i],
			    dev->subdevs[i]->dv_xname, USB_MAX_DEVNAMELEN);
			di->udi_devnames[i][USB_MAX_DEVNAMELEN-1] = '\0';
		}
	} else
		i = 0;

	for (/*i is set */; i < USB_MAX_DEVNAMES; i++)
		di->udi_devnames[i][0] = 0; /* empty */

	if (dev->hub) {
		for (i = 0;
		    i < nitems(di->udi_ports) && i < dev->hub->nports; i++) {
			p = &dev->hub->ports[i];
			if (p->device)
				err = p->device->address;
			else {
				s = UGETW(p->status.wPortStatus);
				if (s & UPS_PORT_ENABLED)
					err = USB_PORT_ENABLED;
				else if (s & UPS_SUSPEND)
					err = USB_PORT_SUSPENDED;
				else if (s & UPS_PORT_POWER)
					err = USB_PORT_POWERED;
				else
					err = USB_PORT_DISABLED;
			}
			di->udi_ports[i] = err;
		}
		di->udi_nports = dev->hub->nports;
	} else
		di->udi_nports = 0;

	bzero(di->udi_serial, sizeof(di->udi_serial));
	usbd_get_string(dev, dev->ddesc.iSerialNumber, di->udi_serial,
	    sizeof(di->udi_serial));
}

/* Retrieve a complete descriptor for a certain device and index. */
usb_config_descriptor_t *
usbd_get_cdesc(struct usbd_device *dev, int index, int *lenp)
{
	usb_config_descriptor_t *cdesc, *tdesc, cdescr;
	int len;
	usbd_status err;

	if (index == USB_CURRENT_CONFIG_INDEX) {
		tdesc = usbd_get_config_descriptor(dev);
		if (tdesc == NULL)
			return (NULL);
		len = UGETW(tdesc->wTotalLength);
		if (lenp)
			*lenp = len;
		cdesc = malloc(len, M_TEMP, M_WAITOK);
		memcpy(cdesc, tdesc, len);
		DPRINTFN(5,("usbd_get_cdesc: current, len=%d\n", len));
	} else {
		err = usbd_get_desc(dev, UDESC_CONFIG, index,
		    USB_CONFIG_DESCRIPTOR_SIZE, &cdescr);
		if (err || cdescr.bDescriptorType != UDESC_CONFIG)
			return (0);
		len = UGETW(cdescr.wTotalLength);
		DPRINTFN(5,("usbd_get_cdesc: index=%d, len=%d\n", index, len));
		if (lenp)
			*lenp = len;
		cdesc = malloc(len, M_TEMP, M_WAITOK);
		err = usbd_get_desc(dev, UDESC_CONFIG, index, len, cdesc);
		if (err) {
			free(cdesc, M_TEMP, 0);
			return (0);
		}
	}
	return (cdesc);
}

void
usb_free_device(struct usbd_device *dev)
{
	int ifcidx, nifc;

	DPRINTF(("usb_free_device: %p\n", dev));

	if (dev->default_pipe != NULL) {
		usbd_abort_pipe(dev->default_pipe);
		usbd_close_pipe(dev->default_pipe);
	}
	if (dev->ifaces != NULL) {
		nifc = dev->cdesc->bNumInterface;
		for (ifcidx = 0; ifcidx < nifc; ifcidx++)
			usbd_free_iface_data(dev, ifcidx);
		free(dev->ifaces, M_USB, 0);
	}
	if (dev->cdesc != NULL)
		free(dev->cdesc, M_USB, 0);
	if (dev->subdevs != NULL)
		free(dev->subdevs, M_USB, 0);
	dev->bus->devices[dev->address] = NULL;

	free(dev, M_USB, 0);
}

/*
 * Should only be called by the USB thread doing bus exploration to
 * avoid connect/disconnect races.
 */
int
usbd_detach(struct usbd_device *dev, struct device *parent)
{
	int i, rv = 0;

	usbd_deactivate(dev);

	for (i = 0; dev->subdevs[i] != NULL; i++)
		rv |= config_detach(dev->subdevs[i], DETACH_FORCE);

	if (rv == 0)
		usb_free_device(dev);

	return (rv);
}
