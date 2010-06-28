/*	$OpenBSD: vscsi.c,v 1.14 2010/06/28 18:31:01 krw Exp $ */

/*
 * Copyright (c) 2008 David Gwynne <dlg@openbsd.org>
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

#include <dev/vscsivar.h>

int		vscsi_match(struct device *, void *, void *);
void		vscsi_attach(struct device *, struct device *, void *);
void		vscsi_shutdown(void *);

struct vscsi_ccb {
	TAILQ_ENTRY(vscsi_ccb)	ccb_entry;
	int			ccb_tag;
	struct scsi_xfer	*ccb_xs;
	size_t			ccb_datalen;
};

TAILQ_HEAD(vscsi_ccb_list, vscsi_ccb);

struct vscsi_softc {
	struct device		sc_dev;
	struct scsi_link	sc_link;
	struct scsibus_softc	*sc_scsibus;

	struct pool		sc_ccb_pool;
	struct vscsi_ccb_list	sc_ccb_i2t;
	struct vscsi_ccb_list	sc_ccb_t2i;
	int			sc_ccb_tag;
	struct mutex		sc_ccb_mtx;
	struct rwlock		sc_ccb_polling;

	struct selinfo		sc_sel;
	struct mutex		sc_sel_mtx;

	struct rwlock		sc_open;
	volatile int		sc_opened;
};

#define DEVNAME(_s) ((_s)->sc_dev.dv_xname)
#define DEV2SC(_d) ((struct vscsi_softc *)device_lookup(&vscsi_cd, minor(_d)))

struct cfattach vscsi_ca = {
	sizeof(struct vscsi_softc),
	vscsi_match,
	vscsi_attach
};

struct cfdriver vscsi_cd = {
	NULL,
	"vscsi",
	DV_DULL
};

void		vscsi_cmd(struct scsi_xfer *);
int		vscsi_probe(struct scsi_link *);

struct scsi_adapter vscsi_switch = {
	vscsi_cmd,
	scsi_minphys,
	vscsi_probe,
	NULL
};

void		vscsi_xs_stuffup(struct scsi_xfer *);


int		vscsi_i2t(struct vscsi_softc *, struct vscsi_ioc_i2t *);
int		vscsi_data(struct vscsi_softc *, struct vscsi_ioc_data *, int);
int		vscsi_t2i(struct vscsi_softc *, struct vscsi_ioc_t2i *);

struct vscsi_ccb *	vscsi_ccb_get(struct vscsi_softc *, int);
#define vscsi_ccb_put(_s, _c) pool_put(&(_s)->sc_ccb_pool, (_c))

void		filt_vscsidetach(struct knote *);
int		filt_vscsiread(struct knote *, long);
  
struct filterops vscsi_filtops = {
	1,
	NULL,
	filt_vscsidetach,
	filt_vscsiread
};


int
vscsi_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
vscsi_attach(struct device *parent, struct device *self, void *aux)
{
	struct vscsi_softc		*sc = (struct vscsi_softc *)self;
	struct scsibus_attach_args	saa;

	printf("\n");

	rw_init(&sc->sc_open, DEVNAME(sc));
	rw_init(&sc->sc_ccb_polling, DEVNAME(sc));

	sc->sc_link.adapter = &vscsi_switch;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 256;
	sc->sc_link.adapter_buswidth = 256;
	sc->sc_link.openings = 1;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	sc->sc_scsibus = (struct scsibus_softc *)config_found(&sc->sc_dev,
	    &saa, scsiprint);
}

void
vscsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link		*link = xs->sc_link;
	struct vscsi_softc		*sc = link->adapter_softc;
	struct vscsi_ccb		*ccb;
	int				polled = ISSET(xs->flags, SCSI_POLL);

	if (sc->sc_opened == 0) { 
		vscsi_xs_stuffup(xs);
		return;
	}

	if (ISSET(xs->flags, SCSI_POLL) && ISSET(xs->flags, SCSI_NOSLEEP)) {
		printf("%s: POLL && NOSLEEP for 0x%02x\n", DEVNAME(sc),
		    xs->cmd->opcode);
		vscsi_xs_stuffup(xs);
		return;
	}

	ccb = vscsi_ccb_get(sc, ISSET(xs->flags, SCSI_NOSLEEP) ? 0 : 1);
	if (ccb == NULL) {
		vscsi_xs_stuffup(xs);
		return;
	}

	ccb->ccb_xs = xs;
	mtx_enter(&sc->sc_ccb_mtx);
	TAILQ_INSERT_TAIL(&sc->sc_ccb_i2t, ccb, ccb_entry);
	mtx_leave(&sc->sc_ccb_mtx);

	selwakeup(&sc->sc_sel);

	if (polled) {
		rw_enter_read(&sc->sc_ccb_polling);
		while (ccb->ccb_xs != NULL)
			tsleep(ccb, PRIBIO, "vscsipoll", 0);
		vscsi_ccb_put(sc, ccb);
		rw_exit_read(&sc->sc_ccb_polling);
	}
}

void
vscsi_xs_stuffup(struct scsi_xfer *xs)
{
	xs->error = XS_DRIVER_STUFFUP;
	scsi_done(xs);
}

int
vscsi_probe(struct scsi_link *link)
{
	struct vscsi_softc		*sc = link->adapter_softc;

	if (sc->sc_opened == 0)
		return (ENXIO);

	return (0);
}

int
vscsiopen(dev_t dev, int flags, int mode, struct proc *p)
{
	struct vscsi_softc		*sc = DEV2SC(dev);
	int				rv;

	if (sc == NULL)
		return (ENXIO);

	rv = rw_enter(&sc->sc_open, RW_WRITE | RW_NOSLEEP);
	if (rv != 0)
		return (rv);

	pool_init(&sc->sc_ccb_pool, sizeof(struct vscsi_ccb), 0, 0, 0,
	    "vscsiccb", NULL);
	pool_setipl(&sc->sc_ccb_pool, IPL_BIO);
	TAILQ_INIT(&sc->sc_ccb_i2t);
	TAILQ_INIT(&sc->sc_ccb_t2i);
	mtx_init(&sc->sc_ccb_mtx, IPL_BIO);
	mtx_init(&sc->sc_sel_mtx, IPL_BIO);

	sc->sc_opened = 1;

	return (0);
}

int
vscsiioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	struct vscsi_softc		*sc = DEV2SC(dev);
	struct vscsi_ioc_devevent	*de = (struct vscsi_ioc_devevent *)addr;
	int				read = 0;
	int				err = 0;

	switch (cmd) {
	case VSCSI_I2T:
		err = vscsi_i2t(sc, (struct vscsi_ioc_i2t *)addr);
		break;

	case VSCSI_DATA_READ:
		read = 1;
	case VSCSI_DATA_WRITE:
		err = vscsi_data(sc, (struct vscsi_ioc_data *)addr, read);
		break;

	case VSCSI_T2I:
		err = vscsi_t2i(sc, (struct vscsi_ioc_t2i *)addr);
		break;

	case VSCSI_REQPROBE:
		err = scsi_req_probe(sc->sc_scsibus, de->target, de->lun);
		break;

	case VSCSI_REQDETACH:
		err = scsi_req_detach(sc->sc_scsibus, de->target, de->lun,
		    DETACH_FORCE);
		break;

	default:
		err = ENOTTY;
		break;
	}

	return (err);
}

int
vscsi_i2t(struct vscsi_softc *sc, struct vscsi_ioc_i2t *i2t)
{
	struct vscsi_ccb		*ccb;
	struct scsi_xfer		*xs;
	struct scsi_link		*link;

	mtx_enter(&sc->sc_ccb_mtx);
	ccb = TAILQ_FIRST(&sc->sc_ccb_i2t);
	if (ccb != NULL)
		TAILQ_REMOVE(&sc->sc_ccb_i2t, ccb, ccb_entry);
	mtx_leave(&sc->sc_ccb_mtx);

	if (ccb == NULL)
		return (EAGAIN);

	xs = ccb->ccb_xs;
	link = xs->sc_link;

	i2t->tag = ccb->ccb_tag;
	i2t->target = link->target;
	i2t->lun = link->lun;
	bcopy(xs->cmd, &i2t->cmd, xs->cmdlen);
	i2t->cmdlen = xs->cmdlen;
	i2t->datalen = xs->datalen;

	switch (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
	case SCSI_DATA_IN:
		i2t->direction = VSCSI_DIR_READ;
		break;
	case SCSI_DATA_OUT:
		i2t->direction = VSCSI_DIR_WRITE;
		break;
	default:
		i2t->direction = VSCSI_DIR_NONE;
		break;
	}

	TAILQ_INSERT_TAIL(&sc->sc_ccb_t2i, ccb, ccb_entry);

	return (0);
}

int
vscsi_data(struct vscsi_softc *sc, struct vscsi_ioc_data *data, int read)
{
	struct vscsi_ccb		*ccb;
	struct scsi_xfer		*xs;
	int				xsread;
	u_int8_t			*buf;
	int				rv = EINVAL;

	TAILQ_FOREACH(ccb, &sc->sc_ccb_t2i, ccb_entry) {
		if (ccb->ccb_tag == data->tag)
			break;
	}
	if (ccb == NULL)
		return (EFAULT);

	xs = ccb->ccb_xs;

	if (data->datalen > xs->datalen - ccb->ccb_datalen)
		return (ENOMEM);

	switch (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
	case SCSI_DATA_IN:
		xsread = 1;
		break;
	case SCSI_DATA_OUT:
		xsread = 0;
		break;
	default:
		return (EINVAL);
	}

	if (read != xsread)
		return (EINVAL);

	buf = xs->data;
	buf += ccb->ccb_datalen;

	if (read)
		rv = copyin(data->data, buf, data->datalen);
	else
		rv = copyout(buf, data->data, data->datalen);

	if (rv == 0)
		ccb->ccb_datalen += data->datalen;

	return (rv);
}

int
vscsi_t2i(struct vscsi_softc *sc, struct vscsi_ioc_t2i *t2i)
{
	struct vscsi_ccb		*ccb;
	struct scsi_xfer		*xs;
	struct scsi_link		*link;
	int				rv = 0;
	int				polled;

	TAILQ_FOREACH(ccb, &sc->sc_ccb_t2i, ccb_entry) {
		if (ccb->ccb_tag == t2i->tag)
			break;
	}
	if (ccb == NULL)
		return (EFAULT);

	TAILQ_REMOVE(&sc->sc_ccb_t2i, ccb, ccb_entry);

	xs = ccb->ccb_xs;
	link = xs->sc_link;

	xs->resid = xs->datalen - ccb->ccb_datalen;
	xs->status = SCSI_OK;

	switch (t2i->status) {
	case VSCSI_STAT_DONE:
		xs->error = XS_NOERROR;
		break;
	case VSCSI_STAT_SENSE:
		xs->error = XS_SENSE;
		bcopy(&t2i->sense, &xs->sense, sizeof(xs->sense));
		break;
	case VSCSI_STAT_ERR:
	default:
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	polled = ISSET(xs->flags, SCSI_POLL);

	scsi_done(xs);

	if (polled) {
		ccb->ccb_xs = NULL;
		wakeup(ccb);
	} else
		vscsi_ccb_put(sc, ccb);

	return (rv);
}

int
vscsipoll(dev_t dev, int events, struct proc *p)
{
	struct vscsi_softc		*sc = DEV2SC(dev);
	int				revents = 0;

	if (events & (POLLIN | POLLRDNORM)) {
		mtx_enter(&sc->sc_ccb_mtx);
		if (!TAILQ_EMPTY(&sc->sc_ccb_i2t))
			revents |= events & (POLLIN | POLLRDNORM);
		mtx_leave(&sc->sc_ccb_mtx);
	}

	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &sc->sc_sel);
	}

	return (revents);
}

int
vscsikqfilter(dev_t dev, struct knote *kn)
{ 
	struct vscsi_softc *sc = DEV2SC(dev);
	struct klist *klist = &sc->sc_sel.si_note;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &vscsi_filtops;
		break;
	default:
		return (1);
	}

	kn->kn_hook = (caddr_t)sc;

	mtx_enter(&sc->sc_sel_mtx);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	mtx_leave(&sc->sc_sel_mtx);

	return (0);
}

void
filt_vscsidetach(struct knote *kn)
{
	struct vscsi_softc *sc = (struct vscsi_softc *)kn->kn_hook;
	struct klist *klist = &sc->sc_sel.si_note;
 
	mtx_enter(&sc->sc_sel_mtx);
	SLIST_REMOVE(klist, kn, knote, kn_selnext);
	mtx_leave(&sc->sc_sel_mtx);
}

int
filt_vscsiread(struct knote *kn, long hint)
{
	struct vscsi_softc *sc = (struct vscsi_softc *)kn->kn_hook;
	int event = 0;

	mtx_enter(&sc->sc_ccb_mtx);
	if (!TAILQ_EMPTY(&sc->sc_ccb_i2t))
		event = 1;
	mtx_leave(&sc->sc_ccb_mtx);

	return (event);
}

int
vscsiclose(dev_t dev, int flags, int mode, struct proc *p)
{
	struct vscsi_softc		*sc = DEV2SC(dev);
	struct vscsi_ccb		*ccb;
	int				polled;
	int				i;

	sc->sc_opened = 0;

	while ((ccb = TAILQ_FIRST(&sc->sc_ccb_t2i)) != NULL) {
		TAILQ_REMOVE(&sc->sc_ccb_t2i, ccb, ccb_entry);
		polled = ISSET(ccb->ccb_xs->flags, SCSI_POLL);

		vscsi_xs_stuffup(ccb->ccb_xs);

		if (polled) {
			ccb->ccb_xs = NULL;
			wakeup(ccb);
		} else
			vscsi_ccb_put(sc, ccb);
	}

	while ((ccb = TAILQ_FIRST(&sc->sc_ccb_i2t)) != NULL) {
		TAILQ_REMOVE(&sc->sc_ccb_i2t, ccb, ccb_entry);
		polled = ISSET(ccb->ccb_xs->flags, SCSI_POLL);

		vscsi_xs_stuffup(ccb->ccb_xs);

		if (polled) {
			ccb->ccb_xs = NULL;
			wakeup(ccb);
		} else
			vscsi_ccb_put(sc, ccb);
	}

	rw_enter_write(&sc->sc_ccb_polling);
	pool_destroy(&sc->sc_ccb_pool);
	rw_exit_write(&sc->sc_ccb_polling);

	for (i = 0; i < sc->sc_link.adapter_buswidth; i++)
		scsi_detach_target(sc->sc_scsibus, i, DETACH_FORCE);

	rw_exit(&sc->sc_open);

	return (0);
}

struct vscsi_ccb *
vscsi_ccb_get(struct vscsi_softc *sc, int waitok)
{
	struct vscsi_ccb		*ccb;

	ccb = pool_get(&sc->sc_ccb_pool, waitok ? PR_WAITOK : PR_NOWAIT);
	if (ccb == NULL)
		return (NULL);

	ccb->ccb_tag = sc->sc_ccb_tag++;
	ccb->ccb_datalen = 0;

	return (ccb);
}
