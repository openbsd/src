/*	$OpenBSD: ip_esp.c,v 1.17 1999/02/24 22:33:01 angelos Exp $	*/

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
 * Encapsulation Security Payload Processing
 * Per RFC1827 (Atkinson, 1995)
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

#include <sys/socketvar.h>
#include <net/raw_cb.h>

#include <netinet/ip_icmp.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>

#include "bpfilter.h"

extern struct ifnet enc_softc;

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

void esp_input __P((struct mbuf *, int));

/*
 * esp_input gets called when we receive an packet with an ESP.
 */

void
esp_input(register struct mbuf *m, int iphlen)
{
    union sockaddr_union sunion;
    struct ifqueue *ifq = NULL;
    struct expiration *exp;
    struct ip *ipo, ipn;
    struct tdb *tdbp;
    u_int32_t spi;
    int s;
	
    espstat.esps_input++;

    /*
     * Make sure that at least the SPI is in the same mbuf
     */

    ipo = mtod(m, struct ip *);
    if (m->m_len < iphlen + sizeof(u_int32_t))
    {
	if ((m = m_pullup(m, iphlen + sizeof(u_int32_t))) == 0)
	{
	    espstat.esps_hdrops++;
	    return;
	}

	ipo = mtod(m, struct ip *);
    }

    spi = *((u_int32_t *) ((caddr_t) ipo + iphlen));

    /*
     * Find tunnel control block and (indirectly) call the appropriate
     * kernel crypto routine. The resulting mbuf chain is a valid
     * IP packet ready to go through input processing.
     */

    bzero(&sunion, sizeof(sunion));
    sunion.sin.sin_family = AF_INET;
    sunion.sin.sin_len = sizeof(struct sockaddr_in);
    sunion.sin.sin_addr = ipo->ip_dst;
    tdbp = gettdb(spi, &sunion, IPPROTO_ESP);
    if (tdbp == NULL)
    {
	DPRINTF(("esp_input(): could not find SA for packet from %s to %s, spi %08x\n", inet_ntoa4(ipo->ip_src), ipsp_address(sunion), ntohl(spi)));
	m_freem(m);
	espstat.esps_notdb++;
	return;
    }
	
    if (tdbp->tdb_flags & TDBF_INVALID)
    {
	DPRINTF(("esp_input(): attempted to use invalid SA %08x, packet from %s to %s\n", ntohl(spi), inet_ntoa4(ipo->ip_src), ipsp_address(sunion)));
	m_freem(m);
	espstat.esps_invalid++;
	return;
    }

    if (tdbp->tdb_xform == NULL)
    {
	DPRINTF(("esp_input(): attempted to use uninitialized SA %08x, packet from %s to %s\n", ntohl(spi), inet_ntoa4(ipo->ip_src), ipsp_address(sunion)));
	m_freem(m);
	espstat.esps_noxform++;
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

    m = (*(tdbp->tdb_xform->xf_input))(m, tdbp);

    if (m == NULL)
    {
	DPRINTF(("esp_input(): processing failed for ESP packet from %s to %s, spi %08x\n", inet_ntoa4(ipn.ip_src), ipsp_address(sunion), ntohl(spi)));
	espstat.esps_badkcr++;
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
	      DPRINTF(("esp_input(): ESP-tunnel with different internal addresses %s->%s (%s->%s), SA %s/%08x\n", inet_ntoa4(ipo->ip_src), inet_ntoa4(ipo->ip_dst), inet_ntoa4(ipn.ip_src), ipsp_address(sunion), ipsp_address(tdbp->tdb_dst), ntohl(tdbp->tdb_spi)));
	      m_freem(m);
	      espstat.esps_hdrops++;
	      return;
	  }

	/*
	 * Check that the inner source address is the same as
	 * the proxy address, if available.
	 */
	if ((tdbp->tdb_proxy.sin.sin_addr.s_addr != INADDR_ANY) &&
	    (ipn.ip_src.s_addr != tdbp->tdb_proxy.sin.sin_addr.s_addr))
	{
	    DPRINTF(("esp_input(): inner source address %s doesn't correspond to expected proxy source %s, SA %s/%08x\n", inet_ntoa4(ipo->ip_src), inet_ntoa4(tdbp->tdb_proxy.sin.sin_addr), inet_ntoa4(tdbp->tdb_dst.sin.sin_addr), ntohl(tdbp->tdb_spi)));
	    m_free(m);
	    espstat.esps_hdrops++;
	    return;
	}
    }

    /* 
     * Check that the source address is an expected one, if we know what
     * it's supposed to be. This avoids source address spoofing.
     */
    if ((tdbp->tdb_src.sin.sin_addr.s_addr != INADDR_ANY) &&
	(ipo->ip_src.s_addr != tdbp->tdb_src.sin.sin_addr.s_addr))
    {
	DPRINTF(("esp_input(): source address %s doesn't correspond to expected source %s, SA %s/%08x\n", inet_ntoa4(ipo->ip_src), ipsp_address(tdbp->tdb_src), ipsp_address(tdbp->tdb_dst), ntohl(tdbp->tdb_spi)));
	m_free(m);
	espstat.esps_hdrops++;
	return;
    }

    /* Packet is confidental */
    m->m_flags |= M_CONF;

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
	m_freem(m);
	espstat.esps_qfull++;
	splx(s);
	DPRINTF(("esp_input(): dropped packet because of full IP queue\n"));
	return;
    }

    IF_ENQUEUE(ifq, m);
    schednetisr(NETISR_IP);
    splx(s);
    return;
}
