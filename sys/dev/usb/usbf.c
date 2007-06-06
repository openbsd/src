/*	$OpenBSD: usbf.c,v 1.5 2007/06/06 19:25:49 mk Exp $	*/

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
 * USB 2.0 logical device driver
 *
 * Specification non-comformities:
 *
 * - not all Standard Device Requests are supported (see 9.4)
 * - USB 2.0 devices (device_descriptor.bcdUSB >= 0x0200) must support
 *   the other_speed requests but we do not
 *
 * Missing functionality:
 *
 * - isochronous pipes/transfers
 * - clever, automatic endpoint address assignment to make optimal use
 *   of available hardware endpoints
 * - alternate settings for interfaces are unsupported
 */

/*
 * The source code below is marked an can be split into a number of pieces
 * (in that order):
 *
 * - USB logical device match/attach/detach
 * - USB device tasks
 * - Bus event handling
 * - Device request handling
 *
 * Stylistic issues:
 *
 * - "endpoint number" and "endpoint address" are sometimes confused in
 *   this source code, OTOH the endpoint number is just the address, aside
 *   from the direction bit that is added to the number to form a unique
 *   endpoint address
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbf.h>
#include <dev/usb/usbfvar.h>

#ifndef USBF_DEBUG
#define DPRINTF(l, x)	do {} while (0)
#else
int usbfdebug = 0;
#define DPRINTF(l, x)	if ((l) <= usbfdebug) printf x; else {}
#endif

struct usbf_softc {
	struct device	 sc_dev;	/* base device */
	usbf_bus_handle	 sc_bus;	/* USB device controller */
	struct usbf_port sc_port;	/* dummy port for function */
	usb_proc_ptr	 sc_proc;	/* task thread */
	TAILQ_HEAD(,usbf_task) sc_tskq;	/* task queue head */
	int		 sc_dying;
};

#define DEVNAME(sc)	USBDEVNAME((sc)->sc_dev)

int	    usbf_match(struct device *, void *, void *);
void	    usbf_attach(struct device *, struct device *, void *);
void	    usbf_create_thread(void *);
void	    usbf_task_thread(void *);

usbf_status usbf_get_descriptor(usbf_device_handle, usb_device_request_t *, void **);
void	    usbf_set_address(usbf_device_handle, u_int8_t);
usbf_status usbf_set_config(usbf_device_handle, u_int8_t);

#ifdef USBF_DEBUG
void	    usbf_dump_request(usbf_device_handle, usb_device_request_t *);
#endif

struct cfattach usbf_ca = {
	sizeof(struct usbf_softc), usbf_match, usbf_attach
};

struct cfdriver usbf_cd = {
	NULL, "usbf", DV_DULL
};

static const char * const usbrev_str[] = USBREV_STR;

int
usbf_match(struct device *parent, void *match, void *aux)
{
	return UMATCH_GENERIC;
}

void
usbf_attach(struct device *parent, struct device *self, void *aux)
{
	struct usbf_softc *sc = (struct usbf_softc *)self;
	int usbrev;
	int speed;
	usbf_status err;

	/* Continue to set up the bus struct. */
	sc->sc_bus = aux;
	sc->sc_bus->usbfctl = sc;

	usbrev = sc->sc_bus->usbrev;
	printf(": USB revision %s", usbrev_str[usbrev]);
	switch (usbrev) {
	case USBREV_2_0:
		speed = USB_SPEED_HIGH;
		break;
	case USBREV_1_1:
	case USBREV_1_0:
		speed = USB_SPEED_FULL;
		break;
	default:
		printf(", not supported\n");
		sc->sc_dying = 1;
		return;
	}
	printf("\n");

	/* Initialize the usbf struct. */
	TAILQ_INIT(&sc->sc_tskq);

	/* Establish the software interrupt. */
	if (usbf_softintr_establish(sc->sc_bus)) {
		printf("%s: can't establish softintr\n", DEVNAME(sc));
		sc->sc_dying = 1;
		return;
	}

	/* Attach the function driver. */
	err = usbf_new_device(self, sc->sc_bus, 0, speed, 0, &sc->sc_port);
	if (err) {
		printf("%s: usbf_new_device failed, %s\n", DEVNAME(sc),
		    usbf_errstr(err));
		sc->sc_dying = 1;
		return;
	}

	/* Create a process context for asynchronous tasks. */
	config_pending_incr();
	kthread_create_deferred(usbf_create_thread, sc);
}

/*
 * USB device tasks
 */

/*
 * Add a task to be performed by the task thread.  This function can be
 * called from any context and the task function will be executed in a
 * process context ASAP.
 */
void
usbf_add_task(usbf_device_handle dev, struct usbf_task *task)
{
	struct usbf_softc *sc = dev->bus->usbfctl;
	int s;

	s = splusb();
	if (!task->onqueue) {
		DPRINTF(1,("usbf_add_task: task=%p, proc=%s\n",
		    task, sc->sc_bus->intr_context ? "(null)" :
		    curproc->p_comm));
		TAILQ_INSERT_TAIL(&sc->sc_tskq, task, next);
		task->onqueue = 1;
	} else {
		DPRINTF(0,("usbf_add_task: task=%p on q, proc=%s\n",
		    task, sc->sc_bus->intr_context ? "(null)" :
		    curproc->p_comm));
	}
	wakeup(&sc->sc_tskq);
	splx(s);
}

void
usbf_rem_task(usbf_device_handle dev, struct usbf_task *task)
{
	struct usbf_softc *sc = dev->bus->usbfctl;
	int s;

	s = splusb();
	if (task->onqueue) {
		DPRINTF(1,("usbf_rem_task: task=%p\n", task));
		TAILQ_REMOVE(&sc->sc_tskq, task, next);
		task->onqueue = 0;

	} else {
		DPRINTF(0,("usbf_rem_task: task=%p not on q", task));
	}
	splx(s);
}

/*
 * Called from the kernel proper when it can create threads.
 */
void
usbf_create_thread(void *arg)
{
	struct usbf_softc *sc = arg;

	if (kthread_create(usbf_task_thread, sc, &sc->sc_proc, "%s",
	    DEVNAME(sc)) != 0) {
		printf("%s: can't create task thread\n", DEVNAME(sc));
		return;
	}
	config_pending_decr();
}

/*
 * Process context for USB function tasks.
 */
void
usbf_task_thread(void *arg)
{
	struct usbf_softc *sc = arg;
	struct usbf_task *task;
	int s;

	DPRINTF(0,("usbf_task_thread: start (pid %d)\n", curproc->p_pid));

	s = splusb();
	while (!sc->sc_dying) {
		task = TAILQ_FIRST(&sc->sc_tskq);
		if (task == NULL) {
			tsleep(&sc->sc_tskq, PWAIT, "usbtsk", 0);
			task = TAILQ_FIRST(&sc->sc_tskq);
		}
		DPRINTF(1,("usbf_task_thread: woke up task=%p\n", task));
		if (task != NULL) {
			TAILQ_REMOVE(&sc->sc_tskq, task, next);
			task->onqueue = 0;
			splx(s);
			task->fun(task->arg);
			s = splusb();
			DPRINTF(1,("usbf_task_thread: done task=%p\n",
			    task));
		}
	}
	splx(s);

	DPRINTF(0,("usbf_task_thread: exit\n"));
	kthread_exit(0);
}

/*
 * Bus event handling
 */

void
usbf_host_reset(usbf_bus_handle bus)
{
	usbf_device_handle dev = bus->usbfctl->sc_port.device;

	DPRINTF(0,("usbf_host_reset\n"));

	/* Change device state from any state backe to Default. */
	(void)usbf_set_config(dev, USB_UNCONFIG_NO);
	dev->address = 0;
}

/*
 * Device request handling
 */

/* XXX */
static u_int8_t hs_config[65536];

usbf_status
usbf_get_descriptor(usbf_device_handle dev, usb_device_request_t *req,
    void **data)
{
	u_int8_t type = UGETW(req->wValue) >> 8;
	u_int8_t index = UGETW(req->wValue) & 0xff;
	usb_device_descriptor_t *dd;
	usb_config_descriptor_t *cd;
	usb_string_descriptor_t *sd;

	switch (type) {
	case UDESC_DEVICE:
		dd = usbf_device_descriptor(dev);
		*data = dd;
		USETW(req->wLength, MIN(UGETW(req->wLength), dd->bLength));;
		return USBF_NORMAL_COMPLETION;

	case UDESC_DEVICE_QUALIFIER: {
		static usb_device_qualifier_t dq;

		dd = usbf_device_descriptor(dev);
		bzero(&dq, sizeof dq);
		dq.bLength = USB_DEVICE_QUALIFIER_SIZE;
		dq.bDescriptorType = UDESC_DEVICE_QUALIFIER;
		USETW(dq.bcdUSB, 0x0200);
		dq.bDeviceClass = dd->bDeviceClass;
		dq.bDeviceSubClass = dd->bDeviceSubClass;
		dq.bDeviceProtocol = dd->bDeviceProtocol;
		dq.bMaxPacketSize0 = dd->bMaxPacketSize;
		dq.bNumConfigurations = dd->bNumConfigurations;
		*data = &dq;
		USETW(req->wLength, MIN(UGETW(req->wLength), dq.bLength));;
		return USBF_NORMAL_COMPLETION;
	}

	case UDESC_CONFIG:
		cd = usbf_config_descriptor(dev, index);
		if (cd == NULL)
			return USBF_INVAL;
		*data = cd;
		USETW(req->wLength, MIN(UGETW(req->wLength),
		    UGETW(cd->wTotalLength)));
		return USBF_NORMAL_COMPLETION;

	/* XXX */
	case UDESC_OTHER_SPEED_CONFIGURATION:
		cd = usbf_config_descriptor(dev, index);
		if (cd == NULL)
			return USBF_INVAL;
		bcopy(cd, &hs_config, UGETW(cd->wTotalLength));
		*data = &hs_config;
		((usb_config_descriptor_t *)&hs_config)->bDescriptorType =
		    UDESC_OTHER_SPEED_CONFIGURATION;
		USETW(req->wLength, MIN(UGETW(req->wLength),
		    UGETW(cd->wTotalLength)));
		return USBF_NORMAL_COMPLETION;

	case UDESC_STRING:
		sd = usbf_string_descriptor(dev, index);
		if (sd == NULL)
			return USBF_INVAL;
		*data = sd;
		USETW(req->wLength, MIN(UGETW(req->wLength), sd->bLength));
		return USBF_NORMAL_COMPLETION;

	default:
		DPRINTF(0,("usbf_get_descriptor: unknown descriptor type=%u\n",
		    type));
		return USBF_INVAL;
	}
}

/*
 * Change device state from Default to Address, or change the device address
 * if the device is not currently in the Default state.
 */
void
usbf_set_address(usbf_device_handle dev, u_int8_t address)
{
	DPRINTF(0,("usbf_set_address: dev=%p, %u -> %u\n", dev,
	    dev->address, address));
	dev->address = address;
}

/*
 * If the device was in the Addressed state (dev->config == NULL) before, it
 * will be in the Configured state upon successful return from this routine.
 */
usbf_status
usbf_set_config(usbf_device_handle dev, u_int8_t new)
{
	usbf_config_handle cfg = dev->config;
	usbf_function_handle fun = dev->function;
	usbf_status err = USBF_NORMAL_COMPLETION;
	u_int8_t old = cfg ? cfg->uc_cdesc->bConfigurationValue :
	    USB_UNCONFIG_NO;

	if (old == new)
		return USBF_NORMAL_COMPLETION;

	DPRINTF(0,("usbf_set_config: dev=%p, %u -> %u\n", dev, old, new));

	/*
	 * Resetting the device state to Unconfigured must always succeed.
	 * This happens typically when the host resets the bus.
	 */
	if (new == USB_UNCONFIG_NO) {
		if (dev->function->methods->set_config)
			err = fun->methods->set_config(fun, NULL);
		if (err) {
			DPRINTF(0,("usbf_set_config: %s\n",
			    usbf_errstr(err)));
		}
		dev->config = NULL;
		return USBF_NORMAL_COMPLETION;
	}

	/*
	 * Changing the device configuration may fail.  The function
	 * may decline to set the new configuration.
	 */
	SIMPLEQ_FOREACH(cfg, &dev->configs, next) {
		if (cfg->uc_cdesc->bConfigurationValue == new) {
			if (dev->function->methods->set_config)
				err = fun->methods->set_config(fun, cfg);
			if (!err)
				dev->config = cfg;
			return err;
		}
	}
	return USBF_INVAL;
}

/*
 * Handle device requests coming in via endpoint 0 pipe.
 */
void
usbf_do_request(usbf_xfer_handle xfer, usbf_private_handle priv,
    usbf_status err)
{
	usbf_device_handle dev = xfer->pipe->device;
	usb_device_request_t *req = xfer->buffer;
	usbf_config_handle cfg;
	void *data = NULL;
	u_int16_t value;
	u_int16_t index;

	if (err) {
		DPRINTF(0,("usbf_do_request: receive failed, %s\n",
		    usbf_errstr(err)));
		return;
	}

#ifdef USBF_DEBUG
	if (usbfdebug >= 0)
		usbf_dump_request(dev, req);
#endif

#define C(x,y) ((x) | ((y) << 8))
	switch (C(req->bRequest, req->bmRequestType)) {
		
	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		/* Change device state from Default to Address. */
		usbf_set_address(dev, UGETW(req->wValue));
		break;

	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		/* Change device state from Address to Configured. */
		printf("set config activated\n");
		err = usbf_set_config(dev, UGETW(req->wValue) & 0xff);
		break;

	case C(UR_GET_CONFIG, UT_READ_DEVICE):
	{			/* XXX */
		if ((cfg = dev->config) == NULL) {
			static u_int8_t zero = 0;
			data = &zero;
		} else
			data = &cfg->uc_cdesc->bConfigurationValue;
		USETW(req->wLength, MIN(UGETW(req->wLength), 1));;
	}
		break;

	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		err = usbf_get_descriptor(dev, req, &data);
		break;

	case C(UR_GET_STATUS, UT_READ_DEVICE):
		DPRINTF(1,("usbf_do_request: UR_GET_STATUS %d\n",
			    UGETW(req->wLength)));
		data = &dev->status;
		USETW(req->wLength, MIN(UGETW(req->wLength),
		    sizeof dev->status));
		break;

	case C(UR_GET_STATUS, UT_READ_ENDPOINT): {
		//u_int8_t addr = UGETW(req->wIndex) & 0xff;
		static u_int16_t status = 0;

		data = &status;
		USETW(req->wLength, MIN(UGETW(req->wLength), sizeof status));
		break;
	}

	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		value = UGETW(req->wValue);
		index = UGETW(req->wIndex);
		if ((cfg = dev->config) == NULL)
			err = USBF_STALLED;
		else
			err = usbf_set_endpoint_feature(cfg, index, value);
		break;

	case C(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		value = UGETW(req->wValue);
		index = UGETW(req->wIndex);
		if ((cfg = dev->config) == NULL)
			err = USBF_STALLED;
		else
			err = usbf_clear_endpoint_feature(cfg, index, value);
		break;

	/* Alternate settings for interfaces are unsupported. */
	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		if (UGETW(req->wValue) != 0)
			err = USBF_STALLED;
		break;
	case C(UR_GET_INTERFACE, UT_READ_INTERFACE): {
		static u_int8_t zero = 0;
		data = &zero;
		USETW(req->wLength, MIN(UGETW(req->wLength), 1));
		break;
	}

	default: {
		usbf_function_handle fun = dev->function;
		
		if (fun == NULL)
			err = USBF_STALLED;
		else
			/* XXX change prototype for this method to remove
			 * XXX the data argument. */
			err = fun->methods->do_request(fun, req, &data);
	}
	}

	if (err) {
		DPRINTF(0,("usbf_do_request: request=%#x, type=%#x "
		    "failed, %s\n", req->bRequest, req->bmRequestType,
		    usbf_errstr(err)));
		usbf_stall_pipe(dev->default_pipe);
	} else if (UGETW(req->wLength) > 0) {
		if (data == NULL) {
			DPRINTF(0,("usbf_do_request: no data, "
			    "sending ZLP\n"));
			USETW(req->wLength, 0);
		}
		/* Transfer IN data in response to the request. */
		usbf_setup_xfer(dev->data_xfer, dev->default_pipe,
		    NULL, data, UGETW(req->wLength), 0, 0, NULL);
		err = usbf_transfer(dev->data_xfer);
		if (err && err != USBF_IN_PROGRESS) {
			DPRINTF(0,("usbf_do_request: data xfer=%p, %s\n",
			    xfer, usbf_errstr(err)));
		}
	}

	/* Schedule another request transfer. */
	usbf_setup_default_xfer(dev->default_xfer, dev->default_pipe,
	    NULL, &dev->def_req, 0, 0, usbf_do_request);
	err = usbf_transfer(dev->default_xfer);
	if (err && err != USBF_IN_PROGRESS) {
		DPRINTF(0,("usbf_do_request: ctrl xfer=%p, %s\n", xfer,
		    usbf_errstr(err)));
	}
}

#ifdef USBF_DEBUG
struct usb_enum_str {
	int code;
	const char * const str;
};

static const struct usb_enum_str usb_request_str[] = {
	{ UR_GET_STATUS,		"GET STATUS"             },
	{ UR_CLEAR_FEATURE,		"CLEAR FEATURE"          },
	{ UR_SET_FEATURE,		"SET FEATURE"            },
	{ UR_SET_ADDRESS,		"SET ADDRESS"            },
	{ UR_GET_DESCRIPTOR,		"GET DESCRIPTOR"         },
	{ UR_SET_DESCRIPTOR,		"SET DESCRIPTOR"         },
	{ UR_GET_CONFIG,		"GET CONFIG"             },
	{ UR_SET_CONFIG,		"SET CONFIG"             },
	{ UR_GET_INTERFACE,		"GET INTERFACE"          },
	{ UR_SET_INTERFACE,		"SET INTERFACE"          },
	{ UR_SYNCH_FRAME,		"SYNCH FRAME"            },
	{ 0, NULL }
};

static const struct usb_enum_str usb_request_type_str[] = {
	{ UT_READ_DEVICE,		"Read Device"            },
	{ UT_READ_INTERFACE,		"Read Interface"         },
	{ UT_READ_ENDPOINT,		"Read Endpoint"          },
	{ UT_WRITE_DEVICE,		"Write Device"           },
	{ UT_WRITE_INTERFACE,		"Write Interface"        },
	{ UT_WRITE_ENDPOINT,		"Write Endpoint"         },
	{ UT_READ_CLASS_DEVICE,		"Read Class Device"      },
	{ UT_READ_CLASS_INTERFACE,	"Read Class Interface"   },
	{ UT_READ_CLASS_OTHER,		"Read Class Other"       },
	{ UT_READ_CLASS_ENDPOINT,	"Read Class Endpoint"    },
	{ UT_WRITE_CLASS_DEVICE,	"Write Class Device"     },
	{ UT_WRITE_CLASS_INTERFACE,	"Write Class Interface"  },
	{ UT_WRITE_CLASS_OTHER,		"Write Class Other"      },
	{ UT_WRITE_CLASS_ENDPOINT,	"Write Class Endpoint"   },
	{ UT_READ_VENDOR_DEVICE,	"Read Vendor Device"     },
	{ UT_READ_VENDOR_INTERFACE,	"Read Vendor Interface"  },
	{ UT_READ_VENDOR_OTHER,		"Read Vendor Other"      },
	{ UT_READ_VENDOR_ENDPOINT,	"Read Vendor Endpoint"   },
	{ UT_WRITE_VENDOR_DEVICE,	"Write Vendor Device"    },
	{ UT_WRITE_VENDOR_INTERFACE,	"Write Vendor Interface" },
	{ UT_WRITE_VENDOR_OTHER,	"Write Vendor Other"     },
	{ UT_WRITE_VENDOR_ENDPOINT,	"Write Vendor Endpoint"  },
	{ 0, NULL }
};

static const struct usb_enum_str usb_request_desc_str[] = {
	{ UDESC_DEVICE,			"Device"                       },
	{ UDESC_CONFIG,			"Configuration"                },
	{ UDESC_STRING,			"String"                       },
	{ UDESC_INTERFACE,		"Interface"                    },
	{ UDESC_ENDPOINT,		"Endpoint"                     },
	{ UDESC_DEVICE_QUALIFIER,	"Device Qualifier"             },
	{ UDESC_OTHER_SPEED_CONFIGURATION, "Other Speed Configuration" },
	{ UDESC_INTERFACE_POWER,	"Interface Power"              },
	{ UDESC_OTG,			"OTG"                          },
	{ UDESC_CS_DEVICE,		"Class-specific Device"        },
	{ UDESC_CS_CONFIG,		"Class-specific Configuration" },
	{ UDESC_CS_STRING,		"Class-specific String"        },
	{ UDESC_CS_INTERFACE,		"Class-specific Interface"     },
	{ UDESC_CS_ENDPOINT,		"Class-specific Endpoint"      },
	{ UDESC_HUB,			"Hub"                          },
	{ 0, NULL }
};

static const char *
usb_enum_string(const struct usb_enum_str *tab, int code)
{
	static char buf[16];

	while (tab->str != NULL) {
		if (tab->code == code)
			return tab->str;
		tab++;
	}

	(void)snprintf(buf, sizeof buf, "0x%02x", code);
	return buf;
}

static const char *
usbf_request_code_string(usb_device_request_t *req)
{
	static char buf[32];

	(void)snprintf(buf, sizeof buf, "%s",
	    usb_enum_string(usb_request_str, req->bRequest));
	return buf;
}

static const char *
usbf_request_type_string(usb_device_request_t *req)
{
	static char buf[32];

	(void)snprintf(buf, sizeof buf, "%s",
	    usb_enum_string(usb_request_type_str, req->bmRequestType));
	return buf;
}

static const char *
usbf_request_desc_string(usb_device_request_t *req)
{
	static char buf[32];
	u_int8_t type = UGETW(req->wValue) >> 8;
	u_int8_t index = UGETW(req->wValue) & 0xff;

	(void)snprintf(buf, sizeof buf, "%s/%u",
	    usb_enum_string(usb_request_desc_str, type), index);
	return buf;
}

void
usbf_dump_request(usbf_device_handle dev, usb_device_request_t *req)
{
	struct usbf_softc *sc = dev->bus->usbfctl;

	printf("%s: %s request %s\n",
	    DEVNAME(sc), usbf_request_type_string(req),
	    usbf_request_code_string(req));

	if (req->bRequest == UR_GET_DESCRIPTOR)
		printf("%s:    VALUE:  0x%04x (%s)\n", DEVNAME(sc),
		    UGETW(req->wValue), usbf_request_desc_string(req));
	else
		printf("%s:    VALUE:  0x%04x\n", DEVNAME(sc),
		    UGETW(req->wValue));

	printf("%s:    INDEX:  0x%04x\n", DEVNAME(sc), UGETW(req->wIndex));
	printf("%s:    LENGTH: 0x%04x\n", DEVNAME(sc), UGETW(req->wLength));
}
#endif
