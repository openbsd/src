/*	$OpenBSD: switchctl.c,v 1.1 2016/09/01 10:06:33 goda Exp $	*/

/*
 * Copyright (c) 2016 Kazuya GODA <goda@openbsd.org>
 * Copyright (c) 2015, 2016 YASUOKA Masahiko <yasuoka@openbsd.org>
 * Copyright (c) 2015, 2016 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/selinfo.h>
#include <sys/rwlock.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/if_switch.h>

extern struct rwlock	switch_ifs_lk;

/*
 * device part of switch(4)
 */
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/vnode.h>

struct switch_softc *vsw_dev2sc(dev_t);
int	switchopen(dev_t, int, int, struct proc *);
int	switchread(dev_t, struct uio *, int);
int	switchwrite(dev_t, struct uio *, int);
int	switchioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	switchclose(dev_t, int, int, struct proc *);
int	switchpoll(dev_t, int, struct proc *);
int	switchkqfilter(dev_t, struct knote *);
void	filt_switch_rdetach(struct knote *);
int	filt_switch_read(struct knote *, long);
void	filt_switch_wdetach(struct knote *);
int	filt_switch_write(struct knote *, long);
int	switch_dev_output(struct switch_softc *, struct mbuf *);
void	switch_dev_wakeup(struct switch_softc *);

struct filterops switch_rd_filtops = {
	1, NULL, filt_switch_rdetach, filt_switch_read
};
struct filterops switch_wr_filtops = {
	1, NULL, filt_switch_wdetach, filt_switch_write
};

struct switch_softc *
switch_dev2sc(dev_t dev)
{
	struct switch_softc	*sc;

	rw_enter_read(&switch_ifs_lk);
	sc = switch_lookup(minor(dev));
	rw_exit_read(&switch_ifs_lk);

	return (sc);
}

int
switchopen(dev_t dev, int flags, int mode, struct proc *p)
{
	struct switch_softc	*sc;
	char			 name[IFNAMSIZ];
	int			 rv, s, error = 0;

	if ((sc = switch_dev2sc(dev)) == NULL) {
		snprintf(name, sizeof(name), "switch%d", minor(dev));
		if ((rv = if_clone_create(name)) != 0)
			return (rv);
		if ((sc = switch_dev2sc(dev)) == NULL)
			return (ENXIO);
	}

	rw_enter_write(&switch_ifs_lk);
	if (sc->sc_swdev != NULL) {
		error = EBUSY;
		goto failed;
	}

	if ((sc->sc_swdev = malloc(sizeof(struct switch_dev), M_DEVBUF,
	    M_DONTWAIT|M_ZERO)) == NULL ) {
		error = ENOBUFS;
		goto failed;
	}

	s = splnet();
	mq_init(&sc->sc_swdev->swdev_outq, 128, IPL_NET);

	sc->sc_swdev->swdev_output = switch_dev_output;
	if (sc->sc_capabilities & SWITCH_CAP_OFP)
		swofp_init(sc);

	splx(s);

 failed:
	rw_exit_write(&switch_ifs_lk);
	return (error);

}

int
switchread(dev_t dev, struct uio *uio, int ioflag)
{
	struct switch_softc	*sc;
	int			 error = 0, s;
	u_int			 len;
	struct mbuf		*m0, *m;

	s = splnet();
	sc = switch_dev2sc(dev);
	while ((m0 = mq_dequeue(&sc->sc_swdev->swdev_outq)) == NULL) {
		if (ISSET(ioflag, IO_NDELAY)) {
			splx(s);
			return (EWOULDBLOCK);
		}
		sc->sc_swdev->swdev_waiting = 1;
		error = tsleep(sc, (PZERO + 1)|PCATCH, "switchread", 0);
		if (error != 0) {
			splx(s);
			return (error);
		}
	}
	splx(s);

	while (m0 != NULL && uio->uio_resid > 0 && error == 0) {
		len = ulmin(uio->uio_resid, m0->m_len);
		error = uiomove(mtod(m0, caddr_t), len, uio);
		m = m_free(m0);
		m0 = m;
	}
	m_freem(m0);

	return (error);
}

int
switchwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct switch_softc	*sc = NULL;
	int			 s, error;
	u_int			 len;
	struct mbuf		*m;

	if (uio->uio_resid == 0 || uio->uio_resid > MAXMCLBYTES)
		return (EMSGSIZE);
	len = uio->uio_resid;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	if (len >= MHLEN) {
		MCLGETI(m, M_DONTWAIT, NULL, len);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return (ENOBUFS);
		}
	}

	error = uiomove(mtod(m, caddr_t), len, uio);
	if (error) {
		m_freem(m);
		return (error);
	}
	m->m_pkthdr.len = m->m_len = len;

	s = splnet();
	sc = switch_dev2sc(dev);
	error = sc->sc_swdev->swdev_input(sc, m);
	splx(s);

	return (error);
}

int
switchioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct proc *p)
{
	int			 error;
	/* struct switch_softc	*sc = switch_dev2sc(dev); */

	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
	case FIONREAD:
		return (0);
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

int
switchclose(dev_t dev, int flags, int mode, struct proc *p)
{
	struct switch_softc	*sc;

	rw_enter_write(&switch_ifs_lk);
	sc = switch_lookup(minor(dev));
	if (sc->sc_swdev != NULL) {
		mq_purge(&sc->sc_swdev->swdev_outq);
		free(sc->sc_swdev, M_DEVBUF, sizeof(struct switch_dev));
		sc->sc_swdev = NULL;
	}
	rw_exit_write(&switch_ifs_lk);

	return (0);
}

void
switch_dev_destroy(struct switch_softc *sc)
{
	int	 s;

	if (sc->sc_swdev == NULL)
		return;
	rw_enter_write(&switch_ifs_lk);
	if (sc->sc_swdev != NULL) {
		switch_dev_wakeup(sc);

		s = splhigh();
		klist_invalidate(&sc->sc_swdev->swdev_rsel.si_note);
		klist_invalidate(&sc->sc_swdev->swdev_wsel.si_note);
		splx(s);

		mq_purge(&sc->sc_swdev->swdev_outq);
		free(sc->sc_swdev, M_DEVBUF, sizeof(struct switch_dev));
		sc->sc_swdev = NULL;
	}
	rw_exit_write(&switch_ifs_lk);
}

int
switchpoll(dev_t dev, int events, struct proc *p)
{
	int			 revents = 0;
	struct switch_softc	*sc = switch_dev2sc(dev);

	if (events & (POLLIN | POLLRDNORM)) {
		if (!mq_empty(&sc->sc_swdev->swdev_outq))
				revents |= events & (POLLIN | POLLRDNORM);
	}
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);
	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &sc->sc_swdev->swdev_rsel);
	}

	return (revents);
}

int
switchkqfilter(dev_t dev, struct knote *kn)
{
	struct switch_softc	*sc = switch_dev2sc(dev);
	struct mutex		*mtx;
	struct klist		*klist;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		mtx = &sc->sc_swdev->swdev_rsel_mtx;
		klist = &sc->sc_swdev->swdev_rsel.si_note;
		kn->kn_fop = &switch_rd_filtops;
		break;
	case EVFILT_WRITE:
		mtx = &sc->sc_swdev->swdev_wsel_mtx;
		klist = &sc->sc_swdev->swdev_wsel.si_note;
		kn->kn_fop = &switch_wr_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = (caddr_t)sc;

	mtx_enter(mtx);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	mtx_leave(mtx);

	return (0);
}

void
filt_switch_rdetach(struct knote *kn)
{
	struct switch_softc	*sc = (struct switch_softc *)kn->kn_hook;
	struct klist		*klist = &sc->sc_swdev->swdev_rsel.si_note;

	if (ISSET(kn->kn_status, KN_DETACHED))
		return;

	mtx_enter(&sc->sc_swdev->swdev_rsel_mtx);
	SLIST_REMOVE(klist, kn, knote, kn_selnext);
	mtx_leave(&sc->sc_swdev->swdev_rsel_mtx);
}

int
filt_switch_read(struct knote *kn, long hint)
{
	struct switch_softc	*sc = (struct switch_softc *)kn->kn_hook;

	if (ISSET(kn->kn_status, KN_DETACHED)) {
		kn->kn_data = 0;
		return (1);
	}

	if (!mq_empty(&sc->sc_swdev->swdev_outq)) {
		kn->kn_data = mq_len(&sc->sc_swdev->swdev_outq);
		return (1);
	}

	return (0);
}

void
filt_switch_wdetach(struct knote *kn)
{
	struct switch_softc	*sc = (struct switch_softc *)kn->kn_hook;
	struct klist		*klist = &sc->sc_swdev->swdev_wsel.si_note;

	if (ISSET(kn->kn_status, KN_DETACHED))
		return;

	mtx_enter(&sc->sc_swdev->swdev_wsel_mtx);
	SLIST_REMOVE(klist, kn, knote, kn_selnext);
	mtx_leave(&sc->sc_swdev->swdev_wsel_mtx);
}

int
filt_switch_write(struct knote *kn, long hint)
{
	/* Always writable */
	return (1);
}

int
switch_dev_output(struct switch_softc *sc, struct mbuf *m)
{
	if (mq_enqueue(&sc->sc_swdev->swdev_outq, m) != 0)
		return (-1);
	switch_dev_wakeup(sc);

	return (0);
}

void
switch_dev_wakeup(struct switch_softc *sc)
{
	if (sc->sc_swdev->swdev_waiting) {
		sc->sc_swdev->swdev_waiting = 0;
		wakeup((caddr_t)sc);
	}
	selwakeup(&sc->sc_swdev->swdev_rsel);
}
