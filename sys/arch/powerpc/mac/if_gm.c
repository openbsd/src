/*	$NetBSD: if_gm.c,v 1.2 2000/03/04 11:17:00 tsubai Exp $	*/

/*-
 * Copyright (c) 2000 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __NetBSD__
#include "opt_inet.h"
#include "opt_ns.h"
#include "bpfilter.h"
#endif /* __NetBSD__ */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <vm/vm.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#ifdef __NetBSD__
#include <netinet/if_inarp.h>
#endif /* __NetBSD__ */
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ofw/openfirm.h>
#include <powerpc/mac/if_gmreg.h>
#include <machine/pio.h>

#define NTXBUF 4
#define NRXBUF 32

struct gmac_softc {
	struct device sc_dev;
#ifdef __OpenBSD__
	struct arpcom arpcom;	/* per-instance network data */
#define sc_if arpcom.ac_if
#define	sc_enaddr arpcom.ac_enaddr
#else
	struct ethercom sc_ethercom;
#define sc_if sc_ethercom.ec_if
	char sc_laddr[6];
#endif
	vaddr_t sc_reg;
	struct gmac_dma *sc_txlist;
	struct gmac_dma *sc_rxlist;
	int sc_txnext;
	int sc_rxlast;
	caddr_t sc_txbuf[NTXBUF];
	caddr_t sc_rxbuf[NRXBUF];
	struct mii_data sc_mii;
};


int gmac_match __P((struct device *, void *, void *));
void gmac_attach __P((struct device *, struct device *, void *));

static __inline u_int gmac_read_reg __P((struct gmac_softc *, int));
static __inline void gmac_write_reg __P((struct gmac_softc *, int, u_int));

static __inline void gmac_start_txdma __P((struct gmac_softc *));
static __inline void gmac_start_rxdma __P((struct gmac_softc *));
static __inline void gmac_stop_txdma __P((struct gmac_softc *));
static __inline void gmac_stop_rxdma __P((struct gmac_softc *));

int gmac_intr __P((void *));
void gmac_tint __P((struct gmac_softc *));
void gmac_rint __P((struct gmac_softc *));
struct mbuf * gmac_get __P((struct gmac_softc *, caddr_t, int));
void gmac_start __P((struct ifnet *));
int gmac_put __P((struct gmac_softc *, caddr_t, struct mbuf *));

void gmac_stop __P((struct gmac_softc *));
void gmac_reset __P((struct gmac_softc *));
void gmac_init __P((struct gmac_softc *));
void gmac_init_mac __P((struct gmac_softc *));

int gmac_ioctl __P((struct ifnet *, u_long, caddr_t));
void gmac_watchdog __P((struct ifnet *));

int gmac_mediachange __P((struct ifnet *));
void gmac_mediastatus __P((struct ifnet *, struct ifmediareq *));
int gmac_mii_readreg __P((struct device *, int, int));
void gmac_mii_writereg __P((struct device *, int, int, int));
void gmac_mii_statchg __P((struct device *));
void gmac_mii_tick __P((void *));

struct cfattach gm_ca = {
	sizeof(struct gmac_softc), gmac_match, gmac_attach
};
struct cfdriver gm_cd = {
	NULL, "gm", DV_IFNET
};

int
gmac_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_APPLE &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_APPLE_GMAC)
		return 1;

	return 0;
}

void
gmac_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct gmac_softc *sc = (void *)self;
	struct pci_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_if;
	struct mii_data *mii = &sc->sc_mii;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
#if 0
	int node;
#endif
	int i;
	char *p;
	struct gmac_dma *dp;
	u_int32_t reg[10];
	u_char laddr[6];

#if 0
	node = pcidev_to_ofdev(pa->pa_pc, pa->pa_tag);
	if (node == 0) {
		printf(": cannot find gmac node\n");
		return;
	}

	OF_getprop(node, "local-mac-address", laddr, sizeof laddr);
	OF_getprop(node, "assigned-addresses", reg, sizeof reg);
	#endif

#ifdef __OpenBSD__
	bcopy(laddr, sc->arpcom.ac_enaddr, 6);
#else /* !__OpenBSD */
	bcopy(laddr, sc->sc_laddr, sizeof laddr);
#endif /* !__OpenBSD */
	sc->sc_reg = reg[2];

#ifdef __NetBSD__
	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": unable to map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);

	if (pci_intr_establish(pa->pa_pc, ih, IPL_NET, gmac_intr, sc) == NULL) {
		printf(": unable to establish interrupt");
		if (intrstr)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
	if (pci_intr_establish(pa->pa_pc, pa->pa_intrline, IPL_NET,
		gmac_intr, sc, "gmac") == NULL)
	{
		printf(": unable to establish interrupt");
		if (intrstr)
			printf(" at %x", pa->pa_intrline);
		printf("\n");
		return;
	}
#endif /* __OpenBSD__ */

	/* Setup packet buffers and dma descriptors. */
	p = malloc((NRXBUF + NTXBUF) * 2048 + 3 * 0x800, M_DEVBUF, M_NOWAIT);
	if (p == NULL) {
		printf(": cannot malloc buffers\n");
		return;
	}
	p = (void *)roundup((vaddr_t)p, 0x800);
	bzero(p, 2048 * (NRXBUF + NTXBUF) + 2 * 0x800);

	sc->sc_rxlist = (void *)p;
	p += 0x800;
	sc->sc_txlist = (void *)p;
	p += 0x800;

	dp = sc->sc_rxlist;
	for (i = 0; i < NRXBUF; i++) {
		sc->sc_rxbuf[i] = p;
		dp->address = htole32(vtophys(p));
		dp->cmd = htole32(GMAC_OWN);
		dp++;
		p += 2048;
	}

	dp = sc->sc_txlist;
	for (i = 0; i < NTXBUF; i++) {
		sc->sc_txbuf[i] = p;
		dp->address = htole32(vtophys(p));
		dp++;
		p += 2048;
	}
#ifdef __OpenBSD__
	{
		/* rather than call openfirmware, expect that ethernet
		 * is already intialized, read the address
		 * from the device -- hack?
		 */
		u_int reg;
		reg = gmac_read_reg(sc, GMAC_MACADDRESS0);
		laddr[5] = reg & 0xff;
		laddr[4] = (reg >> 8) & 0xff;
		reg = gmac_read_reg(sc, GMAC_MACADDRESS1);
		laddr[3] = reg & 0xff;
		laddr[2] = (reg >> 8) & 0xff;
		reg = gmac_read_reg(sc, GMAC_MACADDRESS2);
		laddr[1] = reg & 0xff;
		laddr[0] = (reg >> 8) & 0xff;
	}
#endif /* __OpenBSD__ */

	printf(": Ethernet address %s\n", ether_sprintf(laddr));
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	gmac_reset(sc);
	gmac_init_mac(sc);

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_ioctl = gmac_ioctl;
	ifp->if_start = gmac_start;
	ifp->if_watchdog = gmac_watchdog;
	ifp->if_flags =
		IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	ifp->if_flags |= IFF_ALLMULTI;

	mii->mii_ifp = ifp;
	mii->mii_readreg = gmac_mii_readreg;
	mii->mii_writereg = gmac_mii_writereg;
	mii->mii_statchg = gmac_mii_statchg;

	ifmedia_init(&mii->mii_media, 0, gmac_mediachange, gmac_mediastatus);
#ifdef __NetBSD__
	mii_attach(self, mii, 0xffffffff, MII_PHY_ANY, MII_OFFSET_ANY, 0);
#endif /* __NetBSD__ */

	/* Choose a default media. */
	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER|IFM_AUTO);

	if_attach(ifp);
#ifdef __NetBSD__
	ether_ifattach(ifp, laddr);
#else /* !__NetBSD__ */
	ether_ifattach(ifp);
#endif /* !__NetBSD__ */

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
}

u_int
gmac_read_reg(sc, reg)
	struct gmac_softc *sc;
	int reg;
{
	return in32rb(sc->sc_reg + reg);
}

void
gmac_write_reg(sc, reg, val)
	struct gmac_softc *sc;
	int reg;
	u_int val;
{
	out32rb(sc->sc_reg + reg, val);
}

void
gmac_start_txdma(sc)
	struct gmac_softc *sc;
{
	u_int x;

	x = gmac_read_reg(sc, GMAC_TXDMACONFIG);
	x |= 1;
	gmac_write_reg(sc, GMAC_TXDMACONFIG, x);
	x = gmac_read_reg(sc, GMAC_TXMACCONFIG);
	x |= 1;
	gmac_write_reg(sc, GMAC_TXMACCONFIG, x);
}

void
gmac_start_rxdma(sc)
	struct gmac_softc *sc;
{
	u_int x;

	x = gmac_read_reg(sc, GMAC_RXDMACONFIG);
	x |= 1;
	gmac_write_reg(sc, GMAC_RXDMACONFIG, x);
	x = gmac_read_reg(sc, GMAC_RXMACCONFIG);
	x |= 1;
	gmac_write_reg(sc, GMAC_RXMACCONFIG, x);
}

void
gmac_stop_txdma(sc)
	struct gmac_softc *sc;
{
	u_int x;

	x = gmac_read_reg(sc, GMAC_TXDMACONFIG);
	x &= ~1;
	gmac_write_reg(sc, GMAC_TXDMACONFIG, x);
	x = gmac_read_reg(sc, GMAC_TXMACCONFIG);
	x &= ~1;
	gmac_write_reg(sc, GMAC_TXMACCONFIG, x);
}

void
gmac_stop_rxdma(sc)
	struct gmac_softc *sc;
{
	u_int x;

	x = gmac_read_reg(sc, GMAC_RXDMACONFIG);
	x &= ~1;
	gmac_write_reg(sc, GMAC_RXDMACONFIG, x);
	x = gmac_read_reg(sc, GMAC_RXMACCONFIG);
	x &= ~1;
	gmac_write_reg(sc, GMAC_RXMACCONFIG, x);
}

int
gmac_intr(v)
	void *v;
{
	struct gmac_softc *sc = v;
	u_int status;

	status = gmac_read_reg(sc, GMAC_STATUS) & 0xff;
	if (status == 0)
		return 0;

	if (status & GMAC_INT_RXDONE)
		gmac_rint(sc);

	if (status & GMAC_INT_TXDONE)
		gmac_tint(sc);

	return 1;
}

void
gmac_tint(sc)
	struct gmac_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	volatile struct gmac_dma *dp;
	int i;

	i = gmac_read_reg(sc, GMAC_TXDMACOMPLETE);
	dp = &sc->sc_txlist[i];
	dp->cmd = 0;				/* to be safe */
	__asm __volatile ("sync");

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;
	ifp->if_opackets++;
	gmac_start(ifp);
}

void
gmac_rint(sc)
	struct gmac_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	volatile struct gmac_dma *dp;
	struct mbuf *m;
	int i, len;
	u_int cmd;

	for (i = sc->sc_rxlast;; i++) {
		if (i == NRXBUF)
			i = 0;

		dp = &sc->sc_rxlist[i];
#ifdef __OpenBSD__
		cmd = letoh32(dp->cmd);
#else /* !__OpenBSD__ */
		cmd = le32toh(dp->cmd);
#endif /* !__OpenBSD__ */
		if (cmd & GMAC_OWN)
			break;
		len = (cmd >> 16) & GMAC_LEN_MASK;
		len -= 4;	/* CRC */

#ifdef __OpenBSD__
		if (letoh32(dp->cmd_hi) & 0x40000000) {
#else /* !__OpenBSD__ */
		if (le32toh(dp->cmd_hi) & 0x40000000) {
#endif /* !__OpenBSD__ */
			ifp->if_ierrors++;
			goto next;
		}

		m = gmac_get(sc, sc->sc_rxbuf[i], len);
		if (m == NULL) {
			ifp->if_ierrors++;
			goto next;
		}

#if NBPFILTER > 0
		/*
		 * Check if there's a BPF listener on this interface.
		 * If so, hand off the raw packet to BPF.
		 */
		if (ifp->if_bpf)
			bpf_tap(ifp->if_bpf, sc->sc_rxbuf[i], len);
#endif
#ifdef __OpenBSD__
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp,(void*) sc->sc_rxbuf[i], m);
#else /* !__OpenBSD__ */
		(*ifp->if_input)(ifp, m);
#endif /* !__OpenBSD__ */
		ifp->if_ipackets++;

next:
		dp->cmd_hi = 0;
		__asm __volatile ("sync");
		dp->cmd = htole32(GMAC_OWN);
	}
	sc->sc_rxlast = i;
}

struct mbuf *
gmac_get(sc, pkt, totlen)
	struct gmac_softc *sc;
	caddr_t pkt;
	int totlen;
{
	struct mbuf *m;
	struct mbuf *top, **mp;
	int len;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return 0;
	m->m_pkthdr.rcvif = &sc->sc_if;
	m->m_pkthdr.len = totlen;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return 0;
			}
			len = MLEN;
		}
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				m_freem(top);
				return 0;
			}
			len = MCLBYTES;
		}
		m->m_len = len = min(totlen, len);
		bcopy(pkt, mtod(m, caddr_t), len);
		pkt += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return top;
}

void
gmac_start(ifp)
	struct ifnet *ifp;
{
	struct gmac_softc *sc = ifp->if_softc;
	struct mbuf *m;
	caddr_t buff;
	int i, tlen;
	volatile struct gmac_dma *dp;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		if (ifp->if_flags & IFF_OACTIVE)
			break;

		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;

		ifp->if_flags |= IFF_OACTIVE;

		/* 5 seconds to watch for failing to transmit */
		ifp->if_timer = 5;
		ifp->if_opackets++;		/* # of pkts */

		i = sc->sc_txnext;
		buff = sc->sc_txbuf[i];
		tlen = gmac_put(sc, buff, m);

		dp = &sc->sc_txlist[i];
		dp->cmd_hi = 0;
		dp->address_hi = 0;
		dp->cmd = htole32(tlen | GMAC_OWN | GMAC_SOP);

		i++;
		if (i == NTXBUF)
			i = 0;
		__asm __volatile ("sync");

		gmac_write_reg(sc, GMAC_TXDMAKICK, i);
		sc->sc_txnext = i;

#if NBPFILTER > 0
		/*
		 * If BPF is listening on this interface, let it see the
		 * packet before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_tap(ifp->if_bpf, buff, tlen);
#endif
	}
}

int
gmac_put(sc, buff, m)
	struct gmac_softc *sc;
	caddr_t buff;
	struct mbuf *m;
{
	struct mbuf *n;
	int len, tlen = 0;

	for (; m; m = n) {
		len = m->m_len;
		if (len == 0) {
			MFREE(m, n);
			continue;
		}
		bcopy(mtod(m, caddr_t), buff, len);
		buff += len;
		tlen += len;
		MFREE(m, n);
	}
	if (tlen > 2048)
		panic("%s: gmac_put packet overflow", sc->sc_dev.dv_xname);

	return tlen;
}

void
gmac_reset(sc)
	struct gmac_softc *sc;
{
	int i, s;

	s = splnet();

	gmac_stop_txdma(sc);
	gmac_stop_rxdma(sc);

	gmac_write_reg(sc, GMAC_SOFTWARERESET, 3);
	for (i = 10; i > 0; i--) {
		delay(300000);				/* XXX long delay */
		if ((gmac_read_reg(sc, GMAC_SOFTWARERESET) & 3) == 0)
			break;
	}
	if (i == 0)
		printf("%s: reset timeout\n", sc->sc_dev.dv_xname);

	sc->sc_txnext = 0;
	sc->sc_rxlast = 0;
	for (i = 0; i < NRXBUF; i++)
		sc->sc_rxlist[i].cmd = htole32(GMAC_OWN);
	__asm __volatile ("sync");

	gmac_write_reg(sc, GMAC_TXDMADESCBASEHI, 0);
	gmac_write_reg(sc, GMAC_TXDMADESCBASELO, vtophys(sc->sc_txlist));
	gmac_write_reg(sc, GMAC_RXDMADESCBASEHI, 0);
	gmac_write_reg(sc, GMAC_RXDMADESCBASELO, vtophys(sc->sc_rxlist));
	gmac_write_reg(sc, GMAC_RXDMAKICK, NRXBUF);

	splx(s);
}

void
gmac_stop(sc)
	struct gmac_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	int s;

	s = splnet();

	untimeout(gmac_mii_tick, sc);
#ifndef __OenBSD__
	mii_down(&sc->sc_mii);
#endif

	gmac_stop_txdma(sc);
	gmac_stop_rxdma(sc);

	gmac_write_reg(sc, GMAC_INTMASK, 0xffffffff);

	ifp->if_flags &= ~(IFF_UP | IFF_RUNNING);
	ifp->if_timer = 0;

	splx(s);
}

void
gmac_init_mac(sc)
	struct gmac_softc *sc;
{
	int i, tb;
#ifdef __NetBSD__
	char *laddr = sc->sc_laddr;
#else /* !__NetBSD__ */
	char *laddr = sc->sc_enaddr;
#endif

	__asm ("mftb %0" : "=r"(tb));
	gmac_write_reg(sc, GMAC_RANDOMSEED, tb);

	/* init-mii */
	gmac_write_reg(sc, GMAC_DATAPATHMODE, 4);
	gmac_mii_writereg(&sc->sc_dev, 0, 0, 0x1000);

	gmac_write_reg(sc, GMAC_TXDMACONFIG, 0xffc00);
	gmac_write_reg(sc, GMAC_RXDMACONFIG, 0);
	gmac_write_reg(sc, GMAC_MACPAUSE, 0x1bf0);
	gmac_write_reg(sc, GMAC_INTERPACKETGAP0, 0);
	gmac_write_reg(sc, GMAC_INTERPACKETGAP1, 8);
	gmac_write_reg(sc, GMAC_INTERPACKETGAP2, 4);
	gmac_write_reg(sc, GMAC_MINFRAMESIZE, ETHER_MIN_LEN);
	gmac_write_reg(sc, GMAC_MAXFRAMESIZE, ETHER_MAX_LEN);
	gmac_write_reg(sc, GMAC_PASIZE, 7);
	gmac_write_reg(sc, GMAC_JAMSIZE, 4);
	gmac_write_reg(sc, GMAC_ATTEMPTLIMIT,0x10);
	gmac_write_reg(sc, GMAC_MACCNTLTYPE, 0x8808);

	gmac_write_reg(sc, GMAC_MACADDRESS0, (laddr[4] << 8) | laddr[5]);
	gmac_write_reg(sc, GMAC_MACADDRESS1, (laddr[2] << 8) | laddr[3]);
	gmac_write_reg(sc, GMAC_MACADDRESS2, (laddr[0] << 8) | laddr[1]);
	gmac_write_reg(sc, GMAC_MACADDRESS3, 0);
	gmac_write_reg(sc, GMAC_MACADDRESS4, 0);
	gmac_write_reg(sc, GMAC_MACADDRESS5, 0);
	gmac_write_reg(sc, GMAC_MACADDRESS6, 1);
	gmac_write_reg(sc, GMAC_MACADDRESS7, 0xc200);
	gmac_write_reg(sc, GMAC_MACADDRESS8, 0x0180);
	gmac_write_reg(sc, GMAC_MACADDRFILT0, 0);
	gmac_write_reg(sc, GMAC_MACADDRFILT1, 0);
	gmac_write_reg(sc, GMAC_MACADDRFILT2, 0);
	gmac_write_reg(sc, GMAC_MACADDRFILT2_1MASK, 0);
	gmac_write_reg(sc, GMAC_MACADDRFILT0MASK, 0);

	for (i = 0; i < 0x6c; i+= 4)
		gmac_write_reg(sc, GMAC_HASHTABLE0 + i, 0);

	gmac_write_reg(sc, GMAC_SLOTTIME, 0x40);

	/* XXX */
	gmac_write_reg(sc, GMAC_TXMACCONFIG, 0);
	gmac_write_reg(sc, GMAC_XIFCONFIG, 5);
	gmac_write_reg(sc, GMAC_MACCTRLCONFIG, 0);
}

void
gmac_init(sc)
	struct gmac_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	u_int x;
	int i;

	gmac_stop_txdma(sc);
	gmac_stop_rxdma(sc);

	gmac_init_mac(sc);

	x = gmac_read_reg(sc, GMAC_RXMACCONFIG);
	if (ifp->if_flags & IFF_PROMISC)
		x |= GMAC_RXMAC_PR;
	else
		x &= ~GMAC_RXMAC_PR;
	gmac_write_reg(sc, GMAC_RXMACCONFIG, x);

	gmac_start_txdma(sc);
	gmac_start_rxdma(sc);

	gmac_write_reg(sc, GMAC_INTMASK, ~(GMAC_INT_TXDONE | GMAC_INT_RXDONE));

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;

	untimeout(gmac_mii_tick, sc);
	timeout(gmac_mii_tick, sc, 1);

	gmac_start(ifp);
}

int
gmac_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct gmac_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			gmac_init(sc);
#ifdef __OpenBSD__
			arp_ifinit(&sc->arpcom, ifa);
#else /* !__OpenBSD__ */
			arp_ifinit(ifp, ifa);
#endif /* !__OpenBSD__ */
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)LLADDR(ifp->if_sadl);
			else {
				bcopy(ina->x_host.c_host,
				    LLADDR(ifp->if_sadl),
				    sizeof(sc->sc_enaddr));
			}
			/* Set new address. */
			gmac_init(sc);
			break;
		    }
#endif
		default:
			gmac_init(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			gmac_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			gmac_init(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			gmac_reset(sc);
			gmac_init(sc);
		}
#ifdef GMAC_DEBUG
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_flags |= GMAC_DEBUGFLAG;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
#if defined(__OpenBSD__)
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->arpcom) :
		    ether_delmulti(ifr, &sc->arpcom);
#else
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ethercom) :
		    ether_delmulti(ifr, &sc->sc_ethercom);
#endif

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			gmac_init(sc);
			/* gmac_setladrf(sc); */
			error = 0;
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;

	default:
		error = EINVAL;
	}

	splx(s);
	return error;
}

void
gmac_watchdog(ifp)
	struct ifnet *ifp;
{
	struct gmac_softc *sc = ifp->if_softc;

	printf("%s: device timeout\n", ifp->if_xname);
	ifp->if_oerrors++;

	gmac_reset(sc);
	gmac_init(sc);
}

int
gmac_mediachange(ifp)
	struct ifnet *ifp;
{
	struct gmac_softc *sc = ifp->if_softc;

	return mii_mediachg(&sc->sc_mii);
}

void
gmac_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct gmac_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);

	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}

int
gmac_mii_readreg(dev, phy, reg)
	struct device *dev;
	int phy, reg;
{
	struct gmac_softc *sc = (void *)dev;
	int i;

	gmac_write_reg(sc, GMAC_MIFFRAMEOUTPUT,
		0x60020000 | (phy << 23) | (reg << 18));

	for (i = 1000; i >= 0; i -= 10) {
		if (gmac_read_reg(sc, GMAC_MIFFRAMEOUTPUT) & 0x10000)
			break;
		delay(10);
	}
	if (i < 0) {
		printf("%s: gmac_mii_readreg: timeout\n", sc->sc_dev.dv_xname);
		return 0;
	}

	return gmac_read_reg(sc, GMAC_MIFFRAMEOUTPUT) & 0xffff;
}

void
gmac_mii_writereg(dev, phy, reg, val)
	struct device *dev;
	int phy, reg, val;
{
	struct gmac_softc *sc = (void *)dev;
	int i;

	gmac_write_reg(sc, GMAC_MIFFRAMEOUTPUT,
		0x50020000 | (phy << 23) | (reg << 18) | (val & 0xffff));

	for (i = 1000; i >= 0; i -= 10) {
		if (gmac_read_reg(sc, GMAC_MIFFRAMEOUTPUT) & 0x10000)
			break;
		delay(10);
	}
	if (i < 0)
		printf("%s: gmac_mii_writereg: timeout\n", sc->sc_dev.dv_xname);
}

void
gmac_mii_statchg(dev)
	struct device *dev;
{
	struct gmac_softc *sc = (void *)dev;

	gmac_stop_txdma(sc);
	gmac_stop_rxdma(sc);

	if (IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) {
		gmac_write_reg(sc, GMAC_TXMACCONFIG, 6);
		gmac_write_reg(sc, GMAC_XIFCONFIG, 1);
	} else {
		gmac_write_reg(sc, GMAC_TXMACCONFIG, 0);
		gmac_write_reg(sc, GMAC_XIFCONFIG, 5);
	}

	if (0)	/* g-bit? */
		gmac_write_reg(sc, GMAC_MACCTRLCONFIG, 3);
	else
		gmac_write_reg(sc, GMAC_MACCTRLCONFIG, 0);

	gmac_start_txdma(sc);
	gmac_start_rxdma(sc);
}

void
gmac_mii_tick(v)
	void *v;
{
	struct gmac_softc *sc = v;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout(gmac_mii_tick, sc, hz);
}
