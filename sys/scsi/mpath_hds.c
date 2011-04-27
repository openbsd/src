/*	$OpenBSD: mpath_hds.c,v 1.1 2011/04/27 11:36:20 dlg Exp $ */

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

struct hds_softc {
	struct device		sc_dev;
	struct mpath_path	sc_path;
	int			sc_active;
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

struct mpath_ops hds_mpath_ops = {
	"hds",
	hds_mpath_start,
	hds_mpath_checksense,
	hds_mpath_online,
	hds_mpath_offline
};

struct hds_device {
	char *vendor;
	char *product;
};

int		hds_priority(struct hds_softc *);

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
	struct hds_device *s;
	int i;

	if (mpath_path_probe(sa->sa_sc_link) != 0)
		return (0);

	for (i = 0; i < nitems(hds_devices); i++) {
		s = &hds_devices[i];

		if (bcmp(s->vendor, inq->vendor, strlen(s->vendor)) == 0 &&
		    bcmp(s->product, inq->product, strlen(s->product)) == 0)
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

	printf("\n");

	/* init link */
	link->device_softc = sc;

	/* init path */
	scsi_xsh_set(&sc->sc_path.p_xsh, link, hds_mpath_start);
	sc->sc_path.p_link = link;
	sc->sc_path.p_ops = &hds_mpath_ops;

	if (hds_priority(sc) != 0)
		return;

	if (!sc->sc_active)
		return;

	if (mpath_path_attach(&sc->sc_path) != 0)
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
hds_priority(struct hds_softc *sc)
{
	u_int8_t *buffer;
	struct scsi_inquiry *cdb;
	struct scsi_xfer *xs;
	size_t length;
	u_int8_t ldev[9];
	u_int8_t ctrl;
	u_int8_t port;
	int p, c;
	int error;

	length = MIN(sc->sc_path.p_link->inqdata.additional_length + 5, 255);
	if (length < 51)
		return (EIO);

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

	/* XXX magical */
	bzero(ldev, sizeof(ldev));
	scsi_strvis(ldev, buffer + 44, 4);
	ctrl = buffer[49];
	port = buffer[50];

	if (strlen(ldev) > 4 || ldev[3] < '0' || ldev[3] > 'F' ||
	    ctrl < '0' || ctrl > '9' ||
	    port < 'A' || port > 'B') {
		error = EIO;
		goto done;
	}

	c = ctrl - '0';
	p = port - 'A';
	if ((c & 0x1) == (p & 0x1))
		sc->sc_active = 1;

	printf("%s: ldev %s, controller %c, port %c\n", DEVNAME(sc), ldev,
	    ctrl, port);

	error = 0;
done:
	dma_free(buffer, length);
	return (error);
}
