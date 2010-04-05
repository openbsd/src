/*	$OpenBSD: atascsi.c,v 1.76 2010/04/05 00:55:03 dlg Exp $ */

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

#include <sys/ataio.h>

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

void		atascsi_cmd(struct scsi_xfer *);
int		atascsi_ioctl(struct scsi_link *, u_long, caddr_t, int,
		    struct proc *);
int		atascsi_probe(struct scsi_link *);
void		atascsi_free(struct scsi_link *);

/* template */
struct scsi_adapter atascsi_switch = {
	atascsi_cmd,		/* scsi_cmd */
	scsi_minphys,		/* scsi_minphys */
	atascsi_probe,		/* dev_probe */
	atascsi_free,		/* dev_free */
	atascsi_ioctl		/* ioctl */
};

struct scsi_device atascsi_device = {
	NULL, NULL, NULL, NULL
};

void		ata_swapcopy(void *, void *, size_t);

void		atascsi_disk_cmd(struct scsi_xfer *);
void		atascsi_disk_cmd_done(struct ata_xfer *);
void		atascsi_disk_inq(struct scsi_xfer *);
void		atascsi_disk_inquiry(struct scsi_xfer *);
void		atascsi_disk_vpd_supported(struct scsi_xfer *);
void		atascsi_disk_vpd_serial(struct scsi_xfer *);
void		atascsi_disk_vpd_ident(struct scsi_xfer *);
void		atascsi_disk_vpd_limits(struct scsi_xfer *);
void		atascsi_disk_vpd_info(struct scsi_xfer *);
void		atascsi_disk_capacity(struct scsi_xfer *);
void		atascsi_disk_sync(struct scsi_xfer *);
void		atascsi_disk_sync_done(struct ata_xfer *);
void		atascsi_disk_sense(struct scsi_xfer *);

void		atascsi_atapi_cmd(struct scsi_xfer *);
void		atascsi_atapi_cmd_done(struct ata_xfer *);

void		atascsi_done(struct scsi_xfer *, int);

void		ata_exec(struct atascsi *, struct ata_xfer *);

struct ata_xfer	*ata_get_xfer(struct ata_port *);
void		ata_put_xfer(struct ata_xfer *);

void		ata_polled_complete(struct ata_xfer *);
int		ata_polled(struct ata_xfer *);

u_int64_t	ata_identify_blocks(struct ata_identify *);
u_int		ata_identify_blocksize(struct ata_identify *);

struct atascsi *
atascsi_attach(struct device *self, struct atascsi_attach_args *aaa)
{
	struct scsibus_attach_args	saa;
	struct atascsi			*as;

	as = malloc(sizeof(*as), M_DEVBUF, M_WAITOK | M_ZERO);

	as->as_dev = self;
	as->as_cookie = aaa->aaa_cookie;
	as->as_methods = aaa->aaa_methods;
	as->as_capability = aaa->aaa_capability;

	/* copy from template and modify for ourselves */
	as->as_switch = atascsi_switch;
	if (aaa->aaa_minphys != NULL)
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

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &as->as_link;

	/* stash the scsibus so we can do hotplug on it */
	as->as_scsibus = (struct scsibus_softc *)config_found(self, &saa,
	    scsiprint);

	return (as);
}

int
atascsi_detach(struct atascsi *as, int flags)
{
	int				rv;

	rv = config_detach((struct device *)as->as_scsibus, flags);
	if (rv != 0)
		return (rv);

	free(as->as_ports, M_DEVBUF);
	free(as, M_DEVBUF);

	return (0);
}

int
atascsi_probe_dev(struct atascsi *as, int port)
{
	return (scsi_probe_target(as->as_scsibus, port));
}

int
atascsi_detach_dev(struct atascsi *as, int port, int flags)
{
	return (scsi_detach_target(as->as_scsibus, port, flags));
}

int
atascsi_probe(struct scsi_link *link)
{
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap;
	struct ata_xfer		*xa;
	int			port, type;
	int			rv;
	u_int16_t		cmdset;

	/* revisit this when we do port multipliers */
	if (link->lun > 0)
		return (ENXIO);

	port = link->target;
	if (port > as->as_link.adapter_buswidth)
		return (ENXIO);

	type = as->as_methods->probe(as->as_cookie, port);
	switch (type) {
	case ATA_PORT_T_DISK:
		break;
	case ATA_PORT_T_ATAPI:
		link->flags |= SDEV_ATAPI;
		link->quirks |= SDEV_ONLYBIG;
		break;
	default:
		rv = ENODEV;
		goto unsupported;
	}

	ap = malloc(sizeof(*ap), M_DEVBUF, M_WAITOK | M_ZERO);
	ap->ap_as = as;
	ap->ap_port = port;
	ap->ap_type = type;

	/* fetch the device info */
	xa = ata_get_xfer(ap);
	if (xa == NULL)
		panic("no free xfers on a new port");
	xa->data = &ap->ap_identify;
	xa->datalen = sizeof(ap->ap_identify);
	xa->fis->flags = ATA_H2D_FLAGS_CMD;
	xa->fis->command = (type == ATA_PORT_T_DISK) ?
	    ATA_C_IDENTIFY : ATA_C_IDENTIFY_PACKET;
	xa->fis->device = 0;
	xa->flags = ATA_F_READ | ATA_F_PIO | ATA_F_POLL;
	xa->timeout = 1000;
	xa->complete = ata_polled_complete;
	ata_exec(as, xa);
	rv = ata_polled(xa);
	if (rv != 0)
		goto error;

	as->as_ports[port] = ap;

	if (type != ATA_PORT_T_DISK)
		return (0);

	cmdset = letoh16(ap->ap_identify.cmdset82);

	/* Enable write cache if supported */
	if (ISSET(cmdset, ATA_IDENTIFY_WRITECACHE)) {
		xa = ata_get_xfer(ap);
		if (xa == NULL)
			panic("no free xfers on a new port");
		xa->fis->command = ATA_C_SET_FEATURES;
		xa->fis->features = ATA_SF_WRITECACHE_EN;
		xa->fis->flags = ATA_H2D_FLAGS_CMD;
		xa->flags = ATA_F_READ | ATA_F_PIO | ATA_F_POLL;
		xa->timeout = 1000;
		xa->complete = ata_polled_complete;
		ata_exec(as, xa);
		ata_polled(xa); /* we dont care if it doesnt work */
	}

	/* Enable read lookahead if supported */
	if (ISSET(cmdset, ATA_IDENTIFY_LOOKAHEAD)) {
		xa = ata_get_xfer(ap);
		if (xa == NULL)
			panic("no free xfers on a new port");
		xa->fis->command = ATA_C_SET_FEATURES;
		xa->fis->features = ATA_SF_LOOKAHEAD_EN;
		xa->fis->flags = ATA_H2D_FLAGS_CMD;
		xa->flags = ATA_F_READ | ATA_F_PIO | ATA_F_POLL;
		xa->timeout = 1000;
		xa->complete = ata_polled_complete;
		ata_exec(as, xa);
		ata_polled(xa); /* we dont care if it doesnt work */
	}

	/*
	 * FREEZE LOCK the device so malicous users can't lock it on us.
	 * As there is no harm in issuing this to devices that don't
	 * support the security feature set we just send it, and don't bother
	 * checking if the device sends a command abort to tell us it doesn't
	 * support it
	 */
	xa = ata_get_xfer(ap);
	if (xa == NULL)
		panic("no free xfers on a new port");
	xa->fis->command = ATA_C_SEC_FREEZE_LOCK;
	xa->fis->flags = ATA_H2D_FLAGS_CMD;
	xa->flags = ATA_F_READ | ATA_F_PIO | ATA_F_POLL;
	xa->timeout = 1000;
	xa->complete = ata_polled_complete;
	ata_exec(as, xa);
	ata_polled(xa); /* we dont care if it doesnt work */

	return (0);
error:
	free(ap, M_DEVBUF);
unsupported:
	as->as_methods->free(as->as_cookie, port);
	return (rv);
}

void
atascsi_free(struct scsi_link *link)
{
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap;
	int			port;

	if (link->lun > 0)
		return;

	port = link->target;
	if (port > as->as_link.adapter_buswidth)
		return;

	ap = as->as_ports[port];
	if (ap == NULL)
		return;

	free(ap, M_DEVBUF);
	as->as_ports[port] = NULL;

	as->as_methods->free(as->as_cookie, port);
}

void
atascsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];

	if (ap == NULL) {
		atascsi_done(xs, XS_DRIVER_STUFFUP);
		return;
	}

	switch (ap->ap_type) {
	case ATA_PORT_T_DISK:
		atascsi_disk_cmd(xs);
		break;
	case ATA_PORT_T_ATAPI:
		atascsi_atapi_cmd(xs);
		break;

	case ATA_PORT_T_NONE:
	default:
		atascsi_done(xs, XS_DRIVER_STUFFUP);
		break;
	}
}

void
atascsi_disk_cmd(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	int			flags = 0;
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
		atascsi_disk_sync(xs);
		return;
	case REQUEST_SENSE:
		atascsi_disk_sense(xs);
		return;
	case INQUIRY:
		atascsi_disk_inq(xs);
		return;
	case READ_CAPACITY:
		atascsi_disk_capacity(xs);
		return;

	case TEST_UNIT_READY:
	case START_STOP:
	case PREVENT_ALLOW:
		atascsi_done(xs, XS_NOERROR);
		return;

	default:
		atascsi_done(xs, XS_DRIVER_STUFFUP);
		return;
	}

	xa = ata_get_xfer(ap);
	if (xa == NULL) {
		atascsi_done(xs, XS_NO_CCB);
		return;
	}

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

	ata_exec(as, xa);
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

	scsi_done(xs);
}

void
atascsi_disk_inq(struct scsi_xfer *xs)
{
	struct scsi_inquiry	*inq = (struct scsi_inquiry *)xs->cmd;

	if (ISSET(inq->flags, SI_EVPD)) {
		switch (inq->pagecode) {
		case SI_PG_SUPPORTED:
			atascsi_disk_vpd_supported(xs);
			break;
		case SI_PG_SERIAL:
			atascsi_disk_vpd_serial(xs);
			break;
		case SI_PG_DEVID:
			atascsi_disk_vpd_ident(xs);
			break;
		case SI_PG_DISK_LIMITS:
			atascsi_disk_vpd_limits(xs);
			break;
		case SI_PG_DISK_INFO:
			atascsi_disk_vpd_info(xs);
			break;
		default:
			atascsi_done(xs, XS_DRIVER_STUFFUP);
			break;
		}
	} else
		atascsi_disk_inquiry(xs);
}

void
atascsi_disk_inquiry(struct scsi_xfer *xs)
{
	struct scsi_link        *link = xs->sc_link;
	struct atascsi          *as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct scsi_inquiry_data inq;

	bzero(&inq, sizeof(inq));

	inq.device = T_DIRECT;
	inq.version = 0x05; /* SPC-3 */
	inq.response_format = 2;
	inq.additional_length = 32;
	bcopy("ATA     ", inq.vendor, sizeof(inq.vendor));
	ata_swapcopy(ap->ap_identify.model, inq.product,
	    sizeof(inq.product));
	ata_swapcopy(ap->ap_identify.firmware, inq.revision,
	    sizeof(inq.revision));

	bcopy(&inq, xs->data, MIN(sizeof(inq), xs->datalen));

	atascsi_done(xs, XS_NOERROR);

	if (ap->ap_features & ATA_PORT_F_PROBED)
		return;

	ap->ap_features = ATA_PORT_F_PROBED;

	if (as->as_capability & ASAA_CAP_NCQ &&
	    (letoh16(ap->ap_identify.satacap) & (1 << 8))) {
		int host_ncqdepth;
		/*
		 * At this point, openings should be the number of commands the
		 * host controller supports, less any reserved slot the host
		 * controller needs for recovery.
		 */
		host_ncqdepth = link->openings +
		    ((as->as_capability & ASAA_CAP_NEEDS_RESERVED) ? 1 : 0);

		ap->ap_ncqdepth = (letoh16(ap->ap_identify.qdepth) & 0x1f) + 1;

		/* Limit the number of openings to what the device supports. */
		if (host_ncqdepth > ap->ap_ncqdepth)
			link->openings -= (host_ncqdepth - ap->ap_ncqdepth);

		/*
		 * XXX throw away any xfers that have tag numbers higher than
		 * what the device supports.
		 */
		while (host_ncqdepth--) {
			struct ata_xfer *xa;

			xa = ata_get_xfer(ap);
			if (xa->tag < ap->ap_ncqdepth) {
				xa->state = ATA_S_COMPLETE;
				ata_put_xfer(xa);
			}
		}
	}
}

void
atascsi_disk_vpd_supported(struct scsi_xfer *xs)
{
	struct {
		struct scsi_vpd_hdr	hdr;
		u_int8_t		list[5];
	}			pg;

	bzero(&pg, sizeof(pg));

	pg.hdr.device = T_DIRECT;
	pg.hdr.page_code = SI_PG_SUPPORTED;
	pg.hdr.page_length = sizeof(pg.list);
	pg.list[0] = SI_PG_SUPPORTED;
	pg.list[1] = SI_PG_SERIAL;
	pg.list[2] = SI_PG_DEVID;
	pg.list[3] = SI_PG_DISK_LIMITS;
	pg.list[4] = SI_PG_DISK_INFO;

	bcopy(&pg, xs->data, MIN(sizeof(pg), xs->datalen));

	atascsi_done(xs, XS_NOERROR);
}

void
atascsi_disk_vpd_serial(struct scsi_xfer *xs)
{
	struct scsi_link        *link = xs->sc_link;
	struct atascsi          *as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct scsi_vpd_serial	pg;

	bzero(&pg, sizeof(pg));

	pg.hdr.device = T_DIRECT;
	pg.hdr.page_code = SI_PG_SERIAL;
	pg.hdr.page_length = sizeof(ap->ap_identify.serial);
	ata_swapcopy(ap->ap_identify.serial, pg.serial,
	    sizeof(ap->ap_identify.serial));

	bcopy(&pg, xs->data, MIN(sizeof(pg), xs->datalen));

	atascsi_done(xs, XS_NOERROR);
}

void
atascsi_disk_vpd_ident(struct scsi_xfer *xs)
{
	struct scsi_link        *link = xs->sc_link;
	struct atascsi          *as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct {
		struct scsi_vpd_hdr	hdr;
		struct scsi_vpd_devid_hdr devid_hdr;
		u_int8_t		devid[68];
	}			pg;
	u_int8_t		*p;
	size_t			pg_len;

	bzero(&pg, sizeof(pg));
	if (letoh16(ap->ap_identify.features87) & ATA_ID_F87_WWN) {
		pg_len = 8;

		pg.devid_hdr.pi_code = VPD_DEVID_CODE_BINARY;
		pg.devid_hdr.flags = VPD_DEVID_ASSOC_LU | VPD_DEVID_TYPE_NAA;

		ata_swapcopy(&ap->ap_identify.naa_ieee_oui, pg.devid, pg_len);
	} else {
		pg_len = 68;

		pg.devid_hdr.pi_code = VPD_DEVID_CODE_ASCII;
		pg.devid_hdr.flags = VPD_DEVID_ASSOC_LU | VPD_DEVID_TYPE_T10;

		p = pg.devid;
		bcopy("ATA     ", p, 8);
		p += 8;
		ata_swapcopy(ap->ap_identify.model, p,
		    sizeof(ap->ap_identify.model));
		p += sizeof(ap->ap_identify.model);
		ata_swapcopy(ap->ap_identify.serial, p,
		    sizeof(ap->ap_identify.serial));
	}

	pg.devid_hdr.len = pg_len;
	pg_len += sizeof(pg.devid_hdr);

	pg.hdr.device = T_DIRECT;
	pg.hdr.page_code = SI_PG_DEVID;
	pg.hdr.page_length = pg_len;
	pg_len += sizeof(pg.hdr);

	bcopy(&pg, xs->data, MIN(pg_len, xs->datalen));

	atascsi_done(xs, XS_NOERROR);
}

void
atascsi_disk_vpd_limits(struct scsi_xfer *xs)
{
	struct scsi_link        *link = xs->sc_link;
	struct atascsi          *as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct scsi_vpd_disk_limits pg;
	u_int16_t		p2l_sect;

	bzero(&pg, sizeof(pg));
	pg.hdr.device = T_DIRECT;
	pg.hdr.page_code = SI_PG_DISK_LIMITS;
	pg.hdr.page_length = SI_PG_DISK_LIMITS_LEN_THIN;

	p2l_sect = letoh16(ap->ap_identify.p2l_sect);
	if ((p2l_sect & ATA_ID_P2L_SECT_MASK) == ATA_ID_P2L_SECT_VALID &&
	    ISSET(p2l_sect, ATA_ID_P2L_SECT_SET)) {
		_lto2b(2 << (p2l_sect & SI_PG_DISK_LIMITS_LEN_THIN),
		    pg.optimal_xfer_granularity);
	} else
		_lto2b(1, pg.optimal_xfer_granularity);

	bcopy(&pg, xs->data, MIN(sizeof(pg), xs->datalen));

	atascsi_done(xs, XS_NOERROR);
}

void
atascsi_disk_vpd_info(struct scsi_xfer *xs)
{
	struct scsi_link        *link = xs->sc_link;
	struct atascsi          *as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct scsi_vpd_disk_info pg;

	bzero(&pg, sizeof(pg));
	pg.hdr.device = T_DIRECT;
	pg.hdr.page_code = SI_PG_DISK_INFO;
	pg.hdr.page_length = sizeof(pg) - sizeof(pg.hdr);

	_lto2b(letoh16(ap->ap_identify.rpm), pg.rpm);
	pg.form_factor = letoh16(ap->ap_identify.form) & ATA_ID_FORM_MASK;

	bcopy(&pg, xs->data, MIN(sizeof(pg), xs->datalen));

	atascsi_done(xs, XS_NOERROR);
}

void
atascsi_disk_sync(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct ata_xfer		*xa;

	xa = ata_get_xfer(ap);
	if (xa == NULL) {
		atascsi_done(xs, XS_NO_CCB);
		return;
	}

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

	ata_exec(as, xa);
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

	scsi_done(xs);
}

u_int64_t
ata_identify_blocks(struct ata_identify *id)
{
	u_int64_t		blocks = 0;
	int			i;

	if (letoh16(id->cmdset83) & 0x0400) {
		/* LBA48 feature set supported */
		for (i = 3; i >= 0; --i) {
			blocks <<= 16;
			blocks += letoh16(id->addrsecxt[i]);
		}
	} else {
		blocks = letoh16(id->addrsec[1]);
		blocks <<= 16;
		blocks += letoh16(id->addrsec[0]);
	}

	return (blocks - 1);
}

u_int
ata_identify_blocksize(struct ata_identify *id)
{
	u_int			blocksize = 512;
	u_int16_t		p2l_sect = letoh16(id->p2l_sect);
	
	if ((p2l_sect & ATA_ID_P2L_SECT_MASK) == ATA_ID_P2L_SECT_VALID &&
	    ISSET(p2l_sect, ATA_ID_P2L_SECT_SIZESET)) {
		blocksize = letoh16(id->words_lsec[1]);
		blocksize <<= 16;
		blocksize += letoh16(id->words_lsec[0]);
		blocksize <<= 1;
	}

	return (blocksize);
}

void
atascsi_disk_capacity(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct scsi_read_cap_data rcd;
	u_int64_t		capacity;

	bzero(&rcd, sizeof(rcd));
	capacity = ata_identify_blocks(&ap->ap_identify);
	if (capacity > 0xffffffff)
		capacity = 0xffffffff;

	_lto4b(capacity, rcd.addr);
	_lto4b(ata_identify_blocksize(&ap->ap_identify), rcd.length);

	bcopy(&rcd, xs->data, MIN(sizeof(rcd), xs->datalen));

	atascsi_done(xs, XS_NOERROR);
}

void
atascsi_disk_sense(struct scsi_xfer *xs)
{
	struct scsi_sense_data	*sd = (struct scsi_sense_data *)xs->data;

	bzero(xs->data, xs->datalen);
	/* check datalen > sizeof(struct scsi_sense_data)? */
	sd->error_code = 0x70; /* XXX magic */
	sd->flags = SKEY_NO_SENSE;

	atascsi_done(xs, XS_NOERROR);
}

void
atascsi_atapi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct ata_xfer		*xa;
	struct ata_fis_h2d	*fis;

	xa = ata_get_xfer(ap);
	if (xa == NULL) {
		atascsi_done(xs, XS_NO_CCB);
		return;
	}

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

	ata_exec(as, xa);
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

	scsi_done(xs);
}

void
atascsi_done(struct scsi_xfer *xs, int error)
{
	int			s;

	xs->error = error;

	s = splbio();
	scsi_done(xs);
	splx(s);
}

int atascsi_ioctl_cmd(struct atascsi *, struct ata_port *, atareq_t *);
void atascsi_ioctl_done(struct ata_xfer *);

int
atascsi_ioctl(struct scsi_link *link, u_long cmd, caddr_t addr, int flags,
    struct proc *p)
{
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];

	switch (cmd) {
	case ATAIOCCOMMAND:
		return (atascsi_ioctl_cmd(as, ap, (atareq_t *)addr));
	default:
		return (ENOTTY);
	}
}

int
atascsi_ioctl_cmd(struct atascsi *as, struct ata_port *ap, atareq_t *atareq)
{
	struct ata_xfer		*xa;
	struct ata_fis_h2d	*fis;
	void			*buf;
	int			 rc = 0;
	int			 s;

	xa = ata_get_xfer(ap);
	if (xa == NULL)
		return (ENOMEM);

	fis = xa->fis;
	fis->flags = ATA_H2D_FLAGS_CMD;
	fis->command = atareq->command;
	fis->features = atareq->features;
	fis->lba_low = atareq->sec_num;
	fis->lba_mid = atareq->cylinder;
	fis->lba_high = atareq->cylinder >> 8;
	fis->device = atareq->head & 0x0f;
	fis->sector_count = atareq->sec_count;

	buf = malloc(atareq->datalen, M_TEMP, M_WAITOK);

	xa->data = buf;
	xa->datalen = atareq->datalen;
	xa->complete = atascsi_ioctl_done;
	xa->timeout = atareq->timeout;
	xa->flags = 0;
	if (atareq->flags & ATACMD_READ)
		xa->flags |= ATA_F_READ;
	if (atareq->flags & ATACMD_WRITE) {
		xa->flags |= ATA_F_WRITE;
		copyin(atareq->databuf, buf, atareq->datalen);
	}
	xa->atascsi_private = NULL;

	as->as_methods->ata_cmd(xa);
	s = splbio();
	while (!ISSET(xa->flags, ATA_F_DONE))
		tsleep(xa, PRIBIO, "atascsi", 0);
	splx(s);

	switch (xa->state) {
	case ATA_S_COMPLETE:
		atareq->retsts = ATACMD_OK;
		if (atareq->flags & ATACMD_READ)
			rc = copyout(buf, atareq->databuf, atareq->datalen);
		break;
	case ATA_S_ERROR:
		atareq->retsts = ATACMD_ERROR;
		break;
	case ATA_S_TIMEOUT:
		atareq->retsts = ATACMD_TIMEOUT;
		break;
	default:
		panic("atascsi_ioctl_cmd: unexpected ata_xfer state (%d)",
		    xa->state);
	}

	free(buf, M_TEMP);

	ata_put_xfer(xa);

	return (rc);
}

void
atascsi_ioctl_done(struct ata_xfer *xa)
{
	wakeup(xa);
}

void
ata_exec(struct atascsi *as, struct ata_xfer *xa)
{
	as->as_methods->ata_cmd(xa);
}

struct ata_xfer *
ata_get_xfer(struct ata_port *ap)
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

void
ata_polled_complete(struct ata_xfer *xa)
{
	/* do nothing */
}

int
ata_polled(struct ata_xfer *xa)
{
	int			rv;

	if (!ISSET(xa->flags, ATA_F_DONE))
		panic("ata_polled: xa isnt complete");

	switch (xa->state) {
	case ATA_S_COMPLETE:
		rv = 0;
		break;
	case ATA_S_ERROR:
	case ATA_S_TIMEOUT:
		rv = EIO;
		break;
	default:
		panic("ata_polled: xa state (%d)",
		    xa->state);
	}

	ata_put_xfer(xa);

	return (rv);
}

void
ata_complete(struct ata_xfer *xa)
{
	SET(xa->flags, ATA_F_DONE);
	xa->complete(xa);
}

void
ata_swapcopy(void *src, void *dst, size_t len)
{
	u_int16_t *s = src, *d = dst;
	int i;

	len /= 2;

	for (i = 0; i < len; i++)
		d[i] = swap16(s[i]);
}

