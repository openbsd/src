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
 * IP-inside-IP processing
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
#include <netinet/ip_ip4.h>
#include <dev/rndvar.h>



/*
 * ip4_input gets called when we receive an encapsulated packet,
 * either because we got it at a real interface, or because AH or ESP
 * were being used in tunnel mode (in which case the rcvif element will 
 * contain the address of the encapX interface associated with the tunnel.
 */

void
ip4_input(register struct mbuf *m, int iphlen)
{
	struct ip *ipo, *ipi;
	struct ifqueue *ifq = NULL;
	int s;
	/*
	 * Strip IP options, if any.
	 */

	if (iphlen > sizeof (struct ip))
	{
		ip_stripoptions(m, (struct mbuf *)0);
		iphlen = sizeof (struct ip);
	}
	
	/*
	 * Make sure next IP header is in the first mbuf.
	 *
	 * Careful here! we are receiving the packet from ipintr;
	 * this means that the ip_len field has been adjusted to
	 * not count the ip header, and is also in host order.
	 */

	ipo = mtod(m, struct ip *);

	if (m->m_len < iphlen + sizeof (struct ip))
	{
		if ((m = m_pullup(m, iphlen + sizeof (struct ip))) == 0)
		{
			ip4stat.ip4s_hdrops++;
			return;
		}
		ipo = mtod(m, struct ip *);
	}
	ipi = (struct ip *)((caddr_t)ipo + iphlen);
	
	/*
	 * XXX - Should we do anything to the inner packet?
	 * Does arriving at the far end of the tunnel count as one hop
	 * (thus requiring ipi->ip_ttl to be decremented)?
	 */

	if (ipi->ip_v != IPVERSION)
	{
		ip4stat.ip4s_notip4++;
		return;
	}
	
	/*
	 * Interface pointer is already in first mbuf; chop off the 
	 * `outer' header and reschedule.
	 */

	m->m_len -= iphlen;
	m->m_pkthdr.len -= iphlen;
	m->m_data += iphlen;
	
	/* XXX -- interface pointer stays the same (which is probably
	 * the way it should be.
	 */

	ifq = &ipintrq;

	s = splimp();			/* isn't it already? */
	if (IF_QFULL(ifq))
	{
		IF_DROP(ifq);
		m_freem(m);
		ip4stat.ip4s_qfull++;
		splx(s);
		return;
	}
	IF_ENQUEUE(ifq, m);
	schednetisr(NETISR_IP);
	splx(s);
	
	return;
}

int
ipe4_output(struct mbuf *m, struct sockaddr_encap *gw, struct tdb *tdb, struct mbuf **mp)
{
	struct ip *ipo, *ipi;
	struct ip4_xdata *xd;
	ushort ilen;

	ipi = mtod(m, struct ip *);
	ilen = ntohs(ipi->ip_len);

	M_PREPEND(m, sizeof (struct ip), M_DONTWAIT);
	if (m == 0)
	  return ENOBUFS;

	ipo = mtod(m, struct ip *);
	
	ipo->ip_v = IPVERSION;
	ipo->ip_hl = 5;
	ipo->ip_tos = ipi->ip_tos;
	ipo->ip_len = htons(ilen + sizeof (struct ip));
	/* ipo->ip_id = htons(ip_id++); */
	get_random_bytes((void *)&(ipo->ip_id), sizeof(ipo->ip_id));
	ipo->ip_off = ipi->ip_off & ~(IP_MF | IP_OFFMASK); /* keep C and DF */
	xd = (struct ip4_xdata *)tdb->tdb_xdata;
	switch (xd->ip4_ttl)
	{
	    case IP4_SAME_TTL:
		ipo->ip_ttl = ipi->ip_ttl;
		break;
	    case IP4_DEFAULT_TTL:
		ipo->ip_ttl = ip_defttl;
		break;
	    default:
		ipo->ip_ttl = xd->ip4_ttl;
	}
	
	ipo->ip_p = IPPROTO_IPIP;
	ipo->ip_sum = 0;
	ipo->ip_src = gw->sen_ipsp_src;
	ipo->ip_dst = gw->sen_ipsp_dst;
	
/*	printf("ip4_output: [%x->%x](l=%d, p=%d)", 
	       ntohl(ipi->ip_src.s_addr), ntohl(ipi->ip_dst.s_addr),
	       ilen, ipi->ip_p);
	printf(" through [%x->%x](l=%d, p=%d)\n", 
	       ntohl(ipo->ip_src.s_addr), ntohl(ipo->ip_dst.s_addr),
	       ipo->ip_len, ipo->ip_p);*/

	*mp = m;
	return 0;

/*	return ip_output(m, NULL, NULL, IP_ENCAPSULATED, NULL);*/
}

int
ipe4_attach()
{
	return 0;
}

int
ipe4_init(struct tdb *tdbp, struct xformsw *xsp, struct mbuf *m)
{
        struct ip4_xdata *xd;
	struct ip4_xencap txd;
	struct encap_msghdr *em;
	
#ifdef ENCDEBUG
        if (encdebug)
	  printf("ipe4_init: setting up\n");
#endif
	tdbp->tdb_xform = xsp;
	MALLOC(tdbp->tdb_xdata, caddr_t, sizeof (struct ip4_xdata), M_XDATA,
	       M_WAITOK);
	if (tdbp->tdb_xdata == NULL)
	  return ENOBUFS;
	bzero(tdbp->tdb_xdata, sizeof (struct ip4_xdata));
	xd = (struct ip4_xdata *)tdbp->tdb_xdata;
	
	em = mtod(m, struct encap_msghdr *);
	if (em->em_msglen - EMT_SETSPI_FLEN > sizeof (struct ip4_xencap))
	{
	    free((caddr_t)tdbp->tdb_xdata, M_XDATA);
	    tdbp->tdb_xdata = NULL;
	    return EINVAL;
	}
	m_copydata(m, EMT_SETSPI_FLEN, em->em_msglen - EMT_SETSPI_FLEN,
		   (caddr_t)&txd);
	xd->ip4_ttl = txd.ip4_ttl;
	return 0;
}

int
ipe4_zeroize(struct tdb *tdbp)
{
        FREE(tdbp->tdb_xdata, M_XDATA);
	return 0;
}



void
ipe4_input(struct mbuf *m, ...)
{
	printf("ipe4_input: should never be called\n");
	if (m)
	  m_freem(m);
}
