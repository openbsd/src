/*	$OpenBSD: if_enc.c,v 1.40 2003/05/03 21:15:11 deraadt Exp $	*/
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

#include <net/if_enc.h>

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
#include "enc.h"

#ifdef ENCDEBUG
#define DPRINTF(x)    do { if (encdebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

struct enc_softc encif[NENC];

void	encattach(int);
int	encoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	    	       struct rtentry *);
int	encioctl(struct ifnet *, u_long, caddr_t);
void	encrtrequest(int, struct rtentry *, struct sockaddr *);
void	encstart(struct ifnet *);

extern int ifqmaxlen;

void
encattach(int nenc)
{
    struct ifnet *ifp;
    int i;

    bzero(encif, sizeof(encif));

    for (i = 0; i < NENC; i++)
    {
	ifp = &encif[i].sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "enc%d", i);
	ifp->if_softc = &encif[i];
	ifp->if_mtu = ENCMTU;
	ifp->if_ioctl = encioctl;
	ifp->if_output = encoutput;
	ifp->if_start = encstart;
	ifp->if_type = IFT_ENC;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = ENC_HDRLEN;
	if_attach(ifp);
	if_alloc_sadl(ifp);

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
    struct mbuf *m;
    int s;

    for (;;)
    {
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
encoutput(ifp, m, dst, rt)
struct ifnet *ifp;
register struct mbuf *m;
struct sockaddr *dst;
register struct rtentry *rt;
{
    m_freem(m);
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
    switch (cmd) 
    {
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
	    return (EINVAL);
    }

    return 0;
}
