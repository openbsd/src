/*	$OpenBSD: if_sn.c,v 1.6 1996/09/21 03:34:55 briggs Exp $	*/

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

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#define NTXB	10	/* Number of xmit buffers */

#define SONICDW 32
typedef unsigned char uchar;

#include <machine/cpu.h>
#include <machine/viareg.h>
#include <mac68k/dev/if_sn.h>

#define SWR(a, x) 	(a) = (x)
#define SRD(a)		((a) & 0xffff)

#define wbflush()

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
	struct	arpcom sc_arpcom;
#define	sc_if		sc_arpcom.ac_if		/* network visible interface */
#define	sc_enaddr	sc_arpcom.ac_enaddr	/* hardware ethernet address */

	struct sonic_reg *sc_csr;	/* hardware pointer */
	int	sc_rxmark;		/* position in rx ring for reading buffs */

	int	sc_rramark;		/* index into rra of wp */

	int	sc_txhead;		/* index of first TDA passed to chip  */
	int	sc_missed;		/* missed packet counter */

	int	txb_cnt;		/* total number of xmit buffers */
	int	txb_inuse;		/* number of active xmit buffers */
	int	txb_new;		/* index of next open slot. */

	struct	RXpkt *sc_lrxp;		/* last RDA available to chip */
	struct	sn_stats sc_sum;
	short	sc_iflags;
} sn_softc;

static void snwatchdog __P((struct ifnet *));
static int snmatch __P((struct device *, void *, void *));
static void snattach __P((struct device *, struct device *, void *));
static int sngetaddr __P((struct sn_softc *sc));
static int sninit __P((struct sn_softc *sc));
static int snstop __P((struct sn_softc *sc));
static int sonicput __P((struct sn_softc *sc, struct mbuf *m0));
static int snintr __P((struct sn_softc *, int));
static int snioctl __P((struct ifnet *ifp, u_long cmd, caddr_t data));
static void snstart __P((struct ifnet *ifp));
static void snreset __P((struct sn_softc *sc));

void camdump __P((struct sn_softc *sc));

struct cfattach sn_ca = {
	sizeof(struct sn_softc), snmatch, snattach
};

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
#define	assert(e)	((e) ? (void)0 : __assert(__FILE__, __LINE__, #e))
#else	/* PCC */
#define	assert(e)	((e) ? (void)0 : __assert(__FILE__, __LINE__, "e"))
#endif
#endif

void
__assert(file, line, failedexpr)
	const char *file, *failedexpr;
	int line;
{
	(void)printf(
	    "assertion \"%s\" failed: file \"%s\", line %d\n",
	    failedexpr, file, line);
}

int ethdebug = 0;

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

#define LOWER(x) ((unsigned)(x) & 0xffff)
#define UPPER(x) ((unsigned)(x) >> 16)

/*
 * buffer sizes in 32 bit mode
 * 1 TXpkt is 4 hdr words + (3 * FRAGMAX) + 1 link word
 * FRAGMAX == 16 => 54 words == 216 bytes
 *
 * 1 RxPkt is 7 words == 28 bytes
 * 1 Rda   is 4 words == 16 bytes
 */

#define NRRA	32		/* # receive resource descriptors */
#define RRAMASK	0x1f		/* the reason why it must be power of two */

#define NRBA	16		/* # receive buffers < NRRA */
#define NRDA	NRBA		/* # receive descriptors */
#define NTDA	4		/* # transmit descriptors */

#define CDASIZE sizeof(struct CDA)
#define RRASIZE (NRRA*sizeof(struct RXrsrc))
#define RDASIZE (NRDA*sizeof(struct RXpkt))
#define TDASIZE (NTDA*sizeof(struct TXpkt))

#define FCSSIZE	4		/* size of FCS append te received packets */

/*
 * maximum receive packet size plus 2 byte pad to make each
 * one aligned. 4 byte slop (required for eobc)
 */
#define RBASIZE	(sizeof(struct ether_header) + ETHERMTU	+ FCSSIZE + 2 + 4)

/*
 * space required for descriptors
 */
#define DESC_SIZE (RRASIZE + CDASIZE + RDASIZE + TDASIZE + SONICALIGN - 1)

/*
 * 16k transmit buffer area
 */
#define TXBSIZE	1536	/* 6*2^8 -- the same size as the 8390 TXBUF */
#define TBASIZE	(TXBSIZE * NTXB)

/*
 * Nicely aligned pointers into the SONIC buffers
 * p_ points at physical (K1_SEG) addresses.
 */
struct RXrsrc	*p_rra;	/* receiver resource descriptors */
struct RXpkt	*p_rda;	/* receiver desriptors */
struct TXpkt	*p_tda;	/* transmitter descriptors */
struct CDA	*p_cda;	/* CAM descriptors */
char		*p_rba;	/* receive buffer area base */
char		*p_tba;	/* transmit buffer area base */

/* Meta transmit descriptors */
struct mtd {
	struct	mtd *mtd_link;
	struct	TXpkt *mtd_txp;
	unsigned char *mtd_buf;
} mtda[NTDA];

struct mtd *mtdfree;		/* list of free meta transmit descriptors */
struct mtd *mtdhead;		/* head of descriptors assigned to chip */
struct mtd *mtdtail;		/* tail of descriptors assigned to chip */
struct mtd *mtdnext;		/* next descriptor to give to chip */

void mtd_free __P((struct mtd *));
struct mtd *mtd_alloc __P((void));

static int
snmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	if (!mac68k_machine.sonic)
		return 0;

	return 1;
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
static void
snattach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
extern	unsigned char	SONICSPACE;
extern	unsigned long	SONICSPACE_size;
	struct sn_softc *sc = (void *)self;
	struct ifnet *ifp = &sc->sc_if;
	int base, p, pp;

	/* Must allocate extra memory in case we need to round later. */
	pp = (DESC_SIZE + NRBA*RBASIZE + 0x10000 + 4 + TBASIZE);
	if (pp != SONICSPACE_size) {
		printf(": SONICSPACE_size (%ld) != pp (%d).  Punt!\n",
			SONICSPACE_size, pp);
		return;
	}
	base = p = (int) &SONICSPACE;

#define SONIC_IO_OFFSET	0xA000
	sc->sc_csr = (struct sonic_reg *)(IOBase + SONIC_IO_OFFSET);

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
	if ((p ^ (p + RRASIZE + CDASIZE)) & 0x10000)
		p = (p + 0x10000) & ~0xffff;
	p_rra = (struct RXrsrc *) p;
	p += RRASIZE;

	p_cda = (struct CDA *) p;
	p += CDASIZE;

	if ((p ^ (p + RDASIZE)) & 0x10000)
		p = (p + 0x10000) & ~0xffff;
	p_rda = (struct RXpkt *) p;
	p += RDASIZE;

	if ((p ^ (p + TDASIZE)) & 0x10000)
		p = (p + 0x10000) & ~0xffff;
	p_tda = (struct TXpkt *) p;
	p += TDASIZE;

	p = SOALIGN(p);
	p_rba = (char *) p;
	p += NRBA * RBASIZE;

	p_tba = (char *) p;

#if 0
	camdump(sc);
#endif
	sngetaddr(sc);
	printf(" address %s, ", ether_sprintf(sc->sc_enaddr));
	printf("SONIC ethernet--%d bytes at 0x%x.\n", pp, base);

#if 0
printf("sonic buffers: rra=0x%x cda=0x%x rda=0x%x tda=0x%x rba=0x%x tba=0x%x\n",
	p_rra, p_cda, p_rda, p_tda, p_rba, p_tba);
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

	add_nubus_intr(9, (void (*) __P((void *, int))) snintr, (void *) sc);
	enable_nubus_intr();
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
	int	temp, error;

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
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
		 * If the state of the promiscuous bit changes, the interface
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
	int	len;

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
	len = sonicput(sc, m);
#if DIAGNOSTIC
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
	sc->sc_sum.ls_opacks++;		/* # of pkts */

	/* Jump back for possibly more punishment. */
	goto outloop;
}

/*
 * This is called from sonicioctl() when /etc/ifconfig is run to set
 * the address or switch the i/f on.
 */
void caminitialise __P((void));
void camentry __P((int, unsigned char *ea));
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

	csr->s_cr = CR_RST;	/* s_dcr only accessable reset mode! */

	/* config it */
	csr->s_dcr = DCR_LBR | DCR_SYNC | DCR_WAIT0 | DCR_DW32 | DCR_DMABLOCK |
	    DCR_RFT16 | DCR_TFT16;
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
	initialise_tba(sc);

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

	if (mtdhead && mtdhead->mtd_buf) {
		/* something still pending for transmit */
		if (mtdhead->mtd_txp->status == 0)
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
 * stuff packet into sonic (at splnet)
 */
static int 
sonicput(sc, m0)
	struct sn_softc *sc;
	struct mbuf *m0;
{
	struct sonic_reg *csr = sc->sc_csr;
	unsigned char	*buff, *buffer, *data;
	struct TXpkt *txp;
	struct mtd *mtdnew;
	struct mbuf *m;
	int len = 0, totlen = 0;

	/* grab the replacement mtd */
	if ((mtdnew = mtd_alloc()) == 0)
		return (0);

	/* We are guaranteed, if we get here, that the xmit buffer is free. */
	buff = buffer = p_tba + sc->txb_new * TXBSIZE;
	
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
	SWR(txp->u[0].frag_ptrlo, LOWER(buffer));
	SWR(txp->u[0].frag_ptrhi, UPPER(buffer));
	SWR(txp->u[0].frag_size, totlen);

	if (len < ETHERMIN + sizeof(struct ether_header)) {
		int pad = ETHERMIN + sizeof(struct ether_header) - totlen;
printf("Padding %d to %d bytes\n", totlen, totlen+pad);
		bzero(buffer + totlen, pad);
		SWR(txp->u[0].frag_size, pad + SRD(txp->u[0].frag_size));
		totlen = ETHERMIN + sizeof(struct ether_header);
	}
	SWR(txp->frag_count, 1);
	SWR(txp->pkt_size, totlen);

	/* link onto the next mtd that will be used */
	SWR(txp->u[0].tlink, LOWER(mtdnew->mtd_txp) | EOL);

	if (mtdhead == 0) {
		/* no current transmit list start with this one */
		mtdtail = mtdhead = mtdnext;
		csr->s_ctda = LOWER(txp);
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
	return (totlen);
}

/*
 *  Read out the ethernet address from the cam. It is stored
 *  there by the boot when doing a loopback test. Thus we don't
 *  have to fetch it from nv ram.
 */
static int 
sngetaddr(sc)
	struct sn_softc *sc;
{
	unsigned i;

	sc->sc_csr->s_cr = CR_RST;
	wbflush();
	sc->sc_csr->s_cep = 15; /* For some reason, Apple fills top first. */
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
	unsigned char *ea;
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
	csr->s_cdp = LOWER(p_cda);
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
	int     i;

	csr = sc->sc_csr;

	mtdfree = mtdhead = mtdtail = (struct mtd *) 0;

	for (i = 0; i < NTDA; i++) {
		mtd = &mtda[i];
		mtd->mtd_txp = &p_tda[i];
		mtd->mtd_buf = 0;
		mtd_free(mtd);
	}
	mtdnext = mtd_alloc();

	csr->s_utda = UPPER(p_tda);
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
		SWR(p_rda[i].rlink, LOWER(&p_rda[i + 1]));
		SWR(p_rda[i].in_use, 1);
	}
	SWR(p_rda[NRDA - 1].rlink, LOWER(&p_rda[0]) | EOL);
	SWR(p_rda[NRDA - 1].in_use, 1);

	/* mark end of receive descriptor list */
	sc->sc_lrxp = &p_rda[NRDA - 1];

	sc->sc_rxmark = 0;

	csr->s_urda = UPPER(&p_rda[0]);
	csr->s_crda = LOWER(&p_rda[0]);
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
	csr->s_urra = UPPER(p_rra);
	csr->s_rsa = LOWER(p_rra);
	csr->s_rea = LOWER(&p_rra[NRRA]);
	csr->s_rrp = LOWER(p_rra);

	/* fill up SOME of the rra with buffers */
	for (i = 0; i < NRBA; i++) {
		SWR(p_rra[i].buff_ptrhi, UPPER(&p_rba[i * RBASIZE]));
		SWR(p_rra[i].buff_ptrlo, LOWER(&p_rba[i * RBASIZE]));
		SWR(p_rra[i].buff_wchi, UPPER(RBASIZE / 2));
		SWR(p_rra[i].buff_wclo, LOWER(RBASIZE / 2));
	}
	sc->sc_rramark = NRBA;
	csr->s_rwp = LOWER(&p_rra[sc->sc_rramark]);
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

static int 
snintr(sc, slot)
	struct sn_softc *sc;
	int	slot;
{
	struct sonic_reg *csr = sc->sc_csr;
	int	isr;

	while ((isr = (csr->s_isr & ISR_ALL)) != 0) {
printf("snintr: %x.\n", isr);
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
		snstart(&sc->sc_if);
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

	while ((mtd = mtdhead) != NULL) {
		if (mtd->mtd_buf == 0)
			break;

		txp = mtd->mtd_txp;

		if (SRD(txp->status) == 0)	/* it hasn't really gone yet */
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

			if (mtdhead != mtdnext) {
				printf("resubmitting remaining packets\n");
				csr->s_ctda = LOWER(mtdhead->mtd_txp);
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
		csr->s_rwp = LOWER(&p_rra[sc->sc_rramark]);
		wbflush();

		/*
		 * give receive descriptor back to chip simple
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
	et = (struct ether_header *)pkt;

	if (ethdebug) {
		printf("rcvd 0x%p status=0x%lx, len=%d type=0x%x from %s",
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
			return (0);
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
