/*	$OpenBSD: ip_ah.c,v 1.9 1997/07/18 18:09:51 provos Exp $	*/

/*
 * The author of this code is John Ioannidis, ji@tla.org,
 * 	(except when noted otherwise).
 *
 * This code was written for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis, kermit@forthnet.gr.
 *
 * Copyright (C) 1995, 1996, 1997 by John Ioannidis and Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NEITHER AUTHOR MAKES ANY
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

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#include <sys/socketvar.h>
#include <net/raw_cb.h>
#include <net/encap.h>

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>

#include <sys/syslog.h>

void	ah_input __P((struct mbuf *, int));

/*
 * ah_input gets called when we receive an packet with an AH.
 */

void
ah_input(register struct mbuf *m, int iphlen)
{
    struct ifqueue *ifq = NULL;
    struct ip *ipo, ipn;
    struct ah_old *ahp, ahn;
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
#ifdef ENCDEBUG
	    if (encdebug)
	      printf("ah_input(): (possibly too short) packet from %x to %x dropped\n", ipo->ip_src, ipo->ip_dst);
#endif /* ENCDEBUG */
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

    tdbp = gettdb(ahp->ah_spi, ipo->ip_dst, IPPROTO_AH);
    if (tdbp == NULL)
    {
	log(LOG_ERR, "ah_input(): could not find SA for AH packet from %x to %x, spi %08x", ipo->ip_src, ipo->ip_dst, ntohl(ahp->ah_spi));
	m_freem(m);
	ahstat.ahs_notdb++;
	return;
    }

    if (tdbp->tdb_flags & TDBF_INVALID)
    {
	log(LOG_ALERT,
	    "ah_input(): attempted to use invalid AH SA %08x, packet %x->%x",
	    ntohl(ahp->ah_spi), ipo->ip_src, ipo->ip_dst);
	m_freem(m);
	ahstat.ahs_invalid++;
	return;
    }

    if (tdbp->tdb_xform == NULL)
    {
	log(LOG_ALERT, "ah_input(): attempted to use uninitialized AH SA %08x, packet from %x to %x", ntohl(ahp->ah_spi), ipo->ip_src, ipo->ip_dst);
	m_freem(m);
	ahstat.ahs_noxform++;
	return;
    }

    m->m_pkthdr.rcvif = &enc_softc;

    /* Register first use */
    if (tdbp->tdb_first_use == 0)
      tdbp->tdb_first_use = time.tv_sec;

    ipn = *ipo;
    ahn = *ahp;

    m = (*(tdbp->tdb_xform->xf_input))(m, tdbp);
    if (m == NULL)
    {
	log(LOG_ALERT, "ah_input(): authentication failed for AH packet from %x to %x, spi %08x", ipn.ip_src, ipn.ip_dst, ntohl(ahn.ah_spi));
	ahstat.ahs_badkcr++;
	return;
    }

    /*
     * Interface pointer is already in first mbuf; chop off the 
     * `outer' header and reschedule.
     */

    ifq = &ipintrq;

    s = splimp();			/* isn't it already? */
    if (IF_QFULL(ifq))
    {
	IF_DROP(ifq);
	m_freem(m);
	ahstat.ahs_qfull++;
	splx(s);
#ifdef ENCDEBUG
	if (encdebug)
	  printf("ah_input(): dropped packet because of full IP queue\n");
#endif /* ENCDEBUG */
	return;
    }

    IF_ENQUEUE(ifq, m);
    schednetisr(NETISR_IP);
    splx(s);
    return;
}
