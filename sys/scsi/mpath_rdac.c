/*	$OpenBSD: mpath_rdac.c,v 1.3 2011/04/27 09:09:36 dlg Exp $ */

/*
 * Copyright (c) 2010 David Gwynne <dlg@openbsd.org>
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

/* Redundant Disk Array Controller support for mpath(4) */

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

struct rdac_common_mode_page {
	u_int8_t	controller_serial[16];
	u_int8_t	alt_controller_serial[16];
	u_int8_t	mode[2];
	u_int8_t	alt_mode[2];
	u_int8_t	timeout;
	u_int8_t	options;
};

/*
 * RDAC VPD pages
 */
#define RDAC_VPD_HDWVER		0xc0	/* Hardware Version */
#define RDAC_VPD_SERNUM		0xc1	/* Serial Numbers */
#define RDAC_VPD_SFWVER		0xc2
#define RDAC_VPD_FEAPAR		0xc3	/* Feature Parameters */
#define RDAC_VPD_SUBSYS		0xc4
#define RDAC_VPD_HSTINT		0xc5
#define RDAC_VPD_DGM		0xc6
#define RDAC_VPD_HSTINT2	0xc7
#define RDAC_VPD_EXTDEVID	0xc8
#define RDAC_VPD_VOLACCESSCTL	0xc9

struct rdac_vpd_hdwver {
	struct scsi_vpd_hdr	hdr; /* RDAC_VPD_HDWVER */
	u_int8_t		pg_id[4];
#define RDAC_VPD_ID_HDWVER		0x68777234 /* "hwr4" */
	u_int8_t		num_channels;
	u_int8_t		flags;
	u_int8_t		proc_memory_size;
	u_int8_t		_reserved1[5];
	u_int8_t		board_name[64];
	u_int8_t		board_part_number[16];
	u_int8_t		schematic_number[12];
	u_int8_t		schematic_revision[4];
	u_int8_t		serial_number[16];
	u_int8_t		_reserved2[16];
	u_int8_t		date_manufactured[8];
	u_int8_t		board_revision[2];
	u_int8_t		board_identifier[4];
};

struct rdac_vpd_subsys {
	struct scsi_vpd_hdr	hdr; /* RDAC_VPD_SUBSYS */
	u_int8_t		pg_id[4];
#define RDAC_VPD_ID_SUBSYS		0x73756273 /* "subs" */
	u_int8_t		subsystem_id[16];
	u_int8_t		subsystem_revision[4];
	u_int8_t		controller_slot_id[2];
	u_int8_t		_reserved[2];
};

struct rdac_vpd_extdevid {
	struct scsi_vpd_hdr	hdr; /* RDAC_VPD_EXTDEVID */
	u_int8_t		pg_id[4];
#define RDAC_VPD_ID_EXTDEVID		0x65646964 /* "edid" */
	u_int8_t		_reserved[3];
	u_int8_t		vol_id_len;
	u_int8_t		vol_id[16];
	u_int8_t		vol_label_len;
	u_int8_t		vol_label[60];
	u_int8_t		array_id_len;
	u_int8_t		array_id[16];
	u_int8_t		array_label_len;
	u_int8_t		array_label[60];
	u_int8_t		lun[8];
};

struct rdac_vpd_volaccessctl {
	struct scsi_vpd_hdr	hdr; /* RDAC_VPD_VOLACCESSCTL */
	u_int8_t		pg_id[4];
#define RDAC_VPD_ID_VOLACCESSCTL	0x76616331 /* "vac1" */
	u_int8_t		avtcvp;
#define RDAC_VOLACCESSCTL_OWNER		0x01
#define RDAC_VOLACCESSCTL_AVT		0x70
	u_int8_t		path_priority;
	u_int8_t		_reserved[38];
};

struct rdac_softc {
	struct device		sc_dev;
	struct mpath_path	sc_path;
};
#define DEVNAME(_s) ((_s)->sc_dev.dv_xname)

int		rdac_match(struct device *, void *, void *);
void		rdac_attach(struct device *, struct device *, void *);
int		rdac_detach(struct device *, int);
int		rdac_activate(struct device *, int);

struct cfattach rdac_ca = {
	sizeof(struct rdac_softc),
	rdac_match,
	rdac_attach,
	rdac_detach,
	rdac_activate
};

struct cfdriver rdac_cd = {
	NULL,
	"rdac",
	DV_DULL
};

void		rdac_mpath_start(struct scsi_xfer *);
int		rdac_mpath_checksense(struct scsi_xfer *);
int		rdac_mpath_online(struct scsi_link *);
int		rdac_mpath_offline(struct scsi_link *);

struct mpath_ops rdac_mpath_ops = {
	"rdac",
	rdac_mpath_start,
	rdac_mpath_checksense,
	rdac_mpath_online,
	rdac_mpath_offline,
};

int		rdac_c8(struct rdac_softc *);
int		rdac_c9(struct rdac_softc *);

struct rdac_device {
	char *vendor;
	char *product;
};

struct rdac_device rdac_devices[] = {
/*	  " vendor "  "     device     " */
/*	  "01234567"  "0123456789012345" */
	{ "SUN     ", "CSM200_" },
	{ "DELL    ", "MD3000i         " }
};

int
rdac_match(struct device *parent, void *match, void *aux)
{
	struct scsi_attach_args *sa = aux;
	struct scsi_inquiry_data *inq = sa->sa_inqbuf;
	struct rdac_device *s;
	int i;

	if (mpath_path_probe(sa->sa_sc_link) != 0)
		return (0);

	for (i = 0; i < nitems(rdac_devices); i++) {
		s = &rdac_devices[i];

		if (bcmp(s->vendor, inq->vendor, strlen(s->vendor)) == 0 &&
		    bcmp(s->product, inq->product, strlen(s->product)) == 0)
			return (3);
	}

	return (0);
}

void
rdac_attach(struct device *parent, struct device *self, void *aux)
{
	struct rdac_softc *sc = (struct rdac_softc *)self;
	struct scsi_attach_args *sa = aux;
	struct scsi_link *link = sa->sa_sc_link;

	printf("\n");

	/* init link */
	link->device_softc = sc;

	/* init path */
	scsi_xsh_set(&sc->sc_path.p_xsh, link, rdac_mpath_start);
	sc->sc_path.p_link = link;
	sc->sc_path.p_ops = &rdac_mpath_ops;

	if (rdac_c8(sc) != 0)
		return;

	if (rdac_c9(sc) != 0)
		return;

	if (mpath_path_attach(&sc->sc_path) != 0)
		printf("%s: unable to attach path\n", DEVNAME(sc));
}

int
rdac_detach(struct device *self, int flags)
{
	return (0);
}

int
rdac_activate(struct device *self, int act)
{
	struct rdac_softc *sc = (struct rdac_softc *)self;
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
rdac_mpath_start(struct scsi_xfer *xs)
{
	struct rdac_softc *sc = xs->sc_link->device_softc;

	mpath_start(&sc->sc_path, xs);
}

int
rdac_mpath_checksense(struct scsi_xfer *xs)
{
	return (0);
}

int
rdac_mpath_online(struct scsi_link *link)
{
	return (0);
}

int
rdac_mpath_offline(struct scsi_link *link)
{
	return (0);
}

int
rdac_c8(struct rdac_softc *sc)
{
	struct rdac_vpd_extdevid *pg;
	char array[31];
	char vol[31];
	int i;
	int rv = 1;

	pg = dma_alloc(sizeof(*pg), PR_WAITOK | PR_ZERO);

	if (scsi_inquire_vpd(sc->sc_path.p_link, pg, sizeof(*pg), 0xc8,
	    scsi_autoconf) != 0) {
		printf("%s: unable to fetch vpd page c8\n", DEVNAME(sc));
		goto done;
	}

	if (_4btol(pg->pg_id) != RDAC_VPD_ID_EXTDEVID) {
		printf("%s: extended hardware id page is invalid\n",
		    DEVNAME(sc));
		goto done;
	}

	memset(array, 0, sizeof(array));
	for (i = 0; i < sizeof(pg->array_label) / 2; i++)
		array[i] = pg->array_label[i * 2 + 1];

	memset(vol, 0, sizeof(vol));
	for (i = 0; i < sizeof(pg->vol_label) / 2; i++)
		vol[i] = pg->vol_label[i * 2 + 1];

	printf("%s: array %s, volume %s\n", DEVNAME(sc), array, vol);

	rv = 0;
done:
	dma_free(pg, sizeof(*pg));
	return (rv);
}

int
rdac_c9(struct rdac_softc *sc)
{
	struct rdac_vpd_volaccessctl *pg;
	int rv = 1;

	pg = dma_alloc(sizeof(*pg), PR_WAITOK | PR_ZERO);

	if (scsi_inquire_vpd(sc->sc_path.p_link, pg, sizeof(*pg),
	    RDAC_VPD_VOLACCESSCTL, scsi_autoconf) != 0) {
		printf("%s: unable to fetch vpd page c9\n", DEVNAME(sc));
		goto done;
	}

	if (_4btol(pg->pg_id) != RDAC_VPD_ID_VOLACCESSCTL) {
		printf("%s: volume access control page id is invalid\n",
		    DEVNAME(sc));
		goto done;
	}

	if (ISSET(pg->avtcvp, RDAC_VOLACCESSCTL_AVT)) {
		printf("%s: avt\n", DEVNAME(sc));
		rv = 0;
	} else if (ISSET(pg->avtcvp, RDAC_VOLACCESSCTL_OWNER)) {
		printf("%s: owner\n", DEVNAME(sc));
		rv = 0;
	} else
		printf("%s: unowned\n", DEVNAME(sc));

done:
	dma_free(pg, sizeof(*pg));
	return (rv);
}

