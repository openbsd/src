/*	$OpenBSD: if_enc.c,v 1.13 1999/11/02 00:15:56 angelos Exp $	*/

/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and 
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece, 
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

/*
 * Encapsulation interface driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>

#include <netinet/ip_ipsp.h>
#include <net/if_enc.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef ISO
extern struct ifqueue clnlintrq;
#endif

#ifdef NS
extern struct ifqueue nsintrq;
#endif

#include "bpfilter.h"
#include "enc.h"

struct enc_softc encif[NENC];

void	encattach __P((int));
int	encoutput __P((struct ifnet *, struct mbuf *, struct sockaddr *,
	    	       struct rtentry *));
int	encioctl __P((struct ifnet *, u_long, caddr_t));
void	encrtrequest __P((int, struct rtentry *, struct sockaddr *));
void	encstart __P((struct ifnet *));

extern int ifqmaxlen;

void
encattach(int nenc)
{
    struct ifnet *ifp;
    int i;

    bzero(encif, sizeof(encif));

    for (i = 0; i < NENC; i++) {
	ifp = &encif[i].sc_if;
	sprintf(ifp->if_xname, "enc%d", i);
	ifp->if_softc = &encif[i];
	ifp->if_mtu = ENCMTU;
	ifp->if_ioctl = encioctl;
	ifp->if_output = encoutput;
	ifp->if_start = encstart;
	ifp->if_type = IFT_ENC;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = ENC_HDRLEN;
	if_attach(ifp);

#if NBPFILTER > 0
	bpfattach(&encif[i].sc_if.if_bpf, ifp, DLT_ENC, ENC_HDRLEN);
#endif
    }
}

/*
 * Start output on the enc interface.
 */
void
encstart(ifp)
struct ifnet *ifp;
{
    /* XXX Code needed */
}

/*
 * Shamelessly stolen from looutput()
 */
int
encoutput(ifp, m, dst, rt)
struct ifnet *ifp;
register struct mbuf *m;
struct sockaddr *dst;
register struct rtentry *rt;
{
    register struct ifqueue *ifq = 0;
    int s, isr;

    if ((m->m_flags & M_PKTHDR) == 0)
      panic("encoutput(): no HDR");

    ifp->if_lastchange = time;
    m->m_pkthdr.rcvif = ifp;
    
    if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
	m_freem(m);
	return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
		rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
    }

    ifp->if_opackets++;
    ifp->if_obytes += m->m_pkthdr.len;

    switch (dst->sa_family) {
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
	    m_freem(m);
	    return (EAFNOSUPPORT);
    }

    s = splimp();
    if (IF_QFULL(ifq)) {
	IF_DROP(ifq);
	m_freem(m);
	splx(s);
	return (ENOBUFS);
    }
	
    IF_ENQUEUE(ifq, m);
    schednetisr(isr);

    /* Statistics */
    ifp->if_ipackets++;
    ifp->if_ibytes += m->m_pkthdr.len;
    splx(s);
    return (0);
}

/* ARGSUSED */
void
encrtrequest(cmd, rt, sa)
int cmd;
struct rtentry *rt;
struct sockaddr *sa;
{
    if (rt)
      rt->rt_rmx.rmx_mtu = ENCMTU;
}

/* ARGSUSED */
int
encioctl(ifp, cmd, data)
register struct ifnet *ifp;
u_long cmd;
caddr_t data;
{
    register struct ifaddr *ifa;
    register int error = 0;

    switch (cmd) {
	case SIOCSIFADDR:
	    /*
	     * Everything else is done at a higher level.
	     */
	    ifp->if_flags |= IFF_UP;
	    ifa = (struct ifaddr *) data;
	    break;

	default:
	    error = EINVAL;
	    break;
    }
    return (error);
}
