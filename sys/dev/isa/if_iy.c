/*	$OpenBSD: if_iy.c,v 1.4 1999/02/28 03:23:38 jason Exp $	*/
/*	$NetBSD: if_iy.c,v 1.4 1996/05/12 23:52:53 mycroft Exp $	*/
/* #define IYDEBUG */
/* #define IYMEMDEBUG */
/*-
 * Copyright (c) 1996 Ignatios Souvatzis.
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
 *	This product contains software developed by Ignatios Souvatzis for
 *	the NetBSD project.
 * 4. The names of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>
#include <net/route.h>

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

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/pio.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/i82595reg.h>

#define	ETHER_MIN_LEN	64
#define	ETHER_MAX_LEN	1518

/*
 * Ethernet status, per interface.
 */
struct iy_softc {
	struct device sc_dev;
	void *sc_ih;

	int sc_iobase;
	struct arpcom sc_arpcom;

#define MAX_MBS 8
	struct mbuf *mb[MAX_MBS];
	int next_mb, last_mb;

	int mappedirq;

	int hard_vers;

	int promisc;

	int sram, tx_size, rx_size;

	int tx_start, tx_end, tx_last;
	int rx_start;

#ifdef IYDEBUG
	int sc_debug;
#endif
};

void iywatchdog __P((struct ifnet *));
int iyioctl __P((struct ifnet *, u_long, caddr_t));
int iyintr __P((void *));
void iyinit __P((struct iy_softc *));
void iystop __P((struct iy_softc *));
void iystart __P((struct ifnet *));

void iy_intr_rx __P((struct iy_softc *));
void iy_intr_tx __P((struct iy_softc *));
void eepro_reset_595 __P((struct iy_softc *));
int eepro_probe __P((struct iy_softc *, struct isa_attach_args *));
void eepro_eeprom_outbits __P((struct iy_softc *, int, int));
void eepro_eeprom_clock __P((struct iy_softc *, int));
u_short eepro_read_eeprom __P((struct iy_softc *, int));
int eepro_eeprom_inbits __P((struct iy_softc *));

void iyreset __P((struct iy_softc *));
void iy_readframe __P((struct iy_softc *, int));
void iy_drop_packet_buffer __P((struct iy_softc *));
void iy_find_mem_size __P((struct iy_softc *));
void iyrint __P((struct iy_softc *));
void iytint __P((struct iy_softc *));
void iyxmit __P((struct iy_softc *));
void iyget __P((struct iy_softc *, int, int));
void iymbuffill __P((void *)); 
void iymbufempty __P((void *));
void iyprobemem __P((struct iy_softc *));

/*
 * void iymeminit __P((void *, struct iy_softc *));
 * static int iy_mc_setup __P((struct iy_softc *, void *));
 * static void iy_mc_reset __P((struct iy_softc *));
 */
#ifdef IYDEBUGX
void print_rbd __P((volatile struct iy_recv_buf_desc *));

int in_ifrint = 0;
int in_iftint = 0;
#endif

int iyprobe __P((struct device *, void *, void *));
void iyattach __P((struct device *, struct device *, void *));

static u_int16_t eepromread __P((int, int));

struct cfattach iy_ca = {
	sizeof(struct iy_softc), iyprobe, iyattach
};

struct cfdriver iy_cd = {
	NULL, "iy", DV_IFNET
};

static u_int8_t eepro_irqmap[] = EEPP_INTMAP;
static u_int8_t eepro_revirqmap[] = EEPP_RINTMAP;

int
iyprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct iy_softc *sc = match;
	struct isa_attach_args *ia = aux;

	u_int16_t eaddr[8];
	int iobase;
	int i;

	u_int16_t checksum = 0;
	u_int16_t eepromtmp;
	u_int8_t c, d;

	
	iobase = ia->ia_iobase;

	if (iobase == -1)
		return 0;

	/* try to find the round robin sig: */

	/* check here for addresses already given to other devices */

	c = inb(iobase + ID_REG);
	if (c & ID_REG_MASK != ID_REG_SIG)
		return 0;

	d = inb(iobase + ID_REG);
	if (d & ID_REG_MASK != ID_REG_SIG)
		return 0;

	if (((d-c) & R_ROBIN_BITS) != 0x40)
		return 0;
		
	d = inb(iobase + ID_REG);
	if (d & ID_REG_MASK != ID_REG_SIG)
		return 0;

	if (((d-c) & R_ROBIN_BITS) != 0x80)
		return 0;
		
	d = inb(iobase + ID_REG);
	if (d & ID_REG_MASK != ID_REG_SIG)
		return 0;

	if (((d-c) & R_ROBIN_BITS) != 0xC0)
		return 0;
		
	d = inb(iobase + ID_REG);
	if (d & ID_REG_MASK != ID_REG_SIG)
		return 0;

	if (((d-c) & R_ROBIN_BITS) != 0x00)
		return 0;
		
#ifdef IYDEBUG
		printf("eepro_probe verified working ID reg.\n");
#endif
	
	for (i=0; i<64; ++i) {
		eepromtmp = eepromread(iobase, i);
		checksum += eepromtmp;
		if (i<(sizeof(eaddr)/sizeof(*eaddr)))
			eaddr[i] = eepromtmp;
	}
	if (checksum != EEPP_CHKSUM)
		printf("wrong EEPROM checksum 0x%x should be 0x%x\n",
		    checksum, EEPP_CHKSUM);
		
	
	if ((eaddr[EEPPEther0] != eepromread(iobase, EEPPEther0a)) &&
	    (eaddr[EEPPEther1] != eepromread(iobase, EEPPEther1a)) &&
	    (eaddr[EEPPEther2] != eepromread(iobase, EEPPEther2a)))
		printf("EEPROM Ethernet address differs from copy\n");
	
        sc->sc_arpcom.ac_enaddr[1] = eaddr[EEPPEther0] & 0xFF;
        sc->sc_arpcom.ac_enaddr[0] = eaddr[EEPPEther0] >> 8;
        sc->sc_arpcom.ac_enaddr[3] = eaddr[EEPPEther1] & 0xFF;
        sc->sc_arpcom.ac_enaddr[2] = eaddr[EEPPEther1] >> 8;
        sc->sc_arpcom.ac_enaddr[5] = eaddr[EEPPEther2] & 0xFF;
        sc->sc_arpcom.ac_enaddr[4] = eaddr[EEPPEther2] >> 8;
	
	if (ia->ia_irq == IRQUNK)
		ia->ia_irq = eepro_irqmap[eaddr[EEPPW1] & EEPP_Int];

	if (ia->ia_irq >= sizeof(eepro_revirqmap))
		return 0;

	if ((sc->mappedirq = eepro_revirqmap[ia->ia_irq]) == -1)
		return 0;

	sc->hard_vers = eaddr[EEPW6] & EEPP_BoardRev;

	/* now lets reset the chip */
	
	outb(iobase + COMMAND_REG, RESET_CMD);
	delay(200);
	
       	ia->ia_iobase = iobase;
       	ia->ia_iosize = 16;
	return 1;		/* found */
}

void
iyattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct iy_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = iystart;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
					/* XXX todo: | IFF_MULTICAST */
	sc->sc_iobase = ia->ia_iobase;

	iyprobemem(sc);

	ifp->if_ioctl = iyioctl;
	ifp->if_watchdog = iywatchdog;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);
	printf(": address %s, chip rev. %d, %d kB SRAM\n",
	    ether_sprintf(sc->sc_arpcom.ac_enaddr),
	    sc->hard_vers, sc->sram/1024);
#if NBPFILTER > 0
	bpfattach(&sc->sc_arpcom.ac_if.if_bpf, ifp, DLT_EN10MB,
	    sizeof(struct ether_header));
#endif

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE, 
	    IPL_NET, iyintr, sc);
}

void
iystop(sc)
struct iy_softc *sc;
{
	int iobase;
#ifdef IYDEBUG
	u_int p, v;
#endif

	iobase = sc->sc_iobase;
	
	outb(iobase + COMMAND_REG, RCV_DISABLE_CMD);

	outb(iobase + INT_MASK_REG, ALL_INTS);
	outb(iobase + STATUS_REG, ALL_INTS);

	outb(iobase + COMMAND_REG, RESET_CMD);
	delay(200);
#ifdef IYDEBUG 
	printf("%s: dumping tx chain (st 0x%x end 0x%x last 0x%x)\n", 
		    sc->sc_dev.dv_xname, sc->tx_start, sc->tx_end, sc->tx_last);
	p = sc->tx_last;
	if (!p)
		p = sc->tx_start;
	do {
		outw(iobase + HOST_ADDR_REG, p);
		v = inw(iobase + MEM_PORT_REG);
		printf("0x%04x: %b ", p, v, "\020\006Ab\010Dn");
		v = inw(iobase + MEM_PORT_REG);
		printf("0x%b", v, "\020\6MAX_COL\7HRT_BEAT\010TX_DEF\011UND_RUN\012JERR\013LST_CRS\014LTCOL\016TX_OK\020COLL");
		p = inw(iobase + MEM_PORT_REG);
		printf(" 0x%04x", p);
		v = inw(iobase + MEM_PORT_REG);
		printf(" 0x%b\n", v, "\020\020Ch");
		
	} while (v & 0x8000);
#endif
	sc->tx_start = sc->tx_end = sc->rx_size;
	sc->tx_last = 0;
	sc->sc_arpcom.ac_if.if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);

	iymbufempty((void *)sc);
}

void
iyreset(sc)
struct iy_softc *sc;
{
	int s;
	s = splimp();
	iystop(sc);
	iyinit(sc);
	splx(s);
}

void
iyinit(sc)
struct iy_softc *sc;
{
	int i;
	unsigned temp;
	struct ifnet *ifp;
	int iobase;

	ifp = &sc->sc_arpcom.ac_if;
#ifdef IYDEBUG
	printf("ifp is %p\n", ifp);
#endif
	iobase = sc->sc_iobase;

	outb(iobase, BANK_SEL(2));

	temp = inb(iobase + EEPROM_REG);
	if (temp & 0x10)
		outb(iobase + EEPROM_REG, temp & ~0x10);
	
	for (i=0; i<6; ++i) {
		outb(iobase + I_ADD(i), sc->sc_arpcom.ac_enaddr[i]);
	}

	temp = inb(iobase + REG1);
	outb(iobase + REG1, temp | XMT_CHAIN_INT | XMT_CHAIN_ERRSTOP |
	    RCV_DISCARD_BAD);
	
	temp = inb(iobase + RECV_MODES_REG);
	outb(iobase + RECV_MODES_REG, temp | MATCH_BRDCST);
#ifdef IYDEBUG
	printf("%s: RECV_MODES were %b set to %b\n",
	    sc->sc_dev.dv_xname, 
	    temp, "\020\1PRMSC\2NOBRDST\3SEECRC\4LENGTH\5NOSaIns\6MultiIA",
	    temp|MATCH_BRDCST,
	    "\020\1PRMSC\2NOBRDST\3SEECRC\4LENGTH\5NOSaIns\6MultiIA");
#endif


	DELAY(500000); /* for the hardware to test for the connector */

	temp = inb(iobase + MEDIA_SELECT);
#ifdef IYDEBUG
	printf("%s: media select was 0x%b", sc->sc_dev.dv_xname,
	    temp, "\020\1LnkInDis\2PolCor\3TPE\4JabberDis\5NoAport\6BNC");
#endif
	temp = (temp & TEST_MODE_MASK) /* | BNC_BIT XXX*/;
	outb(iobase + MEDIA_SELECT, temp);
#ifdef IYDEBUG
	printf("changed to 0x%b\n", 
	    temp, "\020\1LnkInDis\2PolCor\3TPE\4JabberDis\5NoAport\6BNC");
#endif

	outb(iobase, BANK_SEL(1));

	temp = inb(iobase + INT_NO_REG);
	outb(iobase + INT_NO_REG, (temp & 0xf8) | sc->mappedirq);

#ifdef IYDEBUG
	printf("%s: int no was %b\n", sc->sc_dev.dv_xname,
	    temp, "\020\4bad_irq\010flash/boot present");
	temp = inb(iobase + INT_NO_REG);
	printf("%s: int no now 0x%02x\n", sc->sc_dev.dv_xname,
	    temp, "\020\4BAD IRQ\010flash/boot present");
#endif


	outb(iobase + RCV_LOWER_LIMIT_REG, 0);
	outb(iobase + RCV_UPPER_LIMIT_REG, (sc->rx_size - 2) >> 8);
	outb(iobase + XMT_LOWER_LIMIT_REG, sc->rx_size >> 8);
	outb(iobase + XMT_UPPER_LIMIT_REG, sc->sram >> 8);

	temp = inb(iobase + REG1);
#ifdef IYDEBUG
	printf("%s: HW access is %b\n", sc->sc_dev.dv_xname, 
	    temp, "\020\2WORD_WIDTH\010INT_ENABLE");
#endif
	outb(iobase + REG1, temp | INT_ENABLE); /* XXX what about WORD_WIDTH? */

#ifdef IYDEBUG
	temp = inb(iobase + REG1);
	printf("%s: HW access is %b\n", sc->sc_dev.dv_xname, 
	    temp, "\020\2WORD_WIDTH\010INT_ENABLE");
#endif

	outb(iobase, BANK_SEL(0));

	outb(iobase + INT_MASK_REG, ALL_INTS & ~(RX_BIT|TX_BIT));
	outb(iobase + STATUS_REG, ALL_INTS); /* clear ints */

	outw(iobase + RCV_START_LOW, 0);
	outw(iobase + RCV_STOP_LOW,  sc->rx_size - 2);
	sc->rx_start = 0;

	outb(iobase, SEL_RESET_CMD);
	DELAY(200);

	outw(iobase + XMT_ADDR_REG, sc->rx_size);

	sc->tx_start = sc->tx_end = sc->rx_size;
	sc->tx_last = 0;

	outb(iobase, RCV_ENABLE_CMD);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
}

void
iystart(ifp)
struct ifnet *ifp;
{
	struct iy_softc *sc;
	int iobase;

	struct mbuf *m0, *m;
	u_int len, pad, last, end;
	u_int llen, residual;
	int avail;
	caddr_t data;
	u_int16_t resval, stat;

#ifdef IYDEBUG
	printf("iystart called\n");
#endif
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
                return;

	sc = ifp->if_softc;
	iobase = sc->sc_iobase;

	while ((m0 = ifp->if_snd.ifq_head) != NULL) {
#ifdef IYDEBUG
		printf("%s: trying to write another packet to the hardware\n",
		    sc->sc_dev.dv_xname);
#endif

		/* We need to use m->m_pkthdr.len, so require the header */
		if ((m0->m_flags & M_PKTHDR) == 0)
			panic("iystart: no header mbuf");

		len = m0->m_pkthdr.len;
		pad = len & 1;

#ifdef IYDEBUG
		printf("%s: length is %d.\n", sc->sc_dev.dv_xname, len);
#endif
		if (len < ETHER_MIN_LEN) {
			pad = ETHER_MIN_LEN - len;
		}

        	if (len + pad > ETHER_MAX_LEN) {
        	        /* packet is obviously too large: toss it */
        	        ++ifp->if_oerrors;
        	        IF_DEQUEUE(&ifp->if_snd, m0);
        	        m_freem(m0);
			continue;
        	}

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0);
#endif

		avail = sc->tx_start - sc->tx_end;
		if (avail <= 0)
			avail += sc->tx_size;

#ifdef IYDEBUG
		printf("%s: avail is %d.\n", sc->sc_dev.dv_xname, avail);
#endif
		/* 
		 * we MUST RUN at splnet here  --- 
		 * XXX todo: or even turn off the boards ints ??? hm... 
		 */
	
       		/* See if there is room to put another packet in the buffer. */
	
		if ((len+pad+2*I595_XMT_HDRLEN) > avail) {
			printf("%s: len = %d, avail = %d, setting OACTIVE\n",
			    sc->sc_dev.dv_xname, len, avail);
			ifp->if_flags |= IFF_OACTIVE;
			return;
		}
	
		/* we know it fits in the hardware now, so dequeue it */
		IF_DEQUEUE(&ifp->if_snd, m0);
		
		last = sc->tx_end;
		end = last + pad + len + I595_XMT_HDRLEN; 
		
		if (end >= sc->sram) {
			if ((sc->sram - last) <= I595_XMT_HDRLEN) {
				/* keep header in one piece */
				last = sc->rx_size;
				end = last + pad + len + I595_XMT_HDRLEN;
			} else
				end -= sc->tx_size;
		}

		outw(iobase + HOST_ADDR_REG, last);
		outw(iobase + MEM_PORT_REG, XMT_CMD);
		outw(iobase + MEM_PORT_REG, 0);
		outw(iobase + MEM_PORT_REG, 0);
		outw(iobase + MEM_PORT_REG, len + pad);

		residual = resval = 0;

		while ((m = m0)!=0) {
			data = mtod(m, caddr_t);
			llen = m->m_len;
			if (residual) {
#ifdef IYDEBUG
				printf("%s: merging residual with next mbuf.\n",
				    sc->sc_dev.dv_xname);
#endif
				resval |= *data << 8;
				outw(iobase + MEM_PORT_REG, resval);
				--llen;
				++data;
			}
			if (llen > 1)
				outsw(iobase + MEM_PORT_REG, data, llen>>1);
			residual = llen & 1;
			if (residual) {
				resval = *(data + llen - 1);
#ifdef IYDEBUG
				printf("%s: got odd mbuf to send.\n",
				    sc->sc_dev.dv_xname);
#endif
			}

			MFREE(m, m0);
		}

		if (residual)
			outw(iobase + MEM_PORT_REG, resval);

		pad >>= 1;
		while (pad-- > 0)
			outw(iobase + MEM_PORT_REG, 0);
			
#ifdef IYDEBUG
		printf("%s: new last = 0x%x, end = 0x%x.\n",
		    sc->sc_dev.dv_xname, last, end);
		printf("%s: old start = 0x%x, end = 0x%x, last = 0x%x\n",
		    sc->sc_dev.dv_xname, sc->tx_start, sc->tx_end, sc->tx_last);
#endif

		if (sc->tx_start != sc->tx_end) {
			outw(iobase + HOST_ADDR_REG, sc->tx_last + XMT_COUNT);
			stat = inw(iobase + MEM_PORT_REG);

			outw(iobase + HOST_ADDR_REG, sc->tx_last + XMT_CHAIN);
			outw(iobase + MEM_PORT_REG, last);
			outw(iobase + MEM_PORT_REG, stat | CHAIN);
#ifdef IYDEBUG
			printf("%s: setting 0x%x to 0x%x\n",
			    sc->sc_dev.dv_xname, sc->tx_last + XMT_COUNT, 
			    stat | CHAIN);
#endif
		}
		stat = inw(iobase + MEM_PORT_REG); /* dummy read */

		/* XXX todo: enable ints here if disabled */
		
		++ifp->if_opackets;

		if (sc->tx_start == sc->tx_end) {
			outw(iobase + XMT_ADDR_REG, last);
			outb(iobase, XMT_CMD);
			sc->tx_start = last;
#ifdef IYDEBUG
			printf("%s: writing 0x%x to XAR and giving XCMD\n",
			    sc->sc_dev.dv_xname, last);
#endif
		} else {
			outb(iobase, RESUME_XMT_CMD);
#ifdef IYDEBUG
			printf("%s: giving RESUME_XCMD\n",
			    sc->sc_dev.dv_xname);
#endif
		}
		sc->tx_last = last;
		sc->tx_end = end;
	}
}


static __inline void
eepromwritebit(eio, what) 
	int eio, what;
{
	outb(eio, what);
	delay(1);
	outb(eio, what|EESK);
	delay(1);
	outb(eio, what);
	delay(1);
}

static __inline int
eepromreadbit(eio) 
	int eio;
{
	int b; 

	outb(eio, EECS|EESK); 
	delay(1);
	b = inb(eio);
	outb(eio, EECS);
	delay(1);

	return ((b & EEDO) != 0);
}

static u_int16_t
eepromread(io, offset)
	int io, offset;
{
	volatile int i;
	volatile int j;
	volatile u_int16_t readval;
	int eio = io+EEPROM_REG;

	outb(io, BANK_SEL(2));
	delay(1);
	outb(io, EECS);
	delay(1);
	
	eepromwritebit(eio, EECS|EEDI);
	eepromwritebit(eio, EECS|EEDI);
	eepromwritebit(eio, EECS);
	
	for (j=5; j>=0; --j) {
		if ((offset>>j) & 1) 
			eepromwritebit(eio, EECS|EEDI);
		else
			eepromwritebit(eio, EECS);
	}

	for (readval=0, i=0; i<16; ++i) {
		readval<<=1;
		readval |= eepromreadbit(eio);
	}

	outb(eio, 0|EESK);
	delay(1);
	outb(eio, 0);

	outb(eio, BANK_SEL(0));

	return readval;
}

/*
 * Device timeout/watchdog routine.  Entered if the device neglects to generate
 * an interrupt after a transmit has been started on it.
 */
void
iywatchdog(ifp)
	struct ifnet *ifp;
{
	struct iy_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;
	iyreset(sc);
}

/*
 * What to do upon receipt of an interrupt.
 */
int
iyintr(arg)
	void *arg;
{
	struct iy_softc *sc = arg;
	int iobase;
	register u_short status;

	iobase = sc->sc_iobase;
	status = inb(iobase + STATUS_REG);
#ifdef IYDEBUG
	if (status & ALL_INTS) {
		printf("%s: got interupt %b", sc->sc_dev.dv_xname, status,
		    "\020\1RX_STP\2RX\3TX\4EXEC");
		if (status & EXEC_INT)
			printf(" event %b\n", inb(iobase),
			    "\020\6ABORT");
		else
			printf("\n");
	}
#endif
	if ((status & (RX_INT | TX_INT) == 0))
		return 0;

	if (status & RX_INT) {
		iy_intr_rx(sc);
		outb(iobase + STATUS_REG, RX_INT);
	} else if (status & TX_INT) {
		iy_intr_tx(sc);
		outb(iobase + STATUS_REG, TX_INT);
	}
	return 1;
}

void
iyget(sc, iobase, rxlen)
struct iy_softc *sc;
int iobase, rxlen;
{
	struct mbuf *m, *top, **mp;
	struct ether_header *eh;
	struct ifnet *ifp;
	int len;

	ifp = &sc->sc_arpcom.ac_if;

	m = sc->mb[sc->next_mb];
	sc->mb[sc->next_mb] = 0;
	if (m == 0) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0)
			goto dropped;
	} else {
		if (sc->last_mb == sc->next_mb)
			timeout(iymbuffill, sc, 1);
		sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
		m->m_data = m->m_pktdat;
		m->m_flags = M_PKTHDR;
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = rxlen;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (rxlen > 0) {
		if (top) {
			m = sc->mb[sc->next_mb];
			sc->mb[sc->next_mb] = 0;
			if (m == 0) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == 0) {
					m_freem(top);
					goto dropped;
				}
			} else {
				sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
			}
			len = MLEN;
		}
		if (rxlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		len = min(rxlen, len);
		if (len > 1) {
			len &= ~1;
			insw(iobase + MEM_PORT_REG, mtod(m, caddr_t), len/2);
		} else {
#ifdef IYDEBUG
			printf("%s: received odd mbuf\n", sc->sc_dev.dv_xname);
#endif
			*(mtod(m, caddr_t)) = inw(iobase + MEM_PORT_REG);
		}
		m->m_len = len;
		rxlen -= len;
		*mp = m;
		mp = &m->m_next;
	}
	/* XXX receive the top here */	
	++ifp->if_ipackets;
	
	eh = mtod(top, struct ether_header *);

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, top);
#endif
	m_adj(top, sizeof(struct ether_header));
	ether_input(ifp, eh, top);
	return;

dropped:
	++ifp->if_ierrors;
	return;
}
void
iy_intr_rx(sc)
struct iy_softc *sc;
{
	struct ifnet *ifp;
	int iobase;
	u_int rxadrs, rxevnt, rxstatus, rxnext, rxlen;

	iobase = sc->sc_iobase;
	ifp = &sc->sc_arpcom.ac_if;

	rxadrs = sc->rx_start;
	outw(iobase + HOST_ADDR_REG, rxadrs);
	rxevnt = inw(iobase + MEM_PORT_REG);
	rxnext = 0;
	
	while (rxevnt == RCV_DONE) {
		rxstatus = inw(iobase + MEM_PORT_REG);
		rxnext = inw(iobase + MEM_PORT_REG);
		rxlen = inw(iobase + MEM_PORT_REG);
#ifdef IYDEBUG
		printf("%s: pck at 0x%04x stat %b next 0x%x len 0x%x\n",
		    sc->sc_dev.dv_xname, rxadrs, rxstatus,
		    "\020\1RCLD\2IA_MCH\010SHORT\011OVRN\013ALGERR"
		    "\014CRCERR\015LENERR\016RCVOK\020TYP",
		    rxnext, rxlen);
#endif
		iyget(sc, iobase, rxlen);

		/* move stop address */
		outw(iobase + RCV_STOP_LOW,
			    rxnext == 0 ? sc->rx_size - 2 : rxnext - 2);

		outw(iobase + HOST_ADDR_REG, rxnext);
		rxadrs = rxnext;
		rxevnt = inw(iobase + MEM_PORT_REG);
	}
	sc->rx_start = rxnext;
}

void
iy_intr_tx(sc)
struct iy_softc *sc;
{
	int iobase;
	struct ifnet *ifp;
	u_int txstatus, txstat2, txlen, txnext;

	ifp = &sc->sc_arpcom.ac_if;
	iobase = sc->sc_iobase;
	
	while (sc->tx_start != sc->tx_end) {
		outw(iobase + HOST_ADDR_REG, sc->tx_start);
		txstatus = inw(iobase + MEM_PORT_REG);
		if ((txstatus & (TX_DONE|CMD_MASK)) != (TX_DONE|XMT_CMD))
			break;

		txstat2 = inw(iobase + MEM_PORT_REG);
		txnext = inw(iobase + MEM_PORT_REG);
		txlen = inw(iobase + MEM_PORT_REG);
#ifdef IYDEBUG
		printf("txstat 0x%x stat2 0x%b next 0x%x len 0x%x\n",
		    txstatus, txstat2, "\020\6MAX_COL\7HRT_BEAT\010TX_DEF"
		    "\011UND_RUN\012JERR\013LST_CRS\014LTCOL\016TX_OK\020COLL",
			txnext, txlen);
#endif
		if (txlen & CHAIN)
			sc->tx_start = txnext;
		else
			sc->tx_start = sc->tx_end;
		ifp->if_flags &= ~IFF_OACTIVE;
		
		if ((txstat2 & 0x2000) == 0)
			++ifp->if_oerrors;
		if (txstat2 & 0x000f)
			ifp->if_oerrors += txstat2 & 0x000f;
	}
	ifp->if_flags &= ~IFF_OACTIVE;
}

#if 0
/*
 * Compare two Ether/802 addresses for equality, inlined and unrolled for
 * speed.  I'd love to have an inline assembler version of this...
 */
static inline int
ether_equal(one, two)
	u_char *one, *two;
{

	if (one[0] != two[0] || one[1] != two[1] || one[2] != two[2] ||
	    one[3] != two[3] || one[4] != two[4] || one[5] != two[5])
		return 0;
	return 1;
}

/*
 * Check for a valid address.  to_bpf is filled in with one of the following:
 *   0 -> BPF doesn't get this packet
 *   1 -> BPF does get this packet
 *   2 -> BPF does get this packet, but we don't
 * Return value is true if the packet is for us, and false otherwise.
 *
 * This routine is a mess, but it's also critical that it be as fast
 * as possible.  It could be made cleaner if we can assume that the
 * only client which will fiddle with IFF_PROMISC is BPF.  This is
 * probably a good assumption, but we do not make it here.  (Yet.)
 */
static inline int
check_eh(sc, eh, to_bpf)
	struct iy_softc *sc;
	struct ether_header *eh;
	int *to_bpf;
{
	int i;

	switch (sc->promisc) {
	case IFF_ALLMULTI:
		/*
		 * Receiving all multicasts, but no unicasts except those
		 * destined for us.
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.iy_bpf != 0); /* BPF gets this packet if anybody cares */
#endif
		if (eh->ether_dhost[0] & 1)
			return 1;
		if (ether_equal(eh->ether_dhost, sc->sc_arpcom.ac_enaddr))
			return 1;
		return 0;

	case IFF_PROMISC:
		/*
		 * Receiving all packets.  These need to be passed on to BPF.
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.iy_bpf != 0);
#endif
		/* If for us, accept and hand up to BPF */
		if (ether_equal(eh->ether_dhost, sc->sc_arpcom.ac_enaddr))
			return 1;

#if NBPFILTER > 0
		if (*to_bpf)
			*to_bpf = 2; /* we don't need to see it */
#endif

		/*
		 * Not a multicast, so BPF wants to see it but we don't.
		 */
		if (!(eh->ether_dhost[0] & 1))
			return 1;

		/*
		 * If it's one of our multicast groups, accept it and pass it
		 * up.
		 */
		for (i = 0; i < sc->mcast_count; i++) {
			if (ether_equal(eh->ether_dhost, (u_char *)&sc->mcast_addrs[i])) {
#if NBPFILTER > 0
				if (*to_bpf)
					*to_bpf = 1;
#endif
				return 1;
			}
		}
		return 1;

	case IFF_ALLMULTI | IFF_PROMISC:
		/*
		 * Acting as a multicast router, and BPF running at the same
		 * time.  Whew!  (Hope this is a fast machine...)
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.iy_bpf != 0);
#endif
		/* We want to see multicasts. */
		if (eh->ether_dhost[0] & 1)
			return 1;

		/* We want to see our own packets */
		if (ether_equal(eh->ether_dhost, sc->sc_arpcom.ac_enaddr))
			return 1;

		/* Anything else goes to BPF but nothing else. */
#if NBPFILTER > 0
		if (*to_bpf)
			*to_bpf = 2;
#endif
		return 1;

	case 0:
		/*
		 * Only accept unicast packets destined for us, or multicasts
		 * for groups that we belong to.  For now, we assume that the
		 * '586 will only return packets that we asked it for.  This
		 * isn't strictly true (it uses hashing for the multicast
		 * filter), but it will do in this case, and we want to get out
		 * of here as quickly as possible.
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.iy_bpf != 0);
#endif
		return 1;
	}

#ifdef DIAGNOSTIC
	panic("check_eh: impossible");
#endif
}
#endif

int
iyioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct iy_softc *sc;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int s, error = 0;

	sc = ifp->if_softc;
	ifa = (struct ifaddr *)data;
	ifr = (struct ifreq *)data;

#ifdef IYDEBUG
	printf("iyioctl called with ifp 0x%p (%s) cmd 0x%x data 0x%p\n", 
	    ifp, ifp->if_xname, cmd, data);
#endif

	s = splimp();

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			iyinit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
#ifdef NS
		/* XXX - This code is probably wrong. */
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)(sc->sc_arpcom.ac_enaddr);
			else
				bcopy(ina->x_host.c_host,
				    sc->sc_arpcom.ac_enaddr,
				    sizeof(sc->sc_arpcom.ac_enaddr));
			/* Set new address. */
			iyinit(sc);
			break;
		    }
#endif /* NS */
		default:
			iyinit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		sc->promisc = ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI);
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			iystop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			iyinit(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			iystop(sc);
			iyinit(sc);
		}
#ifdef IYDEBUGX
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_debug = IFY_ALL;
		else
			sc->sc_debug = 0;
#endif
		break;

#if 0 /* XXX */
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom):
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			iy_mc_reset(sc); /* XXX */
			error = 0;
		}
		break;
#endif
	default:
		error = EINVAL;
	}
	splx(s);
	return error;
}

#if 0
static void
iy_mc_reset(sc)
	struct iy_softc *sc;
{
	struct ether_multi *enm;
	struct ether_multistep step;

	/*
	 * Step through the list of addresses.
	 */
	sc->mcast_count = 0;
	ETHER_FIRST_MULTI(step, &sc->sc_arpcom, enm);
	while (enm) {
		if (sc->mcast_count >= MAXMCAST ||
		    bcmp(enm->enm_addrlo, enm->enm_addrhi, 6) != 0) {
			sc->sc_arpcom.ac_if.if_flags |= IFF_ALLMULTI;
			iyioctl(&sc->sc_arpcom.ac_if, SIOCSIFFLAGS, (void *)0);
			goto setflag;
		}

		bcopy(enm->enm_addrlo, &sc->mcast_addrs[sc->mcast_count], 6);
		sc->mcast_count++;
		ETHER_NEXT_MULTI(step, enm);
	}
	setflag:
	sc->want_mcsetup = 1;
}

#ifdef IYDEBUG
void
print_rbd(rbd)
	volatile struct ie_recv_buf_desc *rbd;
{

	printf("RBD at %08lx:\nactual %04x, next %04x, buffer %08x\n"
	    "length %04x, mbz %04x\n", (u_long)rbd, rbd->ie_rbd_actual,
	    rbd->ie_rbd_next, rbd->ie_rbd_buffer, rbd->ie_rbd_length,
	    rbd->mbz);
}
#endif
#endif

void
iymbuffill(arg)
	void *arg;
{
	struct iy_softc *sc = (struct iy_softc *)arg;
	int s, i;

	s = splimp();
	i = sc->last_mb;
	do {
		if (sc->mb[i] == NULL)
			MGET(sc->mb[i], M_DONTWAIT, MT_DATA);
		if (sc->mb[i] == NULL)
			break;
		i = (i + 1) % MAX_MBS;
	} while (i != sc->next_mb);
	sc->last_mb = i;
	/* If the queue was not filled, try again. */
	if (sc->last_mb != sc->next_mb)
		timeout(iymbuffill, sc, 1);
	splx(s); 
}


void
iymbufempty(arg)
	void *arg;
{
	struct iy_softc *sc = (struct iy_softc *)arg;
	int s, i;

	s = splimp();
	for (i = 0; i<MAX_MBS; i++) {
		if (sc->mb[i]) {
			m_freem(sc->mb[i]);
			sc->mb[i] = NULL;
		}
	}
	sc->last_mb = sc->next_mb = 0;
	untimeout(iymbuffill, sc);
	splx(s);
}

void
iyprobemem(sc)
	struct iy_softc *sc;
{
	int iobase;
	int testing;

	iobase = sc->sc_iobase;

	outw(iobase + HOST_ADDR_REG, 4096-2);
	outw(iobase + MEM_PORT_REG, 0);

	for (testing=65536; testing >= 4096; testing >>= 1) {
		outw(iobase + HOST_ADDR_REG, testing-2);
		outw(iobase + MEM_PORT_REG, 0xdead);
		outw(iobase + HOST_ADDR_REG, testing-2);
		if (inw(iobase + MEM_PORT_REG) != 0xdead) {
#ifdef IYMEMDEBUG
			printf("%s: Didn't keep 0xdead at 0x%x\n",
			    sc->sc_dev.dv_xname, testing-2);
#endif
			continue;
		}

		outw(iobase + HOST_ADDR_REG, testing-2);
		outw(iobase + MEM_PORT_REG, 0xbeef);
		outw(iobase + HOST_ADDR_REG, testing-2);
		if (inw(iobase + MEM_PORT_REG) != 0xbeef) {
#ifdef IYMEMDEBUG
			printf("%s: Didn't keep 0xbeef at 0x%x\n",
			    sc->sc_dev.dv_xname, testing-2);
#endif
			continue;
		}

		outw(iobase + HOST_ADDR_REG, 0);
		outw(iobase + MEM_PORT_REG, 0);
		outw(iobase + HOST_ADDR_REG, testing >> 1);
		outw(iobase + MEM_PORT_REG, testing >> 1);
		outw(iobase + HOST_ADDR_REG, 0);
		if (inw(iobase + MEM_PORT_REG) == (testing >> 1)) {
#ifdef IYMEMDEBUG
			printf("%s: 0x%x alias of 0x0\n",
			    sc->sc_dev.dv_xname, testing >> 1);
#endif
			continue;
		}

		break;
	}

	sc->sram = testing;

	switch(testing) {
		case 65536:
			/* 4 NFS packets + overhead RX, 2 NFS + overhead TX  */
			sc->rx_size = 44*1024;
			break;

		case 32768:
			/* 2 NFS packets + overhead RX, 1 NFS + overhead TX  */
			sc->rx_size = 22*1024;
			break;

		case 16384:
			/* 1 NFS packet + overhead RX, 4 big packets TX */
			sc->rx_size = 10*1024;
			break;
		default:	
			sc->rx_size = testing/2;
			break;
	}
	sc->tx_size = testing - sc->rx_size;
}
