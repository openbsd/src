/*	$OpenBSD: if_tun.c,v 1.3 1996/02/20 14:34:00 mickey Exp $	*/

/*
 * Copyright (c) 1988, Julian Onions <jpo@cs.nott.ac.uk>
 * Nottingham University 1987.
 *
 * This source may be freely distributed, however I would be interested
 * in any changes that are made.
 *
 * This driver takes packets off the IP i/f and hands them up to a
 * user process to have it's wicked way with. This driver has it's
 * roots in a similar driver written by Phil Cockcroft (formerly) at
 * UCL. This driver is based much more on read/write/select mode of
 * operation though.
 */

#include "tun.h"
#if NTUN > 0

/* #define	TUN_DEBUG	9 /* */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/vnode.h>

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

#ifdef ISO
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net/if_tun.h>

#ifdef	TUN_DEBUG
#define TUNDEBUG	if (tundebug) printf
int	tundebug = TUN_DEBUG;
#endif

struct tun_softc tunctl[NTUN];

extern int ifqmaxlen;

int		tunopen		__P((dev_t, int, int, struct proc *));
int		tunclose	__P((dev_t, int, int, struct proc *));
int		tunoutput	__P((struct ifnet *, struct mbuf *, struct sockaddr *,
					struct rtentry *rt));
int		tunread		__P((dev_t, struct uio *, int));
int		tunwrite	__P((dev_t, struct uio *, int));
int		tunioctl	__P((dev_t, u_long, caddr_t, int, struct proc *));
int		tunifioctl	__P((struct ifnet *, u_long, caddr_t));
int		tunselect	__P((dev_t, int, struct proc *));
void	tunattach	__P((int));

static int tuninit __P((int));

void
tunattach(unused)
	int unused;
{
	register int i;
	struct ifnet *ifp;
	struct sockaddr_in *sin;

	for (i = 0; i < NTUN; i++) {
		tunctl[i].tun_flags = TUN_INITED;

		ifp = &tunctl[i].tun_if;
		ifp->if_unit = i;
		ifp->if_name = "tun";
		ifp->if_mtu = TUNMTU;
		ifp->if_ioctl = tunifioctl;
		ifp->if_output = tunoutput;
		ifp->if_flags = IFF_POINTOPOINT;
		ifp->if_type  = IFT_PROPVIRTUAL;
		ifp->if_snd.ifq_maxlen = ifqmaxlen;
		ifp->if_hdrlen = sizeof(struct tunnel_header);
		ifp->if_addrlen = 0;
		ifp->if_collisions = 0;
		ifp->if_ierrors = 0;
		ifp->if_oerrors = 0;
		ifp->if_ipackets = 0;
		ifp->if_opackets = 0;
		if_attach(ifp);
#if NBPFILTER > 0
		bpfattach(&ifp->if_bpf, ifp, DLT_NULL, sizeof(u_int32_t));
#endif
	}
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
	struct ifnet	*ifp;
	struct tun_softc *tp;
	register int	unit, error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);

	if ((unit = minor(dev)) >= NTUN)
		return (ENXIO);
	tp = &tunctl[unit];
	if (tp->tun_flags & TUN_OPEN)
		return ENXIO;
	ifp = &tp->tun_if;
	tp->tun_flags |= TUN_OPEN;
	TUNDEBUG("%s%d: open\n", ifp->if_name, ifp->if_unit);
	return (0);
}

/*
 * tunclose - close the device - mark i/f down & delete
 * routing info
 */
int
tunclose(dev, flag, mode, p)
	dev_t	dev;
	int		flag,mode;
	struct proc *p;
{
	register int	unit = minor(dev), s;
	struct tun_softc *tp = &tunctl[unit];
	struct ifnet	*ifp = &tp->tun_if;
	struct mbuf	*m;

	tp->tun_flags &= ~TUN_OPEN;

	/*
	 * junk all pending output
	 */
	do {
		s = splimp();
		IF_DEQUEUE(&ifp->if_snd, m);
		splx(s);
		if (m)
			m_freem(m);
	} while (m);

	if (ifp->if_flags & IFF_UP) {
		s = splimp();
		if_down(ifp);
		if (ifp->if_flags & IFF_RUNNING) {
			/* find internet addresses and delete routes */
			register struct ifaddr *ifa;
			for (ifa = ifp->if_addrlist.tqh_first; ifa != 0;
				 ifa = ifa->ifa_list.tqe_next) {
				if (ifa->ifa_addr->sa_family == AF_INET) {
					rtinit(ifa, (int)RTM_DELETE,
					    tp->tun_flags & TUN_DSTADDR ? RTF_HOST : 0);
				}
			}
		}
		splx(s);
	}
	tp->tun_pgrp = 0;
	selwakeup(&tp->tun_rsel);
		
	TUNDEBUG ("%s%d: closed\n", ifp->if_name, ifp->if_unit);
	return (0);
}

static int
tuninit(unit)
	int	unit;
{
	struct tun_softc *tp = &tunctl[unit];
	struct ifnet	*ifp = &tp->tun_if;
	register struct ifaddr *ifa;

	TUNDEBUG("%s%d: tuninit\n", ifp->if_name, ifp->if_unit);

	ifp->if_flags |= IFF_UP | IFF_RUNNING;

	for (ifa = ifp->if_addrlist.tqh_first; ifa != 0;
	    ifa = ifa->ifa_list.tqe_next) {
		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct sockaddr_in *sin;

			sin = satosin(ifa->ifa_addr);
			if (sin && sin->sin_addr.s_addr)
				tp->tun_flags |= TUN_IASET;

			sin = satosin(ifa->ifa_dstaddr);
			if (sin && sin->sin_addr.s_addr)
				tp->tun_flags |= TUN_DSTADDR;
		}
	}

	return 0;
}

/*
 * Process an ioctl request.
 */
int
tunifioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long	cmd;
	caddr_t	data;
{
	struct tun_softc *tp = &tunctl[ifp->if_unit];
	int		error = 0, s;

	s = splimp();
	switch(cmd) {
	case SIOCSIFADDR:
		tuninit(ifp->if_unit);
		TUNDEBUG("%s%d: address set\n",
			 ifp->if_name, ifp->if_unit);
		break;
	case SIOCSIFDSTADDR:
		tuninit(ifp->if_unit);
		TUNDEBUG("%s%d: destination address set\n",
			 ifp->if_name, ifp->if_unit);
		break;
	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}

/*
 * tunoutput - queue packets from higher level ready to put out.
 */
int
tunoutput(ifp, m0, dst, rt)
	struct ifnet   *ifp;
	struct mbuf    *m0;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	struct tun_softc *tp = &tunctl[ifp->if_unit];
	struct proc	*p;
	int		s;

	TUNDEBUG ("%s%d: tunoutput\n", ifp->if_name, ifp->if_unit);

	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG ("%s%d: not ready 0%o\n", ifp->if_name,
			  ifp->if_unit, tp->tun_flags);
		m_freem (m0);
		return EHOSTDOWN;
	}
	ifp->if_lastchange = time;

	M_PREPEND( m0, sizeof(struct tunnel_header), M_DONTWAIT );
	{
		struct tunnel_header	*th = mtod( m0, struct tunnel_header * );
		th->tun_af = dst->sa_family;
	}

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0);
#endif

	s = splimp();
	if (IF_QFULL(&ifp->if_snd)) {
		IF_DROP(&ifp->if_snd);
		m_freem(m0);
		splx(s);
		ifp->if_collisions++;
		return (ENOBUFS);
	}
	IF_ENQUEUE(&ifp->if_snd, m0);
	splx(s);

	ifp->if_opackets++;
	ifp->if_obytes += m0->m_pkthdr.len + sizeof(struct tunnel_header);

	if (tp->tun_flags & TUN_RWAIT) {
		tp->tun_flags &= ~TUN_RWAIT;
		wakeup((caddr_t)tp);
	}
	if (tp->tun_flags & TUN_ASYNC && tp->tun_pgrp) {
		if (tp->tun_pgrp > 0)
			gsignal(tp->tun_pgrp, SIGIO);
		else if (p = pfind(-tp->tun_pgrp))
			psignal(p, SIGIO);
	}
	selwakeup(&tp->tun_rsel);
	return 0;
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
	int		unit = minor(dev), s;
	struct tun_softc *tp = &tunctl[unit];
 	struct tuninfo *tunp;

	switch (cmd) {
 	case TUNSIFINFO:
        tunp = (struct tuninfo *)data;
 		tp->tun_if.if_mtu = tunp->mtu;
 		tp->tun_if.if_type = tunp->type;
 		tp->tun_if.if_baudrate = tunp->baudrate;
 		break;
 	case TUNGIFINFO:
 		tunp = (struct tuninfo *)data;
 		tunp->mtu = tp->tun_if.if_mtu;
 		tunp->type = tp->tun_if.if_type;
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
		s = splimp();
		if (tp->tun_if.if_snd.ifq_head)
			*(int *)data = tp->tun_if.if_snd.ifq_head->m_pkthdr.len;
		else	
			*(int *)data = 0;
		splx(s);
		break;
	case TIOCSPGRP:
		tp->tun_pgrp = *(int *)data;
		break;
	case TIOCGPGRP:
		*(int *)data = tp->tun_pgrp;
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * The cdevsw read interface - reads a packet at a time, or at
 * least as much of a packet as can be read.
 */
int
tunread(dev, uio,flags)
	dev_t		dev;
	struct uio	*uio;
	int			flags;
{
	int		unit = minor(dev);
	struct tun_softc *tp = &tunctl[unit];
	struct ifnet	*ifp = &tp->tun_if;
	struct mbuf	*m, *m0;
	int		error=0, len, s;

	TUNDEBUG ("%s%d: read\n", ifp->if_name, ifp->if_unit);
	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG ("%s%d: not ready 0%o\n", ifp->if_name,
			  ifp->if_unit, tp->tun_flags);
		return EHOSTDOWN;
	}

	tp->tun_flags &= ~TUN_RWAIT;

	s = splimp();
	do {
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == 0) {
			if (tp->tun_flags & TUN_NBIO && flags & IO_NDELAY) {
				splx(s);
				return EWOULDBLOCK;
			}
			tp->tun_flags |= TUN_RWAIT;
			tsleep((caddr_t)tp, PZERO + 1, "tunread", 0);
		}
	} while (m0 == 0);
	splx(s);

	while (m0 && uio->uio_resid > 0 && error == 0) {
		len = min(uio->uio_resid, m0->m_len);
		if (len == 0)
			break;
		error = uiomove(mtod(m0, caddr_t), len, uio);
		MFREE(m0, m);
		m0 = m;
	}

	if (m0) {
		TUNDEBUG("Dropping mbuf\n");
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
tunwrite(dev, uio, flags)
	dev_t		dev;
	struct uio	*uio;
	int			flags;
{
	int		unit = minor (dev);
	struct ifnet	*ifp = &tunctl[unit].tun_if;
	struct ifqueue	*ifq;
	struct tunnel_header	*th;
	struct mbuf	*top, **mp, *m;
	int		isr;
	int		error=0, s, tlen, mlen;

	TUNDEBUG("%s%d: tunwrite\n", ifp->if_name, ifp->if_unit);

	if (uio->uio_resid < 0 || uio->uio_resid > TUNMTU) {
		TUNDEBUG("%s%d: len=%d!\n", ifp->if_name, ifp->if_unit,
		    uio->uio_resid);
		return EIO;
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
		if (uio->uio_resid > 0) {
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

	th = mtod( top, struct tunnel_header * );
		/* strip the tunnel header */
	top->m_data += sizeof(*th);
	top->m_len  -= sizeof(*th);
	top->m_pkthdr.len -= sizeof(*th);

	switch( th->tun_af )
	{
#ifdef INET
	case AF_INET:
		ifq = &ipintrq;
		isr = NETISR_IP;
		break;
#endif
#ifdef NS
	case AF_NS:
		ifq = &nsintrq;
		isr = NETISR_NS;
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
		splx(s);
		return EAFNOSUPPORT;
	}

	s = splimp();
	if( IF_QFULL(ifq) )
	{
		IF_DROP( ifq );
		splx(s);
		ifp->if_collisions++;
		m_freem(top);
		return ENOBUFS;
	}
	IF_ENQUEUE( ifq, top );
	schednetisr( isr );
	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;
	splx(s);
	return error;
}

/*
 * tunselect - the select interface, this is only useful on reads
 * really. The write detect always returns true, write never blocks
 * anyway, it either accepts the packet or drops it.
 */
int
tunselect(dev, rw, p)
	dev_t	dev;
	int		rw;
	struct proc *p;
{
	int		unit = minor(dev), s;
	struct tun_softc *tp = &tunctl[unit];
	struct ifnet	*ifp = &tp->tun_if;

	s = splimp();
	TUNDEBUG("%s%d: tunselect\n", ifp->if_name, ifp->if_unit);

	switch (rw) {
	case FREAD:
		if (ifp->if_snd.ifq_len > 0) {
			splx(s);
			TUNDEBUG("%s%d: tunselect q=%d\n", ifp->if_name,
			    ifp->if_unit, ifp->if_snd.ifq_len);
			return 1;
		}
		selrecord(curproc, &tp->tun_rsel);
		break;
	case FWRITE:
		splx(s);
		return 1;
	}
	splx(s);
	TUNDEBUG("%s%d: tunselect waiting\n", ifp->if_name, ifp->if_unit);
	return 0;
}

#endif  /* NTUN */
