/*	$OpenBSD: atascsi.c,v 1.20 2007/03/20 07:09:42 pascoe Exp $ */

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

#include <dev/ic/wdcreg.h>

/* XXX ata_identify should be in atareg.h */

#define ATA_C_IDENTIFY		0xec

struct ata_identify {
	u_int16_t	config;		/*   0 */
	u_int16_t	ncyls;		/*   1 */
	u_int16_t	reserved1;	/*   2 */
	u_int16_t	nheads;		/*   3 */
	u_int16_t	track_size;	/*   4 */
	u_int16_t	sector_size;	/*   5 */
	u_int16_t	nsectors;	/*   6 */
	u_int16_t	reserved2[3];	/*   7 vendor unique */
	u_int8_t	serial[20];	/*  10 */
	u_int16_t	buffer_type;	/*  20 */
	u_int16_t	buffer_size;	/*  21 */
	u_int16_t	ecc;		/*  22 */
	u_int8_t	firmware[8];	/*  23 */
	u_int8_t	model[40];	/*  27 */
	u_int16_t	multi;		/*  47 */
	u_int16_t	dwcap;		/*  48 */
	u_int16_t	cap;		/*  49 */
	u_int16_t	reserved3;	/*  50 */
	u_int16_t	piomode;	/*  51 */
	u_int16_t	dmamode;	/*  52 */
	u_int16_t	validinfo;	/*  53 */
	u_int16_t	curcyls;	/*  54 */
	u_int16_t	curheads;	/*  55 */
	u_int16_t	cursectrk;	/*  56 */
	u_int16_t	curseccp[2];	/*  57 */
	u_int16_t	mult2;		/*  59 */
	u_int16_t	addrsec[2];	/*  60 */
	u_int16_t	worddma;	/*  62 */
	u_int16_t	dworddma;	/*  63 */
	u_int16_t	advpiomode;	/*  64 */
	u_int16_t	minmwdma;	/*  65 */
	u_int16_t	recmwdma;	/*  66 */
	u_int16_t	minpio;		/*  67 */
	u_int16_t	minpioflow;	/*  68 */
	u_int16_t	reserved4[2];	/*  69 */
	u_int16_t	typtime[2];	/*  71 */
	u_int16_t	reserved5[2];	/*  73 */
	u_int16_t	qdepth;		/*  75 */
	u_int16_t	satacap;	/*  76 */
	u_int16_t	reserved6;	/*  77 */
	u_int16_t	satafsup;	/*  78 */
	u_int16_t	satafen;	/*  79 */
	u_int16_t	majver;		/*  80 */
	u_int16_t	minver;		/*  81 */
	u_int16_t	cmdset82;	/*  82 */
	u_int16_t	cmdset83;	/*  83 */
	u_int16_t	cmdset84;	/*  84 */
	u_int16_t	features85;	/*  85 */
	u_int16_t	features86;	/*  86 */
	u_int16_t	features87;	/*  87 */
	u_int16_t	ultradma;	/*  88 */
	u_int16_t	erasetime;	/*  89 */
	u_int16_t	erasetimex;	/*  90 */
	u_int16_t	apm;		/*  91 */
	u_int16_t	masterpw;	/*  92 */
	u_int16_t	hwreset;	/*  93 */
	u_int16_t	acoustic;	/*  94 */
	u_int16_t	stream_min;	/*  95 */
	u_int16_t	stream_xfer_d;	/*  96 */
	u_int16_t	stream_lat;	/*  97 */
	u_int16_t	streamperf[2];	/*  98 */
	u_int16_t	addrsecxt[4];	/* 100 */
	u_int16_t	stream_xfer_p;	/* 104 */
	u_int16_t	padding1;	/* 105 */
	u_int16_t	phys_sect_sz;	/* 106 */
	u_int16_t	seek_delay;	/* 107 */
	u_int16_t	naa_ieee_oui;	/* 108 */
	u_int16_t	ieee_oui_uid;	/* 109 */
	u_int16_t	uid_mid;	/* 110 */
	u_int16_t	uid_low;	/* 111 */
	u_int16_t	resv_wwn[4];	/* 112 */
	u_int16_t	incits;		/* 116 */
	u_int16_t	words_lsec[2];	/* 117 */
	u_int16_t	cmdset119;	/* 119 */
	u_int16_t	features120;	/* 120 */
	u_int16_t	padding2[6];
	u_int16_t	rmsn;		/* 127 */
	u_int16_t	securestatus;	/* 128 */
	u_int16_t	vendor[31];	/* 129 */
	u_int16_t	padding3[16];	/* 160 */
	u_int16_t	curmedser[30];	/* 176 */
	u_int16_t	sctsupport;	/* 206 */
	u_int16_t	padding4[48];	/* 207 */
	u_int16_t	integrity;	/* 255 */
} __packed;

struct atascsi {
	struct device		*as_dev;
	void			*as_cookie;

	struct ata_port		**as_ports;

	struct atascsi_methods	*as_methods;
	struct scsi_adapter	as_switch;
	struct scsi_link	as_link;
	struct scsibus_softc	*as_scsibus;
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
int		atascsi_disk_sense(struct scsi_xfer *);

int		atascsi_atapi_cmd(struct scsi_xfer *);

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

	as = malloc(sizeof(struct atascsi), M_DEVBUF, M_WAITOK);
	bzero(as, sizeof(struct atascsi));

	as->as_dev = self;
	as->as_cookie = aaa->aaa_cookie;
	as->as_methods = aaa->aaa_methods;

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

	as->as_ports = malloc(sizeof(struct ata_port *) * aaa->aaa_nports,
	    M_DEVBUF, M_WAITOK);
	bzero(as->as_ports, sizeof(struct ata_port *) * aaa->aaa_nports);

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
	int			type;

	if (port > as->as_link.adapter_buswidth)
		return (ENXIO);

	type = as->as_methods->probe(as->as_cookie, port);
	if (type != ATA_PORT_T_DISK) /* XXX ATAPI too one day */
		return (ENXIO);

	ap = malloc(sizeof(struct ata_port), M_DEVBUF, M_WAITOK);
	ap->ap_as = as;
	ap->ap_port = port;
	ap->ap_type = type;

	as->as_ports[port] = ap;

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

	xa->data = malloc(512, M_TEMP, nosleep ? M_NOWAIT : M_WAITOK);
	if (xa->data == NULL) {
		s = splbio();
		ata_put_xfer(xa);
		splx(s);
		return (NULL);
	}
	bzero(xa->data, 512);
	xa->datalen = 512;

	xa->cmd.tx->regs[H2D_DEVCTL_OR_COMMAND] = H2D_DEVCTL_OR_COMMAND_COMMAND;
	xa->cmd.tx->regs[H2D_COMMAND] = ATA_C_IDENTIFY;

	xa->cmd.st_bmask = 0x40; /* XXX magic WDCS_DRDY */;
	xa->cmd.st_pmask = 0x00;

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
	u_int64_t		lba;
	u_int32_t		sector_count;
	u_int8_t		*regs;

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

	regs = xa->cmd.tx->regs;

	regs[H2D_DEVCTL_OR_COMMAND] = H2D_DEVCTL_OR_COMMAND_COMMAND;
	regs[LBA_LOW] = lba & 0xff;
	regs[LBA_MID] = (lba >> 8) & 0xff;
	regs[LBA_HIGH] = (lba >> 16) & 0xff;

	if (xs->flags & SCSI_POLL)
		xa->flags |= ATA_F_POLL;

	if (sector_count > 0x100 || lba > 0xfffffff) {
		/* Use LBA48 */
		regs[H2D_COMMAND] = (xa->flags & ATA_F_WRITE) ?
		    WDCC_WRITEDMA_EXT : WDCC_READDMA_EXT;
		regs[DEVICE] = WDSD_LBA;
		regs[LBA_LOW_EXP] = (lba >> 24) & 0xff;
		regs[LBA_MID_EXP] = (lba >> 32) & 0xff;
		regs[LBA_HIGH_EXP] = (lba >> 40) & 0xff;
		regs[SECTOR_COUNT] = sector_count & 0xff;
		regs[SECTOR_COUNT_EXP] = (sector_count >> 8) & 0xff;
	} else {
		/* Use LBA */
		regs[H2D_COMMAND] = (xa->flags & ATA_F_WRITE) ?
		    WDCC_WRITEDMA : WDCC_READDMA;
		regs[DEVICE] = WDSD_LBA | ((lba >> 24) & 0x0f);
		regs[SECTOR_COUNT] = sector_count & 0xff;
	}

	xa->data = xs->data;
	xa->datalen = xs->datalen;
	xa->complete = atascsi_disk_cmd_done;
	xa->timeout = xs->timeout;
	xa->atascsi_private = xs;

	switch (ata_exec(as, xa)) {
	case ATA_COMPLETE:
	case ATA_ERROR:
		return (COMPLETE);
	case ATA_QUEUED:
		return (SUCCESSFULLY_QUEUED);
	default:
		panic("unexpected return from ata_exec");
	}
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
		printf("%s: error\n", __FUNCTION__);
		xs->error = XS_DRIVER_STUFFUP;
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
	int			s;

	xa = ata_setup_identify(ap, xs->flags & SCSI_NOSLEEP);
	if (xa == NULL)
		return (NO_CCB);

	xa->complete = atascsi_disk_inq_done;
	xa->timeout = xs->timeout;
	xa->atascsi_private = xs;
	if (xs->flags & SCSI_POLL)
		xa->flags |= ATA_F_POLL;

	switch (ata_exec(as, xa)) {
	case ATA_COMPLETE:
		return (COMPLETE);
	case ATA_QUEUED:
		return (SUCCESSFULLY_QUEUED);
	case ATA_ERROR:
		s = splbio();
		ata_free_identify(xa);
		splx(s);
		return (atascsi_stuffup(xs));
	default:
		panic("unexpected return from ata_exec");
	}
}

void
atascsi_disk_inq_done(struct ata_xfer *xa)
{
	struct scsi_xfer	*xs = xa->atascsi_private;
	struct ata_identify	id;
	struct scsi_inquiry_data inq;

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
	xs->flags |= ITSDONE;

	scsi_done(xs);
}

int
atascsi_disk_sync(struct scsi_xfer *xs)
{
	return (atascsi_stuffup(xs));
}

int
atascsi_disk_capacity(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct atascsi		*as = link->adapter_softc;
	struct ata_port		*ap = as->as_ports[link->target];
	struct ata_xfer		*xa;
	int			s;

	xa = ata_setup_identify(ap, xs->flags & SCSI_NOSLEEP);
	if (xa == NULL)
		return (NO_CCB);

	xa->complete = atascsi_disk_capacity_done;
	xa->timeout = xs->timeout;
	xa->atascsi_private = xs;
	if (xs->flags & SCSI_POLL)
		xa->flags |= ATA_F_POLL;

	switch (ata_exec(as, xa)) {
	case ATA_COMPLETE:
		return (COMPLETE);
	case ATA_QUEUED:
		return (SUCCESSFULLY_QUEUED);
	case ATA_ERROR:
		s = splbio();
		ata_free_identify(xa);
		splx(s);
		return (atascsi_stuffup(xs));
	default:
		panic("unexpected return from ata_exec");
	}
}

void
atascsi_disk_capacity_done(struct ata_xfer *xa)
{
	struct scsi_xfer	*xs = xa->atascsi_private;
	struct ata_identify	id;
	struct scsi_read_cap_data rcd;
	u_int32_t		capacity;
	int			i;

	ata_complete_identify(xa, &id);

	bzero(&rcd, sizeof(rcd));
	if (id.cmdset83 & 0x0400) {
		for (i = 3; i >= 0; --i) {
			capacity <<= 16;
			capacity += id.addrsecxt[i];
		}
	} else {
		capacity = id.addrsec[1];
		capacity <<= 16;
		capacity += id.addrsec[0];
	}

	_lto4b(capacity - 1, rcd.addr);
	_lto4b(512, rcd.length);

	bcopy(&rcd, xs->data, MIN(sizeof(rcd), xs->datalen));
	xs->error = XS_NOERROR;
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
	return (atascsi_stuffup(xs));
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
	return (as->as_methods->ata_cmd(xa));
}


struct ata_xfer *
ata_get_xfer(struct ata_port *ap, int nosleep /* XXX unused */)
{
	return (ap->ap_as->as_methods->ata_get_xfer(ap->ap_as->as_cookie,
	    ap->ap_port));
}

void
ata_put_xfer(struct ata_xfer *xa)
{
	xa->ata_put_xfer(xa);
}
