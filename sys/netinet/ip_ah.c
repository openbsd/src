/*	$OpenBSD: ip_ah.c,v 1.20 1999/04/09 22:27:54 niklas Exp $	*/

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
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
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
 * Authentication Header Processing
 * Per RFC1826 (Atkinson, 1995)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/bpf.h>
#include <net/if_enc.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#include <sys/socketvar.h>
#include <net/raw_cb.h>

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>

#include "bpfilter.h"

extern struct ifnet enc_softc;

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

void ah_input __P((struct mbuf *, int));

/*
 * ah_input gets called when we receive an packet with an AH.
 */

void
ah_input(register struct mbuf *m, int iphlen)
{
    union sockaddr_union sunion;
    struct ifqueue *ifq = NULL;
    struct ah_old *ahp, ahn;
    struct expiration *exp;
    struct ip *ipo, ipn;
    struct tdb *tdbp;
    int s;
	
    ahstat.ahs_input++;

    /*
     * Make sure that at least the fixed part of the AH header is
     * in the first mbuf.
     */

    ipo = mtod(m, struct ip *);
    if (m->m_len < iphlen + AH_OLD_FLENGTH)
    {
	if ((m = m_pullup(m, iphlen + AH_OLD_FLENGTH)) == 0)
	{
	    ahstat.ahs_hdrops++;
	    return;
	}

	ipo = mtod(m, struct ip *);
    }

    ahp = (struct ah_old *) ((caddr_t) ipo + iphlen);

    /*
     * Find tunnel control block and (indirectly) call the appropriate
     * tranform routine. The resulting mbuf chain is a valid
     * IP packet ready to go through input processing.
     */

    bzero(&sunion, sizeof(sunion));
    sunion.sin.sin_family = AF_INET;
    sunion.sin.sin_len = sizeof(struct sockaddr_in);
    sunion.sin.sin_addr = ipo->ip_dst;
    tdbp = gettdb(ahp->ah_spi, &sunion, IPPROTO_AH);
    if (tdbp == NULL)
    {
	DPRINTF(("ah_input(): could not find SA for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo->ip_src), ipsp_address(sunion), ntohl(ahp->ah_spi)));
	m_freem(m);
	ahstat.ahs_notdb++;
	return;
    }

    if (tdbp->tdb_flags & TDBF_INVALID)
    {
	DPRINTF(("ah_input(): attempted to use invalid SA %08x, packet from %s to %s\n", ntohl(ahp->ah_spi), inet_ntoa4(ipo->ip_src), ipsp_address(sunion)));
	m_freem(m);
	ahstat.ahs_invalid++;
	return;
    }

    if (tdbp->tdb_xform == NULL)
    {
	DPRINTF(("ah_input(): attempted to use uninitialized SA %08x, packet from %s to %s\n", ntohl(ahp->ah_spi), inet_ntoa4(ipo->ip_src), ipsp_address(sunion)));
	m_freem(m);
	ahstat.ahs_noxform++;
	return;
    }

    m->m_pkthdr.rcvif = &enc_softc;

    /* Register first use, setup expiration timer */
    if (tdbp->tdb_first_use == 0)
    {
	tdbp->tdb_first_use = time.tv_sec;

	if (tdbp->tdb_flags & TDBF_FIRSTUSE)
	{
	    exp = get_expiration();
	    bcopy(&tdbp->tdb_dst, &exp->exp_dst, SA_LEN(&tdbp->tdb_dst.sa));
	    exp->exp_spi = tdbp->tdb_spi;
	    exp->exp_sproto = tdbp->tdb_sproto;
	    exp->exp_timeout = tdbp->tdb_first_use + tdbp->tdb_exp_first_use;
	    put_expiration(exp);
	}

	if ((tdbp->tdb_flags & TDBF_SOFT_FIRSTUSE) &&
	    (tdbp->tdb_soft_first_use <= tdbp->tdb_exp_first_use))
	{
	    exp = get_expiration();
	    bcopy(&tdbp->tdb_dst, &exp->exp_dst, SA_LEN(&tdbp->tdb_dst.sa));
	    exp->exp_spi = tdbp->tdb_spi;
	    exp->exp_sproto = tdbp->tdb_sproto;
	    exp->exp_timeout = tdbp->tdb_first_use + tdbp->tdb_soft_first_use;
	    put_expiration(exp);
	}
    }
    
    ipn = *ipo;
    ahn = *ahp;

    m = (*(tdbp->tdb_xform->xf_input))(m, tdbp);
    if (m == NULL)
    {
	DPRINTF(("ah_input(): authentication failed for AH packet from %s to %s, spi %08x\n", inet_ntoa4(ipn.ip_src), ipsp_address(sunion), ntohl(ahn.ah_spi)));
	ahstat.ahs_badkcr++;
	return;
    }

    ipo = mtod(m, struct ip *);
    if (ipo->ip_p == IPPROTO_IPIP)	/* IP-in-IP encapsulation */
    {
	/* ipn will now contain the inner IP header */
	m_copydata(m, ipo->ip_hl << 2, sizeof(struct ip), (caddr_t) &ipn);
	    
	if (tdbp->tdb_flags & TDBF_UNIQUE)
	  if ((ipn.ip_src.s_addr != ipo->ip_src.s_addr) ||
	      (ipn.ip_dst.s_addr != ipo->ip_dst.s_addr))
	  {
	      DPRINTF(("ah_input(): AH-tunnel with different internal addresses %s->%s (%s->%s), SA %s/%08x\n", inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst), inet_ntoa4(ipn.ip_src), ipsp_address(sunion), ipsp_address(tdbp->tdb_dst), ntohl(tdbp->tdb_spi)));
	      m_freem(m);
	      ahstat.ahs_hdrops++;
	      return;
	  }

	/*
	 * Check that the inner source address is the same as
	 * the proxy address, if available.
	 */
	if ((tdbp->tdb_proxy.sin.sin_addr.s_addr != INADDR_ANY) &&
	    (ipn.ip_src.s_addr != tdbp->tdb_proxy.sin.sin_addr.s_addr))
	{
	    DPRINTF(("ah_input(): inner source address %s doesn't correspond to expected proxy source %s, SA %s/%08x\n", inet_ntoa4(ipo->ip_src), ipsp_address(tdbp->tdb_proxy), ipsp_address(tdbp->tdb_dst), ntohl(tdbp->tdb_spi)));
	    m_free(m);
	    ahstat.ahs_hdrops++;
	    return;
	}
    }

    /*
     * Check that the outter source address is an expected one, if we know
     * what it's supposed to be. This avoids source address spoofing.
     */
    if ((tdbp->tdb_src.sin.sin_addr.s_addr != INADDR_ANY) &&
	(ipo->ip_src.s_addr != tdbp->tdb_src.sin.sin_addr.s_addr))
    {
	DPRINTF(("ah_input(): source address %s doesn't correspond to expected source %s, SA %s/%08x\n", inet_ntoa4(ipo->ip_src), ipsp_address(tdbp->tdb_src), ipsp_address(tdbp->tdb_dst), ntohl(tdbp->tdb_spi)));
	m_free(m);
	ahstat.ahs_hdrops++;
	return;
    }

    if (ipo->ip_p == IPPROTO_TCP || ipo->ip_p == IPPROTO_UDP)
    {
	struct tdb_ident *tdbi = NULL;
	if (tdbp->tdb_bind_out)
	{
	    tdbi = m->m_pkthdr.tdbi;
	    if (!(m->m_flags & M_PKTHDR))
	    {
		DPRINTF(("ah_input(): mbuf is not a packet header!\n"));
	    }
	    MALLOC(tdbi, struct tdb_ident *, sizeof(struct tdb_ident),
		   M_TEMP, M_NOWAIT);

	    if (!tdbi)
	      goto no_mem;

	    tdbi->spi = tdbp->tdb_bind_out->tdb_spi;
	    tdbi->dst = tdbp->tdb_bind_out->tdb_dst;
	    tdbi->proto = tdbp->tdb_bind_out->tdb_sproto;
	}

    no_mem:
	m->m_pkthdr.tdbi = tdbi;
    } else
        m->m_pkthdr.tdbi = NULL;

    /* Packet is authentic */
    m->m_flags |= M_AUTH;

#if NBPFILTER > 0
    if (enc_softc.if_bpf) 
    {
        /*
         * We need to prepend the address family as
         * a four byte field.  Cons up a dummy header
         * to pacify bpf.  This is safe because bpf
         * will only read from the mbuf (i.e., it won't
         * try to free it or keep a pointer a to it).
         */
        struct mbuf m0;
        struct enchdr hdr;

	hdr.af = AF_INET;
	hdr.spi = tdbp->tdb_spi;
	hdr.flags = m->m_flags & (M_AUTH|M_CONF);

        m0.m_next = m;
        m0.m_len = ENC_HDRLEN;
        m0.m_data = (char *) &hdr;
        
        bpf_mtap(enc_softc.if_bpf, &m0);
    }
#endif

    /*
     * Interface pointer is already in first mbuf; chop off the 
     * `outer' header and reschedule.
     */

    ifq = &ipintrq;

    s = splimp();			/* isn't it already? */
    if (IF_QFULL(ifq))
    {
	IF_DROP(ifq);
	if (m->m_pkthdr.tdbi)
		free(m->m_pkthdr.tdbi, M_TEMP);
	m_freem(m);
	ahstat.ahs_qfull++;
	splx(s);
	DPRINTF(("ah_input(): dropped packet because of full IP queue\n"));
	return;
    }

    IF_ENQUEUE(ifq, m);
    schednetisr(NETISR_IP);
    splx(s);
    return;
}
