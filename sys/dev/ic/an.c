/*	$OpenBSD: an.c,v 1.40 2005/02/04 01:07:39 kurt Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/an/if_an.c,v 1.21 2001/09/10 02:05:09 brooks Exp $
 */

/*
 * Aironet 4500/4800 802.11 PCMCIA/ISA/PCI driver for FreeBSD.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Aironet 4500/4800 series cards some in PCMCIA, ISA and PCI form.
 * This driver supports all three device types (PCI devices are supported
 * through an extra PCI shim: /sys/pci/if_an_p.c). ISA devices can be
 * supported either using hard-coded IO port/IRQ settings or via Plug
 * and Play. The 4500 series devices support 1Mbps and 2Mbps data rates.
 * The 4800 devices support 1, 2, 5.5 and 11Mbps rates.
 *
 * Like the WaveLAN/IEEE cards, the Aironet NICs are all essentially
 * PCMCIA devices. The ISA and PCI cards are a combination of a PCMCIA
 * device and a PCMCIA to ISA or PCMCIA to PCI adapter card. There are
 * a couple of important differences though:
 *
 * - Lucent doesn't currently offer a PCI card, however Aironet does
 * - Lucent ISA card looks to the host like a PCMCIA controller with
 *   a PCMCIA WaveLAN card inserted. This means that even desktop
 *   machines need to be configured with PCMCIA support in order to
 *   use WaveLAN/IEEE ISA cards. The Aironet cards on the other hand
 *   actually look like normal ISA and PCI devices to the host, so
 *   no PCMCIA controller support is needed
 *
 * The latter point results in a small gotcha. The Aironet PCMCIA
 * cards can be configured for one of two operating modes depending
 * on how the Vpp1 and Vpp2 programming voltages are set when the
 * card is activated. In order to put the card in proper PCMCIA
 * operation (where the CIS table is visible and the interface is
 * programmed for PCMCIA operation), both Vpp1 and Vpp2 have to be
 * set to 5 volts. FreeBSD by default doesn't set the Vpp voltages,
 * which leaves the card in ISA/PCI mode, which prevents it from
 * being activated as an PCMCIA device. Consequently, /sys/pccard/pccard.c
 * has to be patched slightly in order to enable the Vpp voltages in
 * order to make the Aironet PCMCIA cards work.
 *
 * Note that some PCMCIA controller software packages for Windows NT
 * fail to set the voltages as well.
 *
 * The Aironet devices can operate in both station mode and access point
 * mode. Typically, when programmed for station mode, the card can be set
 * to automatically perform encapsulation/decapsulation of Ethernet II
 * and 802.3 frames within 802.11 frames so that the host doesn't have
 * to do it itself. This driver doesn't program the card that way: the
 * driver handles all of the encapsulation/decapsulation itself.
 */

#ifdef INET
#define ANCACHE			/* enable signal strength cache */
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#ifdef ANCACHE
#include <sys/syslog.h>
#include <sys/sysctl.h>
#endif

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/bus.h>

#include <dev/ic/anvar.h>
#include <dev/ic/anreg.h>

#define TIMEOUT(handle,func,sc,time) timeout_add(&(handle), (time))
#define UNTIMEOUT(func,sc,handle) timeout_del(&(handle))
#define BPF_MTAP(if,mbuf) bpf_mtap((if)->if_bpf, (mbuf))
#define BPFATTACH(if_bpf,if,dlt,sz)

struct cfdriver an_cd = {
	NULL, "an", DV_IFNET
};

void an_reset(struct an_softc *);
int an_ioctl(struct ifnet *, u_long, caddr_t);
int an_init_tx_ring(struct an_softc *);
void an_start(struct ifnet *);
void an_watchdog(struct ifnet *);
void an_rxeof(struct an_softc *);
void an_txeof(struct an_softc *, int);

void an_promisc(struct an_softc *, int);
int an_cmd(struct an_softc *, int, int);
int an_read_record(struct an_softc *, struct an_ltv_gen *);
int an_write_record(struct an_softc *, struct an_ltv_gen *);
int an_read_data(struct an_softc *, int,
					int, caddr_t, int);
int an_write_data(struct an_softc *, int,
					int, caddr_t, int);
int an_seek(struct an_softc *, int, int, int);
int an_alloc_nicmem(struct an_softc *, int, int *);
void an_stats_update(void *);
void an_setdef(struct an_softc *, struct an_req *);
#ifdef ANCACHE
void an_cache_store(struct an_softc *, struct ether_header *,
					struct mbuf *, unsigned short);
#endif
int an_media_change(struct ifnet *);
void an_media_status(struct ifnet *, struct ifmediareq *);

static __inline void
an_swap16(u_int16_t *p, int cnt)
{
	for (; cnt--; p++)
		*p = swap16(*p);
}

int
an_attach(sc)
	struct an_softc *sc;
{
	struct ifnet	*ifp = &sc->sc_arpcom.ac_if;

	sc->an_gone = 0;
	sc->an_associated = 0;

	/* disable interrupts */
	CSR_WRITE_2(sc, AN_INT_EN, 0);
	CSR_WRITE_2(sc, AN_EVENT_ACK, 0xffff);

	/* Reset the NIC. */
	an_reset(sc);

	/* Load factory config */
	if (an_cmd(sc, AN_CMD_READCFG, 0)) {
		printf("%s: failed to load config data\n", ifp->if_xname);
		return(EIO);
	}

	/* Read the current configuration */
	sc->an_config.an_type = AN_RID_GENCONFIG;
	sc->an_config.an_len = sizeof(struct an_ltv_genconfig);
	if (an_read_record(sc, (struct an_ltv_gen *)&sc->an_config)) {
		printf("%s: read record failed\n", ifp->if_xname);
		return(EIO);
	}

	/* Read the card capabilities */
	sc->an_caps.an_type = AN_RID_CAPABILITIES;
	sc->an_caps.an_len = sizeof(struct an_ltv_caps);
	if (an_read_record(sc, (struct an_ltv_gen *)&sc->an_caps)) {
		printf("%s: read record failed\n", ifp->if_xname);
		return(EIO);
	}

	/* Read ssid list */
	sc->an_ssidlist.an_type = AN_RID_SSIDLIST;
	sc->an_ssidlist.an_len = sizeof(struct an_ltv_ssidlist);
	if (an_read_record(sc, (struct an_ltv_gen *)&sc->an_ssidlist)) {
		printf("%s: read record failed\n", ifp->if_xname);
		return(EIO);
	}

	/* Read AP list */
	sc->an_aplist.an_type = AN_RID_APLIST;
	sc->an_aplist.an_len = sizeof(struct an_ltv_aplist);
	if (an_read_record(sc, (struct an_ltv_gen *)&sc->an_aplist)) {
		printf("%s: read record failed\n", ifp->if_xname);
		return(EIO);
	}

	bcopy((char *)&sc->an_caps.an_oemaddr,
	   (char *)&sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);

	printf(": address %6s\n", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = an_ioctl;
	ifp->if_start = an_start;
	ifp->if_watchdog = an_watchdog;
	ifp->if_baudrate = 10000000;
	IFQ_SET_READY(&ifp->if_snd);

	bzero(sc->an_config.an_nodename, sizeof(sc->an_config.an_nodename));
	strlcpy(sc->an_config.an_nodename, AN_DEFAULT_NODENAME,
	    sizeof(sc->an_config.an_nodename));

	bzero(sc->an_ssidlist.an_ssid1, sizeof(sc->an_ssidlist.an_ssid1));
	strlcpy(sc->an_ssidlist.an_ssid1, AN_DEFAULT_NETNAME,
	    sizeof(sc->an_ssidlist.an_ssid1));
	sc->an_ssidlist.an_ssid1_len = strlen(sc->an_ssidlist.an_ssid1);

	sc->an_config.an_opmode = AN_OPMODE_INFRASTRUCTURE_STATION;

	sc->an_tx_rate = 0;
	bzero((char *)&sc->an_stats, sizeof(sc->an_stats));
#ifdef ANCACHE
	sc->an_sigitems = sc->an_nextitem = 0;
#endif

	ifmedia_init(&sc->an_ifmedia, 0, an_media_change, an_media_status);
#define	ADD(m, c)	ifmedia_add(&sc->an_ifmedia, (m), (c), NULL)
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS1,
	    IFM_IEEE80211_ADHOC, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS1, 0, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS2,
	    IFM_IEEE80211_ADHOC, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS2, 0, 0), 0);
	if (sc->an_caps.an_rates[2] == AN_RATE_5_5MBPS) {
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS5,
		    IFM_IEEE80211_ADHOC, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS5, 0, 0), 0);
	}
	if (sc->an_caps.an_rates[3] == AN_RATE_11MBPS) {
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS11,
		    IFM_IEEE80211_ADHOC, 0), 0);
		ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_IEEE80211_DS11, 0, 0), 0);
	}
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO,
	    IFM_IEEE80211_ADHOC, 0), 0);
	ADD(IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO, 0, 0), 0);
#undef ADD
	ifmedia_set(&sc->an_ifmedia, IFM_MAKEWORD(IFM_IEEE80211, IFM_AUTO,
	    0, 0));

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);
	timeout_set(&sc->an_stat_ch, an_stats_update, sc);
#if NBPFILTER > 0
	BPFATTACH(&sc->sc_arpcom.ac_if.if_bpf, ifp, DLT_EN10MB,
	    sizeof(struct ether_header));
#endif

	shutdownhook_establish(an_shutdown, sc);

	an_reset(sc);
	an_init(sc);

	return(0);
}

void
an_rxeof(sc)
	struct an_softc	 *sc;
{
	struct ifnet		*ifp = &sc->sc_arpcom.ac_if;
	struct ether_header	*eh;
#ifdef ANCACHE
	struct an_rxframe	rx_frame;
#endif
	struct an_rxframe_802_3	rx_frame_802_3;
	struct mbuf		*m;
	int			id, len, error = 0;

	id = CSR_READ_2(sc, AN_RX_FID);

#ifdef ANCACHE
	/* Read NIC frame header */
	if (an_read_data(sc, id, 0, (caddr_t)&rx_frame, sizeof(rx_frame))) {
		ifp->if_ierrors++;
		return;
	}
#endif
	/* Read in the 802_3 frame header */
	if (an_read_data(sc, id, 0x34, (caddr_t)&rx_frame_802_3,
			 sizeof(rx_frame_802_3))) {
		ifp->if_ierrors++;
		return;
	}

	if (rx_frame_802_3.an_rx_802_3_status != 0) {
		ifp->if_ierrors++;
		return;
	}

	/* Check for insane frame length */
	len = letoh16(rx_frame_802_3.an_rx_802_3_payload_len);
	if (len + ETHER_HDR_LEN + 2 > MCLBYTES) {
		ifp->if_ierrors++;
		return;
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		ifp->if_ierrors++;
		return;
	}
	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		m_freem(m);
		ifp->if_ierrors++;
		return;
	}

	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = len + 12;
	m->m_data += ETHER_ALIGN;
	eh = mtod(m, struct ether_header *);

	bcopy((char *)&rx_frame_802_3.an_rx_dst_addr,
	    (char *)&eh->ether_dhost, ETHER_ADDR_LEN);
	bcopy((char *)&rx_frame_802_3.an_rx_src_addr,
	    (char *)&eh->ether_shost, ETHER_ADDR_LEN);

	/* in mbuf header type is just before payload */
	error = an_read_data(sc, id, 0x44, (caddr_t)&(eh->ether_type), len);
	if (error) {
		m_freem(m);
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

#if NBPFILTER > 0
	/* Handle BPF listeners. */
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m);
#endif

	/* Receive packet. */
#ifdef ANCACHE
	an_cache_store(sc, eh, m, rx_frame.an_rx_signal_strength);
#endif
	ether_input_mbuf(ifp, m);
}

void
an_txeof(sc, status)
	struct an_softc	*sc;
	int		status;
{
	struct ifnet	*ifp;
	int		id;

	ifp = &sc->sc_arpcom.ac_if;

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	id = CSR_READ_2(sc, AN_TX_CMP_FID);

	if (status & AN_EV_TX_EXC)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	if (id != sc->an_rdata.an_tx_ring[sc->an_rdata.an_tx_cons])
		printf("%s: id mismatch: expected %x, got %x\n", ifp->if_xname,
		    sc->an_rdata.an_tx_ring[sc->an_rdata.an_tx_cons], id);

	sc->an_rdata.an_tx_ring[sc->an_rdata.an_tx_cons] = 0;
	AN_INC(sc->an_rdata.an_tx_cons, AN_TX_RING_CNT);
}

/*
 * We abuse the stats updater to check the current NIC status. This
 * is important because we don't want to allow transmissions until
 * the NIC has synchronized to the current cell (either as the master
 * in an ad-hoc group, or as a station connected to an access point).
 */
void
an_stats_update(xsc)
	void			*xsc;
{
	struct an_softc		*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	sc = xsc;
	ifp = &sc->sc_arpcom.ac_if;

	sc->an_status.an_type = AN_RID_STATUS;
	sc->an_status.an_len = sizeof(struct an_ltv_status);
	an_read_record(sc, (struct an_ltv_gen *)&sc->an_status);

	if (sc->an_status.an_opmode & AN_STATUS_OPMODE_IN_SYNC)
		sc->an_associated = 1;
	else
		sc->an_associated = 0;

	/* Don't do this while we're transmitting */
	if (!(ifp->if_flags & IFF_OACTIVE)) {
		sc->an_stats.an_len = sizeof(struct an_ltv_stats);
		sc->an_stats.an_type = AN_RID_32BITS_CUM;
		an_read_record(sc, (struct an_ltv_gen *)&sc->an_stats.an_len);
	}

	splx(s);
	TIMEOUT(sc->an_stat_ch, an_stats_update, sc, hz);
}

int
an_intr(xsc)
	void	*xsc;
{
	struct an_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = (struct an_softc*)xsc;

	if (sc->an_gone)
		return 0;

	ifp = &sc->sc_arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP)) {
		CSR_WRITE_2(sc, AN_EVENT_ACK, 0xFFFF);
		CSR_WRITE_2(sc, AN_INT_EN, 0);
		return 0;
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, AN_INT_EN, 0);

	status = CSR_READ_2(sc, AN_EVENT_STAT);
	CSR_WRITE_2(sc, AN_EVENT_ACK, ~AN_INTRS);

	if (status & AN_EV_AWAKE) {
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_AWAKE);
	}

	if (status & AN_EV_LINKSTAT) {
		if (CSR_READ_2(sc, AN_LINKSTAT) == AN_LINKSTAT_ASSOCIATED)
			sc->an_associated = 1;
		else
			sc->an_associated = 0;
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_LINKSTAT);
	}

	if (status & AN_EV_RX) {
		an_rxeof(sc);
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_RX);
	}

	if (status & AN_EV_TX) {
		an_txeof(sc, status);
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_TX);
	}

	if (status & AN_EV_TX_EXC) {
		an_txeof(sc, status);
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_TX_EXC);
	}

	if (status & AN_EV_ALLOC)
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_ALLOC);

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, AN_INT_EN, AN_INTRS);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		an_start(ifp);

	return 1;
}

int
an_cmd(sc, cmd, val)
	struct an_softc *sc;
	int cmd;
	int val;
{
	int i, stat;

	/* make sure previous command completed */
	if (CSR_READ_2(sc, AN_COMMAND) & AN_CMD_BUSY) {
		printf("%s: command busy\n", sc->sc_dev.dv_xname);
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_CLR_STUCK_BUSY);
	}

	CSR_WRITE_2(sc, AN_PARAM0, val);
	CSR_WRITE_2(sc, AN_PARAM1, 0);
	CSR_WRITE_2(sc, AN_PARAM2, 0);
	DELAY(10);
	CSR_WRITE_2(sc, AN_COMMAND, cmd);
	DELAY(10);

	for (i = AN_TIMEOUT; i--; DELAY(10)) {
		if (CSR_READ_2(sc, AN_EVENT_STAT) & AN_EV_CMD)
			break;
		else {
			if (CSR_READ_2(sc, AN_COMMAND) == cmd) {
				DELAY(10);
				CSR_WRITE_2(sc, AN_COMMAND, cmd);
			}
		}
	}

	stat = CSR_READ_2(sc, AN_STATUS);

	/* clear stuck command busy if needed */
	if (CSR_READ_2(sc, AN_COMMAND) & AN_CMD_BUSY) {
		CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_CLR_STUCK_BUSY);
	}

	/* Ack the command */
	CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_CMD);

	if (i <= 0)
		return(ETIMEDOUT);

	if (stat & AN_STAT_CMD_RESULT)
		return(EIO);

	return(0);
}

/*
 * This reset sequence may look a little strange, but this is the
 * most reliable method I've found to really kick the NIC in the
 * head and force it to reboot correctly.
 */
void
an_reset(sc)
	struct an_softc		*sc;
{
	if (sc->an_gone)
		return;
/*printf("ena ");*/
	an_cmd(sc, AN_CMD_ENABLE, 0);
/* printf("rst ");*/
	an_cmd(sc, AN_CMD_FW_RESTART, 0);
/*printf("nop ");*/
	an_cmd(sc, AN_CMD_NOOP2, 0);

	if (an_cmd(sc, AN_CMD_FORCE_SYNCLOSS, 0) == ETIMEDOUT)
		printf("%s: reset failed\n", sc->sc_dev.dv_xname);

	an_cmd(sc, AN_CMD_DISABLE, 0);
}

/*
 * Read an LTV record from the NIC.
 */
int
an_read_record(sc, ltv)
	struct an_softc		*sc;
	struct an_ltv_gen	*ltv;
{
	u_int16_t	*ptr, len, rlen, ltv_data_length;
	volatile u_int16_t v;
	int		i;

	if (ltv->an_len < 4 || ltv->an_type == 0)
		return(EINVAL);

	/* Tell the NIC to enter record read mode. */
	if (an_cmd(sc, AN_CMD_ACCESS|AN_ACCESS_READ, ltv->an_type)) {
		printf("%s: RID 0x%04x access failed\n",
		    sc->sc_dev.dv_xname, ltv->an_type);
		return(EIO);
	}

	/* Seek to the record. */
	if (an_seek(sc, ltv->an_type, 0, AN_BAP1)) {
		printf("%s: RID 0x%04x seek to record failed\n",
		    sc->sc_dev.dv_xname, ltv->an_type);
		return(EIO);
	}

	/*
	 * Read the length to make sure it
	 * matches what we expect (this verifies that we have enough
	 * room to hold all of the returned data).
	 */
	rlen = len = CSR_READ_2(sc, AN_DATA1);

	/*
	 * Work out record's data length, which is struct length - type word
	 * as we have just read the length.
	 */
	ltv_data_length = ltv->an_len - sizeof(u_int16_t);

	if (rlen > ltv_data_length)
		rlen = ltv_data_length;

	/* Now read the data. */
	len -= 2; rlen -= 2;	/* skip the type */
	ptr = ltv->an_val;
	for (i = 0; (rlen - i) > 1; i += 2)
		*ptr++ = CSR_READ_2(sc, AN_DATA1);
	if (rlen - i == 1)
		*(u_int8_t *)ptr = CSR_READ_1(sc, AN_DATA1);
	for (; i < len; i++)
		v = CSR_READ_1(sc, AN_DATA1);

#if BYTE_ORDER == BIG_ENDIAN
	switch (ltv->an_type) {
	case AN_RID_GENCONFIG:
	case AN_RID_ACTUALCFG:
		an_swap16(&ltv->an_val[4], 7); /* an_macaddr, an_rates */
		an_swap16(&ltv->an_val[63], 8);  /* an_nodename */
		break;
	case AN_RID_SSIDLIST:
		an_swap16(&ltv->an_val[1], 16); /* an_ssid1 */
		an_swap16(&ltv->an_val[18], 16); /* an_ssid2 */
		an_swap16(&ltv->an_val[35], 16); /* an_ssid3 */
		break;
	case AN_RID_APLIST:
		an_swap16(ltv->an_val, 12);
		break;
	case AN_RID_DRVNAME:
		an_swap16(ltv->an_val, 8);
		break;
	case AN_RID_CAPABILITIES:
		an_swap16(ltv->an_val, 2);	/* an_oui */
		an_swap16(&ltv->an_val[3], 34); /* an_manufname .. an_aironetaddr */
		an_swap16(&ltv->an_val[39], 8); /* an_callid .. an_tx_diversity */
		break;
	case AN_RID_STATUS:
		an_swap16(&ltv->an_val[0], 3);	/* an_macaddr */
		an_swap16(&ltv->an_val[7], 36);	/* an_ssid .. an_prev_bssid3 */
		an_swap16(&ltv->an_val[0x74/2], 2);	/* an_ap_ip_addr */
		break;
	case AN_RID_WEP_VOLATILE:
	case AN_RID_WEP_PERMANENT:
		an_swap16(&ltv->an_val[1], 3);	/* an_mac_addr */
		an_swap16(&ltv->an_val[5], 7);
		break;
	case AN_RID_32BITS_CUM:
		for (i = 0x60; i--;) {
			u_int16_t t = ltv->an_val[i * 2] ^ ltv->an_val[i * 2 + 1];
			ltv->an_val[i * 2] ^= t;
			ltv->an_val[i * 2 + 1] ^= t;
		}
		break;
	}
#endif
	return(0);
}

/*
 * Same as read, except we inject data instead of reading it.
 */
int
an_write_record(sc, ltv)
	struct an_softc		*sc;
	struct an_ltv_gen	*ltv;
{
	u_int16_t	*ptr;
	int		i;

	if (an_cmd(sc, AN_CMD_ACCESS|AN_ACCESS_READ, ltv->an_type))
		return(EIO);

	if (an_seek(sc, ltv->an_type, 0, AN_BAP1))
		return(EIO);

#if BYTE_ORDER == BIG_ENDIAN
	switch (ltv->an_type) {
	case AN_RID_GENCONFIG:
	case AN_RID_ACTUALCFG:
		an_swap16(&ltv->an_val[4], 7); /* an_macaddr, an_rates */
		an_swap16(&ltv->an_val[63], 8);  /* an_nodename */
		break;
	case AN_RID_SSIDLIST:
		an_swap16(&ltv->an_val[1], 16); /* an_ssid1 */
		an_swap16(&ltv->an_val[18], 16); /* an_ssid2 */
		an_swap16(&ltv->an_val[35], 16); /* an_ssid3 */
		break;
	case AN_RID_APLIST:
		an_swap16(ltv->an_val, 12);
		break;
	case AN_RID_DRVNAME:
		an_swap16(ltv->an_val, 8);
		break;
	case AN_RID_CAPABILITIES:
		an_swap16(ltv->an_val, 2);	/* an_oui */
		an_swap16(&ltv->an_val[3], 34); /* an_manufname .. an_aironetaddr */
		an_swap16(&ltv->an_val[39], 8); /* an_callid .. an_tx_diversity */
		break;
	case AN_RID_STATUS:
		an_swap16(&ltv->an_val[0], 3);	/* an_macaddr */
		an_swap16(&ltv->an_val[7], 36);	/* an_ssid .. an_prev_bssid3 */
		an_swap16(&ltv->an_val[0x74/2], 2);	/* an_ap_ip_addr */
		break;
	case AN_RID_WEP_VOLATILE:
	case AN_RID_WEP_PERMANENT:
		an_swap16(&ltv->an_val[1], 3);	/* an_mac_addr */
		an_swap16(&ltv->an_val[5], 7);
		break;
	}
#endif

	CSR_WRITE_2(sc, AN_DATA1, ltv->an_len);

	ptr = ltv->an_val;
	for (i = 0; i < (ltv->an_len - 1) >> 1; i++)
		CSR_WRITE_2(sc, AN_DATA1, ptr[i]);

	if (an_cmd(sc, AN_CMD_ACCESS|AN_ACCESS_WRITE, ltv->an_type))
		return(EIO);

	return(0);
}

int
an_seek(sc, id, off, chan)
	struct an_softc		*sc;
	int			id, off, chan;
{
	int			i;
	int			selreg, offreg;

	switch (chan) {
	case AN_BAP0:
		selreg = AN_SEL0;
		offreg = AN_OFF0;
		break;
	case AN_BAP1:
		selreg = AN_SEL1;
		offreg = AN_OFF1;
		break;
	default:
		printf("%s: invalid data path: %x\n",
		    sc->sc_dev.dv_xname, chan);
		return (EIO);
	}

	CSR_WRITE_2(sc, selreg, id);
	CSR_WRITE_2(sc, offreg, off);

	for (i = AN_TIMEOUT; i--; DELAY(10)) {
		if (!(CSR_READ_2(sc, offreg) & (AN_OFF_BUSY|AN_OFF_ERR)))
			break;
	}

	if (i <= 0)
		return(ETIMEDOUT);

	return (0);
}

int
an_read_data(sc, id, off, buf, len)
	struct an_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	if (off != -1 && an_seek(sc, id, off, AN_BAP1))
		return(EIO);

	bus_space_read_raw_multi_2(sc->an_btag, sc->an_bhandle,
	    AN_DATA1, buf, len & ~1);
	if (len & 1)
	        ((u_int8_t *)buf)[len - 1] = CSR_READ_1(sc, AN_DATA1);

	return (0);
}

int
an_write_data(sc, id, off, buf, len)
	struct an_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	if (off != -1 && an_seek(sc, id, off, AN_BAP0))
		return(EIO);

	bus_space_write_raw_multi_2(sc->an_btag, sc->an_bhandle,
	    AN_DATA0, buf, len & ~1);
	if (len & 1)
	        CSR_WRITE_1(sc, AN_DATA0, ((u_int8_t *)buf)[len - 1]);

	return (0);
}

/*
 * Allocate a region of memory inside the NIC and zero
 * it out.
 */
int
an_alloc_nicmem(sc, len, id)
	struct an_softc		*sc;
	int			len;
	int			*id;
{
	int			i;

	if (an_cmd(sc, AN_CMD_ALLOC_MEM, len)) {
		printf("%s: failed to allocate %d bytes on NIC\n",
		    sc->sc_dev.dv_xname, len);
		return(ENOMEM);
	}

	for (i = AN_TIMEOUT; i--; DELAY(10)) {
		if (CSR_READ_2(sc, AN_EVENT_STAT) & AN_EV_ALLOC)
			break;
	}

	if (i <= 0)
		return(ETIMEDOUT);

	CSR_WRITE_2(sc, AN_EVENT_ACK, AN_EV_ALLOC);
	*id = CSR_READ_2(sc, AN_ALLOC_FID);

	if (an_seek(sc, *id, 0, AN_BAP0))
		return(EIO);

	bus_space_set_multi_2(sc->an_btag, sc->an_bhandle,
	    AN_DATA0, 0, len / 2);
	CSR_WRITE_1(sc, AN_DATA0, 0);

	return(0);
}

void
an_setdef(sc, areq)
	struct an_softc		*sc;
	struct an_req		*areq;
{
	struct ifnet		*ifp;
	struct an_ltv_genconfig	*cfg;
	struct an_ltv_ssidlist	*ssid;
	struct an_ltv_aplist	*ap;
	struct an_ltv_gen	*sp;

	ifp = &sc->sc_arpcom.ac_if;

	switch (areq->an_type) {
	case AN_RID_GENCONFIG:
		cfg = (struct an_ltv_genconfig *)areq;
		bcopy((char *)&cfg->an_macaddr,
		    (char *)&sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);
		bcopy((char *)&cfg->an_macaddr, LLADDR(ifp->if_sadl),
		    ETHER_ADDR_LEN);

		bcopy((char *)cfg, (char *)&sc->an_config,
			sizeof(struct an_ltv_genconfig));
		break;
	case AN_RID_SSIDLIST:
		ssid = (struct an_ltv_ssidlist *)areq;
		bcopy((char *)ssid, (char *)&sc->an_ssidlist,
			sizeof(struct an_ltv_ssidlist));
		break;
	case AN_RID_APLIST:
		ap = (struct an_ltv_aplist *)areq;
		bcopy((char *)ap, (char *)&sc->an_aplist,
			sizeof(struct an_ltv_aplist));
		break;
	case AN_RID_TX_SPEED:
		sp = (struct an_ltv_gen *)areq;
		sc->an_tx_rate = sp->an_val[0];
		break;
	case AN_RID_WEP_VOLATILE:
		/* Disable the MAC */
		an_cmd(sc, AN_CMD_DISABLE, 0);

		/* Just write the key, we dont' want to save it */
		an_write_record(sc, (struct an_ltv_gen *)areq);

		/* Turn the MAC back on */
		an_cmd(sc, AN_CMD_ENABLE, 0);

		break;
	case AN_RID_WEP_PERMANENT:
		/* Disable the MAC */
		an_cmd(sc, AN_CMD_DISABLE, 0);

		/* Just write the key, the card will save it in this mode */
		an_write_record(sc, (struct an_ltv_gen *)areq);

		/* Turn the MAC back on */
		an_cmd(sc, AN_CMD_ENABLE, 0);

		break;
	default:
		printf("%s: unknown RID: %x\n",
		    sc->sc_dev.dv_xname, areq->an_type);
		return;
	}

	/* Reinitialize the card. */
	if (ifp->if_flags & IFF_UP)
		an_init(sc);
}

/*
 * We can't change the NIC configuration while the MAC is enabled,
 * so in order to turn on RX monitor mode, we have to turn the MAC
 * off first.
 */
void
an_promisc(sc, promisc)
	struct an_softc		*sc;
	int			promisc;
{
	struct an_ltv_genconfig genconf;

	/* Disable the MAC. */
	an_cmd(sc, AN_CMD_DISABLE, 0);

	/* Set RX mode. */
	if (promisc &&
	    !(sc->an_config.an_rxmode & AN_RXMODE_LAN_MONITOR_CURBSS)) {
		sc->an_rxmode = sc->an_config.an_rxmode;
		sc->an_config.an_rxmode |=
		    AN_RXMODE_LAN_MONITOR_CURBSS;
	} else {
		sc->an_config.an_rxmode = sc->an_rxmode;
	}

	/* Transfer the configuration to the NIC */
	genconf = sc->an_config;
	genconf.an_len = sizeof(struct an_ltv_genconfig);
	genconf.an_type = AN_RID_GENCONFIG;
	if (an_write_record(sc, (struct an_ltv_gen *)&genconf)) {
		printf("%s: failed to set configuration\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	/* Turn the MAC back on. */
	an_cmd(sc, AN_CMD_ENABLE, 0);
}

int
an_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	int			s, error = 0;
	struct an_softc		*sc;
	struct an_req		areq;
	struct ifreq		*ifr;
	struct proc		*p = curproc;
	struct ifaddr		*ifa = (struct ifaddr *)data;

	s = splimp();

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;

	if (sc->an_gone) {
		splx(s);
		return(ENODEV);
	}

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			an_init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			an_init(sc);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->an_if_flags & IFF_PROMISC)) {
				an_promisc(sc, 1);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->an_if_flags & IFF_PROMISC) {
				an_promisc(sc, 0);
				an_reset(sc);
			}
			an_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				an_stop(sc);
		}
		sc->an_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->an_ifmedia, command);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* The Aironet has no multicast filter. */
		error = 0;
		break;
	case SIOCGAIRONET:
		error = copyin(ifr->ifr_data, &areq, sizeof(areq));
		if (error)
			break;
#ifdef ANCACHE
		if (areq.an_type == AN_RID_ZERO_CACHE) {
			error = suser(p, 0);
			if (error)
				break;
			sc->an_sigitems = sc->an_nextitem = 0;
			break;
		} else if (areq.an_type == AN_RID_READ_CACHE) {
			char *pt = (char *)&areq.an_val;
			bcopy((char *)&sc->an_sigitems, (char *)pt,
			    sizeof(int));
			pt += sizeof(int);
			areq.an_len = sizeof(int) / 2;
			bcopy((char *)&sc->an_sigcache, (char *)pt,
			    sizeof(struct an_sigcache) * sc->an_sigitems);
			areq.an_len += ((sizeof(struct an_sigcache) *
			    sc->an_sigitems) / 2) + 1;
		} else
#endif
		if (an_read_record(sc, (struct an_ltv_gen *)&areq)) {
			error = EINVAL;
			break;
		}
		error = copyout(&areq, ifr->ifr_data, sizeof(areq));
		break;
	case SIOCSAIRONET:
		error = suser(p, 0);
		if (error)
			break;
		error = copyin(ifr->ifr_data, &areq, sizeof(areq));
		if (error)
			break;
		an_setdef(sc, &areq);
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return(error);
}

int
an_init_tx_ring(sc)
	struct an_softc		*sc;
{
	int			i;
	int			id;

	if (sc->an_gone)
		return (0);

	for (i = 0; i < AN_TX_RING_CNT; i++) {
		if (an_alloc_nicmem(sc, ETHER_MAX_LEN + 0x44, &id))
			return(ENOMEM);
		sc->an_rdata.an_tx_fids[i] = id;
		sc->an_rdata.an_tx_ring[i] = 0;
	}

	sc->an_rdata.an_tx_prod = 0;
	sc->an_rdata.an_tx_cons = 0;

	return(0);
}

void
an_init(sc)
	struct an_softc *sc;
{
	struct ifnet		*ifp = &sc->sc_arpcom.ac_if;
	struct an_ltv_ssidlist	ssid;
	struct an_ltv_aplist	aplist;
	struct an_ltv_genconfig	genconf;
	int	s;

	if (sc->an_gone)
		return;

	s = splimp();

	if (ifp->if_flags & IFF_RUNNING)
		an_stop(sc);

	sc->an_associated = 0;

	/* Allocate the TX buffers */
	if (an_init_tx_ring(sc)) {
		printf("%s: tx buffer allocation failed\n",
		    sc->sc_dev.dv_xname);
		splx(s);
		return;
	}

	/* Set our MAC address. */
	bcopy((char *)&sc->sc_arpcom.ac_enaddr,
	    (char *)&sc->an_config.an_macaddr, ETHER_ADDR_LEN);

	if (ifp->if_flags & IFF_BROADCAST)
		sc->an_config.an_rxmode = AN_RXMODE_BC_ADDR;
	else
		sc->an_config.an_rxmode = AN_RXMODE_ADDR;

	if (ifp->if_flags & IFF_MULTICAST)
		sc->an_config.an_rxmode = AN_RXMODE_BC_MC_ADDR;

	/* Initialize promisc mode. */
	if (ifp->if_flags & IFF_PROMISC)
		sc->an_config.an_rxmode |= AN_RXMODE_LAN_MONITOR_CURBSS;

	sc->an_rxmode = sc->an_config.an_rxmode;

	/* Set the ssid list */
	ssid = sc->an_ssidlist;
	ssid.an_type = AN_RID_SSIDLIST;
	ssid.an_len = sizeof(struct an_ltv_ssidlist);
	if (an_write_record(sc, (struct an_ltv_gen *)&ssid)) {
		printf("%s: failed to set ssid list\n", sc->sc_dev.dv_xname);
		splx(s);
		return;
	}

	/* Set the AP list */
	aplist = sc->an_aplist;
	aplist.an_type = AN_RID_APLIST;
	aplist.an_len = sizeof(struct an_ltv_aplist);
	if (an_write_record(sc, (struct an_ltv_gen *)&aplist)) {
		printf("%s: failed to set AP list\n", sc->sc_dev.dv_xname);
		splx(s);
		return;
	}

	/* Set the configuration in the NIC */
	genconf = sc->an_config;
	genconf.an_len = sizeof(struct an_ltv_genconfig);
	genconf.an_type = AN_RID_GENCONFIG;
	if (an_write_record(sc, (struct an_ltv_gen *)&genconf)) {
		printf("%s: failed to set configuration\n",
		    sc->sc_dev.dv_xname);
		splx(s);
		return;
	}

	/* Enable the MAC */
	if (an_cmd(sc, AN_CMD_ENABLE, 0)) {
		printf("%s: failed to enable MAC\n", sc->sc_dev.dv_xname);
		splx(s);
		return;
	}

	/* enable interrupts */
	CSR_WRITE_2(sc, AN_INT_EN, AN_INTRS);

	splx(s);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	TIMEOUT(sc->an_stat_ch, an_stats_update, sc, hz);
}

void
an_start(ifp)
	struct ifnet		*ifp;
{
	struct an_softc		*sc;
	struct mbuf		*m0 = NULL;
	struct an_txframe_802_3	tx_frame_802_3;
	struct ether_header	*eh;
	u_int16_t		len;
	int			id;
	int			idx;
	unsigned char           txcontrol;
	int			pkts = 0;

	sc = ifp->if_softc;

	if (sc->an_gone)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	if (!sc->an_associated)
		return;

	idx = sc->an_rdata.an_tx_prod;
	bzero((char *)&tx_frame_802_3, sizeof(tx_frame_802_3));

	while(sc->an_rdata.an_tx_ring[idx] == 0) {
		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		pkts++;
		id = sc->an_rdata.an_tx_fids[idx];
		eh = mtod(m0, struct ether_header *);

		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame_802_3.an_tx_dst_addr, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_shost,
		    (char *)&tx_frame_802_3.an_tx_src_addr, ETHER_ADDR_LEN);

		len = m0->m_pkthdr.len - 12;  /* minus src/dest mac & type */
		tx_frame_802_3.an_tx_802_3_payload_len = htole16(len);

		m_copydata(m0, sizeof(struct ether_header) - 2, len,
		    (caddr_t)&sc->an_txbuf);

		txcontrol=AN_TXCTL_8023;
		/* write the txcontrol only */
		an_write_data(sc, id, 0x08, (caddr_t)&txcontrol,
			      sizeof(txcontrol));

		/* 802_3 header */
		an_write_data(sc, id, 0x34, (caddr_t)&tx_frame_802_3,
			      sizeof(struct an_txframe_802_3));

		/* in mbuf header type is just before payload */
		an_write_data(sc, id, 0x44, (caddr_t)&sc->an_txbuf, len);

		/*
		 * If there's a BPF listener, bounce a copy of
		 * this frame to him.
		 */
#if NBPFILTER > 0
		if (ifp->if_bpf)
			BPF_MTAP(ifp, m0);
#endif

		m_freem(m0);
		m0 = NULL;

		sc->an_rdata.an_tx_ring[idx] = id;
		if (an_cmd(sc, AN_CMD_TX, id))
			printf("%s: xmit failed\n", sc->sc_dev.dv_xname);

		AN_INC(idx, AN_TX_RING_CNT);
	}
	if (pkts == 0)
		return;

	if (m0 != NULL)
		ifp->if_flags |= IFF_OACTIVE;

	sc->an_rdata.an_tx_prod = idx;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

void
an_stop(sc)
	struct an_softc		*sc;
{
	struct ifnet		*ifp;
	int			i;

	if (sc->an_gone)
		return;

	ifp = &sc->sc_arpcom.ac_if;

	an_cmd(sc, AN_CMD_FORCE_SYNCLOSS, 0);
	CSR_WRITE_2(sc, AN_INT_EN, 0);
	an_cmd(sc, AN_CMD_DISABLE, 0);

	for (i = 0; i < AN_TX_RING_CNT; i++)
		an_cmd(sc, AN_CMD_DEALLOC_MEM, sc->an_rdata.an_tx_fids[i]);

	UNTIMEOUT(an_stats_update, sc, sc->an_stat_ch);

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
}

void
an_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct an_softc		*sc;

	sc = ifp->if_softc;

	if (sc->an_gone)
		return;

	printf("%s: device timeout\n", sc->sc_dev.dv_xname);

	an_reset(sc);
	an_init(sc);

	ifp->if_oerrors++;
}

void
an_shutdown(self)
	void *self;
{
	an_stop(self);
}

#ifdef ANCACHE
/* Aironet signal strength cache code.
 * store signal/noise/quality on per MAC src basis in
 * a small fixed cache.  The cache wraps if > MAX slots
 * used.  The cache may be zeroed out to start over.
 * Two simple filters exist to reduce computation:
 * 1. ip only (literally 0x800) which may be used
 * to ignore some packets.  It defaults to ip only.
 * it could be used to focus on broadcast, non-IP 802.11 beacons.
 * 2. multicast/broadcast only.  This may be used to
 * ignore unicast packets and only cache signal strength
 * for multicast/broadcast packets (beacons); e.g., Mobile-IP
 * beacons and not unicast traffic.
 *
 * The cache stores (MAC src(index), IP src (major clue), signal,
 *	quality, noise)
 *
 * No apologies for storing IP src here.  It's easy and saves much
 * trouble elsewhere.  The cache is assumed to be INET dependent,
 * although it need not be.
 *
 * Note: the Aironet only has a single byte of signal strength value
 * in the rx frame header, and it's not scaled to anything sensible.
 * This is kind of lame, but it's all we've got.
 */

#ifdef documentation

int an_sigitems;                                /* number of cached entries */
struct an_sigcache an_sigcache[MAXANCACHE];  /*  array of cache entries */
int an_nextitem;                                /*  index/# of entries */


#endif

/* control variables for cache filtering.  Basic idea is
 * to reduce cost (e.g., to only Mobile-IP agent beacons
 * which are broadcast or multicast).  Still you might
 * want to measure signal strength anth unicast ping packets
 * on a pt. to pt. ant. setup.
 */
/* set true if you want to limit cache items to broadcast/mcast
 * only packets (not unicast).  Useful for mobile-ip beacons which
 * are broadcast/multicast at network layer.  Default is all packets
 * so ping/unicast anll work say anth pt. to pt. antennae setup.
 */
#if 0
static int an_cache_mcastonly = 0;
SYSCTL_INT(_machdep, OID_AUTO, an_cache_mcastonly, CTLFLAG_RW,
	&an_cache_mcastonly, 0, "");

/* set true if you want to limit cache items to IP packets only
*/
static int an_cache_iponly = 1;
SYSCTL_INT(_machdep, OID_AUTO, an_cache_iponly, CTLFLAG_RW,
	&an_cache_iponly, 0, "");
#endif

/*
 * an_cache_store, per rx packet store signal
 * strength in MAC (src) indexed cache.
 */
void
an_cache_store(sc, eh, m, rx_quality)
	struct an_softc *sc;
	struct ether_header *eh;
	struct mbuf *m;
	unsigned short rx_quality;
{
	static int cache_slot = 0;	/* use this cache entry */
	static int wrapindex = 0;       /* next "free" cache entry */
	int i, saanp = 0;

	/* filters:
	 * 1. ip only
	 * 2. configurable filter to throw out unicast packets,
	 * keep multicast only.
	 */

	if ((ntohs(eh->ether_type) == 0x800))
		saanp = 1;

	/* filter for ip packets only */
	if (sc->an_cache_iponly && !saanp)
		return;

	/* filter for broadcast/multicast only */
	if (sc->an_cache_mcastonly && ((eh->ether_dhost[0] & 1) == 0))
		return;

#ifdef SIGDEBUG
	printf("an: q value %x (MSB=0x%x, LSB=0x%x) \n",
	    rx_quality & 0xffff, rx_quality >> 8, rx_quality & 0xff);
#endif
	/* do a linear search for a matching MAC address
	 * in the cache table
	 * . MAC address is 6 bytes,
	 * . var w_nextitem holds total number of entries already cached
	 */
	for(i = 0; i < sc->an_nextitem; i++)
		if (!bcmp(eh->ether_shost, sc->an_sigcache[i].macsrc, 6))
			/* Match!,
			 * so we already have this entry, update the data
			 */
			break;

	/* did we find a matching mac address?
	 * if yes, then overwrite a previously existing cache entry
	 */
	if (i < sc->an_nextitem)
		cache_slot = i;

	/* else, have a new address entry,so
	 * add this new entry,
	 * if table full, then we need to replace LRU entry
	 */
	else {

		/* check for space in cache table
		 * note: an_nextitem also holds number of entries
		 * added in the cache table
		 */
		if (sc->an_nextitem < MAXANCACHE) {
			cache_slot = sc->an_nextitem;
			sc->an_nextitem++;
			sc->an_sigitems = sc->an_nextitem;
		}
		/* no space found, so simply wrap anth wrap index
		 * and "zap" the next entry
		 */
		else {
			if (wrapindex == MAXANCACHE)
				wrapindex = 0;
			cache_slot = wrapindex++;
		}
	}

	/* invariant: cache_slot now points at some slot
	 * in cache.
	 */
	if (cache_slot < 0 || cache_slot >= MAXANCACHE) {
		log(LOG_ERR, "an_cache_store, bad index: %d of "
		    "[0..%d], gross cache error\n",
		    cache_slot, MAXANCACHE);
		return;
	}

	/*  store items in cache
	 *  .ip source address
	 *  .mac src
	 *  .signal, etc.
	 */
	if (saanp) {
		struct ip *ip = (struct ip *)(mtod(m, char *) + ETHER_HDR_LEN);
		sc->an_sigcache[cache_slot].ipsrc = ntohl(ip->ip_src.s_addr);
	}
	bcopy(eh->ether_shost, sc->an_sigcache[cache_slot].macsrc, 6);

	sc->an_sigcache[cache_slot].signal = rx_quality;
}
#endif

int
an_media_change(ifp)
	struct ifnet		*ifp;
{
	struct an_softc *sc = ifp->if_softc;
	int otype = sc->an_config.an_opmode;
	int orate = sc->an_tx_rate;

	if ((sc->an_ifmedia.ifm_cur->ifm_media & IFM_IEEE80211_ADHOC) != 0)
		sc->an_config.an_opmode = AN_OPMODE_IBSS_ADHOC;
	else
		sc->an_config.an_opmode = AN_OPMODE_INFRASTRUCTURE_STATION;

	switch (IFM_SUBTYPE(sc->an_ifmedia.ifm_cur->ifm_media)) {
	case IFM_IEEE80211_DS1:
		sc->an_tx_rate = AN_RATE_1MBPS;
		break;
	case IFM_IEEE80211_DS2:
		sc->an_tx_rate = AN_RATE_2MBPS;
		break;
	case IFM_IEEE80211_DS5:
		sc->an_tx_rate = AN_RATE_5_5MBPS;
		break;
	case IFM_IEEE80211_DS11:
		sc->an_tx_rate = AN_RATE_11MBPS;
		break;
	case IFM_AUTO:
		sc->an_tx_rate = 0;
		break;
	}

	if (otype != sc->an_config.an_opmode ||
	    orate != sc->an_tx_rate)
		an_init(sc);

	return(0);
}

void
an_media_status(ifp, imr)
	struct ifnet		*ifp;
	struct ifmediareq	*imr;
{
	struct an_ltv_status	status;
	struct an_softc		*sc = ifp->if_softc;

	status.an_len = sizeof(status);
	status.an_type = AN_RID_STATUS;
	if (an_read_record(sc, (struct an_ltv_gen *)&status)) {
		/* If the status read fails, just lie. */
		imr->ifm_active = sc->an_ifmedia.ifm_cur->ifm_media;
		imr->ifm_status = IFM_AVALID|IFM_ACTIVE;
	}

	if (sc->an_tx_rate == 0) {
		imr->ifm_active = IFM_IEEE80211|IFM_AUTO;
		if (sc->an_config.an_opmode == AN_OPMODE_IBSS_ADHOC)
			imr->ifm_active |= IFM_IEEE80211_ADHOC;
		switch (status.an_current_tx_rate) {
		case AN_RATE_1MBPS:
			imr->ifm_active |= IFM_IEEE80211_DS1;
			break;
		case AN_RATE_2MBPS:
			imr->ifm_active |= IFM_IEEE80211_DS2;
			break;
		case AN_RATE_5_5MBPS:
			imr->ifm_active |= IFM_IEEE80211_DS5;
			break;
		case AN_RATE_11MBPS:
			imr->ifm_active |= IFM_IEEE80211_DS11;
			break;
		}
	} else {
		imr->ifm_active = sc->an_ifmedia.ifm_cur->ifm_media;
	}

	imr->ifm_status = IFM_AVALID;
	if (sc->an_config.an_opmode == AN_OPMODE_IBSS_ADHOC)
		imr->ifm_status |= IFM_ACTIVE;
	else if (status.an_opmode & AN_STATUS_OPMODE_ASSOCIATED)
		imr->ifm_status |= IFM_ACTIVE;
}
