/*	$OpenBSD: if_bm.c,v 1.4 2000/03/25 04:36:56 rahnds Exp $	*/
/*	$NetBSD: if_bm.c,v 1.6 2000/02/02 17:09:43 thorpej Exp $	*/

/*-
 * Copyright (C) 1998, 1999, 2000 Tsubai Masanari.  All rights reserved.
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
#endif /* __NetBSD__ */
#include "bpfilter.h"

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

#include <dev/ofw/openfirm.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#ifdef __NetBSD__
#include <dev/mii/mii_bitbang.h>
#endif /* __NetBSD__ */

#include <machine/autoconf.h>
#include <machine/pio.h>

#include <powerpc/mac/dbdma.h>
#include <powerpc/mac/if_bmreg.h>

#define BMAC_TXBUFS 2
#define BMAC_RXBUFS 16
#define BMAC_BUFLEN 2048

struct bmac_softc {
	struct device sc_dev;
#ifdef __OpenBSD__
	struct arpcom arpcom;	/* per-instance network data */
#define sc_if arpcom.ac_if
#define	sc_enaddr arpcom.ac_enaddr
#else
	struct ethercom sc_ethercom;
#define sc_if sc_ethercom.ec_if
	u_char sc_enaddr[6];
#endif
	vaddr_t sc_regs;
	dbdma_regmap_t *sc_txdma;
	dbdma_regmap_t *sc_rxdma;
	dbdma_command_t *sc_txcmd;
	dbdma_command_t *sc_rxcmd;
	caddr_t sc_txbuf;
	caddr_t sc_rxbuf;
	int sc_rxlast;
	int sc_flags;
	struct mii_data sc_mii;
	int txcnt_outstanding;
};

#define BMAC_BMACPLUS	0x01
#define BMAC_DEBUGFLAG	0x02

extern u_int *heathrow_FCR;

static __inline int bmac_read_reg __P((struct bmac_softc *, int));
static __inline void bmac_write_reg __P((struct bmac_softc *, int, int));
static __inline void bmac_set_bits __P((struct bmac_softc *, int, int));
static __inline void bmac_reset_bits __P((struct bmac_softc *, int, int));

int bmac_match __P((struct device *, void *, void *));
void bmac_attach __P((struct device *, struct device *, void *));
void bmac_reset_chip __P((struct bmac_softc *));
void bmac_init __P((struct bmac_softc *));
void bmac_init_dma __P((struct bmac_softc *));
int bmac_intr __P((void *));
int bmac_tx_intr __P((void *));
int bmac_rint __P((void *));
void bmac_reset __P((struct bmac_softc *));
void bmac_stop __P((struct bmac_softc *));
void bmac_start __P((struct ifnet *));
void bmac_transmit_packet __P((struct bmac_softc *, void *, int));
int bmac_put __P((struct bmac_softc *, caddr_t, struct mbuf *));
struct mbuf *bmac_get __P((struct bmac_softc *, caddr_t, int));
void bmac_watchdog __P((struct ifnet *));
int bmac_ioctl __P((struct ifnet *, u_long, caddr_t));
int bmac_mediachange __P((struct ifnet *));
void bmac_mediastatus __P((struct ifnet *, struct ifmediareq *));
void bmac_setladrf __P((struct bmac_softc *));

#ifdef __NetBSD__
int bmac_mii_readreg __P((struct device *, int, int));
void bmac_mii_writereg __P((struct device *, int, int, int));
void bmac_mii_statchg __P((struct device *));
void bmac_mii_tick __P((void *));
u_int32_t bmac_mbo_read __P((struct device *));
void bmac_mbo_write __P((struct device *, u_int32_t));
#endif /* __NetBSD__ */

struct cfattach bm_ca = {
	sizeof(struct bmac_softc), bmac_match, bmac_attach
};

#ifdef __NetBSD__
struct mii_bitbang_ops bmac_mbo = {
	bmac_mbo_read, bmac_mbo_write,
	{ MIFDO, MIFDI, MIFDC, MIFDIR, 0 }
};
#endif /* __NetBSD__ */

struct cfdriver bm_cd = {
	NULL, "bm", DV_IFNET
};

int
bmac_read_reg(sc, off)
	struct bmac_softc *sc;
	int off;
{
	return in16rb(sc->sc_regs + off);
}

void
bmac_write_reg(sc, off, val)
	struct bmac_softc *sc;
	int off, val;
{
	out16rb(sc->sc_regs + off, val);
}

void
bmac_set_bits(sc, off, val)
	struct bmac_softc *sc;
	int off, val;
{
	val |= bmac_read_reg(sc, off);
	bmac_write_reg(sc, off, val);
}

void
bmac_reset_bits(sc, off, val)
	struct bmac_softc *sc;
	int off, val;
{
	bmac_write_reg(sc, off, bmac_read_reg(sc, off) & ~val);
}

int
bmac_match(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct confargs *ca = aux;

	if (ca->ca_nreg < 24 || ca->ca_nintr < 12)
		return 0;

	if (strcmp(ca->ca_name, "bmac") == 0) {		/* bmac */
		return 1;
	}
	if (strcmp(ca->ca_name, "ethernet") == 0) {	/* bmac+ */
		return 1;
	}

	return 0;
}

void
bmac_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct bmac_softc *sc = (void *)self;
	struct ifnet *ifp = &sc->sc_if;
	struct mii_data *mii = &sc->sc_mii;
	u_char laddr[6];

	sc->sc_flags =0;
	if (strcmp(ca->ca_name, "ethernet") == 0) {
		char name[64];

		bzero(name, 64);
		OF_package_to_path(ca->ca_node, name, sizeof(name));
		OF_open(name);
		sc->sc_flags |= BMAC_BMACPLUS;
	}

	ca->ca_reg[0] += ca->ca_baseaddr;
	ca->ca_reg[2] += ca->ca_baseaddr;
	ca->ca_reg[4] += ca->ca_baseaddr;

	sc->sc_regs = (vaddr_t)mapiodev(ca->ca_reg[0], NBPG);

	bmac_write_reg(sc, INTDISABLE, NoEventsMask);

	if (OF_getprop(ca->ca_node, "local-mac-address", laddr, 6) == -1 &&
	    OF_getprop(ca->ca_node, "mac-address", laddr, 6) == -1) {
		printf(": cannot get mac-address\n");
		return;
	}
	bcopy(laddr, sc->arpcom.ac_enaddr, 6);

	sc->sc_txdma = mapiodev(ca->ca_reg[2], 0x100);
	sc->sc_rxdma = mapiodev(ca->ca_reg[4], 0x100);
	sc->sc_txcmd = dbdma_alloc(BMAC_TXBUFS * sizeof(dbdma_command_t));
	sc->sc_rxcmd = dbdma_alloc((BMAC_RXBUFS + 1) * sizeof(dbdma_command_t));
	sc->sc_txbuf = malloc(BMAC_BUFLEN * BMAC_TXBUFS, M_DEVBUF, M_NOWAIT);
	sc->sc_rxbuf = malloc(BMAC_BUFLEN * BMAC_RXBUFS, M_DEVBUF, M_NOWAIT);
	if (sc->sc_txbuf == NULL || sc->sc_rxbuf == NULL ||
	    sc->sc_txcmd == NULL || sc->sc_rxcmd == NULL) {
		printf("cannot allocate memory\n");
		return;
	}

	printf(" irq %d,%d: address %s\n", ca->ca_intr[0], ca->ca_intr[2],
		ether_sprintf(laddr));

	mac_intr_establish(parent, ca->ca_intr[0], IST_LEVEL, IPL_NET,
		bmac_intr, sc, "bmac intr");
	/*
	mac_intr_establish(parent, ca->ca_intr[1], IST_LEVEL, IPL_NET,
		bmac_tx_intr, sc, "bmac_tx");
	*/
	mac_intr_establish(parent, ca->ca_intr[2], IST_LEVEL, IPL_NET,
		bmac_rint, sc, "bmac rint");

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_ioctl = bmac_ioctl;
	ifp->if_start = bmac_start;
	ifp->if_flags =
		IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	ifp->if_watchdog = bmac_watchdog;

#ifdef __NetBSD__
	mii->mii_ifp = ifp;
	mii->mii_readreg = bmac_mii_readreg;
	mii->mii_writereg = bmac_mii_writereg;
	mii->mii_statchg = bmac_mii_statchg;
#endif /* __NetBSD__ */

#ifdef __NetBSD__
	ifmedia_init(&mii->mii_media, 0, bmac_mediachange, bmac_mediastatus);
#endif /* __NetBSD__ */
#ifdef __NetBSD__
	mii_attach(&sc->sc_dev, mii, 0xffffffff, MII_PHY_ANY,
		      MII_OFFSET_ANY, 0);
#endif /* __NetBSD__ */

	/* Choose a default media. */
	if (LIST_FIRST(&mii->mii_phys) == NULL) {
		ifmedia_add(&mii->mii_media, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_set(&mii->mii_media, IFM_ETHER|IFM_10_T);
	} else
		ifmedia_set(&mii->mii_media, IFM_ETHER|IFM_AUTO);

	bmac_reset_chip(sc);

	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
}

/*
 * Reset and enable bmac by heathrow FCR.
 */
void
bmac_reset_chip(sc)
	struct bmac_softc *sc;
{
	u_int v;

	dbdma_reset(sc->sc_txdma);
	dbdma_reset(sc->sc_rxdma);

	v = in32rb(heathrow_FCR);

	v |= EnetEnable;
	out32rb(heathrow_FCR, v);
	delay(50000);

	/* assert reset */
	v |= ResetEnetCell;
	out32rb(heathrow_FCR, v);
	delay(70000);

	/* deassert reset */
	v &= ~ResetEnetCell;
	out32rb(heathrow_FCR, v);
	delay(50000);

	/* enable */
	v |= EnetEnable;
	out32rb(heathrow_FCR, v);
	delay(50000);

	/* make certain they stay set? */
	out32rb(heathrow_FCR, v);
	v = in32rb(heathrow_FCR);
}

void
bmac_init(sc)
	struct bmac_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	struct ether_header *eh;
	caddr_t data;
	int i, tb, bmcr;
	u_short *p;

	bmac_reset_chip(sc);

#ifdef __NetBSD__
	/* XXX */
	bmcr = bmac_mii_readreg((struct device *)sc, 0, MII_BMCR);
	bmcr &= ~BMCR_ISO;
	bmac_mii_writereg((struct device *)sc, 0, MII_BMCR, bmcr);
#endif /* __NetBSD__ */

	bmac_write_reg(sc, RXRST, RxResetValue);
	bmac_write_reg(sc, TXRST, TxResetBit);

	/* Wait for reset completion. */
 	do {
 		delay(100); 
 	} while (bmac_read_reg(sc, TXRST) & TxResetBit);
	if (i <= 0)
		printf("%s: reset timeout\n", ifp->if_xname);

	if (! (sc->sc_flags & BMAC_BMACPLUS)) {
  		bmac_set_bits(sc, XCVRIF, ClkBit|SerialMode|COLActiveLow);
	}

	__asm __volatile ("mftb %0" : "=r"(tb));
	bmac_write_reg(sc, RSEED, tb);
	bmac_set_bits(sc, XIFC, TxOutputEnable);
	bmac_read_reg(sc, PAREG);

	/* Reset various counters. */
	bmac_write_reg(sc, NCCNT, 0);
	bmac_write_reg(sc, NTCNT, 0);
	bmac_write_reg(sc, EXCNT, 0);
	bmac_write_reg(sc, LTCNT, 0);
	bmac_write_reg(sc, FRCNT, 0);
	bmac_write_reg(sc, LECNT, 0);
	bmac_write_reg(sc, AECNT, 0);
	bmac_write_reg(sc, FECNT, 0);
	bmac_write_reg(sc, RXCV, 0);

	/* Set tx fifo information. */
	bmac_write_reg(sc, TXTH, 4);	/* 4 octets before tx starts */

	bmac_write_reg(sc, TXFIFOCSR, 0);
	bmac_write_reg(sc, TXFIFOCSR, TxFIFOEnable);

	/* Set rx fifo information. */
	bmac_write_reg(sc, RXFIFOCSR, 0);
	bmac_write_reg(sc, RXFIFOCSR, RxFIFOEnable);

	/* Clear status register. */
	bmac_read_reg(sc, STATUS);

	bmac_write_reg(sc, HASH3, 0);
	bmac_write_reg(sc, HASH2, 0);
	bmac_write_reg(sc, HASH1, 0);
	bmac_write_reg(sc, HASH0, 0);

	/* Set MAC address. */
	p = (u_short *)sc->sc_enaddr;
	bmac_write_reg(sc, MADD0, *p++);
	bmac_write_reg(sc, MADD1, *p++);
	bmac_write_reg(sc, MADD2, *p);

	bmac_write_reg(sc, RXCFG,
		RxCRCEnable | RxHashFilterEnable | RxRejectOwnPackets);

	if (ifp->if_flags & IFF_PROMISC)
		bmac_set_bits(sc, RXCFG, RxPromiscEnable);

	bmac_init_dma(sc);

	/* Enable TX/RX */
	bmac_set_bits(sc, RXCFG, RxMACEnable);
	bmac_set_bits(sc, TXCFG, TxMACEnable);

	bmac_write_reg(sc, INTDISABLE, NormalIntEvents);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;

	data = sc->sc_txbuf;
	eh = (struct ether_header *)data;

	bzero(data, sizeof(eh) + ETHERMIN);
	bcopy(sc->sc_enaddr, eh->ether_dhost, ETHER_ADDR_LEN);
	bcopy(sc->sc_enaddr, eh->ether_shost, ETHER_ADDR_LEN);
	bmac_transmit_packet(sc, data, sizeof(eh) + ETHERMIN);

	bmac_start(ifp);

#ifdef __NetBSD__
	untimeout(bmac_mii_tick, sc);
	timeout(bmac_mii_tick, sc, hz);
#endif /* __NetBSD__ */
}

void
bmac_init_dma(sc)
	struct bmac_softc *sc;
{
	dbdma_command_t *cmd = sc->sc_rxcmd;
	int i;

	dbdma_reset(sc->sc_txdma);
	dbdma_reset(sc->sc_rxdma);

	bzero(sc->sc_txcmd, BMAC_TXBUFS * sizeof(dbdma_command_t));
	bzero(sc->sc_rxcmd, (BMAC_RXBUFS + 1) * sizeof(dbdma_command_t));

	for (i = 0; i < BMAC_RXBUFS; i++) {
		DBDMA_BUILD(cmd, DBDMA_CMD_IN_LAST, 0, BMAC_BUFLEN,
			vtophys(sc->sc_rxbuf + BMAC_BUFLEN * i),
			DBDMA_INT_ALWAYS, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
		cmd++;
	}
	DBDMA_BUILD(cmd, DBDMA_CMD_NOP, 0, 0, 0,
		DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_ALWAYS);
	dbdma_st32(&cmd->d_cmddep, vtophys(sc->sc_rxcmd));

	sc->sc_rxlast = 0;

	dbdma_start(sc->sc_rxdma, sc->sc_rxcmd);
}

int
bmac_tx_intr(v)
	void *v;
{
	struct bmac_softc *sc = v;
	u_int16_t stat;

	sc->sc_if.if_flags &= ~IFF_OACTIVE;
	sc->sc_if.if_timer = 0;
	sc->sc_if.if_opackets++;
	bmac_start(&sc->sc_if);

#ifndef BMAC_DEBUG
	printf("bmac_tx_intr \n");
#endif
	#if 0
	stat = bmac_read_reg(sc, STATUS);
	if (stat == 0) {
		printf("tx intr fired, but status 0\n");
		return 0;
	}


	if (stat & IntFrameSent) {
		sc->sc_if.if_flags &= ~IFF_OACTIVE;
		sc->sc_if.if_timer = 0;
		sc->sc_if.if_opackets++;
		bmac_start(&sc->sc_if);
	}
	#endif
	return 1;
}
int
bmac_intr(v)
	void *v;
{
	struct bmac_softc *sc = v;
	int stat;

#ifdef BMAC_DEBUG
	printf("bmac_intr called\n");
#endif
	stat = bmac_read_reg(sc, STATUS);
	if (stat == 0)
		return 0;

#ifdef BMAC_DEBUG
	printf("bmac_intr status = 0x%x\n", stat);
#endif

	if (stat & IntFrameSent) {
		sc->sc_if.if_flags &= ~IFF_OACTIVE;
		sc->sc_if.if_timer = 0;
		sc->sc_if.if_opackets++;
		bmac_start(&sc->sc_if);
	}

	/* XXX should do more! */

	return 1;
}

int
bmac_rint(v)
	void *v;
{
	struct bmac_softc *sc = v;
	struct ifnet *ifp = &sc->sc_if;
	struct mbuf *m;
	dbdma_command_t *cmd;
	int status, resid, count, datalen;
	int i, n;
	void *data;
#ifdef BMAC_DEBUG
	printf("bmac_rint() called\n");
#endif

	i = sc->sc_rxlast;
	for (n = 0; n < BMAC_RXBUFS; n++, i++) {
		if (i == BMAC_RXBUFS)
			i = 0;
		cmd = &sc->sc_rxcmd[i];
		status = dbdma_ld16(&cmd->d_status);
		resid = dbdma_ld16(&cmd->d_resid);

#ifdef BMAC_DEBUG
		if (status != 0 && status != 0x8440 && status != 0x9440)
			printf("bmac_rint status = 0x%x\n", status);
#endif

		if ((status & DBDMA_CNTRL_ACTIVE) == 0)	/* 0x9440 | 0x8440 */
			continue;
		count = dbdma_ld16(&cmd->d_count);
		datalen = count - resid;
		if (datalen < sizeof(struct ether_header)) {
			printf("%s: short packet len = %d\n",
				ifp->if_xname, datalen);
			goto next;
		}
		DBDMA_BUILD_CMD(cmd, DBDMA_CMD_STOP, 0, 0, 0, 0);
		data = sc->sc_rxbuf + BMAC_BUFLEN * i;
		m = bmac_get(sc, data, datalen);

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
			bpf_mtap(ifp->if_bpf, m);
#endif
#ifdef __OpenBSD__
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp, data, m);
#else
		(*ifp->if_input)(ifp, m);
#endif
		ifp->if_ipackets++;

next:
		DBDMA_BUILD_CMD(cmd, DBDMA_CMD_IN_LAST, 0, DBDMA_INT_ALWAYS,
			DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);

		cmd->d_status = 0;
		cmd->d_resid = 0;
		sc->sc_rxlast = i + 1;
	}
	dbdma_continue(sc->sc_rxdma);

	return 1;
}

void
bmac_reset(sc)
	struct bmac_softc *sc;
{
	int s;

	s = splnet();
	bmac_init(sc);
	splx(s);
}

void
bmac_stop(sc)
	struct bmac_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	int s;

	s = splnet();

#ifdef __NetBSD__
	untimeout(bmac_mii_tick, sc);
	mii_down(&sc->sc_mii);
#endif /* __NetBSD__ */

	/* Disable TX/RX. */
	bmac_reset_bits(sc, TXCFG, TxMACEnable);
	bmac_reset_bits(sc, RXCFG, RxMACEnable);

	/* Disable all interrupts. */
	bmac_write_reg(sc, INTDISABLE, NoEventsMask);

	dbdma_stop(sc->sc_txdma);
	dbdma_stop(sc->sc_rxdma);

	ifp->if_flags &= ~(IFF_UP | IFF_RUNNING);
	ifp->if_timer = 0;

	splx(s);
}

void
bmac_start(ifp)
	struct ifnet *ifp;
{
	struct bmac_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int tlen;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	while (1) {
		if (ifp->if_flags & IFF_OACTIVE)
			return;

		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;
#if NBPFILTER > 0
		/*
		 * If BPF is listening on this interface, let it see the
		 * packet before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		ifp->if_flags |= IFF_OACTIVE;
		tlen = bmac_put(sc, sc->sc_txbuf, m);

		/* 5 seconds to watch for failing to transmit */
		ifp->if_timer = 5;
		ifp->if_opackets++;		/* # of pkts */

		bmac_transmit_packet(sc, sc->sc_txbuf, tlen);
	}
}

void
bmac_transmit_packet(sc, buff, len)
	struct bmac_softc *sc;
	void *buff;
	int len;
{
	dbdma_command_t *cmd = sc->sc_txcmd;
	vaddr_t va = (vaddr_t)buff;

#ifdef BMAC_DEBUG
	if (vtophys(va) + len - 1 != vtophys(va + len - 1))
		panic("bmac_transmit_packet");
#endif

	DBDMA_BUILD(cmd, DBDMA_CMD_OUT_LAST, 0, len, vtophys(va),
		DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
	cmd++;
	DBDMA_BUILD(cmd, DBDMA_CMD_STOP, 0, 0, 0,
		DBDMA_INT_ALWAYS, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);

	dbdma_start(sc->sc_txdma, sc->sc_txcmd);
}

int
bmac_put(sc, buff, m)
	struct bmac_softc *sc;
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
	if (tlen > NBPG)
		panic("%s: putpacket packet overflow", sc->sc_dev.dv_xname);

	return tlen;
}

struct mbuf *
bmac_get(sc, pkt, totlen)
	struct bmac_softc *sc;
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
bmac_watchdog(ifp)
	struct ifnet *ifp;
{
	struct bmac_softc *sc = ifp->if_softc;

	bmac_reset_bits(sc, RXCFG, RxMACEnable);
	bmac_reset_bits(sc, TXCFG, TxMACEnable);

	printf("%s: device timeout\n", ifp->if_xname);
	ifp->if_oerrors++;

	bmac_reset(sc);
}

int
bmac_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct bmac_softc *sc = ifp->if_softc;
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
			bmac_init(sc);
#ifdef __OpenBSD__
			arp_ifinit(&sc->arpcom, ifa);
#else
			arp_ifinit(ifp, ifa);
#endif
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
			bmac_init(sc);
			break;
		    }
#endif
		default:
			bmac_init(sc);
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
			bmac_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			bmac_init(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			/*bmac_stop(sc);*/
			bmac_init(sc);
		}
#ifdef BMAC_DEBUG
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_flags |= BMAC_DEBUGFLAG;
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
			bmac_init(sc);
			bmac_setladrf(sc);
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

#ifdef __NetBSD__
int
bmac_mediachange(ifp)
	struct ifnet *ifp;
{
	struct bmac_softc *sc = ifp->if_softc;

	return mii_mediachg(&sc->sc_mii);
}
#endif /* __NetBSD__ */

#ifdef __NetBSD__
void
bmac_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct bmac_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);

	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}
#endif /* __NetBSD__ */

#define MC_POLY_BE 0x04c11db7UL		/* mcast crc, big endian */
#define MC_POLY_LE 0xedb88320UL		/* mcast crc, little endian */

/*
 * Set up the logical address filter.
 */
void
bmac_setladrf(sc)
	struct bmac_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	int i, j;
	u_int32_t crc;
	u_int16_t hash[4];
	u_int8_t octet;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	if (ifp->if_flags & IFF_ALLMULTI)
		goto allmulti;

	if (ifp->if_flags & IFF_PROMISC) {
		bmac_set_bits(sc, RXCFG, RxPromiscEnable);
		goto allmulti;
	}

	hash[3] = hash[2] = hash[1] = hash[0] = 0;
#ifdef __OpenBSD__
	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
#else
	ETHER_FIRST_MULTI(step, &sc->sc_ethercom, enm);
#endif
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		crc = 0xffffffff;
		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			octet = enm->enm_addrlo[i];

			for (j = 0; j < 8; j++) {
				if ((crc & 1) ^ (octet & 1)) {
					crc >>= 1;
					crc ^= MC_POLY_LE;
				}
				else
					crc >>= 1;
				octet >>= 1;
			}
		}

		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Set the corresponding bit in the filter. */
		hash[crc >> 4] |= 1 << (crc & 0xf);

		ETHER_NEXT_MULTI(step, enm);
	}
	bmac_write_reg(sc, HASH3, hash[3]);
	bmac_write_reg(sc, HASH2, hash[2]);
	bmac_write_reg(sc, HASH1, hash[1]);
	bmac_write_reg(sc, HASH0, hash[0]);
	ifp->if_flags &= ~IFF_ALLMULTI;
	return;

allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	bmac_write_reg(sc, HASH3, 0xffff);
	bmac_write_reg(sc, HASH2, 0xffff);
	bmac_write_reg(sc, HASH1, 0xffff);
	bmac_write_reg(sc, HASH0, 0xffff);
}

#ifdef __NetBSD__
int
bmac_mii_readreg(dev, phy, reg)
	struct device *dev;
	int phy, reg;
{
	return mii_bitbang_readreg(dev, &bmac_mbo, phy, reg);
}
#endif /* __NetBSD__ */

#ifdef __NetBSD__
void
bmac_mii_writereg(dev, phy, reg, val)
	struct device *dev;
	int phy, reg, val;
{
	mii_bitbang_writereg(dev, &bmac_mbo, phy, reg, val);
}
#endif /* __NetBSD__ */

#ifdef __NetBSD__
u_int32_t
bmac_mbo_read(dev)
	struct device *dev;
{
	struct bmac_softc *sc = (void *)dev;

	return bmac_read_reg(sc, MIFCSR);
}

void
bmac_mbo_write(dev, val)
	struct device *dev;
	u_int32_t val;
{
	struct bmac_softc *sc = (void *)dev;

	bmac_write_reg(sc, MIFCSR, val);
}

void
bmac_mii_statchg(dev)
	struct device *dev;
{
	struct bmac_softc *sc = (void *)dev;
	int x;

	/* Update duplex mode in TX configuration */
	x = bmac_read_reg(sc, TXCFG);
	if ((IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) != 0)
		x |= TxFullDuplex;
	else
		x &= ~TxFullDuplex;
	bmac_write_reg(sc, TXCFG, x);

#ifdef BMAC_DEBUG
	printf("bmac_mii_statchg 0x%x\n",
		IFM_OPTIONS(sc->sc_mii.mii_media_active));
#endif
}
#endif /* __NetBSD__ */

#ifdef __NetBSD__
void
bmac_mii_tick(v)
	void *v;
{
	struct bmac_softc *sc = v;
	int s;

	s = splnet();
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout(bmac_mii_tick, sc, hz);
}
#endif /* __NetBSD__ */
