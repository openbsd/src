/*	$OpenBSD: if_wi.c,v 1.8 2000/02/03 00:56:45 angelos Exp $	*/

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
 *	From: if_wi.c,v 1.7 1999/07/04 14:40:22 wpaul Exp $
 */

/*
 * Lucent WaveLAN/IEEE 802.11 PCMCIA driver for FreeBSD.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The WaveLAN/IEEE adapter is the second generation of the WaveLAN
 * from Lucent. Unlike the older cards, the new ones are programmed
 * entirely via a firmware-driven controller called the Hermes.
 * Unfortunately, Lucent will not release the Hermes programming manual
 * without an NDA (if at all). What they do release is an API library
 * called the HCF (Hardware Control Functions) which is supposed to
 * do the device-specific operations of a device driver for you. The
 * publically available version of the HCF library (the 'HCF Light') is 
 * a) extremely gross, b) lacks certain features, particularly support
 * for 802.11 frames, and c) is contaminated by the GNU Public License.
 *
 * This driver does not use the HCF or HCF Light at all. Instead, it
 * programs the Hermes controller directly, using information gleaned
 * from the HCF Light code and corresponding documentation.
 *
 * This driver supports both the PCMCIA and ISA versions of the
 * WaveLAN/IEEE cards. Note however that the ISA card isn't really
 * anything of the sort: it's actually a PCMCIA bridge adapter
 * that fits into an ISA slot, into which a PCMCIA WaveLAN card is
 * inserted. Consequently, you need to use the pccard support for
 * both the ISA and PCMCIA adapters.
 */

#define WI_HERMES_AUTOINC_WAR	/* Work around data write autoinc bug. */
#define WI_HERMES_STATS_WAR	/* Work around stats counter bug. */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/device.h>

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

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/bus.h>

#include <i386/isa/icu.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>
#include <dev/pcmcia/if_wireg.h>
#include <dev/pcmcia/if_wavelan_ieee.h>

#define TIMEOUT(handle,func,arg,time) timeout((func), (arg), (time))
#define UNTIMEOUT(func,arg,handle) untimeout((func), (arg))
#define BPF_MTAP(if,mbuf) bpf_mtap((if)->if_bpf, (mbuf))
#define BPFATTACH(if_bpf,if,dlt,sz) bpfattach((if_bpf), (if), (dlt), (sz))
#define STATIC
#define WI_PRT_FMT "%s"
#define WI_PRT_ARG(sc) (sc)->sc_dev.dv_xname

#ifdef WIDEBUG

u_int32_t	widebug = WIDEBUG;

#define WID_INTR	0x01
#define WID_START	0x02
#define WID_IOCTL	0x04
#define WID_INIT	0x08
#define WID_STOP	0x10
#define WID_RESET	0x20

#define DPRINTF(mask,args) if (widebug & (mask)) printf args;

#else	/* !WIDEBUG */
#define DPRINTF(mask,args)
#endif	/* WIDEBUG */

#if !defined(lint)
static const char rcsid[] =
	"$OpenBSD: if_wi.c,v 1.8 2000/02/03 00:56:45 angelos Exp $";
#endif	/* lint */

#ifdef foo
static u_int8_t	wi_mcast_addr[6] = { 0x01, 0x60, 0x1D, 0x00, 0x01, 0x00 };
#endif

STATIC void wi_reset		__P((struct wi_softc *));
STATIC int wi_ioctl		__P((struct ifnet *, u_long, caddr_t));
STATIC void wi_init		__P((void *));
STATIC void wi_start		__P((struct ifnet *));
STATIC void wi_stop		__P((struct wi_softc *));
STATIC void wi_watchdog		__P((struct ifnet *));
STATIC void wi_shutdown		__P((void *));
STATIC void wi_rxeof		__P((struct wi_softc *));
STATIC void wi_txeof		__P((struct wi_softc *, int));
STATIC void wi_update_stats	__P((struct wi_softc *));
STATIC void wi_setmulti		__P((struct wi_softc *));

STATIC int wi_cmd		__P((struct wi_softc *, int, int));
STATIC int wi_read_record	__P((struct wi_softc *, struct wi_ltv_gen *));
STATIC int wi_write_record	__P((struct wi_softc *, struct wi_ltv_gen *));
STATIC int wi_read_data		__P((struct wi_softc *, int,
					int, caddr_t, int));
STATIC int wi_write_data	__P((struct wi_softc *, int,
					int, caddr_t, int));
STATIC int wi_seek		__P((struct wi_softc *, int, int, int));
STATIC int wi_alloc_nicmem	__P((struct wi_softc *, int, int *));
STATIC void wi_inquire		__P((void *));
STATIC void wi_setdef		__P((struct wi_softc *, struct wi_req *));
STATIC int wi_mgmt_xmit		__P((struct wi_softc *, caddr_t, int));

int	wi_pcmcia_match		__P((struct device *, void *, void *));
void	wi_pcmcia_attach	__P((struct device *, struct device *, void *));
int	wi_pcmcia_detach	__P((struct device *, int));
int	wi_pcmcia_activate	__P((struct device *, enum devact));
int	wi_intr			__P((void *));

/* Autoconfig definition of driver back-end */
struct cfdriver wi_cd = {
	NULL, "wi", DV_IFNET
};

struct cfattach wi_ca = {
	sizeof (struct wi_softc), wi_pcmcia_match, wi_pcmcia_attach,
	wi_pcmcia_detach, wi_pcmcia_activate
};

int
wi_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pcmcia_attach_args *pa = aux;
	
	if (pa->pf->function != PCMCIA_FUNCTION_NETWORK)
		return (0);

	switch (pa->manufacturer) {
	case PCMCIA_VENDOR_LUCENT:
		/* XXX Per-productid checking here. */
		return (1);

	default:
		return (0);
	}
}

void
wi_pcmcia_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wi_softc		*sc = (struct wi_softc *)self;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_function	*pf = pa->pf;
	struct pcmcia_config_entry *cfe = pf->cfe_head.sqh_first;
	struct wi_ltv_macaddr	mac;
	struct wi_ltv_gen	gen;
	struct ifnet		*ifp;
	int			state = 0;

	sc->sc_pf = pf;

	/* Enable the card. */
	pcmcia_function_init(pf, cfe);
	if (pcmcia_function_enable(pf)) {
		printf(": function enable failed\n");
		goto bad;
	}
	state++;

	if (pcmcia_io_alloc(pf, 0, WI_IOSIZ, WI_IOSIZ, &sc->sc_pcioh)) {
		printf(": can't alloc i/o space\n");
		goto bad;
	}
	state++;

	if (pcmcia_io_map(pf, PCMCIA_WIDTH_IO16, 0, WI_IOSIZ, &sc->sc_pcioh,
	    &sc->sc_io_window)) {
		printf(": can't map io space\n");
		goto bad;
	}
	state++;

	sc->wi_gone = 0;
	sc->wi_btag = sc->sc_pcioh.iot;
	sc->wi_bhandle = sc->sc_pcioh.ioh;

	/* Make sure interrupts are disabled. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	wi_reset(sc);

	/* Read the station address. */
	mac.wi_type = WI_RID_MAC_NODE;
	mac.wi_len = 4;
	wi_read_record(sc, (struct wi_ltv_gen *)&mac);
	bcopy((char *)&mac.wi_mac_addr, (char *)&sc->arpcom.ac_enaddr,
	    ETHER_ADDR_LEN);

	printf(": address %s", ether_sprintf(sc->arpcom.ac_enaddr));

	ifp = &sc->arpcom.ac_if;
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wi_ioctl;
	ifp->if_start = wi_start;
	ifp->if_watchdog = wi_watchdog;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	bzero(sc->wi_node_name, sizeof(sc->wi_node_name));
	bcopy(WI_DEFAULT_NODENAME, sc->wi_node_name,
	    sizeof(WI_DEFAULT_NODENAME) - 1);

	bzero(sc->wi_net_name, sizeof(sc->wi_net_name));
	bcopy(WI_DEFAULT_NETNAME, sc->wi_net_name,
	    sizeof(WI_DEFAULT_NETNAME) - 1);

	bzero(sc->wi_ibss_name, sizeof(sc->wi_ibss_name));
	bcopy(WI_DEFAULT_IBSS, sc->wi_ibss_name,
	    sizeof(WI_DEFAULT_IBSS) - 1);

	sc->wi_portnum = WI_DEFAULT_PORT;
	sc->wi_ptype = WI_PORTTYPE_ADHOC;
	sc->wi_ap_density = WI_DEFAULT_AP_DENSITY;
	sc->wi_rts_thresh = WI_DEFAULT_RTS_THRESH;
	sc->wi_tx_rate = WI_DEFAULT_TX_RATE;
	sc->wi_max_data_len = WI_DEFAULT_DATALEN;
	sc->wi_create_ibss = WI_DEFAULT_CREATE_IBSS;
	sc->wi_pm_enabled = WI_DEFAULT_PM_ENABLED;
	sc->wi_max_sleep = WI_DEFAULT_MAX_SLEEP;

	/*
	 * Read the default channel from the NIC. This may vary
	 * depending on the country where the NIC was purchased, so
	 * we can't hard-code a default and expect it to work for
	 * everyone.
	 */
	gen.wi_type = WI_RID_OWN_CHNL;
	gen.wi_len = 2;
	wi_read_record(sc, &gen);
	sc->wi_channel = gen.wi_val;

	/*
	 * Find out if we support WEP on this card.
	 */
	gen.wi_type = WI_RID_WEP_AVAIL;
	gen.wi_len = 2;
	wi_read_record(sc, &gen);
	sc->wi_has_wep = gen.wi_val;

	bzero((char *)&sc->wi_stats, sizeof(sc->wi_stats));

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	BPFATTACH(&sc->arpcom.ac_if.if_bpf, ifp, DLT_EN10MB,
	    sizeof(struct ether_header));
#endif

	shutdownhook_establish(wi_shutdown, sc);

	/* Establish the interrupt. */
	sc->sc_ih = pcmcia_intr_establish(pa->pf, IPL_NET, wi_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		goto bad;
	}
	printf("\n");

	wi_init(sc);
	wi_stop(sc);
	return;

bad:
	if (state > 2)
		pcmcia_io_unmap(pf, sc->sc_io_window);
	if (state > 1)
		pcmcia_io_free(pf, &sc->sc_pcioh);
	if (state > 0)
		pcmcia_function_disable(pf);
}

int
wi_pcmcia_detach(dev, flags)
	struct device *dev;
	int flags;
{
	struct wi_softc *sc = (struct wi_softc *)dev;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int rv = 0;

	pcmcia_io_unmap(sc->sc_pf, sc->sc_io_window);
	pcmcia_io_free(sc->sc_pf, &sc->sc_pcioh);

	ether_ifdetach(ifp);
	if_detach(ifp);

	return (rv);
}

int
wi_pcmcia_activate(dev, act)
	struct device *dev;
	enum devact act;
{
	struct wi_softc *sc = (struct wi_softc *)dev;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int s;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		pcmcia_function_enable(sc->sc_pf);
		printf("%s:", WI_PRT_ARG(sc));
		sc->sc_ih =
		    pcmcia_intr_establish(sc->sc_pf, IPL_NET, wi_intr, sc);
		printf("\n");
		wi_init(sc);
		break;

	case DVACT_DEACTIVATE:
		ifp->if_timer = 0;
		if (ifp->if_flags & IFF_RUNNING)
			wi_stop(sc);
		pcmcia_function_disable(sc->sc_pf);
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		break;
	}
	splx(s);
	return (0);
}

int wi_intr(vsc)
	void			*vsc;
{
	struct wi_softc		*sc = vsc;
	struct ifnet		*ifp;
	u_int16_t		status;

	DPRINTF(WID_INTR, ("wi_intr: sc %p\n", sc));

	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP)) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);
		CSR_WRITE_2(sc, WI_INT_EN, 0);
		return (0);
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, WI_INT_EN, 0);

	status = CSR_READ_2(sc, WI_EVENT_STAT);
	CSR_WRITE_2(sc, WI_EVENT_ACK, ~WI_INTRS);

	if (status & WI_EV_RX) {
		wi_rxeof(sc);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_RX);
	}

	if (status & WI_EV_TX) {
		wi_txeof(sc, status);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_TX);
	}

	if (status & WI_EV_ALLOC) {
		int			id;
		id = CSR_READ_2(sc, WI_ALLOC_FID);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);
		if (id == sc->wi_tx_data_id)
			wi_txeof(sc, status);
	}

	if (status & WI_EV_INFO) {
		wi_update_stats(sc);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_INFO);
	}

	if (status & WI_EV_TX_EXC) {
		wi_txeof(sc, status);
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_TX_EXC);
	}

	if (status & WI_EV_INFO_DROP) {
		CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_INFO_DROP);
	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

	if (ifp->if_snd.ifq_head != NULL)
		wi_start(ifp);

	return (1);
}

STATIC void wi_rxeof(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;
	struct ether_header	*eh;
	struct wi_frame		rx_frame;
	struct mbuf		*m;
	int			id;

	ifp = &sc->arpcom.ac_if;

	id = CSR_READ_2(sc, WI_RX_FID);

	/* First read in the frame header */
	if (wi_read_data(sc, id, 0, (caddr_t)&rx_frame, sizeof(rx_frame))) {
		ifp->if_ierrors++;
		return;
	}

	if (rx_frame.wi_status & WI_STAT_ERRSTAT) {
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

	eh = mtod(m, struct ether_header *);
	m->m_pkthdr.rcvif = ifp;

	if (rx_frame.wi_status == WI_STAT_1042 ||
	    rx_frame.wi_status == WI_STAT_TUNNEL ||
	    rx_frame.wi_status == WI_STAT_WMP_MSG) {
		if((rx_frame.wi_dat_len + WI_SNAPHDR_LEN) > MCLBYTES) {
			printf(WI_PRT_FMT ": oversized packet received "
			    "(wi_dat_len=%d, wi_status=0x%x)\n",
			    WI_PRT_ARG(sc), rx_frame.wi_dat_len,
			    rx_frame.wi_status);
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
		m->m_pkthdr.len = m->m_len =
		    rx_frame.wi_dat_len + WI_SNAPHDR_LEN;

		bcopy((char *)&rx_frame.wi_addr1,
		    (char *)&eh->ether_dhost, ETHER_ADDR_LEN);
		bcopy((char *)&rx_frame.wi_addr2,
		    (char *)&eh->ether_shost, ETHER_ADDR_LEN);
		bcopy((char *)&rx_frame.wi_type,
		    (char *)&eh->ether_type, sizeof(u_int16_t));

		if (wi_read_data(sc, id, WI_802_11_OFFSET,
		    mtod(m, caddr_t) + sizeof(struct ether_header),
		    m->m_len + 2)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
	} else {
		if((rx_frame.wi_dat_len +
		    sizeof(struct ether_header)) > MCLBYTES) {
			printf(WI_PRT_FMT ": oversized packet received "
			    "(wi_dat_len=%d, wi_status=0x%x)\n",
			    WI_PRT_ARG(sc), rx_frame.wi_dat_len,
			    rx_frame.wi_status);
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
		m->m_pkthdr.len = m->m_len =
		    rx_frame.wi_dat_len + sizeof(struct ether_header);

		if (wi_read_data(sc, id, WI_802_3_OFFSET,
		    mtod(m, caddr_t), m->m_len + 2)) {
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
	}

	ifp->if_ipackets++;

#if NBPFILTER > 0
	/* Handle BPF listeners. */
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m);
#endif

	/* Receive packet. */
	m_adj(m, sizeof(struct ether_header));
	ether_input(ifp, eh, m);

	return;
}

STATIC void wi_txeof(sc, status)
	struct wi_softc		*sc;
	int			status;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status & WI_EV_TX_EXC)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	return;
}

void wi_inquire(xsc)
	void			*xsc;
{
	struct wi_softc		*sc;
	struct ifnet		*ifp;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	TIMEOUT(sc->wi_stat_ch, wi_inquire, sc, hz * 60);

	/* Don't do this while we're transmitting */
	if (ifp->if_flags & IFF_OACTIVE)
		return;

	wi_cmd(sc, WI_CMD_INQUIRE, WI_INFO_COUNTERS);

	return;
}

void wi_update_stats(sc)
	struct wi_softc		*sc;
{
	struct wi_ltv_gen	gen;
	u_int16_t		id;
	struct ifnet		*ifp;
	u_int32_t		*ptr;
	int			i;
	u_int16_t		t;

	ifp = &sc->arpcom.ac_if;

	id = CSR_READ_2(sc, WI_INFO_FID);

	wi_read_data(sc, id, 0, (char *)&gen, 4);

	if (gen.wi_type != WI_INFO_COUNTERS ||
	    gen.wi_len > (sizeof(sc->wi_stats) / 4) + 1)
		return;

	ptr = (u_int32_t *)&sc->wi_stats;

	for (i = 0; i < gen.wi_len - 1; i++) {
		t = CSR_READ_2(sc, WI_DATA1);
#ifdef WI_HERMES_STATS_WAR
		if (t > 0xF000)
			t = ~t & 0xFFFF;
#endif
		ptr[i] += t;
	}

	ifp->if_collisions = sc->wi_stats.wi_tx_single_retries +
	    sc->wi_stats.wi_tx_multi_retries +
	    sc->wi_stats.wi_tx_retry_limit;

	return;
}

STATIC int wi_cmd(sc, cmd, val)
	struct wi_softc		*sc;
	int			cmd;
	int			val;
{
	int			i, s = 0;

	CSR_WRITE_2(sc, WI_PARAM0, val);
	CSR_WRITE_2(sc, WI_COMMAND, cmd);

	for (i = 0; i < WI_TIMEOUT; i++) {
		/*
		 * Wait for 'command complete' bit to be
		 * set in the event status register.
		 */
		s = CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_CMD;
		if (s) {
			/* Ack the event and read result code. */
			s = CSR_READ_2(sc, WI_STATUS);
			CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_CMD);
#ifdef foo
			if ((s & WI_CMD_CODE_MASK) != (cmd & WI_CMD_CODE_MASK))
				return(EIO);
#endif
			if (s & WI_STAT_CMD_RESULT)
				return(EIO);
			break;
		}
	}

	if (i == WI_TIMEOUT)
		return(ETIMEDOUT);

	return(0);
}

STATIC void wi_reset(sc)
	struct wi_softc		*sc;
{
	DPRINTF(WID_RESET, ("wi_reset: sc %p\n", sc));

	if (wi_cmd(sc, WI_CMD_INI, 0))
		printf(WI_PRT_FMT ": init failed\n", WI_PRT_ARG(sc));
	CSR_WRITE_2(sc, WI_INT_EN, 0);
	CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

	/* Calibrate timer. */
	WI_SETVAL(WI_RID_TICK_TIME, 8);

	return;
}

/*
 * Read an LTV record from the NIC.
 */
STATIC int wi_read_record(sc, ltv)
	struct wi_softc		*sc;
	struct wi_ltv_gen	*ltv;
{
	u_int16_t		*ptr;
	int			i, len, code;

	/* Tell the NIC to enter record read mode. */
	if (wi_cmd(sc, WI_CMD_ACCESS|WI_ACCESS_READ, ltv->wi_type))
		return(EIO);

	/* Seek to the record. */
	if (wi_seek(sc, ltv->wi_type, 0, WI_BAP1))
		return(EIO);

	/*
	 * Read the length and record type and make sure they
	 * match what we expect (this verifies that we have enough
	 * room to hold all of the returned data).
	 */
	len = CSR_READ_2(sc, WI_DATA1);
	if (len > ltv->wi_len)
		return(ENOSPC);
	code = CSR_READ_2(sc, WI_DATA1);
	if (code != ltv->wi_type)
		return(EIO);

	ltv->wi_len = len;
	ltv->wi_type = code;

	/* Now read the data. */
	ptr = &ltv->wi_val;
	for (i = 0; i < ltv->wi_len - 1; i++)
		ptr[i] = CSR_READ_2(sc, WI_DATA1);

	return(0);
}

/*
 * Same as read, except we inject data instead of reading it.
 */
STATIC int wi_write_record(sc, ltv)
	struct wi_softc		*sc;
	struct wi_ltv_gen	*ltv;
{
	u_int16_t		*ptr;
	int			i;

	if (wi_seek(sc, ltv->wi_type, 0, WI_BAP1))
		return(EIO);

	CSR_WRITE_2(sc, WI_DATA1, ltv->wi_len);
	CSR_WRITE_2(sc, WI_DATA1, ltv->wi_type);

	ptr = &ltv->wi_val;
	for (i = 0; i < ltv->wi_len - 1; i++)
		CSR_WRITE_2(sc, WI_DATA1, ptr[i]);

	if (wi_cmd(sc, WI_CMD_ACCESS|WI_ACCESS_WRITE, ltv->wi_type))
		return(EIO);

	return(0);
}

STATIC int wi_seek(sc, id, off, chan)
	struct wi_softc		*sc;
	int			id, off, chan;
{
	int			i;
	int			selreg, offreg;

	switch (chan) {
	case WI_BAP0:
		selreg = WI_SEL0;
		offreg = WI_OFF0;
		break;
	case WI_BAP1:
		selreg = WI_SEL1;
		offreg = WI_OFF1;
		break;
	default:
		printf(WI_PRT_FMT ": invalid data path: %x\n", WI_PRT_ARG(sc),
		    chan);
		return(EIO);
	}

	CSR_WRITE_2(sc, selreg, id);
	CSR_WRITE_2(sc, offreg, off);

	for (i = 0; i < WI_TIMEOUT; i++) {
		if (!(CSR_READ_2(sc, offreg) & (WI_OFF_BUSY|WI_OFF_ERR)))
			break;
	}

	if (i == WI_TIMEOUT)
		return(ETIMEDOUT);

	return(0);
}

STATIC int wi_read_data(sc, id, off, buf, len)
	struct wi_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	int			i;
	u_int16_t		*ptr;

	if (wi_seek(sc, id, off, WI_BAP1))
		return(EIO);

	ptr = (u_int16_t *)buf;
	for (i = 0; i < len / 2; i++)
		ptr[i] = CSR_READ_2(sc, WI_DATA1);

	return(0);
}

/*
 * According to the comments in the HCF Light code, there is a bug in
 * the Hermes (or possibly in certain Hermes firmware revisions) where
 * the chip's internal autoincrement counter gets thrown off during
 * data writes: the autoincrement is missed, causing one data word to
 * be overwritten and subsequent words to be written to the wrong memory
 * locations. The end result is that we could end up transmitting bogus
 * frames without realizing it. The workaround for this is to write a
 * couple of extra guard words after the end of the transfer, then
 * attempt to read then back. If we fail to locate the guard words where
 * we expect them, we preform the transfer over again.
 */
STATIC int wi_write_data(sc, id, off, buf, len)
	struct wi_softc		*sc;
	int			id, off;
	caddr_t			buf;
	int			len;
{
	int			i;
	u_int16_t		*ptr;

#ifdef WI_HERMES_AUTOINC_WAR
again:
#endif

	if (wi_seek(sc, id, off, WI_BAP0))
		return(EIO);

	ptr = (u_int16_t *)buf;
	for (i = 0; i < (len / 2); i++)
		CSR_WRITE_2(sc, WI_DATA0, ptr[i]);

#ifdef WI_HERMES_AUTOINC_WAR
	CSR_WRITE_2(sc, WI_DATA0, 0x1234);
	CSR_WRITE_2(sc, WI_DATA0, 0x5678);

	if (wi_seek(sc, id, off + len, WI_BAP0))
		return(EIO);

	if (CSR_READ_2(sc, WI_DATA0) != 0x1234 ||
	    CSR_READ_2(sc, WI_DATA0) != 0x5678)
		goto again;
#endif

	return(0);
}

/*
 * Allocate a region of memory inside the NIC and zero
 * it out.
 */
STATIC int wi_alloc_nicmem(sc, len, id)
	struct wi_softc		*sc;
	int			len;
	int			*id;
{
	int			i;

	if (wi_cmd(sc, WI_CMD_ALLOC_MEM, len)) {
		printf(WI_PRT_FMT ": failed to allocate %d bytes on NIC\n",
		    WI_PRT_ARG(sc), len);
		return(ENOMEM);
	}

	for (i = 0; i < WI_TIMEOUT; i++) {
		if (CSR_READ_2(sc, WI_EVENT_STAT) & WI_EV_ALLOC)
			break;
	}

	if (i == WI_TIMEOUT)
		return(ETIMEDOUT);

	CSR_WRITE_2(sc, WI_EVENT_ACK, WI_EV_ALLOC);
	*id = CSR_READ_2(sc, WI_ALLOC_FID);

	if (wi_seek(sc, *id, 0, WI_BAP0))
		return(EIO);

	for (i = 0; i < len / 2; i++)
		CSR_WRITE_2(sc, WI_DATA0, 0);

	return(0);
}

STATIC void wi_setmulti(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;
	int			i = 0;
	struct wi_ltv_mcast	mcast;
	struct ether_multistep	step;
	struct ether_multi	*enm;

	ifp = &sc->arpcom.ac_if;

	bzero((char *)&mcast, sizeof(mcast));

	mcast.wi_type = WI_RID_MCAST;
	mcast.wi_len = (3 * 16) + 1;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		wi_write_record(sc, (struct wi_ltv_gen *)&mcast);
		return;
	}

	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
	while (enm != NULL) {
		if (i >= 16) {
			bzero((char *)&mcast, sizeof(mcast));
			break;
		}

		/* Punt on ranges. */
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
		    sizeof(enm->enm_addrlo)) != 0)
			break;
		bcopy(enm->enm_addrlo, (char *)&mcast.wi_mcast[i],
		    ETHER_ADDR_LEN);
		i++;
		ETHER_NEXT_MULTI(step, enm);
	}

	mcast.wi_len = (i * 3) + 1;
	wi_write_record(sc, (struct wi_ltv_gen *)&mcast);

	return;
}

STATIC void wi_setdef(sc, wreq)
	struct wi_softc		*sc;
	struct wi_req		*wreq;
{
	struct sockaddr_dl	*sdl;
	struct ifaddr		*ifa;
	struct ifnet		*ifp;
	extern struct ifaddr	**ifnet_addrs;

	ifp = &sc->arpcom.ac_if;

	switch(wreq->wi_type) {
	case WI_RID_MAC_NODE:
		ifa = ifnet_addrs[ifp->if_index];
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		bcopy((char *)&wreq->wi_val, LLADDR(sdl), ETHER_ADDR_LEN);
		bcopy((char *)&wreq->wi_val, (char *)&sc->arpcom.ac_enaddr,
		      ETHER_ADDR_LEN);
		break;
	case WI_RID_PORTTYPE:
		sc->wi_ptype = wreq->wi_val[0];
		break;
	case WI_RID_TX_RATE:
		sc->wi_tx_rate = wreq->wi_val[0];
		break;
	case WI_RID_MAX_DATALEN:
		sc->wi_max_data_len = wreq->wi_val[0];
		break;
	case WI_RID_RTS_THRESH:
		sc->wi_rts_thresh = wreq->wi_val[0];
		break;
	case WI_RID_SYSTEM_SCALE:
		sc->wi_ap_density = wreq->wi_val[0];
		break;
	case WI_RID_CREATE_IBSS:
		sc->wi_create_ibss = wreq->wi_val[0];
		break;
	case WI_RID_OWN_CHNL:
		sc->wi_channel = wreq->wi_val[0];
		break;
	case WI_RID_NODENAME:
		bzero(sc->wi_node_name, sizeof(sc->wi_node_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_node_name, 30);
		break;
	case WI_RID_DESIRED_SSID:
		bzero(sc->wi_net_name, sizeof(sc->wi_net_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_net_name, 30);
		break;
	case WI_RID_OWN_SSID:
		bzero(sc->wi_ibss_name, sizeof(sc->wi_ibss_name));
		bcopy((char *)&wreq->wi_val[1], sc->wi_ibss_name, 30);
		break;
	case WI_RID_PM_ENABLED:
		sc->wi_pm_enabled = wreq->wi_val[0];
		break;
	case WI_RID_MAX_SLEEP:
		sc->wi_max_sleep = wreq->wi_val[0];
		break;
	case WI_RID_ENCRYPTION:
		sc->wi_use_wep = wreq->wi_val[0];
		break;
	case WI_RID_TX_CRYPT_KEY:
		sc->wi_tx_key = wreq->wi_val[0];
		break;
	case WI_RID_DEFLT_CRYPT_KEYS:
		bcopy((char *)wreq, (char *)&sc->wi_keys,
		    sizeof(struct wi_ltv_keys));
		break;
	default:
		break;
	}

	/* Reinitialize WaveLAN. */
	wi_init(sc);

	return;
}

STATIC int wi_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	int			s, error = 0;
	struct wi_softc		*sc;
	struct wi_req		wreq;
	struct ifreq		*ifr;
	struct proc *p = curproc;
	struct ifaddr		*ifa = (struct ifaddr *)data;

	s = splimp();

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;

	if (sc->wi_gone)
		return(ENODEV);

	DPRINTF (WID_IOCTL, ("wi_ioctl: command %lu data %p", command, data));

	if ((error = ether_ioctl(ifp, &sc->arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			wi_init(sc);
			arp_ifinit(&sc->arpcom, ifa);
			break;
#endif	/* INET */
		default:
			wi_init(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->wi_if_flags & IFF_PROMISC)) {
				WI_SETVAL(WI_RID_PROMISC, 1);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->wi_if_flags & IFF_PROMISC) {
				WI_SETVAL(WI_RID_PROMISC, 0);
			} else
				wi_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				wi_stop(sc);
			}
		}
		sc->wi_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		wi_setmulti(sc);
		error = 0;
		break;
	case SIOCGWAVELAN:
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		if (wreq.wi_type == WI_RID_IFACE_STATS) {
			bcopy((char *)&sc->wi_stats, (char *)&wreq.wi_val,
			    sizeof(sc->wi_stats));
			wreq.wi_len = (sizeof(sc->wi_stats) / 2) + 1;
		} else if (wreq.wi_type == WI_RID_DEFLT_CRYPT_KEYS) {
			/* For non-root user, return all-zeroes keys */
			if (suser(p->p_ucred, &p->p_acflag))
				bzero((char *)&wreq,
			    		sizeof(struct wi_ltv_keys));
			else
				bcopy((char *)&sc->wi_keys, (char *)&wreq,
			    		sizeof(struct wi_ltv_keys));
		} else {
			if (wi_read_record(sc, (struct wi_ltv_gen *)&wreq)) {
				error = EINVAL;
				break;
			}
		}
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
		break;
	case SIOCSWAVELAN:
		error = suser(p->p_ucred, &p->p_acflag);
		if (error)
			break;
		error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
		if (error)
			break;
		if (wreq.wi_type == WI_RID_IFACE_STATS) {
			error = EINVAL;
			break;
		} else if (wreq.wi_type == WI_RID_MGMT_XMIT) {
			error = wi_mgmt_xmit(sc, (caddr_t)&wreq.wi_val,
			    wreq.wi_len);
		} else {
			error = wi_write_record(sc, (struct wi_ltv_gen *)&wreq);
			if (!error)
				wi_setdef(sc, &wreq);
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return(error);
}

STATIC void wi_init(xsc)
	void			*xsc;
{
	struct wi_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			s;
	struct wi_ltv_macaddr	mac;
	int			id = 0;

	if (sc->wi_gone)
		return;

	DPRINTF(WID_INIT, ("wi_init: sc %p\n", sc));

	s = splimp();

	if (ifp->if_flags & IFF_RUNNING)
		wi_stop(sc);

	wi_reset(sc);

	/* Program max data length. */
	WI_SETVAL(WI_RID_MAX_DATALEN, sc->wi_max_data_len);

	/* Enable/disable IBSS creation. */
	WI_SETVAL(WI_RID_CREATE_IBSS, sc->wi_create_ibss);

	/* Set the port type. */
	WI_SETVAL(WI_RID_PORTTYPE, sc->wi_ptype);

	/* Program the RTS/CTS threshold. */
	WI_SETVAL(WI_RID_RTS_THRESH, sc->wi_rts_thresh);

	/* Program the TX rate */
	WI_SETVAL(WI_RID_TX_RATE, sc->wi_tx_rate);

	/* Access point density */
	WI_SETVAL(WI_RID_SYSTEM_SCALE, sc->wi_ap_density);

	/* Power Management Enabled */
	WI_SETVAL(WI_RID_PM_ENABLED, sc->wi_pm_enabled);

	/* Power Managment Max Sleep */
	WI_SETVAL(WI_RID_MAX_SLEEP, sc->wi_max_sleep);

	/* Specify the IBSS name */
	WI_SETSTR(WI_RID_OWN_SSID, sc->wi_ibss_name);

	/* Specify the network name */
	WI_SETSTR(WI_RID_DESIRED_SSID, sc->wi_net_name);

	/* Specify the frequency to use */
	WI_SETVAL(WI_RID_OWN_CHNL, sc->wi_channel);

	/* Program the nodename. */
	WI_SETSTR(WI_RID_NODENAME, sc->wi_node_name);

	/* Set our MAC address. */
	mac.wi_len = 4;
	mac.wi_type = WI_RID_MAC_NODE;
	bcopy((char *)&sc->arpcom.ac_enaddr,
	   (char *)&mac.wi_mac_addr, ETHER_ADDR_LEN);
	wi_write_record(sc, (struct wi_ltv_gen *)&mac);

	/* Configure WEP. */
	if (sc->wi_has_wep) {
		WI_SETVAL(WI_RID_ENCRYPTION, sc->wi_use_wep);
		WI_SETVAL(WI_RID_TX_CRYPT_KEY, sc->wi_tx_key);
		sc->wi_keys.wi_len = (sizeof(struct wi_ltv_keys) / 2) + 1;
		sc->wi_keys.wi_type = WI_RID_DEFLT_CRYPT_KEYS;
		wi_write_record(sc, (struct wi_ltv_gen *)&sc->wi_keys);
	}

	/* Initialize promisc mode. */
	if (ifp->if_flags & IFF_PROMISC) {
		WI_SETVAL(WI_RID_PROMISC, 1);
	} else {
		WI_SETVAL(WI_RID_PROMISC, 0);
	}

	/* Set multicast filter. */
	wi_setmulti(sc);

	/* Enable desired port */
	wi_cmd(sc, WI_CMD_ENABLE|sc->wi_portnum, 0);

	if (wi_alloc_nicmem(sc, 1518 + sizeof(struct wi_frame) + 8, &id))
		printf(WI_PRT_FMT ": tx buffer allocation failed\n",
		    WI_PRT_ARG(sc));
	sc->wi_tx_data_id = id;

	if (wi_alloc_nicmem(sc, 1518 + sizeof(struct wi_frame) + 8, &id))
		printf(WI_PRT_FMT ": mgmt. buffer allocation failed\n",
		    WI_PRT_ARG(sc));
	sc->wi_tx_mgmt_id = id;

	/* enable interrupts */
	CSR_WRITE_2(sc, WI_INT_EN, WI_INTRS);

	splx(s);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	TIMEOUT(sc->wi_stat_ch, wi_inquire, sc, hz * 60);

	return;
}

STATIC void wi_start(ifp)
	struct ifnet		*ifp;
{
	struct wi_softc		*sc;
	struct mbuf		*m0;
	struct wi_frame		tx_frame;
	struct ether_header	*eh;
	int			id;

	sc = ifp->if_softc;

	DPRINTF(WID_START, ("wi_start: ifp %p sc %p\n", ifp, sc));

	if (sc->wi_gone)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IF_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == NULL)
		return;

	bzero((char *)&tx_frame, sizeof(tx_frame));
	id = sc->wi_tx_data_id;
	eh = mtod(m0, struct ether_header *);

	/*
	 * Use RFC1042 encoding for IP and ARP datagrams,
	 * 802.3 for anything else.
	 */
	if (ntohs(eh->ether_type) == ETHERTYPE_IP ||
	    ntohs(eh->ether_type) == ETHERTYPE_ARP ||
	    ntohs(eh->ether_type) == ETHERTYPE_REVARP) {
		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame.wi_addr1, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_shost,
		    (char *)&tx_frame.wi_addr2, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_dhost,
		    (char *)&tx_frame.wi_dst_addr, ETHER_ADDR_LEN);
		bcopy((char *)&eh->ether_shost,
		    (char *)&tx_frame.wi_src_addr, ETHER_ADDR_LEN);

		tx_frame.wi_dat_len = m0->m_pkthdr.len - WI_SNAPHDR_LEN;
		tx_frame.wi_frame_ctl = WI_FTYPE_DATA;
		tx_frame.wi_dat[0] = htons(WI_SNAP_WORD0);
		tx_frame.wi_dat[1] = htons(WI_SNAP_WORD1);
		tx_frame.wi_len = htons(m0->m_pkthdr.len - WI_SNAPHDR_LEN);
		tx_frame.wi_type = eh->ether_type;

		m_copydata(m0, sizeof(struct ether_header),
		    m0->m_pkthdr.len - sizeof(struct ether_header),
		    (caddr_t)&sc->wi_txbuf);

		wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
		    sizeof(struct wi_frame));
		wi_write_data(sc, id, WI_802_11_OFFSET, (caddr_t)&sc->wi_txbuf,
		    (m0->m_pkthdr.len - sizeof(struct ether_header)) + 2);
	} else {
		tx_frame.wi_dat_len = m0->m_pkthdr.len;

		m_copydata(m0, 0, m0->m_pkthdr.len, (caddr_t)&sc->wi_txbuf);

		wi_write_data(sc, id, 0, (caddr_t)&tx_frame,
		    sizeof(struct wi_frame));
		wi_write_data(sc, id, WI_802_3_OFFSET, (caddr_t)&sc->wi_txbuf,
		    m0->m_pkthdr.len + 2);
	}

#if NBPFILTER > 0
	/*
	 * If there's a BPF listner, bounce a copy of
	 * this frame to him.
	 */
	if (ifp->if_bpf)
		BPF_MTAP(ifp, m0);
#endif

	m_freem(m0);

	if (wi_cmd(sc, WI_CMD_TX|WI_RECLAIM, id))
		printf(WI_PRT_FMT ": xmit failed\n", WI_PRT_ARG(sc));

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

STATIC int wi_mgmt_xmit(sc, data, len)
	struct wi_softc		*sc;
	caddr_t			data;
	int			len;
{
	struct wi_frame		tx_frame;
	int			id;
	struct wi_80211_hdr	*hdr;
	caddr_t			dptr;

	if (sc->wi_gone)
		return(ENODEV);

	hdr = (struct wi_80211_hdr *)data;
	dptr = data + sizeof(struct wi_80211_hdr);

	bzero((char *)&tx_frame, sizeof(tx_frame));
	id = sc->wi_tx_mgmt_id;

	bcopy((char *)hdr, (char *)&tx_frame.wi_frame_ctl,
	   sizeof(struct wi_80211_hdr));

	tx_frame.wi_dat_len = len - WI_SNAPHDR_LEN;
	tx_frame.wi_len = htons(len - WI_SNAPHDR_LEN);

	wi_write_data(sc, id, 0, (caddr_t)&tx_frame, sizeof(struct wi_frame));
	wi_write_data(sc, id, WI_802_11_OFFSET_RAW, dptr,
	    (len - sizeof(struct wi_80211_hdr)) + 2);

	if (wi_cmd(sc, WI_CMD_TX|WI_RECLAIM, id)) {
		printf(WI_PRT_FMT ": xmit failed\n", WI_PRT_ARG(sc));
		return(EIO);
	}

	return(0);
}

STATIC void wi_stop(sc)
	struct wi_softc		*sc;
{
	struct ifnet		*ifp;

	if (sc->wi_gone)
		return;

	DPRINTF(WID_STOP, ("wi_stop: sc %p\n", sc));

	ifp = &sc->arpcom.ac_if;

	CSR_WRITE_2(sc, WI_INT_EN, 0);
	wi_cmd(sc, WI_CMD_DISABLE|sc->wi_portnum, 0);

	UNTIMEOUT(wi_inquire, sc, sc->wi_stat_ch);

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);

	return;
}

STATIC void wi_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct wi_softc		*sc;

	sc = ifp->if_softc;

	printf(WI_PRT_FMT ": device timeout\n", WI_PRT_ARG(sc));

	wi_init(sc);

	ifp->if_oerrors++;

	return;
}

STATIC void wi_shutdown(arg)
	void			*arg;
{
	struct wi_softc		*sc;

	sc = arg;
	wi_stop(sc);

	return;
}
