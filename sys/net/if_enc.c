/*	$OpenBSD: if_enc.c,v 1.17 2000/01/07 20:14:51 angelos Exp $	*/

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
#include <sys/proc.h>

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
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#include <netinet6/in6.h>
#include <netinet6/ip6.h>
#include <netinet6/ip6_var.h>
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

#ifndef offsetof
#define offsetof(s, e) ((int)&((s *)0)->e)
#endif

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

    for (i = 0; i < NENC; i++)
    {
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
    struct mbuf *m;
    int s;

#ifndef IPSEC
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
#else /* IPSEC */
    struct enc_softc *enc = ifp->if_softc;
    int err = 0, protoflag;
    struct mbuf *mp;
    struct tdb *tdb;

    /* If the interface is not setup, flush the queue */
    if ((enc->sc_spi == 0) && (enc->sc_sproto == 0) &&
	((enc->sc_dst.sa.sa_family == AF_INET) ||
	 (enc->sc_dst.sa.sa_family == AF_INET6)))
    {
	DPRINTF(("%s: not initialized with SA\n", ifp->if_xname));

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

	/* Unreachable */
    }

    /* Find what type of processing we need to do */
    tdb = gettdb(enc->sc_spi, &(enc->sc_dst), enc->sc_sproto);
    if (tdb == NULL)
    {
	DPRINTF(("%s: SA non-existant\n", ifp->if_xname));

	/* Flush the queue */
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

    /* See if we need to notify a key mgmt. daemon to setup SAs */
    if (ntohl(enc->sc_spi) == SPI_LOCAL_USE)
    {
	/*
	 * XXX Can't do this for now, as there's no way for
	 * XXX key mgmt. to specify link-layer properties
	 * XXX (e.g., encrypt everything on this interface)
	 */ 
#ifdef notyet
	if (tdb->tdb_satype != SADB_X_SATYPE_BYPASS)
	  pfkeyv2_acquire(tdb, 0); /* No point checking for errors */
#endif

	/* Flush the queue */
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

	/* Unreachable */
    }

    /* IPsec-process all packets in the queue */
    for (;;)
    {
	/* Get a packet from the queue */
	s = splimp();
	IF_DEQUEUE(&ifp->if_snd, m);
	splx(s);

	if (m == NULL) /* Empty queue */
	  return;

	/* First, we encapsulate in etherip */
	err = etherip_output(m, tdb, &mp, 0, 0); /* Last 2 args not used */
	if ((mp == NULL) || err)
	{
	    /* Just skip this frame */
            IF_DROP(&ifp->if_snd);
	    if (mp)
	      m_freem(mp);
	    continue;
	}
	else
	{
	    m = mp;
	    mp = NULL;
	}

	protoflag = tdb->tdb_dst.sa.sa_family;

	/* IPsec packet processing -- skip encapsulation */
	err = ipsp_process_packet(m, &mp, tdb, &protoflag, 1);
	if ((mp == NULL) || err)
	{
            IF_DROP(&ifp->if_snd);
	    if (mp)
	      m_freem(mp);
	    continue;
	}
	else
	{
	    m = mp;
	    mp = NULL;
	}

#ifdef INET
	/* Send the packet on its way, no point checking for errors here */
	if (protoflag == AF_INET)
	  ip_output(m, NULL, NULL, IP_ENCAPSULATED | IP_RAWOUTPUT, NULL, NULL);
#endif /* INET */

#ifdef INET6
	/* Send the packet on its way, no point checking for errors here */
	if (protoflag == AF_INET6)
	  ip6_output(m, NULL, NULL, IP_ENCAPSULATED | IP_RAWOUTPUT,
		     NULL, NULL);
#endif /* INET6 */

	/* XXX Should find a way to avoid bridging-loops, some mbuf flag ? */
    }
#endif /* IPSEC */
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
    
    if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE))
    {
	m_freem(m);
	return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
		rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
    }

    ifp->if_opackets++;
    ifp->if_obytes += m->m_pkthdr.len;

    switch (dst->sa_family)
    {
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
    if (IF_QFULL(ifq))
    {
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
#ifdef IPSEC
    struct enc_softc *enc = (struct enc_softc *) ifp->if_softc;
    struct ifsa *ifsa = (struct ifsa *) data;
    struct proc *prc = curproc;             /* XXX */
    struct tdb *tdb;
    int s, error;

    switch (cmd) 
    {
	case SIOCSIFADDR:
	    return EOPNOTSUPP;

	case SIOCGENCSA:
	    ifsa->sa_spi = enc->sc_spi;
	    ifsa->sa_proto = enc->sc_sproto;
	    bcopy(&enc->sc_dst, &ifsa->sa_dst, enc->sc_dst.sa.sa_len);
	    break;

	case SIOCSENCCLEARSA:
	    /* Check for superuser */
	    if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
	      break;

	    if (ifsa->sa_proto == 0)
	    {
		/* Clear SA if requested */
		if (enc->sc_sproto != 0)
		{
		    s = spltdb();
		    tdb = gettdb(enc->sc_spi, &enc->sc_dst, enc->sc_sproto);
		    if (tdb != NULL)
		      tdb->tdb_interface = 0;
		    splx(s);
		}
		
		bzero(&enc->sc_dst, sizeof(union sockaddr_union));
		enc->sc_spi = 0;
		enc->sc_sproto = 0;
		break;
	    }

	    s = spltdb();
	    tdb = gettdb(ifsa->sa_spi, &ifsa->sa_dst, ifsa->sa_proto);
	    if (tdb == NULL)
	    {
		splx(s);
		error = ENOENT;
		break;
	    }

	    tdb->tdb_interface = 0;
	    splx(s);
	    break;

	case SIOCSENCSRCSA:
	    /* Check for superuser */
	    if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
	      break;

	    if (ifsa->sa_proto == 0)
	    {
		error = ENOENT;
		break;
	    }

	    s = spltdb();
	    tdb = gettdb(ifsa->sa_spi, &ifsa->sa_dst, ifsa->sa_proto);
	    if (tdb == NULL)
	    {
		splx(s);
		error = ENOENT;
		break;
	    }

	    /* Is it already bound ? */
	    if (tdb->tdb_interface)
	    {
		splx(s);
		error = EEXIST;
		break;
	    }

	    tdb->tdb_interface = (caddr_t) ifp;
	    splx(s);
	    break;

	case SIOCSENCDSTSA:
	    /* Check for superuser */
	    if ((error = suser(prc->p_ucred, &prc->p_acflag)) != 0)
	      break;

	    /* Check for pre-existing TDB */
	    if (enc->sc_sproto != 0)
	    {
		error = EEXIST;
		break;
	    }

	    s = spltdb();

	    if (ifsa->sa_proto != 0)
	    {
		tdb = gettdb(ifsa->sa_spi, &ifsa->sa_dst, ifsa->sa_proto);
		if (tdb == NULL)
		{
		    splx(s);
		    error = ENOENT;
		    break;
		}
	    }
	    else
	    {
		/* Clear SA if requested */
		if (enc->sc_sproto != 0)
		{
		    tdb = gettdb(enc->sc_spi, &enc->sc_dst, enc->sc_sproto);
		    if (tdb != NULL)
		      tdb->tdb_interface = 0;
		}

		bzero(&enc->sc_dst, sizeof(enc->sc_dst));
		enc->sc_spi = 0;
		enc->sc_sproto = 0;

		splx(s);
		break;
	    }

#ifdef INET
	    if ((ifsa->sa_dst.sa.sa_family == AF_INET) &&
		(ifsa->sa_dst.sa.sa_len != sizeof(struct sockaddr_in)))
	    {
		splx(s);
	    	error = EINVAL;
		break;
	    }
#endif /* INET */

#ifdef INET6
	    if ((ifsa->sa_dst.sa.sa_family == AF_INET6) &&
		(ifsa->sa_dst.sa.sa_len != sizeof(struct sockaddr_in6)))
	    {
		splx(s);
		error = EINVAL;
		break;
	    }
#endif /* INET6 */

	    bcopy(&ifsa->sa_dst, &enc->sc_dst, ifsa->sa_dst.sa.sa_len);
	    enc->sc_spi = ifsa->sa_spi;
	    enc->sc_sproto = ifsa->sa_proto;
	    tdb->tdb_interface = (caddr_t) ifp;

	    splx(s);
	    break;

	default:
	    error = EINVAL;
	    break;
    }

    return (error);
#else /* IPSEC */
    return EOPNOTSUPP;
#endif /* IPSEC */
}
