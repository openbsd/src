/*	$OpenBSD: usbf_subr.c,v 1.7 2007/06/12 16:26:37 mbalmer Exp $	*/

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
 * USB function driver interface subroutines
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usbf.h>
#include <dev/usb/usbfvar.h>

#ifndef USBF_DEBUG
#define DPRINTF(l, x)	do {} while (0)
#else
extern int usbfdebug;
#define DPRINTF(l, x)	if ((l) <= usbfdebug) printf x; else {}
#endif

void	    *usbf_realloc(void **, size_t *, size_t);
size_t	     usbf_get_string(usbf_device_handle, u_int8_t, char *, size_t);
usbf_status  usbf_open_pipe_ival(usbf_interface_handle, u_int8_t,
				 usbf_pipe_handle *, int);
usbf_status  usbf_setup_pipe(usbf_device_handle, usbf_interface_handle,
			     struct usbf_endpoint *, int,
			     usbf_pipe_handle *);
void	     usbf_start_next(usbf_pipe_handle);
void	     usbf_set_endpoint_halt(usbf_endpoint_handle);
void	     usbf_clear_endpoint_halt(usbf_endpoint_handle);

static const char * const usbf_error_strs[] = USBF_ERROR_STRS;

const char *
usbf_errstr(usbf_status err)
{
	static char buffer[5];

	if (err < USBF_ERROR_MAX)
		return usbf_error_strs[err];

	snprintf(buffer, sizeof buffer, "%d", err);
	return buffer;
}

void *
usbf_realloc(void **pp, size_t *sizep, size_t newsize)
{
	void *p;
	size_t oldsize;

	if (newsize == 0) {
		if (*sizep > 0)
			free(*pp, M_USB);
		*pp = NULL;
		*sizep = 0;
		return NULL;
	}

	p = malloc(newsize, M_USB, M_NOWAIT);
	if (p == NULL)
		return NULL;

	oldsize = MIN(*sizep, newsize);
	if (oldsize > 0)
		bcopy(*pp, p, oldsize);
	*pp = p;
	*sizep = newsize;
	return p;
}

/*
 * Attach a function driver.
 */
static usbf_status
usbf_probe_and_attach(struct device *parent, usbf_device_handle dev, int port)
{
	struct usbf_attach_arg uaa;
	struct device *dv;

	KASSERT(dev->function == NULL);

	bzero(&uaa, sizeof uaa);
	uaa.device = dev;

	/*
	 * The softc structure of a USB function driver must begin with a
	 * "struct usbf_function" member (instead of USBBASEDEV), which must
	 * be initialized in the function driver's attach routine.  Also, it
	 * should use usbf_devinfo_setup() to set the device identification.
	 */
	dv = config_found_sm(parent, &uaa, NULL, NULL);
	if (dv != NULL) {
		dev->function = (struct usbf_function *)dv;
		return USBF_NORMAL_COMPLETION;
	}

	/*
	 * We failed to attach a function driver for this device, but the
	 * device can still function as a generic USB device without any
	 * interfaces.
	 */
	return USBF_NORMAL_COMPLETION;
}

static void
usbf_remove_device(usbf_device_handle dev, struct usbf_port *up)
{
	KASSERT(dev != NULL && dev == up->device);

	if (dev->function != NULL)
		config_detach((struct device *)dev->function, DETACH_FORCE);
	if (dev->default_pipe != NULL)
		usbf_close_pipe(dev->default_pipe);
	up->device = NULL;
	free(dev, M_USB);
}

usbf_status
usbf_new_device(struct device *parent, usbf_bus_handle bus, int depth,
    int speed, int port, struct usbf_port *up)
{
	struct usbf_device *dev;
	usb_device_descriptor_t *ud;
	usbf_status err;

#ifdef DIAGNOSTIC
	KASSERT(up->device == NULL);
#endif

	dev = malloc(sizeof(*dev), M_USB, M_NOWAIT);
	if (dev == NULL)
		return USBF_NOMEM;

	bzero(dev, sizeof *dev);
	dev->bus = bus;
	dev->string_id = USBF_STRING_ID_MIN;
	SIMPLEQ_INIT(&dev->configs);

	/* Initialize device status. */
	USETW(dev->status.wStatus, UDS_SELF_POWERED);

	/*
	 * Initialize device descriptor.  The function driver for this
	 * device (attached below) must complete the device descriptor.
	 */
	ud = &dev->ddesc;
	ud->bLength = USB_DEVICE_DESCRIPTOR_SIZE;
	ud->bDescriptorType = UDESC_DEVICE;
	ud->bMaxPacketSize = bus->ep0_maxp;
	if (bus->usbrev >= USBREV_2_0)
		USETW(ud->bcdUSB, UD_USB_2_0);
	else
		USETW(ud->bcdUSB, 0x0101);

	/* Set up the default endpoint handle and descriptor. */
	dev->def_ep.edesc = &dev->def_ep_desc;
	dev->def_ep_desc.bLength = USB_ENDPOINT_DESCRIPTOR_SIZE;
	dev->def_ep_desc.bDescriptorType = UDESC_ENDPOINT;
	dev->def_ep_desc.bEndpointAddress = USB_CONTROL_ENDPOINT;
	dev->def_ep_desc.bmAttributes = UE_CONTROL;
	USETW(dev->def_ep_desc.wMaxPacketSize, ud->bMaxPacketSize);
	dev->def_ep_desc.bInterval = 0;

	/* Establish the default pipe. */
	err = usbf_setup_pipe(dev, NULL, &dev->def_ep, 0,
	    &dev->default_pipe);
	if (err) {
		free(dev, M_USB);
		return err;
	}

	/* Preallocate xfers for default pipe. */
	dev->default_xfer = usbf_alloc_xfer(dev);
	dev->data_xfer = usbf_alloc_xfer(dev);
	if (dev->default_xfer == NULL || dev->data_xfer == NULL) {
		if (dev->default_xfer != NULL)
			usbf_free_xfer(dev->default_xfer);
		usbf_close_pipe(dev->default_pipe);
		free(dev, M_USB);
		return USBF_NOMEM;
	}

	/* Insert device request xfer. */
	usbf_setup_default_xfer(dev->default_xfer, dev->default_pipe,
	    NULL, &dev->def_req, 0, 0, usbf_do_request);
	err = usbf_transfer(dev->default_xfer);
	if (err && err != USBF_IN_PROGRESS) {
		usbf_free_xfer(dev->default_xfer);
		usbf_free_xfer(dev->data_xfer);
		usbf_close_pipe(dev->default_pipe);
		free(dev, M_USB);
		return err;
	}

	/* Associate the upstream port with the device. */
	bzero(up, sizeof *up);
	up->portno = port;
	up->device = dev;

	/* Attach function driver. */
	err = usbf_probe_and_attach(parent, dev, port);
	if (err)
		usbf_remove_device(dev, up);
	return err;
}

/*
 * Should be called by the function driver in its attach routine to change
 * the default device identification according to the particular function.
 */
void
usbf_devinfo_setup(usbf_device_handle dev, u_int8_t devclass,
    u_int8_t subclass, u_int8_t proto, u_int16_t vendor, u_int16_t product,
    u_int16_t device, const char *manf, const char *prod, const char *ser)
{
	usb_device_descriptor_t *dd;

	dd = usbf_device_descriptor(dev);
	dd->bDeviceClass = devclass;
	dd->bDeviceSubClass = subclass;
	dd->bDeviceProtocol = proto;
	if (vendor != 0)
		USETW(dd->idVendor, vendor);
	if (product != 0)
		USETW(dd->idProduct, product);
	if (device != 0)
		USETW(dd->bcdDevice, device);
	if (manf != NULL)
		dd->iManufacturer = usbf_add_string(dev, manf);
	if (prod != NULL)
		dd->iProduct = usbf_add_string(dev, prod);
	if (ser != NULL)
		dd->iSerialNumber = usbf_add_string(dev, ser);
}

char *
usbf_devinfo_alloc(usbf_device_handle dev)
{
	char manf[40];
	char prod[40];
	usb_device_descriptor_t *dd;
	size_t len;
	char *devinfo;

	dd = usbf_device_descriptor(dev);
	usbf_get_string(dev, dd->iManufacturer, manf, sizeof manf);
	usbf_get_string(dev, dd->iProduct, prod, sizeof prod);

	len = strlen(manf) + strlen(prod) + 32;
	devinfo = malloc(len, M_USB, M_NOWAIT);
	if (devinfo == NULL)
		return NULL;

	snprintf(devinfo, len, "%s %s, rev %d.%02d/%d.%02d", manf, prod,
	    (UGETW(dd->bcdUSB)>>8) & 0xff, UGETW(dd->bcdUSB) & 0xff,
	    (UGETW(dd->bcdDevice)>>8) & 0xff, UGETW(dd->bcdDevice) & 0xff);
	return devinfo;
}

void
usbf_devinfo_free(char *devinfo)
{
	if (devinfo != NULL)
		free(devinfo, M_USB);
}

/*
 * Add a string descriptor to a logical device and return the string's id.
 *
 * If there is not enough memory available for the new string descriptor, or
 * if there is no unused string id left, return the id of the empty string
 * instead of failing.
 */
u_int8_t
usbf_add_string(usbf_device_handle dev, const char *string)
{
	usb_string_descriptor_t *sd;
	size_t oldsize;
	size_t newsize;
	size_t len, i;
	u_int8_t id;

	if (string == NULL || *string == '\0' ||
	    dev->string_id == USBF_STRING_ID_MAX)
		return USBF_EMPTY_STRING_ID;

	if ((len = strlen(string)) > USB_MAX_STRING_LEN)
		len = USB_MAX_STRING_LEN;

	oldsize = dev->sdesc_size;
	newsize = oldsize + 2 + 2 * len;

	sd = usbf_realloc((void **)&dev->sdesc, &dev->sdesc_size,
	    newsize);
	if (sd == NULL)
		return USBF_EMPTY_STRING_ID;

	sd = (usb_string_descriptor_t *)((char *)sd + oldsize);
	sd->bLength = newsize - oldsize;
	sd->bDescriptorType = UDESC_STRING;
	for (i = 0; string[i] != '\0'; i++)
		USETW(sd->bString[i], string[i]);

	id = dev->string_id++;
	return id;
}

usb_string_descriptor_t *
usbf_string_descriptor(usbf_device_handle dev, u_int8_t id)
{
	static usb_string_descriptor_t sd0;
	static usb_string_descriptor_t sd1;
	usb_string_descriptor_t *sd;

	/* handle the special string ids */
	switch (id) {
	case USB_LANGUAGE_TABLE:
		sd0.bLength = 4;
		sd0.bDescriptorType = UDESC_STRING;
		USETW(sd0.bString[0], 0x0409 /* en_US */);
		return &sd0;

	case USBF_EMPTY_STRING_ID:
		sd1.bLength = 2;
		sd1.bDescriptorType = UDESC_STRING;
		return &sd0;
	}

	/* check if the string id is valid */
	if (id > dev->string_id)
		return NULL;

	/* seek and return the descriptor of a non-empty string */
	id -= USBF_STRING_ID_MIN;
	sd = dev->sdesc;
	while (id-- > 0)
		sd = (usb_string_descriptor_t *)((char *)sd + sd->bLength);
	return sd;
}

size_t
usbf_get_string(usbf_device_handle dev, u_int8_t id, char *s, size_t size)
{
	usb_string_descriptor_t *sd = NULL;
	size_t i, len;

	if (id != USB_LANGUAGE_TABLE)
		sd = usbf_string_descriptor(dev, id);

	if (sd == NULL) {
		if (size > 0)
			*s = '\0';
		return 0;
	}

	len = (sd->bLength - 2) / 2;
	if (size < 1)
		return len;

	for (i = 0; i < (size - 1) && i < len; i++)
		s[i] = UGETW(sd->bString[i]) & 0xff;
	s[i] = '\0';
	return len;
}

/*
 * Add a new device configuration to an existing USB logical device.
 * The new configuration initially has zero interfaces.
 */
usbf_status
usbf_add_config(usbf_device_handle dev, usbf_config_handle *ucp)
{
	struct usbf_config *uc;
	usb_config_descriptor_t *cd;

	uc = malloc(sizeof *uc, M_USB, M_NOWAIT);
	if (uc == NULL)
		return USBF_NOMEM;

	cd = malloc(sizeof *cd, M_USB, M_NOWAIT);
	if (cd == NULL) {
		free(uc, M_USB);
		return USBF_NOMEM;
	}

	bzero(uc, sizeof *uc);
	uc->uc_device = dev;
	uc->uc_cdesc = cd;
	uc->uc_cdesc_size = sizeof *cd;
	SIMPLEQ_INIT(&uc->iface_head);

	bzero(cd, sizeof *cd);
	cd->bLength = USB_CONFIG_DESCRIPTOR_SIZE;
	cd->bDescriptorType = UDESC_CONFIG;
	USETW(cd->wTotalLength, USB_CONFIG_DESCRIPTOR_SIZE);
	cd->bConfigurationValue = USB_UNCONFIG_NO + 1 +
	    dev->ddesc.bNumConfigurations;
	cd->iConfiguration = 0;
	cd->bmAttributes = UC_BUS_POWERED | UC_SELF_POWERED;
#if 0
	cd->bMaxPower = 100 / UC_POWER_FACTOR; /* 100 mA */
#else
	cd->bMaxPower = 0; /* XXX 0 mA */
#endif

	SIMPLEQ_INSERT_TAIL(&dev->configs, uc, next);
	dev->ddesc.bNumConfigurations++;

	if (ucp != NULL)
		*ucp = uc;
	return USBF_NORMAL_COMPLETION;
}

/*
 * Allocate memory for a new descriptor at the end of the existing
 * device configuration descriptor.
 */
usbf_status
usbf_add_config_desc(usbf_config_handle uc, usb_descriptor_t *d,
    usb_descriptor_t **dp)
{
    	usb_config_descriptor_t *cd;
	size_t oldsize;
	size_t newsize;

	oldsize = uc->uc_cdesc_size;
	newsize = oldsize + d->bLength;
	if (d->bLength < sizeof(usb_descriptor_t) || newsize > 65535)
		return USBF_INVAL;

	cd = usbf_realloc((void **)&uc->uc_cdesc, &uc->uc_cdesc_size,
	    newsize);
	if (cd == NULL)
		return USBF_NOMEM;

	bcopy(d, (char *)cd + oldsize, d->bLength);
	USETW(cd->wTotalLength, newsize);
	if (dp != NULL)
		*dp = (usb_descriptor_t *)((char *)cd + oldsize);
	return USBF_NORMAL_COMPLETION;
}

usbf_status
usbf_add_interface(usbf_config_handle uc, u_int8_t bInterfaceClass,
    u_int8_t bInterfaceSubClass, u_int8_t bInterfaceProtocol,
    const char *string, usbf_interface_handle *uip)
{
	struct usbf_interface *ui;
	usb_interface_descriptor_t *id;

	if (uc->uc_closed)
		return USBF_INVAL;

	ui = malloc(sizeof *ui, M_USB, M_NOWAIT);
	if (ui == NULL)
		return USBF_NOMEM;

	id = malloc(sizeof *id, M_USB, M_NOWAIT);
	if (id == NULL) {
		free(ui, M_USB);
		return USBF_NOMEM;
	}

	bzero(ui, sizeof *ui);
	ui->config = uc;
	ui->idesc = id;
	LIST_INIT(&ui->pipes);
	SIMPLEQ_INIT(&ui->endpoint_head);

	bzero(id, sizeof *id);
	id->bLength = USB_INTERFACE_DESCRIPTOR_SIZE;
	id->bDescriptorType = UDESC_INTERFACE;
	id->bInterfaceNumber = uc->uc_cdesc->bNumInterface;
	id->bInterfaceClass = bInterfaceClass;
	id->bInterfaceSubClass = bInterfaceSubClass;
	id->bInterfaceProtocol = bInterfaceProtocol;
	id->iInterface = 0; /*usbf_add_string(uc->uc_device, string);*/ /* XXX */

	SIMPLEQ_INSERT_TAIL(&uc->iface_head, ui, next);
	uc->uc_cdesc->bNumInterface++;

	*uip = ui;
	return USBF_NORMAL_COMPLETION;
}

usbf_status
usbf_add_endpoint(usbf_interface_handle ui, u_int8_t bEndpointAddress,
    u_int8_t bmAttributes, u_int16_t wMaxPacketSize, u_int8_t bInterval,
    usbf_endpoint_handle *uep)
{
	struct usbf_endpoint *ue;
	usb_endpoint_descriptor_t *ed;

	if (ui->config->uc_closed)
		return USBF_INVAL;

	ue = malloc(sizeof *ue, M_USB, M_NOWAIT);
	if (ue == NULL)
		return USBF_NOMEM;

	ed = malloc(sizeof *ed, M_USB, M_NOWAIT);
	if (ed == NULL) {
		free(ue, M_USB);
		return USBF_NOMEM;
	}

	bzero(ue, sizeof *ue);
	ue->iface = ui;
	ue->edesc = ed;

	bzero(ed, sizeof *ed);
	ed->bLength = USB_ENDPOINT_DESCRIPTOR_SIZE;
	ed->bDescriptorType = UDESC_ENDPOINT;
	ed->bEndpointAddress = bEndpointAddress;
	ed->bmAttributes = bmAttributes;
	USETW(ed->wMaxPacketSize, wMaxPacketSize);
	ed->bInterval = bInterval;

	SIMPLEQ_INSERT_TAIL(&ui->endpoint_head, ue, next);
	ui->idesc->bNumEndpoints++;

	*uep = ue;
	return USBF_NORMAL_COMPLETION;
}

/*
 * Close the configuration, thereby combining all descriptors and creating
 * the real USB configuration descriptor that can be sent to the USB host.
 */
usbf_status
usbf_end_config(usbf_config_handle uc)
{
	struct usbf_interface *ui;
	struct usbf_endpoint *ue;
	usb_descriptor_t *d;
	usbf_status err = USBF_NORMAL_COMPLETION;

	if (uc->uc_closed)
		return USBF_INVAL;

	SIMPLEQ_FOREACH(ui, &uc->iface_head, next) {
		err = usbf_add_config_desc(uc,
		    (usb_descriptor_t *)ui->idesc, &d);
		if (err)
			break;

		free(ui->idesc, M_USB);
		ui->idesc = (usb_interface_descriptor_t *)d;

		SIMPLEQ_FOREACH(ue, &ui->endpoint_head, next) {
			err = usbf_add_config_desc(uc,
			    (usb_descriptor_t *)ue->edesc, &d);
			if (err)
				break;

			free(ue->edesc, M_USB);
			ue->edesc = (usb_endpoint_descriptor_t *)d;
		}
	}

	uc->uc_closed = 1;
	return err;
}

usb_device_descriptor_t *
usbf_device_descriptor(usbf_device_handle dev)
{
	return &dev->ddesc;
}

usb_config_descriptor_t *
usbf_config_descriptor(usbf_device_handle dev, u_int8_t index)
{
	struct usbf_config *uc;

	SIMPLEQ_FOREACH(uc, &dev->configs, next) {
		if (index-- == 0)
			return uc->uc_cdesc;
	}
	return NULL;
}

int
usbf_interface_number(usbf_interface_handle iface)
{
	return iface->idesc->bInterfaceNumber;
}

u_int8_t
usbf_endpoint_address(usbf_endpoint_handle endpoint)
{
	return endpoint->edesc->bEndpointAddress;
}

u_int8_t
usbf_endpoint_attributes(usbf_endpoint_handle endpoint)
{
	return endpoint->edesc->bmAttributes;
}

usbf_status
usbf_open_pipe(usbf_interface_handle iface, u_int8_t address,
    usbf_pipe_handle *pipe)
{
	return usbf_open_pipe_ival(iface, address, pipe, 0);
}

usbf_status
usbf_open_pipe_ival(usbf_interface_handle iface, u_int8_t address,
    usbf_pipe_handle *pipe, int ival)
{
	struct usbf_endpoint *ep;
	usbf_pipe_handle p;
	usbf_status err;

	ep = usbf_iface_endpoint(iface, address);
	if (ep == NULL)
		return USBF_BAD_ADDRESS;

	err = usbf_setup_pipe(iface->config->uc_device, iface, ep,
	    ival, &p);
	if (err)
		return err;
	LIST_INSERT_HEAD(&iface->pipes, p, next);
	*pipe = p;
	return USBF_NORMAL_COMPLETION;
}

usbf_status
usbf_setup_pipe(usbf_device_handle dev, usbf_interface_handle iface,
    struct usbf_endpoint *ep, int ival, usbf_pipe_handle *pipe)
{
	struct usbf_pipe *p;
	usbf_status err;

	p = malloc(dev->bus->pipe_size, M_USB, M_NOWAIT);
	if (p == NULL)
		return USBF_NOMEM;

	p->device = dev;
	p->iface = iface;
	p->endpoint = ep;
	ep->refcnt++;
	p->running = 0;
	p->refcnt = 1;
	p->repeat = 0;
	p->interval = ival;
	p->methods = NULL;	/* set by bus driver in open_pipe() */
	SIMPLEQ_INIT(&p->queue);
	err = dev->bus->methods->open_pipe(p);
	if (err) {
		free(p, M_USB);
		return err;
	}
	*pipe = p;
	return USBF_NORMAL_COMPLETION;
}

/* Dequeue all pipe operations. */
void
usbf_abort_pipe(usbf_pipe_handle pipe)
{
	usbf_xfer_handle xfer;
	int s;

	s = splusb();
	pipe->repeat = 0;
	pipe->aborting = 1;

	while ((xfer = SIMPLEQ_FIRST(&pipe->queue)) != NULL) {
		DPRINTF(0,("usbf_abort_pipe: pipe=%p, xfer=%p\n", pipe,
		    xfer));
		/* Make the DC abort it (and invoke the callback). */
		pipe->methods->abort(xfer);
	}

	pipe->aborting = 0;
	splx(s);
}

/* Abort all pipe operations and close the pipe. */
void
usbf_close_pipe(usbf_pipe_handle pipe)
{
	usbf_abort_pipe(pipe);
	pipe->methods->close(pipe);
	pipe->endpoint->refcnt--;
	free(pipe, M_USB);
}

void
usbf_stall_pipe(usbf_pipe_handle pipe)
{
	DPRINTF(0,("usbf_stall_pipe not implemented\n"));
}

usbf_endpoint_handle
usbf_iface_endpoint(usbf_interface_handle iface, u_int8_t address)
{
	usbf_endpoint_handle ep;

	SIMPLEQ_FOREACH(ep, &iface->endpoint_head, next) {
		if (ep->edesc->bEndpointAddress == address)
			return ep;
	}
	return NULL;
}

usbf_endpoint_handle
usbf_config_endpoint(usbf_config_handle cfg, u_int8_t address)
{
	usbf_interface_handle iface;
	usbf_endpoint_handle ep;

	SIMPLEQ_FOREACH(iface, &cfg->iface_head, next) {
		SIMPLEQ_FOREACH(ep, &iface->endpoint_head, next) {
			if (ep->edesc->bEndpointAddress == address)
				return ep;
		}
	}
	return NULL;
}

void
usbf_set_endpoint_halt(usbf_endpoint_handle endpoint)
{
}

void
usbf_clear_endpoint_halt(usbf_endpoint_handle endpoint)
{
}

usbf_status
usbf_set_endpoint_feature(usbf_config_handle cfg, u_int8_t address,
    u_int16_t value)
{
	usbf_endpoint_handle ep;

	DPRINTF(0,("usbf_set_endpoint_feature: cfg=%p address=%#x"
	    " value=%#x\n", cfg, address, value));

	ep = usbf_config_endpoint(cfg, address);
	if (ep == NULL)
		return USBF_BAD_ADDRESS;

	switch (value) {
	case UF_ENDPOINT_HALT:
		usbf_set_endpoint_halt(ep);
		return USBF_NORMAL_COMPLETION;
	default:
		/* unsupported feature, send STALL in data/status phase */
		return USBF_STALLED;
	}
}

usbf_status
usbf_clear_endpoint_feature(usbf_config_handle cfg, u_int8_t address,
    u_int16_t value)
{
	usbf_endpoint_handle ep;

	DPRINTF(0,("usbf_clear_endpoint_feature: cfg=%p address=%#x"
	    " value=%#x\n", cfg, address, value));

	ep = usbf_config_endpoint(cfg, address);
	if (ep == NULL)
		return USBF_BAD_ADDRESS;

	switch (value) {
	case UF_ENDPOINT_HALT:
		usbf_clear_endpoint_halt(ep);
		return USBF_NORMAL_COMPLETION;
	default:
		/* unsupported feature, send STALL in data/status phase */
		return USBF_STALLED;
	}
}

usbf_xfer_handle
usbf_alloc_xfer(usbf_device_handle dev)
{
	struct usbf_xfer *xfer;

	/* allocate zero-filled buffer */
	xfer = dev->bus->methods->allocx(dev->bus);
	if (xfer == NULL)
		return NULL;
	xfer->device = dev;
	timeout_set(&xfer->timeout_handle, NULL, NULL);
	DPRINTF(1,("usbf_alloc_xfer() = %p\n", xfer));
	return xfer;
}

void
usbf_free_xfer(usbf_xfer_handle xfer)
{
	DPRINTF(1,("usbf_free_xfer: %p\n", xfer));
	if (xfer->rqflags & (URQ_DEV_DMABUF | URQ_AUTO_DMABUF))
		usbf_free_buffer(xfer);
	xfer->device->bus->methods->freex(xfer->device->bus, xfer);
}

usbf_status
usbf_allocmem(usbf_bus_handle bus, size_t size, size_t align, usb_dma_t *p)
{
	struct usbd_bus dbus;
	usbd_status err;

	/* XXX bad idea, fix usb_mem.c instead! */
	dbus.dmatag = bus->dmatag;
	err = usb_allocmem(&dbus, size, align, p);
	return err ? USBF_NOMEM : USBF_NORMAL_COMPLETION;
}

void
usbf_freemem(usbf_bus_handle bus, usb_dma_t *p)
{
	usb_freemem((usbd_bus_handle)NULL, p);
}

void *
usbf_alloc_buffer(usbf_xfer_handle xfer, u_int32_t size)
{
	struct usbf_bus *bus = xfer->device->bus;
	usbf_status err;

#ifdef DIAGNOSTIC
	if (xfer->rqflags & (URQ_DEV_DMABUF | URQ_AUTO_DMABUF))
		printf("xfer %p already has a buffer\n", xfer);
#endif

	err = bus->methods->allocm(bus, &xfer->dmabuf, size);
	if (err)
		return NULL;

	xfer->rqflags |= URQ_DEV_DMABUF;
	return KERNADDR(&xfer->dmabuf, 0);
}

void
usbf_free_buffer(usbf_xfer_handle xfer)
{
#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_DEV_DMABUF)) {
		printf("usbf_free_buffer: no buffer\n");
		return;
	}
#endif
	xfer->rqflags &= ~URQ_DEV_DMABUF;
	xfer->device->bus->methods->freem(xfer->device->bus, &xfer->dmabuf);
}

#ifdef USBF_DEBUG
/*
 * The dump format is similar to Linux' Gadget driver so that we can
 * easily compare traces.
 */
static void
usbf_dump_buffer(usbf_xfer_handle xfer)
{
	struct device *dev = (struct device *)xfer->pipe->device->bus->usbfctl;
	usbf_endpoint_handle ep = xfer->pipe->endpoint;
	int index = usbf_endpoint_index(ep);
	int dir = usbf_endpoint_dir(ep);
	u_char *p = xfer->buffer;
	u_int i;

	printf("%s: ep%d-%s, length=%u, %s", dev->dv_xname, index,
	    (xfer->rqflags & URQ_REQUEST) ? "setup" :
	    (index == 0 ? "in" : (dir == UE_DIR_IN ? "in" : "out")),
	    xfer->length, usbf_errstr(xfer->status));

	for (i = 0; i < xfer->length; i++) {
		if ((i % 16) == 0)
			printf("\n%4x:", i);
		else if ((i % 8) == 0)
			printf(" ");
		printf(" %02x", p[i]);
	}
	printf("\n");
}
#endif

void
usbf_setup_xfer(usbf_xfer_handle xfer, usbf_pipe_handle pipe,
    usbf_private_handle priv, void *buffer, u_int32_t length,
    u_int16_t flags, u_int32_t timeout, usbf_callback callback)
{
	xfer->pipe = pipe;
	xfer->priv = priv;
	xfer->buffer = buffer;
	xfer->length = length;
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = timeout;
	xfer->status = USBF_NOT_STARTED;
	xfer->callback = callback;
	xfer->rqflags &= ~URQ_REQUEST;
}

void
usbf_setup_default_xfer(usbf_xfer_handle xfer, usbf_pipe_handle pipe,
    usbf_private_handle priv, usb_device_request_t *req, u_int16_t flags,
    u_int32_t timeout, usbf_callback callback)
{
	xfer->pipe = pipe;
	xfer->priv = priv;
	xfer->buffer = req;
	xfer->length = sizeof *req;
	xfer->actlen = 0;
	xfer->flags = flags;
	xfer->timeout = timeout;
	xfer->status = USBF_NOT_STARTED;
	xfer->callback = callback;
	xfer->rqflags |= URQ_REQUEST;
}

void
usbf_get_xfer_status(usbf_xfer_handle xfer, usbf_private_handle *priv,
    void **buffer, u_int32_t *actlen, usbf_status *status)
{
	if (priv != NULL)
		*priv = xfer->priv;
	if (buffer != NULL)
		*buffer = xfer->buffer;
	if (actlen != NULL)
		*actlen = xfer->actlen;
	if (status != NULL)
		*status = xfer->status;
}

usbf_status
usbf_transfer(usbf_xfer_handle xfer)
{
	usbf_pipe_handle pipe = xfer->pipe;
	usbf_status err;

	err = pipe->methods->transfer(xfer);
	if (err != USBF_IN_PROGRESS && err) {
		if (xfer->rqflags & URQ_AUTO_DMABUF) {
			usbf_free_buffer(xfer);
			xfer->rqflags &= ~URQ_AUTO_DMABUF;
		}
	}
	return err;
}

usbf_status
usbf_insert_transfer(usbf_xfer_handle xfer)
{
	usbf_pipe_handle pipe = xfer->pipe;
	usbf_status err;
	int s;

	DPRINTF(1,("usbf_insert_transfer: xfer=%p pipe=%p running=%d\n",
	    xfer, pipe, pipe->running));

	s = splusb();
	SIMPLEQ_INSERT_TAIL(&pipe->queue, xfer, next);
	if (pipe->running)
		err = USBF_IN_PROGRESS;
	else {
		pipe->running = 1;
		err = USBF_NORMAL_COMPLETION;
	}
	splx(s);
	return err;
}

void
usbf_start_next(usbf_pipe_handle pipe)
{
	usbf_xfer_handle xfer;
	usbf_status err;

	SPLUSBCHECK;

	/* Get next request in queue. */
	xfer = SIMPLEQ_FIRST(&pipe->queue);
	if (xfer == NULL)
		pipe->running = 0;
	else {
		err = pipe->methods->start(xfer);
		if (err != USBF_IN_PROGRESS) {
			printf("usbf_start_next: %s\n", usbf_errstr(err));
			pipe->running = 0;
			/* XXX do what? */
		}
	}
}

/* Called at splusb() */
void
usbf_transfer_complete(usbf_xfer_handle xfer)
{
	usbf_pipe_handle pipe = xfer->pipe;
	int repeat = pipe->repeat;

	SPLUSBCHECK;
	DPRINTF(1,("usbf_transfer_complete: xfer=%p pipe=%p running=%d\n",
	    xfer, pipe, pipe->running));
#ifdef USBF_DEBUG
	if (usbfdebug > 0)
		usbf_dump_buffer(xfer);
#endif

	if (!repeat) {
		/* Remove request from queue. */
		KASSERT(SIMPLEQ_FIRST(&pipe->queue) == xfer);
		SIMPLEQ_REMOVE_HEAD(&pipe->queue, next);
	}

	if (xfer->status == USBF_NORMAL_COMPLETION &&
	    xfer->actlen < xfer->length &&
	    !(xfer->flags & USBD_SHORT_XFER_OK)) {
		DPRINTF(0,("usbf_transfer_complete: short xfer=%p %u<%u\n",
		    xfer, xfer->actlen, xfer->length));
		xfer->status = USBF_SHORT_XFER;
	}

	if (xfer->callback != NULL)
		xfer->callback(xfer, xfer->priv, xfer->status);

	pipe->methods->done(xfer);

	/* XXX wake up any processes waiting for the transfer to complete */

	if (!repeat) {
		if (xfer->status != USBF_NORMAL_COMPLETION &&
		    pipe->iface != NULL) /* not control pipe */
			pipe->running = 0;
		else
			usbf_start_next(pipe);
	}
}

/*
 * Software interrupts
 */

usbf_status
usbf_softintr_establish(struct usbf_bus *bus)
{
#ifdef USB_USE_SOFTINTR
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	KASSERT(bus->soft == NULL);
	/* XXX we should have our own level */
	bus->soft = softintr_establish(IPL_SOFTNET,
	    bus->methods->soft_intr, bus);
	if (bus->soft == NULL)
		return USBF_INVAL;
#else
	timeout_set(&bus->softi, NULLL, NULL);
#endif
#endif
	return USBF_NORMAL_COMPLETION;
}

void
usbf_schedsoftintr(struct usbf_bus *bus)
{
#ifdef USB_USE_SOFTINTR
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	softintr_schedule(bus->soft);
#else
	if (!timeout_pending(&bus->softi)) {
		timeout_del(&bus->softi);
		timeout_set(&bus->softi, bus->methods->soft_intr, bus);
		timeout_add(&bus->softi, 0);
	}
#endif /* __HAVE_GENERIC_SOFT_INTERRUPTS */
#else
	bus->methods->soft_intr(bus);
#endif /* USB_USE_SOFTINTR */
}
