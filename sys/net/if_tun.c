/*	$OpenBSD: if_tun.c,v 1.104 2010/01/12 11:28:09 deraadt Exp $	*/
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
#include <sys/file.h>
#include <sys/time.h>
#include <sys/device.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/poll.h>
#include <sys/conf.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef PIPEX
#include <net/pipex.h>
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
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
	LIST_ENTRY(tun_softc) tun_list;	/* all tunnel interfaces */
	struct ifmedia	tun_media;
	int		tun_unit;
	uid_t		tun_siguid;	/* uid for process that set tun_pgid */
	uid_t		tun_sigeuid;	/* euid for process that set tun_pgid */
	pid_t		tun_pgid;	/* the process group - if any */
	u_short		tun_flags;	/* misc flags */
#define tun_if	arpcom.ac_if
#ifdef PIPEX
	/* pipex context */
	struct pipex_iface_context	pipex_iface;
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
int	tunopen(dev_t, int, int, struct proc *);
int	tunclose(dev_t, int, int, struct proc *);
int	tun_ioctl(struct ifnet *, u_long, caddr_t);
int	tun_output(struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *);
int	tunioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	tunread(dev_t, struct uio *, int);
int	tunwrite(dev_t, struct uio *, int);
int	tunpoll(dev_t, int, struct proc *);
int	tunkqfilter(dev_t, struct knote *);
int	tun_clone_create(struct if_clone *, int);
int	tun_create(struct if_clone *, int, int);
int	tun_clone_destroy(struct ifnet *);
struct	tun_softc *tun_lookup(int);
void	tun_wakeup(struct tun_softc *);
int	tun_switch(struct tun_softc *, int);

int	tuninit(struct tun_softc *);
int	filt_tunread(struct knote *, long);
int	filt_tunwrite(struct knote *, long);
void	filt_tunrdetach(struct knote *);
void	filt_tunwdetach(struct knote *);
void	tunstart(struct ifnet *);
void	tun_link_state(struct tun_softc *);
int	tun_media_change(struct ifnet *);
void	tun_media_status(struct ifnet *, struct ifmediareq *);

struct filterops tunread_filtops =
	{ 1, NULL, filt_tunrdetach, filt_tunread};

struct filterops tunwrite_filtops =
	{ 1, NULL, filt_tunwdetach, filt_tunwrite};

LIST_HEAD(, tun_softc) tun_softc_list;

struct if_clone tun_cloner =
    IF_CLONE_INITIALIZER("tun", tun_clone_create, tun_clone_destroy);

void
tunattach(int n)
{
	LIST_INIT(&tun_softc_list);
	if_clone_attach(&tun_cloner);
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
tun_create(struct if_clone *ifc, int unit, int flags)
{
	struct tun_softc	*tp;
	struct ifnet		*ifp;
	int			 s;

	tp = malloc(sizeof(*tp), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!tp)
		return (ENOMEM);

	tp->tun_unit = unit;
	tp->tun_flags = TUN_INITED|TUN_STAYUP;

	ifp = &tp->tun_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "%s%d", ifc->ifc_name,
	    unit);
	ether_fakeaddr(ifp);

	ifp->if_softc = tp;
	ifp->if_ioctl = tun_ioctl;
	ifp->if_output = tun_output;
	ifp->if_start = tunstart;
	ifp->if_hardmtu = TUNMRU;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	IFQ_SET_READY(&ifp->if_snd);

	ifmedia_init(&tp->tun_media, 0, tun_media_change, tun_media_status);
	ifmedia_add(&tp->tun_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&tp->tun_media, IFM_ETHER | IFM_AUTO);

	if ((flags & TUN_LAYER2) == 0) {
		tp->tun_flags &= ~TUN_LAYER2;
		ifp->if_mtu = ETHERMTU;
		ifp->if_flags = IFF_POINTOPOINT;
		ifp->if_type = IFT_TUNNEL;
		ifp->if_hdrlen = sizeof(u_int32_t);

		if_attach(ifp);
		if_alloc_sadl(ifp);
#if NBPFILTER > 0
		bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(u_int32_t));
#endif
	} else {
		tp->tun_flags |= TUN_LAYER2;
		ifp->if_flags =
		    (IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST|IFF_LINK0);
		ifp->if_capabilities = IFCAP_VLAN_MTU;

		if_attach(ifp);
		ether_ifattach(ifp);
	}
	/* force output function to our function */
	ifp->if_output = tun_output;

	s = splnet();
	LIST_INSERT_HEAD(&tun_softc_list, tp, tun_list);
	splx(s);
#ifdef PIPEX
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
	pipex_iface_stop(&tp->pipex_iface);
#endif
	tun_wakeup(tp);

	s = splhigh();
	klist_invalidate(&tp->tun_rsel.si_note);
	klist_invalidate(&tp->tun_wsel.si_note);
	splx(s);

	s = splnet();
	LIST_REMOVE(tp, tun_list);
	splx(s);

	if (tp->tun_flags & TUN_LAYER2)
		ether_ifdetach(ifp);

	if_detach(ifp);

	free(tp, M_DEVBUF);
	return (0);
}

struct tun_softc *
tun_lookup(int unit)
{
	struct tun_softc *tp;

	LIST_FOREACH(tp, &tun_softc_list, tun_list)
		if (tp->tun_unit == unit)
			return (tp);
	return (NULL);
}

int
tun_switch(struct tun_softc *tp, int flags)
{
	struct ifnet		*ifp = &tp->tun_if;
	int			 unit, open, r, s;
	struct ifg_list		*ifgl;
	u_int			ifgr_len;
	char			*ifgrpnames, *p;

	if ((tp->tun_flags & TUN_LAYER2) == (flags & TUN_LAYER2))
		return (0);

	/* tp will be removed so store unit number */
	unit = tp->tun_unit;
	open = tp->tun_flags & (TUN_OPEN|TUN_NBIO|TUN_ASYNC);
	TUNDEBUG(("%s: switching to layer %d\n", ifp->if_xname,
		    flags & TUN_LAYER2 ? 2 : 3));

	/* remember joined groups */
	ifgr_len = 0;
	ifgrpnames = NULL;
	TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
		ifgr_len += IFNAMSIZ;
	if (ifgr_len)
		ifgrpnames = malloc(ifgr_len + 1, M_TEMP, M_NOWAIT|M_ZERO);
	if (ifgrpnames) {
		p = ifgrpnames;
		TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next) {
			strlcpy(p, ifgl->ifgl_group->ifg_group, IFNAMSIZ);
			p += IFNAMSIZ;
		}
	}

	/* remove old device and ... */
	tun_clone_destroy(ifp);
	/* attach new interface */
	r = tun_create(&tun_cloner, unit, flags);

	if (r == 0) {
		if ((tp = tun_lookup(unit)) == NULL) {
			/* this should never fail */
			r = ENXIO;
			goto abort;
		}

		/* rejoin groups */
		ifp = &tp->tun_if;
		for (p = ifgrpnames; p && *p; p += IFNAMSIZ)
			if_addgroup(ifp, p);
	}
	if (open && r == 0) {
		/* already opened before ifconfig tunX link0 */
		s = splnet();
		tp->tun_flags |= open;
		tun_link_state(tp);
		splx(s);
		TUNDEBUG(("%s: already open\n", tp->tun_if.if_xname));
	}
 abort:
	if (ifgrpnames)
		free(ifgrpnames, M_TEMP);
	return (r);
}

/*
 * tunnel open - must be superuser & the device must be
 * configured in
 */
int
tunopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tun_softc	*tp;
	struct ifnet		*ifp;
	int			 error, s;

	if ((error = suser(p, 0)) != 0)
		return (error);

	if ((tp = tun_lookup(minor(dev))) == NULL) {	/* create on demand */
		char	xname[IFNAMSIZ];

		snprintf(xname, sizeof(xname), "%s%d", "tun", minor(dev));
		if ((error = if_clone_create(xname)) != 0)
			return (error);

		if ((tp = tun_lookup(minor(dev))) == NULL)
			return (ENXIO);
		tp->tun_flags &= ~TUN_STAYUP;
	}

	if (tp->tun_flags & TUN_OPEN)
		return (EBUSY);

	ifp = &tp->tun_if;
	tp->tun_flags |= TUN_OPEN;

	/* automatically UP the interface on open */
	s = splnet();
	ifp->if_flags |= IFF_RUNNING;
	tun_link_state(tp);
	if_up(ifp);
	splx(s);

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
	int			 s;
	struct tun_softc	*tp;
	struct ifnet		*ifp;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

	ifp = &tp->tun_if;
	tp->tun_flags &= ~(TUN_OPEN|TUN_NBIO|TUN_ASYNC);

	/*
	 * junk all pending output
	 */
	s = splnet();
	ifp->if_flags &= ~IFF_RUNNING;
	tun_link_state(tp);
	IFQ_PURGE(&ifp->if_snd);
	splx(s);

	TUNDEBUG(("%s: closed\n", ifp->if_xname));

	if (!(tp->tun_flags & TUN_STAYUP))
		return (if_clone_destroy(ifp->if_xname));
	else {
		tp->tun_pgid = 0;
		selwakeup(&tp->tun_rsel);
	}

	return (0);
}

int
tuninit(struct tun_softc *tp)
{
	struct ifnet	*ifp = &tp->tun_if;
	struct ifaddr	*ifa;

	TUNDEBUG(("%s: tuninit\n", ifp->if_xname));

	ifp->if_flags |= IFF_UP | IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE; /* we are never active */

	tp->tun_flags &= ~(TUN_IASET|TUN_DSTADDR|TUN_BRDADDR);
	TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
#ifdef INET
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
#endif
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			struct sockaddr_in6 *sin;

			sin = (struct sockaddr_in6 *)ifa->ifa_addr;
			if (!IN6_IS_ADDR_UNSPECIFIED(&sin->sin6_addr))
				tp->tun_flags |= TUN_IASET;

			if (ifp->if_flags & IFF_POINTOPOINT) {
				sin = (struct sockaddr_in6 *)ifa->ifa_dstaddr;
				if (sin &&
				    !IN6_IS_ADDR_UNSPECIFIED(&sin->sin6_addr))
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
	int			 error = 0, s;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		tuninit(tp);
		TUNDEBUG(("%s: address set\n", ifp->if_xname));
		if (tp->tun_flags & TUN_LAYER2)
			switch (((struct ifaddr *)data)->ifa_addr->sa_family) {
#ifdef INET
			case AF_INET:
				arp_ifinit(&tp->arpcom, (struct ifaddr *)data);
				break;
#endif
			default:
				break;
			}
		break;
	case SIOCSIFDSTADDR:
		tuninit(tp);
		TUNDEBUG(("%s: destination address set\n", ifp->if_xname));
		break;
	case SIOCSIFBRDADDR:
		tuninit(tp);
		TUNDEBUG(("%s: broadcast address set\n", ifp->if_xname));
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > TUNMRU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI: {
		if (ifr == 0) {
			error = EAFNOSUPPORT;	   /* XXX */
			break;
		}

		if (tp->tun_flags & TUN_LAYER2) {
			error = (cmd == SIOCADDMULTI) ?
			    ether_addmulti(ifr, &tp->arpcom) :
			    ether_delmulti(ifr, &tp->arpcom);
			if (error == ENETRESET) {
				/*
				 * Multicast list has changed; set the hardware
				 * filter accordingly. The good thing is we do 
				 * not have a hardware filter (:
				 */
				error = 0;
			}
			break;
		}

		switch (ifr->ifr_addr.sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;
	}

	case SIOCSIFFLAGS:
		error = tun_switch(tp,
		    ifp->if_flags & IFF_LINK0 ? TUN_LAYER2 : 0);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &tp->tun_media, cmd);
		break;
	default:
		if (tp->tun_flags & TUN_LAYER2)
			error = ether_ioctl(ifp, &tp->arpcom, cmd, data);
		else
			error = ENOTTY;
	}

	splx(s);
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
	int			 s, len, error;
	u_int32_t		*af;
#ifdef PIPEX
	struct pipex_session *session;
#endif /* PIPEX */

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
		/* call ether_output and that will call tunstart at the end */
		return (ether_output(ifp, m0, dst, rt));

	M_PREPEND(m0, sizeof(*af), M_DONTWAIT);
	if (m0 == NULL)
		return (ENOBUFS);
	af = mtod(m0, u_int32_t *);
	*af = htonl(dst->sa_family);

	s = splnet();

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif
#ifdef PIPEX
	if ((session = pipex_ip_lookup_session(m0, &tp->pipex_iface)) != NULL) {
		pipex_ip_output(m0, session);
		splx(s);
		return (0);
	}
#endif /* PIPEX */

	len = m0->m_pkthdr.len;
	IFQ_ENQUEUE(&ifp->if_snd, m0, NULL, error);
	if (error) {
		splx(s);
		ifp->if_collisions++;
		return (error);
	}
	splx(s);
	ifp->if_opackets++;
	ifp->if_obytes += len;

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
	int			 s;
	struct tun_softc	*tp;
	struct tuninfo		*tunp;
	struct mbuf		*m;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

	s = splnet();
	switch (cmd) {
	case TUNSIFINFO:
		tunp = (struct tuninfo *)data;
		if (tunp->mtu < ETHERMIN || tunp->mtu > TUNMRU) {
			splx(s);
			return (EINVAL);
		}
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
			splx(s);
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
		IFQ_POLL(&tp->tun_if.if_snd, m);
		if (m != NULL)
			*(int *)data = m->m_pkthdr.len;
		else
			*(int *)data = 0;
		break;
	case TIOCSPGRP:
		tp->tun_pgid = *(int *)data;
		tp->tun_siguid = p->p_cred->p_ruid;
		tp->tun_sigeuid = p->p_ucred->cr_uid;
		break;
	case TIOCGPGRP:
		*(int *)data = tp->tun_pgid;
		break;
	case OSIOCGIFADDR:
	case SIOCGIFADDR:
		if (!(tp->tun_flags & TUN_LAYER2)) {
			splx(s);
			return (EINVAL);
		}
		bcopy(tp->arpcom.ac_enaddr, data,
		    sizeof(tp->arpcom.ac_enaddr));
		break;

	case SIOCSIFADDR:
		if (!(tp->tun_flags & TUN_LAYER2)) {
			splx(s);
			return (EINVAL);
		}
		bcopy(data, tp->arpcom.ac_enaddr,
		    sizeof(tp->arpcom.ac_enaddr));
		break;
	default:
#ifdef PIPEX
	    {
		int ret;
		ret = pipex_ioctl(&tp->pipex_iface, cmd, data);
		splx(s);
		return ret;
	    }
#else
		splx(s);
		return (ENOTTY);
#endif
	}
	splx(s);
	return (0);
}

/*
 * The cdevsw read interface - reads a packet at a time, or at
 * least as much of a packet as can be read.
 */
int
tunread(dev_t dev, struct uio *uio, int ioflag)
{
	struct tun_softc	*tp;
	struct ifnet		*ifp;
	struct mbuf		*m, *m0;
	int			 error = 0, len, s;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

	ifp = &tp->tun_if;
	TUNDEBUG(("%s: read\n", ifp->if_xname));
	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG(("%s: not ready %#x\n", ifp->if_xname, tp->tun_flags));
		return (EHOSTDOWN);
	}

	tp->tun_flags &= ~TUN_RWAIT;

	s = splnet();
	do {
		while ((tp->tun_flags & TUN_READY) != TUN_READY)
			if ((error = tsleep((caddr_t)tp,
			    (PZERO + 1)|PCATCH, "tunread", 0)) != 0) {
				splx(s);
				return (error);
			}
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL) {
			if (tp->tun_flags & TUN_NBIO && ioflag & IO_NDELAY) {
				splx(s);
				return (EWOULDBLOCK);
			}
			tp->tun_flags |= TUN_RWAIT;
			if ((error = tsleep((caddr_t)tp,
			    (PZERO + 1)|PCATCH, "tunread", 0)) != 0) {
				splx(s);
				return (error);
			}
		}
	} while (m0 == NULL);
	splx(s);

	while (m0 != NULL && uio->uio_resid > 0 && error == 0) {
		len = min(uio->uio_resid, m0->m_len);
		if (len != 0)
			error = uiomove(mtod(m0, caddr_t), len, uio);
		MFREE(m0, m);
		m0 = m;
	}

	if (m0 != NULL) {
		TUNDEBUG(("Dropping mbuf\n"));
		m_freem(m0);
	}
	if (error)
		ifp->if_ierrors++;

	return (error);
}

/*
 * the cdevsw write interface - an atomic write is a packet - or else!
 */
int
tunwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct tun_softc	*tp;
	struct ifnet		*ifp;
	struct ifqueue		*ifq;
	u_int32_t		*th;
	struct mbuf		*top, **mp, *m;
	int			 isr;
	int			 error=0, s, tlen, mlen;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

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
		m->m_len = min(mlen, uio->uio_resid);
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
		if (top != NULL)
			m_freem(top);
		ifp->if_ierrors++;
		return (error);
	}

	top->m_pkthdr.len = tlen;
	top->m_pkthdr.rcvif = ifp;

#if NBPFILTER > 0
	if (ifp->if_bpf) {
		s = splnet();
		bpf_mtap(ifp->if_bpf, top, BPF_DIRECTION_IN);
		splx(s);
	}
#endif

	if (tp->tun_flags & TUN_LAYER2) {
		/* quirk to not add randomness from a virtual device */
		atomic_setbits_int(&netisr, (1 << NETISR_RND_DONE));

		s = splnet();
		ether_input_mbuf(ifp, top);
		splx(s);

		ifp->if_ipackets++; /* ibytes are counted in ether_input */

		return (0);
	}

	th = mtod(top, u_int32_t *);
	/* strip the tunnel header */
	top->m_data += sizeof(*th);
	top->m_len  -= sizeof(*th);
	top->m_pkthdr.len -= sizeof(*th);
	top->m_pkthdr.rdomain = ifp->if_rdomain;

	switch (ntohl(*th)) {
#ifdef INET
	case AF_INET:
		ifq = &ipintrq;
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		ifq = &ip6intrq;
		isr = NETISR_IPV6;
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK:
		ifq = &atintrq2;
		isr = NETISR_ATALK;
		break;
#endif
	default:
		m_freem(top);
		return (EAFNOSUPPORT);
	}

	s = splnet();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		splx(s);
		ifp->if_collisions++;
		m_freem(top);
		if (!ifq->ifq_congestion)
			if_congestion(ifq);
		return (ENOBUFS);
	}
	IF_ENQUEUE(ifq, top);
	schednetisr(isr);
	ifp->if_ipackets++;
	ifp->if_ibytes += top->m_pkthdr.len;
	splx(s);
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
	int			 revents, s;
	struct tun_softc	*tp;
	struct ifnet		*ifp;
	struct mbuf		*m;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (POLLERR);

	ifp = &tp->tun_if;
	revents = 0;
	s = splnet();
	TUNDEBUG(("%s: tunpoll\n", ifp->if_xname));

	if (events & (POLLIN | POLLRDNORM)) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m != NULL) {
			TUNDEBUG(("%s: tunselect q=%d\n", ifp->if_xname,
			    ifp->if_snd.ifq_len));
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			TUNDEBUG(("%s: tunpoll waiting\n", ifp->if_xname));
			selrecord(p, &tp->tun_rsel);
		}
	}
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);
	splx(s);
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
	int			 s;
	struct klist		*klist;
	struct tun_softc	*tp;
	struct ifnet		*ifp;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

	ifp = &tp->tun_if;

	s = splnet();
	TUNDEBUG(("%s: tunkqfilter\n", ifp->if_xname));
	splx(s);

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
			return (EPERM);	/* 1 */
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
	int			 s;
	struct tun_softc	*tp;
	struct ifnet		*ifp;
	struct mbuf		*m;

	if (kn->kn_status & KN_DETACHED) {
		kn->kn_data = 0;
		return (1);
	}

	tp = (struct tun_softc *)kn->kn_hook;
	ifp = &tp->tun_if;

	s = splnet();
	IFQ_POLL(&ifp->if_snd, m);
	if (m != NULL) {
		splx(s);
		kn->kn_data = ifp->if_snd.ifq_len;

		TUNDEBUG(("%s: tunkqread q=%d\n", ifp->if_xname,
		    ifp->if_snd.ifq_len));
		return (1);
	}
	splx(s);
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

/*
 * Start packet transmission on the interface.
 * when the interface queue is rate-limited by ALTQ or TBR,
 * if_start is needed to drain packets from the queue in order
 * to notify readers when outgoing packets become ready.
 * In layer 2 mode this function is called from ether_output.
 */
void
tunstart(struct ifnet *ifp)
{
	struct tun_softc	*tp = ifp->if_softc;
	struct mbuf		*m;

	splassert(IPL_NET);

	if (!(tp->tun_flags & TUN_LAYER2) &&
	    !ALTQ_IS_ENABLED(&ifp->if_snd) &&
	    !TBR_IS_ENABLED(&ifp->if_snd))
		return;

	IFQ_POLL(&ifp->if_snd, m);
	if (m != NULL) {
		if (tp->tun_flags & TUN_LAYER2) {
#if NBPFILTER > 0
			if (ifp->if_bpf)
				bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
			ifp->if_opackets++;
		}
		tun_wakeup(tp);
	}
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

int
tun_media_change(struct ifnet *ifp)
{
	/* Ignore */
	return (0);
}

void
tun_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct tun_softc *tp = ifp->if_softc;

	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID;

	tun_link_state(tp);

	if (LINK_STATE_IS_UP(ifp->if_link_state) &&
	    ifp->if_flags & IFF_UP)
		imr->ifm_status |= IFM_ACTIVE;
}
