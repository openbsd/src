/*    $OpenBSD: if_sn.c,v 1.34 2004/11/26 21:21:24 miod Exp $        */
/*    $NetBSD: if_sn.c,v 1.13 1997/04/25 03:40:10 briggs Exp $        */

/*
 * National Semiconductor  DP8393X SONIC Driver
 * Copyright (c) 1991   Algorithmics Ltd (http://www.algor.co.uk)
 * You may use, copy, and modify this program so long as you retain the
 * copyright line.
 *
 * This driver has been substantially modified since Algorithmics donated
 * it.
 *
 *   Denton Gentry <denny1@home.com>
 * and also
 *   Yanagisawa Takeshi <yanagisw@aa.ap.titech.ac.jp>
 * did the work to get this running on the Macintosh.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/netisr.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <uvm/uvm_extern.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/bus.h>
#include <machine/cpu.h>
/* #include <machine/viareg.h> */
#include <mac68k/dev/if_snreg.h>
#include <mac68k/dev/if_snvar.h>

static void	snwatchdog(struct ifnet *);
static int	sninit(struct sn_softc *sc);
static int	snstop(struct sn_softc *sc);
static int	snioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static void	snstart(struct ifnet *ifp);
static void	snreset(struct sn_softc *sc);

static void	caminitialise(struct sn_softc *);
static void	camentry(struct sn_softc *, int, u_char *ea);
static void	camprogram(struct sn_softc *);
static void	initialise_tda(struct sn_softc *);
static void	initialise_rda(struct sn_softc *);
static void	initialise_rra(struct sn_softc *);
#ifdef SNDEBUG
static void	camdump(struct sn_softc *sc);
#endif

static void	sonictxint(struct sn_softc *);
static void	sonicrxint(struct sn_softc *);

static __inline__ int	sonicput(struct sn_softc *sc, struct mbuf *m0,
			    int mtd_next);
static __inline__ int	sonic_read(struct sn_softc *, caddr_t, int);
static __inline__ struct mbuf *sonic_get(struct sn_softc *,
			    struct ether_header *, int);

struct cfdriver sn_cd = {
	NULL, "sn", DV_IFNET
};

#undef assert
#undef _assert

#ifdef NDEBUG
#define	assert(e)	((void)0)
#define	_assert(e)	((void)0)
#else
#define	_assert(e)	assert(e)
#ifdef __STDC__
#define	assert(e)	((e) ? (void)0 : __assert("sn ", __FILE__, __LINE__, #e))
#else	/* PCC */
#define	assert(e)	((e) ? (void)0 : __assert("sn "__FILE__, __LINE__, "e"))
#endif
#endif

int sndebug = 0;

/*
 * SONIC buffers need to be aligned 16 or 32 bit aligned.
 * These macros calculate and verify alignment.
 */
#define	ROUNDUP(p, N)	(((int) p + N - 1) & ~(N - 1))

#define SOALIGN(m, array)	(m ? (ROUNDUP(array, 4)) : (ROUNDUP(array, 2)))

#define LOWER(x) ((unsigned)(x) & 0xffff)
#define UPPER(x) ((unsigned)(x) >> 16)

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
int
snsetup(sc, lladdr)
	struct sn_softc	*sc;
	u_int8_t *lladdr;
{
	struct ifnet *ifp = &sc->sc_if;
	u_char	*p;
	u_char	*pp;
	int	i;
	int	offset;

	/*
	 * XXX if_sn.c is intended to be MI. Should it allocate memory
	 * for its descriptor areas, or expect the MD attach code
	 * to do that?
	 */
	sc->space = malloc((SN_NPAGES + 1) * NBPG, M_DEVBUF, M_WAITOK);

	/*
	 * Put the pup in reset mode (sninit() will fix it later),
	 * stop the timer, disable all interrupts and clear any interrupts.
	 */
	NIC_PUT(sc, SNR_CR, CR_STP);
	wbflush();
	NIC_PUT(sc, SNR_CR, CR_RST);
	wbflush();
	NIC_PUT(sc, SNR_IMR, 0);
	wbflush();
	NIC_PUT(sc, SNR_ISR, ISR_ALL);
	wbflush();

	/*
	 * because the SONIC is basically 16bit device it 'concatenates'
	 * a higher buffer address to a 16 bit offset--this will cause wrap
	 * around problems near the end of 64k !!
	 */
	p = sc->space;
	pp = (u_char *)ROUNDUP ((int)p, NBPG);
	p = pp;

	/*
	 * Disable caching on the SONIC's data space.
	 * The pages might not be physically contiguous, so set
	 * each page individually.
	 */
	for (i = 0; i < SN_NPAGES; i++) {
		physaccess (p, (caddr_t)SONIC_GETDMA(p), NBPG,
			PG_V | PG_RW | PG_CI);
		p += NBPG;
	}
	p = pp;

	for (i = 0; i < NRRA; i++) {
		sc->p_rra[i] = (void *)p;
		sc->v_rra[i] = SONIC_GETDMA(p);
		p += RXRSRC_SIZE(sc);
	}
	sc->v_rea = SONIC_GETDMA(p);

	p = (u_char *)SOALIGN(sc, p);

	sc->p_cda = (void *)(p);
	sc->v_cda = SONIC_GETDMA(p);
	p += CDA_SIZE(sc);

	p = (u_char *)SOALIGN(sc, p);

	for (i = 0; i < NTDA; i++) {
		struct mtd *mtdp = &sc->mtda[i];
		mtdp->mtd_txp = (void *)p;
		mtdp->mtd_vtxp = SONIC_GETDMA(p);
		p += TXP_SIZE(sc);
	}

	p = (u_char *)SOALIGN(sc, p);

	if ((p - pp) > NBPG) {
		printf ("%s: sizeof RRA (%ld) + CDA (%ld) +"
		    "TDA (%ld) > NBPG (%d). Punt!\n",
		    sc->sc_dev.dv_xname,
		    (ulong)sc->p_cda - (ulong)sc->p_rra[0],
		    (ulong)sc->mtda[0].mtd_txp - (ulong)sc->p_cda,
		    (ulong)p - (ulong)sc->mtda[0].mtd_txp,
		    NBPG);
		return(1);
	}

	p = pp + NBPG;
	pp = p;

	sc->sc_nrda = NBPG / RXPKT_SIZE(sc);
	sc->p_rda = (caddr_t) p;
	sc->v_rda = SONIC_GETDMA(p);

	p = pp + NBPG;

	for (i = 0; i < NRBA; i++) {
		sc->rbuf[i] = (caddr_t)p;
		p += NBPG;
	}

	pp = p;
	offset = TXBSIZE;
	for (i = 0; i < NTDA; i++) {
		struct mtd *mtdp = &sc->mtda[i];

		mtdp->mtd_buf = p;
		mtdp->mtd_vbuf = SONIC_GETDMA(p);
		offset += TXBSIZE;
		if (offset < NBPG) {
			p += TXBSIZE;
		} else {
			p = pp + NBPG;
			pp = p;
			offset = TXBSIZE;
		}
	}

#ifdef SNDEBUG
	camdump(sc);
#endif
	printf("address %s\n", ether_sprintf(lladdr));

#ifdef SNDEBUG
	printf("%s: buffers: rra=%p cda=%p rda=%p tda=%p\n",
	    sc->sc_dev.dv_xname, sc->p_rra[0], sc->p_cda,
	    sc->p_rda, sc->mtda[0].mtd_txp);
#endif

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	bcopy(lladdr, sc->sc_enaddr, ETHER_ADDR_LEN);

	ifp->if_softc = sc;
	ifp->if_ioctl = snioctl;
	ifp->if_start = snstart;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	ifp->if_watchdog = snwatchdog;
	if_attach(ifp);
	ether_ifattach(ifp);

	return (0);
}

static int
snioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ifaddr *ifa;
	struct ifreq *ifr;
	struct sn_softc *sc = ifp->if_softc;
	int	s = splnet(), err = 0;
	int	temp;

	switch (cmd) {

	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			(void)sninit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			(void)sninit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running,
			 * then stop it.
			 */
			snstop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			    (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped,
			 * then start it.
			 */
			(void)sninit(sc);
		} else {
			/*
			 * reset the interface to pick up any other changes
			 * in flags
			 */
			temp = ifp->if_flags & IFF_UP;
			snreset(sc);
			ifp->if_flags |= temp;
			snstart(ifp);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *) data;
		if (cmd == SIOCADDMULTI)
			err = ether_addmulti(ifr, &sc->sc_arpcom);
		else
			err = ether_delmulti(ifr, &sc->sc_arpcom);

		if (err == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly. But remember UP flag!
			 */
			temp = ifp->if_flags & IFF_UP;
			snreset(sc);
			ifp->if_flags |= temp;
			err = 0;
		}
		break;
	default:
		err = EINVAL;
	}
	splx(s);
	return (err);
}

/*
 * Encapsulate a packet of type family for the local net.
 */
static void
snstart(ifp)
	struct ifnet *ifp;
{
	struct sn_softc	*sc = ifp->if_softc;
	struct mbuf	*m;
	int		mtd_next;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

outloop:
	/* Check for room in the xmit buffer. */
	if ((mtd_next = (sc->mtd_free + 1)) == NTDA)
		mtd_next = 0;

	if (mtd_next == sc->mtd_hw) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	IF_DEQUEUE(&ifp->if_snd, m);
	if (m == 0)
		return;

	/* We need the header for m_pkthdr.len. */
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("%s: snstart: no header mbuf", sc->sc_dev.dv_xname);

#if NBPFILTER > 0
	/*
	 * If bpf is listening on this interface, let it
	 * see the packet before we commit it to the wire.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m);
#endif

	/*
	 * If there is nothing in the o/p queue, and there is room in
	 * the Tx ring, then send the packet directly.  Otherwise append
	 * it to the o/p queue.
	 */
	if ((sonicput(sc, m, mtd_next)) > 0) {
	} else {
		IF_PREPEND(&ifp->if_snd, m);
		return;
	}

	sc->mtd_prev = sc->mtd_free;
	sc->mtd_free = mtd_next;

	ifp->if_opackets++;		/* # of pkts */

	/* Jump back for possibly more punishment. */
	goto outloop;
}

/*
 * reset and restart the SONIC.  Called in case of fatal
 * hardware/software errors.
 */
static void
snreset(sc)
	struct sn_softc *sc;
{
	snstop(sc);
	sninit(sc);
}

static int 
sninit(sc)
	struct sn_softc *sc;
{
	u_long	s_rcr;
	int	s;

	if (sc->sc_if.if_flags & IFF_RUNNING)
		/* already running */
		return (0);

	s = splnet();

	NIC_PUT(sc, SNR_CR, CR_RST);	/* DCR only accessible in reset mode! */

	/* config it */
	NIC_PUT(sc, SNR_DCR, (sc->snr_dcr |
		(sc->bitmode ? DCR_DW32 : DCR_DW16)));
	NIC_PUT(sc, SNR_DCR2, sc->snr_dcr2);

	s_rcr = RCR_BRD | RCR_LBNONE;
	if (sc->sc_if.if_flags & IFF_PROMISC)
		s_rcr |= RCR_PRO;
	if (sc->sc_if.if_flags & IFF_ALLMULTI)
		s_rcr |= RCR_AMC;
	NIC_PUT(sc, SNR_RCR, s_rcr);

	NIC_PUT(sc, SNR_IMR, (IMR_PRXEN | IMR_PTXEN | IMR_TXEREN | IMR_LCDEN));

	/* clear pending interrupts */
	NIC_PUT(sc, SNR_ISR, ISR_ALL);

	/* clear tally counters */
	NIC_PUT(sc, SNR_CRCT, -1);
	NIC_PUT(sc, SNR_FAET, -1);
	NIC_PUT(sc, SNR_MPT, -1);

	initialise_tda(sc);
	initialise_rda(sc);
	initialise_rra(sc);

	/* enable the chip */
	NIC_PUT(sc, SNR_CR, 0);
	wbflush();

	/* program the CAM */
	camprogram(sc);

	/* get it to read resource descriptors */
	NIC_PUT(sc, SNR_CR, CR_RRRA);
	wbflush();
	while ((NIC_GET(sc, SNR_CR)) & CR_RRRA)
		continue;

	/* enable rx */
	NIC_PUT(sc, SNR_CR, CR_RXEN);
	wbflush();

	/* flag interface as "running" */
	sc->sc_if.if_flags |= IFF_RUNNING;
	sc->sc_if.if_flags &= ~IFF_OACTIVE;

	splx(s);
	return (0);
}

/*
 * close down an interface and free its buffers
 * Called on final close of device, or if sninit() fails
 * part way through.
 */
static int 
snstop(sc)
	struct sn_softc *sc;
{
	struct mtd *mtd;
	int	s = splnet();

	/* stick chip in reset */
	NIC_PUT(sc, SNR_CR, CR_RST);
	wbflush();

	/* free all receive buffers (currently static so nothing to do) */

	/* free all pending transmit mbufs */
	while (sc->mtd_hw != sc->mtd_free) {
		mtd = &sc->mtda[sc->mtd_hw];
		if (mtd->mtd_mbuf)
			m_freem(mtd->mtd_mbuf);
		if (++sc->mtd_hw == NTDA) sc->mtd_hw = 0;
	}

	sc->sc_if.if_timer = 0;
	sc->sc_if.if_flags &= ~(IFF_RUNNING | IFF_UP);

	splx(s);
	return (0);
}

/*
 * Called if any Tx packets remain unsent after 5 seconds,
 * In all cases we just reset the chip, and any retransmission
 * will be handled by higher level protocol timeouts.
 */
static void
snwatchdog(ifp)
	struct ifnet *ifp;
{
	struct sn_softc *sc = ifp->if_softc;
	struct mtd *mtd;
	int	temp;

	if (sc->mtd_hw != sc->mtd_free) {
		/* something still pending for transmit */
		mtd = &sc->mtda[sc->mtd_hw];
		if (SRO(sc->bitmode, mtd->mtd_txp, TXP_STATUS) == 0)
			log(LOG_ERR, "%s: Tx - timeout\n",
			    sc->sc_dev.dv_xname);
		else
			log(LOG_ERR, "%s: Tx - lost interrupt\n",
			    sc->sc_dev.dv_xname);
		temp = ifp->if_flags & IFF_UP;
		snreset(sc);
		ifp->if_flags |= temp;
	}
}

/*
 * stuff packet into sonic (at splnet)
 */
static __inline__ int 
sonicput(sc, m0, mtd_next)
	struct sn_softc *sc;
	struct mbuf *m0;
	int mtd_next;
{
	struct mtd *mtdp;
	struct mbuf *m;
	u_char	*buff;
	void	*txp;
	u_int	len = 0;
	u_int	totlen = 0;

	/* grab the replacement mtd */
	mtdp = &sc->mtda[sc->mtd_free];

	buff = mtdp->mtd_buf;
	
	/* this packet goes to mtdnext fill in the TDA */
	mtdp->mtd_mbuf = m0;
	txp = mtdp->mtd_txp;

	/* Write to the config word. Every (NTDA/2)+1 packets we set an intr */
	if (sc->mtd_pint == 0) {
		sc->mtd_pint = NTDA/2;
		SWO(sc->bitmode, txp, TXP_CONFIG, TCR_PINT);
	} else {
		sc->mtd_pint--;
		SWO(sc->bitmode, txp, TXP_CONFIG, 0);
	}

	for (m = m0; m; m = m->m_next) {
		u_char *data = mtod(m, u_char *);
		len = m->m_len;
		totlen += len;
		bcopy(data, buff, len);
		buff += len;
	}
	if (totlen >= TXBSIZE) {
		panic("%s: sonicput: packet overflow", sc->sc_dev.dv_xname);
	}

	SWO(sc->bitmode, txp, TXP_FRAGOFF + (0 * TXP_FRAGSIZE) + TXP_FPTRLO,
	    LOWER(mtdp->mtd_vbuf));
	SWO(sc->bitmode, txp, TXP_FRAGOFF + (0 * TXP_FRAGSIZE) + TXP_FPTRHI,
	    UPPER(mtdp->mtd_vbuf));

	if (totlen < ETHERMIN + sizeof(struct ether_header)) {
		int pad = ETHERMIN + sizeof(struct ether_header) - totlen;
		bzero(mtdp->mtd_buf + totlen, pad);
		totlen = ETHERMIN + sizeof(struct ether_header);
	}

	SWO(sc->bitmode, txp, TXP_FRAGOFF + (0 * TXP_FRAGSIZE) + TXP_FSIZE,
	    totlen);
	SWO(sc->bitmode, txp, TXP_FRAGCNT, 1);
	SWO(sc->bitmode, txp, TXP_PKTSIZE, totlen);

	/* link onto the next mtd that will be used */
	SWO(sc->bitmode, txp, TXP_FRAGOFF + (1 * TXP_FRAGSIZE) + TXP_FPTRLO,
	    LOWER(sc->mtda[mtd_next].mtd_vtxp) | EOL);

	/*
	 * The previous txp.tlink currently contains a pointer to
	 * our txp | EOL. Want to clear the EOL, so write our
	 * pointer to the previous txp.
	 */
	SWO(sc->bitmode, sc->mtda[sc->mtd_prev].mtd_txp, sc->mtd_tlinko,
	    LOWER(mtdp->mtd_vtxp));

	/* make sure chip is running */
	wbflush();
	NIC_PUT(sc, SNR_CR, CR_TXP);
	wbflush();
	sc->sc_if.if_timer = 5;	/* 5 seconds to watch for failing to transmit */

	return (totlen);
}

/*
 * These are called from sonicioctl() when /etc/ifconfig is run to set
 * the address or switch the i/f on.
 */
/*
 * CAM support
 */
static void 
caminitialise(sc)
	struct sn_softc *sc;
{
	void	*p_cda = sc->p_cda;
	int	i;
	int	bitmode = sc->bitmode;
	int	camoffset;

	for (i = 0; i < MAXCAM; i++) {
		camoffset = i * CDA_CAMDESC;
		SWO(bitmode, p_cda, (camoffset + CDA_CAMEP), i);
		SWO(bitmode, p_cda, (camoffset + CDA_CAMAP2), 0);
		SWO(bitmode, p_cda, (camoffset + CDA_CAMAP1), 0);
		SWO(bitmode, p_cda, (camoffset + CDA_CAMAP0), 0);
	}
	SWO(bitmode, p_cda, CDA_ENABLE, 0);
}

static void 
camentry(sc, entry, ea)
	int entry;
	u_char *ea;
	struct sn_softc *sc;
{
	void	*p_cda = sc->p_cda;
	int	bitmode = sc->bitmode;
	int	camoffset = entry * CDA_CAMDESC;

	SWO(bitmode, p_cda, camoffset + CDA_CAMEP, entry);
	SWO(bitmode, p_cda, camoffset + CDA_CAMAP2, (ea[5] << 8) | ea[4]);
	SWO(bitmode, p_cda, camoffset + CDA_CAMAP1, (ea[3] << 8) | ea[2]);
	SWO(bitmode, p_cda, camoffset + CDA_CAMAP0, (ea[1] << 8) | ea[0]);
	SWO(bitmode, p_cda, CDA_ENABLE, 
	    (SRO(bitmode, p_cda, CDA_ENABLE) | (1 << entry)));
}

static void 
camprogram(sc)
	struct sn_softc *sc;
{
	struct ether_multistep step;
	struct ether_multi *enm;
	struct ifnet *ifp;
	int	timeout;
	int	mcount = 0;

	caminitialise(sc);

	ifp = &sc->sc_if;

	/* Always load our own address first. */
	camentry (sc, mcount, sc->sc_enaddr);
	mcount++;

	/* Assume we won't need allmulti bit. */
	ifp->if_flags &= ~IFF_ALLMULTI;

	/* Loop through multicast addresses */
	ETHER_FIRST_MULTI(step, &sc->sc_arpcom, enm);
	while (enm != NULL) {
		if (mcount == MAXCAM) {
			 ifp->if_flags |= IFF_ALLMULTI;
			 break;
		}

		if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
		    sizeof(enm->enm_addrlo)) != 0) {
			/*
			 * SONIC's CAM is programmed with specific
			 * addresses. It has no way to specify a range.
			 * (Well, thats not exactly true. If the
			 * range is small one could program each addr
			 * within the range as a separate CAM entry)
			 */
			ifp->if_flags |= IFF_ALLMULTI;
			break;
		}

		/* program the CAM with the specified entry */
		camentry(sc, mcount, enm->enm_addrlo);
		mcount++;

		ETHER_NEXT_MULTI(step, enm);
	}

	NIC_PUT(sc, SNR_CDP, LOWER(sc->v_cda));
	NIC_PUT(sc, SNR_CDC, MAXCAM);
	NIC_PUT(sc, SNR_CR, CR_LCAM);
	wbflush();

	timeout = 10000;
	while ((NIC_GET(sc, SNR_CR) & CR_LCAM) && timeout--)
		continue;
	if (timeout == 0) {
		/* XXX */
		panic("%s: CAM initialisation failed", sc->sc_dev.dv_xname);
	}
	timeout = 10000;
	while (((NIC_GET(sc, SNR_ISR) & ISR_LCD) == 0) && timeout--)
		continue;

	if (NIC_GET(sc, SNR_ISR) & ISR_LCD)
		NIC_PUT(sc, SNR_ISR, ISR_LCD);
	else
		printf("%s: CAM initialisation without interrupt\n",
		    sc->sc_dev.dv_xname);
}

#ifdef SNDEBUG
static void 
camdump(sc)
	struct sn_softc *sc;
{
	int	i;

	printf("CAM entries:\n");
	NIC_PUT(sc, SNR_CR, CR_RST);
	wbflush();

	for (i = 0; i < 16; i++) {
		ushort  ap2, ap1, ap0;
		NIC_PUT(sc, SNR_CEP, i);
		wbflush();
		ap2 = NIC_GET(sc, SNR_CAP2);
		ap1 = NIC_GET(sc, SNR_CAP1);
		ap0 = NIC_GET(sc, SNR_CAP0);
		printf("%d: ap2=0x%x ap1=0x%x ap0=0x%x\n", i, ap2, ap1, ap0);
	}
	printf("CAM enable 0x%x\n", NIC_GET(sc, SNR_CEP));

	NIC_PUT(sc, SNR_CR, 0);
	wbflush();
}
#endif

static void 
initialise_tda(sc)
	struct sn_softc *sc;
{
	struct mtd *mtd;
	int	i;

	for (i = 0; i < NTDA; i++) {
		mtd = &sc->mtda[i];
		mtd->mtd_mbuf = 0;
	}

	sc->mtd_hw = 0;
	sc->mtd_prev = NTDA - 1;
	sc->mtd_free = 0;
	sc->mtd_tlinko = TXP_FRAGOFF + 1*TXP_FRAGSIZE + TXP_FPTRLO;
	sc->mtd_pint = NTDA/2;

	NIC_PUT(sc, SNR_UTDA, UPPER(sc->mtda[0].mtd_vtxp));
	NIC_PUT(sc, SNR_CTDA, LOWER(sc->mtda[0].mtd_vtxp));
}

static void
initialise_rda(sc)
	struct sn_softc *sc;
{
	int		bitmode = sc->bitmode;
	int		i;
	caddr_t		p_rda = 0;
	u_int32_t	v_rda = 0;

	/* link the RDA's together into a circular list */
	for (i = 0; i < (sc->sc_nrda - 1); i++) {
		p_rda = sc->p_rda + (i * RXPKT_SIZE(sc));
		v_rda = sc->v_rda + ((i+1) * RXPKT_SIZE(sc));
		SWO(bitmode, p_rda, RXPKT_RLINK, LOWER(v_rda));
		SWO(bitmode, p_rda, RXPKT_INUSE, 1);
	}
	p_rda = sc->p_rda + ((sc->sc_nrda - 1) * RXPKT_SIZE(sc));
	SWO(bitmode, p_rda, RXPKT_RLINK, LOWER(sc->v_rda) | EOL);
	SWO(bitmode, p_rda, RXPKT_INUSE, 1);

	/* mark end of receive descriptor list */
	sc->sc_rdamark = sc->sc_nrda - 1;

	sc->sc_rxmark = 0;

	NIC_PUT(sc, SNR_URDA, UPPER(sc->v_rda));
	NIC_PUT(sc, SNR_CRDA, LOWER(sc->v_rda));
	wbflush();
}

static void
initialise_rra(sc)
	struct sn_softc *sc;
{
	int	i;
	u_int	v;
	int	bitmode = sc->bitmode;

	if (bitmode)
		NIC_PUT(sc, SNR_EOBC, RBASIZE(sc) / 2 - 2);
	else
		NIC_PUT(sc, SNR_EOBC, RBASIZE(sc) / 2 - 1);

	NIC_PUT(sc, SNR_URRA, UPPER(sc->v_rra[0]));
	NIC_PUT(sc, SNR_RSA, LOWER(sc->v_rra[0]));
	/* rea must point just past the end of the rra space */
	NIC_PUT(sc, SNR_REA, LOWER(sc->v_rea));
	NIC_PUT(sc, SNR_RRP, LOWER(sc->v_rra[0]));
	NIC_PUT(sc, SNR_RSC, 0);

	/* fill up SOME of the rra with buffers */
	for (i = 0; i < NRBA; i++) {
		v = SONIC_GETDMA(sc->rbuf[i]);
		SWO(bitmode, sc->p_rra[i], RXRSRC_PTRHI, UPPER(v));
		SWO(bitmode, sc->p_rra[i], RXRSRC_PTRLO, LOWER(v));
		SWO(bitmode, sc->p_rra[i], RXRSRC_WCHI, UPPER(NBPG/2));
		SWO(bitmode, sc->p_rra[i], RXRSRC_WCLO, LOWER(NBPG/2));
	}
	sc->sc_rramark = NRBA;
	NIC_PUT(sc, SNR_RWP, LOWER(sc->v_rra[sc->sc_rramark]));
	wbflush();
}

int
snintr(void *arg)
{
	struct sn_softc *sc = (struct sn_softc *)arg;
	int isr;
	int rv = 0;

	while ((isr = (NIC_GET(sc, SNR_ISR) & ISR_ALL)) != 0) {
		rv = 1;
		/* scrub the interrupts that we are going to service */
		NIC_PUT(sc, SNR_ISR, isr);
		wbflush();

		if (isr & (ISR_BR | ISR_LCD | ISR_TC))
			printf("%s: unexpected interrupt status 0x%x\n",
			    sc->sc_dev.dv_xname, isr);

		if (isr & (ISR_TXDN | ISR_TXER | ISR_PINT))
			sonictxint(sc);

		if (isr & ISR_PKTRX)
			sonicrxint(sc);

		if (isr & (ISR_HBL | ISR_RDE | ISR_RBE | ISR_RBAE | ISR_RFO)) {
			if (isr & ISR_HBL)
				/*
				 * The repeater is not providing a heartbeat.
				 * In itself this isn't harmful, lots of the
				 * cheap repeater hubs don't supply a heartbeat.
				 * So ignore the lack of heartbeat. Its only
				 * if we can't detect a carrier that we have a
				 * problem.
				 */
				;
			if (isr & ISR_RDE)
				printf("%s: receive descriptors exhausted\n",
				    sc->sc_dev.dv_xname);
			if (isr & ISR_RBE)
				printf("%s: receive buffers exhausted\n",
				    sc->sc_dev.dv_xname);
			if (isr & ISR_RBAE)
				printf("%s: receive buffer area exhausted\n",
				    sc->sc_dev.dv_xname);
			if (isr & ISR_RFO)
				printf("%s: receive FIFO overrun\n",
				    sc->sc_dev.dv_xname);
		}
		if (isr & (ISR_CRC | ISR_FAE | ISR_MP)) {
#ifdef notdef
			if (isr & ISR_CRC)
				sc->sc_crctally++;
			if (isr & ISR_FAE)
				sc->sc_faetally++;
			if (isr & ISR_MP)
				sc->sc_mptally++;
#endif
		}
		snstart(&sc->sc_if);
	}

	return (rv);
}

/*
 * Transmit interrupt routine
 */
static void 
sonictxint(sc)
	struct sn_softc *sc;
{
	struct mtd	*mtd;
	void		*txp;
	unsigned short	txp_status;
	int		mtd_hw;
	struct ifnet	*ifp = &sc->sc_if;

	mtd_hw = sc->mtd_hw;

	if (mtd_hw == sc->mtd_free)
		return;

	while (mtd_hw != sc->mtd_free) {
		mtd = &sc->mtda[mtd_hw];

		txp = mtd->mtd_txp;

		if (SRO(sc->bitmode, txp, TXP_STATUS) == 0) {
			break; /* it hasn't really gone yet */
		}

#ifdef SNDEBUG
		{
			struct ether_header *eh;

			eh = (struct ether_header *) mtd->mtd_buf;
			printf("%s: xmit status=0x%x len=%d type=0x%x from %s",
			    sc->sc_dev.dv_xname,
			    SRO(sc->bitmode, txp, TXP_STATUS),
			    SRO(sc->bitmode, txp, TXP_PKTSIZE),
			    htons(eh->ether_type),
			    ether_sprintf(eh->ether_shost));
			printf(" (to %s)\n", ether_sprintf(eh->ether_dhost));
		}
#endif /* SNDEBUG */

		ifp->if_flags &= ~IFF_OACTIVE;

		if (mtd->mtd_mbuf != 0) {
			m_freem(mtd->mtd_mbuf);
			mtd->mtd_mbuf = 0;
		}
		if (++mtd_hw == NTDA) mtd_hw = 0;

		txp_status = SRO(sc->bitmode, txp, TXP_STATUS);

		ifp->if_collisions += (txp_status & TCR_EXC) ? 16 :
			((txp_status & TCR_NC) >> 12);

		if ((txp_status & TCR_PTX) == 0) {
			ifp->if_oerrors++;
			
			/* XXX - DG This looks bogus */
			if (mtd_hw != sc->mtd_free) {
				mtd = &sc->mtda[mtd_hw];
				NIC_PUT(sc, SNR_CTDA, LOWER(mtd->mtd_vtxp));
				NIC_PUT(sc, SNR_CR, CR_TXP);
				wbflush();
				break;
			}
		}
	}

	sc->mtd_hw = mtd_hw;
	return;
}

/*
 * Receive interrupt routine
 */
static void 
sonicrxint(sc)
	struct sn_softc *sc;
{
	caddr_t	rda;
	int	orra;
	int	len;
	int	rramark;
	int	rdamark;
	int	bitmode = sc->bitmode;
	u_int16_t rxpkt_ptr;

	rda = sc->p_rda + (sc->sc_rxmark * RXPKT_SIZE(sc));

	while (SRO(bitmode, rda, RXPKT_INUSE) == 0) {
		u_int status = SRO(bitmode, rda, RXPKT_STATUS);

		orra = RBASEQ(SRO(bitmode, rda, RXPKT_SEQNO)) & RRAMASK;
		rxpkt_ptr = SRO(bitmode, rda, RXPKT_PTRLO);
		len = SRO(bitmode, rda, RXPKT_BYTEC) -
			sizeof(struct ether_header) - FCSSIZE;
		if (status & RCR_PRX) {
			caddr_t pkt =
			    sc->rbuf[orra & RBAMASK] + (rxpkt_ptr & PGOFSET);
			if (sonic_read(sc, pkt, len))
				sc->sc_if.if_ipackets++;
			else
				sc->sc_if.if_ierrors++;
		} else
			sc->sc_if.if_ierrors++;

		/*
		 * give receive buffer area back to chip.
		 *
		 * If this was the last packet in the RRA, give the RRA to
		 * the chip again.
		 * If sonic read didnt copy it out then we would have to
		 * wait !!
		 * (dont bother add it back in again straight away)
		 *
		 * Really, we're doing p_rra[rramark] = p_rra[orra] but
		 * we have to use the macros because SONIC might be in
		 * 16 or 32 bit mode.
		 */
		if (status & RCR_LPKT) {
			void *tmp1, *tmp2;

			rramark = sc->sc_rramark;
			tmp1 = sc->p_rra[rramark];
			tmp2 = sc->p_rra[orra];
			SWO(bitmode, tmp1, RXRSRC_PTRLO,
				SRO(bitmode, tmp2, RXRSRC_PTRLO));
			SWO(bitmode, tmp1, RXRSRC_PTRHI,
				SRO(bitmode, tmp2, RXRSRC_PTRHI));
			SWO(bitmode, tmp1, RXRSRC_WCLO,
				SRO(bitmode, tmp2, RXRSRC_WCLO));
			SWO(bitmode, tmp1, RXRSRC_WCHI,
				SRO(bitmode, tmp2, RXRSRC_WCHI));

			/* zap old rra for fun */
			SWO(bitmode, tmp2, RXRSRC_WCHI, 0);
			SWO(bitmode, tmp2, RXRSRC_WCLO, 0);

			sc->sc_rramark = (++rramark) & RRAMASK;
			NIC_PUT(sc, SNR_RWP, LOWER(sc->v_rra[rramark]));
			wbflush();
		}

		/*
		 * give receive descriptor back to chip simple
		 * list is circular
		 */
		rdamark = sc->sc_rdamark;
		SWO(bitmode, rda, RXPKT_INUSE, 1);
		SWO(bitmode, rda, RXPKT_RLINK,
			SRO(bitmode, rda, RXPKT_RLINK) | EOL);
		SWO(bitmode, (sc->p_rda + (rdamark * RXPKT_SIZE(sc))), RXPKT_RLINK,
			SRO(bitmode, (sc->p_rda + (rdamark * RXPKT_SIZE(sc))),
			RXPKT_RLINK) & ~EOL);
		sc->sc_rdamark = sc->sc_rxmark;

		if (++sc->sc_rxmark >= sc->sc_nrda)
			sc->sc_rxmark = 0;
		rda = sc->p_rda + (sc->sc_rxmark * RXPKT_SIZE(sc));
	}
}

/*
 * sonic_read -- pull packet off interface and forward to
 * appropriate protocol handler
 */
static __inline__ int 
sonic_read(sc, pkt, len)
	struct sn_softc *sc;
	caddr_t pkt;
	int len;
{
	struct ifnet *ifp = &sc->sc_if;
	struct ether_header *et;
	struct mbuf *m;

	/*
	 * Get pointer to ethernet header (in input buffer).
	 */
	et = (struct ether_header *)pkt;

#ifdef SNDEBUG
	{
		printf("%s: rcvd 0x%p len=%d type=0x%x from %s",
		    sc->sc_dev.dv_xname, et, len, htons(et->ether_type),
		    ether_sprintf(et->ether_shost));
		printf(" (to %s)\n", ether_sprintf(et->ether_dhost));
	}
#endif /* SNDEBUG */

	if (len < ETHERMIN || len > ETHERMTU) {
		printf("%s: invalid packet length %d bytes\n",
		    sc->sc_dev.dv_xname, len);
		return (0);
	}

#if NBPFILTER > 0
	/*
	 * Check if there's a bpf filter listening on this interface.
	 * If so, hand off the raw packet to enet.
	 */
	if (ifp->if_bpf)
		bpf_tap(ifp->if_bpf, pkt,
		    len + sizeof(struct ether_header));
#endif
	m = sonic_get(sc, et, len);
	if (m == NULL)
		return (0);
	ether_input(ifp, et, m);
	return (1);
}

#define sonicdataaddr(eh, off, type)	((type)(((caddr_t)((eh) + 1) + (off))))

/*
 * munge the received packet into an mbuf chain
 */
static __inline__ struct mbuf *
sonic_get(sc, eh, datalen)
	struct sn_softc *sc;
	struct ether_header *eh;
	int datalen;
{
	struct	mbuf *m, *top, **mp;
	int	len;
	caddr_t	pkt = sonicdataaddr(eh, 0, caddr_t);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = &sc->sc_if;
	m->m_pkthdr.len = datalen;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (datalen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			len = MLEN;
		}
		if (datalen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				if (top) m_freem(top);
				return (0);
			}
			len = MCLBYTES;
		}
		m->m_len = len = min(datalen, len);

		bcopy(pkt, mtod(m, caddr_t), (unsigned) len);
		pkt += len;
		datalen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}

static u_char bbr4[] = {0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};
#define bbr(v)	((bbr4[(v)&0xf] << 4) | bbr4[((v)>>4) & 0xf])

void
sn_get_enaddr(t, h, o, dst)
	bus_space_tag_t	t;
	bus_space_handle_t h;
	vm_offset_t o;
	u_char *dst;
{
	int	i, do_bbr;
	u_char	b;

	/*
	 * For reasons known only to Apple, MAC addresses in the ethernet
	 * PROM are stored in Token Ring (IEEE 802.5) format, that is
	 * with all of the bits in each byte reversed (canonical bit format).
	 * When the address is read out it must be reversed to ethernet format
	 * before use.
	 *
	 * Apple has been assigned OUI's 08:00:07 and 00:a0:40. All onboard
	 * ethernet addresses on 68K machines should be in one of these
	 * two ranges.
	 *
	 * Here is where it gets complicated.
	 *
	 * The PMac 7200, 7500, 8500, and 9500 accidentally had the PROM
	 * written in standard ethernet format. The MacOS accounted for this
	 * in these systems, and did not reverse the bytes. Some other
	 * networking utilities were not so forgiving, and got confused.
	 * "Some" of Apple's Nubus ethernet cards also had their bits
	 * burned in ethernet format.
	 *
	 * Apple petitioned the IEEE and was granted the 00:05:02 (bit reversal
	 * of 00:a0:40) as well. As of OpenTransport 1.1.1, Apple removed
	 * their workaround and now reverses the bits regardless of
	 * what kind of machine it is. So PMac systems and the affected
	 * Nubus cards now use 00:05:02, instead of the 00:a0:40 for which they
	 * were intended.
	 *
	 * See Apple Techinfo article TECHINFO-0020552, "OpenTransport 1.1.1
	 * and MacOS System 7.5.3 FAQ (10/96)" for more details.
	 */
	do_bbr = 0;
	b = bus_space_read_1(t, h, o);
	if (b == 0x10)
		do_bbr = 1;
	dst[0] = (do_bbr) ? bbr(b) : b;

	for (i = 1 ; i < ETHER_ADDR_LEN ; i++) {
		b = bus_space_read_1(t, h, o+i);
		dst[i] = (do_bbr) ? bbr(b) : b;
	}
}
