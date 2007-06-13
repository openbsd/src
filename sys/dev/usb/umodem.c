/*	$OpenBSD: umodem.c,v 1.32 2007/06/13 06:25:03 mbalmer Exp $ */
/*	$NetBSD: umodem.c,v 1.45 2002/09/23 05:51:23 simonb Exp $	*/

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
 * Comm Class spec:  http://www.usb.org/developers/devclass_docs/usbccs10.pdf
 *                   http://www.usb.org/developers/devclass_docs/usbcdc11.pdf
 */

/*
 * TODO:
 * - Add error recovery in various places; the big problem is what
 *   to do in a callback if there is an error.
 * - Implement a Call Device for modems without multiplexed commands.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/ucomvar.h>

#ifdef UMODEM_DEBUG
#define DPRINTFN(n, x)	do { if (umodemdebug > (n)) printf x; } while (0)
int	umodemdebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

/*
 * These are the maximum number of bytes transferred per frame.
 * Buffers are this large to deal with high speed wireless devices.
 * Capped at 1024 as ttymalloc() is limited to this amount.
 */
#define UMODEMIBUFSIZE 1024
#define UMODEMOBUFSIZE 1024

struct umodem_softc {
	struct device		 sc_dev;	/* base device */

	usbd_device_handle	 sc_udev;	/* USB device */

	int			 sc_ctl_iface_no;
	usbd_interface_handle	 sc_ctl_iface;	/* control interface */
	int			 sc_data_iface_no;
	usbd_interface_handle	 sc_data_iface;	/* data interface */

	int			 sc_cm_cap;	/* CM capabilities */
	int			 sc_acm_cap;	/* ACM capabilities */

	int			 sc_cm_over_data;

	usb_cdc_line_state_t	 sc_line_state;	/* current line state */
	u_char			 sc_dtr;	/* current DTR state */
	u_char			 sc_rts;	/* current RTS state */

	struct device		*sc_subdev;	/* ucom device */

	u_char			 sc_opening;	/* lock during open */
	u_char			 sc_dying;	/* disconnecting */

	int			 sc_ctl_notify;	/* Notification endpoint */
	usbd_pipe_handle	 sc_notify_pipe; /* Notification pipe */
	usb_cdc_notification_t	 sc_notify_buf;	/* Notification structure */
	u_char			 sc_lsr;	/* Local status register */
	u_char			 sc_msr;	/* Modem status register */
};

usbd_status umodem_set_comm_feature(struct umodem_softc *sc,
					   int feature, int state);
usbd_status umodem_set_line_coding(struct umodem_softc *sc,
					  usb_cdc_line_state_t *state);

void	umodem_get_status(void *, int portno, u_char *lsr, u_char *msr);
void	umodem_set(void *, int, int, int);
void	umodem_dtr(struct umodem_softc *, int);
void	umodem_rts(struct umodem_softc *, int);
void	umodem_break(struct umodem_softc *, int);
void	umodem_set_line_state(struct umodem_softc *);
int	umodem_param(void *, int, struct termios *);
int	umodem_ioctl(void *, int, u_long, caddr_t, int, struct proc *);
int	umodem_open(void *, int portno);
void	umodem_close(void *, int portno);
void	umodem_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

struct ucom_methods umodem_methods = {
	umodem_get_status,
	umodem_set,
	umodem_param,
	umodem_ioctl,
	umodem_open,
	umodem_close,
	NULL,
	NULL,
};

USB_DECLARE_DRIVER(umodem);

int
umodem_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	usb_interface_descriptor_t *id;
	usb_device_descriptor_t *dd;
	int ret;

	if (uaa->iface == NULL)
		return (UMATCH_NONE);

	id = usbd_get_interface_descriptor(uaa->iface);
	dd = usbd_get_device_descriptor(uaa->device);
	if (id == NULL || dd == NULL)
		return (UMATCH_NONE);

	ret = UMATCH_NONE;
	if (UGETW(dd->idVendor) == USB_VENDOR_KYOCERA &&
	    UGETW(dd->idProduct) == USB_PRODUCT_KYOCERA_AHK3001V &&
	    id->bInterfaceNumber == 0)
		ret = UMATCH_VENDOR_PRODUCT;

	if (ret == UMATCH_NONE &&
	    id->bInterfaceClass == UICLASS_CDC &&
	    id->bInterfaceSubClass == UISUBCLASS_ABSTRACT_CONTROL_MODEL &&
	    id->bInterfaceProtocol == UIPROTO_CDC_AT)
		ret = UMATCH_IFACECLASS_IFACESUBCLASS_IFACEPROTO;

	return (ret);
}

void
umodem_attach(struct device *parent, struct device *self, void *aux)
{
	struct umodem_softc *sc = (struct umodem_softc *)self;
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usb_cdc_cm_descriptor_t *cmd;      
	usb_interface_descriptor_t *idesc;
	const usb_cdc_acm_descriptor_t *acmd;
	const usb_cdc_union_descriptor_t *uniond;
	const usb_descriptor_t *desc;
	usbd_desc_iter_t iter;
	char *devinfop;
	usbd_status err;
	int current_iface_no = -1;
	int i;
	struct ucom_attach_args uca;

	devinfop = usbd_devinfo_alloc(dev, 0);
	printf("\n");

	sc->sc_udev = dev;
	sc->sc_ctl_iface = uaa->iface;

	id = usbd_get_interface_descriptor(sc->sc_ctl_iface);
	printf("%s: %s, iclass %d/%d\n", sc->sc_dev.dv_xname,
	       devinfop, id->bInterfaceClass, id->bInterfaceSubClass);
	usbd_devinfo_free(devinfop);
	sc->sc_ctl_iface_no = id->bInterfaceNumber;

	/* Get the data interface no. and capabilities */
	sc->sc_cm_cap = 0;
	sc->sc_data_iface_no = 0;
	sc->sc_acm_cap = 0;
	usb_desc_iter_init(dev, &iter);
	desc = usb_desc_iter_next(&iter);
	while (desc) {
		if (desc->bDescriptorType == UDESC_INTERFACE) {
		    idesc = (usb_interface_descriptor_t *)desc;
		    current_iface_no = idesc->bInterfaceNumber;
		}
		if (current_iface_no == sc->sc_ctl_iface_no &&
		    desc->bDescriptorType == UDESC_CS_INTERFACE) {
			switch(desc->bDescriptorSubtype) {
			case UDESCSUB_CDC_CM:
				cmd = (usb_cdc_cm_descriptor_t *)desc;
				sc->sc_cm_cap = cmd->bmCapabilities;
				sc->sc_data_iface_no = cmd->bDataInterface;
				break;
			case UDESCSUB_CDC_ACM:
				acmd = (usb_cdc_acm_descriptor_t *)desc;
				sc->sc_acm_cap = acmd->bmCapabilities;
				break;
			case UDESCSUB_CDC_UNION:
				uniond = (usb_cdc_union_descriptor_t *)desc;
				sc->sc_data_iface_no =
				    uniond->bSlaveInterface[0];
				break;
			}
		}
		desc = usb_desc_iter_next(&iter);
	}

	if (sc->sc_data_iface_no == 0) {
		printf("%s: no pointer to data interface\n",
		       sc->sc_dev.dv_xname);
		goto bad;
	}

	printf("%s: data interface %d, has %sCM over data, has %sbreak\n",
	       sc->sc_dev.dv_xname, sc->sc_data_iface_no,
	       sc->sc_cm_cap & USB_CDC_CM_OVER_DATA ? "" : "no ",
	       sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK ? "" : "no ");

	/* Get the data interface too. */
	for (i = 0; i < uaa->nifaces; i++) {
		if (uaa->ifaces[i] != NULL) {
			id = usbd_get_interface_descriptor(uaa->ifaces[i]);
			if (id != NULL &&
                            id->bInterfaceNumber == sc->sc_data_iface_no) {
				sc->sc_data_iface = uaa->ifaces[i];
				uaa->ifaces[i] = NULL;
			}
		}
	}
	if (sc->sc_data_iface == NULL) {
		printf("%s: no data interface\n", sc->sc_dev.dv_xname);
		goto bad;
	}

	/*
	 * Find the bulk endpoints.
	 * Iterate over all endpoints in the data interface and take note.
	 */
	uca.bulkin = uca.bulkout = -1;

	id = usbd_get_interface_descriptor(sc->sc_data_iface);
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_data_iface, i);
		if (ed == NULL) {
			printf("%s: no endpoint descriptor for %d\n",
				sc->sc_dev.dv_xname, i);
			goto bad;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
                        uca.bulkin = ed->bEndpointAddress;
                } else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
                        uca.bulkout = ed->bEndpointAddress;
                }
        }

	if (uca.bulkin == -1) {
		printf("%s: Could not find data bulk in\n",
		       sc->sc_dev.dv_xname);
		goto bad;
	}
	if (uca.bulkout == -1) {
		printf("%s: Could not find data bulk out\n",
			sc->sc_dev.dv_xname);
		goto bad;
	}

	if (usbd_get_quirks(sc->sc_udev)->uq_flags & UQ_ASSUME_CM_OVER_DATA) {
		sc->sc_cm_over_data = 1;
	} else {
		if (sc->sc_cm_cap & USB_CDC_CM_OVER_DATA) {
			if (sc->sc_acm_cap & USB_CDC_ACM_HAS_FEATURE)
				err = umodem_set_comm_feature(sc,
				    UCDC_ABSTRACT_STATE, UCDC_DATA_MULTIPLEXED);
			else
				err = 0;
			if (err) {
				printf("%s: could not set data multiplex mode\n",
				       sc->sc_dev.dv_xname);
				goto bad;
			}
			sc->sc_cm_over_data = 1;
		}
	}

	/*
	 * The standard allows for notification messages (to indicate things
	 * like a modem hangup) to come in via an interrupt endpoint
	 * off of the control interface.  Iterate over the endpoints on
	 * the control interface and see if there are any interrupt
	 * endpoints; if there are, then register it.
	 */

	sc->sc_ctl_notify = -1;
	sc->sc_notify_pipe = NULL;

	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_ctl_iface, i);
		if (ed == NULL)
			continue;

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			printf("%s: status change notification available\n",
			       sc->sc_dev.dv_xname);
			sc->sc_ctl_notify = ed->bEndpointAddress;
		}
	}

	sc->sc_dtr = -1;

	uca.portno = UCOM_UNK_PORTNO;
	/* bulkin, bulkout set above */
	uca.ibufsize = UMODEMIBUFSIZE;
	uca.obufsize = UMODEMOBUFSIZE;
	uca.ibufsizepad = UMODEMIBUFSIZE;
	uca.opkthdrlen = 0;
	uca.device = sc->sc_udev;
	uca.iface = sc->sc_data_iface;
	uca.methods = &umodem_methods;
	uca.arg = sc;
	uca.info = NULL;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   &sc->sc_dev);

	DPRINTF(("umodem_attach: sc=%p\n", sc));
	sc->sc_subdev = config_found_sm(self, &uca, ucomprint, ucomsubmatch);

	return;

 bad:
	sc->sc_dying = 1;
}

int
umodem_open(void *addr, int portno)
{
	struct umodem_softc *sc = addr;
	int err;

	DPRINTF(("umodem_open: sc=%p\n", sc));

	if (sc->sc_ctl_notify != -1 && sc->sc_notify_pipe == NULL) {
		err = usbd_open_pipe_intr(sc->sc_ctl_iface, sc->sc_ctl_notify,
		    USBD_SHORT_XFER_OK, &sc->sc_notify_pipe, sc,
		    &sc->sc_notify_buf, sizeof(sc->sc_notify_buf),
		    umodem_intr, USBD_DEFAULT_INTERVAL);

		if (err) {
			DPRINTF(("Failed to establish notify pipe: %s\n",
				usbd_errstr(err)));
			return EIO;
		}
	}

	return 0;
}

void
umodem_close(void *addr, int portno)
{
	struct umodem_softc *sc = addr;
	int err;

	DPRINTF(("umodem_close: sc=%p\n", sc));

	if (sc->sc_notify_pipe != NULL) {
		err = usbd_abort_pipe(sc->sc_notify_pipe);
		if (err)
			printf("%s: abort notify pipe failed: %s\n",
			    sc->sc_dev.dv_xname, usbd_errstr(err));
		err = usbd_close_pipe(sc->sc_notify_pipe);
		if (err)
			printf("%s: close notify pipe failed: %s\n",
			    sc->sc_dev.dv_xname, usbd_errstr(err));
		sc->sc_notify_pipe = NULL;
	}
}

void
umodem_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct umodem_softc *sc = priv;
	u_char mstatus;

	if (sc->sc_dying)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		printf("%s: abnormal status: %s\n", sc->sc_dev.dv_xname,
		       usbd_errstr(status));
		return;
	}

	if (sc->sc_notify_buf.bmRequestType != UCDC_NOTIFICATION) {
		DPRINTF(("%s: unknown message type (%02x) on notify pipe\n",
			 sc->sc_dev.dv_xname,
			 sc->sc_notify_buf.bmRequestType));
		return;
	}

	switch (sc->sc_notify_buf.bNotification) {
	case UCDC_N_SERIAL_STATE:
		/*
		 * Set the serial state in ucom driver based on
		 * the bits from the notify message
		 */
		if (UGETW(sc->sc_notify_buf.wLength) != 2) {
			printf("%s: Invalid notification length! (%d)\n",
			       sc->sc_dev.dv_xname,
			       UGETW(sc->sc_notify_buf.wLength));
			break;
		}
		DPRINTF(("%s: notify bytes = %02x%02x\n",
			 sc->sc_dev.dv_xname,
			 sc->sc_notify_buf.data[0],
			 sc->sc_notify_buf.data[1]));
		/* Currently, lsr is always zero. */
		sc->sc_lsr = sc->sc_msr = 0;
		mstatus = sc->sc_notify_buf.data[0];

		if (ISSET(mstatus, UCDC_N_SERIAL_RI))
			sc->sc_msr |= UMSR_RI;
		if (ISSET(mstatus, UCDC_N_SERIAL_DSR))
			sc->sc_msr |= UMSR_DSR;
		if (ISSET(mstatus, UCDC_N_SERIAL_DCD))
			sc->sc_msr |= UMSR_DCD;
		ucom_status_change((struct ucom_softc *)sc->sc_subdev);
		break;
	default:
		DPRINTF(("%s: unknown notify message: %02x\n",
			 sc->sc_dev.dv_xname,
			 sc->sc_notify_buf.bNotification));
		break;
	}
}

void
umodem_get_status(void *addr, int portno, u_char *lsr, u_char *msr)
{
	struct umodem_softc *sc = addr;

	DPRINTF(("umodem_get_status:\n"));

	if (lsr != NULL)
		*lsr = sc->sc_lsr;
	if (msr != NULL)
		*msr = sc->sc_msr;
}

int
umodem_param(void *addr, int portno, struct termios *t)
{
	struct umodem_softc *sc = addr;
	usbd_status err;
	usb_cdc_line_state_t ls;

	DPRINTF(("umodem_param: sc=%p\n", sc));

	USETDW(ls.dwDTERate, t->c_ospeed);
	if (ISSET(t->c_cflag, CSTOPB))
		ls.bCharFormat = UCDC_STOP_BIT_2;
	else
		ls.bCharFormat = UCDC_STOP_BIT_1;
	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			ls.bParityType = UCDC_PARITY_ODD;
		else
			ls.bParityType = UCDC_PARITY_EVEN;
	} else
		ls.bParityType = UCDC_PARITY_NONE;
	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		ls.bDataBits = 5;
		break;
	case CS6:
		ls.bDataBits = 6;
		break;
	case CS7:
		ls.bDataBits = 7;
		break;
	case CS8:
		ls.bDataBits = 8;
		break;
	}

	err = umodem_set_line_coding(sc, &ls);
	if (err) {
		DPRINTF(("umodem_param: err=%s\n", usbd_errstr(err)));
		return (1);
	}
	return (0);
}

int
umodem_ioctl(void *addr, int portno, u_long cmd, caddr_t data, int flag,
	     struct proc *p)
{
	struct umodem_softc *sc = addr;
	int error = 0;

	if (sc->sc_dying)
		return (EIO);

	DPRINTF(("umodemioctl: cmd=0x%08lx\n", cmd));

	switch (cmd) {
	case USB_GET_CM_OVER_DATA:
		*(int *)data = sc->sc_cm_over_data;
		break;

	case USB_SET_CM_OVER_DATA:
		if (*(int *)data != sc->sc_cm_over_data) {
			/* XXX change it */
		}
		break;

	default:
		DPRINTF(("umodemioctl: unknown\n"));
		error = ENOTTY;
		break;
	}

	return (error);
}

void
umodem_dtr(struct umodem_softc *sc, int onoff)
{
	DPRINTF(("umodem_modem: onoff=%d\n", onoff));

	if (sc->sc_dtr == onoff)
		return;
	sc->sc_dtr = onoff;

	umodem_set_line_state(sc);
}

void
umodem_rts(struct umodem_softc *sc, int onoff)
{
	DPRINTF(("umodem_modem: onoff=%d\n", onoff));

	if (sc->sc_rts == onoff)
		return;
	sc->sc_rts = onoff;

	umodem_set_line_state(sc);
}

void
umodem_set_line_state(struct umodem_softc *sc)
{
	usb_device_request_t req;
	int ls;

	ls = (sc->sc_dtr ? UCDC_LINE_DTR : 0) |
	     (sc->sc_rts ? UCDC_LINE_RTS : 0);
	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, ls);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);

	(void)usbd_do_request(sc->sc_udev, &req, 0);

}

void
umodem_break(struct umodem_softc *sc, int onoff)
{
	usb_device_request_t req;

	DPRINTF(("umodem_break: onoff=%d\n", onoff));

	if (!(sc->sc_acm_cap & USB_CDC_ACM_HAS_BREAK))
		return;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_BREAK;
	USETW(req.wValue, onoff ? UCDC_BREAK_ON : UCDC_BREAK_OFF);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, 0);

	(void)usbd_do_request(sc->sc_udev, &req, 0);
}

void
umodem_set(void *addr, int portno, int reg, int onoff)
{
	struct umodem_softc *sc = addr;

	switch (reg) {
	case UCOM_SET_DTR:
		umodem_dtr(sc, onoff);
		break;
	case UCOM_SET_RTS:
		umodem_rts(sc, onoff);
		break;
	case UCOM_SET_BREAK:
		umodem_break(sc, onoff);
		break;
	default:
		break;
	}
}

usbd_status
umodem_set_line_coding(struct umodem_softc *sc, usb_cdc_line_state_t *state)
{
	usb_device_request_t req;
	usbd_status err;

	DPRINTF(("umodem_set_line_coding: rate=%d fmt=%d parity=%d bits=%d\n",
		 UGETDW(state->dwDTERate), state->bCharFormat,
		 state->bParityType, state->bDataBits));

	if (memcmp(state, &sc->sc_line_state, UCDC_LINE_STATE_LENGTH) == 0) {
		DPRINTF(("umodem_set_line_coding: already set\n"));
		return (USBD_NORMAL_COMPLETION);
	}

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_LINE_CODING;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, UCDC_LINE_STATE_LENGTH);

	err = usbd_do_request(sc->sc_udev, &req, state);
	if (err) {
		DPRINTF(("umodem_set_line_coding: failed, err=%s\n",
			 usbd_errstr(err)));
		return (err);
	}

	sc->sc_line_state = *state;

	return (USBD_NORMAL_COMPLETION);
}

usbd_status
umodem_set_comm_feature(struct umodem_softc *sc, int feature, int state)
{
	usb_device_request_t req;
	usbd_status err;
	usb_cdc_abstract_state_t ast;

	DPRINTF(("umodem_set_comm_feature: feature=%d state=%d\n", feature,
		 state));

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_COMM_FEATURE;
	USETW(req.wValue, feature);
	USETW(req.wIndex, sc->sc_ctl_iface_no);
	USETW(req.wLength, UCDC_ABSTRACT_STATE_LENGTH);
	USETW(ast.wState, state);

	err = usbd_do_request(sc->sc_udev, &req, &ast);
	if (err) {
		DPRINTF(("umodem_set_comm_feature: feature=%d, err=%s\n",
			 feature, usbd_errstr(err)));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

int
umodem_activate(struct device *self, enum devact act)
{
	struct umodem_softc *sc = (struct umodem_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if (sc->sc_subdev)
			rv = config_deactivate(sc->sc_subdev);
		break;
	}
	return (rv);
}

int
umodem_detach(struct device *self, int flags)
{
	struct umodem_softc *sc = (struct umodem_softc *)self;
	int rv = 0;

	DPRINTF(("umodem_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;

	if (sc->sc_subdev != NULL)
		rv = config_detach(sc->sc_subdev, flags);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   &sc->sc_dev);

	return (rv);
}
