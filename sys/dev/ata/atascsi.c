/*	$OpenBSD: atascsi.c,v 1.42 2007/10/01 15:34:48 krw Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/ata/atascsi.h>

struct atascsi {
	struct device		*as_dev;
	void			*as_cookie;

	struct ata_port		**as_ports;

	struct atascsi_methods	*as_methods;
	struct scsi_adapter	as_switch;
	struct scsi_link	as_link;
	struct scsibus_softc	*as_scsibus;

	int			as_capability;
};

int		atascsi_cmd(struct scsi_xfer *);

/* template */
struct scsi_adapter atascsi_switch = {
	atascsi_cmd,		/* scsi_cmd */
	minphys,		/* scsi_minphys */
	NULL,
	NULL,
	NULL			/* ioctl */
};

struct scsi_device atascsi_device = {
	NULL, NULL, NULL, NULL
};

int		atascsi_probe(struct atascsi *, int);

struct ata_xfer *ata_setup_identify(struct ata_port *, int);
void		ata_free_identify(struct ata_xfer *);
void		ata_complete_identify(struct ata_xfer *,
		    struct ata_identify *);

int		atascsi_disk_cmd(struct scsi_xfer *);
void		atascsi_disk_cmd_done(struct ata_xfer *);
int		atascsi_disk_inq(struct scsi_xfer *);
void		atascsi_disk_inq_done(struct ata_xfer *);
int		atascsi_disk_capacity(struct scsi_xfer *);
void		atascsi_disk_capacity_done(struct ata_xfer *);
int		atascsi_disk_sync(struct scsi_xfer *);
void		atascsi_disk_sync_done(struct ata_xfer *);
int		atascsi_disk_sense(struct scsi_xfer *);

void		atascsi_empty_done(struct ata_xfer *);

int		atascsi_atapi_cmd(struct scsi_xfer *);
void		atascsi_atapi_cmd_done(struct ata_xfer *);

int		atascsi_stuffup(struct scsi_xfer *);


int		ata_running = 0;

int		ata_exec(struct atascsi *, struct ata_xfer *);

struct ata_xfer	*ata_get_xfer(struct ata_port *, int);
void		ata_put_xfer(struct ata_xfer *);

struct atascsi *
atascsi_attach(struct device *self, struct atascsi_attach_args *aaa)
{
	struct scsibus_attach_args	saa;
	struct atascsi			*as;
	int				i;

	as = malloc(sizeof(*as), M_DEVBUF, M_WAITOK | M_ZERO);

	as->as_dev = self;
	as->as_cookie = aaa->aaa_cookie;
	as->as_methods = aaa->aaa_methods;
	as->as_capability = aaa->aaa_capability;

	/* copy from template and modify for ourselves */
	as->as_switch = atascsi_switch;
	as->as_switch.scsi_minphys = aaa->aaa_minphys;

	/* fill in our scsi_link */
	as->as_link.device = &atascsi_device;
	as->as_link.adapter = &as->as_switch;
	as->as_link.adapter_softc = as;
	as->as_link.adapter_buswidth = aaa->aaa_nports;
	as->as_link.luns = 1; /* XXX port multiplier as luns */
	as->as_link.adapter_target = aaa->aaa_nports;
	as->as_link.openings = aaa->aaa_ncmds;
	if (as->as_capability & ASAA_CAP_NEEDS_RESERVED)
		as->as_link.openings--;

	as->as_ports = malloc(sizeof(struct ata_port *) * aaa->aaa_nports,
	    M_DEVBUF, M_WAITOK | M_ZERO);

	/* fill in the port array with the type of devices there */
	for (i = 0; i < as->as_link.adapter_buswidth; i++)
		atascsi_probe(as, i);

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &as->as_link;

	/* stash the scsibus so we can do hotplug on it */
	as->as_scsibus = (struct scsibus_softc *)config_found(self, &saa,
	    scsiprint);

	return (as);
}

int
atascsi_detach(struct atascsi *as)
{
	return (0);
}

int
atascsi_probe(struct atascsi *as, int port)
{
	struct ata_port		*ap;
	struct ata_xfer		*xa;
	int			type, s;

	if (port > as->as_link.adapter_buswidth)
		return (ENXIO);

	type = as->as_methods->probe(as->as_cookie, port);
	switch (type) {
	case ATA_PORT_T_DISK:
		break;
	case ATA_PORT_T_ATAPI:
		as->as_link.flags |= SDEV_ATAPI;
		as->as_link.quirks |= SDEV_ONLYBIG;
		break;
	default:
		return (ENODEV);
	}

	ap = malloc(sizeof(*ap), M_DEVBUF, M_WAITOK | M_ZERO);
	ap->ap_as = as;
	ap->ap_port = port;
	ap->ap_type = type;

	as->as_ports[port] = ap;

	s = splbio();
	xa = ata_get_xfer(ap, 1);
	splx(s);
	if (xa == NULL)
		return (EBUSY);

	/*
	 * FREEZE LOCK the device so malicous users can't lock it on us.
	 * As there is no harm in issuing this to devices that don't
	 * support the security feature set we just send it, and don't bother
	 * checking if the device sends a command abort to tell us it doesn't
	 * support it
	 */
	xa->fis->command = ATA_C_SEC_FREEZE_LOCK;
	xa->fis->flags = ATA_H2D_FLAGS_CMD;
	xa->complete = atascsi_empty_done;
	xa->flags = ATA_F_POLL | ATA_F_PIO;
	xa->timeout = 1000;
	ata_exec(as, xa);

	return (0);
}

struct ata_xfer *
ata_setup_identify(struct ata_port *ap, int nosleep)
{
	struct ata_xfer		*xa;
	int			s;

	s = splbio();
	xa = ata_get_xfer(ap, nosleep);
	splx(s);
	if (xa == NULL)
		return (NULL);

	xa->data = malloc(512, M_TEMP, nosleep ? (M_NOWAIT | M_ZERO) :
	    (M_WAITOK | M_ZERO));
	if (xa->data == NULL) {
		s = splbio();
		xa->state = ATA_S_ERROR;
		ata_put_xfer(xa);
		splx(s);
		return (NULL);
	}
	xa->datalen = 512;

	xa->fis->flags = ATA_H2D_FLAGS_CMD;
	xa->fis->command = ATA_C_IDENTIFY;
	xa->fis->device = 0;

	xa->flags = ATA_F_READ | ATA_F_PIO;

	return (xa);
}

void
ata_free_identify(struct ata_xfer *xa)
{
	free(xa->data, M_TEMP);
	ata_put_xfer(xa);
}

void
ata_complete_identify(struct ata_xfer *xa, struct ata_identify *id)
{
	u_int16_t		*swap;
	int			i;

	bcopy(xa->data, id, sizeof(struct ata_identify));
	ata_free_identify(xa);

	swap = (u_int16_t *)id->serial;
	for (i = 0; i < sizeof(id->serial) / sizeof(u_int16_t); i++)
		swap[i] = swap16(swap[i]);

	swap = (u_int16_t *)id->firmware;
	for (i = 0; i < sizeof(id->firmware) / sizeof(u_int16_t); i++)
		swap[i] = swap16(swap[i]);

	swap = (u_int16_t *)id->model;
	for (i = 0; i < sizeof(id->model) / sizeof(u_int16_t); i++)
		swap[i] = swap16(swap[i]);
}

int
atascsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];

	if (ap == NULL)
		return (atascsi_stuffup(xs));

	switch (ap->ap_type) {
	case ATA_PORT_T_DISK:
		return (atascsi_disk_cmd(xs));
	case ATA_PORT_T_ATAPI:
		return (atascsi_atapi_cmd(xs));

	case ATA_PORT_T_NONE:
	default:
		return (atascsi_stuffup(xs));
	}
}

int
atascsi_disk_cmd(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	int			s, flags = 0;
	struct scsi_rw		*rw;
	struct scsi_rw_big	*rwb;
	struct ata_xfer		*xa;
	struct ata_fis_h2d	*fis;
	u_int64_t		lba;
	u_int32_t		sector_count;

	switch (xs->cmd->opcode) {
	case READ_BIG:
	case READ_COMMAND:
		flags = ATA_F_READ;
		break;
	case WRITE_BIG:
	case WRITE_COMMAND:
		flags = ATA_F_WRITE;
		/* deal with io outside the switch */
		break;

	case SYNCHRONIZE_CACHE:
		return (atascsi_disk_sync(xs));
	case REQUEST_SENSE:
		return (atascsi_disk_sense(xs));
	case INQUIRY:
		return (atascsi_disk_inq(xs));
	case READ_CAPACITY:
		return (atascsi_disk_capacity(xs));

	case TEST_UNIT_READY:
	case START_STOP:
	case PREVENT_ALLOW:
		return (COMPLETE);

	default:
		return (atascsi_stuffup(xs));
	}

	s = splbio();
	xa = ata_get_xfer(ap, xs->flags & SCSI_NOSLEEP);
	splx(s);
	if (xa == NULL)
		return (NO_CCB);

	xa->flags = flags;
	if (xs->cmdlen == 6) {
		rw = (struct scsi_rw *)xs->cmd;
		lba = _3btol(rw->addr) & (SRW_TOPADDR << 16 | 0xffff);
		sector_count = rw->length ? rw->length : 0x100;
	} else {
		rwb = (struct scsi_rw_big *)xs->cmd;
		lba = _4btol(rwb->addr);
		sector_count = _2btol(rwb->length);
	}

	fis = xa->fis;

	fis->flags = ATA_H2D_FLAGS_CMD;
	fis->lba_low = lba & 0xff;
	fis->lba_mid = (lba >> 8) & 0xff;
	fis->lba_high = (lba >> 16) & 0xff;

	if (ap->ap_ncqdepth && !(xs->flags & SCSI_POLL)) {
		/* Use NCQ */
		xa->flags |= ATA_F_NCQ;
		fis->command = (xa->flags & ATA_F_WRITE) ?
		    ATA_C_WRITE_FPDMA : ATA_C_READ_FPDMA;
		fis->device = ATA_H2D_DEVICE_LBA;
		fis->lba_low_exp = (lba >> 24) & 0xff;
		fis->lba_mid_exp = (lba >> 32) & 0xff;
		fis->lba_high_exp = (lba >> 40) & 0xff;
		fis->sector_count = xa->tag << 3;
		fis->features = sector_count & 0xff;
		fis->features_exp = (sector_count >> 8) & 0xff;
	} else if (sector_count > 0x100 || lba > 0xfffffff) {
		/* Use LBA48 */
		fis->command = (xa->flags & ATA_F_WRITE) ?
		    ATA_C_WRITEDMA_EXT : ATA_C_READDMA_EXT;
		fis->device = ATA_H2D_DEVICE_LBA;
		fis->lba_low_exp = (lba >> 24) & 0xff;
		fis->lba_mid_exp = (lba >> 32) & 0xff;
		fis->lba_high_exp = (lba >> 40) & 0xff;
		fis->sector_count = sector_count & 0xff;
		fis->sector_count_exp = (sector_count >> 8) & 0xff;
	} else {
		/* Use LBA */
		fis->command = (xa->flags & ATA_F_WRITE) ?
		    ATA_C_WRITEDMA : ATA_C_READDMA;
		fis->device = ATA_H2D_DEVICE_LBA | ((lba >> 24) & 0x0f);
		fis->sector_count = sector_count & 0xff;
	}

	xa->data = xs->data;
	xa->datalen = xs->datalen;
	xa->complete = atascsi_disk_cmd_done;
	xa->timeout = xs->timeout;
	xa->atascsi_private = xs;
	if (xs->flags & SCSI_POLL)
		xa->flags |= ATA_F_POLL;

	return (ata_exec(as, xa));
}

void
atascsi_empty_done(struct ata_xfer *xa)
{
	ata_put_xfer(xa);
}

void
atascsi_disk_cmd_done(struct ata_xfer *xa)
{
	struct scsi_xfer	*xs = xa->atascsi_private;

	switch (xa->state) {
	case ATA_S_COMPLETE:
		xs->error = XS_NOERROR;
		break;
	case ATA_S_ERROR:
		/* fake sense? */
		xs->error = XS_DRIVER_STUFFUP;
		break;
	case ATA_S_TIMEOUT:
		xs->error = XS_TIMEOUT;
		break;
	default:
		panic("atascsi_disk_cmd_done: unexpected ata_xfer state (%d)",
		    xa->state);
	}

	xs->resid = xa->resid;
	ata_put_xfer(xa);

	xs->flags |= ITSDONE;
	scsi_done(xs);
}

int
atascsi_disk_inq(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct ata_xfer		*xa;

	xa = ata_setup_identify(ap, xs->flags & SCSI_NOSLEEP);
	if (xa == NULL)
		return (NO_CCB);

	xa->complete = atascsi_disk_inq_done;
	xa->timeout = xs->timeout;
	xa->atascsi_private = xs;
	if (xs->flags & SCSI_POLL)
		xa->flags |= ATA_F_POLL;

	return (ata_exec(as, xa));
}

void
atascsi_disk_inq_done(struct ata_xfer *xa)
{
	struct scsi_xfer	*xs = xa->atascsi_private;
	struct scsi_link        *link = xs->sc_link;
	struct atascsi          *as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct ata_identify	id;
	struct scsi_inquiry_data inq;
	int			host_ncqdepth, complete = 0;

	switch (xa->state) {
	case ATA_S_COMPLETE:
		ata_complete_identify(xa, &id);

		bzero(&inq, sizeof(inq));

		inq.device = T_DIRECT;
		inq.version = 2;
		inq.response_format = 2;
		inq.additional_length = 32;
		bcopy("ATA     ", inq.vendor, sizeof(inq.vendor));
		bcopy(id.model, inq.product, sizeof(inq.product));
		bcopy(id.firmware, inq.revision, sizeof(inq.revision));

		bcopy(&inq, xs->data, MIN(sizeof(inq), xs->datalen));
		xs->error = XS_NOERROR;
		complete = 1;
		break;

	case ATA_S_ERROR:
	case ATA_S_TIMEOUT:
		ata_free_identify(xa);
		xs->error = (xa->state == ATA_S_TIMEOUT ? XS_TIMEOUT :
		    XS_DRIVER_STUFFUP);
		break;

	default:
		panic("atascsi_disk_inq_done: unexpected ata_xfer state (%d)",
		    xa->state);
	}

	xs->flags |= ITSDONE;
	scsi_done(xs);

	if (!complete || (ap->ap_features & ATA_PORT_F_PROBED))
		return;

	ap->ap_features = ATA_PORT_F_PROBED;

	if (as->as_capability & ASAA_CAP_NCQ && (letoh16(id.satacap) &
	    (1 << 8))) {
		/*
		 * At this point, openings should be the number of commands the
		 * host controller supports, less the one that is outstanding
		 * as a result of this inquiry, less any reserved slot the 
		 * host controller needs for recovery.
		 */
		host_ncqdepth = link->openings + 1 + ((as->as_capability &
		    ASAA_CAP_NEEDS_RESERVED) ? 1 : 0);

		ap->ap_ncqdepth = (letoh16(id.qdepth) & 0x1f) + 1;

		/* Limit the number of openings to what the device supports. */
		if (host_ncqdepth > ap->ap_ncqdepth)
			link->openings -= (host_ncqdepth - ap->ap_ncqdepth);

		/*
		 * XXX throw away any xfers that have tag numbers higher than
		 * what the device supports.
		 */
		while (host_ncqdepth--) {
			struct ata_xfer *xa;

			xa = ata_get_xfer(ap, 1);
			if (xa->tag < ap->ap_ncqdepth) {
				xa->state = ATA_S_COMPLETE;
				ata_put_xfer(xa);
			}
		}
	}
}

int
atascsi_disk_sync(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct ata_xfer		*xa;
	int			s;

	s = splbio();
	xa = ata_get_xfer(ap, xs->flags & SCSI_NOSLEEP);
	splx(s);
	if (xa == NULL)
		return (NO_CCB);

	xa->datalen = 0;
	xa->flags = ATA_F_READ;
	xa->complete = atascsi_disk_sync_done;
	/* Spec says flush cache can take >30 sec, so give it at least 45. */
	xa->timeout = (xs->timeout < 45000) ? 45000 : xs->timeout;
	xa->atascsi_private = xs;
	if (xs->flags & SCSI_POLL)
		xa->flags |= ATA_F_POLL;

	xa->fis->flags = ATA_H2D_FLAGS_CMD;
	xa->fis->command = ATA_C_FLUSH_CACHE;
	xa->fis->device = 0;

	return (ata_exec(as, xa));
}

void
atascsi_disk_sync_done(struct ata_xfer *xa)
{
	struct scsi_xfer	*xs = xa->atascsi_private;

	switch (xa->state) {
	case ATA_S_COMPLETE:
		xs->error = XS_NOERROR;
		break;

	case ATA_S_ERROR:
	case ATA_S_TIMEOUT:
		printf("atascsi_disk_sync_done: %s\n",
		    xa->state == ATA_S_TIMEOUT ? "timeout" : "error");
		xs->error = (xa->state == ATA_S_TIMEOUT ? XS_TIMEOUT :
		    XS_DRIVER_STUFFUP);
		break;

	default:
		panic("atascsi_disk_sync_done: unexpected ata_xfer state (%d)",
		    xa->state);
	}

	ata_put_xfer(xa);

	xs->flags |= ITSDONE;
	scsi_done(xs);
}

int
atascsi_disk_capacity(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct ata_xfer		*xa;

	xa = ata_setup_identify(ap, xs->flags & SCSI_NOSLEEP);
	if (xa == NULL)
		return (NO_CCB);

	xa->complete = atascsi_disk_capacity_done;
	xa->timeout = xs->timeout;
	xa->atascsi_private = xs;
	if (xs->flags & SCSI_POLL)
		xa->flags |= ATA_F_POLL;

	return (ata_exec(as, xa));
}

void
atascsi_disk_capacity_done(struct ata_xfer *xa)
{
	struct scsi_xfer	*xs = xa->atascsi_private;
	struct ata_identify	id;
	struct scsi_read_cap_data rcd;
	u_int64_t		capacity;
	int			i;

	switch (xa->state) {
	case ATA_S_COMPLETE:
		ata_complete_identify(xa, &id);

		bzero(&rcd, sizeof(rcd));
		if (letoh16(id.cmdset83) & 0x0400) {
			/* LBA48 feature set supported */
			for (i = 3; i >= 0; --i) {
				capacity <<= 16;
				capacity += letoh16(id.addrsecxt[i]);
			}
		} else {
			capacity = letoh16(id.addrsec[1]);
			capacity <<= 16;
			capacity += letoh16(id.addrsec[0]);
		}

		/* XXX SCSI layer can't handle a device this big yet */
		if (capacity > 0xffffffff)
			capacity = 0xffffffff;

		_lto4b(capacity - 1, rcd.addr);
		_lto4b(512, rcd.length);

		bcopy(&rcd, xs->data, MIN(sizeof(rcd), xs->datalen));
		xs->error = XS_NOERROR;
		break;

	case ATA_S_ERROR:
	case ATA_S_TIMEOUT:
		ata_free_identify(xa);
		xs->error = (xa->state == ATA_S_TIMEOUT ? XS_TIMEOUT :
		    XS_DRIVER_STUFFUP);
		break;

	default:
		panic("atascsi_disk_capacity_done: "
		    "unexpected ata_xfer state (%d)", xa->state);
	}

	xs->flags |= ITSDONE;
	scsi_done(xs);
}

int
atascsi_disk_sense(struct scsi_xfer *xs)
{
	struct scsi_sense_data	*sd = (struct scsi_sense_data *)xs->data;
	int			s;

	bzero(xs->data, xs->datalen);
	/* check datalen > sizeof(struct scsi_sense_data)? */
	sd->error_code = 0x70; /* XXX magic */
	sd->flags = SKEY_NO_SENSE;

	xs->error = XS_NOERROR;
	xs->flags |= ITSDONE;

	s = splbio();
	scsi_done(xs);
	splx(s);
	return (COMPLETE);
}

int
atascsi_atapi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	int			s;
	struct ata_xfer		*xa;
	struct ata_fis_h2d	*fis;

	s = splbio();
	xa = ata_get_xfer(ap, xs->flags & SCSI_NOSLEEP);
	splx(s);
	if (xa == NULL)
		return (NO_CCB);

	switch (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
	case SCSI_DATA_IN:
		xa->flags = ATA_F_PACKET | ATA_F_READ;
		break;
	case SCSI_DATA_OUT:
		xa->flags = ATA_F_PACKET | ATA_F_WRITE;
		break;
	default:
		xa->flags = ATA_F_PACKET;
	}

	xa->data = xs->data;
	xa->datalen = xs->datalen;
	xa->complete = atascsi_atapi_cmd_done;
	xa->timeout = xs->timeout;
	xa->atascsi_private = xs;
	if (xs->flags & SCSI_POLL)
		xa->flags |= ATA_F_POLL;

	fis = xa->fis;
	fis->flags = ATA_H2D_FLAGS_CMD;
	fis->command = ATA_C_PACKET;
	fis->device = 0;
	fis->sector_count = xa->tag << 3;
	fis->features = ATA_H2D_FEATURES_DMA | ((xa->flags & ATA_F_WRITE) ?
	    ATA_H2D_FEATURES_DIR_WRITE : ATA_H2D_FEATURES_DIR_READ);
	fis->lba_mid = 0x00;
	fis->lba_high = 0x20;

	/* Copy SCSI command into ATAPI packet. */
	memcpy(xa->packetcmd, xs->cmd, xs->cmdlen);

	return (ata_exec(as, xa));
}

void
atascsi_atapi_cmd_done(struct ata_xfer *xa)
{
	struct scsi_xfer	*xs = xa->atascsi_private;
	struct scsi_sense_data  *sd = &xs->sense;

	switch (xa->state) {
	case ATA_S_COMPLETE:
		xs->error = XS_NOERROR;
		break;
	case ATA_S_ERROR:
		/* Return PACKET sense data */
		sd->error_code = SSD_ERRCODE_CURRENT;
		sd->flags = (xa->rfis.error & 0xf0) >> 4;
		if (xa->rfis.error & 0x04)
			sd->flags = SKEY_ILLEGAL_REQUEST;
		if (xa->rfis.error & 0x02)
			sd->flags |= SSD_EOM;
		if (xa->rfis.error & 0x01)
			sd->flags |= SSD_ILI;
		xs->error = XS_SENSE;
		break;
	case ATA_S_TIMEOUT:
		printf("atascsi_atapi_cmd_done, timeout\n");
		xs->error = XS_TIMEOUT;
		break;
	default:
		panic("atascsi_atapi_cmd_done: unexpected ata_xfer state (%d)",
		    xa->state);
	}

	xs->resid = xa->resid;
	ata_put_xfer(xa);

	xs->flags |= ITSDONE;
	scsi_done(xs);
}

int
atascsi_stuffup(struct scsi_xfer *xs)
{
	int			s;

	xs->error = XS_DRIVER_STUFFUP;
	xs->flags |= ITSDONE;

	s = splbio();
	scsi_done(xs);
	splx(s);
	return (COMPLETE);
}

int
ata_exec(struct atascsi *as, struct ata_xfer *xa)
{
	int polled = xa->flags & ATA_F_POLL;

	switch (as->as_methods->ata_cmd(xa)) {
	case ATA_COMPLETE:
	case ATA_ERROR:
		return (COMPLETE);
	case ATA_QUEUED:
		if (!polled)
			return (SUCCESSFULLY_QUEUED);
	default:
		panic("unexpected return from ata_exec");
	}
}

struct ata_xfer *
ata_get_xfer(struct ata_port *ap, int nosleep /* XXX unused */)
{
	struct atascsi		*as = ap->ap_as;
	struct ata_xfer		*xa;

	xa = as->as_methods->ata_get_xfer(as->as_cookie, ap->ap_port);
	if (xa != NULL)
		xa->fis->type = ATA_FIS_TYPE_H2D;

	return (xa);
}

void
ata_put_xfer(struct ata_xfer *xa)
{
	xa->ata_put_xfer(xa);
}
