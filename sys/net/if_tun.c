/*	$OpenBSD: if_tun.c,v 1.58 2004/03/02 23:09:29 markus Exp $	*/
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
#include <sys/select.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/device.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/poll.h>
#include <sys/conf.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
/* #include <netinet/if_ether.h> */
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#endif

#ifdef ISO
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net/if_tun.h>

struct tun_softc {
	struct	ifnet tun_if;		/* the interface */
	u_short	tun_flags;		/* misc flags */
	pid_t	tun_pgid;		/* the process group - if any */
	uid_t	tun_siguid;		/* uid for process that set tun_pgid */
	uid_t	tun_sigeuid;		/* euid for process that set tun_pgid */
	struct	selinfo	tun_rsel;	/* read select */
	struct	selinfo	tun_wsel;	/* write select (not used) */
	int	tun_unit;
	LIST_ENTRY(tun_softc) tun_list;	/* all tunnel interfaces */
};

#ifdef	TUN_DEBUG
int	tundebug = TUN_DEBUG;
#define TUNDEBUG(a)	(tundebug? printf a : 0)
#else
#define TUNDEBUG(a)	/* (tundebug? printf a : 0) */
#endif

extern int ifqmaxlen;

void	tunattach(int);
int	tunopen(dev_t, int, int, struct proc *);
int	tunclose(dev_t, int, int, struct proc *);
int	tun_ioctl(struct ifnet *, u_long, caddr_t);
int	tun_output(struct ifnet *, struct mbuf *, struct sockaddr *,
		        struct rtentry *rt);
int	tunioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	tunread(dev_t, struct uio *, int);
int	tunwrite(dev_t, struct uio *, int);
int	tunpoll(dev_t, int, struct proc *);
int	tunkqfilter(dev_t, struct knote *);
int	tun_clone_create(struct if_clone *, int);
int	tun_clone_destroy(struct ifnet *);
struct	tun_softc *tun_lookup(int);
void	tun_wakeup(struct tun_softc *);

static int tuninit(struct tun_softc *);
#ifdef ALTQ
static void tunstart(struct ifnet *);
#endif
int	filt_tunread(struct knote *, long);
int	filt_tunwrite(struct knote *, long);
void	filt_tunrdetach(struct knote *);
void	filt_tunwdetach(struct knote *);

struct filterops tunread_filtops =
	{ 1, NULL, filt_tunrdetach, filt_tunread};

struct filterops tunwrite_filtops =
	{ 1, NULL, filt_tunwdetach, filt_tunwrite};

LIST_HEAD(, tun_softc) tun_softc_list;

struct if_clone tun_cloner =
    IF_CLONE_INITIALIZER("tun", tun_clone_create, tun_clone_destroy);

void
tunattach(n)
	int n;
{
	LIST_INIT(&tun_softc_list);
	if_clone_attach(&tun_cloner);
}

int
tun_clone_create(ifc, unit)
	struct if_clone *ifc;
	int unit;
{
	struct tun_softc *tp;
	struct ifnet *ifp;
	int s;

	tp = malloc(sizeof(*tp), M_DEVBUF, M_NOWAIT);
	if (!tp)
		return (ENOMEM);
	bzero(tp, sizeof(*tp));

	tp->tun_unit = unit;
	tp->tun_flags = TUN_INITED;

	ifp = &tp->tun_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "%s%d", ifc->ifc_name, 
	    unit);
	ifp->if_softc = tp;
	ifp->if_mtu = TUNMTU;
	ifp->if_ioctl = tun_ioctl;
	ifp->if_output = tun_output;
#ifdef ALTQ
	ifp->if_start = tunstart;
#endif
	ifp->if_flags = IFF_POINTOPOINT;
	ifp->if_type  = IFT_PROPVIRTUAL;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_hdrlen = sizeof(u_int32_t);
	ifp->if_collisions = 0;
	ifp->if_ierrors = 0;
	ifp->if_oerrors = 0;
	ifp->if_ipackets = 0;
	ifp->if_opackets = 0;
	ifp->if_ibytes = 0;
	ifp->if_obytes = 0;
	if_attach(ifp);
	if_alloc_sadl(ifp);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(u_int32_t));
#endif
	s = splimp();
	LIST_INSERT_HEAD(&tun_softc_list, tp, tun_list);
	splx(s);

	return (0);
}

int
tun_clone_destroy(ifp)
	struct ifnet *ifp;
{
	struct tun_softc *tp = ifp->if_softc;
	int s;

	s = splhigh();
	klist_invalidate(&tp->tun_rsel.si_note);
	klist_invalidate(&tp->tun_wsel.si_note);
	splx(s);

	s = splimp();
	LIST_REMOVE(tp, tun_list);
	splx(s);

	tun_wakeup(tp);
#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);

	free(tp, M_DEVBUF);
	return (0);
}

struct tun_softc *
tun_lookup(unit)
	int unit;
{
	struct tun_softc *tp;

	LIST_FOREACH(tp, &tun_softc_list, tun_list)
		if (tp->tun_unit == unit)
			return (tp);
	return (NULL);
}

/*
 * tunnel open - must be superuser & the device must be
 * configured in
 */
int
tunopen(dev, flag, mode, p)
	dev_t	dev;
	int	flag, mode;
	struct proc *p;
{
	struct tun_softc *tp;
	struct ifnet	*ifp;
	int error;

	if ((error = suser(p, 0)) != 0)
		return (error);

	if ((tp = tun_lookup(minor(dev))) == NULL) {
		/* create on demand */
                (void) tun_clone_create(&tun_cloner, minor(dev));

		if ((tp = tun_lookup(minor(dev))) == NULL)
			return (ENXIO);
	}

	if (tp->tun_flags & TUN_OPEN)
		return EBUSY;

	ifp = &tp->tun_if;
	tp->tun_flags |= TUN_OPEN;
	TUNDEBUG(("%s: open\n", ifp->if_xname));
	return (0);
}

/*
 * tunclose - close the device; if closing the real device, flush pending
 *  output and (unless set STAYUP) bring down the interface.
 */
int
tunclose(dev, flag, mode, p)
	dev_t	dev;
	int	flag;
	int	mode;
	struct proc *p;
{
	int	s;
	struct tun_softc *tp;
	struct ifnet	*ifp;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

	ifp = &tp->tun_if;
	tp->tun_flags &= ~TUN_OPEN;

	/*
	 * junk all pending output
	 */
	s = splimp();
	IFQ_PURGE(&ifp->if_snd);
	splx(s);

	if ((ifp->if_flags & IFF_UP) && !(tp->tun_flags & TUN_STAYUP)) {
		s = splimp();
		if_down(ifp);
		if (ifp->if_flags & IFF_RUNNING) {
			/* find internet addresses and delete routes */
			struct ifaddr *ifa;
			TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
#ifdef INET
				if (ifa->ifa_addr->sa_family == AF_INET) {
					rtinit(ifa, (int)RTM_DELETE,
					       (tp->tun_flags & TUN_DSTADDR)?
							RTF_HOST : 0);
				}
#endif
			}
		}
		splx(s);
	}
	tp->tun_pgid = 0;
	selwakeup(&tp->tun_rsel);
	KNOTE(&tp->tun_rsel.si_note, 0);
		
	TUNDEBUG(("%s: closed\n", ifp->if_xname));
	return (0);
}

static int
tuninit(tp)
	struct tun_softc *tp;
{
	struct ifnet	*ifp = &tp->tun_if;
	struct ifaddr *ifa;

	TUNDEBUG(("%s: tuninit\n", ifp->if_xname));

	ifp->if_flags |= IFF_UP | IFF_RUNNING;

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

	return 0;
}

/*
 * Process an ioctl request.
 */
int
tun_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long	cmd;
	caddr_t	data;
{
	int	error = 0, s;

	s = splimp();
	switch(cmd) {
	case SIOCSIFADDR:
		tuninit((struct tun_softc *)(ifp->if_softc));
		TUNDEBUG(("%s: address set\n", ifp->if_xname));
		break;
	case SIOCSIFDSTADDR:
		tuninit((struct tun_softc *)(ifp->if_softc));
		TUNDEBUG(("%s: destination address set\n", ifp->if_xname));
		break;
	case SIOCSIFBRDADDR:
		tuninit((struct tun_softc *)(ifp->if_softc));
		TUNDEBUG(("%s: broadcast address set\n", ifp->if_xname));
		break;
	case SIOCSIFMTU:
		ifp->if_mtu = ((struct ifreq *)data)->ifr_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI: {
		struct ifreq *ifr = (struct ifreq *)data;
		if (ifr == 0) {
			error = EAFNOSUPPORT;	   /* XXX */
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
		break;
	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}

/*
 * tun_output - queue packets from higher level ready to put out.
 */
int
tun_output(ifp, m0, dst, rt)
	struct ifnet   *ifp;
	struct mbuf    *m0;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	struct tun_softc *tp = ifp->if_softc;
	int		s, len, error;
	u_int32_t	*af;

	TUNDEBUG(("%s: tun_output\n", ifp->if_xname));

	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG(("%s: not ready 0%o\n", ifp->if_xname,
			  tp->tun_flags));
		m_freem (m0);
		return EHOSTDOWN;
	}

	M_PREPEND(m0, sizeof(*af), M_DONTWAIT);
	af = mtod(m0, u_int32_t *);
	*af = htonl(dst->sa_family);

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0);
#endif

	len = m0->m_pkthdr.len + sizeof(*af);
	s = splimp();
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
	return 0;
}

void
tun_wakeup(tp)
	struct tun_softc *tp;
{
	if (tp->tun_flags & TUN_RWAIT) {
		tp->tun_flags &= ~TUN_RWAIT;
		wakeup((caddr_t)tp);
	}
	if (tp->tun_flags & TUN_ASYNC && tp->tun_pgid)
		csignal(tp->tun_pgid, SIGIO,
		    tp->tun_siguid, tp->tun_sigeuid);
	selwakeup(&tp->tun_rsel);
	KNOTE(&tp->tun_rsel.si_note, 0);
}

/*
 * the cdevsw interface is now pretty minimal.
 */
int
tunioctl(dev, cmd, data, flag, p)
	dev_t		dev;
	u_long		cmd;
	caddr_t		data;
	int		flag;
	struct proc	*p;
{
	int		s;
	struct tun_softc *tp;
	struct tuninfo *tunp;
	struct mbuf *m;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

	s = splimp();
	switch (cmd) {
	case TUNSIFINFO:
		tunp = (struct tuninfo *)data;
		tp->tun_if.if_mtu = tunp->mtu;
		tp->tun_if.if_type = tunp->type;
		tp->tun_if.if_flags = tunp->flags;
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
                        if (tp->tun_if.if_flags & IFF_UP) {
                                splx(s);
                                return (EBUSY);
                        }
                        tp->tun_if.if_flags &=
                                ~(IFF_BROADCAST|IFF_POINTOPOINT|IFF_MULTICAST);
                        tp->tun_if.if_flags |= *(int *)data;
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
	default:
		splx(s);
		return (ENOTTY);
	}
	splx(s);
	return (0);
}

/*
 * The cdevsw read interface - reads a packet at a time, or at
 * least as much of a packet as can be read.
 */
int
tunread(dev, uio, ioflag)
	dev_t		dev;
	struct uio	*uio;
	int		ioflag;
{
	struct tun_softc *tp;
	struct ifnet	*ifp;
	struct mbuf	*m, *m0;
	int		error = 0, len, s;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

	ifp = &tp->tun_if;
	TUNDEBUG(("%s: read\n", ifp->if_xname));
	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG(("%s: not ready 0%o\n", ifp->if_xname,
			  tp->tun_flags));
		return EHOSTDOWN;
	}

	tp->tun_flags &= ~TUN_RWAIT;

	s = splimp();
	do {
		while ((tp->tun_flags & TUN_READY) != TUN_READY)
			if ((error = tsleep((caddr_t)tp,
			    (PZERO+1)|PCATCH, "tunread", 0)) != 0) {
				splx(s);
				return (error);
			}
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == 0) {
			if (tp->tun_flags & TUN_NBIO && ioflag & IO_NDELAY) {
				splx(s);
				return EWOULDBLOCK;
			}
			tp->tun_flags |= TUN_RWAIT;
			if ((error = tsleep((caddr_t)tp,
			    (PZERO + 1)|PCATCH, "tunread", 0)) != 0) {
				splx(s);
				return (error);
			}
		}
	} while (m0 == 0);
	splx(s);

	while (m0 && uio->uio_resid > 0 && error == 0) {
		len = min(uio->uio_resid, m0->m_len);
		if (len != 0)
			error = uiomove(mtod(m0, caddr_t), len, uio);
		MFREE(m0, m);
		m0 = m;
	}

	if (m0) {
		TUNDEBUG(("Dropping mbuf\n"));
		m_freem(m0);
	}
	if (error)
		ifp->if_ierrors++;

	return error;
}

/*
 * the cdevsw write interface - an atomic write is a packet - or else!
 */
int
tunwrite(dev, uio, ioflag)
	dev_t		dev;
	struct uio	*uio;
	int		ioflag;
{
	struct tun_softc *tp;
	struct ifnet	*ifp;
	struct ifqueue	*ifq;
	u_int32_t	*th;
	struct mbuf	*top, **mp, *m;
	int		isr;
	int		error=0, s, tlen, mlen;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

	ifp = &tp->tun_if;
	TUNDEBUG(("%s: tunwrite\n", ifp->if_xname));

	if (uio->uio_resid == 0 || uio->uio_resid > TUNMRU) {
		TUNDEBUG(("%s: len=%d!\n", ifp->if_xname, uio->uio_resid));
		return EMSGSIZE;
	}
	tlen = uio->uio_resid;

	/* get a header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;
	mlen = MHLEN;

	top = 0;
	mp = &top;
	while (error == 0 && uio->uio_resid > 0) {
		m->m_len = min(mlen, uio->uio_resid);
		error = uiomove(mtod (m, caddr_t), m->m_len, uio);
		*mp = m;
		mp = &m->m_next;
		if (error == 0 && uio->uio_resid > 0) {
			MGET (m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				error = ENOBUFS;
				break;
			}
			mlen = MLEN;
		}
	}
	if (error) {
		if (top)
			m_freem (top);
		ifp->if_ierrors++;
		return error;
	}

	top->m_pkthdr.len = tlen;
	top->m_pkthdr.rcvif = ifp;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, top);
#endif

	th = mtod(top, u_int32_t *);
	/* strip the tunnel header */
	top->m_data += sizeof(*th);
	top->m_len  -= sizeof(*th);
	top->m_pkthdr.len -= sizeof(*th);

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
#ifdef NS
	case AF_NS:
		ifq = &nsintrq;
		isr = NETISR_NS;
		break;
#endif
#ifdef IPX
	case AF_IPX:
		ifq = &ipxintrq;
		isr = NETISR_IPX;
		break;
#endif
#ifdef NETATALK
	case AF_APPLETALK:
		ifq = &atintrq2;
		isr = NETISR_ATALK;
		break;
#endif
#ifdef ISO
	case AF_ISO:
		ifq = &clnlintrq;
		isr = NETISR_ISO;
		break;
#endif
	default:
		m_freem(top);
		return EAFNOSUPPORT;
	}

	s = splimp();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		splx(s);
		ifp->if_collisions++;
		m_freem(top);
		return ENOBUFS;
	}
	IF_ENQUEUE(ifq, top);
	schednetisr(isr);
	ifp->if_ipackets++;
	ifp->if_ibytes += top->m_pkthdr.len;
	splx(s);
	return error;
}

/*
 * tunselect - the select interface, this is only useful on reads
 * really. The write detect always returns true, write never blocks
 * anyway, it either accepts the packet or drops it.
 */
int
tunpoll(dev, events, p)
	dev_t		dev;
	int		events;
	struct proc	*p;
{
	int		revents, s;
	struct tun_softc *tp;
	struct ifnet	*ifp;
	struct mbuf	*m;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

	ifp = &tp->tun_if;
	revents = 0;
	s = splimp();
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
tunkqfilter(dev_t dev,struct knote *kn)
{
	int s;
	struct klist *klist;
	struct tun_softc *tp;
	struct ifnet *ifp;

	if ((tp = tun_lookup(minor(dev))) == NULL)
		return (ENXIO);

	ifp = &tp->tun_if;

	s = splimp();
	TUNDEBUG(("%s: tunselect\n", ifp->if_xname));
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
			return EPERM;	/* 1 */
	}

	kn->kn_hook = (caddr_t)tp;

	s = splhigh();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return 0;
}

void
filt_tunrdetach(struct knote *kn)
{
	int s;
	struct tun_softc *tp = (struct tun_softc *)kn->kn_hook;

	s = splhigh();
	if (!(kn->kn_status & KN_DETACHED))
		SLIST_REMOVE(&tp->tun_rsel.si_note, kn, knote, kn_selnext);
	splx(s);
}

int
filt_tunread(struct knote *kn, long hint)
{
	int s;
	struct tun_softc *tp;
	struct ifnet *ifp;
	struct mbuf *m;

	if (kn->kn_status & KN_DETACHED) {
		kn->kn_data = 0;
		return 1;
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
		return 1;
	}
	splx(s);
	TUNDEBUG(("%s: tunkqread waiting\n", ifp->if_xname));
	return 0;
}

void
filt_tunwdetach(struct knote *kn)
{
	int s;
	struct tun_softc *tp = (struct tun_softc *)kn->kn_hook;

	s = splhigh();
	if (!(kn->kn_status & KN_DETACHED))
		SLIST_REMOVE(&tp->tun_wsel.si_note, kn, knote, kn_selnext);
	splx(s);
}

int
filt_tunwrite(struct knote *kn, long hint)
{
	struct tun_softc *tp;
	struct ifnet *ifp;

	if (kn->kn_status & KN_DETACHED) {
		kn->kn_data = 0;
		return 1;
	}

	tp = (struct tun_softc *)kn->kn_hook;
	ifp = &tp->tun_if;

	kn->kn_data = ifp->if_mtu;

	return 1;
}

#ifdef ALTQ
/*
 * Start packet transmission on the interface.
 * when the interface queue is rate-limited by ALTQ or TBR,
 * if_start is needed to drain packets from the queue in order
 * to notify readers when outgoing packets become ready.
 */
static void
tunstart(ifp)
	struct ifnet *ifp;
{
	struct tun_softc *tp = ifp->if_softc;
	struct mbuf *m;

	if (!ALTQ_IS_ENABLED(&ifp->if_snd) && !TBR_IS_ENABLED(&ifp->if_snd))
		return;

	IFQ_POLL(&ifp->if_snd, m);
	if (m != NULL)
		tun_wakeup(tp);
}
#endif
