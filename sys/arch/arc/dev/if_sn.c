/*	$OpenBSD: if_sn.c,v 1.11 1999/01/11 05:11:09 millert Exp $	*/
/*
 * National Semiconductor  SONIC Driver
 * Copyright (c) 1991   Algorithmics Ltd (http://www.algor.co.uk)
 * You may use, copy, and modify this program so long as you retain the
 * copyright line.
 *
 * This driver has been substantially modified since Algorithmics donated
 * it.
 */

#include "sn.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <machine/autoconf.h>

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

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <arc/dev/dma.h>

#define SONICDW 32
typedef unsigned char uchar;

#include <arc/dev/if_sn.h>
#define SWR(a, x) 	(a) = (x)
#define SRD(a)		((a) & 0xffff)

#include <machine/pte.h>
#include <machine/cpu.h>

/*
 * Statistics collected over time
 */
struct sn_stats {
	int     ls_opacks;	/* packets transmitted */
	int     ls_ipacks;	/* packets received */
	int     ls_tdr;		/* contents of tdr after collision */
	int     ls_tdef;	/* packets where had to wait */
	int     ls_tone;	/* packets with one retry */
	int     ls_tmore;	/* packets with more than one retry */
	int     ls_tbuff;	/* transmit buff errors */
	int     ls_tuflo;	/* "      uflo  "     */
	int     ls_tlcol;
	int     ls_tlcar;
	int     ls_trtry;
	int     ls_rbuff;	/* receive buff errors */
	int     ls_rfram;	/* framing     */
	int     ls_roflo;	/* overflow    */
	int     ls_rcrc;
	int     ls_rrng;	/* rx ring sequence error */
	int     ls_babl;	/* chip babl error */
	int     ls_cerr;	/* collision error */
	int     ls_miss;	/* missed packet */
	int     ls_merr;	/* memory error */
	int     ls_copies;	/* copies due to out of range mbufs */
	int     ls_maxmbufs;	/* max mbufs on transmit */
	int     ls_maxslots;	/* max ring slots on transmit */
};

struct sn_softc {
	struct	device sc_dev;
	struct	arpcom sc_ac;
#define	sc_if		sc_ac.ac_if	/* network visible interface */
#define	sc_enaddr	sc_ac.ac_enaddr	/* hardware ethernet address */

	struct sonic_reg *sc_csr;	/* hardware pointer */
	dma_softc_t	__dma;		/* stupid macro ... */
	dma_softc_t	*dma;		/* dma mapper control */
	int	sc_rxmark;		/* position in rx ring for reading buffs */

	int	sc_rramark;		/* index into rra of wp */

	int	sc_txhead;		/* index of first TDA passed to chip  */
	int	sc_missed;		/* missed packet counter */
	struct	RXpkt *sc_lrxp;		/* last RDA available to chip */
	struct	sn_stats sc_sum;
	short	sc_iflags;
} sn_softc;

int snmatch __P((struct device *, void *, void *));
void snattach __P((struct device *, struct device *, void *));

struct cfattach sn_ca = {
	sizeof(struct sn_softc), snmatch, snattach
};
struct cfdriver sn_cd = {
	NULL, "sn", DV_IFNET, NULL, 0
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

int ethdebug = 0;

int snintr __P((struct sn_softc *));
int snioctl __P((struct ifnet *ifp, u_long cmd, caddr_t data));
void snstart __P((struct ifnet *ifp));
void snwatchdog __P((struct ifnet *ifp));
void snreset __P((struct sn_softc *sc));

/*
 * SONIC buffers need to be aligned 16 or 32 bit aligned.
 * These macros calculate and verify alignment.
 */
#if SONICDW == 32
#define SONICALIGN 4
#else
#define SONICALIGN 2
#endif
#define SOALIGN(array) (((int)array+SONICALIGN-1) & ~(SONICALIGN-1))
#define SOALIGNED(p) (!(((uint)p)&(SONICALIGN-1)))

#define UPPER(x) ((unsigned)(x) >> 16)
#define LOWER(x) ((unsigned)(x) & 0xffff)

#define NRRA	32		/* # receive resource descriptors */
#define RRAMASK	0x1f		/* why it must be poer of two */

#define NRBA	16		/* # receive buffers < NRRA */
#define NRDA	NRBA		/* # receive descriptors */
#define NTDA	4		/* # transmit descriptors */

#define CDASIZE sizeof(struct CDA)
#define RRASIZE (NRRA*sizeof(struct RXrsrc))
#define RDASIZE (NRDA*sizeof(struct RXpkt))
#define TDASIZE (NTDA*sizeof(struct TXpkt))

#define FCSSIZE	4		/* size of FCS append te received packets */

/*
 * maximum recieve packet size plus 2 byte pad to make each
 * one aligned. 4 byte slop (required for eobc)
 */
#define RBASIZE	(sizeof(struct ether_header) + ETHERMTU	+ FCSSIZE + 2 + 4)

/*
 * space requiered for descriptors
 */
#define DESC_SIZE (RRASIZE + CDASIZE + RDASIZE + TDASIZE + SONICALIGN - 1)

/*
 *  This should really be 'allocated' but for now we
 *  'hardwire' it.
 */
#define SONICBUF	0xa0010000

/*
 *  Nicely aligned pointers into the sonicbuffers
 *  p_ points at physical (K1_SEG) addresses.
 *  v_ is dma viritual address used by sonic.
 */
struct RXrsrc	*p_rra;	/* receiver resource descriptors */
struct RXrsrc	*v_rra;
struct RXpkt	*p_rda;	/* receiver desriptors */
struct RXpkt	*v_rda;
struct TXpkt	*p_tda;	/* transmitter descriptors */
struct TXpkt	*v_tda;
struct CDA	*p_cda;	/* CAM descriptors */
struct CDA	*v_cda;
char		*p_rba;	/* receive buffer area base */
char		*v_rba;

/* Meta transmit descriptors */
struct mtd {
	struct	mtd *mtd_link;
	struct	TXpkt *mtd_txp;
	struct	mbuf *mtd_mbuf;
} mtda[NTDA];

struct mtd *mtdfree;		/* list of free meta transmit descriptors */
struct mtd *mtdhead;		/* head of descriptors assigned to chip */
struct mtd *mtdtail;		/* tail of descriptors assigned to chip */
struct mtd *mtdnext;		/* next descriptor to give to chip */

void mtd_free __P((struct mtd *));
struct mtd *mtd_alloc __P((void));

int sngetaddr __P((struct sn_softc *sc));
int sninit __P((struct sn_softc *sc));
int snstop __P((struct sn_softc *sc));
int sonicput __P((struct sn_softc *sc, struct mbuf *m0));

void camdump __P((struct sn_softc *sc));

int
snmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct confargs *ca = aux;

	/* XXX CHECK BUS */
	/* make sure that we're looking for this type of device. */
	if (!BUS_MATCHNAME(ca, "sonic"))
		return (0);

	return (1);
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
snattach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
	struct sn_softc *sc = (void *)self;
	struct confargs *ca = aux;
	struct ifnet *ifp = &sc->sc_if;
	int p, pp;

	sc->sc_csr = (struct sonic_reg *)BUS_CVTADDR(ca);

	sc->dma = &sc->__dma;
	sn_dma_init(sc->dma, FRAGMAX * NTDA
			   + (NRBA * RBASIZE / R4030_DMA_PAGE_SIZE) + 1
			   + (DESC_SIZE * 2 / R4030_DMA_PAGE_SIZE) + 1);

/*
 * because the sonic is basicly 16bit device it 'concatenates'
 * a higher buffer address to a 16 bit offset this will cause wrap
 * around problems near the end of 64k !!
 */
	p = SONICBUF;
	pp = SONICBUF - (FRAGMAX * NTDA * R4030_DMA_PAGE_SIZE);

	if ((p ^ (p + TDASIZE)) & 0x10000)
		p = (p + 0x10000) & ~0xffff;
	p_tda = (struct TXpkt *) p;
	v_tda = (struct TXpkt *)(p - pp + sc->dma->dma_va);
	p += TDASIZE;

	if ((p ^ (p + RRASIZE + CDASIZE)) & 0x10000)
		p = (p + 0x10000) & ~0xffff;
	p_rra = (struct RXrsrc *) p;
	v_rra = (struct RXrsrc *)(p - pp + sc->dma->dma_va);
	p += RRASIZE;

	if ((p ^ (p + RDASIZE)) & 0x10000)
		p = (p + 0x10000) & ~0xffff;
	p_rda = (struct RXpkt *) p;
	v_rda = (struct RXpkt *)(p - pp + sc->dma->dma_va);
	p += RDASIZE;

	p_cda = (struct CDA *) p;
	v_cda = (struct CDA *)(p - pp + sc->dma->dma_va);
	p += CDASIZE;

	p += R4030_DMA_PAGE_SIZE - (p & (R4030_DMA_PAGE_SIZE -1));
	p_rba = (char *)p;
	v_rba = (char *)(p - pp + sc->dma->dma_va);
	p += NRBA * RBASIZE;

	DMA_MAP(sc->dma, (caddr_t)SONICBUF, p - SONICBUF, SONICBUF - pp);
	printf(": bufsize %d",p - SONICBUF);

#if 0
	camdump(sc);
#endif
	sngetaddr(sc);
	printf(" address %s\n", ether_sprintf(sc->sc_enaddr));

#if 0
printf("\nsonic buffers: rra=0x%x cda=0x%x rda=0x%x tda=0x%x rba=0x%x\n",
	p_rra, p_cda, p_rda, p_tda, p_rba);
printf("sonic buffers: rra=0x%x cda=0x%x rda=0x%x tda=0x%x rba=0x%x\n",
	v_rra, v_cda, v_rda, v_tda, v_rba);
printf("mapped to offset 0x%x size 0x%x\n", SONICBUF - pp, p - SONICBUF);
#endif

	BUS_INTR_ESTABLISH(ca, (intr_handler_t)snintr, (void *)sc);

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
}

int
snioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ifaddr *ifa;
	struct sn_softc *sc = ifp->if_softc;
	int     s = splnet(), err = 0;
	int	temp;
	int	error;

	if ((error = ether_ioctl(ifp, &sc->sc_ac, cmd, data)) > 0) {
		splx(s);
		return error;
	}

	switch (cmd) {

	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			(void)sninit(ifp->if_softc);
			arp_ifinit(&sc->sc_ac, ifa);
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
		 * If the state of the promiscuous bit changes, the interface
		 * must be reset to effect the change.
		 */
		if (((ifp->if_flags ^ sc->sc_iflags) & IFF_PROMISC) &&
		    (ifp->if_flags & IFF_RUNNING)) {
			sc->sc_iflags = ifp->if_flags;
			temp = sc->sc_if.if_flags & IFF_UP;
			snreset(sc);
			sc->sc_if.if_flags |= temp;
			snstart(ifp);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if(cmd == SIOCADDMULTI)
			err = ether_addmulti((struct ifreq *)data, &sc->sc_ac);
		else
			err = ether_delmulti((struct ifreq *)data, &sc->sc_ac);

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
void
snstart(ifp)
	struct ifnet *ifp;
{
	struct sn_softc *sc = ifp->if_softc;
	struct mbuf *m;

	if ((sc->sc_if.if_flags & IFF_RUNNING) == 0)
		return;
	IF_DEQUEUE(&sc->sc_if.if_snd, m);
	if (m == 0)
		return;

	/*
	 * If there is nothing in the o/p queue, and there is room in
	 * the Tx ring, then send the packet directly.  Otherwise append
	 * it to the o/p queue.
	 */
	if (!sonicput(sc, m)) { /* not enough space */
		IF_PREPEND(&sc->sc_if.if_snd, m);
	}
#if NBPFILTER > 0
	/*
	 * If bpf is listening on this interface, let it
	 * see the packet before we commit it to the wire.
	 */
	if (sc->sc_if.if_bpf)
		bpf_mtap(sc->sc_if.if_bpf, m);
#endif

	sc->sc_if.if_opackets++;	/* # of pkts */
	sc->sc_sum.ls_opacks++;		/* # of pkts */
}

/*
 * This is called from sonicioctl() when /etc/ifconfig is run to set
 * the address or switch the i/f on.
 */
void caminitialise __P((void));
void camentry __P((int, u_char *ea));
void camprogram __P((struct sn_softc *));
void initialise_tda __P((struct sn_softc *));
void initialise_rda __P((struct sn_softc *));
void initialise_rra __P((struct sn_softc *));

/*
 * reset and restart the SONIC.  Called in case of fatal
 * hardware/software errors.
 */
void
snreset(sc)
	struct sn_softc *sc;
{
	snstop(sc);
	sninit(sc);
}

int 
sninit(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr = sc->sc_csr;
	int s;

	if (sc->sc_if.if_flags & IFF_RUNNING)
		/* already running */
		return (0);

	s = splnet();

	csr->s_cr = CR_RST;	/* s_dcr only accessable reset mode! */

	/* config it */
	csr->s_dcr = DCR_LBR | DCR_SYNC | DCR_WAIT0 | DCR_DW32 | 
	    DCR_RFT4 | DCR_TFT28; /*XXX RFT & TFT according to MIPS manual */
	csr->s_rcr = RCR_BRD | RCR_LBNONE;
	csr->s_imr = IMR_PRXEN | IMR_PTXEN | IMR_TXEREN | IMR_HBLEN | IMR_LCDEN;

	/* clear pending interrupts */
	csr->s_isr = 0x7fff;

	/* clear tally counters */
	csr->s_crct = -1;
	csr->s_faet = -1;
	csr->s_mpt = -1;

	initialise_tda(sc);
	initialise_rda(sc);
	initialise_rra(sc);

	/* enable the chip */
	csr->s_cr = 0;
	wbflush();

	/* program the CAM with our address */
	caminitialise();
	camentry(0, sc->sc_enaddr);
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
int 
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
	while ((mtd = mtdhead)) {
		mtdhead = mtdhead->mtd_link;
		if (mtd->mtd_mbuf)
			m_freem(mtd->mtd_mbuf);
		mtd->mtd_mbuf = 0;
		mtd_free(mtd);
	}
	mtdnext = mtd_alloc();

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
void
snwatchdog(ifp)
	struct ifnet *ifp;
{
	struct sn_softc *sc = ifp->if_softc;
	int temp;

	if (mtdhead && mtdhead->mtd_mbuf) {
		/* something still pending for transmit */
		if (mtdhead->mtd_txp->status == 0)
			log(LOG_ERR, "%s: Tx - timeout\n",
			    sc->sc_if.if_xname);
		else
			log(LOG_ERR, "%s: Tx - lost interrupt\n",
			    sc->sc_if.if_xname);
		temp = sc->sc_if.if_flags & IFF_UP;
		snreset(sc);
		sc->sc_if.if_flags |= temp;
	}
}
/*
 * stuff packet into sonic (at splnet)
*/
int 
sonicput(sc, m0)
	struct sn_softc *sc;
	struct mbuf *m0;
{
	struct sonic_reg *csr = sc->sc_csr;
	struct TXpkt *txp;
	struct mtd *mtdnew;
	struct mbuf *m;
	int len = 0, fr = 0;
	int fragoffset;		/* Offset in viritual dma space for fragment */

	/* grab the replacement mtd */
	if ((mtdnew = mtd_alloc()) == 0)
		return (0);

	/* this packet goes to mdtnext fill in the TDA */
	mtdnext->mtd_mbuf = m0;
	txp = mtdnext->mtd_txp;
	SWR(txp->config, 0);
	fragoffset = (txp - p_tda) * FRAGMAX * R4030_DMA_PAGE_SIZE;

	/*
	 * Now fill in the fragments. Each fragment maps to it's
	 * own dma page. Fragments crossing a dma page boundary
	 * are split up in two fragments. This is somewhat stupid
	 * because the dma mapper can do the work, but it helps
	 * keeping the fragments in order. (read lazy programmer).
	 */
	for (m = m0; m; m = m->m_next) {
		unsigned va = (unsigned) mtod(m, caddr_t);
		int resid = m->m_len;

		if(resid != 0) {
			R4K_HitFlushDCache(va, resid);
			DMA_MAP(sc->dma, (caddr_t)va, resid, fragoffset);
		}
		len += resid;

		while (resid) {
			unsigned pa;
			unsigned n;

			pa = sc->dma->dma_va + (va & PGOFSET) + fragoffset;
			n = resid;
			if (n > NBPG - (va & PGOFSET)) {
				n = NBPG - (va & PGOFSET);
			}
			if (fr < FRAGMAX) {
				SWR(txp->u[fr].frag_ptrlo, LOWER(pa));
				SWR(txp->u[fr].frag_ptrhi, UPPER(pa));
				SWR(txp->u[fr].frag_size, n);
			}
			fr++;
			va += n;
			resid -= n;
			fragoffset += R4030_DMA_PAGE_SIZE;
		}
	}
	/*
	 * pad out last fragment for minimum size
	 */
        if (len < ETHERMIN + sizeof(struct ether_header) && fr < FRAGMAX) {
                int pad = ETHERMIN + sizeof(struct ether_header) - len;
                static char zeros[64];
                unsigned pa;

                DMA_MAP(sc->dma, (caddr_t)zeros, pad, fragoffset);
                pa = sc->dma->dma_va + ((unsigned)zeros & PGOFSET) + fragoffset;
                SWR(txp->u[fr].frag_ptrlo, LOWER(pa));
                SWR(txp->u[fr].frag_ptrhi, UPPER(pa));
                SWR(txp->u[fr].frag_size, pad);
                fr++;
                len = ETHERMIN + sizeof(struct ether_header);
        }

	DMA_START(sc->dma, (caddr_t)0, 0, 0); /* Flush dma tlb */

	if (fr > FRAGMAX) {
		mtd_free(mtdnew);
		m_freem(m0);
		log(LOG_ERR, "%s: tx too many fragments %d\n",
		    sc->sc_if.if_xname, fr);
		sc->sc_if.if_oerrors++;
		return (len);
	}

	SWR(txp->frag_count, fr);
	SWR(txp->pkt_size, len);

	/* link onto the next mtd that will be used */
	SWR(txp->u[fr].tlink, LOWER(v_tda + (mtdnew->mtd_txp - p_tda)) | EOL);

	if (mtdhead == 0) {
		/* no current transmit list start with this one */
		mtdtail = mtdhead = mtdnext;
		csr->s_ctda = LOWER(v_tda + (txp - p_tda));
	} else {
		/*
		 * have a transmit list append it to end note
		 * mtdnext is already physicaly linked to mtdtail in
		 * mtdtail->mtd_txp->u[mtdtail->mtd_txp->frag_count].tlink
		 */
		SWR(mtdtail->mtd_txp->u[mtdtail->mtd_txp->frag_count].tlink,
		    SRD(mtdtail->mtd_txp->u[mtdtail->mtd_txp->frag_count].tlink) & ~EOL);
		mtdtail = mtdnext;
	}
	mtdnext->mtd_link = mtdnew;
	mtdnext = mtdnew;

	/* make sure chip is running */
	wbflush();
	csr->s_cr = CR_TXP;
	wbflush();
	sc->sc_if.if_timer = 5;	/* 5 seconds to watch for failing to transmit */
	return (len);
}

/*
 *  Read out the ethernet address from the cam. It is stored
 *  there by the boot when doing a loopback test. Thus we don't
 *  have to fetch it from nv ram.
 */
int 
sngetaddr(sc)
	struct sn_softc *sc;
{
#if 0
	int i;

	sc->sc_csr->s_cr = CR_RST;
	wbflush();
	sc->sc_csr->s_cep = 0;
	i = sc->sc_csr->s_cap2;
	wbflush();
	sc->sc_enaddr[5] = i >> 8;
	sc->sc_enaddr[4] = i;
	i = sc->sc_csr->s_cap1;
	wbflush();
	sc->sc_enaddr[3] = i >> 8;
	sc->sc_enaddr[2] = i;
	i = sc->sc_csr->s_cap0;
	wbflush();
	sc->sc_enaddr[1] = i >> 8;
	sc->sc_enaddr[0] = i;

	sc->sc_csr->s_cr = 0;
	wbflush();
#else
	sc->sc_enaddr[0] = 0x08;
	sc->sc_enaddr[1] = 0x00;
	sc->sc_enaddr[2] = 0x20;
	sc->sc_enaddr[3] = 0xa0;
	sc->sc_enaddr[4] = 0x66;
	sc->sc_enaddr[5] = 0x54;
#endif	
	return (0);
}

void sonictxint __P((struct sn_softc *));
void sonicrxint __P((struct sn_softc *));

int sonic_read __P((struct sn_softc *, struct RXpkt *));
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
caminitialise()
{
	int     i;

	for (i = 0; i < MAXCAM; i++)
		SWR(p_cda->desc[i].cam_ep, i);
	SWR(p_cda->enable, 0);
}

void 
camentry(entry, ea)
	int entry;
	u_char *ea;
{
	SWR(p_cda->desc[entry].cam_ep, entry);
	SWR(p_cda->desc[entry].cam_ap2, (ea[5] << 8) | ea[4]);
	SWR(p_cda->desc[entry].cam_ap1, (ea[3] << 8) | ea[2]);
	SWR(p_cda->desc[entry].cam_ap0, (ea[1] << 8) | ea[0]);
	SWR(p_cda->enable, SRD(p_cda->enable) | (1 << entry));
}

void 
camprogram(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr;
	int     timeout;

	csr = sc->sc_csr;
	csr->s_cdp = LOWER(v_cda);
	csr->s_cdc = MAXCAM;
	csr->s_cr = CR_LCAM;
	wbflush();

	timeout = 10000;
	while (csr->s_cr & CR_LCAM && timeout--)
		continue;
	if (timeout == 0) {
		/* XXX */
		panic("sonic: CAM initialisation failed");
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
	printf("CAM enable 0x%x\n", csr->s_cep);

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
	int     i;

	csr = sc->sc_csr;

	mtdfree = mtdhead = mtdtail = (struct mtd *) 0;

	for (i = 0; i < NTDA; i++) {
		mtd = &mtda[i];
		mtd->mtd_txp = &p_tda[i];
		mtd->mtd_mbuf = (struct mbuf *) 0;
		mtd_free(mtd);
	}
	mtdnext = mtd_alloc();

	csr->s_utda = UPPER(v_tda);
}

void 
initialise_rda(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr;
	int     i;

	csr = sc->sc_csr;

	/* link the RDA's together into a circular list */
	for (i = 0; i < (NRDA - 1); i++) {
		SWR(p_rda[i].rlink, LOWER(&v_rda[i + 1]));
		SWR(p_rda[i].in_use, 1);
	}
	SWR(p_rda[NRDA - 1].rlink, LOWER(&v_rda[0]) | EOL);
	SWR(p_rda[NRDA - 1].in_use, 1);

	/* mark end of receive descriptor list */
	sc->sc_lrxp = &p_rda[NRDA - 1];

	sc->sc_rxmark = 0;

	csr->s_urda = UPPER(&v_rda[0]);
	csr->s_crda = LOWER(&v_rda[0]);
	wbflush();
}

void 
initialise_rra(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr;
	int     i;

	csr = sc->sc_csr;

	csr->s_eobc = RBASIZE / 2 - 2;	/* must be >= MAXETHERPKT */
	csr->s_urra = UPPER(v_rra);
	csr->s_rsa = LOWER(v_rra);
	csr->s_rea = LOWER(&v_rra[NRRA]);
	csr->s_rrp = LOWER(v_rra);
	csr->s_rsc = 0;

	/* fill up SOME of the rra with buffers */
	for (i = 0; i < NRBA; i++) {
		SWR(p_rra[i].buff_ptrhi, UPPER(&v_rba[i * RBASIZE]));
		SWR(p_rra[i].buff_ptrlo, LOWER(&v_rba[i * RBASIZE]));
		SWR(p_rra[i].buff_wchi, UPPER(RBASIZE / 2));
		SWR(p_rra[i].buff_wclo, LOWER(RBASIZE / 2));
	}
	sc->sc_rramark = NRBA;
	csr->s_rwp = LOWER(&v_rra[sc->sc_rramark]);
	wbflush();
}

int 
snintr(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr = sc->sc_csr;
	int	isr;

	while ((isr = (csr->s_isr & ISR_ALL))) {
		/* scrub the interrupts that we are going to service */
		csr->s_isr = isr;
		wbflush();

		if (isr & (ISR_BR | ISR_LCD | ISR_PINT | ISR_TC))
			printf("sonic: unexpected interrupt status 0x%x\n", isr);

		if (isr & (ISR_TXDN | ISR_TXER))
			sonictxint(sc);

		if (isr & ISR_PKTRX)
			sonicrxint(sc);

		if (isr & (ISR_HBL | ISR_RDE | ISR_RBE | ISR_RBAE | ISR_RFO)) {
			if (isr & ISR_HBL)
				printf("sonic: no heartbeat\n");
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
	}
	return (1);
}

/*
 * Transmit interrupt routine
 */
void 
sonictxint(sc)
	struct sn_softc *sc;
{
	struct TXpkt *txp;
	struct sonic_reg *csr;
	struct mtd *mtd;

	if (mtdhead == (struct mtd *) 0)
		return;

	csr = sc->sc_csr;

	while ((mtd = mtdhead)) {
		struct mbuf *m = mtd->mtd_mbuf;

		if (m == 0)
			break;

		txp = mtd->mtd_txp;

		if (SRD(txp->status) == 0)	/* it hasn't really gone yet */
			return;

		if (ethdebug) {
			struct ether_header *eh = mtod(m, struct ether_header *);
			printf("xmit status=0x%x len=%d type=0x%x from %s",
			    txp->status,
			    txp->pkt_size,
			    htons(eh->ether_type),
			    ether_sprintf(eh->ether_shost));
			printf(" (to %s)\n", ether_sprintf(eh->ether_dhost));
		}
		m_freem(m);
		mtd->mtd_mbuf = 0;
		mtdhead = mtd->mtd_link;

		mtd_free(mtd);

		if ((SRD(txp->status) & TCR_PTX) == 0) {
			if (mtdhead != mtdnext) {
				csr->s_ctda = LOWER(v_tda + (mtdhead->mtd_txp - p_tda));
				csr->s_cr = CR_TXP;
				wbflush();
				return;
			}
		}
	}
	/* mtdhead should be at mtdnext (go) */
	assert(mtdhead == mtdnext);
	assert(mtdhead->mtd_link == 0);
	mtdhead = 0;

	/* and start feeding any queued packets to chip */
	while (1) {
		struct mbuf *m;

		IF_DEQUEUE(&sc->sc_if.if_snd, m);
		if (m == 0)	/* nothing left to send */
			break;
		if (!sonicput(sc, m)) {	/* not enough space */
			IF_PREPEND(&sc->sc_if.if_snd, m);
			break;
		}
	}
}

/*
 * Receive interrupt routine
 */
void 
sonicrxint(sc)
	struct sn_softc *sc;
{
	struct sonic_reg *csr = sc->sc_csr;
	struct RXpkt *rxp;
	int     orra;

	rxp = &p_rda[sc->sc_rxmark];

	while (SRD(rxp->in_use) == 0) {
		unsigned status = SRD(rxp->status);
		if ((status & RCR_LPKT) == 0)
			printf("sonic: more than one packet in RBA!\n");
		assert(PSNSEQ(SRD(rxp->seq_no)) == 0);

		if (status & RCR_PRX) {
			if (sonic_read(sc, rxp)) {
				sc->sc_if.if_ipackets++;
				sc->sc_sum.ls_ipacks++;
				sc->sc_missed = 0;
			}
		} else
			sc->sc_if.if_ierrors++;

		/*
		 * give receive buffer area back to chip XXX what buffer
		 * did the sonic use for this descriptor answer look at
		 * the rba sequence number !! 
		 */
		orra = RBASEQ(SRD(rxp->seq_no)) & RRAMASK;

		assert(SRD(rxp->pkt_ptrhi) == SRD(p_rra[orra].buff_ptrhi));
		assert(SRD(rxp->pkt_ptrlo) == SRD(p_rra[orra].buff_ptrlo));
if(SRD(rxp->pkt_ptrlo) != SRD(p_rra[orra].buff_ptrlo))
printf("%x,%x\n",SRD(rxp->pkt_ptrlo),SRD(p_rra[orra].buff_ptrlo));
		assert(SRD(p_rra[orra].buff_wclo));

		/*
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
		csr->s_rwp = LOWER(&v_rra[sc->sc_rramark]);
		wbflush();

		/*
		 * give recieve descriptor back to chip simple
		 * list is circular
		 */
		SWR(rxp->in_use, 1);
		SWR(rxp->rlink, SRD(rxp->rlink) | EOL);
		SWR(sc->sc_lrxp->rlink, SRD(sc->sc_lrxp->rlink) & ~EOL);
		sc->sc_lrxp = rxp;

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
sonic_read(sc, rxp)
	struct sn_softc *sc;
	struct RXpkt *rxp;
{
	struct ifnet *ifp = &sc->sc_if;
	/*extern char *ether_sprintf();*/
	struct ether_header *et;
	struct mbuf *m;
	int     len;
	caddr_t	pkt;

	/*
         * Get input data length.
         * Get pointer to ethernet header (in input buffer).
         * Deal with trailer protocol: if type is PUP trailer
         * get true type from first 16-bit word past data.
         * Remember that type was trailer by setting off.
         */

	len = SRD(rxp->byte_count) - sizeof(struct ether_header) - FCSSIZE;
	pkt = (caddr_t)((SRD(rxp->pkt_ptrhi) << 16) | SRD(rxp->pkt_ptrlo));
	pkt = pkt - v_rba + p_rba;
	et = (struct ether_header *)pkt;

	if (ethdebug) {
		printf("rcvd 0x%x status=0x%x, len=%d type=0x%x from %s",
		    et, rxp->status, len, htons(et->ether_type),
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
	 * not destined for us (but be sure to keep broadcast/multicast).
	 */
	if (sc->sc_if.if_bpf) {
		bpf_tap(sc->sc_if.if_bpf, pkt,
		    len + sizeof(struct ether_header));
		if ((ifp->if_flags & IFF_PROMISC) != 0 &&
		    (et->ether_dhost[0] & 1) == 0 && /* !mcast and !bcast */
		    bcmp(et->ether_dhost, sc->sc_enaddr,
			    sizeof(et->ether_dhost)) != 0)
			return(0);
	}
#endif
	m = sonic_get(sc, et, len);
	if (m == NULL)
		return (0);
	ether_input(ifp, et, m);
	return(1);
}

#define sonicdataaddr(eh, off, type)       ((type)(((caddr_t)((eh)+1)+(off))))

/*
 * munge the received packet into a mbuf chain
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
	char   *spkt = sonicdataaddr(eh, 0, caddr_t);
	char   *epkt = spkt + datalen;
	char *cp = spkt;

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
