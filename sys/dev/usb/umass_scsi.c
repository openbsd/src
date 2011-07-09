/*	$OpenBSD: umass_scsi.c,v 1.37 2011/07/09 04:37:53 matthew Exp $ */
/*	$NetBSD: umass_scsipi.c,v 1.9 2003/02/16 23:14:08 augustss Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/umassvar.h>
#include <dev/usb/umass_scsi.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>
#include <machine/bus.h>

struct umass_scsi_softc {
	struct umassbus_softc	base;
	struct scsi_link	sc_link;
	struct scsi_iopool	sc_iopool;
	int			sc_open;

	struct scsi_sense	sc_sense_cmd;
};


#define UMASS_SCSIID_DEVICE	0x00

int umass_scsi_probe(struct scsi_link *);
void umass_scsi_cmd(struct scsi_xfer *);
void umass_scsi_minphys(struct buf *, struct scsi_link *);

struct scsi_adapter umass_scsi_switch = {
	umass_scsi_cmd,
	umass_scsi_minphys,
	umass_scsi_probe
};

void umass_scsi_cb(struct umass_softc *sc, void *priv, int residue,
		   int status);
void umass_scsi_sense_cb(struct umass_softc *sc, void *priv, int residue,
			 int status);
struct umass_scsi_softc *umass_scsi_setup(struct umass_softc *);

void *umass_io_get(void *);
void umass_io_put(void *, void *);

int
umass_scsi_attach(struct umass_softc *sc)
{
	struct scsibus_attach_args saa;
	struct umass_scsi_softc *scbus;

	scbus = umass_scsi_setup(sc);
	scbus->sc_link.adapter_target = SCSI_NO_ADAPTER_TARGET;
	scbus->sc_link.flags &= ~SDEV_ATAPI;
	scbus->sc_link.flags |= SDEV_UMASS;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &scbus->sc_link;
	saa.saa_targets = 1;
	saa.saa_luns = sc->maxlun + 1;

	DPRINTF(UDMASS_USB, ("%s: umass_attach_bus: SCSI\n"
			     "sc = 0x%x, scbus = 0x%x\n",
			     sc->sc_dev.dv_xname, sc, scbus));

	sc->sc_refcnt++;
	scbus->base.sc_child =
	  config_found((struct device *)sc, &saa, scsiprint);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_dev);

	return (0);
}

int
umass_atapi_attach(struct umass_softc *sc)
{
	struct scsibus_attach_args saa;
	struct umass_scsi_softc *scbus;

	scbus = umass_scsi_setup(sc);
	scbus->sc_link.adapter_target = SCSI_NO_ADAPTER_TARGET;
	scbus->sc_link.openings = 1;
	scbus->sc_link.flags |= SDEV_ATAPI;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &scbus->sc_link;
	saa.saa_targets = 1;
	saa.saa_luns = 1;

	DPRINTF(UDMASS_USB, ("%s: umass_attach_bus: ATAPI\n"
			     "sc = 0x%x, scbus = 0x%x\n",
			     sc->sc_dev.dv_xname, sc, scbus));

	sc->sc_refcnt++;
	scbus->base.sc_child = config_found((struct device *)sc,
	    &saa, scsiprint);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(&sc->sc_dev);

	return (0);
}

struct umass_scsi_softc *
umass_scsi_setup(struct umass_softc *sc)
{
	struct umass_scsi_softc *scbus;

	scbus = malloc(sizeof(*scbus), M_DEVBUF, M_WAITOK | M_ZERO);

	sc->bus = (struct umassbus_softc *)scbus;

	scsi_iopool_init(&scbus->sc_iopool, scbus, umass_io_get, umass_io_put);

	/* Fill in the link. */
	scbus->sc_link.adapter = &umass_scsi_switch;
	scbus->sc_link.adapter_softc = sc;
	scbus->sc_link.openings = 1;
	scbus->sc_link.quirks |= SDEV_ONLYBIG | sc->sc_busquirks;
	scbus->sc_link.pool = &scbus->sc_iopool;

	return (scbus);
}

int
umass_scsi_probe(struct scsi_link *link)
{
	struct umass_softc *sc = link->adapter_softc;
	struct usb_device_info udi;
	size_t len;

	/* dont fake devids when more than one scsi device can attach. */
	if (sc->maxlun > 0)
		return (0);

	usbd_fill_deviceinfo(sc->sc_udev, &udi, 1);

	/*
	 * Create a fake devid using the vendor and product ids and the last
	 * 12 characters of serial number, as recommended by Section 4.1.1 of
	 * the USB Mass Storage Class - Bulk Only Transport spec. 
	 */
	len = strlen(udi.udi_serial);
	if (len >= 12) {
		char buf[21];
		snprintf(buf, sizeof(buf), "%04x%04x%s", udi.udi_vendorNo,
		    udi.udi_productNo, udi.udi_serial + len - 12);
		link->id = devid_alloc(DEVID_SERIAL, DEVID_F_PRINT,
		    sizeof(buf) - 1, buf);
	}

	return (0);
}

void
umass_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *sc_link = xs->sc_link;
	struct umass_softc *sc = sc_link->adapter_softc;
	struct scsi_generic *cmd;
	int cmdlen, dir;

#ifdef UMASS_DEBUG
	microtime(&sc->tv);
#endif

	DIF(UDMASS_UPPER, sc_link->flags |= SCSIDEBUG_LEVEL);

	DPRINTF(UDMASS_CMD, ("%s: umass_scsi_cmd: at %lu.%06lu: %d:%d "
		"xs=%p cmd=0x%02x datalen=%d (quirks=0x%x, poll=%d)\n",
		sc->sc_dev.dv_xname, sc->tv.tv_sec, sc->tv.tv_usec,
		sc_link->target, sc_link->lun, xs, xs->cmd->opcode,
		xs->datalen, sc_link->quirks, xs->flags & SCSI_POLL));

	if (sc->sc_dying) {
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}

#if defined(UMASS_DEBUG)
	if (sc_link->target != UMASS_SCSIID_DEVICE) {
		DPRINTF(UDMASS_SCSI, ("%s: wrong SCSI ID %d\n",
			sc->sc_dev.dv_xname, sc_link->target));
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}
#endif

	cmd = xs->cmd;
	cmdlen = xs->cmdlen;

	dir = DIR_NONE;
	if (xs->datalen) {
		switch (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
		case SCSI_DATA_IN:
			dir = DIR_IN;
			break;
		case SCSI_DATA_OUT:
			dir = DIR_OUT;
			break;
		}
	}

	if (xs->datalen > UMASS_MAX_TRANSFER_SIZE) {
		printf("umass_cmd: large datalen, %d\n", xs->datalen);
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}

	if (xs->flags & SCSI_POLL) {
		DPRINTF(UDMASS_SCSI, ("umass_scsi_cmd: sync dir=%d\n", dir));
		usbd_set_polling(sc->sc_udev, 1);
		sc->sc_xfer_flags = USBD_SYNCHRONOUS;
		sc->polled_xfer_status = USBD_INVAL;
		sc->sc_methods->wire_xfer(sc, sc_link->lun, cmd, cmdlen,
					  xs->data, xs->datalen, dir,
					  xs->timeout, umass_scsi_cb, xs);
		sc->sc_xfer_flags = 0;
		DPRINTF(UDMASS_SCSI, ("umass_scsi_cmd: done err=%d\n",
				      sc->polled_xfer_status));
		usbd_set_polling(sc->sc_udev, 0);
		/* scsi_done() has already been called. */
		return;
	} else {
		DPRINTF(UDMASS_SCSI,
			("umass_scsi_cmd: async dir=%d, cmdlen=%d"
			 " datalen=%d\n",
			 dir, cmdlen, xs->datalen));
		sc->sc_methods->wire_xfer(sc, sc_link->lun, cmd, cmdlen,
					  xs->data, xs->datalen, dir,
					  xs->timeout, umass_scsi_cb, xs);
		/* scsi_done() has already been called. */
		return;
	}

	/* Return if command finishes early. */
 done:
	scsi_done(xs);
}

void
umass_scsi_minphys(struct buf *bp, struct scsi_link *sl)
{
	if (bp->b_bcount > UMASS_MAX_TRANSFER_SIZE)
		bp->b_bcount = UMASS_MAX_TRANSFER_SIZE;

	minphys(bp);
}

void
umass_scsi_cb(struct umass_softc *sc, void *priv, int residue, int status)
{
	struct umass_scsi_softc *scbus = (struct umass_scsi_softc *)sc->bus;
	struct scsi_xfer *xs = priv;
	struct scsi_link *link = xs->sc_link;
	int cmdlen;
#ifdef UMASS_DEBUG
	struct timeval tv;
	u_int delta;
	microtime(&tv);
	delta = (tv.tv_sec - sc->tv.tv_sec) * 1000000 +
		tv.tv_usec - sc->tv.tv_usec;
#endif

	DPRINTF(UDMASS_CMD,
		("umass_scsi_cb: at %lu.%06lu, delta=%u: xs=%p residue=%d"
		 " status=%d\n", tv.tv_sec, tv.tv_usec, delta, xs, residue,
		 status));

	xs->resid = residue;

	switch (status) {
	case STATUS_CMD_OK:
		xs->error = XS_NOERROR;
		break;

	case STATUS_CMD_UNKNOWN:
		DPRINTF(UDMASS_CMD, ("umass_scsi_cb: status cmd unknown\n"));
		/* we can't issue REQUEST SENSE */
		if (xs->sc_link->quirks & ADEV_NOSENSE) {
			/*
			 * If no residue and no other USB error,
			 * command succeeded.
			 */
			if (residue == 0) {
				xs->error = XS_NOERROR;
				break;
			}

			/*
			 * Some devices return a short INQUIRY
			 * response, omitting response data from the
			 * "vendor specific data" on...
			 */
			if (xs->cmd->opcode == INQUIRY &&
			    residue < xs->datalen) {
				xs->error = XS_NOERROR;
				break;
			}

			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		/* FALLTHROUGH */
	case STATUS_CMD_FAILED:
		DPRINTF(UDMASS_CMD, ("umass_scsi_cb: status cmd failed for "
		    "scsi op 0x%02x\n", xs->cmd->opcode));
		/* fetch sense data */
		sc->sc_sense = 1;
		memset(&scbus->sc_sense_cmd, 0, sizeof(scbus->sc_sense_cmd));
		scbus->sc_sense_cmd.opcode = REQUEST_SENSE;
		scbus->sc_sense_cmd.byte2 = link->lun << SCSI_CMD_LUN_SHIFT;
		scbus->sc_sense_cmd.length = sizeof(xs->sense);

		cmdlen = sizeof(scbus->sc_sense_cmd);
		if (xs->flags & SCSI_POLL) {
			usbd_set_polling(sc->sc_udev, 1);
			sc->sc_xfer_flags = USBD_SYNCHRONOUS;
			sc->polled_xfer_status = USBD_INVAL;
		}
		/* scsi_done() has already been called. */
		sc->sc_methods->wire_xfer(sc, link->lun,
					  &scbus->sc_sense_cmd, cmdlen,
					  &xs->sense, sizeof(xs->sense),
					  DIR_IN, xs->timeout,
					  umass_scsi_sense_cb, xs);
		if (xs->flags & SCSI_POLL) {
			sc->sc_xfer_flags = 0;
			usbd_set_polling(sc->sc_udev, 0);
		}
		return;

	case STATUS_WIRE_FAILED:
		xs->error = XS_RESET;
		break;

	default:
		panic("%s: Unknown status %d in umass_scsi_cb",
		      sc->sc_dev.dv_xname, status);
	}

	DPRINTF(UDMASS_CMD,("umass_scsi_cb: at %lu.%06lu: return error=%d, "
			    "status=0x%x resid=%d\n",
			    tv.tv_sec, tv.tv_usec,
			    xs->error, xs->status, xs->resid));

	if ((xs->flags & SCSI_POLL) && (xs->error == XS_NOERROR)) {
		switch (sc->polled_xfer_status) {
		case USBD_NORMAL_COMPLETION:
			xs->error = XS_NOERROR;
			break;
		case USBD_TIMEOUT:
			xs->error = XS_TIMEOUT;
			break;
		default:
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
	}

	scsi_done(xs);
}

/*
 * Finalise a completed autosense operation
 */
void
umass_scsi_sense_cb(struct umass_softc *sc, void *priv, int residue,
		    int status)
{
	struct scsi_xfer *xs = priv;

	DPRINTF(UDMASS_CMD,("umass_scsi_sense_cb: xs=%p residue=%d "
		"status=%d\n", xs, residue, status));

	sc->sc_sense = 0;
	switch (status) {
	case STATUS_CMD_OK:
	case STATUS_CMD_UNKNOWN:
		/* getting sense data succeeded */
		if (residue == 0 || residue == 14)/* XXX */
			xs->error = XS_SENSE;
		else
			xs->error = XS_SHORTSENSE;
		break;
	default:
		DPRINTF(UDMASS_SCSI, ("%s: Autosense failed, status %d\n",
			sc->sc_dev.dv_xname, status));
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	DPRINTF(UDMASS_CMD,("umass_scsi_sense_cb: return xs->error=%d, "
		"xs->flags=0x%x xs->resid=%d\n", xs->error, xs->status,
		xs->resid));

	if ((xs->flags & SCSI_POLL) && (xs->error == XS_NOERROR)) {
		switch (sc->polled_xfer_status) {
		case USBD_NORMAL_COMPLETION:
			xs->error = XS_NOERROR;
			break;
		case USBD_TIMEOUT:
			xs->error = XS_TIMEOUT;
			break;
		default:
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
	}

	scsi_done(xs);
}

void *
umass_io_get(void *cookie)
{
	struct umass_scsi_softc *scbus = cookie;
	void *io = NULL;
	int s;

	s = splusb();
	if (!scbus->sc_open) {
		scbus->sc_open = 1;
		io = scbus; /* just has to be non-NULL */
	}
	splx(s);

	return (io);
}

void
umass_io_put(void *cookie, void *io)
{
	struct umass_scsi_softc *scbus = cookie;
	int s;

	s = splusb();
	scbus->sc_open = 0;
	splx(s);
}
