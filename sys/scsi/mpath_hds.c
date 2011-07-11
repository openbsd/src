/*	$OpenBSD: mpath_hds.c,v 1.6 2011/07/11 01:02:48 dlg Exp $ */

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

/* Hitachi Modular Storage support for mpath(4) */

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

#define HDS_INQ_LDEV_OFFSET	44
#define HDS_INQ_LDEV_LEN	4
#define HDS_INQ_CTRL_OFFSET	49
#define HDS_INQ_PORT_OFFSET	50
#define HDS_INQ_TYPE_OFFSET	128
#define HDS_INQ_TYPE		0x44463030 /* "DF00" */

#define HDS_VPD			0xe0

struct hds_vpd {
        struct scsi_vpd_hdr	hdr; /* HDS_VPD */
	u_int8_t		state;
#define HDS_VPD_VALID			0x80
#define HDS_VPD_PREFERRED		0x40

	/* followed by lots of unknown stuff */
};

#define HDS_SYMMETRIC		0
#define HDS_ASYMMETRIC		1

struct hds_softc {
	struct device		sc_dev;
	struct mpath_path	sc_path;
	int			sc_mode;
};
#define DEVNAME(_s) ((_s)->sc_dev.dv_xname)

int		hds_match(struct device *, void *, void *);
void		hds_attach(struct device *, struct device *, void *);
int		hds_detach(struct device *, int);
int		hds_activate(struct device *, int);

struct cfattach hds_ca = {
	sizeof(struct hds_softc),
	hds_match,
	hds_attach,
	hds_detach,
	hds_activate
};

struct cfdriver hds_cd = {
	NULL,
	"hds",
	DV_DULL
};

void		hds_mpath_start(struct scsi_xfer *);
int		hds_mpath_checksense(struct scsi_xfer *);
int		hds_mpath_online(struct scsi_link *);
int		hds_mpath_offline(struct scsi_link *);

const struct mpath_ops hds_mpath_ops = {
	"hds",
	hds_mpath_checksense,
	hds_mpath_online,
	hds_mpath_offline,
	MPATH_ROUNDROBIN
};

struct hds_device {
	char *vendor;
	char *product;
};

int		hds_inquiry(struct scsi_link *, int *);
int		hds_info(struct hds_softc *);
int		hds_preferred(struct hds_softc *, int *);

struct hds_device hds_devices[] = {
/*	  " vendor "  "     device     " */
/*	  "01234567"  "0123456789012345" */
	{ "HITACHI ", "DF600F          " },
	{ "HITACHI ", "DF600F-CM       " }
};

int
hds_match(struct device *parent, void *match, void *aux)
{
	struct scsi_attach_args *sa = aux;
	struct scsi_inquiry_data *inq = sa->sa_inqbuf;
	struct scsi_link *link = sa->sa_sc_link;
	struct hds_device *s;
	int i, mode;

	if (mpath_path_probe(sa->sa_sc_link) != 0)
		return (0);

	for (i = 0; i < nitems(hds_devices); i++) {
		s = &hds_devices[i];

		if (bcmp(s->vendor, inq->vendor, strlen(s->vendor)) == 0 &&
		    bcmp(s->product, inq->product, strlen(s->product)) == 0 &&
		    hds_inquiry(link, &mode) == 0)
			return (3);
	}

	return (0);
}

void
hds_attach(struct device *parent, struct device *self, void *aux)
{
	struct hds_softc *sc = (struct hds_softc *)self;
	struct scsi_attach_args *sa = aux;
	struct scsi_link *link = sa->sa_sc_link;
	int preferred = 1;

	printf("\n");

	/* init link */
	link->device_softc = sc;

	/* init path */
	scsi_xsh_set(&sc->sc_path.p_xsh, link, hds_mpath_start);
	sc->sc_path.p_link = link;

	if (hds_inquiry(link, &sc->sc_mode) != 0) {
		printf("%s: unable to query controller mode\n");
		return;
	}

	if (hds_info(sc) != 0) {
		printf("%s: unable to query path info\n");
		return;
	}

	if (hds_preferred(sc, &preferred) != 0) {
		printf("%s: unable to query preferred path\n", DEVNAME(sc));
		return;
	}

	if (!preferred)
		return;

	if (mpath_path_attach(&sc->sc_path, &hds_mpath_ops) != 0)
		printf("%s: unable to attach path\n", DEVNAME(sc));
}

int
hds_detach(struct device *self, int flags)
{
	return (0);
}

int
hds_activate(struct device *self, int act)
{
	struct hds_softc *sc = (struct hds_softc *)self;
	int rv = 0;

	switch (act) {
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
hds_mpath_start(struct scsi_xfer *xs)
{
	struct hds_softc *sc = xs->sc_link->device_softc;

	mpath_start(&sc->sc_path, xs);
}

int
hds_mpath_checksense(struct scsi_xfer *xs)
{
	return (0);
}

int
hds_mpath_online(struct scsi_link *link)
{
	return (0);
}

int
hds_mpath_offline(struct scsi_link *link)
{
	return (0);
}

int
hds_inquiry(struct scsi_link *link, int *mode)
{
	struct scsi_xfer *xs;
	u_int8_t *buf;
	size_t len = link->inqdata.additional_length + 5;
	int error;

	if (len < HDS_INQ_TYPE_OFFSET + sizeof(int))
		return (ENXIO);

	xs = scsi_xs_get(link, scsi_autoconf);
	if (xs == NULL)
		return (ENOMEM);

	buf = dma_alloc(len, PR_WAITOK);
	scsi_init_inquiry(xs, 0, 0, buf, len);
	error = scsi_xs_sync(xs);
	scsi_xs_put(xs);
	if (error)
		goto done;

	if (buf[128] == '\0')
		*mode = HDS_ASYMMETRIC;
	else if (_4btol(&buf[HDS_INQ_TYPE_OFFSET]) == HDS_INQ_TYPE)
		*mode = HDS_SYMMETRIC;
	else
		error = ENXIO;

done:
	dma_free(buf, len);
	return (error);
}

int
hds_info(struct hds_softc *sc)
{
	struct scsi_link *link = sc->sc_path.p_link;
	struct scsi_xfer *xs;
	u_int8_t *buf;
	size_t len = link->inqdata.additional_length + 5;
	char ldev[9], ctrl, port;
	int error;

	xs = scsi_xs_get(link, scsi_autoconf);
	if (xs == NULL)
		return (ENOMEM);

	buf = dma_alloc(len, PR_WAITOK);
	scsi_init_inquiry(xs, 0, 0, buf, len);
	error = scsi_xs_sync(xs);
	scsi_xs_put(xs);
	if (error)
		goto done;

	bzero(ldev, sizeof(ldev));
	scsi_strvis(ldev, &buf[HDS_INQ_LDEV_OFFSET], HDS_INQ_LDEV_LEN);
	ctrl = buf[HDS_INQ_CTRL_OFFSET];
	port = buf[HDS_INQ_PORT_OFFSET];

	if (ctrl >= '0' && ctrl <= '9' && port >= 'A' && port <= 'B') {
		printf("%s: ldev %s, controller %c, port %c, %s\n",
		    DEVNAME(sc), ldev, ctrl, port,
		    sc->sc_mode == HDS_SYMMETRIC ? "symmetric" : "asymmetric");
	} else
		error = ENXIO;

done:
	dma_free(buf, len);
	return (error);
}

int
hds_preferred(struct hds_softc *sc, int *preferred)
{
	struct scsi_link *link = sc->sc_path.p_link;
	struct hds_vpd *pg;
	int error;

	if (sc->sc_mode == HDS_SYMMETRIC) {
		*preferred = 1;
		return (0);
	}

	pg = dma_alloc(sizeof(*pg), PR_WAITOK);

	error = scsi_inquire_vpd(link, pg, sizeof(*pg), HDS_VPD, scsi_autoconf);
	if (error)
		goto done;

	if (_2btol(pg->hdr.page_length) < sizeof(pg->state) ||
	     !ISSET(pg->state, HDS_VPD_VALID)) {
		error = ENXIO;
		goto done;
	}

	*preferred = ISSET(pg->state, HDS_VPD_PREFERRED);

done:
	dma_free(pg, sizeof(*pg));
	return (error);
}
