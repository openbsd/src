/*      $OpenBSD: if_sn.c,v 1.9 1997/03/12 13:20:31 briggs Exp $       
*/

/*
 * National Semiconductor  SONIC Driver
 * Copyright (c) 1991   Algorithmics Ltd (http://www.algor.co.uk)
 * You may use, copy, and modify this program so long as you retain the
 * copyright line.
 *
 * This driver has been substantially modified since Algorithmics donated
 * it.
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
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <vm/vm.h>

extern int kvtop(caddr_t addr);

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

typedef unsigned char uchar;

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/viareg.h>
#include <mac68k/dev/if_snreg.h>
#include <mac68k/dev/if_snvar.h>

#include	"nubus.h"

#define SWR(a, x)       (a) = (x)
#define SRD(a)	  ((a) & 0xffff)

#define wbflush()

static void snwatchdog __P((struct ifnet *));
static int sninit __P((struct sn_softc *sc));
static int snstop __P((struct sn_softc *sc));
static int sonicput32 __P((struct sn_softc *sc, struct mbuf *m0));
static int sonicput16 __P((struct sn_softc *sc, struct mbuf *m0));
static void snintr __P((void *, int));
static int snioctl __P((struct ifnet *ifp, u_long cmd, caddr_t data));
static void snstart __P((struct ifnet *ifp));
static void snreset __P((struct sn_softc *sc));
static void sntxint16 __P((struct sn_softc *));
static void sntxint32 __P((struct sn_softc *));
static void snrxint16 __P((struct sn_softc *));
static void snrxint32 __P((struct sn_softc *));


void camdump __P((struct sn_softc *sc));

#undef assert
#undef _assert

#ifdef NDEBUG
#define assert(e)       ((void)0)
#define _assert(e)      ((void)0)
#else
#define _assert(e)      assert(e)
#ifdef __STDC__
#define assert(e)       ((e) ? (void)0 : __assert("sn ", __FILE__, __LINE__, #e))
#else   /* PCC */
#define assert(e)       ((e) ? (void)0 : __assert("sn "__FILE__, __LINE__, "e"))
#endif
#endif

int ethdebug = 0;

#define ROUNDUP(p, N)   (((int) p + N - 1) & ~(N - 1))

#define LOWER(x) ((unsigned)(x) & 0xffff)
#define UPPER(x) ((unsigned)(x) >> 16)

/*
 * Nicely aligned pointers into the SONIC buffers
 * p_ points at physical (K1_SEG) addresses.
 */

/* Meta transmit descriptors */
struct mtd {
	struct mtd	*mtd_link;
	void		*mtd_txp;
	int		mtd_vtxp;
	unsigned char	*mtd_buf;
} mtda[NTDA];

struct mtd *mtdfree;	    /* list of free meta transmit descriptors */
struct mtd *mtdhead;	    /* head of descriptors assigned to chip */
struct mtd *mtdtail;	    /* tail of descriptors assigned to chip */
struct mtd *mtdnext;	    /* next descriptor to give to chip */

void mtd_free __P((struct mtd *));
struct mtd *mtd_alloc __P((void));

struct cfdriver sn_cd = {
	NULL, "sn", DV_IFNET
};

void
snsetup(sc)
	struct sn_softc	*sc;
{
	struct ifnet	*ifp = &sc->sc_if;
	unsigned char	*p;
	unsigned char	*pp;
	int		i;

	sc->sc_csr = (struct sonic_reg *) sc->sc_regh;

/*
 * Disable caching on register and DMA space.
 */
	physaccess((caddr_t) sc->sc_csr, (caddr_t) kvtop((caddr_t) sc->sc_csr),
		SN_REGSIZE, PG_V | PG_RW | PG_CI);

	physaccess((caddr_t) sc->space, (caddr_t) kvtop((caddr_t) sc->space),
		sizeof(sc->space), PG_V | PG_RW | PG_CI);

/*
 * Put the pup in reset mode (sninit() will fix it later)
 * and clear any interrupts.
 */
	sc->sc_csr->s_cr = CR_RST;
	wbflush();
	sc->sc_csr->s_isr = 0x7fff;
	wbflush();

/*
 * because the SONIC is basically 16bit device it 'concatenates'
 * a higher buffer address to a 16 bit offset--this will cause wrap
 * around problems near the end of 64k !!
 */
	p = &sc->space[0];
	pp = (unsigned char *)ROUNDUP ((int)p, NBPG);

	if ((RRASIZE + CDASIZE + RDASIZE + TDASIZE) > NBPG) {
		printf ("sn: sizeof RRA (%d) + CDA (%d) + "
			"RDA (%d) + TDA (%d) > NBPG (%d).  Punt!\n",
			RRASIZE, CDASIZE, RDASIZE, TDASIZE, NBPG);
		return;
	}

	p = pp;
	sc->p_rra = (void *) p;
	sc->v_rra = kvtop((caddr_t) sc->p_rra);
	p += RRASIZE;

	sc->p_cda = (void *) (p);
	sc->v_cda = kvtop((caddr_t) sc->p_cda);
	p += CDASIZE;

	sc->p_rda = (void *) p;
	sc->v_rda = kvtop((caddr_t) sc->p_rda);
	p += RDASIZE;

	sc->p_tda = (void *) p;
	sc->v_tda = kvtop((caddr_t) sc->p_tda);
	p += TDASIZE;

	p = pp + NBPG;

	for (i = 0; i < NRBA; i+=2) {
		sc->rbuf[i] = (caddr_t) p;
		sc->rbuf[i+1] = (caddr_t)(p + (NBPG/2));
		p += NBPG;
	}

	for (i = 0; i < NTXB; i+=2) {
		sc->tbuf[i] = (caddr_t) p;
		sc->tbuf[i+1] = (caddr_t)(p + (NBPG/2));
		sc->vtbuf[i] = kvtop(sc->tbuf[i]);
		sc->vtbuf[i+1] = kvtop(sc->tbuf[i+1]);
		p += NBPG;
	}

#if 0
	camdump(sc);
#endif
	printf(" address %s\n", ether_sprintf(sc->sc_enaddr));

#if 0
printf("sonic buffers: rra=0x%x cda=0x%x rda=0x%x tda=0x%x\n",
	sc->p_rra, sc->p_cda, sc->p_rda, sc->p_tda);
#endif

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_ioctl = snioctl;
	ifp->if_start = snstart;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_watchdog = snwatchdog;

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	if_attach(ifp);
	ether_ifattach(ifp);

	if (sc->sc_is16) {
		sc->rxint = snrxint16;
		sc->txint = sntxint16;
	} else {
		sc->rxint = snrxint32;
		sc->txint = sntxint32;
	}
	add_nubus_intr(sc->slotno, snintr, (void *) sc);
}

static int
snioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ifaddr *ifa;
	struct sn_softc *sc = ifp->if_softc;
	int     s = splnet(), err = 0;
	int     temp;

	switch (cmd) {

	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			(void)sninit(ifp->if_softc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			(void)sninit(ifp->if_softc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    ifp->if_flags & IFF_RUNNING) {
			snstop(ifp->if_softc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if (ifp->if_flags & IFF_UP &&
		    (ifp->if_flags & IFF_RUNNING) == 0)
			(void)sninit(ifp->if_softc);
		/*
		 * If the state of the promiscuous bit changes, the
interface
		 * must be reset to effect the change.
		 */
		if (((ifp->if_flags ^ sc->sc_iflags) & IFF_PROMISC) &&
		    (ifp->if_flags & IFF_RUNNING)) {
			sc->sc_iflags = ifp->if_flags;
			printf("change in flags\n");
			temp = sc->sc_if.if_flags & IFF_UP;
			snreset(sc);
			sc->sc_if.if_flags |= temp;
			snstart(ifp);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if(cmd == SIOCADDMULTI)
			err = ether_addmulti((struct ifreq *)data,
						&sc->sc_arpcom);
		else
			err = ether_delmulti((struct ifreq *)data,
						&sc->sc_arpcom);

		if (err == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly. But remember UP flag!
			 */
			temp = sc->sc_if.if_flags & IFF_UP;
			snreset(sc);
			sc->sc_if.if_flags |= temp;
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
 * Use trailer local net encapsulation if enough data in first
 * packet leaves a multiple of 512 bytes of data in remainder.
 */
static void
snstart(ifp)
	struct ifnet *ifp;
{
	struct sn_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int     len;

	if ((sc->sc_if.if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

outloop:
	/* Check for room in the xmit buffer. */
	if (sc->txb_inuse == sc->txb_cnt) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	IF_DEQUEUE(&sc->sc_if.if_snd, m);
	if (m == 0)
		return;

	/* We need the header for m_pkthdr.len. */
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("snstart: no header mbuf");

#if NBPFILTER > 0
	/*
	 * If bpf is listening on this interface, let it
	 * see the packet before we commit it to the wire.
	 */
	if (sc->sc_if.if_bpf)
		bpf_mtap(sc->sc_if.if_bpf, m);
#endif

	/*
	 * If there is nothing in the o/p queue, and there is room in
	 * the Tx ring, then send the packet directly.  Otherwise append
	 * it to the o/p queue.
	 */
	if (sc->sc_is16) {
		len = sonicput16(sc, m);
	} else {
		len = sonicput32(sc, m);
	}
#if 0
	if (len != m->m_pkthdr.len) {
		printf("snstart: len %d != m->m_pkthdr.len %d.\n",
			len, m->m_pkthdr.len);
	}
#endif
	len = m->m_pkthdr.len;

	m_freem(m);

	/* Point to next buffer slot and wrap if necessary. */
	if (++sc->txb_new == sc->txb_cnt)
		sc->txb_new = 0;

	sc->txb_inuse++;

	sc->sc_if.if_opackets++;	/* # of pkts */
	sc->sc_sum.ls_opacks++;	 /* # of pkts */

	/* Jump back for possibly more punishment. */
	goto outloop;
}

/*
 * This is called from sonicioctl() when /etc/ifconfig is run to set
 * the address or switch the i/f on.
 */
void caminitialise __P((struct sn_softc *));
void camentry __P((struct sn_softc *, int, unsigned char *));
void camprogram __P((struct sn_softc *));
void initialise_tda __P((struct sn_softc *));
void initialise_rda __P((struct sn_softc *));
void initialise_rra __P((struct sn_softc *));
void initialise_tba __P((struct sn_softc *));

/*
 * reset and restart the SONIC.  Called in case of fatal
 * hardware/software errors.
 */
static void
snreset(sc)
	struct sn_softc *sc;
{
	printf("snreset\n");
	snstop(sc);
	sninit(sc);
}

static int 
sninit(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr = sc->sc_csr;
	int s;

	if (sc->sc_if.if_flags & IFF_RUNNING)
		/* already running */
		return (0);

	s = splnet();

	csr->s_cr = CR_RST;     /* s_dcr only accessable reset mode! */

	/* config it */
	csr->s_dcr = sc->s_dcr;
	csr->s_rcr = RCR_BRD | RCR_LBNONE;
	csr->s_imr = IMR_PRXEN | IMR_PTXEN | IMR_TXEREN | IMR_LCDEN;

	/* clear pending interrupts */
	csr->s_isr = 0x7fff;

	/* clear tally counters */
	csr->s_crct = -1;
	csr->s_faet = -1;
	csr->s_mpt = -1;

	initialise_tda(sc);
	initialise_rda(sc);
	initialise_rra(sc);
	initialise_tba(sc);

	/* enable the chip */
	csr->s_cr = 0;
	wbflush();

	/* program the CAM with our address */
	caminitialise(sc);
	camentry(sc, 0, sc->sc_enaddr);
	camprogram(sc);

	/* get it to read resource descriptors */
	csr->s_cr = CR_RRRA;
	wbflush();
	while (csr->s_cr & CR_RRRA)
		continue;

	/* enable rx */
	csr->s_cr = CR_RXEN;
	wbflush();

	/* flag interface as "running" */
	sc->sc_if.if_flags |= IFF_RUNNING;

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
	int s = splnet();

	/* stick chip in reset */
	sc->sc_csr->s_cr = CR_RST;
	wbflush();

	/* free all receive buffers (currently static so nothing to do) */

	/* free all pending transmit mbufs */
	while ((mtd = mtdhead) != NULL) {
		mtdhead = mtdhead->mtd_link;
		mtd->mtd_buf = 0;
		mtd_free(mtd);
	}
	mtdnext = mtd_alloc();
	sc->txb_inuse = 0;

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
	int temp;
	u_long status;

	if (mtdhead && mtdhead->mtd_buf) {
		/* something still pending for transmit */
		if (sc->sc_is16) {
			status = ((struct _short_TXpkt *)
				  mtdhead->mtd_txp)->status;
		} else {
			status = ((struct TXpkt *) mtdhead->mtd_txp)->status;
		}
		if (status == 0)
			log(LOG_ERR, "%s: Tx - timeout\n",
				sc->sc_dev.dv_xname);
		else
			log(LOG_ERR, "%s: Tx - lost interrupt\n",
				 sc->sc_dev.dv_xname);
		temp = sc->sc_if.if_flags & IFF_UP;
		snreset(sc);
		sc->sc_if.if_flags |= temp;
	}
}

/*
 * stuff packet into sonic (at splnet) (16-bit)
 */
static int 
sonicput16(sc, m0)
	struct sn_softc *sc;
	struct mbuf *m0;
{
	struct sonic_reg *csr = sc->sc_csr;
	unsigned char   *buff, *buffer, *data;
	struct _short_TXpkt *txp;
	struct mtd *mtdnew;
	struct mbuf *m;
	unsigned int len = 0;
	unsigned int totlen = 0;

	/* grab the replacement mtd */
	if ((mtdnew = mtd_alloc()) == 0)
		return (0);

	/* We are guaranteed, if we get here, that the xmit buffer is free. */
	buff = buffer = sc->tbuf[sc->txb_new];
	
	/* this packet goes to mdtnext fill in the TDA */
	mtdnext->mtd_buf = buffer;
	txp = mtdnext->mtd_txp;
	SWR(txp->config, 0);

	for (m = m0; m; m = m->m_next) {
		data = mtod(m, u_char *);
		len = m->m_len;
		totlen += len;
		bcopy(data, buff, len);
		buff += len;
	}
	if (totlen >= TXBSIZE) {
		panic("packet overflow in sonicput.");
	}
	SWR(txp->u[0].frag_ptrlo, LOWER(sc->vtbuf[sc->txb_new]));
	SWR(txp->u[0].frag_ptrhi, UPPER(sc->vtbuf[sc->txb_new]));
	SWR(txp->u[0].frag_size, totlen);

	if (totlen < ETHERMIN + sizeof(struct ether_header)) {
		int pad = ETHERMIN + sizeof(struct ether_header) - totlen;
		bzero(buffer + totlen, pad);
		SWR(txp->u[0].frag_size, pad + SRD(txp->u[0].frag_size));
		totlen = ETHERMIN + sizeof(struct ether_header);
	}
	SWR(txp->frag_count, 1);
	SWR(txp->pkt_size, totlen);

	/* link onto the next mtd that will be used */
	SWR(txp->u[1].tlink, LOWER(mtdnew->mtd_vtxp) | EOL);

	if (mtdhead == 0) {
		/* no current transmit list start with this one */
		mtdtail = mtdhead = mtdnext;
		csr->s_ctda = LOWER(mtdnext->mtd_vtxp);
	} else {
		/*
		 * have a transmit list append it to end note
		 * mtdnext is already physicaly linked to mtdtail in
		 * mtdtail->mtd_txp->u[mtdtail->mtd_txp->frag_count].tlink
		 */
		struct _short_TXpkt *tp;

		tp = (struct _short_TXpkt *) mtdtail->mtd_txp;
		SWR(tp->u[tp->frag_count].tlink,
			SRD(tp->u[tp->frag_count].tlink) & ~EOL);
		mtdtail = mtdnext;
	}
	mtdnext->mtd_link = mtdnew;
	mtdnext = mtdnew;

	/* make sure chip is running */
	wbflush();
	csr->s_cr = CR_TXP;
	wbflush();
	sc->sc_if.if_timer = 5; /* 5 seconds to watch for failing to transmit */
	return (totlen);
}

/*
 * 32-bit version of sonicput
 */
static int 
sonicput32(sc, m0)
	struct sn_softc *sc;
	struct mbuf *m0;
{
	struct sonic_reg *csr = sc->sc_csr;
	unsigned char   *buff, *buffer, *data;
	struct TXpkt *txp;
	struct mtd *mtdnew;
	struct mbuf *m;
	unsigned int len = 0;
	unsigned int totlen = 0;

	/* grab the replacement mtd */
	if ((mtdnew = mtd_alloc()) == 0)
		return (0);

	/* We are guaranteed, if we get here, that the xmit buffer is free. */
	buff = buffer = sc->tbuf[sc->txb_new];
	
	/* this packet goes to mdtnext fill in the TDA */
	mtdnext->mtd_buf = buffer;
	txp = mtdnext->mtd_txp;
	SWR(txp->config, 0);

	for (m = m0; m; m = m->m_next) {
		data = mtod(m, u_char *);
		len = m->m_len;
		totlen += len;
		bcopy(data, buff, len);
		buff += len;
	}
	if (totlen >= TXBSIZE) {
		panic("packet overflow in sonicput.");
	}
	SWR(txp->u[0].frag_ptrlo, LOWER(sc->vtbuf[sc->txb_new]));
	SWR(txp->u[0].frag_ptrhi, UPPER(sc->vtbuf[sc->txb_new]));
	SWR(txp->u[0].frag_size, totlen);

	if (totlen < ETHERMIN + sizeof(struct ether_header)) {
		int pad = ETHERMIN + sizeof(struct ether_header) - totlen;
		bzero(buffer + totlen, pad);
		SWR(txp->u[0].frag_size, pad + SRD(txp->u[0].frag_size));
		totlen = ETHERMIN + sizeof(struct ether_header);
	}
	SWR(txp->frag_count, 1);
	SWR(txp->pkt_size, totlen);

	/* link onto the next mtd that will be used */
	SWR(txp->u[1].tlink, LOWER(mtdnew->mtd_vtxp) | EOL);

	if (mtdhead == 0) {
		/* no current transmit list start with this one */
		mtdtail = mtdhead = mtdnext;
		csr->s_ctda = LOWER(mtdnext->mtd_vtxp);
	} else {
		/*
		 * have a transmit list append it to end note
		 * mtdnext is already physicaly linked to mtdtail in
		 * mtdtail->mtd_txp->u[mtdtail->mtd_txp->frag_count].tlink
		 */
		struct TXpkt *tp;

		tp = (struct TXpkt *) mtdtail->mtd_txp;
		SWR(tp->u[tp->frag_count].tlink,
			SRD(tp->u[tp->frag_count].tlink) & ~EOL);
		mtdtail = mtdnext;
	}
	mtdnext->mtd_link = mtdnew;
	mtdnext = mtdnew;

	/* make sure chip is running */
	wbflush();
	csr->s_cr = CR_TXP;
	wbflush();
	sc->sc_if.if_timer = 5; /* 5 seconds to watch for failing to transmit */
	return (totlen);
}

int sonic_read __P((struct sn_softc *, caddr_t, int));
struct mbuf *sonic_get __P((struct sn_softc *, struct ether_header *, int));

void 
mtd_free(mtd)
	struct mtd *mtd;
{
	mtd->mtd_link = mtdfree;
	mtdfree = mtd;
}

struct mtd *
mtd_alloc()
{
	struct mtd *mtd = mtdfree;

	if (mtd) {
		mtdfree = mtd->mtd_link;
		mtd->mtd_link = 0;
	}
	return (mtd);
}

/*
 * CAM support
 */
void 
caminitialise(sc)
	struct sn_softc *sc;
{
	int     i;

	if (sc->sc_is16) {
		struct _short_CDA *p_cda;

		p_cda = (struct _short_CDA *) sc->p_cda;
		for (i = 0; i < MAXCAM; i++)
			SWR(p_cda->desc[i].cam_ep, i);
		SWR(p_cda->enable, 0);
	} else {
		struct CDA *p_cda;

		p_cda = (struct CDA *) sc->p_cda;
		for (i = 0; i < MAXCAM; i++)
			SWR(p_cda->desc[i].cam_ep, i);
		SWR(p_cda->enable, 0);
	}
}

void 
camentry(sc, entry, ea)
	struct sn_softc *sc;
	int entry;
	unsigned char *ea;
{
	if (sc->sc_is16) {
		struct _short_CDA *p_cda;

		p_cda = (struct _short_CDA *) sc->p_cda;
		SWR(p_cda->desc[entry].cam_ep, entry);
		SWR(p_cda->desc[entry].cam_ap2, (ea[5] << 8) | ea[4]);
		SWR(p_cda->desc[entry].cam_ap1, (ea[3] << 8) | ea[2]);
		SWR(p_cda->desc[entry].cam_ap0, (ea[1] << 8) | ea[0]);
		SWR(p_cda->enable, SRD(p_cda->enable) | (1 << entry));
	} else {
		struct CDA *p_cda;

		p_cda = (struct CDA *) sc->p_cda;
		SWR(p_cda->desc[entry].cam_ep, entry);
		SWR(p_cda->desc[entry].cam_ap2, (ea[5] << 8) | ea[4]);
		SWR(p_cda->desc[entry].cam_ap1, (ea[3] << 8) | ea[2]);
		SWR(p_cda->desc[entry].cam_ap0, (ea[1] << 8) | ea[0]);
		SWR(p_cda->enable, SRD(p_cda->enable) | (1 << entry));
	}
}

void 
camprogram(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr;
	int     timeout;

	csr = sc->sc_csr;
	csr->s_cdp = LOWER(sc->v_cda);
	csr->s_cdc = MAXCAM;
	csr->s_cr = CR_LCAM;
	wbflush();

	timeout = 10000;
	while (csr->s_cr & CR_LCAM && timeout--)
		continue;
	if (timeout == 0) {
		/* XXX */
		panic("sonic: CAM initialisation failed\n");
	}
	timeout = 10000;
	while ((csr->s_isr & ISR_LCD) == 0 && timeout--)
		continue;

	if (csr->s_isr & ISR_LCD)
		csr->s_isr = ISR_LCD;
	else
		printf("sonic: CAM initialisation without interrupt\n");
}

#if 0
void 
camdump(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr = sc->sc_csr;
	int i;

	printf("CAM entries:\n");
	csr->s_cr = CR_RST;
	wbflush();

	for (i = 0; i < 16; i++) {
		ushort  ap2, ap1, ap0;
		csr->s_cep = i;
		wbflush();
		ap2 = csr->s_cap2;
		ap1 = csr->s_cap1;
		ap0 = csr->s_cap0;
		printf("%d: ap2=0x%x ap1=0x%x ap0=0x%x\n", i, ap2, ap1, ap0);
	}
	printf("CAM enable 0x%lx\n", csr->s_cep);

	csr->s_cr = 0;
	wbflush();
}
#endif

void 
initialise_tda(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr;
	struct mtd *mtd;
	int     i, psize;
	u_long	p;

	csr = sc->sc_csr;

	mtdfree = mtdhead = mtdtail = (struct mtd *) 0;

	p = (u_long) sc->p_tda;
	psize = (sc->sc_is16 ?
			sizeof(struct _short_TXpkt) : sizeof(struct TXpkt));

	for (i = 0; i < NTDA; i++) {
		mtd = &mtda[i];
		mtd->mtd_txp = (struct TXpkt *) p;
		mtd->mtd_vtxp = kvtop((caddr_t) mtd->mtd_txp);
		mtd->mtd_buf = 0;
		mtd_free(mtd);
		p += psize;
	}
	mtdnext = mtd_alloc();

	csr->s_utda = UPPER(sc->v_tda);
}

void 
initialise_rda(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr;
	int     i;

	csr = sc->sc_csr;

	/* link the RDA's together into a circular list */
	if (sc->sc_is16) {
		int			v_rda;
		struct _short_RXpkt	*p_rda;

		p_rda = (struct _short_RXpkt *) sc->p_rda;
		for (i = 0; i < (NRDA - 1); i++) {
			SWR(p_rda[i].rlink,
			 LOWER(v_rda + (i + 1) * sizeof(struct _short_RXpkt)));
			SWR(p_rda[i].in_use, 1);
		}
		SWR(p_rda[NRDA - 1].rlink, LOWER(v_rda) | EOL);
		SWR(p_rda[NRDA - 1].in_use, 1);

		/* mark end of receive descriptor list */
		sc->sc_lrxp = &p_rda[NRDA - 1];
	} else {
		int		v_rda;
		struct RXpkt	*p_rda;

		v_rda = sc->v_rda;
		p_rda = (struct RXpkt *) sc->p_rda;
		for (i = 0; i < (NRDA - 1); i++) {
			SWR(p_rda[i].rlink,
				LOWER(v_rda + (i + 1) * sizeof(struct RXpkt)));
			SWR(p_rda[i].in_use, 1);
		}
		SWR(p_rda[NRDA - 1].rlink, LOWER(v_rda) | EOL);
		SWR(p_rda[NRDA - 1].in_use, 1);

		/* mark end of receive descriptor list */
		sc->sc_lrxp = &p_rda[NRDA - 1];
	}

	sc->sc_rxmark = 0;

	csr->s_urda = UPPER(sc->v_rda);
	csr->s_crda = LOWER(sc->v_rda);
	wbflush();
}

void 
initialise_rra(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr;
	int     i;
	int	rr_size;

	csr = sc->sc_csr;

	if (sc->sc_is16) {
		rr_size = sizeof(struct _short_RXrsrc);
		csr->s_eobc = RBASIZE(sc) / 2 - 1;  /* must be >= MAXETHERPKT */
	} else {
		rr_size = sizeof(struct RXrsrc);
		csr->s_eobc = RBASIZE(sc) / 2 - 2;  /* must be >= MAXETHERPKT */
	}
	csr->s_urra = UPPER(sc->v_rra);
	csr->s_rsa = LOWER(sc->v_rra);
	csr->s_rea = LOWER(sc->v_rra + (NRRA * rr_size));
	csr->s_rrp = LOWER(sc->v_rra);

	/* fill up SOME of the rra with buffers */
	if (sc->sc_is16) {
		struct _short_RXrsrc *p_rra;

		p_rra = (struct _short_RXrsrc *) sc->p_rra;
		for (i = 0; i < NRBA; i++) {
			SWR(p_rra[i].buff_ptrhi, UPPER(kvtop(sc->rbuf[i])));
			SWR(p_rra[i].buff_ptrlo, LOWER(kvtop(sc->rbuf[i])));
			SWR(p_rra[i].buff_wchi, UPPER(RBASIZE(sc) / 2));
			SWR(p_rra[i].buff_wclo, LOWER(RBASIZE(sc) / 2));
		}
	} else {
		struct RXrsrc *p_rra;

		p_rra = (struct RXrsrc *) sc->p_rra;
		for (i = 0; i < NRBA; i++) {
			SWR(p_rra[i].buff_ptrhi, UPPER(kvtop(sc->rbuf[i])));
			SWR(p_rra[i].buff_ptrlo, LOWER(kvtop(sc->rbuf[i])));
			SWR(p_rra[i].buff_wchi, UPPER(RBASIZE(sc) / 2));
			SWR(p_rra[i].buff_wclo, LOWER(RBASIZE(sc) / 2));
		}
	}
	sc->sc_rramark = NRBA;
	csr->s_rwp = LOWER(sc->v_rra + (sc->sc_rramark * rr_size));
	wbflush();
}

void 
initialise_tba(sc)
	struct sn_softc *sc;
{
	sc->txb_cnt = NTXB;
	sc->txb_inuse = 0;
	sc->txb_new = 0;
}

static void
snintr(arg, slot)
	void    *arg;
	int     slot;
{
	struct sn_softc *sc = (struct sn_softc *)arg;
	struct sonic_reg *csr = sc->sc_csr;
	int     isr;

	while ((isr = (csr->s_isr & ISR_ALL)) != 0) {
		/* scrub the interrupts that we are going to service */
		csr->s_isr = isr;
		wbflush();

		if (isr & (ISR_BR | ISR_LCD | ISR_PINT | ISR_TC))
			printf("sonic: unexpected interrupt status 0x%x\n", isr);

		if (isr & (ISR_TXDN | ISR_TXER))
			(*sc->txint)(sc);

		if (isr & ISR_PKTRX)
			(*sc->rxint)(sc);

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
			if (isr & ISR_RDE)
				printf("sonic: receive descriptors exhausted\n");
			if (isr & ISR_RBE)
				printf("sonic: receive buffers exhausted\n");
			if (isr & ISR_RBAE)
				printf("sonic: receive buffer area exhausted\n");
			if (isr & ISR_RFO)
				printf("sonic: receive FIFO overrun\n");
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
	return;
}

/*
 * Transmit interrupt routine (16-bit)
 */
static void 
sntxint16(sc)
	struct sn_softc *sc;
{
	struct _short_TXpkt *txp;
	struct sonic_reg *csr;
	struct mtd *mtd;

	if (mtdhead == (struct mtd *) 0)
		return;

	csr = sc->sc_csr;

	while ((mtd = mtdhead) != NULL) {
		if (mtd->mtd_buf == 0)
			break;

		txp = (struct _short_TXpkt *) mtd->mtd_txp;

		if (SRD(txp->status) == 0)      /* it hasn't really gone yet */
			return;

		if (ethdebug) {
			struct ether_header *eh;

			eh = (struct ether_header *) mtd->mtd_buf;
			printf("xmit status=0x%x len=%d type=0x%x from %s",
			    txp->status,
			    txp->pkt_size,
			    htons(eh->ether_type),
			    ether_sprintf(eh->ether_shost));
			printf(" (to %s)\n", ether_sprintf(eh->ether_dhost));
		}
		sc->txb_inuse--;
		mtd->mtd_buf = 0;
		mtdhead = mtd->mtd_link;

		mtd_free(mtd);

		/* XXX - Do stats here. */

		if ((SRD(txp->status) & TCR_PTX) == 0) {
			printf("sonic: Tx packet status=0x%x\n", txp->status);

			/* XXX - DG This looks bogus */
			if (mtdhead != mtdnext) {
				printf("resubmitting remaining packets\n");
				csr->s_ctda = LOWER(mtdhead->mtd_vtxp);
				csr->s_cr = CR_TXP;
				wbflush();
				return;
			}
		}
	}
	/* mtdhead should be at mtdnext (go) */
	mtdhead = 0;
}

/*
 * Transmit interrupt routine (32-bit)
 */
static void 
sntxint32(sc)
	struct sn_softc *sc;
{
	struct TXpkt *txp;
	struct sonic_reg *csr;
	struct mtd *mtd;

	if (mtdhead == (struct mtd *) 0)
		return;

	csr = sc->sc_csr;

	while ((mtd = mtdhead) != NULL) {
		if (mtd->mtd_buf == 0)
			break;

		txp = mtd->mtd_txp;

		if (SRD(txp->status) == 0)      /* it hasn't really gone yet */
			return;

		if (ethdebug) {
			struct ether_header *eh;

			eh = (struct ether_header *) mtd->mtd_buf;
			printf("xmit status=0x%lx len=%ld type=0x%x from %s",
			    txp->status,
			    txp->pkt_size,
			    htons(eh->ether_type),
			    ether_sprintf(eh->ether_shost));
			printf(" (to %s)\n", ether_sprintf(eh->ether_dhost));
		}
		sc->txb_inuse--;
		mtd->mtd_buf = 0;
		mtdhead = mtd->mtd_link;

		mtd_free(mtd);

		/* XXX - Do stats here. */

		if ((SRD(txp->status) & TCR_PTX) == 0) {
			printf("sonic: Tx packet status=0x%lx\n", txp->status);

			/* XXX - DG This looks bogus */
			if (mtdhead != mtdnext) {
				printf("resubmitting remaining packets\n");
				csr->s_ctda = LOWER(mtdhead->mtd_vtxp);
				csr->s_cr = CR_TXP;
				wbflush();
				return;
			}
		}
	}
	/* mtdhead should be at mtdnext (go) */
	mtdhead = 0;
}

/*
 * Receive interrupt routine (16-bit)
 */
static void 
snrxint16(sc)
	struct sn_softc *sc;
{
	struct sonic_reg	*csr = sc->sc_csr;
	struct _short_RXpkt	*rxp, *p_rda;
	struct _short_RXrsrc	*p_rra;
	int			orra;
	int			len;

	p_rra = (struct _short_RXrsrc *) sc->p_rra;
	p_rda = (struct _short_RXpkt *) sc->p_rda;
	rxp = &p_rda[sc->sc_rxmark];

	while (SRD(rxp->in_use) == 0) {
		unsigned status = SRD(rxp->status);
		if ((status & RCR_LPKT) == 0)
			printf("sonic: more than one packet in RBA!\n");

		orra = RBASEQ(SRD(rxp->seq_no)) & RRAMASK;
		len = SRD(rxp->byte_count)
				- sizeof(struct ether_header) - FCSSIZE;
		if (status & RCR_PRX) {
			if (sonic_read(sc, sc->rbuf[orra & RBAMASK], len)) {
				sc->sc_if.if_ipackets++;
				sc->sc_sum.ls_ipacks++;
				sc->sc_missed = 0;
			}
		} else
			sc->sc_if.if_ierrors++;

		/*
		 * give receive buffer area back to chip.
		 *
		 * orra is now empty of packets and can be freed if
		 * sonic read didnt copy it out then we would have to
		 * wait !!
		 * (dont bother add it back in again straight away)
		 */
		p_rra[sc->sc_rramark] = p_rra[orra];

		/* zap old rra for fun */
		p_rra[orra].buff_wchi = 0;
		p_rra[orra].buff_wclo = 0;

		sc->sc_rramark = (sc->sc_rramark + 1) & RRAMASK;
		csr->s_rwp = LOWER(sc->v_rra +
			(sc->sc_rramark * sizeof(struct _short_RXrsrc)));
		wbflush();

		/*
		 * give receive descriptor back to chip simple
		 * list is circular
		 */
		SWR(rxp->in_use, 1);
		SWR(rxp->rlink, SRD(rxp->rlink) | EOL);
		SWR(((struct _short_RXpkt *) sc->sc_lrxp)->rlink,
		    SRD(((struct _short_RXpkt *) sc->sc_lrxp)->rlink) & ~EOL);
		sc->sc_lrxp = (void *) rxp;

		if (++sc->sc_rxmark >= NRDA)
			sc->sc_rxmark = 0;
		rxp = &p_rda[sc->sc_rxmark];
	}
}

/*
 * Receive interrupt routine (normal 32-bit)
 */
static void 
snrxint32(sc)
	struct sn_softc *sc;
{
	struct sonic_reg  *csr = sc->sc_csr;
	struct RXpkt      *rxp, *p_rda;
	struct RXrsrc	  *p_rra;
	int		  orra;
	int		  len;

	p_rra = (struct RXrsrc *) sc->p_rra;
	p_rda = (struct RXpkt *) sc->p_rda;
	rxp = &p_rda[sc->sc_rxmark];

	while (SRD(rxp->in_use) == 0) {
		unsigned status = SRD(rxp->status);
		if ((status & RCR_LPKT) == 0)
			printf("sonic: more than one packet in RBA!\n");

		orra = RBASEQ(SRD(rxp->seq_no)) & RRAMASK;
		len = SRD(rxp->byte_count)
				- sizeof(struct ether_header) - FCSSIZE;
		if (status & RCR_PRX) {
			if (sonic_read(sc, sc->rbuf[orra & RBAMASK], len)) {
				sc->sc_if.if_ipackets++;
				sc->sc_sum.ls_ipacks++;
				sc->sc_missed = 0;
			}
		} else
			sc->sc_if.if_ierrors++;

		/*
		 * give receive buffer area back to chip.
		 *
		 * orra is now empty of packets and can be freed if
		 * sonic read didnt copy it out then we would have to
		 * wait !!
		 * (dont bother add it back in again straight away)
		 */
		p_rra[sc->sc_rramark] = p_rra[orra];

		/* zap old rra for fun */
		p_rra[orra].buff_wchi = 0;
		p_rra[orra].buff_wclo = 0;

		sc->sc_rramark = (sc->sc_rramark + 1) & RRAMASK;
		csr->s_rwp = LOWER(sc->v_rra +
			(sc->sc_rramark * sizeof(struct RXrsrc)));
		wbflush();

		/*
		 * give receive descriptor back to chip simple
		 * list is circular
		 */
		SWR(rxp->in_use, 1);
		SWR(rxp->rlink, SRD(rxp->rlink) | EOL);
		SWR(((struct RXpkt *) sc->sc_lrxp)->rlink,
			SRD(((struct RXpkt *) sc->sc_lrxp)->rlink) & ~EOL);
		sc->sc_lrxp = (void *) rxp;

		if (++sc->sc_rxmark >= NRDA)
			sc->sc_rxmark = 0;
		rxp = &p_rda[sc->sc_rxmark];
	}
}

/*
 * sonic_read -- pull packet off interface and forward to
 * appropriate protocol handler
 */
int 
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
	 * Deal with trailer protocol: if type is PUP trailer
	 * get true type from first 16-bit word past data.
	 * Remember that type was trailer by setting off.
	 */
	et = (struct ether_header *)pkt;

	if (ethdebug) {
		printf("rcvd 0x%p len=%d type=0x%x from %s",
		    et, len, htons(et->ether_type),
		    ether_sprintf(et->ether_shost));
		printf(" (to %s)\n", ether_sprintf(et->ether_dhost));
	}
	if (len < ETHERMIN || len > ETHERMTU) {
		printf("sonic: invalid packet length %d bytes\n", len);
		return (0);
	}

#if NBPFILTER > 0
	/*
	 * Check if there's a bpf filter listening on this interface.
	 * If so, hand off the raw packet to enet, then discard things
	 * not destined for us (but be sure to keep
broadcast/multicast).
	 */
	if (sc->sc_if.if_bpf) {
		bpf_tap(sc->sc_if.if_bpf, pkt,
		    len + sizeof(struct ether_header));
		if ((ifp->if_flags & IFF_PROMISC) != 0 &&
		    (et->ether_dhost[0] & 1) == 0 && /* !mcast and !bcast */
		    bcmp(et->ether_dhost, sc->sc_enaddr,
			    sizeof(et->ether_dhost)) != 0)
			return (0);
	}
#endif
	m = sonic_get(sc, et, len);
	if (m == NULL)
		return (0);
	ether_input(ifp, et, m);
	return(1);
}

#define sonicdataaddr(eh, off, type)      ((type)(((caddr_t)((eh)+1)+(off))))

/*
 * munge the received packet into an mbuf chain
 * because we are using stupid buffer management this
 * is slow.
 */
struct mbuf *
sonic_get(sc, eh, datalen)
	struct sn_softc *sc;
	struct ether_header *eh;
	int datalen;
{
	struct mbuf *m;
	struct mbuf *top = 0, **mp = &top;
	int	len;
	char	*spkt = sonicdataaddr(eh, 0, caddr_t);
	char	*epkt = spkt + datalen;
	char	*cp = spkt;

	epkt = cp + datalen;
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = &sc->sc_if;
	m->m_pkthdr.len = datalen;
	m->m_len = MHLEN;

	while (datalen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			m->m_len = MLEN;
		}
		len = min(datalen, epkt - cp);
		if (len >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				m->m_len = len = min(len, MCLBYTES);
			else
				len = m->m_len;
		} else {
			/*
			 * Place initial small packet/header at end of mbuf.
			 */
			if (len < m->m_len) {
				if (top == 0 && len + max_linkhdr <= m->m_len)
					m->m_data += max_linkhdr;
				m->m_len = len;
			} else
				len = m->m_len;
		}
		bcopy(cp, mtod(m, caddr_t), (unsigned) len);
		cp += len;
		*mp = m;
		mp = &m->m_next;
		datalen -= len;
		if (cp == epkt)
			cp = spkt;
	}
	return (top);
}
