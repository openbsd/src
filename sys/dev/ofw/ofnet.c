/*	$OpenBSD: ofnet.c,v 1.9 2004/11/29 12:50:05 jsg Exp $	*/
/*	$NetBSD: ofnet.c,v 1.4 1996/10/16 19:33:21 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "ofnet.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/ofw/openfirm.h>

#if NIPKDB_OFN > 0
#include <ipkdb/ipkdb.h>
#include <machine/ipkdb.h>

struct cfattach ipkdb_ofn_ca = {
	0, ipkdb_probe, ipkdb_attach
};

static struct ipkdb_if *kifp;
static struct ofn_softc *ipkdb_of;

static int ipkdbprobe(void *, void *);
#endif

struct ofn_softc {
	struct device sc_dev;
	int sc_phandle;
	int sc_ihandle;
	struct arpcom sc_arpcom;
	struct timeout sc_tmo;
	void *dmabuf;
};

static int ofnprobe(struct device *, void *, void *);
static void ofnattach(struct device *, struct device *, void *);

struct cfattach ofnet_ca = {
	sizeof(struct ofn_softc), ofnprobe, ofnattach
};

struct cfdriver ofnet_cd = {
	NULL, "ofnet", DV_IFNET
};

static void ofnread(struct ofn_softc *);
static void ofntimer(struct ofn_softc *);
static void ofninit(struct ofn_softc *);
static void ofnstop(struct ofn_softc *);

static void ofnstart(struct ifnet *);
static int ofnioctl(struct ifnet *, u_long, caddr_t);
static void ofnwatchdog(struct ifnet *);

static int
ofnprobe(struct device *parent, void *match, void *aux)
{
	struct ofprobe *ofp = aux;
	char type[32];
	int l;
	
#if NIPKDB_OFN > 0
	if (!parent)
		return ipkdbprobe(match, aux);
#endif
	if ((l = OF_getprop(ofp->phandle, "device_type", type, sizeof type - 1)) < 0)
		return 0;
	if (l >= sizeof type)
		return 0;
	type[l] = 0;
	if (strcmp(type, "network"))
		return 0;
	return 1;
}

static void
ofnattach(struct device *parent, struct device *self, void *aux)
{
	struct ofn_softc *of = (void *)self;
	struct ifnet *ifp = &of->sc_arpcom.ac_if;
	struct ofprobe *ofp = aux;
	char path[256];
	int l;
	
	of->sc_phandle = ofp->phandle;
#if NIPKDB_OFN > 0
	if (kifp
	    && kifp->unit - 1 == of->sc_dev.dv_unit
	    && OF_instance_to_package(kifp->port) == ofp->phandle)  {
		ipkdb_of = of;
		of->sc_ihandle = kifp->port;
	} else
#endif
	if ((l = OF_package_to_path(ofp->phandle, path, sizeof path - 1)) < 0
	    || l >= sizeof path
	    || (path[l] = 0, !(of->sc_ihandle = OF_open(path))))
		panic("ofnattach: unable to open");
printf("\nethernet dev: path %s\n", path);
	OF_call_method("dma-alloc", of->sc_ihandle, 1, 1, MAXPHYS,
		&(of->dmabuf));
	if (OF_getprop(ofp->phandle, "mac-address",
		       of->sc_arpcom.ac_enaddr, sizeof
			       (of->sc_arpcom.ac_enaddr)) < 0)
	{
		if (OF_getprop(ofp->phandle, "local-mac-address",
			       of->sc_arpcom.ac_enaddr, sizeof
				       (of->sc_arpcom.ac_enaddr)) < 0)
		{
			panic("ofnattach: no mac-address");
		}
	}
	printf(": address %s\n", ether_sprintf(of->sc_arpcom.ac_enaddr));
	
	bcopy(of->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = of;
	ifp->if_start = ofnstart;
	ifp->if_ioctl = ofnioctl;
	ifp->if_watchdog = ofnwatchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	IFQ_SET_READY(&ifp->if_snd);

	timeout_set(&of->sc_tmo, ofntimer, of);

	if_attach(ifp);
	ether_ifattach(ifp);

	dk_establish(0, self);					/* XXX */
}

static void
ofnread(struct ofn_softc *of)
{
	struct ifnet *ifp = &of->sc_arpcom.ac_if;
	struct mbuf *m, **mp, *head;
	int l, len;
	char *bufp;
	char *buf = of->dmabuf;

#if NIPKDB_OFN > 0
	ipkdbrint(kifp, ifp);
#endif	
	while (1) {
		if ((len = OF_read(of->sc_ihandle, buf, sizeof buf)) < 0) {
			if (len == -2)
				return;
			ifp->if_ierrors++;
			continue;
		}
		if (len < sizeof(struct ether_header)) {
			ifp->if_ierrors++;
			continue;
		}
		bufp = buf;
		
		/* Allocate a header mbuf */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0) {
			ifp->if_ierrors++;
			continue;
		}
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = len;
		l = MHLEN;
		head = 0;
		mp = &head;
		
		while (len > 0) {
			if (head) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == 0) {
					ifp->if_ierrors++;
					m_freem(head);
					head = 0;
					break;
				}
				l = MLEN;
			}
			if (len >= MINCLSIZE) {
				MCLGET(m, M_DONTWAIT);
				if (m->m_flags & M_EXT)
					l = MCLBYTES;
			}
			m->m_len = l = min(len, l);
			bcopy(bufp, mtod(m, char *), l);
			bufp += l;
			len -= l;
			*mp = m;
			mp = &m->m_next;
		}
		if (head == 0)
			continue;

#if	NBPFILTER > 0
		if (ifp->if_bpf) {
			bpf->mtap(ifp->if_bpf, m);
#endif
		ifp->if_ipackets++;
		ether_input_mbuf(ifp, head);
	}
}

static void
ofntimer(struct ofn_softc *of)
{
	ofnread(of);
	timeout_add(&of->sc_tmo, 1);
}

static void
ofninit(struct ofn_softc *of)
{
	struct ifnet *ifp = &of->sc_arpcom.ac_if;

	if (ifp->if_flags & IFF_RUNNING)
		return;

	ifp->if_flags |= IFF_RUNNING;
	/* Start reading from interface */
	ofntimer(of);
	/* Attempt to start output */
	ofnstart(ifp);
}

static void
ofnstop(struct ofn_softc *of)
{
	timeout_del(&of->sc_tmo);
	of->sc_arpcom.ac_if.if_flags &= ~IFF_RUNNING;
}

static void
ofnstart(struct ifnet *ifp)
{
	struct ofn_softc *of = ifp->if_softc;
	struct mbuf *m, *m0;
	char *bufp;
	char *buf;
	int len;
	buf = of->dmabuf;
	
	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	for (;;) {
		/* First try reading any packets */
		ofnread(of);
		
		/* Now get the first packet on the queue */
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (!m0)
			return;
		
		if (!(m0->m_flags & M_PKTHDR))
			panic("ofnstart: no header mbuf");
		len = m0->m_pkthdr.len;
		
		if (len > ETHERMTU + sizeof(struct ether_header)) {
			/* packet too large, toss it */
			ifp->if_oerrors++;
			m_freem(m0);
			continue;
		}

#if NPBFILTER > 0
		if (ifp->if_bpf)
			bpf_mtab(ifp->if_bpf, m0);
#endif
		for (bufp = buf; m = m0;) {
			bcopy(mtod(m, char *), bufp, m->m_len);
			bufp += m->m_len;
			MFREE(m, m0);
		}
		if (OF_write(of->sc_ihandle, buf, bufp - buf) != bufp - buf)
			ifp->if_oerrors++;
		else
			ifp->if_opackets++;
	}
}

static int
ofnioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ofn_softc *of = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;
	
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		
		switch (ifa->ifa_addr->sa_family) {
#ifdef	INET
		case AF_INET:
			arp_ifinit(&of->sc_arpcom, ifa);
			break;
#endif
		default:
			break;
		}
		ofninit(of);
		break;
	case SIOCSIFFLAGS:
		if (!(ifp->if_flags & IFF_UP)
		    && (ifp->if_flags & IFF_RUNNING)) {
			/* If interface is down, but running, stop it. */
			ofnstop(of);
		} else if ((ifp->if_flags & IFF_UP)
			   && !(ifp->if_flags & IFF_RUNNING)) {
			/* If interface is up, but not running, start it. */
			ofninit(of);
		} else {
			/* Other flags are ignored. */
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

static void
ofnwatchdog(struct ifnet *ifp)
{
	struct ofn_softc *of = ifp->if_softc;
	
	log(LOG_ERR, "%s: device timeout\n", of->sc_dev.dv_xname);
	of->sc_arpcom.ac_if.if_oerrors++;
	ofnstop(of);
	ofninit(of);
}

#if NIPKDB_OFN > 0
/* has not been updated to use dmabuf */
static void
ipkdbofstart(struct ipkdb_if *kip)
{
	int unit = kip->unit - 1;
	
	if (ipkdb_of)
		ipkdbattach(kip, &ipkdb_of->sc_arpcom);
}

static void
ipkdbofleave(struct ipkdb_if *kip)
{
}

static int
ipkdbofrcv(struct ipkdb_if *kip, u_char *buf, int poll)
{
	int l;
	
	do {
		l = OF_read(kip->port, buf, ETHERMTU);
		if (l < 0)
			l = 0;
	} while (!poll && !l);
	return l;
}

static void
ipkdbofsend(struct ipkdb_if *kip, u_char *buf, int l)
{
	OF_write(kip->port, buf, l);
}

static int
ipkdbprobe(void *match, void *aux)
{
	struct cfdata *cf = match;
	struct ipkdb_if *kip = aux;
	static char name[256];
	int len;
	int phandle;
	
	kip->unit = cf->cf_unit + 1;

	if (!(kip->port = OF_open("net")))
		return -1;
	if ((len = OF_instance_to_path(kip->port, name, sizeof name - 1)) < 0
	    || len >= sizeof name)
		return -1;
	name[len] = 0;
	if ((phandle = OF_instance_to_package(kip->port)) == -1)
		return -1;
	if ( OF_getprop(phandle, "local-mac-address", kip->myenetaddr, sizeof kip->myenetaddr) &&
	OF_getprop(phandle, "mac-address", kip->myenetaddr, sizeof kip->myenetaddr)
	    < 0)
		return -1;
	
	kip->flags |= IPKDB_MYHW;
	kip->name = name;
	kip->start = ipkdbofstart;
	kip->leave = ipkdbofleave;
	kip->receive = ipkdbofrcv;
	kip->send = ipkdbofsend;

	kifp = kip;
	
	return 0;
}
#endif
