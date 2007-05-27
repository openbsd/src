/*	$OpenBSD: usscanner.c,v 1.14 2007/05/27 04:00:25 jsg Exp $	*/
/*	$NetBSD: usscanner.c,v 1.6 2001/01/23 14:04:14 augustss Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) and LLoyd Parkes.
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
 * This driver is partly based on information taken from the Linux driver
 * by John Fremlin, Oliver Neukum, and Jeremy Hall.
 */
/*
 * Protocol:
 * Send raw SCSI command on the bulk-out pipe.
 * If output command then
 *     send further data on the bulk-out pipe
 * else if input command then
 *     read data on the bulk-in pipe
 * else
 *     don't do anything.
 * Read status byte on the interrupt pipe (which doesn't seem to be
 * an interrupt pipe at all).  This operation sometimes times out.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/buf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/usbdevs.h>

#if defined(__NetBSD__)
#include <sys/scsiio.h>
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/atapiconf.h>
#elif defined(__OpenBSD__)
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <machine/bus.h>
#endif

#ifdef USSCANNER_DEBUG
#define DPRINTF(x)	do { if (usscannerdebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (usscannerdebug>(n)) printf x; } while (0)
int	usscannerdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define XS_CTL_DATA_IN		SCSI_DATA_IN
#define XS_CTL_DATA_OUT		SCSI_DATA_OUT
#define scsipi_adapter		scsi_adapter
#define scsipi_cmd		scsi_cmd
#define scsipi_device		scsi_device
#define scsipi_done		scsi_done
#define scsipi_link		scsi_link
#define scsipi_minphys		scsi_minphys
#define scsipi_sense		scsi_sense
#define scsipi_xfer		scsi_xfer
#define show_scsipi_xs          show_scsi_xs
#define show_scsipi_cmd         show_scsi_cmd
#define xs_control		flags
#define xs_status		status
#define XS_STS_DONE		ITSDONE
#define XS_CTL_POLL		SCSI_POLL

#define USSCANNER_CONFIG_NO		1
#define USSCANNER_IFACE_IDX		0

#define USSCANNER_SCSIID_HOST	0x00
#define USSCANNER_SCSIID_DEVICE	0x01

#define USSCANNER_MAX_TRANSFER_SIZE	MAXBSIZE

#define USSCANNER_TIMEOUT 2000

Static struct scsipi_device usscanner_dev =
{
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

struct usscanner_softc {
 	USBBASEDEVICE		sc_dev;
	usbd_device_handle	sc_udev;
	usbd_interface_handle	sc_iface;

	int			sc_in_addr;
	usbd_pipe_handle	sc_in_pipe;

	int			sc_intr_addr;
	usbd_pipe_handle	sc_intr_pipe;
	usbd_xfer_handle	sc_intr_xfer;
	u_char			sc_status;

	int			sc_out_addr;
	usbd_pipe_handle	sc_out_pipe;

	usbd_xfer_handle	sc_cmd_xfer;
	void			*sc_cmd_buffer;
	usbd_xfer_handle	sc_data_xfer;
	void			*sc_data_buffer;

	int			sc_state;
#define UAS_IDLE	0
#define UAS_CMD		1
#define UAS_DATA	2
#define UAS_SENSECMD	3
#define UAS_SENSEDATA	4
#define UAS_STATUS	5

	struct scsipi_xfer	*sc_xs;

	device_ptr_t		sc_child;	/* child device, for detach */

	struct scsipi_link	sc_link;
#if defined(__NetBSD__)
	struct atapi_adapter	sc_atapi_adapter;
#define sc_adapter sc_atapi_adapter._generic
#else
	struct scsi_adapter	sc_atapi_adapter;
#define sc_adapter sc_atapi_adapter
#endif

	int			sc_refcnt;
	char			sc_dying;
};


Static void usscanner_cleanup(struct usscanner_softc *sc);
Static int usscanner_scsipi_cmd(struct scsipi_xfer *xs);
Static void usscanner_scsipi_minphys(struct buf *bp);
Static void usscanner_done(struct usscanner_softc *sc);
Static void usscanner_sense(struct usscanner_softc *sc);
typedef void callback(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static callback usscanner_intr_cb;
Static callback usscanner_cmd_cb;
Static callback usscanner_data_cb;
Static callback usscanner_sensecmd_cb;
Static callback usscanner_sensedata_cb;

USB_DECLARE_DRIVER(usscanner);

int
usscanner_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg	*uaa = aux;

	DPRINTFN(50,("usscanner_match\n"));

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	if (uaa->vendor == USB_VENDOR_HP &&
	    uaa->product == USB_PRODUCT_HP_5300C)
		return (UMATCH_VENDOR_PRODUCT);
	else
		return (UMATCH_NONE);
}

void
usscanner_attach(struct device *parent, struct device *self, void *aux)
{
	struct usscanner_softc	*sc = (struct usscanner_softc *)self;
	struct usb_attach_arg	*uaa = aux;
	struct scsibus_attach_args saa;
	usbd_device_handle	dev = uaa->device;
	usbd_interface_handle	iface;
	char			*devinfop;
	usbd_status		err;
	usb_endpoint_descriptor_t *ed;
	u_int8_t		epcount;
	int			i;

	DPRINTFN(10,("usscanner_attach: sc=%p\n", sc));

	devinfop = usbd_devinfo_alloc(dev, 0);
	printf("\n%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
	usbd_devinfo_free(devinfop);

	err = usbd_set_config_no(dev, USSCANNER_CONFIG_NO, 1);
	if (err) {
		printf("%s: setting config no failed\n",
		    USBDEVNAME(sc->sc_dev));
		return;
	}

	err = usbd_device2interface_handle(dev, USSCANNER_IFACE_IDX, &iface);
	if (err) {
		printf("%s: getting interface handle failed\n",
		    USBDEVNAME(sc->sc_dev));
		return;
	}

	sc->sc_udev = dev;
	sc->sc_iface = iface;

	epcount = 0;
	(void)usbd_endpoint_count(iface, &epcount);

	sc->sc_in_addr = -1;
	sc->sc_intr_addr = -1;
	sc->sc_out_addr = -1;
	for (i = 0; i < epcount; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			printf("%s: couldn't get ep %d\n",
			    USBDEVNAME(sc->sc_dev), i);
			return;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_in_addr = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->sc_intr_addr = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->sc_out_addr = ed->bEndpointAddress;
		}
	}
	if (sc->sc_in_addr == -1 || sc->sc_intr_addr == -1 ||
	    sc->sc_out_addr == -1) {
		printf("%s: missing endpoint\n", USBDEVNAME(sc->sc_dev));
		return;
	}

	err = usbd_open_pipe(sc->sc_iface, sc->sc_in_addr,
			     USBD_EXCLUSIVE_USE, &sc->sc_in_pipe);
	if (err) {
		printf("%s: open in pipe failed, err=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		return;
	}

	/* The interrupt endpoint must be opened as a normal pipe. */
	err = usbd_open_pipe(sc->sc_iface, sc->sc_intr_addr,
			     USBD_EXCLUSIVE_USE, &sc->sc_intr_pipe);

	if (err) {
		printf("%s: open intr pipe failed, err=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		usscanner_cleanup(sc);
		return;
	}
	err = usbd_open_pipe(sc->sc_iface, sc->sc_out_addr,
			     USBD_EXCLUSIVE_USE, &sc->sc_out_pipe);
	if (err) {
		printf("%s: open out pipe failed, err=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		usscanner_cleanup(sc);
		return;
	}

	sc->sc_cmd_xfer = usbd_alloc_xfer(uaa->device);
	if (sc->sc_cmd_xfer == NULL) {
		printf("%s: alloc cmd xfer failed, err=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		usscanner_cleanup(sc);
		return;
	}

	/* XXX too big */
	sc->sc_cmd_buffer = usbd_alloc_buffer(sc->sc_cmd_xfer,
					     USSCANNER_MAX_TRANSFER_SIZE);
	if (sc->sc_cmd_buffer == NULL) {
		printf("%s: alloc cmd buffer failed, err=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		usscanner_cleanup(sc);
		return;
	}

	sc->sc_intr_xfer = usbd_alloc_xfer (uaa->device);
	if (sc->sc_intr_xfer == NULL) {
	  printf("%s: alloc intr xfer failed, err=%d\n",
		 USBDEVNAME(sc->sc_dev), err);
	  usscanner_cleanup(sc);
	  return; 
        }

	sc->sc_data_xfer = usbd_alloc_xfer(uaa->device);
	if (sc->sc_data_xfer == NULL) {
		printf("%s: alloc data xfer failed, err=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		usscanner_cleanup(sc);
		return;
	}
	sc->sc_data_buffer = usbd_alloc_buffer(sc->sc_data_xfer,
					      USSCANNER_MAX_TRANSFER_SIZE);
	if (sc->sc_data_buffer == NULL) {
		printf("%s: alloc data buffer failed, err=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		usscanner_cleanup(sc);
		return;
	}

	/*
	 * Fill in the adapter.
	 */
	sc->sc_adapter.scsipi_cmd = usscanner_scsipi_cmd;
	sc->sc_adapter.scsipi_minphys = usscanner_scsipi_minphys;

	/*
	 * fill in the prototype scsipi_link.
	 */
#if defined(__NetBSD__)
	sc->sc_link.type = BUS_SCSI;
#endif
#if defined(__OpenBSD__)
	sc->sc_link.flags &= ~SDEV_ATAPI;
	sc->sc_link.adapter_buswidth = 2;
	sc->sc_link.adapter_target = USSCANNER_SCSIID_HOST;
#endif

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter = &sc->sc_adapter;
	sc->sc_link.device = &usscanner_dev;
	sc->sc_link.openings = 1;
#if defined(__NetBSD__)
	sc->sc_link.scsipi_scsi.channel = SCSI_CHANNEL_ONLY_ONE;
	sc->sc_link.scsipi_scsi.adapter_target = USSCANNER_SCSIID_HOST;
	sc->sc_link.scsipi_scsi.max_target = USSCANNER_SCSIID_DEVICE;
	sc->sc_link.scsipi_scsi.max_lun = 0;
#endif

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	sc->sc_child = config_found(&sc->sc_dev, &saa, scsiprint);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	DPRINTFN(10, ("usscanner_attach: %p\n", sc->sc_udev));
}

int
usscanner_detach(struct device *self, int flags)
{
	struct usscanner_softc *sc = (struct usscanner_softc *)self;
	int rv, s;

	DPRINTF(("usscanner_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;
	/* Abort all pipes.  Causes processes waiting for transfer to wake. */
	if (sc->sc_in_pipe != NULL)
		usbd_abort_pipe(sc->sc_in_pipe);
	if (sc->sc_intr_pipe != NULL)
		usbd_abort_pipe(sc->sc_intr_pipe);
	if (sc->sc_out_pipe != NULL)
		usbd_abort_pipe(sc->sc_out_pipe);

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->sc_dev));
	}
	splx(s);

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);
	else
		rv = 0;

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	return (rv);
}

Static void
usscanner_cleanup(struct usscanner_softc *sc)
{
	if (sc->sc_in_pipe != NULL) {
		usbd_close_pipe(sc->sc_in_pipe);
		sc->sc_in_pipe = NULL;
	}
	if (sc->sc_intr_pipe != NULL) {
		usbd_close_pipe(sc->sc_intr_pipe);
		sc->sc_intr_pipe = NULL;
	}
	if (sc->sc_out_pipe != NULL) {
		usbd_close_pipe(sc->sc_out_pipe);
		sc->sc_out_pipe = NULL;
	}
	if (sc->sc_cmd_xfer != NULL) {
		usbd_free_xfer(sc->sc_cmd_xfer);
		sc->sc_cmd_xfer = NULL;
	}
	if (sc->sc_data_xfer != NULL) {
		usbd_free_xfer(sc->sc_data_xfer);
		sc->sc_data_xfer = NULL;
	}
}

int
usscanner_activate(device_ptr_t self, enum devact act)
{
	struct usscanner_softc *sc = (struct usscanner_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}

Static void
usscanner_scsipi_minphys(struct buf *bp)
{
	if (bp->b_bcount > USSCANNER_MAX_TRANSFER_SIZE)
		bp->b_bcount = USSCANNER_MAX_TRANSFER_SIZE;
	minphys(bp);
}

Static void
usscanner_sense(struct usscanner_softc *sc)
{
	struct scsipi_xfer *xs = sc->sc_xs;
	struct scsipi_link *sc_link = xs->sc_link;
	struct scsipi_sense sense_cmd;
	usbd_status err;

	/* fetch sense data */
	memset(&sense_cmd, 0, sizeof(sense_cmd));
	sense_cmd.opcode = REQUEST_SENSE;
#if defined(__NetBSD__)
	sense_cmd.byte2 = sc_link->scsipi_scsi.lun << SCSI_CMD_LUN_SHIFT;
#else
	sense_cmd.byte2 = sc_link->lun << SCSI_CMD_LUN_SHIFT;
#endif
	sense_cmd.length = sizeof xs->sense;

	sc->sc_state = UAS_SENSECMD;
	memcpy(sc->sc_cmd_buffer, &sense_cmd, sizeof sense_cmd);
	usbd_setup_xfer(sc->sc_cmd_xfer, sc->sc_out_pipe, sc, sc->sc_cmd_buffer,
	    sizeof sense_cmd, USBD_NO_COPY, USSCANNER_TIMEOUT,
	    usscanner_sensecmd_cb);
	err = usbd_transfer(sc->sc_cmd_xfer);
	if (err == USBD_IN_PROGRESS)
		return;

	xs->error = XS_DRIVER_STUFFUP;
	usscanner_done(sc);
}

Static void
usscanner_intr_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		 usbd_status status)
{
	struct usscanner_softc *sc = priv;
	int s;

	DPRINTFN(10, ("usscanner_data_cb status=%d\n", status));

#ifdef USSCANNER_DEBUG
	if (sc->sc_state != UAS_STATUS) {
		printf("%s: !UAS_STATUS\n", USBDEVNAME(sc->sc_dev));
	}
	if (sc->sc_status != 0) {
		printf("%s: status byte=0x%02x\n", USBDEVNAME(sc->sc_dev), sc->sc_status);
	}
#endif
	/* XXX what should we do on non-0 status */

	sc->sc_state = UAS_IDLE;

	sc->sc_xs->xs_status |= XS_STS_DONE;
	s = splbio();
	scsipi_done(sc->sc_xs);
	splx(s);
}

Static void
usscanner_data_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		 usbd_status status)
{
	struct usscanner_softc *sc = priv;
	struct scsipi_xfer *xs = sc->sc_xs;
	u_int32_t len;

	DPRINTFN(10, ("usscanner_data_cb status=%d\n", status));

#ifdef USSCANNER_DEBUG
	if (sc->sc_state != UAS_DATA) {
		printf("%s: !UAS_DATA\n", USBDEVNAME(sc->sc_dev));
	}
#endif

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	xs->resid = xs->datalen - len;

	switch (status) {
	case USBD_NORMAL_COMPLETION:
		if (xs->xs_control & XS_CTL_DATA_IN)
			memcpy(xs->data, sc->sc_data_buffer, len);
		xs->error = XS_NOERROR;
		break;
	case USBD_TIMEOUT:
		xs->error = XS_TIMEOUT;
		break;
	case USBD_CANCELLED:
		if (xs->error == XS_SENSE) {
			usscanner_sense(sc);
			return;
		}
		break;
	default:
		xs->error = XS_DRIVER_STUFFUP; /* XXX ? */
		break;
	}
	usscanner_done(sc);
}

Static void
usscanner_sensedata_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		       usbd_status status)
{
	struct usscanner_softc *sc = priv;
	struct scsipi_xfer *xs = sc->sc_xs;
	u_int32_t len;

	DPRINTFN(10, ("usscanner_sensedata_cb status=%d\n", status));

#ifdef USSCANNER_DEBUG
	if (sc->sc_state != UAS_SENSEDATA) {
		printf("%s: !UAS_SENSEDATA\n", USBDEVNAME(sc->sc_dev));
	}
#endif

	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	switch (status) {
	case USBD_NORMAL_COMPLETION:
		memcpy(&xs->sense, sc->sc_data_buffer, len);
		if (len < sizeof xs->sense)
			xs->error = XS_SHORTSENSE;
		break;
	case USBD_TIMEOUT:
		xs->error = XS_TIMEOUT;
		break;
	case USBD_CANCELLED:
		xs->error = XS_RESET;
		break;
	default:
		xs->error = XS_DRIVER_STUFFUP; /* XXX ? */
		break;
	}
	usscanner_done(sc);
}

Static void
usscanner_done(struct usscanner_softc *sc)
{
	struct scsipi_xfer *xs = sc->sc_xs;
	usbd_status err;

	DPRINTFN(10,("usscanner_done: error=%d\n", sc->sc_xs->error));

	sc->sc_state = UAS_STATUS;
	usbd_setup_xfer(sc->sc_intr_xfer, sc->sc_intr_pipe, sc, &sc->sc_status,
	    1, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USSCANNER_TIMEOUT, usscanner_intr_cb);
	err = usbd_transfer(sc->sc_intr_xfer);
	if (err == USBD_IN_PROGRESS)
		return;
	xs->error = XS_DRIVER_STUFFUP;
}

Static void
usscanner_sensecmd_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		      usbd_status status)
{
	struct usscanner_softc *sc = priv;
	struct scsipi_xfer *xs = sc->sc_xs;
	usbd_status err;

	DPRINTFN(10, ("usscanner_sensecmd_cb status=%d\n", status));

#ifdef USSCANNER_DEBUG
#ifdef notyet
	if (usscannerdebug > 15)
		xs->sc_link->flags |= DEBUGLEVEL;
#endif

	if (sc->sc_state != UAS_SENSECMD) {
		printf("%s: !UAS_SENSECMD\n", USBDEVNAME(sc->sc_dev));
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
#endif

	switch (status) {
	case USBD_NORMAL_COMPLETION:
		break;
	case USBD_TIMEOUT:
		xs->error = XS_TIMEOUT;
		goto done;
	default:
		xs->error = XS_DRIVER_STUFFUP; /* XXX ? */
		goto done;
	}

	sc->sc_state = UAS_SENSEDATA;
	usbd_setup_xfer(sc->sc_data_xfer, sc->sc_in_pipe, sc,
	    sc->sc_data_buffer,
	    sizeof xs->sense, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    USSCANNER_TIMEOUT, usscanner_sensedata_cb);
	err = usbd_transfer(sc->sc_data_xfer);
	if (err == USBD_IN_PROGRESS)
		return;
	xs->error = XS_DRIVER_STUFFUP;
 done:
	usscanner_done(sc);
}

Static void
usscanner_cmd_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
		 usbd_status status)
{
	struct usscanner_softc *sc = priv;
	struct scsipi_xfer *xs = sc->sc_xs;
	usbd_pipe_handle pipe;
	usbd_status err;

	DPRINTFN(10, ("usscanner_cmd_cb status=%d\n", status));

#ifdef USSCANNER_DEBUG
#ifdef notyet
	if (usscannerdebug > 15)
		xs->sc_link->flags |= DEBUGLEVEL;
#endif

	if (sc->sc_state != UAS_CMD) {
		printf("%s: !UAS_CMD\n", USBDEVNAME(sc->sc_dev));
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
#endif

	switch (status) {
	case USBD_NORMAL_COMPLETION:
		break;
	case USBD_TIMEOUT:
		xs->error = XS_TIMEOUT;
		goto done;
	case USBD_CANCELLED:
		goto done;
	default:
		xs->error = XS_DRIVER_STUFFUP; /* XXX ? */
		goto done;
	}

	if (xs->datalen == 0) {
		DPRINTFN(4, ("usscanner_cmd_cb: no data phase\n"));
		xs->error = XS_NOERROR;
		goto done;
	}

	if (xs->xs_control & XS_CTL_DATA_IN) {
		DPRINTFN(4, ("usscanner_cmd_cb: data in len=%d\n",
			     xs->datalen));
		pipe = sc->sc_in_pipe;
	} else {
		DPRINTFN(4, ("usscanner_cmd_cb: data out len=%d\n",
			     xs->datalen));
		memcpy(sc->sc_data_buffer, xs->data, xs->datalen);
		pipe = sc->sc_out_pipe;
	}
	sc->sc_state = UAS_DATA;
	usbd_setup_xfer(sc->sc_data_xfer, pipe, sc, sc->sc_data_buffer,
	    xs->datalen, USBD_SHORT_XFER_OK | USBD_NO_COPY,
	    xs->timeout, usscanner_data_cb);
	err = usbd_transfer(sc->sc_data_xfer);
	if (err == USBD_IN_PROGRESS)
		return;
	xs->error = XS_DRIVER_STUFFUP;

 done:
	usscanner_done(sc);
}

Static int
usscanner_scsipi_cmd(struct scsipi_xfer *xs)
{
	struct scsipi_link *sc_link = xs->sc_link;
	struct usscanner_softc *sc = sc_link->adapter_softc;
	usbd_status err;

#ifdef notyet
	DPRINTFN(8, ("%s: usscanner_scsi_cmd: %d:%d "
	    "xs=%p cmd=0x%02x datalen=%d (quirks=0x%x, poll=%d)\n",
	    USBDEVNAME(sc->sc_dev),
	    sc_link->scsipi_scsi.target, sc_link->scsipi_scsi.lun,
	    xs, xs->cmd->opcode, xs->datalen,
	    sc_link->quirks, xs->xs_control & XS_CTL_POLL));
#endif

	if (sc->sc_dying) {
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}

#ifdef USSCANNER_DEBUG
#ifdef notyet
	if (sc_link->scsipi_scsi.target != USSCANNER_SCSIID_DEVICE) {
		DPRINTF(("%s: wrong SCSI ID %d\n", USBDEVNAME(sc->sc_dev),
		    sc_link->scsipi_scsi.target));
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
#endif
	if (sc->sc_state != UAS_IDLE) {
		printf("%s: !UAS_IDLE\n", USBDEVNAME(sc->sc_dev));
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
#endif

	if (xs->datalen > USSCANNER_MAX_TRANSFER_SIZE) {
		printf("umass_cmd: large datalen, %d\n", xs->datalen);
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}

	DPRINTFN(4, ("usscanner_scsi_cmd: async cmdlen=%d datalen=%d\n",
		    xs->cmdlen, xs->datalen));
	sc->sc_state = UAS_CMD;
	sc->sc_xs = xs;
	memcpy(sc->sc_cmd_buffer, xs->cmd, xs->cmdlen);
	usbd_setup_xfer(sc->sc_cmd_xfer, sc->sc_out_pipe, sc, sc->sc_cmd_buffer,
	    xs->cmdlen, USBD_NO_COPY, USSCANNER_TIMEOUT, usscanner_cmd_cb);
	err = usbd_transfer(sc->sc_cmd_xfer);
	if (err != USBD_IN_PROGRESS) {
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}

	return (SUCCESSFULLY_QUEUED);


 done:
	sc->sc_state = UAS_IDLE;
	xs->xs_status |= XS_STS_DONE;
	scsipi_done(xs);
	if (xs->xs_control & XS_CTL_POLL)
		return (COMPLETE);
	else
		return (SUCCESSFULLY_QUEUED);
}
