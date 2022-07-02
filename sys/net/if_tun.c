/*	$OpenBSD: if_tun.c,v 1.237 2022/07/02 08:50:42 visa Exp $	*/
/*	$NetBSD: if_tun.c,v 1.24 1996/05/07 02:40:48 thorpej Exp $	*/

/*
 * Copyright (c) 1988, Julian Onions <Julian.Onions@nexor.co.uk>
 * Nottingham University 1987.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This driver takes packets off the IP i/f and hands them up to a
 * user process to have its wicked way with. This driver has its
 * roots in a similar driver written by Phil Cockcroft (formerly) at
 * UCL. This driver is based much more on read/write/select mode of
 * operation though.
 */

/* #define	TUN_DEBUG	9 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/sigio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/selinfo.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/device.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/conf.h>
#include <sys/smr.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/rtable.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef MPLS
#include <netmpls/mpls.h>
#endif /* MPLS */

#include <net/if_tun.h>

struct tun_softc {
	struct arpcom		sc_ac;		/* ethernet common data */
#define sc_if			sc_ac.ac_if
	struct selinfo		sc_rsel;	/* read select */
	struct selinfo		sc_wsel;	/* write select (not used) */
	SMR_LIST_ENTRY(tun_softc)
				sc_entry;	/* all tunnel interfaces */
	int			sc_unit;
	struct sigio_ref	sc_sigio;	/* async I/O registration */
	unsigned int		sc_flags;	/* misc flags */
#define TUN_DEAD			(1 << 16)

	dev_t			sc_dev;
	struct refcnt		sc_refs;
	unsigned int		sc_reading;
};

#ifdef	TUN_DEBUG
int	tundebug = TUN_DEBUG;
#define TUNDEBUG(a)	(tundebug? printf a : 0)
#else
#define TUNDEBUG(a)	/* (tundebug? printf a : 0) */
#endif

/* Only these IFF flags are changeable by TUNSIFINFO */
#define TUN_IFF_FLAGS (IFF_UP|IFF_POINTOPOINT|IFF_MULTICAST|IFF_BROADCAST)

void	tunattach(int);

int	tun_dev_open(dev_t, const struct if_clone *, int, struct proc *);
int	tun_dev_close(dev_t, struct proc *);
int	tun_dev_ioctl(dev_t, u_long, void *);
int	tun_dev_read(dev_t, struct uio *, int);
int	tun_dev_write(dev_t, struct uio *, int, int);
int	tun_dev_kqfilter(dev_t, struct knote *);

int	tun_ioctl(struct ifnet *, u_long, caddr_t);
void	tun_input(struct ifnet *, struct mbuf *);
int	tun_output(struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *);
int	tun_enqueue(struct ifnet *, struct mbuf *);
int	tun_clone_create(struct if_clone *, int);
int	tap_clone_create(struct if_clone *, int);
int	tun_create(struct if_clone *, int, int);
int	tun_clone_destroy(struct ifnet *);
void	tun_wakeup(struct tun_softc *);
int	tun_init(struct tun_softc *);
void	tun_start(struct ifnet *);
int	filt_tunread(struct knote *, long);
int	filt_tunwrite(struct knote *, long);
void	filt_tunrdetach(struct knote *);
void	filt_tunwdetach(struct knote *);
void	tun_link_state(struct ifnet *, int);

const struct filterops tunread_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_tunrdetach,
	.f_event	= filt_tunread,
};

const struct filterops tunwrite_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_tunwdetach,
	.f_event	= filt_tunwrite,
};

SMR_LIST_HEAD(tun_list, tun_softc);

struct if_clone tun_cloner =
    IF_CLONE_INITIALIZER("tun", tun_clone_create, tun_clone_destroy);

struct if_clone tap_cloner =
    IF_CLONE_INITIALIZER("tap", tap_clone_create, tun_clone_destroy);

void
tunattach(int n)
{
	if_clone_attach(&tun_cloner);
	if_clone_attach(&tap_cloner);
}

int
tun_clone_create(struct if_clone *ifc, int unit)
{
	return (tun_create(ifc, unit, 0));
}

int
tap_clone_create(struct if_clone *ifc, int unit)
{
	return (tun_create(ifc, unit, TUN_LAYER2));
}

struct tun_list tun_devs_list = SMR_LIST_HEAD_INITIALIZER(tun_list);

struct tun_softc *
tun_name_lookup(const char *name)
{
	struct tun_softc *sc;

	KERNEL_ASSERT_LOCKED();

	SMR_LIST_FOREACH_LOCKED(sc, &tun_devs_list, sc_entry) {
		if (strcmp(sc->sc_if.if_xname, name) == 0)
			return (sc);
	}

	return (NULL);
}

int
tun_insert(struct tun_softc *sc)
{
	int error = 0;

	/* check for a race */
	if (tun_name_lookup(sc->sc_if.if_xname) != NULL)
		error = EEXIST;
	else {
		/* tun_name_lookup checks for the right lock already */
		SMR_LIST_INSERT_HEAD_LOCKED(&tun_devs_list, sc, sc_entry);
	}

	return (error);
}

int
tun_create(struct if_clone *ifc, int unit, int flags)
{
	struct tun_softc	*sc;
	struct ifnet		*ifp;

	if (unit > minor(~0U))
		return (ENXIO);

	KERNEL_ASSERT_LOCKED();

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	refcnt_init(&sc->sc_refs);

	ifp = &sc->sc_if;
	snprintf(ifp->if_xname, sizeof(ifp->if_xname),
	    "%s%d", ifc->ifc_name, unit);
	ifp->if_softc = sc;

	/* this is enough state for tun_dev_open to work with */

	if (tun_insert(sc) != 0)
		goto exists;

	/* build the interface */

	ifp->if_ioctl = tun_ioctl;
	ifp->if_enqueue = tun_enqueue;
	ifp->if_start = tun_start;
	ifp->if_hardmtu = TUNMRU;
	ifp->if_link_state = LINK_STATE_DOWN;

	if_counters_alloc(ifp);

	if ((flags & TUN_LAYER2) == 0) {
#if NBPFILTER > 0
		ifp->if_bpf_mtap = bpf_mtap;
#endif
		ifp->if_input = tun_input;
		ifp->if_output = tun_output;
		ifp->if_mtu = ETHERMTU;
		ifp->if_flags = (IFF_POINTOPOINT|IFF_MULTICAST);
		ifp->if_type = IFT_TUNNEL;
		ifp->if_hdrlen = sizeof(u_int32_t);
		ifp->if_rtrequest = p2p_rtrequest;

		if_attach(ifp);
		if_alloc_sadl(ifp);

#if NBPFILTER > 0
		bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(u_int32_t));
#endif
	} else {
		sc->sc_flags |= TUN_LAYER2;
		ether_fakeaddr(ifp);
		ifp->if_flags =
		    (IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST);

		if_attach(ifp);
		ether_ifattach(ifp);
	}

	sigio_init(&sc->sc_sigio);

	/* tell tun_dev_open we're initialised */

	sc->sc_flags |= TUN_INITED|TUN_STAYUP;
	wakeup(sc);

	return (0);

exists:
	free(sc, M_DEVBUF, sizeof(*sc));
	return (EEXIST);
}

int
tun_clone_destroy(struct ifnet *ifp)
{
	struct tun_softc	*sc = ifp->if_softc;
	dev_t			 dev;
	int			 s;

	KERNEL_ASSERT_LOCKED();

	if (ISSET(sc->sc_flags, TUN_DEAD))
		return (ENXIO);
	SET(sc->sc_flags, TUN_DEAD);

	/* kick userland off the device */
	dev = sc->sc_dev;
	if (dev) {
		struct vnode *vp;

		if (vfinddev(dev, VCHR, &vp))
			VOP_REVOKE(vp, REVOKEALL);

		KASSERT(sc->sc_dev == 0);
	}

	/* prevent userland from getting to the device again */
	SMR_LIST_REMOVE_LOCKED(sc, sc_entry);
	smr_barrier();

	/* help read() give up */
	if (sc->sc_reading)
		wakeup(&ifp->if_snd);

	/* wait for device entrypoints to finish */
	refcnt_finalize(&sc->sc_refs, "tundtor");

	s = splhigh();
	klist_invalidate(&sc->sc_rsel.si_note);
	klist_invalidate(&sc->sc_wsel.si_note);
	splx(s);

	if (ISSET(sc->sc_flags, TUN_LAYER2))
		ether_ifdetach(ifp);

	if_detach(ifp);
	sigio_free(&sc->sc_sigio);

	free(sc, M_DEVBUF, sizeof *sc);
	return (0);
}

static struct tun_softc *
tun_get(dev_t dev)
{
	struct tun_softc *sc;

	smr_read_enter();
	SMR_LIST_FOREACH(sc, &tun_devs_list, sc_entry) {
		if (sc->sc_dev == dev) {
			refcnt_take(&sc->sc_refs);
			break;
		}
	}
	smr_read_leave();

	return (sc);
}

static inline void
tun_put(struct tun_softc *sc)
{
	refcnt_rele_wake(&sc->sc_refs);
}

int
tunopen(dev_t dev, int flag, int mode, struct proc *p)
{
	return (tun_dev_open(dev, &tun_cloner, mode, p));
}

int
tapopen(dev_t dev, int flag, int mode, struct proc *p)
{
	return (tun_dev_open(dev, &tap_cloner, mode, p));
}

int
tun_dev_open(dev_t dev, const struct if_clone *ifc, int mode, struct proc *p)
{
	struct tun_softc *sc;
	struct ifnet *ifp;
	int error;
	u_short stayup = 0;
	struct vnode *vp;

	char name[IFNAMSIZ];
	unsigned int rdomain;

	/*
	 * Find the vnode associated with this open before we sleep
	 * and let something else revoke it. Our caller has a reference
	 * to it so we don't need to account for it.
	 */
	if (!vfinddev(dev, VCHR, &vp))
		panic("%s vfinddev failed", __func__);

	snprintf(name, sizeof(name), "%s%u", ifc->ifc_name, minor(dev));
	rdomain = rtable_l2(p->p_p->ps_rtableid);

	/* let's find or make an interface to work with */
	while ((sc = tun_name_lookup(name)) == NULL) {
		error = if_clone_create(name, rdomain);
		switch (error) {
		case 0: /* it's probably ours */
			stayup = TUN_STAYUP;
			/* FALLTHROUGH */
		case EEXIST: /* we may have lost a race with someone else */
			break;
		default:
			return (error);
		}
	}

	refcnt_take(&sc->sc_refs);

	/* wait for it to be fully constructed before we use it */
	for (;;) {
		if (ISSET(sc->sc_flags, TUN_DEAD)) {
			error = ENXIO;
			goto done;
		}

		if (ISSET(sc->sc_flags, TUN_INITED))
			break;

		error = tsleep_nsec(sc, PCATCH, "tuninit", INFSLP);
		if (error != 0) {
			/* XXX if_clone_destroy if stayup? */
			goto done;
		}
	}

	/* Has tun_clone_destroy torn the rug out under us? */
	if (vp->v_type == VBAD) {
		error = ENXIO;
		goto done;
	}

	if (sc->sc_dev != 0) {
		/* aww, we lost */
		error = EBUSY;
		goto done;
	}
	/* it's ours now */
	sc->sc_dev = dev;
	CLR(sc->sc_flags, stayup);

	/* automatically mark the interface running on open */
	ifp = &sc->sc_if;
	NET_LOCK();
	SET(ifp->if_flags, IFF_UP | IFF_RUNNING);
	NET_UNLOCK();
	tun_link_state(ifp, LINK_STATE_FULL_DUPLEX);
	error = 0;

done:
	tun_put(sc);
	return (error);
}

/*
 * tunclose - close the device; if closing the real device, flush pending
 *  output and unless STAYUP bring down and destroy the interface.
 */
int
tunclose(dev_t dev, int flag, int mode, struct proc *p)
{
	return (tun_dev_close(dev, p));
}

int
tapclose(dev_t dev, int flag, int mode, struct proc *p)
{
	return (tun_dev_close(dev, p));
}

int
tun_dev_close(dev_t dev, struct proc *p)
{
	struct tun_softc	*sc;
	struct ifnet		*ifp;
	int			 error = 0;
	char			 name[IFNAMSIZ];
	int			 destroy = 0;

	sc = tun_get(dev);
	if (sc == NULL)
		return (ENXIO);

	ifp = &sc->sc_if;

	/*
	 * junk all pending output
	 */
	NET_LOCK();
	CLR(ifp->if_flags, IFF_UP | IFF_RUNNING);
	NET_UNLOCK();
	ifq_purge(&ifp->if_snd);

	CLR(sc->sc_flags, TUN_ASYNC);
	selwakeup(&sc->sc_rsel);
	sigio_free(&sc->sc_sigio);

	if (!ISSET(sc->sc_flags, TUN_DEAD)) {
		/* we can't hold a reference to sc before we start a dtor */
		if (!ISSET(sc->sc_flags, TUN_STAYUP)) {
			destroy = 1;
			strlcpy(name, ifp->if_xname, sizeof(name));
		} else {
			tun_link_state(ifp, LINK_STATE_DOWN);
		}
	}

	sc->sc_dev = 0;

	tun_put(sc);

	if (destroy)
		if_clone_destroy(name);

	return (error);
}

int
tun_init(struct tun_softc *sc)
{
	struct ifnet	*ifp = &sc->sc_if;
	struct ifaddr	*ifa;

	TUNDEBUG(("%s: tun_init\n", ifp->if_xname));

	ifp->if_flags |= IFF_UP | IFF_RUNNING;

	sc->sc_flags &= ~(TUN_IASET|TUN_DSTADDR|TUN_BRDADDR);
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct sockaddr_in *sin;

			sin = satosin(ifa->ifa_addr);
			if (sin && sin->sin_addr.s_addr)
				sc->sc_flags |= TUN_IASET;

			if (ifp->if_flags & IFF_POINTOPOINT) {
				sin = satosin(ifa->ifa_dstaddr);
				if (sin && sin->sin_addr.s_addr)
					sc->sc_flags |= TUN_DSTADDR;
			} else
				sc->sc_flags &= ~TUN_DSTADDR;

			if (ifp->if_flags & IFF_BROADCAST) {
				sin = satosin(ifa->ifa_broadaddr);
				if (sin && sin->sin_addr.s_addr)
					sc->sc_flags |= TUN_BRDADDR;
			} else
				sc->sc_flags &= ~TUN_BRDADDR;
		}
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			struct sockaddr_in6 *sin6;

			sin6 = satosin6(ifa->ifa_addr);
			if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
				sc->sc_flags |= TUN_IASET;

			if (ifp->if_flags & IFF_POINTOPOINT) {
				sin6 = satosin6(ifa->ifa_dstaddr);
				if (sin6 &&
				    !IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
					sc->sc_flags |= TUN_DSTADDR;
			} else
				sc->sc_flags &= ~TUN_DSTADDR;
		}
#endif /* INET6 */
	}

	return (0);
}

/*
 * Process an ioctl request.
 */
int
tun_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct tun_softc	*sc = (struct tun_softc *)(ifp->if_softc);
	struct ifreq		*ifr = (struct ifreq *)data;
	int			 error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		tun_init(sc);
		break;
	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP))
			SET(ifp->if_flags, IFF_RUNNING);
		else
			CLR(ifp->if_flags, IFF_RUNNING);
		break;

	case SIOCSIFDSTADDR:
		tun_init(sc);
		TUNDEBUG(("%s: destination address set\n", ifp->if_xname));
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > TUNMRU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;
	default:
		if (sc->sc_flags & TUN_LAYER2)
			error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		else
			error = ENOTTY;
	}

	return (error);
}

/*
 * tun_output - queue packets from higher level ready to put out.
 */
int
tun_output(struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst,
    struct rtentry *rt)
{
	u_int32_t		*af;

	if (!ISSET(ifp->if_flags, IFF_RUNNING)) {
		m_freem(m0);
		return (EHOSTDOWN);
	}

	M_PREPEND(m0, sizeof(*af), M_DONTWAIT);
	if (m0 == NULL)
		return (ENOBUFS);
	af = mtod(m0, u_int32_t *);
	*af = htonl(dst->sa_family);

	return (if_enqueue(ifp, m0));
}

int
tun_enqueue(struct ifnet *ifp, struct mbuf *m0)
{
	struct tun_softc	*sc = ifp->if_softc;
	int			 error;

	error = ifq_enqueue(&ifp->if_snd, m0);
	if (error != 0)
		return (error);

	tun_wakeup(sc);

	return (0);
}

void
tun_wakeup(struct tun_softc *sc)
{
	if (sc->sc_reading)
		wakeup(&sc->sc_if.if_snd);

	selwakeup(&sc->sc_rsel);
	if (sc->sc_flags & TUN_ASYNC)
		pgsigio(&sc->sc_sigio, SIGIO, 0);
}

/*
 * the cdevsw interface is now pretty minimal.
 */
int
tunioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	return (tun_dev_ioctl(dev, cmd, data));
}

int
tapioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	return (tun_dev_ioctl(dev, cmd, data));
}

int
tun_dev_ioctl(dev_t dev, u_long cmd, void *data)
{
	struct tun_softc	*sc;
	struct tuninfo		*tunp;
	int			 error = 0;

	sc = tun_get(dev);
	if (sc == NULL)
		return (ENXIO);

	switch (cmd) {
	case TUNSIFINFO:
		tunp = (struct tuninfo *)data;
		if (tunp->mtu < ETHERMIN || tunp->mtu > TUNMRU) {
			error = EINVAL;
			break;
		}
		if (tunp->type != sc->sc_if.if_type) {
			error = EINVAL;
			break;
		}
		sc->sc_if.if_mtu = tunp->mtu;
		sc->sc_if.if_flags =
		    (tunp->flags & TUN_IFF_FLAGS) |
		    (sc->sc_if.if_flags & ~TUN_IFF_FLAGS);
		sc->sc_if.if_baudrate = tunp->baudrate;
		break;
	case TUNGIFINFO:
		tunp = (struct tuninfo *)data;
		tunp->mtu = sc->sc_if.if_mtu;
		tunp->type = sc->sc_if.if_type;
		tunp->flags = sc->sc_if.if_flags;
		tunp->baudrate = sc->sc_if.if_baudrate;
		break;
#ifdef TUN_DEBUG
	case TUNSDEBUG:
		tundebug = *(int *)data;
		break;
	case TUNGDEBUG:
		*(int *)data = tundebug;
		break;
#endif
	case TUNSIFMODE:
		switch (*(int *)data & (IFF_POINTOPOINT|IFF_BROADCAST)) {
		case IFF_POINTOPOINT:
		case IFF_BROADCAST:
			sc->sc_if.if_flags &= ~TUN_IFF_FLAGS;
			sc->sc_if.if_flags |= *(int *)data & TUN_IFF_FLAGS;
			break;
		default:
			error = EINVAL;
			break;
		}
		break;

	case FIONBIO:
		break;
	case FIOASYNC:
		if (*(int *)data)
			sc->sc_flags |= TUN_ASYNC;
		else
			sc->sc_flags &= ~TUN_ASYNC;
		break;
	case FIONREAD:
		*(int *)data = ifq_hdatalen(&sc->sc_if.if_snd);
		break;
	case FIOSETOWN:
	case TIOCSPGRP:
		error = sigio_setown(&sc->sc_sigio, cmd, data);
		break;
	case FIOGETOWN:
	case TIOCGPGRP:
		sigio_getown(&sc->sc_sigio, cmd, data);
		break;
	case SIOCGIFADDR:
		if (!(sc->sc_flags & TUN_LAYER2)) {
			error = EINVAL;
			break;
		}
		bcopy(sc->sc_ac.ac_enaddr, data,
		    sizeof(sc->sc_ac.ac_enaddr));
		break;

	case SIOCSIFADDR:
		if (!(sc->sc_flags & TUN_LAYER2)) {
			error = EINVAL;
			break;
		}
		bcopy(data, sc->sc_ac.ac_enaddr,
		    sizeof(sc->sc_ac.ac_enaddr));
		break;
	default:
		error = ENOTTY;
		break;
	}

	tun_put(sc);
	return (error);
}

/*
 * The cdevsw read interface - reads a packet at a time, or at
 * least as much of a packet as can be read.
 */
int
tunread(dev_t dev, struct uio *uio, int ioflag)
{
	return (tun_dev_read(dev, uio, ioflag));
}

int
tapread(dev_t dev, struct uio *uio, int ioflag)
{
	return (tun_dev_read(dev, uio, ioflag));
}

int
tun_dev_read(dev_t dev, struct uio *uio, int ioflag)
{
	struct tun_softc	*sc;
	struct ifnet		*ifp;
	struct mbuf		*m, *m0;
	int			 error = 0;

	sc = tun_get(dev);
	if (sc == NULL)
		return (ENXIO);

	ifp = &sc->sc_if;

	error = ifq_deq_sleep(&ifp->if_snd, &m0, ISSET(ioflag, IO_NDELAY),
	    (PZERO + 1)|PCATCH, "tunread", &sc->sc_reading, &sc->sc_dev);
	if (error != 0)
		goto put;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif

	m = m0;
	while (uio->uio_resid > 0) {
		size_t len = ulmin(uio->uio_resid, m->m_len);
		if (len > 0) {
			error = uiomove(mtod(m, void *), len, uio);
			if (error != 0)
				break;
		}

		m = m->m_next;
		if (m == NULL)
			break;
	}

	m_freem(m0);

put:
	tun_put(sc);
	return (error);
}

/*
 * the cdevsw write interface - an atomic write is a packet - or else!
 */
int
tunwrite(dev_t dev, struct uio *uio, int ioflag)
{
	return (tun_dev_write(dev, uio, ioflag, 0));
}

int
tapwrite(dev_t dev, struct uio *uio, int ioflag)
{
	return (tun_dev_write(dev, uio, ioflag, ETHER_ALIGN));
}

int
tun_dev_write(dev_t dev, struct uio *uio, int ioflag, int align)
{
	struct tun_softc	*sc;
	struct ifnet		*ifp;
	struct mbuf		*m0;
	int			error = 0;
	size_t			mlen;

	sc = tun_get(dev);
	if (sc == NULL)
		return (ENXIO);

	ifp = &sc->sc_if;

	if (uio->uio_resid < ifp->if_hdrlen ||
	    uio->uio_resid > (ifp->if_hdrlen + ifp->if_hardmtu)) {
		error = EMSGSIZE;
		goto put;
	}

	align += max_linkhdr;
	mlen = align + uio->uio_resid;

	m0 = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m0 == NULL) {
		error = ENOMEM;
		goto put;
	}
	if (mlen > MHLEN) {
		m_clget(m0, M_DONTWAIT, mlen);
		if (!ISSET(m0->m_flags, M_EXT)) {
			error = ENOMEM;
			goto drop;
		}
	}

	m_align(m0, mlen);
	m0->m_pkthdr.len = m0->m_len = mlen;
	m_adj(m0, align);

	error = uiomove(mtod(m0, void *), m0->m_len, uio);
	if (error != 0)
		goto drop;

	NET_LOCK();
	if_vinput(ifp, m0);
	NET_UNLOCK();

	tun_put(sc);
	return (0);

drop:
	m_freem(m0);
put:
	tun_put(sc);
	return (error);
}

void
tun_input(struct ifnet *ifp, struct mbuf *m0)
{
	uint32_t		af;

	KASSERT(m0->m_len >= sizeof(af));

	af = *mtod(m0, uint32_t *);
	/* strip the tunnel header */
	m_adj(m0, sizeof(af));

	switch (ntohl(af)) {
	case AF_INET:
		ipv4_input(ifp, m0);
		break;
#ifdef INET6
	case AF_INET6:
		ipv6_input(ifp, m0);
		break;
#endif
#ifdef MPLS
	case AF_MPLS:
		mpls_input(ifp, m0);
		break;
#endif
	default:
		m_freem(m0);
		break;
	}
}

int
tunkqfilter(dev_t dev, struct knote *kn)
{
	return (tun_dev_kqfilter(dev, kn));
}

int
tapkqfilter(dev_t dev, struct knote *kn)
{
	return (tun_dev_kqfilter(dev, kn));
}

int
tun_dev_kqfilter(dev_t dev, struct knote *kn)
{
	struct tun_softc	*sc;
	struct ifnet		*ifp;
	struct klist		*klist;
	int			 error = 0;
	int			 s;

	sc = tun_get(dev);
	if (sc == NULL)
		return (ENXIO);

	ifp = &sc->sc_if;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->sc_rsel.si_note;
		kn->kn_fop = &tunread_filtops;
		break;
	case EVFILT_WRITE:
		klist = &sc->sc_wsel.si_note;
		kn->kn_fop = &tunwrite_filtops;
		break;
	default:
		error = EINVAL;
		goto put;
	}

	kn->kn_hook = (caddr_t)sc; /* XXX give the sc_ref to the hook? */

	s = splhigh();
	klist_insert_locked(klist, kn);
	splx(s);

put:
	tun_put(sc);
	return (error);
}

void
filt_tunrdetach(struct knote *kn)
{
	int			 s;
	struct tun_softc	*sc = kn->kn_hook;

	s = splhigh();
	klist_remove_locked(&sc->sc_rsel.si_note, kn);
	splx(s);
}

int
filt_tunread(struct knote *kn, long hint)
{
	struct tun_softc	*sc = kn->kn_hook;
	struct ifnet		*ifp = &sc->sc_if;

	kn->kn_data = ifq_hdatalen(&ifp->if_snd);

	return (kn->kn_data > 0);
}

void
filt_tunwdetach(struct knote *kn)
{
	int			 s;
	struct tun_softc	*sc = kn->kn_hook;

	s = splhigh();
	klist_remove_locked(&sc->sc_wsel.si_note, kn);
	splx(s);
}

int
filt_tunwrite(struct knote *kn, long hint)
{
	struct tun_softc	*sc = kn->kn_hook;
	struct ifnet		*ifp = &sc->sc_if;

	kn->kn_data = ifp->if_hdrlen + ifp->if_hardmtu;

	return (1);
}

void
tun_start(struct ifnet *ifp)
{
	struct tun_softc	*sc = ifp->if_softc;

	splassert(IPL_NET);

	if (ifq_len(&ifp->if_snd))
		tun_wakeup(sc);
}

void
tun_link_state(struct ifnet *ifp, int link_state)
{
	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}
}
