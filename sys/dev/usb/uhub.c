/*	$OpenBSD: uhub.c,v 1.58 2011/07/03 15:47:17 matthew Exp $ */
/*	$NetBSD: uhub.c,v 1.64 2003/02/08 03:32:51 ichiro Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/uhub.c,v 1.18 1999/11/17 22:33:43 n_hibma Exp $	*/

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

/*
 * USB spec: http://www.usb.org/developers/docs/usbspec.zip
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>

#define UHUB_INTR_INTERVAL 255	/* ms */

#ifdef UHUB_DEBUG
#define DPRINTF(x)	do { if (uhubdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (uhubdebug>(n)) printf x; } while (0)
int	uhubdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct uhub_softc {
	struct device		sc_dev;		/* base device */
	usbd_device_handle	sc_hub;		/* USB device */
	usbd_pipe_handle	sc_ipipe;	/* interrupt pipe */
	u_int8_t		*sc_statusbuf;	/* per port status buffer */
	size_t			sc_statuslen;	/* status bufferlen */
	u_char			sc_running;
};
#define UHUB_PROTO(sc) ((sc)->sc_hub->ddesc.bDeviceProtocol)
#define UHUB_IS_HIGH_SPEED(sc) (UHUB_PROTO(sc) != UDPROTO_FSHUB)
#define UHUB_IS_SINGLE_TT(sc) (UHUB_PROTO(sc) == UDPROTO_HSHUBSTT)

usbd_status uhub_explore(usbd_device_handle hub);
void uhub_intr(usbd_xfer_handle, usbd_private_handle,usbd_status);

/*
 * We need two attachment points:
 * hub to usb and hub to hub
 * Every other driver only connects to hubs
 */

int uhub_match(struct device *, void *, void *); 
void uhub_attach(struct device *, struct device *, void *); 
int uhub_detach(struct device *, int); 
int uhub_activate(struct device *, int); 

struct cfdriver uhub_cd = { 
	NULL, "uhub", DV_DULL 
}; 

const struct cfattach uhub_ca = { 
	sizeof(struct uhub_softc), 
	uhub_match, 
	uhub_attach, 
	uhub_detach, 
	uhub_activate, 
};

struct cfattach uhub_uhub_ca = {
	sizeof(struct uhub_softc), uhub_match, uhub_attach,
	uhub_detach, uhub_activate
};

int
uhub_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;
	usb_device_descriptor_t *dd = usbd_get_device_descriptor(uaa->device);

	DPRINTFN(5,("uhub_match, dd=%p\n", dd));
	/*
	 * The subclass for hubs seems to be 0 for some and 1 for others,
	 * so we just ignore the subclass.
	 */
	if (uaa->iface == NULL && dd->bDeviceClass == UDCLASS_HUB)
		return (UMATCH_DEVCLASS_DEVSUBCLASS);
	return (UMATCH_NONE);
}

void
uhub_attach(struct device *parent, struct device *self, void *aux)
{
	struct uhub_softc *sc = (struct uhub_softc *)self;
	struct usb_attach_arg *uaa = aux;
	usbd_device_handle dev = uaa->device;
	usbd_status err;
	struct usbd_hub *hub = NULL;
	usb_device_request_t req;
	usb_hub_descriptor_t hubdesc;
	int p, port, nports, nremov, pwrdly;
	usbd_interface_handle iface;
	usb_endpoint_descriptor_t *ed;
	struct usbd_tt *tts = NULL;

	DPRINTFN(1,("uhub_attach\n"));
	sc->sc_hub = dev;

	err = usbd_set_config_index(dev, 0, 1);
	if (err) {
		DPRINTF(("%s: configuration failed, error=%s\n",
			 sc->sc_dev.dv_xname, usbd_errstr(err)));
		return;
	}

	if (dev->depth > USB_HUB_MAX_DEPTH) {
		printf("%s: hub depth (%d) exceeded, hub ignored\n",
		       sc->sc_dev.dv_xname, USB_HUB_MAX_DEPTH);
		return;
	}

	/* Get hub descriptor. */
	req.bmRequestType = UT_READ_CLASS_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_HUB, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, USB_HUB_DESCRIPTOR_SIZE);
	DPRINTFN(1,("usb_init_hub: getting hub descriptor\n"));
	err = usbd_do_request(dev, &req, &hubdesc);
	nports = hubdesc.bNbrPorts;
	if (!err && nports > 7) {
		USETW(req.wLength, USB_HUB_DESCRIPTOR_SIZE + (nports+1) / 8);
		err = usbd_do_request(dev, &req, &hubdesc);
	}
	if (err) {
		DPRINTF(("%s: getting hub descriptor failed, error=%s\n",
			 sc->sc_dev.dv_xname, usbd_errstr(err)));
		return;
	}

	for (nremov = 0, port = 1; port <= nports; port++)
		if (!UHD_NOT_REMOV(&hubdesc, port))
			nremov++;

#ifdef UHUB_DEBUG
	printf("%s: %d port%s with %d removable, %s powered",
	       sc->sc_dev.dv_xname, nports, nports != 1 ? "s" : "",
	       nremov, dev->self_powered ? "self" : "bus");

	if (dev->depth > 0 && UHUB_IS_HIGH_SPEED(sc)) {
		printf(", %s transaction translator%s",
		    UHUB_IS_SINGLE_TT(sc) ? "single" : "multiple",
		    UHUB_IS_SINGLE_TT(sc) ? "" : "s");
	}
	printf("\n");
#endif

	if (nports == 0) {
		printf("%s: no ports, hub ignored\n", sc->sc_dev.dv_xname);
		goto bad;
	}

	hub = malloc(sizeof(*hub), M_USBDEV, M_NOWAIT);
	if (hub == NULL)
		return;
	hub->ports = malloc(sizeof(struct usbd_port) * nports,
	    M_USBDEV, M_NOWAIT);
	if (hub->ports == NULL) {
		free(hub, M_USBDEV);
		return;
	}
	dev->hub = hub;
	dev->hub->hubsoftc = sc;
	hub->explore = uhub_explore;
	hub->hubdesc = hubdesc;

	DPRINTFN(1,("usbhub_init_hub: selfpowered=%d, parent=%p, "
		    "parent->selfpowered=%d\n",
		 dev->self_powered, dev->powersrc->parent,
		 dev->powersrc->parent ?
		 dev->powersrc->parent->self_powered : 0));

	if (!dev->self_powered && dev->powersrc->parent != NULL &&
	    !dev->powersrc->parent->self_powered) {
		printf("%s: bus powered hub connected to bus powered hub, "
		       "ignored\n", sc->sc_dev.dv_xname);
		goto bad;
	}

	/* Set up interrupt pipe. */
	err = usbd_device2interface_handle(dev, 0, &iface);
	if (err) {
		printf("%s: no interface handle\n", sc->sc_dev.dv_xname);
		goto bad;
	}
	ed = usbd_interface2endpoint_descriptor(iface, 0);
	if (ed == NULL) {
		printf("%s: no endpoint descriptor\n", sc->sc_dev.dv_xname);
		goto bad;
	}
	if ((ed->bmAttributes & UE_XFERTYPE) != UE_INTERRUPT) {
		printf("%s: bad interrupt endpoint\n", sc->sc_dev.dv_xname);
		goto bad;
	}

	sc->sc_statuslen = (nports + 1 + 7) / 8;
	sc->sc_statusbuf = malloc(sc->sc_statuslen, M_USBDEV, M_NOWAIT);
	if (!sc->sc_statusbuf)
		goto bad;

	err = usbd_open_pipe_intr(iface, ed->bEndpointAddress,
		  USBD_SHORT_XFER_OK, &sc->sc_ipipe, sc, sc->sc_statusbuf,
		  sc->sc_statuslen, uhub_intr, UHUB_INTR_INTERVAL);
	if (err) {
		printf("%s: cannot open interrupt pipe\n",
		       sc->sc_dev.dv_xname);
		goto bad;
	}

	/* Wait with power off for a while. */
	usbd_delay_ms(dev, USB_POWER_DOWN_TIME);

	/*
	 * To have the best chance of success we do things in the exact same
	 * order as Windoze98.  This should not be necessary, but some
	 * devices do not follow the USB specs to the letter.
	 *
	 * These are the events on the bus when a hub is attached:
	 *  Get device and config descriptors (see attach code)
	 *  Get hub descriptor (see above)
	 *  For all ports
	 *     turn on power
	 *     wait for power to become stable
	 * (all below happens in explore code)
	 *  For all ports
	 *     clear C_PORT_CONNECTION
	 *  For all ports
	 *     get port status
	 *     if device connected
	 *        wait 100 ms
	 *        turn on reset
	 *        wait
	 *        clear C_PORT_RESET
	 *        get port status
	 *        proceed with device attachment
	 */

	if (UHUB_IS_HIGH_SPEED(sc)) {
		tts = malloc((UHUB_IS_SINGLE_TT(sc) ? 1 : nports) *
		    sizeof (struct usbd_tt), M_USBDEV, M_NOWAIT);
		if (!tts)
			goto bad;
	}
	/* Set up data structures */
	for (p = 0; p < nports; p++) {
		struct usbd_port *up = &hub->ports[p];
		up->device = NULL;
		up->parent = dev;
		up->portno = p+1;
		if (dev->self_powered)
			/* Self powered hub, give ports maximum current. */
			up->power = USB_MAX_POWER;
		else
			up->power = USB_MIN_POWER;
		up->restartcnt = 0;
		up->reattach = 0;
		if (UHUB_IS_HIGH_SPEED(sc)) {
			up->tt = &tts[UHUB_IS_SINGLE_TT(sc) ? 0 : p];
			up->tt->hub = hub;
		} else {
			up->tt = NULL;
		}
	}

	/* XXX should check for none, individual, or ganged power? */

	pwrdly = dev->hub->hubdesc.bPwrOn2PwrGood * UHD_PWRON_FACTOR
	    + USB_EXTRA_POWER_UP_TIME;
	for (port = 1; port <= nports; port++) {
		/* Turn the power on. */
		err = usbd_set_port_feature(dev, port, UHF_PORT_POWER);
		if (err)
			printf("%s: port %d power on failed, %s\n",
			       sc->sc_dev.dv_xname, port,
			       usbd_errstr(err));
		DPRINTF(("usb_init_port: turn on port %d power\n", port));
	}

	/* Wait for stable power.  Root hubs delay in their event thread. */
        if (dev->powersrc->parent != NULL)
		usbd_delay_ms(dev, pwrdly);

	/* The usual exploration will finish the setup. */

	sc->sc_running = 1;

	return;

 bad:
	if (sc->sc_statusbuf)
		free(sc->sc_statusbuf, M_USBDEV);
	if (hub->ports)
		free(hub->ports, M_USBDEV);
	if (hub)
		free(hub, M_USBDEV);
	dev->hub = NULL;
}

usbd_status
uhub_explore(usbd_device_handle dev)
{
	usb_hub_descriptor_t *hd = &dev->hub->hubdesc;
	struct uhub_softc *sc = dev->hub->hubsoftc;
	struct usbd_port *up;
	usbd_status err;
	int speed;
	int port;
	int change, status, reconnect;

	DPRINTFN(10, ("uhub_explore dev=%p addr=%d\n", dev, dev->address));

	if (usbd_is_dying(dev)) {
		DPRINTF(("%s: dying\n", __func__));
		return (USBD_IOERROR);
	}

	if (!sc->sc_running)
		return (USBD_NOT_STARTED);

	/* Ignore hubs that are too deep. */
	if (dev->depth > USB_HUB_MAX_DEPTH)
		return (USBD_TOO_DEEP);

	for (port = 1; port <= hd->bNbrPorts; port++) {
		up = &dev->hub->ports[port-1];
		err = usbd_get_port_status(dev, port, &up->status);
		if (err) {
			DPRINTF(("uhub_explore: get port status failed, "
				 "error=%s\n", usbd_errstr(err)));
			continue;
		}
		status = UGETW(up->status.wPortStatus);
		change = UGETW(up->status.wPortChange);
		reconnect = up->reattach;
		up->reattach = 0;
		DPRINTFN(3,("uhub_explore: %s port %d status 0x%04x 0x%04x\n",
			    sc->sc_dev.dv_xname, port, status, change));
		if (change & UPS_C_PORT_ENABLED) {
			DPRINTF(("uhub_explore: C_PORT_ENABLED\n"));
			usbd_clear_port_feature(dev, port, UHF_C_PORT_ENABLE);
			if (change & UPS_C_CONNECT_STATUS) {
				/* Ignore the port error if the device
				   vanished. */
			} else if (status & UPS_PORT_ENABLED) {
				printf("%s: illegal enable change, port %d\n",
				       sc->sc_dev.dv_xname, port);
			} else {
				/* Port error condition. */
				if (up->restartcnt) /* no message first time */
					printf("%s: port error, restarting "
					       "port %d\n",
					       sc->sc_dev.dv_xname, port);

				if (up->restartcnt++ < USBD_RESTART_MAX)
					goto disco;
				else
					printf("%s: port error, giving up "
					       "port %d\n",
					       sc->sc_dev.dv_xname, port);
			}
		}
		if (!reconnect && !(change & UPS_C_CONNECT_STATUS)) {
			DPRINTFN(3,("uhub_explore: port=%d !C_CONNECT_"
				    "STATUS\n", port));
			/* No status change, just do recursive explore. */
			if (up->device != NULL && up->device->hub != NULL)
				up->device->hub->explore(up->device);
#if 0 && defined(DIAGNOSTIC)
			if (up->device == NULL &&
			    (status & UPS_CURRENT_CONNECT_STATUS))
				printf("%s: connected, no device\n",
				       sc->sc_dev.dv_xname);
#endif
			continue;
		}

		/* We have a connect status change, handle it. */

		DPRINTF(("uhub_explore: status change hub=%d port=%d\n",
			 dev->address, port));
		usbd_clear_port_feature(dev, port, UHF_C_PORT_CONNECTION);
		/*usbd_clear_port_feature(dev, port, UHF_C_PORT_ENABLE);*/
		/*
		 * If there is already a device on the port the change status
		 * must mean that is has disconnected.  Looking at the
		 * current connect status is not enough to figure this out
		 * since a new unit may have been connected before we handle
		 * the disconnect.
		 */
	disco:
		if (up->device != NULL) {
			/* Disconnected */
			DPRINTF(("uhub_explore: device addr=%d disappeared "
				 "on port %d\n", up->device->address, port));
			usb_disconnect_port(up, &sc->sc_dev);
			usbd_clear_port_feature(dev, port,
						UHF_C_PORT_CONNECTION);
		}
		if (!(status & UPS_CURRENT_CONNECT_STATUS)) {
			/* Nothing connected, just ignore it. */
			DPRINTFN(3,("uhub_explore: port=%d !CURRENT_CONNECT"
				    "_STATUS\n", port));
			continue;
		}

		/* Connected */

		if (!(status & UPS_PORT_POWER))
			printf("%s: strange, connected port %d has no power\n",
			       sc->sc_dev.dv_xname, port);

		/* Wait for maximum device power up time. */
		usbd_delay_ms(dev, USB_PORT_POWERUP_DELAY);

		/* Reset port, which implies enabling it. */
		if (usbd_reset_port(dev, port, &up->status)) {
			printf("%s: port %d reset failed\n",
			       sc->sc_dev.dv_xname, port);
			continue;
		}
		/* Get port status again, it might have changed during reset */
		err = usbd_get_port_status(dev, port, &up->status);
		if (err) {
			DPRINTF(("uhub_explore: get port status failed, "
				 "error=%s\n", usbd_errstr(err)));
			continue;
		}
		status = UGETW(up->status.wPortStatus);
		change = UGETW(up->status.wPortChange);
		if (!(status & UPS_CURRENT_CONNECT_STATUS)) {
			/* Nothing connected, just ignore it. */
#ifdef UHUB_DEBUG
			printf("%s: port %d, device disappeared after reset\n",
			       sc->sc_dev.dv_xname, port);
#endif
			continue;
		}

		/* Figure out device speed */
		if (status & UPS_HIGH_SPEED)
			speed = USB_SPEED_HIGH;
		else if (status & UPS_LOW_SPEED)
			speed = USB_SPEED_LOW;
		else
			speed = USB_SPEED_FULL;
		/* Get device info and set its address. */
		err = usbd_new_device(&sc->sc_dev, dev->bus,
			  dev->depth + 1, speed, port, up);
		/* XXX retry a few times? */
		if (err) {
			DPRINTFN(-1,("uhub_explore: usbd_new_device failed, "
				     "error=%s\n", usbd_errstr(err)));
			/* Avoid addressing problems by disabling. */
			/* usbd_reset_port(dev, port, &up->status); */

			/*
			 * The unit refused to accept a new address, or had
			 * some other serious problem.  Since we cannot leave
			 * at 0 we have to disable the port instead.
			 */
			printf("%s: device problem, disabling port %d\n",
			       sc->sc_dev.dv_xname, port);
			usbd_clear_port_feature(dev, port, UHF_PORT_ENABLE);
		} else {
			/* The port set up succeeded, reset error count. */
			up->restartcnt = 0;

			if (up->device->hub)
				up->device->hub->explore(up->device);
		}
	}
	return (USBD_NORMAL_COMPLETION);
}

int
uhub_activate(struct device *self, int act)
{
	struct uhub_softc *sc = (struct uhub_softc *)self;
	struct usbd_hub *hub = sc->sc_hub->hub;
	usbd_device_handle dev;
	int nports, port, i;

	switch (act) {
	case DVACT_DEACTIVATE:
		if (hub == NULL) /* malfunctioning hub */
			break;
		nports = hub->hubdesc.bNbrPorts;
		for(port = 0; port < nports; port++) {
			dev = hub->ports[port].device;
			if (dev != NULL && dev->subdevs != NULL) {
				for (i = 0; dev->subdevs[i] != NULL; i++)
					config_deactivate(dev->subdevs[i]);
			}
		}
		break;
	}
	return (0);
}

/*
 * Called from process context when the hub is gone.
 * Detach all devices on active ports.
 */
int
uhub_detach(struct device *self, int flags)
{
	struct uhub_softc *sc = (struct uhub_softc *)self;
	struct usbd_hub *hub = sc->sc_hub->hub;
	struct usbd_port *rup;
	int port, nports;

	DPRINTF(("uhub_detach: sc=%p flags=%d\n", sc, flags));

	if (hub == NULL)		/* Must be partially working */
		return (0);

	usbd_abort_pipe(sc->sc_ipipe);
	usbd_close_pipe(sc->sc_ipipe);

	nports = hub->hubdesc.bNbrPorts;
	for(port = 0; port < nports; port++) {
		rup = &hub->ports[port];
		if (rup->device)
			usb_disconnect_port(rup, self);
	}

	if (hub->ports[0].tt)
		free(hub->ports[0].tt, M_USBDEV);
	if (sc->sc_statusbuf)
		free(sc->sc_statusbuf, M_USBDEV);
	if (hub->ports)
		free(hub->ports, M_USBDEV);
	free(hub, M_USBDEV);
	sc->sc_hub->hub = NULL;

	return (0);
}

/*
 * Hub interrupt.
 * This an indication that some port has changed status.
 * Notify the bus event handler thread that we need
 * to be explored again.
 */
void
uhub_intr(usbd_xfer_handle xfer, usbd_private_handle addr, usbd_status status)
{
	struct uhub_softc *sc = addr;

	DPRINTFN(5,("uhub_intr: sc=%p\n", sc));
	if (status == USBD_STALLED)
		usbd_clear_endpoint_stall_async(sc->sc_ipipe);
	else if (status == USBD_NORMAL_COMPLETION)
		usb_needs_explore(sc->sc_hub, 0);
	else
		DPRINTFN(8, ("uhub_intr: unknown status, %d\n", status));
}
