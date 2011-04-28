/*	$OpenBSD: mpath_emc.c,v 1.4 2011/04/28 10:43:36 dlg Exp $ */

/*
 * Copyright (c) 2011 David Gwynne <dlg@openbsd.org>
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

/* EMC CLARiiON AX/CX support for mpath(4) */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/pool.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/selinfo.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/mpathvar.h>

#define EMC_VPD_SP_INFO			0xc0

struct emc_vpd_sp_info {
	struct scsi_vpd_hdr	hdr; /* EMC_VPD_SP_INFO */

	u_int8_t		lun_state;
#define EMC_SP_INFO_LUN_STATE_UNBOUND	0x00
#define EMC_SP_INFO_LUN_STATE_BOUND	0x01
#define EMC_SP_INFO_LUN_STATE_OWNED	0x02
	u_int8_t		default_sp;
	u_int8_t		_reserved1[1];
	u_int8_t		port;
	u_int8_t		current_sp;
	u_int8_t		_reserved2[1];
	u_int8_t		unique_id[16];
	u_int8_t		_reserved3[1];
	u_int8_t		type;
	u_int8_t		failover_mode;
	u_int8_t		_reserved4[21];
	u_int8_t		serial[16];
} __packed;

struct emc_softc {
	struct device		sc_dev;
	struct mpath_path	sc_path;
	u_int			sc_flags;
	u_int8_t		sc_sp;
	u_int8_t		sc_port;
	u_int8_t		sc_lun_state;

};
#define DEVNAME(_s) ((_s)->sc_dev.dv_xname)

int		emc_match(struct device *, void *, void *);
void		emc_attach(struct device *, struct device *, void *);
int		emc_detach(struct device *, int);
int		emc_activate(struct device *, int);

struct cfattach emc_ca = {
	sizeof(struct emc_softc),
	emc_match,
	emc_attach,
	emc_detach,
	emc_activate
};

struct cfdriver emc_cd = {
	NULL,
	"emc",
	DV_DULL
};

void		emc_mpath_start(struct scsi_xfer *);
int		emc_mpath_checksense(struct scsi_xfer *);
int		emc_mpath_online(struct scsi_link *);
int		emc_mpath_offline(struct scsi_link *);

struct mpath_ops emc_mpath_ops = {
	"emc",
	emc_mpath_checksense,
	emc_mpath_online,
	emc_mpath_offline,
};

struct emc_device {
	char *vendor;
	char *product;
};

int		emc_inquiry(struct emc_softc *, char *, char *);
int		emc_sp_info(struct emc_softc *);

struct emc_device emc_devices[] = {
/*	  " vendor "  "     device     " */
/*	  "01234567"  "0123456789012345" */
	{ "DGC     ", "LUNZ" },
	{ "DGC     ", "RAID" },
	{ "DGC     ", "DISK" },
	{ "DGC     ", "VRAID" }
};

int
emc_match(struct device *parent, void *match, void *aux)
{
	struct scsi_attach_args *sa = aux;
	struct scsi_inquiry_data *inq = sa->sa_inqbuf;
	struct emc_device *s;
	int i;

	if (mpath_path_probe(sa->sa_sc_link) != 0)
		return (0);

	for (i = 0; i < nitems(emc_devices); i++) {
		s = &emc_devices[i];

		if (bcmp(s->vendor, inq->vendor, strlen(s->vendor)) == 0 &&
		    bcmp(s->product, inq->product, strlen(s->product)) == 0)
			return (3);
	}

	return (0);
}

void
emc_attach(struct device *parent, struct device *self, void *aux)
{
	char model[256], serial[256];
	struct emc_softc *sc = (struct emc_softc *)self;
	struct scsi_attach_args *sa = aux;
	struct scsi_link *link = sa->sa_sc_link;

	printf("\n");

	/* init link */
	link->device_softc = sc;

	/* init path */
	scsi_xsh_set(&sc->sc_path.p_xsh, link, emc_mpath_start);
	sc->sc_path.p_link = link;
	sc->sc_path.p_ops = &emc_mpath_ops;

	if (emc_sp_info(sc)) {
		printf("%s: unable to get sp info\n", DEVNAME(sc));
		return;
	}

	if (emc_inquiry(sc, model, serial) != 0) {
		printf("%s: unable to get inquiry data\n", DEVNAME(sc));
		return;
	}

	printf("%s: %s %s SP-%c port %d\n", DEVNAME(sc), model, serial,
	    sc->sc_sp + 'A', sc->sc_port);

	if (sc->sc_lun_state == EMC_SP_INFO_LUN_STATE_OWNED) {
		if (mpath_path_attach(&sc->sc_path) != 0)
			printf("%s: unable to attach path\n", DEVNAME(sc));
	}
}

int
emc_detach(struct device *self, int flags)
{
	return (0);
}

int
emc_activate(struct device *self, int act)
{
	struct emc_softc *sc = (struct emc_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
	case DVACT_SUSPEND:
	case DVACT_RESUME:
		break;
	case DVACT_DEACTIVATE:
		if (sc->sc_path.p_dev != NULL)
			mpath_path_detach(&sc->sc_path);
		break;
	}
	return (rv);
}

void
emc_mpath_start(struct scsi_xfer *xs)
{
	struct emc_softc *sc = xs->sc_link->device_softc;

	mpath_start(&sc->sc_path, xs);
}

int
emc_mpath_checksense(struct scsi_xfer *xs)
{
	return (0);
}

int
emc_mpath_online(struct scsi_link *link)
{
	return (0);
}

int
emc_mpath_offline(struct scsi_link *link)
{
	return (0);
}

int
emc_inquiry(struct emc_softc *sc, char *model, char *serial)
{
	u_int8_t *buffer;
	struct scsi_inquiry *cdb;
	struct scsi_xfer *xs;
	size_t length;
	int error;
	u_int8_t slen, mlen;

	length = MIN(sc->sc_path.p_link->inqdata.additional_length + 5, 255);
	if (length < 160) {
		printf("%s: FC (Legacy)\n");
		return (0);
	}

	buffer = dma_alloc(length, PR_WAITOK);

	xs = scsi_xs_get(sc->sc_path.p_link, scsi_autoconf);
	if (xs == NULL) {
		error = EBUSY;
		goto done;
	}

	cdb = (struct scsi_inquiry *)xs->cmd;
	cdb->opcode = INQUIRY;
	_lto2b(length, cdb->length);

	xs->cmdlen = sizeof(*cdb);
	xs->flags |= SCSI_DATA_IN;
	xs->data = buffer;
	xs->datalen = length;

	error = scsi_xs_sync(xs);
	scsi_xs_put(xs);

	if (error != 0)
		goto done;

	slen = buffer[160];
	if (slen == 0 || slen + 161 > length) {
		error = EIO;
		goto done;
	}

	mlen = buffer[99];
	if (mlen == 0 || slen + mlen + 161 > length) {
		error = EIO;
		goto done;
	}

	scsi_strvis(serial, buffer + 161, slen);
	scsi_strvis(model, buffer + 161 + slen, mlen);

	error = 0;
done:
	dma_free(buffer, length);
	return (error);
}

int
emc_sp_info(struct emc_softc *sc)
{
	struct emc_vpd_sp_info *pg;
	int error;

	pg = dma_alloc(sizeof(*pg), PR_WAITOK);

	error = scsi_inquire_vpd(sc->sc_path.p_link, &pg, sizeof(pg),
	    EMC_VPD_SP_INFO, scsi_autoconf);
	if (error != 0)
		goto done;

	sc->sc_sp = pg->current_sp;
	sc->sc_port = pg->port;
	sc->sc_lun_state = pg->lun_state;

	error = 0;
done:
	dma_free(pg, sizeof(*pg));
	return (error);
}
