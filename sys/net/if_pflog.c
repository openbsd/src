/*	$OpenBSD: if_pflog.c,v 1.1 2001/06/25 20:48:16 provos Exp $	*/
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
 * Copyright (c) 2001, Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
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
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/bpf.h>

#include <net/if_pflog.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/nd6.h>
#endif /* INET6 */

#ifdef ISO
extern struct ifqueue clnlintrq;
#endif

#ifdef NS
extern struct ifqueue nsintrq;
#endif

#include "bpfilter.h"
#include "pflog.h"

#define PFLOGMTU	(32768 + MHLEN + MLEN)

#ifdef PFLOGDEBUG
#define DPRINTF(x)    do { if (pflogdebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

#if NPFLOG != 2
#error "if_plog needs exactly two interfaces, fix sys/conf/GENERIC"
#endif

struct pflog_softc pflogif[NPFLOG];

void	pflogattach __P((int));
int	pflogoutput __P((struct ifnet *, struct mbuf *, struct sockaddr *,
	    	       struct rtentry *));
int	pflogioctl __P((struct ifnet *, u_long, caddr_t));
void	pflogrtrequest __P((int, struct rtentry *, struct sockaddr *));
void	pflogstart __P((struct ifnet *));

extern int ifqmaxlen;

void
pflogattach(int npflog)
{
	struct ifnet *ifp;
	int i;

	bzero(pflogif, sizeof(pflogif));

	for (i = 0; i < NPFLOG; i++) {
		ifp = &pflogif[i].sc_if;
		sprintf(ifp->if_xname, "pflog%d", i);
		ifp->if_softc = &pflogif[i];
		ifp->if_mtu = PFLOGMTU;
		ifp->if_ioctl = pflogioctl;
		ifp->if_output = pflogoutput;
		ifp->if_start = pflogstart;
		ifp->if_type = IFT_LOOP;
		ifp->if_snd.ifq_maxlen = ifqmaxlen;
		ifp->if_hdrlen = PFLOG_HDRLEN;
		if_attach(ifp);

#if NBPFILTER > 0
		bpfattach(&pflogif[i].sc_if.if_bpf, ifp, DLT_LOOP,
			  PFLOG_HDRLEN);
#endif
#ifdef INET6
		nd6_ifattach(ifp);
#endif
	}
}

/*
 * Start output on the pflog interface.
 */
void
pflogstart(ifp)
    struct ifnet *ifp;
{
	struct mbuf *m;
	int s;

	for (;;) {
		s = splimp();
		IF_DROP(&ifp->if_snd);
		IF_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL)
			return;
		else
			m_freem(m);
	}
}

int
pflogoutput(ifp, m, dst, rt)
    struct ifnet *ifp;
    register struct mbuf *m;
    struct sockaddr *dst;
    register struct rtentry *rt;
{
	m_freem(m);
	return 0;
}

/* ARGSUSED */
void
pflogrtrequest(cmd, rt, sa)
int cmd;
struct rtentry *rt;
struct sockaddr *sa;
{
    if (rt)
	    rt->rt_rmx.rmx_mtu = PFLOGMTU;
}

/* ARGSUSED */
int
pflogioctl(ifp, cmd, data)
    register struct ifnet *ifp;
    u_long cmd;
    caddr_t data;
{
	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;

	default:
		return EINVAL;
	}

	return (0);
}
