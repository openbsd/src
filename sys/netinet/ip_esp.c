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

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#include <sys/socketvar.h>
#include <net/raw_cb.h>
#include <net/encap.h>

#include <netinet/ip_icmp.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_esp.h>

/*
 * esp_input gets called when we receive an packet with an ESP.
 */

void
esp_input(register struct mbuf *m, int iphlen)
{
	struct ip *ipo;
	struct ifqueue *ifq = NULL;
	int s;
	u_long spi;
	struct tdb *tdbp;
	
	/*
	 * Strip IP options, if any.
	 */

	if (iphlen > sizeof (struct ip))
	{
		ip_stripoptions(m, (struct mbuf *)0);
		iphlen = sizeof (struct ip);
	}
	
	/*
	 * Make sure that at least the SPI is in the same mbuf
	 */

	ipo = mtod(m, struct ip *);
	if (m->m_len < iphlen + ESP_FLENGTH)
	{
		if ((m = m_pullup(m, iphlen + ESP_FLENGTH)) == 0)
		{
			espstat.esps_hdrops++;
			return;
		}
		ipo = mtod(m, struct ip *);
	}
	spi = *((u_long *)((caddr_t)ipo + iphlen));

	/*
	 * Find tunnel control block and (indirectly) call the appropriate
	 * kernel crypto routine. The resulting mbuf chain is a valid
	 * IP packet ready to go through input processing.
	 */

	tdbp = gettdb(spi, ipo->ip_dst);
	if (tdbp == NULL)
	{
#ifdef ENCDEBUG
		if (encdebug)
		  printf("esp_input: no tdb for spi=%x\n", spi);
#endif ENCDEBUG
		m_freem(m);
		espstat.esps_notdb++;
		return;
	}
	
	if (tdbp->tdb_xform == NULL)
	{
#ifdef ENCDEBUG
		if (encdebug)
		  printf("esp_input: no xform for spi=%x\n", spi);
#endif ENCDEBUG
		m_freem(m);
		espstat.esps_noxform++;
		return;
	}

	m->m_pkthdr.rcvif = tdbp->tdb_rcvif;

	m = (*(tdbp->tdb_xform->xf_input))(m, tdbp);

	if (m == NULL)
	{
		espstat.esps_badkcr++;
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
		espstat.esps_qfull++;
		splx(s);
		return;
	}
	IF_ENQUEUE(ifq, m);
	schednetisr(NETISR_IP);
	splx(s);
	return;
}
