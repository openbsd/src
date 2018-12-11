/*	$OpenBSD: if_tun.c,v 1.183 2018/12/11 01:34:10 dlg Exp $	*/
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
#include <sys/protosw.h>
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
#include <sys/poll.h>
#include <sys/conf.h>


#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/rtable.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#ifdef PIPEX
#include <net/pipex.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net/if_tun.h>

struct tun_softc {
	struct arpcom	arpcom;		/* ethernet common data */
	struct selinfo	tun_rsel;	/* read select */
	struct selinfo	tun_wsel;	/* write select (not used) */
	LIST_ENTRY(tun_softc) entry;	/* all tunnel interfaces */
	int		tun_unit;
	uid_t		tun_siguid;	/* uid for process that set tun_pgid */
	uid_t		tun_sigeuid;	/* euid for process that set tun_pgid */
	pid_t		tun_pgid;	/* the process group - if any */
	u_short		tun_flags;	/* misc flags */
#define tun_if	arpcom.ac_if
#ifdef PIPEX
	struct pipex_iface_context pipex_iface; /* pipex context */
#endif
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

/* cdev functions */
int	tunopen(dev_t, int, int, struct proc *);
int	tunclose(dev_t, int, int, struct proc *);
int	tunioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	tunread(dev_t, struct uio *, int);
int	tunwrite(dev_t, struct uio *, int);
int	tunpoll(dev_t, int, struct proc *);
int	tunkqfilter(dev_t, struct knote *);

int	tapopen(dev_t, int, int, struct proc *);
int	tapclose(dev_t, int, int, struct proc *);
int	tapioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	tapread(dev_t, struct uio *, int);
int	tapwrite(dev_t, struct uio *, int);
int	tappoll(dev_t, int, struct proc *);
int	tapkqfilter(dev_t, struct knote *);

int	tun_dev_open(struct tun_softc *, int, int, struct proc *);
int	tun_dev_close(struct tun_softc *, int, int, struct proc *);
int	tun_dev_ioctl(struct tun_softc *, u_long, caddr_t, int, struct proc *);
int	tun_dev_read(struct tun_softc *, struct uio *, int);
int	tun_dev_write(struct tun_softc *, struct uio *, int);
int	tun_dev_poll(struct tun_softc *, int, struct proc *);
int	tun_dev_kqfilter(struct tun_softc *, struct knote *);


int	tun_ioctl(struct ifnet *, u_long, caddr_t);
int	tun_output(struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *);
int	tun_clone_create(struct if_clone *, int);
int	tap_clone_create(struct if_clone *, int);
int	tun_create(struct if_clone *, int, int);
int	tun_clone_destroy(struct ifnet *);
static inline struct	tun_softc *tun_lookup(int);
static inline struct	tun_softc *tap_lookup(int);
void	tun_wakeup(struct tun_softc *);
int	tun_init(struct tun_softc *);
void	tun_start(struct ifnet *);
int	filt_tunread(struct knote *, long);
int	filt_tunwrite(struct knote *, long);
void	filt_tunrdetach(struct knote *);
void	filt_tunwdetach(struct knote *);
void	tun_link_state(struct tun_softc *);

struct filterops tunread_filtops =
	{ 1, NULL, filt_tunrdetach, filt_tunread};

struct filterops tunwrite_filtops =
	{ 1, NULL, filt_tunwdetach, filt_tunwrite};

LIST_HEAD(, tun_softc) tun_softc_list;
LIST_HEAD(, tun_softc) tap_softc_list;

struct if_clone tun_cloner =
    IF_CLONE_INITIALIZER("tun", tun_clone_create, tun_clone_destroy);

struct if_clone tap_cloner =
    IF_CLONE_INITIALIZER("tap", tap_clone_create, tun_clone_destroy);

void
tunattach(int n)
{
	LIST_INIT(&tun_softc_list);
	LIST_INIT(&tap_softc_list);
	if_clone_attach(&tun_cloner);
	if_clone_attach(&tap_cloner);
#ifdef PIPEX
	pipex_init();
#endif
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

int
tun_create(struct if_clone *ifc, int unit, int flags)
{
	struct tun_softc	*tp;
	struct ifnet		*ifp;

	if (unit > minor(~0U))
		return (ENXIO);

	tp = malloc(sizeof(*tp), M_DEVBUF, M_WAITOK|M_ZERO);
	tp->tun_unit = unit;
	tp->tun_flags = TUN_INITED|TUN_STAYUP;

	ifp = &tp->tun_if;
	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s%d", ifc->ifc_name,
	    unit);
	ifp->if_softc = tp;

	ifp->if_ioctl = tun_ioctl;
	ifp->if_output = tun_output;
	ifp->if_start = tun_start;
	ifp->if_hardmtu = TUNMRU;
	ifp->if_link_state = LINK_STATE_DOWN;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);

	if ((flags & TUN_LAYER2) == 0) {
		tp->tun_flags &= ~TUN_LAYER2;
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
		LIST_INSERT_HEAD(&tun_softc_list, tp, entry);
	} else {
		tp->tun_flags |= TUN_LAYER2;
		ether_fakeaddr(ifp);
		ifp->if_flags =
		    (IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST);
		ifp->if_capabilities = IFCAP_VLAN_MTU;

		if_attach(ifp);
		ether_ifattach(ifp);

		LIST_INSERT_HEAD(&tap_softc_list, tp, entry);
	}

#ifdef PIPEX
	if ((tp->tun_flags & TUN_LAYER2) == 0)
		pipex_iface_init(&tp->pipex_iface, ifp);
#endif

	return (0);
}

int
tun_clone_destroy(struct ifnet *ifp)
{
	struct tun_softc	*tp = ifp->if_softc;
	int			 s;

#ifdef PIPEX
	if ((tp->tun_flags & TUN_LAYER2) == 0)
		pipex_iface_fini(&tp->pipex_iface);
#endif
	tun_wakeup(tp);

	s = splhigh();
	klist_invalidate(&tp->tun_rsel.si_note);
	klist_invalidate(&tp->tun_wsel.si_note);
	splx(s);

	LIST_REMOVE(tp, entry);

	if (tp->tun_flags & TUN_LAYER2)
		ether_ifdetach(ifp);

	if_detach(ifp);

	free(tp, M_DEVBUF, sizeof *tp);
	return (0);
}

static inline struct tun_softc *
tun_lookup(int unit)
{
	struct tun_softc *tp;

	LIST_FOREACH(tp, &tun_softc_list, entry)
		if (tp->tun_unit == unit)
			return (tp);
	return (NULL);
}

static inline struct tun_softc *
tap_lookup(int unit)
{
	struct tun_softc *tp;

	LIST_FOREACH(tp, &tap_softc_list, entry)
		if (tp->tun_unit == unit)
			return (tp);
	return (NULL);
}

/*
 * tunnel open - must be superuser & the device must be
 * configured in
 */
int
tunopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tun_softc *tp;
	unsigned int rdomain = rtable_l2(p->p_p->ps_rtableid);

	if ((tp = tun_lookup(minor(dev))) == NULL) {	/* create on demand */
		char	xname[IFNAMSIZ];
		int	error;

		snprintf(xname, sizeof(xname), "%s%d", "tun", minor(dev));
		NET_LOCK();
		error = if_clone_create(xname, rdomain);
		NET_UNLOCK();
		if (error != 0)
			return (error);

		if ((tp = tun_lookup(minor(dev))) == NULL)
			return (ENXIO);
		tp->tun_flags &= ~TUN_STAYUP;
	}

	return (tun_dev_open(tp, flag, mode, p));
}

int
tapopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tun_softc *tp;
	unsigned int rdomain = rtable_l2(p->p_p->ps_rtableid);

	if ((tp = tap_lookup(minor(dev))) == NULL) {	/* create on demand */
		char	xname[IFNAMSIZ];
		int	error;

		snprintf(xname, sizeof(xname), "%s%d", "tap", minor(dev));
		NET_LOCK();
		error = if_clone_create(xname, rdomain);
		NET_UNLOCK();
		if (error != 0)
			return (error);

		if ((tp = tap_lookup(minor(dev))) == NULL)
			return (ENXIO);
		tp->tun_flags &= ~TUN_STAYUP;
	}

	return (tun_dev_open(tp, flag, mode, p));
}

int
tun_dev_open(struct tun_softc *tp, int flag, int mode, struct proc *p)
{
	struct ifnet *ifp;

	if (tp->tun_flags & TUN_OPEN)
		return (EBUSY);

	ifp = &tp->tun_if;
	tp->tun_flags |= TUN_OPEN;
	if (flag & FNONBLOCK)
		tp->tun_flags |= TUN_NBIO;

	/* automatically mark the interface running on open */
	ifp->if_flags |= IFF_RUNNING;
	tun_link_state(tp);

	TUNDEBUG(("%s: open\n", ifp->if_xname));
	return (0);
}

/*
 * tunclose - close the device; if closing the real device, flush pending
 *  output and unless STAYUP bring down and destroy the interface.
 */
int
tunclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tun_softc	*tp;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);
	return (tun_dev_close(tp, flag, mode, p));
}

int
tapclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tun_softc	*tp;

	if ((tp = tap_lookup(minor(dev))) == NULL)
		return (ENXIO);
	return (tun_dev_close(tp, flag, mode, p));
}

int
tun_dev_close(struct tun_softc *tp, int flag, int mode, struct proc *p)
{
	int			 error = 0;
	struct ifnet		*ifp;

	ifp = &tp->tun_if;
	tp->tun_flags &= ~(TUN_OPEN|TUN_NBIO|TUN_ASYNC);

	/*
	 * junk all pending output
	 */
	ifp->if_flags &= ~IFF_RUNNING;
	tun_link_state(tp);
	IFQ_PURGE(&ifp->if_snd);

	TUNDEBUG(("%s: closed\n", ifp->if_xname));

	if (!(tp->tun_flags & TUN_STAYUP)) {
		NET_LOCK();
		error = if_clone_destroy(ifp->if_xname);
		NET_UNLOCK();
	} else {
		tp->tun_pgid = 0;
		selwakeup(&tp->tun_rsel);
	}

	return (error);
}

int
tun_init(struct tun_softc *tp)
{
	struct ifnet	*ifp = &tp->tun_if;
	struct ifaddr	*ifa;

	TUNDEBUG(("%s: tun_init\n", ifp->if_xname));

	ifp->if_flags |= IFF_UP | IFF_RUNNING;

	tp->tun_flags &= ~(TUN_IASET|TUN_DSTADDR|TUN_BRDADDR);
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct sockaddr_in *sin;

			sin = satosin(ifa->ifa_addr);
			if (sin && sin->sin_addr.s_addr)
				tp->tun_flags |= TUN_IASET;

			if (ifp->if_flags & IFF_POINTOPOINT) {
				sin = satosin(ifa->ifa_dstaddr);
				if (sin && sin->sin_addr.s_addr)
					tp->tun_flags |= TUN_DSTADDR;
			} else
				tp->tun_flags &= ~TUN_DSTADDR;

			if (ifp->if_flags & IFF_BROADCAST) {
				sin = satosin(ifa->ifa_broadaddr);
				if (sin && sin->sin_addr.s_addr)
					tp->tun_flags |= TUN_BRDADDR;
			} else
				tp->tun_flags &= ~TUN_BRDADDR;
		}
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			struct sockaddr_in6 *sin6;

			sin6 = satosin6(ifa->ifa_addr);
			if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
				tp->tun_flags |= TUN_IASET;

			if (ifp->if_flags & IFF_POINTOPOINT) {
				sin6 = satosin6(ifa->ifa_dstaddr);
				if (sin6 &&
				    !IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
					tp->tun_flags |= TUN_DSTADDR;
			} else
				tp->tun_flags &= ~TUN_DSTADDR;
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
	struct tun_softc	*tp = (struct tun_softc *)(ifp->if_softc);
	struct ifreq		*ifr = (struct ifreq *)data;
	int			 error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		tun_init(tp);
		break;
	case SIOCSIFDSTADDR:
		tun_init(tp);
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
	case SIOCSIFFLAGS:
		break;
	default:
		if (tp->tun_flags & TUN_LAYER2)
			error = ether_ioctl(ifp, &tp->arpcom, cmd, data);
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
	struct tun_softc	*tp = ifp->if_softc;
	int			 error;
	u_int32_t		*af;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		m_freem(m0);
		return (EHOSTDOWN);
	}

	TUNDEBUG(("%s: tun_output\n", ifp->if_xname));

	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG(("%s: not ready %#x\n", ifp->if_xname,
		     tp->tun_flags));
		m_freem(m0);
		return (EHOSTDOWN);
	}

	if (tp->tun_flags & TUN_LAYER2)
		return (ether_output(ifp, m0, dst, rt));

	M_PREPEND(m0, sizeof(*af), M_DONTWAIT);
	if (m0 == NULL)
		return (ENOBUFS);
	af = mtod(m0, u_int32_t *);
	*af = htonl(dst->sa_family);

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif
#ifdef PIPEX
	if (pipex_enable && (m0 = pipex_output(m0, dst->sa_family,
	    sizeof(u_int32_t), &tp->pipex_iface)) == NULL) {
		return (0);
	}
#endif

	error = if_enqueue(ifp, m0);

	if (error) {
		ifp->if_collisions++;
		return (error);
	}

	tun_wakeup(tp);
	return (0);
}

void
tun_wakeup(struct tun_softc *tp)
{
	if (tp->tun_flags & TUN_RWAIT) {
		tp->tun_flags &= ~TUN_RWAIT;
		wakeup((caddr_t)tp);
	}
	if (tp->tun_flags & TUN_ASYNC && tp->tun_pgid)
		csignal(tp->tun_pgid, SIGIO,
		    tp->tun_siguid, tp->tun_sigeuid);
	selwakeup(&tp->tun_rsel);
}

/*
 * the cdevsw interface is now pretty minimal.
 */
int
tunioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct tun_softc *tp;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);
	return (tun_dev_ioctl(tp, cmd, data, flag, p));
}

int
tapioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct tun_softc *tp;

	if ((tp = tap_lookup(minor(dev))) == NULL)
		return (ENXIO);
	return (tun_dev_ioctl(tp, cmd, data, flag, p));
}

int
tun_dev_ioctl(struct tun_softc *tp, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
	struct tuninfo		*tunp;

	switch (cmd) {
	case TUNSIFINFO:
		tunp = (struct tuninfo *)data;
		if (tunp->mtu < ETHERMIN || tunp->mtu > TUNMRU)
			return (EINVAL);
		tp->tun_if.if_mtu = tunp->mtu;
		tp->tun_if.if_type = tunp->type;
		tp->tun_if.if_flags =
		    (tunp->flags & TUN_IFF_FLAGS) |
		    (tp->tun_if.if_flags & ~TUN_IFF_FLAGS);
		tp->tun_if.if_baudrate = tunp->baudrate;
		break;
	case TUNGIFINFO:
		tunp = (struct tuninfo *)data;
		tunp->mtu = tp->tun_if.if_mtu;
		tunp->type = tp->tun_if.if_type;
		tunp->flags = tp->tun_if.if_flags;
		tunp->baudrate = tp->tun_if.if_baudrate;
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
			tp->tun_if.if_flags &= ~TUN_IFF_FLAGS;
			tp->tun_if.if_flags |= *(int *)data & TUN_IFF_FLAGS;
			break;
		default:
			return (EINVAL);
		}
		break;

	case FIONBIO:
		if (*(int *)data)
			tp->tun_flags |= TUN_NBIO;
		else
			tp->tun_flags &= ~TUN_NBIO;
		break;
	case FIOASYNC:
		if (*(int *)data)
			tp->tun_flags |= TUN_ASYNC;
		else
			tp->tun_flags &= ~TUN_ASYNC;
		break;
	case FIONREAD:
		*(int *)data = ifq_hdatalen(&tp->tun_if.if_snd);
		break;
	case TIOCSPGRP:
		tp->tun_pgid = *(int *)data;
		tp->tun_siguid = p->p_ucred->cr_ruid;
		tp->tun_sigeuid = p->p_ucred->cr_uid;
		break;
	case TIOCGPGRP:
		*(int *)data = tp->tun_pgid;
		break;
	case SIOCGIFADDR:
		if (!(tp->tun_flags & TUN_LAYER2))
			return (EINVAL);
		bcopy(tp->arpcom.ac_enaddr, data,
		    sizeof(tp->arpcom.ac_enaddr));
		break;

	case SIOCSIFADDR:
		if (!(tp->tun_flags & TUN_LAYER2))
			return (EINVAL);
		bcopy(data, tp->arpcom.ac_enaddr,
		    sizeof(tp->arpcom.ac_enaddr));
		break;
	default:
#ifdef PIPEX
		if (!(tp->tun_flags & TUN_LAYER2)) {
			int ret;
			ret = pipex_ioctl(&tp->pipex_iface, cmd, data);
			return (ret);
		}
#endif
		return (ENOTTY);
	}
	return (0);
}

/*
 * The cdevsw read interface - reads a packet at a time, or at
 * least as much of a packet as can be read.
 */
int
tunread(dev_t dev, struct uio *uio, int ioflag)
{
	struct tun_softc *tp;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);
	return (tun_dev_read(tp, uio, ioflag));
}

int
tapread(dev_t dev, struct uio *uio, int ioflag)
{
	struct tun_softc *tp;

	if ((tp = tap_lookup(minor(dev))) == NULL)
		return (ENXIO);
	return (tun_dev_read(tp, uio, ioflag));
}

int
tun_dev_read(struct tun_softc *tp, struct uio *uio, int ioflag)
{
	struct ifnet		*ifp = &tp->tun_if;
	struct mbuf		*m, *m0;
	unsigned int		 ifidx;
	int			 error = 0;
	size_t			 len;

	if ((tp->tun_flags & TUN_READY) != TUN_READY)
		return (EHOSTDOWN);

	ifidx = ifp->if_index;
	tp->tun_flags &= ~TUN_RWAIT;

	do {
		struct ifnet *ifp1;
		int destroyed;

		while ((tp->tun_flags & TUN_READY) != TUN_READY) {
			if ((error = tsleep((caddr_t)tp,
			    (PZERO + 1)|PCATCH, "tunread", 0)) != 0)
				return (error);
			/* Make sure the interface still exists. */
			ifp1 = if_get(ifidx);
			destroyed = (ifp1 == NULL);
			if_put(ifp1);
			if (destroyed)
				return (ENXIO);
		}
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL) {
			if (tp->tun_flags & TUN_NBIO && ioflag & IO_NDELAY)
				return (EWOULDBLOCK);
			tp->tun_flags |= TUN_RWAIT;
			if ((error = tsleep((caddr_t)tp,
			    (PZERO + 1)|PCATCH, "tunread", 0)) != 0)
				return (error);
			/* Make sure the interface still exists. */
			ifp1 = if_get(ifidx);
			destroyed = (ifp1 == NULL);
			if_put(ifp1);
			if (destroyed)
				return (ENXIO);
		}
	} while (m0 == NULL);

	if (tp->tun_flags & TUN_LAYER2) {
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif
	}

	while (m0 != NULL && uio->uio_resid > 0 && error == 0) {
		len = ulmin(uio->uio_resid, m0->m_len);
		if (len != 0)
			error = uiomove(mtod(m0, caddr_t), len, uio);
		m = m_free(m0);
		m0 = m;
	}

	if (m0 != NULL) {
		TUNDEBUG(("Dropping mbuf\n"));
		m_freem(m0);
	}
	if (error)
		ifp->if_oerrors++;

	return (error);
}

/*
 * the cdevsw write interface - an atomic write is a packet - or else!
 */
int
tunwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct tun_softc *tp;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);
	return (tun_dev_write(tp, uio, ioflag));
}

int
tapwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct tun_softc *tp;

	if ((tp = tap_lookup(minor(dev))) == NULL)
		return (ENXIO);
	return (tun_dev_write(tp, uio, ioflag));
}

int
tun_dev_write(struct tun_softc *tp, struct uio *uio, int ioflag)
{
	struct ifnet		*ifp;
	u_int32_t		*th;
	struct mbuf		*top, **mp, *m;
	int			error = 0, tlen;
	size_t			mlen;

	ifp = &tp->tun_if;
	TUNDEBUG(("%s: tunwrite\n", ifp->if_xname));

	if (uio->uio_resid == 0 || uio->uio_resid > ifp->if_mtu +
	    (tp->tun_flags & TUN_LAYER2 ? ETHER_HDR_LEN : sizeof(*th))) {
		TUNDEBUG(("%s: len=%d!\n", ifp->if_xname, uio->uio_resid));
		return (EMSGSIZE);
	}
	tlen = uio->uio_resid;

	/* get a header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	mlen = MHLEN;
	if (uio->uio_resid >= MINCLSIZE) {
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			m_free(m);
			return (ENOBUFS);
		}
		mlen = MCLBYTES;
	}

	top = NULL;
	mp = &top;
	if (tp->tun_flags & TUN_LAYER2) {
		/*
		 * Pad so that IP header is correctly aligned
		 * this is necessary for all strict aligned architectures.
		 */
		mlen -= ETHER_ALIGN;
		m->m_data += ETHER_ALIGN;
	}
	while (error == 0 && uio->uio_resid > 0) {
		m->m_len = ulmin(mlen, uio->uio_resid);
		error = uiomove(mtod (m, caddr_t), m->m_len, uio);
		*mp = m;
		mp = &m->m_next;
		if (error == 0 && uio->uio_resid > 0) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				error = ENOBUFS;
				break;
			}
			mlen = MLEN;
			if (uio->uio_resid >= MINCLSIZE) {
				MCLGET(m, M_DONTWAIT);
				if (!(m->m_flags & M_EXT)) {
					error = ENOBUFS;
					m_free(m);
					break;
				}
				mlen = MCLBYTES;
			}
		}
	}
	if (error) {
		m_freem(top);
		ifp->if_ierrors++;
		return (error);
	}

	top->m_pkthdr.len = tlen;

	if (tp->tun_flags & TUN_LAYER2) {
		struct mbuf_list ml = MBUF_LIST_INITIALIZER();

		ml_enqueue(&ml, top);
		if_input(ifp, &ml);
		return (0);
	}

#if NBPFILTER > 0
	if (ifp->if_bpf) {
		bpf_mtap(ifp->if_bpf, top, BPF_DIRECTION_IN);
	}
#endif

	th = mtod(top, u_int32_t *);
	/* strip the tunnel header */
	top->m_data += sizeof(*th);
	top->m_len  -= sizeof(*th);
	top->m_pkthdr.len -= sizeof(*th);
	top->m_pkthdr.ph_rtableid = ifp->if_rdomain;
	top->m_pkthdr.ph_ifidx = ifp->if_index;

	ifp->if_ipackets++;
	ifp->if_ibytes += top->m_pkthdr.len;

	NET_LOCK();

	switch (ntohl(*th)) {
	case AF_INET:
		ipv4_input(ifp, top);
		break;
#ifdef INET6
	case AF_INET6:
		ipv6_input(ifp, top);
		break;
#endif
	default:
		m_freem(top);
		error = EAFNOSUPPORT;
		break;
	}

	NET_UNLOCK();

	return (error);
}

/*
 * tunpoll - the poll interface, this is only useful on reads
 * really. The write detect always returns true, write never blocks
 * anyway, it either accepts the packet or drops it.
 */
int
tunpoll(dev_t dev, int events, struct proc *p)
{
	struct tun_softc *tp;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (POLLERR);
	return (tun_dev_poll(tp, events, p));
}

int
tappoll(dev_t dev, int events, struct proc *p)
{
	struct tun_softc *tp;

	if ((tp = tap_lookup(minor(dev))) == NULL)
		return (POLLERR);
	return (tun_dev_poll(tp, events, p));
}

int
tun_dev_poll(struct tun_softc *tp, int events, struct proc *p)
{
	int			 revents;
	struct ifnet		*ifp;
	unsigned int		 len;

	ifp = &tp->tun_if;
	revents = 0;
	TUNDEBUG(("%s: tunpoll\n", ifp->if_xname));

	if (events & (POLLIN | POLLRDNORM)) {
		len = IFQ_LEN(&ifp->if_snd);
		if (len > 0) {
			TUNDEBUG(("%s: tunselect q=%d\n", ifp->if_xname, len));
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			TUNDEBUG(("%s: tunpoll waiting\n", ifp->if_xname));
			selrecord(p, &tp->tun_rsel);
		}
	}
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);
	return (revents);
}

/*
 * kqueue(2) support.
 *
 * The tun driver uses an array of tun_softc's based on the minor number
 * of the device.  kn->kn_hook gets set to the specific tun_softc.
 *
 * filt_tunread() sets kn->kn_data to the iface qsize
 * filt_tunwrite() sets kn->kn_data to the MTU size
 */
int
tunkqfilter(dev_t dev, struct knote *kn)
{
	struct tun_softc *tp;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);
	return (tun_dev_kqfilter(tp, kn));
}

int
tapkqfilter(dev_t dev, struct knote *kn)
{
	struct tun_softc *tp;

	if ((tp = tap_lookup(minor(dev))) == NULL)
		return (ENXIO);
	return (tun_dev_kqfilter(tp, kn));
}

int
tun_dev_kqfilter(struct tun_softc *tp, struct knote *kn)
{
	int			 s;
	struct klist		*klist;
	struct ifnet		*ifp;

	ifp = &tp->tun_if;
	TUNDEBUG(("%s: tunkqfilter\n", ifp->if_xname));

	switch (kn->kn_filter) {
		case EVFILT_READ:
			klist = &tp->tun_rsel.si_note;
			kn->kn_fop = &tunread_filtops;
			break;
		case EVFILT_WRITE:
			klist = &tp->tun_wsel.si_note;
			kn->kn_fop = &tunwrite_filtops;
			break;
		default:
			return (EINVAL);
	}

	kn->kn_hook = (caddr_t)tp;

	s = splhigh();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}

void
filt_tunrdetach(struct knote *kn)
{
	int			 s;
	struct tun_softc	*tp;

	tp = (struct tun_softc *)kn->kn_hook;
	s = splhigh();
	if (!(kn->kn_status & KN_DETACHED))
		SLIST_REMOVE(&tp->tun_rsel.si_note, kn, knote, kn_selnext);
	splx(s);
}

int
filt_tunread(struct knote *kn, long hint)
{
	struct tun_softc	*tp;
	struct ifnet		*ifp;
	unsigned int		 len;

	if (kn->kn_status & KN_DETACHED) {
		kn->kn_data = 0;
		return (1);
	}

	tp = (struct tun_softc *)kn->kn_hook;
	ifp = &tp->tun_if;

	len = IFQ_LEN(&ifp->if_snd);
	if (len > 0) {
		kn->kn_data = len;

		TUNDEBUG(("%s: tunkqread q=%d\n", ifp->if_xname,
		    IFQ_LEN(&ifp->if_snd)));
		return (1);
	}
	TUNDEBUG(("%s: tunkqread waiting\n", ifp->if_xname));
	return (0);
}

void
filt_tunwdetach(struct knote *kn)
{
	int			 s;
	struct tun_softc	*tp;

	tp = (struct tun_softc *)kn->kn_hook;
	s = splhigh();
	if (!(kn->kn_status & KN_DETACHED))
		SLIST_REMOVE(&tp->tun_wsel.si_note, kn, knote, kn_selnext);
	splx(s);
}

int
filt_tunwrite(struct knote *kn, long hint)
{
	struct tun_softc	*tp;
	struct ifnet		*ifp;

	if (kn->kn_status & KN_DETACHED) {
		kn->kn_data = 0;
		return (1);
	}

	tp = (struct tun_softc *)kn->kn_hook;
	ifp = &tp->tun_if;

	kn->kn_data = ifp->if_mtu;

	return (1);
}

void
tun_start(struct ifnet *ifp)
{
	struct tun_softc	*tp = ifp->if_softc;

	splassert(IPL_NET);

	if (IFQ_LEN(&ifp->if_snd))
		tun_wakeup(tp);
}

void
tun_link_state(struct tun_softc *tp)
{
	struct ifnet *ifp = &tp->tun_if;
	int link_state = LINK_STATE_DOWN;

	if (tp->tun_flags & TUN_OPEN) {
		if (tp->tun_flags & TUN_LAYER2)
			link_state = LINK_STATE_FULL_DUPLEX;
		else
			link_state = LINK_STATE_UP;
	}
	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}
}
