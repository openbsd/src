/*	$OpenBSD: mpath.c,v 1.13 2010/03/23 01:57:20 krw Exp $ */

/*
 * Copyright (c) 2009 David Gwynne <dlg@openbsd.org>
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
#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/pool.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/selinfo.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#define MPATH_BUSWIDTH 256

int		mpath_match(struct device *, void *, void *);
void		mpath_attach(struct device *, struct device *, void *);
void		mpath_shutdown(void *);

struct mpath_path {
	struct scsi_link	*path_link;
	TAILQ_ENTRY(mpath_path)	 path_entry;
};
TAILQ_HEAD(mpath_paths, mpath_path);

struct mpath_node {
	struct devid		*node_id;
	struct mpath_paths	 node_paths;
};

struct mpath_softc {
	struct device		sc_dev;
	struct scsi_link	sc_link;
	struct scsibus_softc	*sc_scsibus;
};

struct mpath_softc	*mpath;
struct mpath_node	*mpath_nodes[MPATH_BUSWIDTH];

#define DEVNAME(_s) ((_s)->sc_dev.dv_xname)

struct cfattach mpath_ca = {
	sizeof(struct mpath_softc),
	mpath_match,
	mpath_attach
};

struct cfdriver mpath_cd = {
	NULL,
	"mpath",
	DV_DULL
};

void		mpath_cmd(struct scsi_xfer *);
void		mpath_minphys(struct buf *, struct scsi_link *);
int		mpath_probe(struct scsi_link *);

void		mpath_done(struct scsi_xfer *);

struct scsi_adapter mpath_switch = {
	mpath_cmd,
	scsi_minphys,
	mpath_probe,
	NULL
};

struct scsi_device mpath_dev = {
	NULL, NULL, NULL, NULL
};

void		mpath_xs_stuffup(struct scsi_xfer *);

int
mpath_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
mpath_attach(struct device *parent, struct device *self, void *aux)
{
	struct mpath_softc		*sc = (struct mpath_softc *)self;
	struct scsibus_attach_args	saa;

	mpath = sc;

	printf("\n");

	sc->sc_link.device = &mpath_dev;
	sc->sc_link.adapter = &mpath_switch;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = MPATH_BUSWIDTH;
	sc->sc_link.adapter_buswidth = MPATH_BUSWIDTH;
	sc->sc_link.openings = 1;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	sc->sc_scsibus = (struct scsibus_softc *)config_found(&sc->sc_dev,
	    &saa, scsiprint);
}

void
mpath_xs_stuffup(struct scsi_xfer *xs)
{
	int s;

	xs->error = XS_DRIVER_STUFFUP;
	s = splbio();
	scsi_done(xs);
	splx(s);
}

int
mpath_probe(struct scsi_link *link)
{
	struct mpath_node *n = mpath_nodes[link->target];

	if (link->lun != 0 || n == NULL)
		return (ENXIO);

	link->id = devid_copy(n->node_id);

	return (0);
}

void
mpath_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct mpath_node *n = mpath_nodes[link->target];
	struct mpath_path *p = TAILQ_FIRST(&n->node_paths);
	struct scsi_xfer *mxs;

	if (n == NULL || p == NULL) {
		mpath_xs_stuffup(xs);
		return;
	}

	mxs = scsi_xs_get(p->path_link, xs->flags);
	if (mxs == NULL) {
		mpath_xs_stuffup(xs);
		return;
	}

	memcpy(mxs->cmd, xs->cmd, xs->cmdlen);
	mxs->cmdlen = xs->cmdlen;
	mxs->data = xs->data;
	mxs->datalen = xs->datalen;
	mxs->retries = xs->retries;
	mxs->timeout = xs->timeout;
	mxs->req_sense_length = xs->req_sense_length;

	mxs->cookie = xs;
	mxs->done = mpath_done;

	scsi_xs_exec(mxs);
}

void
mpath_done(struct scsi_xfer *mxs)
{
	struct scsi_xfer *xs = mxs->cookie;
	int s;

	xs->error = mxs->error;
	xs->status = mxs->status;
	xs->flags = mxs->flags;
	xs->resid = mxs->resid;

	memcpy(&xs->sense, &mxs->sense, sizeof(xs->sense));

	scsi_xs_put(mxs);

	s = splbio();
	scsi_done(xs);
	splx(s);
}

void
mpath_minphys(struct buf *bp, struct scsi_link *link)
{
	struct mpath_node *n = mpath_nodes[link->target];
	struct mpath_path *p;

	if (n == NULL)
		return;

	TAILQ_FOREACH(p, &n->node_paths, path_entry)
		p->path_link->adapter->scsi_minphys(bp, p->path_link);
}

int
mpath_path_attach(struct scsi_link *link)
{
	struct mpath_node *n;
	struct mpath_path *p;
	int probe = 0;
	int target;

	if (mpath != NULL && link->adapter_softc == mpath)
		return (ENODEV);

	/* XXX this is dumb. should check inq shizz */
	if (ISSET(link->flags, SDEV_VIRTUAL) || link->id == NULL)
		return (ENXIO);

	for (target = 0; target < MPATH_BUSWIDTH; target++) {
		if ((n = mpath_nodes[target]) == NULL)
			continue;

		if (DEVID_CMP(n->node_id, link->id))
			break;

		n = NULL;
	}

	if (n == NULL) {
		for (target = 0; target < MPATH_BUSWIDTH; target++) {
			if (mpath_nodes[target] == NULL)
				break;
		}
		if (target >= MPATH_BUSWIDTH)
			return (ENXIO);

		n = malloc(sizeof(*n), M_DEVBUF, M_WAITOK | M_ZERO);
		TAILQ_INIT(&n->node_paths);

		n->node_id = devid_copy(link->id);

		mpath_nodes[target] = n;
		probe = 1;
	} else {
		/*
		 * instead of carrying identical values in different devid
		 * instances, delete the new one and reference the old one in
		 * the new scsi_link.
		 */
		devid_free(link->id);
		link->id = devid_copy(n->node_id);
	}

	p = malloc(sizeof(*p), M_DEVBUF, M_WAITOK);

	p->path_link = link;
	TAILQ_INSERT_TAIL(&n->node_paths, p, path_entry);

	if (mpath != NULL && probe)
		scsi_probe_target(mpath->sc_scsibus, target);

	return (0);
}

int
mpath_path_detach(struct scsi_link *link, int flags)
{
	struct mpath_node *n;
	struct mpath_path *p;
	int target;

	for (target = 0; target < MPATH_BUSWIDTH; target++) {
		if ((n = mpath_nodes[target]) == NULL)
			continue;

		if (DEVID_CMP(n->node_id, link->id))
			break;

		n = NULL;
	}

	if (n == NULL)
		panic("mpath: detaching a path from a nonexistant bus");

	TAILQ_FOREACH(p, &n->node_paths, path_entry) {
		if (p->path_link == link) {
			TAILQ_REMOVE(&n->node_paths, p, path_entry);
			free(p, M_DEVBUF);
			return (0);
		}
	}

	panic("mpath: unable to locate path for detach");
}

void
mpath_path_activate(struct scsi_link *link)
{

}

void
mpath_path_deactivate(struct scsi_link *link)
{

}

