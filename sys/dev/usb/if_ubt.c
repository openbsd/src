/*	$OpenBSD: if_ubt.c,v 1.12 2007/05/28 15:48:02 fkr Exp $	*/

/*
 * ng_ubt.c
 *
 * Copyright (c) 2001-2002 Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: if_ubt.c,v 1.12 2007/05/28 15:48:02 fkr Exp $
 * $FreeBSD: src/sys/netgraph/bluetooth/drivers/ubt/ng_ubt.c,v 1.20 2004/10/12 23:33:46 emax Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_types.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/l2cap.h>
#include <netbt/bt.h>
#include <netbt/hci_var.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_ubtreg.h>

USB_DECLARE_DRIVER_CLASS(ubt, DV_IFNET);

Static usbd_status	ubt_request_start(struct ubt_softc *);
Static void		ubt_request_complete(usbd_xfer_handle,
			    usbd_private_handle, usbd_status);

Static usbd_status	ubt_intr_start(struct ubt_softc *);
Static void		ubt_intr_complete(usbd_xfer_handle,
			    usbd_private_handle, usbd_status);

Static usbd_status	ubt_bulk_in_start(struct ubt_softc *);
Static void		ubt_bulk_in_complete(usbd_xfer_handle,
			    usbd_private_handle, usbd_status);

Static usbd_status	ubt_bulk_out_start(struct ubt_softc *);
Static void		ubt_bulk_out_complete(usbd_xfer_handle,
			    usbd_private_handle, usbd_status);

Static usbd_status	ubt_isoc_in_start(struct ubt_softc *);
Static void		ubt_isoc_in_complete(usbd_xfer_handle,
			    usbd_private_handle, usbd_status);

Static usbd_status	ubt_isoc_out_start(struct ubt_softc *);
Static void		ubt_isoc_out_complete(usbd_xfer_handle,
			    usbd_private_handle, usbd_status);

Static void		ubt_reset(struct ubt_softc *);
Static void		ubt_stop(struct ubt_softc *);

Static int		ubt_rcvdata(struct ubt_softc *, struct mbuf *);

Static void		ubt_if_start(struct ifnet *);
Static int		ubt_if_ioctl(struct ifnet *, u_long, caddr_t);
Static int		ubt_if_init(struct ifnet *);
Static void		ubt_if_watchdog(struct ifnet *);

int
ubt_match(struct device *parent, void *match, void *aux)
{
#ifdef notyet
	/*
	 * If for some reason device should not be attached then put
	 * VendorID/ProductID pair into the list below. Currently I
	 * do not know of any such devices. The format is as follows:
	 *
	 *	{ VENDOR_ID, PRODUCT_ID },
	 *
	 * where VENDOR_ID and PRODUCT_ID are hex numbers.
	 */

	Static struct usb_devno const	ubt_ignored_devices[] = {
	};
#endif

	/*
	 * If device violates Bluetooth specification and has bDeviceClass,
	 * bDeviceSubClass and bDeviceProtocol set to wrong values then you
	 * could try to put VendorID/ProductID pair into the list below.
	 */

	Static struct usb_devno const	ubt_broken_devices[] = {
		{ USB_VENDOR_CSR, USB_PRODUCT_CSR_BLUECORE },
		{ USB_VENDOR_MSI, USB_PRODUCT_MSI_BLUETOOTH },
		{ USB_VENDOR_MSI, USB_PRODUCT_MSI_BLUETOOTH_2 }
	};

	struct usb_attach_arg *uaa = aux;
	usb_device_descriptor_t	*dd = usbd_get_device_descriptor(uaa->device);

	if (uaa->iface == NULL)
		return (UMATCH_NONE);
#ifdef notyet
	if (usb_lookup(ubt_ignored_devices, uaa->vendor, uaa->product))
		return (UMATCH_NONE);
#endif

	if (dd->bDeviceClass == UDCLASS_WIRELESS &&
	    dd->bDeviceSubClass == UDSUBCLASS_RF &&
	    dd->bDeviceProtocol == UDPROTO_BLUETOOTH)
		return (UMATCH_DEVCLASS_DEVSUBCLASS);

	if (usb_lookup(ubt_broken_devices, uaa->vendor, uaa->product))
		return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

void
ubt_attach(struct device *parent, struct device *self, void *aux)
{
	struct ubt_softc		*sc = (struct ubt_softc *)self;
	struct usb_attach_arg		*uaa = aux;
	usb_config_descriptor_t		*cd = NULL;
	usb_interface_descriptor_t	*id = NULL;
	usb_endpoint_descriptor_t	*ed = NULL;
	char				 *devinfop;
	usbd_status			 error;
	int				 i, ai, alt_no, isoc_in, isoc_out,
					 isoc_isize, isoc_osize;
	struct ifnet *ifp = &sc->sc_if;

	/* Get USB device info */
	sc->sc_udev = uaa->device;

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	printf("\n%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
	usbd_devinfo_free(devinfop);

	/*
	 * Initialize device softc structure
	 */

	/* State */
	sc->sc_debug = NG_UBT_WARN_LEVEL;
	sc->sc_flags = 0;
	NG_UBT_STAT_RESET(sc->sc_stat);

	/* Interfaces */
	sc->sc_iface0 = sc->sc_iface1 = NULL;

	/* Interrupt pipe */
	sc->sc_intr_ep = -1;
	sc->sc_intr_pipe = NULL;
	sc->sc_intr_xfer = NULL;
	sc->sc_intr_buffer = NULL;

	/* Control pipe */
	sc->sc_ctrl_xfer = NULL;
	sc->sc_ctrl_buffer = NULL;
	NG_BT_MBUFQ_INIT(&sc->sc_cmdq, UBT_DEFAULT_QLEN);

	/* Bulk-in pipe */
	sc->sc_bulk_in_ep = -1;
	sc->sc_bulk_in_pipe = NULL;
	sc->sc_bulk_in_xfer = NULL;
	sc->sc_bulk_in_buffer = NULL;

	/* Bulk-out pipe */
	sc->sc_bulk_out_ep = -1;
	sc->sc_bulk_out_pipe = NULL;
	sc->sc_bulk_out_xfer = NULL;
	sc->sc_bulk_out_buffer = NULL;
	NG_BT_MBUFQ_INIT(&sc->sc_aclq, UBT_DEFAULT_QLEN);

	/* Isoc-in pipe */
	sc->sc_isoc_in_ep = -1;
	sc->sc_isoc_in_pipe = NULL;
	sc->sc_isoc_in_xfer = NULL;

	/* Isoc-out pipe */
	sc->sc_isoc_out_ep = -1;
	sc->sc_isoc_out_pipe = NULL;
	sc->sc_isoc_out_xfer = NULL;
	sc->sc_isoc_size = -1;
	NG_BT_MBUFQ_INIT(&sc->sc_scoq, UBT_DEFAULT_QLEN);

	/*
	 * XXX set configuration?
	 *
	 * Configure Bluetooth USB device. Discover all required USB interfaces
	 * and endpoints.
	 *
	 * USB device must present two interfaces:
	 * 1) Interface 0 that has 3 endpoints
	 *	1) Interrupt endpoint to receive HCI events
	 *	2) Bulk IN endpoint to receive ACL data
	 *	3) Bulk OUT endpoint to send ACL data
	 *
	 * 2) Interface 1 then has 2 endpoints
	 *	1) Isochronous IN endpoint to receive SCO data
 	 *	2) Isochronous OUT endpoint to send SCO data
	 *
	 * Interface 1 (with isochronous endpoints) has several alternate
	 * configurations with different packet size.
	 */

	/*
	 * Interface 0
	 */

	error = usbd_device2interface_handle(sc->sc_udev, 0, &sc->sc_iface0);
	if (error || sc->sc_iface0 == NULL) {
		printf("%s: Could not get interface 0 handle. %s (%d), " \
			"handle=%p\n", USBDEVNAME(sc->sc_dev),
			usbd_errstr(error), error, sc->sc_iface0);
		return;
	}

	id = usbd_get_interface_descriptor(sc->sc_iface0);
	if (id == NULL) {
		printf("%s: Could not get interface 0 descriptor\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}

	for (i = 0; i < id->bNumEndpoints; i ++) {
		ed = usbd_interface2endpoint_descriptor(sc->sc_iface0, i);
		if (ed == NULL) {
			printf("%s: Could not read endpoint descriptor for " \
				"interface 0, i=%d\n", USBDEVNAME(sc->sc_dev),
				i);
			return;
		}

		switch (UE_GET_XFERTYPE(ed->bmAttributes)) {
		case UE_BULK:
			if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN)
				sc->sc_bulk_in_ep = ed->bEndpointAddress;
			else
				sc->sc_bulk_out_ep = ed->bEndpointAddress;
			break;

		case UE_INTERRUPT:
			sc->sc_intr_ep = ed->bEndpointAddress;
			break;
		}
	}

	/* Check if we got everything we wanted on Interface 0 */
	if (sc->sc_intr_ep == -1) {
		printf("%s: Could not detect interrupt endpoint\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	if (sc->sc_bulk_in_ep == -1) {
		printf("%s: Could not detect bulk-in endpoint\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	if (sc->sc_bulk_out_ep == -1) {
		printf("%s: Could not detect bulk-out endpoint\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}

	printf("%s: Interface 0 endpoints: interrupt=%#x, bulk-in=%#x, " \
		"bulk-out=%#x\n", USBDEVNAME(sc->sc_dev),
		sc->sc_intr_ep, sc->sc_bulk_in_ep, sc->sc_bulk_out_ep);

	/*
	 * Interface 1
	 */

	cd = usbd_get_config_descriptor(sc->sc_udev);
	if (cd == NULL) {
		printf("%s: Could not get device configuration descriptor\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}

	error = usbd_device2interface_handle(sc->sc_udev, 1, &sc->sc_iface1);
	if (error || sc->sc_iface1 == NULL) {
		printf("%s: Could not get interface 1 handle. %s (%d), " \
			"handle=%p\n", USBDEVNAME(sc->sc_dev),
			usbd_errstr(error), error, sc->sc_iface1);
		return;
	}

	id = usbd_get_interface_descriptor(sc->sc_iface1);
	if (id == NULL) {
		printf("%s: Could not get interface 1 descriptor\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}

	/*
	 * Scan all alternate configurations for interface 1
	 */

	alt_no = -1;

	for (ai = 0; ai < usbd_get_no_alts(cd, 1); ai++)  {
		error = usbd_set_interface(sc->sc_iface1, ai);
		if (error) {
			printf("%s: [SCAN] Could not set alternate " \
				"configuration %d for interface 1. %s (%d)\n",
				USBDEVNAME(sc->sc_dev),  ai, usbd_errstr(error),
				error);
			return;
		}
		id = usbd_get_interface_descriptor(sc->sc_iface1);
		if (id == NULL) {
			printf("%s: Could not get interface 1 descriptor for " \
				"alternate configuration %d\n",
				USBDEVNAME(sc->sc_dev), ai);
			return;
		}

		isoc_in = isoc_out = -1;
		isoc_isize = isoc_osize = 0;

		for (i = 0; i < id->bNumEndpoints; i ++) {
			ed = usbd_interface2endpoint_descriptor(sc->sc_iface1, i);
			if (ed == NULL) {
				printf("%s: Could not read endpoint " \
					"descriptor for interface 1, " \
					"alternate configuration %d, i=%d\n",
					USBDEVNAME(sc->sc_dev), ai, i);
				return;
			}

			if (UE_GET_XFERTYPE(ed->bmAttributes) != UE_ISOCHRONOUS)
				continue;

			if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN) {
				isoc_in = ed->bEndpointAddress;
				isoc_isize = UGETW(ed->wMaxPacketSize);
			} else {
				isoc_out = ed->bEndpointAddress;
				isoc_osize = UGETW(ed->wMaxPacketSize);
			}
		}

		/*
		 * Make sure that configuration looks sane and if so
		 * update current settings
		 */

		if (isoc_in != -1 && isoc_out != -1 &&
		    isoc_isize > 0  && isoc_osize > 0 &&
		    isoc_isize == isoc_osize && isoc_isize > sc->sc_isoc_size) {
			sc->sc_isoc_in_ep = isoc_in;
			sc->sc_isoc_out_ep = isoc_out;
			sc->sc_isoc_size = isoc_isize;
			alt_no = ai;
		}
	}

	/* Check if we got everything we wanted on Interface 0 */
	if (sc->sc_isoc_in_ep == -1) {
		printf("%s: Could not detect isoc-in endpoint\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	if (sc->sc_isoc_out_ep == -1) {
		printf("%s: Could not detect isoc-out endpoint\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	if (sc->sc_isoc_size <= 0) {
		printf("%s: Invalid isoc. packet size=%d\n",
			USBDEVNAME(sc->sc_dev), sc->sc_isoc_size);
		return;
	}

	error = usbd_set_interface(sc->sc_iface1, alt_no);
	if (error) {
		printf("%s: Could not set alternate configuration " \
			"%d for interface 1. %s (%d)\n", USBDEVNAME(sc->sc_dev),
			alt_no, usbd_errstr(error), error);
		return;
	}

	/* Allocate USB transfer handles and buffers */
	sc->sc_ctrl_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_ctrl_xfer == NULL) {
		printf("%s: Could not allocate control xfer handle\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	sc->sc_ctrl_buffer = usbd_alloc_buffer(sc->sc_ctrl_xfer,
						UBT_CTRL_BUFFER_SIZE);
	if (sc->sc_ctrl_buffer == NULL) {
		printf("%s: Could not allocate control buffer\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}

	sc->sc_intr_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_intr_xfer == NULL) {
		printf("%s: Could not allocate interrupt xfer handle\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	sc->sc_intr_buffer = usbd_alloc_buffer(sc->sc_intr_xfer,
						UBT_INTR_BUFFER_SIZE);
	if (sc->sc_intr_buffer == NULL) {
		printf("%s: Could not allocate interrupt buffer\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}

	sc->sc_bulk_in_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_bulk_in_xfer == NULL) {
		printf("%s: Could not allocate bulk-in xfer handle\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	sc->sc_bulk_in_buffer = usbd_alloc_buffer(sc->sc_bulk_in_xfer,
						UBT_BULK_BUFFER_SIZE);
	if (sc->sc_bulk_in_buffer == NULL) {
		printf("%s: Could not allocate bulk-in buffer\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}

	sc->sc_bulk_out_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_bulk_out_xfer == NULL) {
		printf("%s: Could not allocate bulk-out xfer handle\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	sc->sc_bulk_out_buffer = usbd_alloc_buffer(sc->sc_bulk_out_xfer,
						UBT_BULK_BUFFER_SIZE);
	if (sc->sc_bulk_out_buffer == NULL) {
		printf("%s: Could not allocate bulk-out buffer\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}

	/*
	 * Allocate buffers for isoc. transfers
	 */

	sc->sc_isoc_nframes = (UBT_ISOC_BUFFER_SIZE / sc->sc_isoc_size) + 1;

	sc->sc_isoc_in_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_isoc_in_xfer == NULL) {
		printf("%s: Could not allocate isoc-in xfer handle\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	sc->sc_isoc_in_buffer = usbd_alloc_buffer(sc->sc_isoc_in_xfer,
					sc->sc_isoc_nframes * sc->sc_isoc_size);
	if (sc->sc_isoc_in_buffer == NULL) {
		printf("%s: Could not allocate isoc-in buffer\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	sc->sc_isoc_in_frlen = malloc(sizeof(u_int16_t) * sc->sc_isoc_nframes,
						M_USBDEV, M_NOWAIT);
	if (sc->sc_isoc_in_frlen == NULL) {
		printf("%s: Could not allocate isoc-in frame sizes buffer\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}

	sc->sc_isoc_out_xfer = usbd_alloc_xfer(sc->sc_udev);
	if (sc->sc_isoc_out_xfer == NULL) {
		printf("%s: Could not allocate isoc-out xfer handle\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	sc->sc_isoc_out_buffer = usbd_alloc_buffer(sc->sc_isoc_out_xfer,
					sc->sc_isoc_nframes * sc->sc_isoc_size);
	if (sc->sc_isoc_out_buffer == NULL) {
		printf("%s: Could not allocate isoc-out buffer\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}
	sc->sc_isoc_out_frlen = malloc(sizeof(u_int16_t) * sc->sc_isoc_nframes,
						M_USBDEV, M_NOWAIT);
	if (sc->sc_isoc_out_frlen == NULL) {
		printf("%s: Could not allocate isoc-out frame sizes buffer\n",
			USBDEVNAME(sc->sc_dev));
		return;
	}

	printf("%s: Interface 1 (alt.config %d) endpoints: isoc-in=%#x, " \
		"isoc-out=%#x; wMaxPacketSize=%d; nframes=%d, buffer size=%d\n",
		USBDEVNAME(sc->sc_dev), alt_no, sc->sc_isoc_in_ep,
		sc->sc_isoc_out_ep, sc->sc_isoc_size, sc->sc_isoc_nframes,
		(sc->sc_isoc_nframes * sc->sc_isoc_size));

	/*
	 * Open pipes
	 */

	/* Interrupt */
	error = usbd_open_pipe(sc->sc_iface0, sc->sc_intr_ep,
			USBD_EXCLUSIVE_USE, &sc->sc_intr_pipe);
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s - Could not open interrupt pipe. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(error),
			error);
		return;
	}

	/* Bulk-in */
	error = usbd_open_pipe(sc->sc_iface0, sc->sc_bulk_in_ep,
			USBD_EXCLUSIVE_USE, &sc->sc_bulk_in_pipe);
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s - Could not open bulk-in pipe. %s (%d)\n",
			__func__,  USBDEVNAME(sc->sc_dev), usbd_errstr(error),
			error);
		return;
	}

	/* Bulk-out */
	error = usbd_open_pipe(sc->sc_iface0, sc->sc_bulk_out_ep,
			USBD_EXCLUSIVE_USE, &sc->sc_bulk_out_pipe);
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s - Could not open bulk-out pipe. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(error),
			error);
		return;
	}

#if __broken__ /* XXX FIXME */
	/* Isoc-in */
	error = usbd_open_pipe(sc->sc_iface1, sc->sc_isoc_in_ep,
			USBD_EXCLUSIVE_USE, &sc->sc_isoc_in_pipe);
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s - Could not open isoc-in pipe. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(error),
			error);
		return;
	}

	/* Isoc-out */
	error = usbd_open_pipe(sc->sc_iface1, sc->sc_isoc_out_ep,
			USBD_EXCLUSIVE_USE, &sc->sc_isoc_out_pipe);
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s - Could not open isoc-out pipe. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(error),
			error);
		return;
	}
#endif /* __broken__ */

	/* Start intr transfer */
	error = ubt_intr_start(sc);
	if (error != USBD_NORMAL_COMPLETION) {
		NG_UBT_ALERT(
"%s: %s - Could not start interrupt transfer. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(error),
			error);
		return;
	}

	/* Start bulk-in transfer */
	error = ubt_bulk_in_start(sc);
	if (error != USBD_NORMAL_COMPLETION) {
		NG_UBT_ALERT(
"%s: %s - Could not start bulk-in transfer. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(error),
			error);
		return;
	}

#if __broken__ /* XXX FIXME */
	/* Start isoc-in transfer */
	error = ubt_isoc_in_start(sc);
	if (error != USBD_NORMAL_COMPLETION) {
		NG_UBT_ALERT(
"%s: %s - Could not start isoc-in transfer. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(error),
			error);
		return;
		}
#endif /* __broken__ */

	/* Claim all interfaces on the device */
	for (i = 0; i < uaa->nifaces; i++)
		uaa->ifaces[i] = NULL;

	/* Attach network interface */
	bzero(ifp, sizeof(*ifp));
	strlcpy(ifp->if_xname, USBDEVNAME(sc->sc_dev), sizeof(ifp->if_xname));
	ifp->if_softc = sc;
	ifp->if_mtu = 65536;
	ifp->if_start = ubt_if_start;
	ifp->if_ioctl = ubt_if_ioctl;
	ifp->if_init = ubt_if_init;
	ifp->if_watchdog = ubt_if_watchdog;
	ifp->if_type = IFT_BLUETOOTH;
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	if_alloc_sadl(ifp);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
		USBDEV(sc->sc_dev));
}

int
ubt_detach(struct device *self, int flags)
{
	struct ubt_softc *sc = (struct ubt_softc *)self;
	struct ifnet *ifp = &sc->sc_if;

	/* Close pipes */
	if (sc->sc_intr_pipe != NULL) {
		usbd_close_pipe(sc->sc_intr_pipe);
		sc->sc_intr_pipe = NULL;
	}

	if (sc->sc_bulk_in_pipe != NULL) {
		usbd_close_pipe(sc->sc_bulk_in_pipe);
		sc->sc_bulk_in_pipe = NULL;
	}
	if (sc->sc_bulk_out_pipe != NULL) {
		usbd_close_pipe(sc->sc_bulk_out_pipe);
		sc->sc_bulk_out_pipe = NULL;
	}

	if (sc->sc_isoc_in_pipe != NULL) {
		usbd_close_pipe(sc->sc_isoc_in_pipe);
		sc->sc_isoc_in_pipe = NULL;
	}
	if (sc->sc_isoc_out_pipe != NULL) {
		usbd_close_pipe(sc->sc_isoc_out_pipe);
		sc->sc_isoc_out_pipe = NULL;
	}

	/* Destroy USB transfer handles */
	if (sc->sc_ctrl_xfer != NULL) {
		usbd_free_xfer(sc->sc_ctrl_xfer);
		sc->sc_ctrl_xfer = NULL;
	}

	if (sc->sc_intr_xfer != NULL) {
		usbd_free_xfer(sc->sc_intr_xfer);
		sc->sc_intr_xfer = NULL;
	}

	if (sc->sc_bulk_in_xfer != NULL) {
		usbd_free_xfer(sc->sc_bulk_in_xfer);
		sc->sc_bulk_in_xfer = NULL;
	}
	if (sc->sc_bulk_out_xfer != NULL) {
		usbd_free_xfer(sc->sc_bulk_out_xfer);
		sc->sc_bulk_out_xfer = NULL;
	}

	if (sc->sc_isoc_in_xfer != NULL) {
		usbd_free_xfer(sc->sc_isoc_in_xfer);
		sc->sc_isoc_in_xfer = NULL;
	}
	if (sc->sc_isoc_out_xfer != NULL) {
		usbd_free_xfer(sc->sc_isoc_out_xfer);
		sc->sc_isoc_out_xfer = NULL;
	}

	/* Destroy isoc. frame size buffers */
	if (sc->sc_isoc_in_frlen != NULL) {
		free(sc->sc_isoc_in_frlen, M_USBDEV);
		sc->sc_isoc_in_frlen = NULL;
	}
	if (sc->sc_isoc_out_frlen != NULL) {
		free(sc->sc_isoc_out_frlen, M_USBDEV);
		sc->sc_isoc_out_frlen = NULL;
	}

#if 0
	/* Destroy queues */
	NG_BT_MBUFQ_DRAIN(&sc->sc_cmdq);
	NG_BT_MBUFQ_DRAIN(&sc->sc_aclq);
	NG_BT_MBUFQ_DRAIN(&sc->sc_scoq);
#endif

	if_detach(ifp);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			USBDEV(sc->sc_dev));

	return (0);
}

int
ubt_activate(device_ptr_t self, enum devact act)
{
	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
	case DVACT_DEACTIVATE:
		break;
	}

	return (0);
}

/*
 * Start USB control request (HCI command).
 */
Static usbd_status
ubt_request_start(struct ubt_softc * sc)
{
	usb_device_request_t	 req;
	struct mbuf		*m = NULL;
	usbd_status		 status;

	KASSERT(!(sc->sc_flags & UBT_CMD_XMIT), (
"%s: %s - Another control request is pending\n",
		__func__, USBDEVNAME(sc->sc_dev)));

	NG_BT_MBUFQ_DEQUEUE(&sc->sc_cmdq, m);
	if (m == NULL) {
		NG_UBT_INFO(
"%s: %s - HCI command queue is empty\n", __func__, USBDEVNAME(sc->sc_dev));

		return (USBD_NORMAL_COMPLETION);
	}

	/*
	 * Check HCI command frame size and copy it back to
	 * linear USB transfer buffer.
	 */

	if (m->m_pkthdr.len > UBT_CTRL_BUFFER_SIZE)
		panic(
"%s: %s - HCI command frame too big, size=%lu, len=%d\n",
			__func__, USBDEVNAME(sc->sc_dev),
			(ulong)UBT_CTRL_BUFFER_SIZE, m->m_pkthdr.len);

	m_copydata(m, 0, m->m_pkthdr.len, sc->sc_ctrl_buffer);

	/* Initialize a USB control request and then schedule it */
	bzero(&req, sizeof(req));
	req.bmRequestType = UBT_HCI_REQUEST;
	USETW(req.wLength, m->m_pkthdr.len);

	NG_UBT_INFO(
"%s: %s - Sending control request, bmRequestType=%#x, wLength=%d\n",
		__func__, USBDEVNAME(sc->sc_dev), req.bmRequestType,
		UGETW(req.wLength));

	usbd_setup_default_xfer(
		sc->sc_ctrl_xfer,
		sc->sc_udev,
		(usbd_private_handle) sc,
		USBD_DEFAULT_TIMEOUT, /* XXX */
		&req,
		sc->sc_ctrl_buffer,
		m->m_pkthdr.len,
		USBD_NO_COPY,
		ubt_request_complete);

	status = usbd_transfer(sc->sc_ctrl_xfer);
	if (status != USBD_NORMAL_COMPLETION && status != USBD_IN_PROGRESS) {
		NG_UBT_ERR(
"%s: %s - Could not start control request. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev),
			usbd_errstr(status), status);

		NG_BT_MBUFQ_DROP(&sc->sc_cmdq);
		NG_UBT_STAT_OERROR(sc->sc_stat);

		/* XXX FIXME should we try to resubmit another request? */
	} else {
		NG_UBT_INFO(
"%s: %s - Control request has been started\n",
			__func__, USBDEVNAME(sc->sc_dev));

		sc->sc_flags |= UBT_CMD_XMIT;
		status = USBD_NORMAL_COMPLETION;
	}

	NG_FREE_M(m);

	return (status);
} /* ubt_request_start */

/*
 * USB control request callback
 */
Static void
ubt_request_complete(usbd_xfer_handle h, usbd_private_handle p, usbd_status s)
{
	struct ubt_softc *		sc = p;

	if (sc == NULL)
		return;

	KASSERT((sc->sc_flags & UBT_CMD_XMIT), (
"%s: %s - No control request is pending\n", __func__, USBDEVNAME(sc->sc_dev)));

	sc->sc_flags &= ~UBT_CMD_XMIT;

	if (s == USBD_CANCELLED) {
		NG_UBT_INFO(
"%s: %s - Control request cancelled\n", __func__, USBDEVNAME(sc->sc_dev));

		return;
	}

	if (s != USBD_NORMAL_COMPLETION) {
		NG_UBT_ERR(
"%s: %s - Control request failed. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(s), s);

		if (s == USBD_STALLED)
			usbd_clear_endpoint_stall_async(h->pipe);

		NG_UBT_STAT_OERROR(sc->sc_stat);
	} else {
		NG_UBT_INFO(
"%s: %s - Sent %d bytes to control pipe\n",
			__func__, USBDEVNAME(sc->sc_dev), h->actlen);

		NG_UBT_STAT_BYTES_SENT(sc->sc_stat, h->actlen);
		NG_UBT_STAT_PCKTS_SENT(sc->sc_stat);
	}

	if (NG_BT_MBUFQ_LEN(&sc->sc_cmdq) > 0)
		ubt_request_start(sc);
} /* ubt_request_complete2 */

/*
 * Start interrupt transfer. Must be called when node is locked
 */

Static usbd_status
ubt_intr_start(struct ubt_softc * sc)
{
	struct mbuf	*m = NULL;
	usbd_status	 status;

	KASSERT(!(sc->sc_flags & UBT_EVT_RECV), (
"%s: %s - Another interrupt request is pending\n",
		__func__, USBDEVNAME(sc->sc_dev)));

	/* Allocate new mbuf cluster */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (USBD_NOMEM);

	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		NG_FREE_M(m);
		return (USBD_NOMEM);
	}

	if (!(sc->sc_flags & UBT_HAVE_FRAME_TYPE)) {
		*mtod(m, u_int8_t *) = NG_HCI_EVENT_PKT;
		m->m_pkthdr.len = m->m_len = 1;
	} else
		m->m_pkthdr.len = m->m_len = 0;

	/* Initialize a USB transfer and then schedule it */
	usbd_setup_xfer(
			sc->sc_intr_xfer,
			sc->sc_intr_pipe,
			(usbd_private_handle) sc,
			sc->sc_intr_buffer,
			UBT_INTR_BUFFER_SIZE,
			USBD_SHORT_XFER_OK,
			USBD_NO_TIMEOUT,
			ubt_intr_complete);

	status = usbd_transfer(sc->sc_intr_xfer);
	if (status != USBD_NORMAL_COMPLETION && status != USBD_IN_PROGRESS) {
		NG_UBT_ERR(
"%s: %s - Failed to start intrerrupt transfer. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(status),
			status);

		NG_FREE_M(m);

		return (status);
	}

	sc->sc_flags |= UBT_EVT_RECV;
	sc->sc_intr_mbuf = m;

	return (USBD_NORMAL_COMPLETION);
} /* ubt_intr_start */

/*
 * Process interrupt from USB device (We got data from interrupt pipe)
 */
Static void
ubt_intr_complete(usbd_xfer_handle h, usbd_private_handle p, usbd_status s)
{
	struct ubt_softc *	sc = p;
	struct mbuf		*m = NULL;
	ng_hci_event_pkt_t	*hdr = NULL;
	int			error = 0;

	if (sc == NULL)
		return;

	KASSERT((sc->sc_flags & UBT_EVT_RECV), (
"%s: %s - No interrupt request is pending\n",
		__func__, USBDEVNAME(sc->sc_dev)));

	sc->sc_flags &= ~UBT_EVT_RECV;

	m = sc->sc_intr_mbuf;
	bcopy(sc->sc_intr_buffer, mtod(m, void *) + m->m_len, h->actlen);

	hdr = mtod(m, ng_hci_event_pkt_t *);

	if (s == USBD_CANCELLED) {
		NG_UBT_INFO(
"%s: %s - Interrupt xfer cancelled\n", __func__, USBDEVNAME(sc->sc_dev));

		NG_FREE_M(m);
		return;
	}
		
	if (s != USBD_NORMAL_COMPLETION) {
		NG_UBT_WARN(
"%s: %s - Interrupt xfer failed, %s (%d). No new xfer will be submitted!\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(s), s);

		if (s == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_intr_pipe);

		NG_UBT_STAT_IERROR(sc->sc_stat);
		NG_FREE_M(m);

		return; /* XXX FIXME we should restart after some delay */
	}

	NG_UBT_STAT_BYTES_RECV(sc->sc_stat, h->actlen);
	m->m_pkthdr.len += h->actlen;
	m->m_len += h->actlen;

	NG_UBT_INFO(
"%s: %s - Got %d bytes from interrupt pipe\n",
		__func__, USBDEVNAME(sc->sc_dev), h->actlen);

	if (m->m_pkthdr.len < sizeof(*hdr)) {
		NG_FREE_M(m);
		goto done;
	}

	if (hdr->length == m->m_pkthdr.len - sizeof(*hdr)) {
		NG_UBT_INFO(
"%s: %s - Got complete HCI event frame, pktlen=%d, length=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), m->m_pkthdr.len,
			hdr->length);

		NG_UBT_STAT_PCKTS_RECV(sc->sc_stat);

#if 0
		NG_SEND_DATA_ONLY(error, sc->sc_hook, m);
#endif
		ng_btsocket_hci_raw_node_rcvdata(&sc->sc_if, m);
		if (error != 0)
			NG_UBT_STAT_IERROR(sc->sc_stat);
	} else {
		NG_UBT_ERR(
"%s: %s - Invalid HCI event frame size, length=%d, pktlen=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), hdr->length,
			m->m_pkthdr.len);

		NG_UBT_STAT_IERROR(sc->sc_stat);
		NG_FREE_M(m);
	}
done:
	ubt_intr_start(sc);
} /* ubt_intr_complete */

/*
 * Start bulk-in USB transfer (ACL data). Must be called when node is locked
 */
Static usbd_status
ubt_bulk_in_start(struct ubt_softc * sc)
{
	struct mbuf	*m = NULL;
	usbd_status	 status;

	KASSERT(!(sc->sc_flags & UBT_ACL_RECV), (
"%s: %s - Another bulk-in request is pending\n",
		__func__, USBDEVNAME(sc->sc_dev)));

	/* Allocate new mbuf cluster */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (USBD_NOMEM);

	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		NG_FREE_M(m);
		return (USBD_NOMEM);
	}

	if (!(sc->sc_flags & UBT_HAVE_FRAME_TYPE)) {
		*mtod(m, u_int8_t *) = NG_HCI_ACL_DATA_PKT;
		m->m_pkthdr.len = m->m_len = 1;
	} else
		m->m_pkthdr.len = m->m_len = 0;
	
	/* Initialize a bulk-in USB transfer and then schedule it */
	usbd_setup_xfer(
			sc->sc_bulk_in_xfer,
			sc->sc_bulk_in_pipe,
			(usbd_private_handle) sc,
			sc->sc_bulk_in_buffer,
			UBT_BULK_BUFFER_SIZE,
			USBD_SHORT_XFER_OK,
			USBD_NO_TIMEOUT,
			ubt_bulk_in_complete);

	status = usbd_transfer(sc->sc_bulk_in_xfer);
	if (status != USBD_NORMAL_COMPLETION && status != USBD_IN_PROGRESS) {
		NG_UBT_ERR(
"%s: %s - Failed to start bulk-in transfer. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(status),
			status);

		NG_FREE_M(m);

		return (status);
	}

	sc->sc_flags |= UBT_ACL_RECV;
	sc->sc_bulk_in_mbuf = m;

	return (USBD_NORMAL_COMPLETION);
} /* ubt_bulk_in_start */

/*
 * USB bulk-in transfer callback
 */
Static void
ubt_bulk_in_complete(usbd_xfer_handle h, usbd_private_handle p, usbd_status s)
{
	struct ubt_softc *	sc = p;
	struct mbuf		*m = NULL;
	ng_hci_acldata_pkt_t	*hdr = NULL;
	int			 len;

	if (sc == NULL)
		return;

	KASSERT((sc->sc_flags & UBT_ACL_RECV), (
"%s: %s - No bulk-in request is pending\n", __func__, USBDEVNAME(sc->sc_dev)));

	sc->sc_flags &= ~UBT_ACL_RECV;

	m = sc->sc_bulk_in_mbuf;
	bcopy(sc->sc_intr_buffer, mtod(m, void *) + m->m_len, h->actlen);

	hdr = mtod(m, ng_hci_acldata_pkt_t *);

	if (s == USBD_CANCELLED) {
		NG_UBT_INFO(
"%s: %s - Bulk-in xfer cancelled, pipe=%p\n",
			__func__, USBDEVNAME(sc->sc_dev), sc->sc_bulk_in_pipe);

		NG_FREE_M(m);
		return;
	}

	if (s != USBD_NORMAL_COMPLETION) {
		NG_UBT_WARN(
"%s: %s - Bulk-in xfer failed, %s (%d). No new xfer will be submitted!\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(s), s);

		if (s == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_bulk_in_pipe);

		NG_UBT_STAT_IERROR(sc->sc_stat);
		NG_FREE_M(m);

		return; /* XXX FIXME we should restart after some delay */
	}

	NG_UBT_STAT_BYTES_RECV(sc->sc_stat, h->actlen);
	m->m_pkthdr.len += h->actlen;
	m->m_len += h->actlen;

	NG_UBT_INFO(
"%s: %s - Got %d bytes from bulk-in pipe\n",
		__func__, USBDEVNAME(sc->sc_dev), h->actlen);

	if (m->m_pkthdr.len < sizeof(*hdr)) {
		NG_FREE_M(m);
		goto done;
	}

	len = letoh16(hdr->length);
	if (len == m->m_pkthdr.len - sizeof(*hdr)) {
		NG_UBT_INFO(
"%s: %s - Got complete ACL data frame, pktlen=%d, length=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), m->m_pkthdr.len, len);

		NG_UBT_STAT_PCKTS_RECV(sc->sc_stat);

#if 0
		NG_SEND_DATA_ONLY(len, sc->sc_hook, m);
#endif
		if (len != 0)
			NG_UBT_STAT_IERROR(sc->sc_stat);
	} else {
		NG_UBT_ERR(
"%s: %s - Invalid ACL frame size, length=%d, pktlen=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), len,
			m->m_pkthdr.len);

		NG_UBT_STAT_IERROR(sc->sc_stat);
		NG_FREE_M(m);
	}
done:
	ubt_bulk_in_start(sc);
} /* ubt_bulk_in_complete2 */

/*
 * Start bulk-out USB transfer. Must be called with node locked
 */

Static usbd_status
ubt_bulk_out_start(struct ubt_softc * sc)
{
	struct mbuf	*m = NULL;
	usbd_status	status;

	KASSERT(!(sc->sc_flags & UBT_ACL_XMIT), (
"%s: %s - Another bulk-out request is pending\n",
		__func__, USBDEVNAME(sc->sc_dev)));

	NG_BT_MBUFQ_DEQUEUE(&sc->sc_aclq, m);
	if (m == NULL) {
		NG_UBT_INFO(
"%s: %s - ACL data queue is empty\n", __func__, USBDEVNAME(sc->sc_dev));

 		return (USBD_NORMAL_COMPLETION);
	}

	/*
	 * Check ACL data frame size and copy it back to linear USB
	 * transfer buffer.
	 */

	if (m->m_pkthdr.len > UBT_BULK_BUFFER_SIZE)
		panic(
"%s: %s - ACL data frame too big, size=%d, len=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), UBT_BULK_BUFFER_SIZE,
			m->m_pkthdr.len);

	m_copydata(m, 0, m->m_pkthdr.len, sc->sc_bulk_out_buffer);

	/* Initialize a bulk-out USB transfer and then schedule it */
	usbd_setup_xfer(
			sc->sc_bulk_out_xfer,
			sc->sc_bulk_out_pipe,
			(usbd_private_handle) sc,
			sc->sc_bulk_out_buffer,
			m->m_pkthdr.len,
			USBD_NO_COPY,
			USBD_DEFAULT_TIMEOUT, /* XXX */
			ubt_bulk_out_complete);

	status = usbd_transfer(sc->sc_bulk_out_xfer);
	if (status != USBD_NORMAL_COMPLETION && status != USBD_IN_PROGRESS) {
		NG_UBT_ERR(
"%s: %s - Could not start bulk-out transfer. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(status),
			status);

		NG_BT_MBUFQ_DROP(&sc->sc_aclq);
		NG_UBT_STAT_OERROR(sc->sc_stat);

		/* XXX FIXME should we try to start another transfer? */
	} else {
		NG_UBT_INFO(
"%s: %s - Bulk-out transfer has been started, len=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), m->m_pkthdr.len);

		sc->sc_flags |= UBT_ACL_XMIT;
		status = USBD_NORMAL_COMPLETION;
	}

	NG_FREE_M(m);

	return (status);
} /* ubt_bulk_out_start */

/*
 * USB bulk-out transfer callback
 */
Static void
ubt_bulk_out_complete(usbd_xfer_handle h, usbd_private_handle p, usbd_status s)
{
	struct ubt_softc *	sc = p;

	if (sc == NULL)
		return;

	KASSERT((sc->sc_flags & UBT_ACL_XMIT), (
"%s: %s - No bulk-out request is pending\n", __func__, USBDEVNAME(sc->sc_dev)));

	sc->sc_flags &= ~UBT_ACL_XMIT;

	if (s == USBD_CANCELLED) {
		NG_UBT_INFO(
"%s: %s - Bulk-out xfer cancelled, pipe=%p\n",
			__func__, USBDEVNAME(sc->sc_dev), sc->sc_bulk_out_pipe);

		return;
	}

	if (s != USBD_NORMAL_COMPLETION) {
		NG_UBT_WARN(
"%s: %s - Bulk-out xfer failed. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(s), s);

		if (s == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_bulk_out_pipe);

		NG_UBT_STAT_OERROR(sc->sc_stat);
	} else {
		NG_UBT_INFO(
"%s: %s - Sent %d bytes to bulk-out pipe\n",
			__func__, USBDEVNAME(sc->sc_dev), h->actlen);

		NG_UBT_STAT_BYTES_SENT(sc->sc_stat, h->actlen);
		NG_UBT_STAT_PCKTS_SENT(sc->sc_stat);
	}

	if (NG_BT_MBUFQ_LEN(&sc->sc_aclq) > 0)
		ubt_bulk_out_start(sc);
} /* ubt_bulk_out_complete2 */

/*
 * Start Isochronous-in USB transfer. Must be called with node locked
 */

Static usbd_status
ubt_isoc_in_start(struct ubt_softc * sc)
{
	usbd_status	status;
	int		i;

	KASSERT(!(sc->sc_flags & UBT_SCO_RECV), (
"%s: %s - Another isoc-in request is pending\n",
                __func__, USBDEVNAME(sc->sc_dev)));

	/* Initialize a isoc-in USB transfer and then schedule it */
	for (i = 0; i < sc->sc_isoc_nframes; i++)
		sc->sc_isoc_in_frlen[i] = sc->sc_isoc_size;

	usbd_setup_isoc_xfer(
			sc->sc_isoc_in_xfer,
			sc->sc_isoc_in_pipe,
			(usbd_private_handle) sc,
			sc->sc_isoc_in_frlen,
			sc->sc_isoc_nframes,
			USBD_NO_COPY, /* XXX flags */
			ubt_isoc_in_complete);

	status = usbd_transfer(sc->sc_isoc_in_xfer);
	if (status != USBD_NORMAL_COMPLETION && status != USBD_IN_PROGRESS) {
		NG_UBT_ERR(
"%s: %s - Failed to start isoc-in transfer. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev),
			usbd_errstr(status), status);

		return (status);
	}

	sc->sc_flags |= UBT_SCO_RECV;

	return (USBD_NORMAL_COMPLETION);
} /* ubt_isoc_in_start */

/*
 * USB isochronous transfer callback
 */
Static void
ubt_isoc_in_complete(usbd_xfer_handle h, usbd_private_handle p, usbd_status s)
{
	struct ubt_softc *	sc = p;
	struct mbuf		*m = NULL;
	ng_hci_scodata_pkt_t	*hdr = NULL;
	u_int8_t		*b = NULL;
	int			 i;

	if (sc == NULL)
		return;

	KASSERT((sc->sc_flags & UBT_SCO_RECV), (
"%s: %s - No isoc-in request is pending\n", __func__, USBDEVNAME(sc->sc_dev)));

	sc->sc_flags &= ~UBT_SCO_RECV;

#if 0
	if (sc->sc_hook == NULL || NG_HOOK_NOT_VALID(sc->sc_hook)) {
		NG_UBT_INFO(
"%s: %s - No upstream hook\n", __func__, USBDEVNAME(sc->sc_dev));

		return;
	}
#endif

	if (s == USBD_CANCELLED) {
		NG_UBT_INFO(
"%s: %s - Isoc-in xfer cancelled, pipe=%p\n",
			__func__, USBDEVNAME(sc->sc_dev), sc->sc_isoc_in_pipe);

		return;
	}

	if (s != USBD_NORMAL_COMPLETION) {
		NG_UBT_WARN(
"%s: %s - Isoc-in xfer failed, %s (%d). No new xfer will be submitted!\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(s), s);

		if (s == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_isoc_in_pipe);

		NG_UBT_STAT_IERROR(sc->sc_stat);

		return; /* XXX FIXME we should restart after some delay */
	}

	NG_UBT_STAT_BYTES_RECV(sc->sc_stat, h->actlen);

	NG_UBT_INFO(
"%s: %s - Got %d bytes from isoc-in pipe\n",
		__func__, USBDEVNAME(sc->sc_dev), h->actlen);

	/* Copy SCO data frame to mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		NG_UBT_ALERT(
"%s: %s - Could not allocate mbuf\n",
			__func__, USBDEVNAME(sc->sc_dev));

		NG_UBT_STAT_IERROR(sc->sc_stat);
		goto done;
	}

	/* Fix SCO data frame header if required */
	if (!(sc->sc_flags & UBT_HAVE_FRAME_TYPE)) {
		*mtod(m, u_int8_t *) = NG_HCI_SCO_DATA_PKT;
		m->m_pkthdr.len = 1;
		m->m_len = min(MHLEN, h->actlen + 1); /* XXX m_copyback */
	} else {
		m->m_pkthdr.len = 0;
		m->m_len = min(MHLEN, h->actlen); /* XXX m_copyback */
	}

	/*
	 * XXX FIXME how do we know how many frames we have received?
	 * XXX use frlen for now. is that correct?
	 */

	b = (u_int8_t *) sc->sc_isoc_in_buffer;

	for (i = 0; i < sc->sc_isoc_nframes; i++) {
		b += (i * sc->sc_isoc_size);

		if (sc->sc_isoc_in_frlen[i] > 0)
			m_copyback(m, m->m_pkthdr.len,
				sc->sc_isoc_in_frlen[i], b);
	}

	if (m->m_pkthdr.len < sizeof(*hdr))
		goto done;

	hdr = mtod(m, ng_hci_scodata_pkt_t *);

	if (hdr->length == m->m_pkthdr.len - sizeof(*hdr)) {
		NG_UBT_INFO(
"%s: %s - Got complete SCO data frame, pktlen=%d, length=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), m->m_pkthdr.len,
			hdr->length);

		NG_UBT_STAT_PCKTS_RECV(sc->sc_stat);

#if 0
		NG_SEND_DATA_ONLY(i, sc->sc_hook, m);
#endif
		if (i != 0)
			NG_UBT_STAT_IERROR(sc->sc_stat);
	} else {
		NG_UBT_ERR(
"%s: %s - Invalid SCO frame size, length=%d, pktlen=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), hdr->length,
			m->m_pkthdr.len);

		NG_UBT_STAT_IERROR(sc->sc_stat);
		NG_FREE_M(m);
	}
done:
	ubt_isoc_in_start(sc);
} /* ubt_isoc_in_complete2 */

/*
 * Start isochronous-out USB transfer. Must be called with node locked
 */

Static usbd_status
ubt_isoc_out_start(struct ubt_softc * sc)
{
	struct mbuf	*m = NULL;
	u_int8_t	*b = NULL;
	int		 i, len, nframes;
	usbd_status	 status;

	KASSERT(!(sc->sc_flags & UBT_SCO_XMIT), (
"%s: %s - Another isoc-out request is pending\n",
		__func__, USBDEVNAME(sc->sc_dev)));

	NG_BT_MBUFQ_DEQUEUE(&sc->sc_scoq, m);
	if (m == NULL) {
		NG_UBT_INFO(
"%s: %s - SCO data queue is empty\n", __func__, USBDEVNAME(sc->sc_dev));

 		return (USBD_NORMAL_COMPLETION);
	}

	/* Copy entire SCO frame into USB transfer buffer and start transfer */
	b = (u_int8_t *) sc->sc_isoc_out_buffer;
	nframes = 0;

	for (i = 0; i < sc->sc_isoc_nframes; i++) {
		b += (i * sc->sc_isoc_size);

		len = min(m->m_pkthdr.len, sc->sc_isoc_size);
		if (len > 0) {
			m_copydata(m, 0, len, b);
			m_adj(m, len);
			nframes ++;
		}

		sc->sc_isoc_out_frlen[i] = len;
	}

	if (m->m_pkthdr.len > 0)
		panic(
"%s: %s - SCO data frame is too big, nframes=%d, size=%d, len=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), sc->sc_isoc_nframes,
			sc->sc_isoc_size, m->m_pkthdr.len);

	NG_FREE_M(m);

	/* Initialize a isoc-out USB transfer and then schedule it */
	usbd_setup_isoc_xfer(
			sc->sc_isoc_out_xfer,
			sc->sc_isoc_out_pipe,
			(usbd_private_handle) sc,
			sc->sc_isoc_out_frlen,
			nframes,
			USBD_NO_COPY,
			ubt_isoc_out_complete);

	status = usbd_transfer(sc->sc_isoc_out_xfer);
	if (status != USBD_NORMAL_COMPLETION && status != USBD_IN_PROGRESS) {
		NG_UBT_ERR(
"%s: %s - Could not start isoc-out transfer. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(status),
			status);

		NG_BT_MBUFQ_DROP(&sc->sc_scoq);
		NG_UBT_STAT_OERROR(sc->sc_stat);
	} else {
		NG_UBT_INFO(
"%s: %s - Isoc-out transfer has been started, nframes=%d, size=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), nframes,
			sc->sc_isoc_size);

		sc->sc_flags |= UBT_SCO_XMIT;
		status = USBD_NORMAL_COMPLETION;
	}

	return (status);
} /* ubt_isoc_out_start */

/*
 * USB isoc-out. transfer callback
 */
Static void
ubt_isoc_out_complete(usbd_xfer_handle h, usbd_private_handle p, usbd_status s)
{
	struct ubt_softc *	sc = p;

	if (sc == NULL)
		return;

	KASSERT((sc->sc_flags & UBT_SCO_XMIT), (
"%s: %s - No isoc-out request is pending\n", __func__, USBDEVNAME(sc->sc_dev)));

	sc->sc_flags &= ~UBT_SCO_XMIT;

	if (s == USBD_CANCELLED) {
		NG_UBT_INFO(
"%s: %s - Isoc-out xfer cancelled, pipe=%p\n",
			__func__, USBDEVNAME(sc->sc_dev),
			sc->sc_isoc_out_pipe);

		return;
	}

	if (s != USBD_NORMAL_COMPLETION) {
		NG_UBT_WARN(
"%s: %s - Isoc-out xfer failed. %s (%d)\n",
			__func__, USBDEVNAME(sc->sc_dev), usbd_errstr(s), s);

		if (s == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_isoc_out_pipe);

		NG_UBT_STAT_OERROR(sc->sc_stat);
	} else {
		NG_UBT_INFO(
"%s: %s - Sent %d bytes to isoc-out pipe\n",
			__func__, USBDEVNAME(sc->sc_dev), h->actlen);

		NG_UBT_STAT_BYTES_SENT(sc->sc_stat, h->actlen);
		NG_UBT_STAT_PCKTS_SENT(sc->sc_stat);
	}

	if (NG_BT_MBUFQ_LEN(&sc->sc_scoq) > 0)
		ubt_isoc_out_start(sc);
} /* ubt_isoc_out_complete2 */

/*
 * Abort transfers on all USB pipes
 */

Static void
ubt_reset(struct ubt_softc * sc)
{
	/* Interrupt */
	if (sc->sc_intr_pipe != NULL)
		usbd_abort_pipe(sc->sc_intr_pipe);

	/* Bulk-in/out */
	if (sc->sc_bulk_in_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulk_in_pipe);
	if (sc->sc_bulk_out_pipe != NULL)
		usbd_abort_pipe(sc->sc_bulk_out_pipe);

	/* Isoc-in/out */
	if (sc->sc_isoc_in_pipe != NULL)
		usbd_abort_pipe(sc->sc_isoc_in_pipe);
	if (sc->sc_isoc_out_pipe != NULL)
		usbd_abort_pipe(sc->sc_isoc_out_pipe);

#if 0
	/* Cleanup queues */
	NG_BT_MBUFQ_DRAIN(&sc->sc_cmdq);
	NG_BT_MBUFQ_DRAIN(&sc->sc_aclq);
	NG_BT_MBUFQ_DRAIN(&sc->sc_scoq);
#endif
} /* ubt_reset */

/*
 * Process data
 */
Static int
ubt_rcvdata(struct ubt_softc *sc, struct mbuf *m)
{
	usbd_status		(*f)(struct ubt_softc *) = NULL;
	struct ng_bt_mbufq	*q = NULL;
	int			 b, error = 0;

	if (sc == NULL) {
		error = EHOSTDOWN;
		goto done;
	}

#if 0
	if (hook != sc->sc_hook) {
		error = EINVAL;
		goto done;
	}
#endif

#if 0
	/* Deatch mbuf and get HCI frame type */
	NGI_GET_M(item, m);
#endif

	/* Process HCI frame */
	switch (*mtod(m, u_int8_t *)) { /* XXX call m_pullup ? */
	case NG_HCI_CMD_PKT:
		f = ubt_request_start;
		q = &sc->sc_cmdq;
		b = UBT_CMD_XMIT;
		break;

	case NG_HCI_ACL_DATA_PKT:
		f = ubt_bulk_out_start;
		q = &sc->sc_aclq;
		b = UBT_ACL_XMIT;
		break;

#if __broken__ /* XXX FIXME */
	case NG_HCI_SCO_DATA_PKT:
		f = ubt_isoc_out_start;
		q = &sc->sc_scoq;
		b = UBT_SCO_XMIT;
		break;
#endif /* __broken__ */

	default:
		NG_UBT_ERR(
"%s: %s - Dropping unknown/unsupported HCI frame, type=%d, pktlen=%d\n",
			__func__, USBDEVNAME(sc->sc_dev), *mtod(m, u_int8_t *),
			m->m_pkthdr.len);

		NG_FREE_M(m);
		error = EINVAL;

		goto done;
		/* NOT REACHED */
	}

	/* Loose frame type, if required */
	if (!(sc->sc_flags & UBT_NEED_FRAME_TYPE))
		m_adj(m, sizeof(u_int8_t));

	if (NG_BT_MBUFQ_FULL(q)) {
		NG_UBT_ERR(
"%s: %s - Dropping HCI frame %#x, len=%d. Queue full\n",
			__func__, USBDEVNAME(sc->sc_dev),
			*mtod(m, u_int8_t *), m->m_pkthdr.len);

		NG_FREE_M(m);
	} else
		NG_BT_MBUFQ_ENQUEUE(q, m);

	if (!(sc->sc_flags & b))
		if ((*f)(sc) != USBD_NORMAL_COMPLETION)
			error = EIO;
done:
#if 0
	NG_FREE_ITEM(item);
#endif

	return (error);
} /* ubt_rcvdata */

void
ubt_stop(struct ubt_softc *sc)
{
	/* nothing yet */
	return;
}

void
ubt_if_start(struct ifnet *ifp)
{
	struct ubt_softc *sc = ifp->if_softc;
	struct mbuf *m0;

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		IFQ_DEQUEUE(&ifp->if_snd, m0);
		ubt_rcvdata(sc, m0);
	}
}

int
ubt_if_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ubt_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s;
	int error = 0;

	s = splnet();
	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		ubt_if_init(ifp);
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > 65536)
			error = EINVAL;
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			ubt_if_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ubt_stop(sc);
		}
		error = 0;
		break;
	default:
		error = EINVAL;
		break;
	}
	splx(s);

	return(error);
}

int
ubt_if_init(struct ifnet *ifp)
{
	/* nothing yet */
	return (0);
}

void
ubt_if_watchdog(struct ifnet *ifp)
{
	/* nothing yet */
	return;
}
