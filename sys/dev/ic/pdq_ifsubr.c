/*	$OpenBSD: pdq_ifsubr.c,v 1.2 1996/05/10 12:41:12 deraadt Exp $	*/
/*	$NetBSD: pdq_ifsubr.c,v 1.3 1996/05/07 01:43:15 thorpej Exp $	*/

/*-
 * Copyright (c) 1995 Matt Thomas (thomas@lkg.dec.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * from Id: pdq_ifsubr.c,v 1.2 1995/08/20 18:59:00 thomas Exp
 *
 * Log: pdq_ifsubr.c,v
 * Revision 1.2  1995/08/20  18:59:00  thomas
 * Changes for NetBSD
 *
 * Revision 1.1  1995/08/20  15:43:49  thomas
 * Initial revision
 *
 * Revision 1.13  1995/08/04  21:54:56  thomas
 * Clean IRQ processing under BSD/OS.
 * A receive tweaks.  (print source of MAC CRC errors, etc.)
 *
 * Revision 1.12  1995/06/02  16:04:22  thomas
 * Use correct PCI defs for BSDI now that they have fixed them.
 * Increment the slot number 0x1000, not one! (*duh*)
 *
 * Revision 1.11  1995/04/21  13:23:55  thomas
 * Fix a few pub in the DEFPA BSDI support
 *
 * Revision 1.10  1995/04/20  21:46:42  thomas
 * Why???
 * ,
 *
 * Revision 1.9  1995/04/20  20:17:33  thomas
 * Add PCI support for BSD/OS.
 * Fix BSD/OS EISA support.
 * Set latency timer for DEFPA to recommended value if 0.
 *
 * Revision 1.8  1995/04/04  22:54:29  thomas
 * Fix DEFEA support
 *
 * Revision 1.7  1995/03/14  01:52:52  thomas
 * Update for new FreeBSD PCI Interrupt interface
 *
 * Revision 1.6  1995/03/10  17:06:59  thomas
 * Update for latest version of FreeBSD.
 * Compensate for the fast that the ifp will not be first thing
 * in softc on BSDI.
 *
 * Revision 1.5  1995/03/07  19:59:42  thomas
 * First pass at BSDI EISA support
 *
 * Revision 1.4  1995/03/06  17:06:03  thomas
 * Add transmit timeout support.
 * Add support DEFEA (untested).
 *
 * Revision 1.3  1995/03/03  13:48:35  thomas
 * more fixes
 *
 *
 */

/*
 * DEC PDQ FDDI Controller; code for BSD derived operating systems
 *
 * Written by Matt Thomas
 *
 *   This driver supports the following FDDI controllers:
 *
 *	Device:			Config file entry:
 *	  DEC DEFPA (PCI)         device fpa0
 *	  DEC DEFEA (EISA)        device fea0 at isa0 net irq ? vector feaintr
 *
 *   Eventually, the following adapters will also be supported:
 *
 *	  DEC DEFTA (TC)	  device fta0 at tc? slot * vector ftaintr
 *	  DEC DEFQA (Q-Bus)	  device fta0 at uba? csr 0?? vector fqaintr
 *	  DEC DEFAA (FB+)	  device faa0 at fbus? slot * vector faaintr
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#if defined(__FreeBSD__)
#include <sys/devconf.h>
#elif defined(__bsdi__) || defined(__NetBSD__)
#include <sys/device.h>
#endif

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif
#if defined(__FreeBSD__)
#include <netinet/if_fddi.h>
#else
#include <net/if_fddi.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>

#include "pdqreg.h"
#if defined(__NetBSD__)
#include "pdqvar.h"
#else
#include "pdq_os.h"
#endif

void
pdq_ifinit(
    pdq_softc_t *sc)
{
    if (sc->sc_if.if_flags & IFF_UP) {
	sc->sc_if.if_flags |= IFF_RUNNING;
	if (sc->sc_if.if_flags & IFF_PROMISC) {
	    sc->sc_pdq->pdq_flags |= PDQ_PROMISC;
	} else {
	    sc->sc_pdq->pdq_flags &= ~PDQ_PROMISC;
	}
	if (sc->sc_if.if_flags & IFF_ALLMULTI) {
	    sc->sc_pdq->pdq_flags |= PDQ_ALLMULTI;
	} else {
	    sc->sc_pdq->pdq_flags &= ~PDQ_ALLMULTI;
	}
	if (sc->sc_if.if_flags & IFF_LINK1) {
	    sc->sc_pdq->pdq_flags |= PDQ_PASS_SMT;
	} else {
	    sc->sc_pdq->pdq_flags &= ~PDQ_PASS_SMT;
	}
	sc->sc_pdq->pdq_flags |= PDQ_RUNNING;
	pdq_run(sc->sc_pdq);
    } else {
	sc->sc_if.if_flags &= ~IFF_RUNNING;
	sc->sc_pdq->pdq_flags &= ~PDQ_RUNNING;
	pdq_stop(sc->sc_pdq);
    }
}

void
pdq_ifwatchdog(
    pdq_softc_t *sc)
{
    struct mbuf *m;
    /*
     * No progress was made on the transmit queue for PDQ_OS_TX_TRANSMIT
     * seconds.  Remove all queued packets.
     */

    sc->sc_if.if_flags &= ~IFF_OACTIVE;
    sc->sc_if.if_timer = 0;
    for (;;) {
	IF_DEQUEUE(&sc->sc_if.if_snd, m);
	if (m == NULL)
	    return;
	m_freem(m);
    }
}

ifnet_ret_t
pdq_ifstart(
    struct ifnet *ifp)
{
    pdq_softc_t *sc = (pdq_softc_t *) ((caddr_t) ifp - offsetof(pdq_softc_t, sc_ac.ac_if));
    struct ifqueue *ifq = &ifp->if_snd;
    struct mbuf *m;
    int tx = 0;

    if ((ifp->if_flags & IFF_RUNNING) == 0)
	return;

    if (sc->sc_if.if_timer == 0)
	sc->sc_if.if_timer = PDQ_OS_TX_TIMEOUT;

    if ((sc->sc_pdq->pdq_flags & PDQ_TXOK) == 0) {
	sc->sc_if.if_flags |= IFF_OACTIVE;
	return;
    }
    for (;; tx = 1) {
	IF_DEQUEUE(ifq, m);
	if (m == NULL)
	    break;

	if (pdq_queue_transmit_data(sc->sc_pdq, m) == PDQ_FALSE) {
	    ifp->if_flags |= IFF_OACTIVE;
	    IF_PREPEND(ifq, m);
	    break;
	}
    }
    if (tx)
	PDQ_DO_TYPE2_PRODUCER(sc->sc_pdq);
}

void
pdq_os_receive_pdu(
    pdq_t *pdq,
    struct mbuf *m,
    size_t pktlen)
{
    pdq_softc_t *sc = (pdq_softc_t *) pdq->pdq_os_ctx;
    struct fddi_header *fh = mtod(m, struct fddi_header *);

    sc->sc_if.if_ipackets++;
#if NBPFILTER > 0
    if (sc->sc_bpf != NULL)
	bpf_mtap(sc->sc_bpf, m);
    if ((fh->fddi_fc & (FDDIFC_L|FDDIFC_F)) != FDDIFC_LLC_ASYNC) {
	m_freem(m);
	return;
    }
#endif

    m->m_data += sizeof(struct fddi_header);
    m->m_len  -= sizeof(struct fddi_header);
    m->m_pkthdr.len = pktlen - sizeof(struct fddi_header);
    m->m_pkthdr.rcvif = &sc->sc_if;
    fddi_input(&sc->sc_if, fh, m);
}

void
pdq_os_restart_transmitter(
    pdq_t *pdq)
{
    pdq_softc_t *sc = (pdq_softc_t *) pdq->pdq_os_ctx;
    sc->sc_if.if_flags &= ~IFF_OACTIVE;
    if (sc->sc_if.if_snd.ifq_head != NULL) {
	sc->sc_if.if_timer = PDQ_OS_TX_TIMEOUT;
	pdq_ifstart(&sc->sc_if);
    } else {
	sc->sc_if.if_timer = 0;
    }
}

void
pdq_os_transmit_done(
    pdq_t *pdq,
    struct mbuf *m)
{
    pdq_softc_t *sc = (pdq_softc_t *) pdq->pdq_os_ctx;
#if NBPFILTER > 0
    if (sc->sc_bpf != NULL)
	bpf_mtap(sc->sc_bpf, m);
#endif
    m_freem(m);
    sc->sc_if.if_opackets++;
}

void
pdq_os_addr_fill(
    pdq_t *pdq,
    pdq_lanaddr_t *addr,
    size_t num_addrs)
{
    pdq_softc_t *sc = (pdq_softc_t *) pdq->pdq_os_ctx;
    struct ether_multistep step;
    struct ether_multi *enm;

    ETHER_FIRST_MULTI(step, &sc->sc_ac, enm);
    while (enm != NULL && num_addrs > 0) {
	((u_short *) addr->lanaddr_bytes)[0] = ((u_short *) enm->enm_addrlo)[0];
	((u_short *) addr->lanaddr_bytes)[1] = ((u_short *) enm->enm_addrlo)[1];
	((u_short *) addr->lanaddr_bytes)[2] = ((u_short *) enm->enm_addrlo)[2];
	ETHER_NEXT_MULTI(step, enm);
	addr++;
	num_addrs--;
    }
}

int
pdq_ifioctl(
    struct ifnet *ifp,
    ioctl_cmd_t cmd,
    caddr_t data)
{
    pdq_softc_t *sc = (pdq_softc_t *) ((caddr_t) ifp - offsetof(pdq_softc_t, sc_ac.ac_if));
    int s, error = 0;

    s = splimp();

    switch (cmd) {
	case SIOCSIFADDR: {
	    struct ifaddr *ifa = (struct ifaddr *)data;

	    ifp->if_flags |= IFF_UP;
	    switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET: {
		    sc->sc_ac.ac_ipaddr = IA_SIN(ifa)->sin_addr;
		    pdq_ifinit(sc);
#if !defined(__bsdi__)
		    arp_ifinit(&sc->sc_ac, ifa);
#else
		    arpwhohas(&sc->sc_ac, &IA_SIN(ifa)->sin_addr);
		    ifa->ifa_rtrequest = arp_rtrequest;
		    ifa->ifa_flags |= RTF_CLONING;
#endif
		    break;
		}
#endif /* INET */

#ifdef NS
		/* This magic copied from if_is.c; I don't use XNS,
		 * so I have no way of telling if this actually
		 * works or not.
		 */
		case AF_NS: {
		    struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);
		    if (ns_nullhost(*ina)) {
			ina->x_host = *(union ns_host *)(sc->sc_ac.ac_enaddr);
		    } else {
			ifp->if_flags &= ~IFF_RUNNING;
			bcopy((caddr_t)ina->x_host.c_host,
			      (caddr_t)sc->sc_ac.ac_enaddr,
			      sizeof sc->sc_ac.ac_enaddr);
		    }

		    pdq_ifinit(sc);
		    break;
		}
#endif /* NS */

		default: {
		    pdq_ifinit(sc);
		    break;
		}
	    }
	    break;
	}

	case SIOCSIFFLAGS: {
	    pdq_ifinit(sc);
	    break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI: {
	    /*
	     * Update multicast listeners
	     */
	    if (cmd == SIOCADDMULTI)
		error = ether_addmulti((struct ifreq *)data, &sc->sc_ac);
	    else
		error = ether_delmulti((struct ifreq *)data, &sc->sc_ac);

	    if (error == ENETRESET) {
		if (sc->sc_if.if_flags & IFF_RUNNING)
		    pdq_run(sc->sc_pdq);
		error = 0;
	    }
	    break;
	}

	default: {
	    error = EINVAL;
	    break;
	}
    }

    splx(s);
    return error;
}

void
pdq_ifattach(
    pdq_softc_t *sc,
    pdq_ifinit_t ifinit,
    pdq_ifwatchdog_t ifwatchdog)
{
    struct ifnet *ifp = &sc->sc_if;

    ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_NOTRAILERS|IFF_MULTICAST;

#if !defined(__NetBSD__)
    ifp->if_init = ifinit;
#endif
    ifp->if_watchdog = ifwatchdog;

    ifp->if_ioctl = pdq_ifioctl;
    ifp->if_output = fddi_output;
    ifp->if_start = pdq_ifstart;
  
    if_attach(ifp);
    fddi_ifattach(ifp);
#if NBPFILTER > 0
    bpfattach(&sc->sc_bpf, ifp, DLT_FDDI, sizeof(struct fddi_header));
#endif
}
