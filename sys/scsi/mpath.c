/*	$OpenBSD: mpath.c,v 1.21 2011/04/27 05:22:24 dlg Exp $ */

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
#include <scsi/mpathvar.h>

#define MPATH_BUSWIDTH 256

int		mpath_match(struct device *, void *, void *);
void		mpath_attach(struct device *, struct device *, void *);
void		mpath_shutdown(void *);

TAILQ_HEAD(mpath_paths, mpath_path);

struct mpath_ccb {
	struct scsi_xfer	*c_xs;
	SIMPLEQ_ENTRY(mpath_ccb) c_entry;
};
SIMPLEQ_HEAD(mpath_ccbs, mpath_ccb);

struct mpath_dev {
	struct mutex		 d_mtx;

	struct mpath_ccbs	 d_ccbs;
	struct mpath_paths	 d_paths;
	struct mpath_path	*d_next_path;

	u_int			 d_path_count;

	struct devid		*d_id;
};

struct mpath_softc {
	struct device		sc_dev;
	struct scsi_link	sc_link;
	struct pool		sc_ccb_pool;
	struct scsi_iopool	sc_iopool;
	struct scsibus_softc	*sc_scsibus;
};
#define DEVNAME(_s) ((_s)->sc_dev.dv_xname)

struct mpath_softc	*mpath;
struct mpath_dev	*mpath_devs[MPATH_BUSWIDTH];

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

struct mpath_path *mpath_next_path(struct mpath_dev *);
void		mpath_done(struct scsi_xfer *);

struct scsi_adapter mpath_switch = {
	mpath_cmd,
	scsi_minphys,
	mpath_probe
};

void		mpath_xs_stuffup(struct scsi_xfer *);

void *		mpath_ccb_get(void *);
void		mpath_ccb_put(void *, void *);

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

	pool_init(&sc->sc_ccb_pool, sizeof(struct mpath_ccb), 0, 0, 0,
	    "mpathccb", NULL);
	pool_setipl(&sc->sc_ccb_pool, IPL_BIO);

	scsi_iopool_init(&sc->sc_iopool, sc, mpath_ccb_get, mpath_ccb_put);

	sc->sc_link.adapter = &mpath_switch;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = MPATH_BUSWIDTH;
	sc->sc_link.adapter_buswidth = MPATH_BUSWIDTH;
	sc->sc_link.luns = 1;
	sc->sc_link.openings = 1024; /* XXX magical */
	sc->sc_link.pool = &sc->sc_iopool;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	sc->sc_scsibus = (struct scsibus_softc *)config_found(&sc->sc_dev,
	    &saa, scsiprint);
}

void
mpath_xs_stuffup(struct scsi_xfer *xs)
{
	xs->error = XS_DRIVER_STUFFUP;
	scsi_done(xs);
}

int
mpath_probe(struct scsi_link *link)
{
	struct mpath_dev *d = mpath_devs[link->target];

	if (link->lun != 0 || d == NULL)
		return (ENXIO);

	link->id = devid_copy(d->d_id);

	return (0);
}

struct mpath_path *
mpath_next_path(struct mpath_dev *d)
{
	struct mpath_path *p;

	if (d == NULL)
		panic("%s: d is NULL", __func__);

	p = d->d_next_path;
	if (p != NULL) {
		d->d_next_path = TAILQ_NEXT(p, p_entry);
		if (d->d_next_path == NULL)
			d->d_next_path = TAILQ_FIRST(&d->d_paths);
	}

	return (p);
}

void
mpath_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct mpath_dev *d = mpath_devs[link->target];
	struct mpath_ccb *ccb = xs->io;
	struct mpath_path *p;
	struct scsi_xfer *mxs;

#ifdef DIAGNOSTIC
	if (d == NULL)
		panic("mpath_cmd issued against nonexistant device");
#endif

	if (ISSET(xs->flags, SCSI_POLL)) {
		mtx_enter(&d->d_mtx);
		p = mpath_next_path(d);
		mtx_leave(&d->d_mtx);
		if (p == NULL) {
			mpath_xs_stuffup(xs);
			return;
		}

		mxs = scsi_xs_get(p->p_link, xs->flags);
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
		mxs->bp = xs->bp;

		scsi_xs_sync(mxs);

		xs->error = mxs->error;
		xs->status = mxs->status;
		xs->resid = mxs->resid;

		memcpy(&xs->sense, &mxs->sense, sizeof(xs->sense));

		scsi_xs_put(mxs);
		scsi_done(xs);
		return;
	}

	ccb->c_xs = xs;

	mtx_enter(&d->d_mtx);
	SIMPLEQ_INSERT_TAIL(&d->d_ccbs, ccb, c_entry);
	p = mpath_next_path(d);
	mtx_leave(&d->d_mtx);

	if (p != NULL)
		scsi_xsh_add(&p->p_xsh);
}

void
mpath_start(struct mpath_path *p, struct scsi_xfer *mxs)
{
	struct mpath_dev *d = p->p_dev;
	struct mpath_ccb *ccb;
	struct scsi_xfer *xs;
	int addxsh = 0;

	if (ISSET(p->p_link->state, SDEV_S_DYING) || d == NULL)
		goto fail;

	mtx_enter(&d->d_mtx);
	ccb = SIMPLEQ_FIRST(&d->d_ccbs);
	if (ccb != NULL) {
		SIMPLEQ_REMOVE_HEAD(&d->d_ccbs, c_entry);
		if (!SIMPLEQ_EMPTY(&d->d_ccbs))
			addxsh = 1;
	}
	mtx_leave(&d->d_mtx);

	if (ccb == NULL)
		goto fail;

	xs = ccb->c_xs;

	memcpy(mxs->cmd, xs->cmd, xs->cmdlen);
	mxs->cmdlen = xs->cmdlen;
	mxs->data = xs->data;
	mxs->datalen = xs->datalen;
	mxs->retries = xs->retries;
	mxs->timeout = xs->timeout;
	mxs->bp = xs->bp;
	mxs->flags = xs->flags;

	mxs->cookie = xs;
	mxs->done = mpath_done;

	scsi_xs_exec(mxs);

	if (addxsh)
		scsi_xsh_add(&p->p_xsh);

	return;
fail:
	scsi_xs_put(mxs);
}

void
mpath_done(struct scsi_xfer *mxs)
{
	struct scsi_xfer *xs = mxs->cookie;
	struct scsi_link *link = xs->sc_link;
	struct mpath_ccb *ccb = xs->io;
	struct mpath_dev *d = mpath_devs[link->target];
	struct mpath_path *p;

	if (mxs->error == XS_RESET || mxs->error == XS_SELTIMEOUT) {
		mtx_enter(&d->d_mtx);
		SIMPLEQ_INSERT_HEAD(&d->d_ccbs, ccb, c_entry);
		p = mpath_next_path(d);
		mtx_leave(&d->d_mtx);

		scsi_xs_put(mxs);

		if (p != NULL)
			scsi_xsh_add(&p->p_xsh);

		return;
	}

	xs->error = mxs->error;
	xs->status = mxs->status;
	xs->resid = mxs->resid;

	memcpy(&xs->sense, &mxs->sense, sizeof(xs->sense));

	scsi_xs_put(mxs);

	scsi_done(xs);
}

void
mpath_minphys(struct buf *bp, struct scsi_link *link)
{
	struct mpath_dev *d = mpath_devs[link->target];
	struct mpath_path *p;

#ifdef DIAGNOSTIC
	if (d == NULL)
		panic("mpath_minphys against nonexistant device");
#endif

	TAILQ_FOREACH(p, &d->d_paths, p_entry)
		p->p_link->adapter->scsi_minphys(bp, p->p_link);
}

int
mpath_path_probe(struct scsi_link *link)
{
	static struct cfdata *cf = NULL;

	if (cf == NULL) {
		for (cf = cfdata; cf->cf_attach != (struct cfattach *)-1;
		    cf++) {
			if (cf->cf_attach == NULL)
				continue;
			if (cf->cf_driver == &mpath_cd)
				break;
		}
	}

	if (cf->cf_fstate == FSTATE_DNOTFOUND || cf->cf_fstate == FSTATE_DSTAR)
		return (ENXIO);

	if (link->id == NULL)
		return (EINVAL);

	if (mpath != NULL && mpath == link->adapter_softc)
		return (ENXIO);

	return (0);
}

int
mpath_path_attach(struct mpath_path *p)
{
	struct scsi_link *link = p->p_link;
	struct mpath_dev *d = NULL;
	int newdev = 0, addxsh = 0;
	int target;

#ifdef DIAGNOSTIC
	if (p->p_link == NULL)
		panic("mpath_path_attach: NULL link");
	if (p->p_dev != NULL)
		panic("mpath_path_attach: dev is not NULL");
#endif

	for (target = 0; target < MPATH_BUSWIDTH; target++) {
		if ((d = mpath_devs[target]) == NULL)
			continue;

		if (DEVID_CMP(d->d_id, link->id))
			break;

		d = NULL;
	}

	if (d == NULL) {
		for (target = 0; target < MPATH_BUSWIDTH; target++) {
			if (mpath_devs[target] == NULL)
				break;
		}
		if (target >= MPATH_BUSWIDTH)
			return (ENXIO);

		d = malloc(sizeof(*d), M_DEVBUF, M_WAITOK | M_ZERO);
		if (d == NULL)
			return (ENOMEM);

		mtx_init(&d->d_mtx, IPL_BIO);
		TAILQ_INIT(&d->d_paths);
		SIMPLEQ_INIT(&d->d_ccbs);
		d->d_id = devid_copy(link->id);

		mpath_devs[target] = d;
		newdev = 1;
	} else {
		/*
		 * instead of carrying identical values in different devid
		 * instances, delete the new one and reference the old one in
		 * the new scsi_link.
		 */
		devid_free(link->id);
		link->id = devid_copy(d->d_id);
	}

	p->p_dev = d;
	mtx_enter(&d->d_mtx);
	if (TAILQ_EMPTY(&d->d_paths))
		d->d_next_path = p;
	TAILQ_INSERT_TAIL(&d->d_paths, p, p_entry);
	d->d_path_count++;
	if (!SIMPLEQ_EMPTY(&d->d_ccbs))
		addxsh = 1;
	mtx_leave(&d->d_mtx);

	if (newdev && mpath != NULL)
		scsi_probe_target(mpath->sc_scsibus, target);
	else if (addxsh)
		scsi_xsh_add(&p->p_xsh);

	return (0);
}

int
mpath_path_detach(struct mpath_path *p)
{
	struct mpath_dev *d = p->p_dev;
	struct mpath_path *np = NULL;

#ifdef DIAGNOSTIC
	if (d == NULL)
		panic("mpath: detaching a path from a nonexistant bus");
#endif
	p->p_dev = NULL;

	mtx_enter(&d->d_mtx);
	TAILQ_REMOVE(&d->d_paths, p, p_entry);
	if (d->d_next_path == p)
		d->d_next_path = TAILQ_FIRST(&d->d_paths);

	d->d_path_count--;
	if (!SIMPLEQ_EMPTY(&d->d_ccbs))
		np = d->d_next_path;
	mtx_leave(&d->d_mtx);

	scsi_xsh_del(&p->p_xsh);

	if (np != NULL)
		scsi_xsh_add(&np->p_xsh);

	return (0);
}

void *
mpath_ccb_get(void *cookie)
{
	struct mpath_softc *sc = cookie;

	return (pool_get(&sc->sc_ccb_pool, PR_NOWAIT));
}

void
mpath_ccb_put(void *cookie, void *io)
{
	struct mpath_softc *sc = cookie;

	pool_put(&sc->sc_ccb_pool, io);
}

struct device *
mpath_bootdv(struct device *dev)
{
	struct mpath_dev *d;
	struct mpath_path *p;
	int target;

	if (mpath == NULL)
		return (dev);

	for (target = 0; target < MPATH_BUSWIDTH; target++) {
		if ((d = mpath_devs[target]) == NULL)
			continue;

		TAILQ_FOREACH(p, &d->d_paths, p_entry) {
			if (p->p_link->device_softc == dev) {
				return (scsi_get_link(mpath->sc_scsibus,
				    target, 0)->device_softc);
			}
		}
	}

	return (dev);
}
