/*	$OpenBSD: if_eg.c,v 1.24 2003/01/27 19:09:12 jason Exp $	*/
/*	$NetBSD: if_eg.c,v 1.26 1996/05/12 23:52:27 mycroft Exp $	*/

/*
 * Copyright (c) 1993 Dean Huxley <dean@fsa.ca>
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
 *      This product includes software developed by Dean Huxley.
 * 4. The name of Dean Huxley may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Support for 3Com 3c505 Etherlink+ card.
 */

/* To do:
 * - multicast
 * - promiscuous
 */
#include "bpfilter.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/pio.h>

#include <dev/isa/isavar.h>
#include <dev/isa/if_egreg.h>
#include <dev/isa/elink.h>

/* for debugging convenience */
#ifdef EGDEBUG
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#define EG_INLEN  	10
#define EG_BUFLEN	0x0670

/*
 * Ethernet software status per interface.
 */
struct eg_softc {
	struct device sc_dev;
	void *sc_ih;
	bus_space_tag_t sc_bst;
	bus_space_handle_t sc_bsh;
	struct arpcom sc_arpcom;	/* Ethernet common part */
	u_char  eg_rom_major;		/* Cards ROM version (major number) */ 
	u_char  eg_rom_minor;		/* Cards ROM version (minor number) */ 
	short	eg_ram;			/* Amount of RAM on the card */
	u_char	eg_pcb[64];		/* Primary Command Block buffer */
	u_char  eg_incount;		/* Number of buffers currently used */
	u_char  *eg_inbuf;		/* Incoming packet buffer */
	u_char	*eg_outbuf;		/* Outgoing packet buffer */
};

int egprobe(struct device *, void *, void *);
void egattach(struct device *, struct device *, void *);

struct cfattach eg_ca = {
	sizeof(struct eg_softc), egprobe, egattach
};

struct cfdriver eg_cd = {
	NULL, "eg", DV_IFNET
};

int egintr(void *);
void eginit(struct eg_softc *);
int egioctl(struct ifnet *, u_long, caddr_t);
void egrecv(struct eg_softc *);
void egstart(struct ifnet *);
void egwatchdog(struct ifnet *);
void egreset(struct eg_softc *);
void egread(struct eg_softc *, caddr_t, int);
struct mbuf *egget(struct eg_softc *, caddr_t, int);
void egstop(struct eg_softc *);

static __inline void egprintpcb(struct eg_softc *);
static __inline void egprintstat(u_char);
static int egoutPCB(struct eg_softc *, u_char);
static int egreadPCBstat(struct eg_softc *, u_char);
static int egreadPCBready(struct eg_softc *);
static int egwritePCB(struct eg_softc *);
static int egreadPCB(struct eg_softc *);

/*
 * Support stuff
 */
	
static __inline void
egprintpcb(sc)
	struct eg_softc *sc;
{
	int i;
	
	for (i = 0; i < sc->eg_pcb[1] + 2; i++)
		dprintf(("pcb[%2d] = %x\n", i, sc->eg_pcb[i]));
}


static __inline void
egprintstat(b)
	u_char b;
{
	dprintf(("%s %s %s %s %s %s %s\n", 
	    (b & EG_STAT_HCRE)?"HCRE":"",
	    (b & EG_STAT_ACRF)?"ACRF":"",
	    (b & EG_STAT_DIR )?"DIR ":"",
	    (b & EG_STAT_DONE)?"DONE":"",
	    (b & EG_STAT_ASF3)?"ASF3":"",
	    (b & EG_STAT_ASF2)?"ASF2":"",
	    (b & EG_STAT_ASF1)?"ASF1":""));
}

static int
egoutPCB(sc, b)
	struct eg_softc *sc;
	u_char b;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	int i;

	for (i=0; i < 4000; i++) {
		if (bus_space_read_1(bst, bsh, EG_STATUS) & EG_STAT_HCRE) {
			bus_space_write_1(bst, bsh, EG_COMMAND, b);
			return 0;
		}
		delay(10);
	}
	dprintf(("egoutPCB failed\n"));
	return (1);
}
	
static int
egreadPCBstat(sc, statb)
	struct eg_softc *sc;
	u_char statb;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	int i;

	for (i=0; i < 5000; i++) {
		if ((bus_space_read_1(bst, bsh, EG_STATUS) & EG_PCB_STAT) !=
		    EG_PCB_NULL) 
			break;
		delay(10);
	}
	if ((bus_space_read_1(bst, bsh, EG_STATUS) & EG_PCB_STAT) == statb) 
		return (0);
	return (1);
}

static int
egreadPCBready(sc)
	struct eg_softc *sc;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	int i;

	for (i=0; i < 10000; i++) {
		if (bus_space_read_1(bst, bsh, EG_STATUS) & EG_STAT_ACRF)
			return (0);
		delay(5);
	}
	dprintf(("PCB read not ready\n"));
	return (1);
}
	
static int
egwritePCB(sc)
	struct eg_softc *sc;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	int i;
	u_char len;

	bus_space_write_1(bst, bsh, EG_CONTROL,
	    (bus_space_read_1(bst, bsh, EG_CONTROL) & ~EG_PCB_STAT) |
	    EG_PCB_NULL);

	len = sc->eg_pcb[1] + 2;
	for (i = 0; i < len; i++)
		egoutPCB(sc, sc->eg_pcb[i]);
	
	for (i=0; i < 4000; i++) {
		if (bus_space_read_1(bst, bsh, EG_STATUS) & EG_STAT_HCRE)
			break;
		delay(10);
	}

	bus_space_write_1(bst, bsh, EG_CONTROL,
	    (bus_space_read_1(bst, bsh, EG_CONTROL) & ~EG_PCB_STAT) |
	    EG_PCB_DONE);

	egoutPCB(sc, len);

	if (egreadPCBstat(sc, EG_PCB_ACCEPT))
		return (1);
	return (0);
}	
	
static int
egreadPCB(sc)
	struct eg_softc *sc;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	int i;
	u_char b;
	
	bus_space_write_1(bst, bsh, EG_CONTROL,
	    (bus_space_read_1(bst, bsh, EG_CONTROL) & ~EG_PCB_STAT) |
	    EG_PCB_NULL);

	bzero(sc->eg_pcb, sizeof(sc->eg_pcb));

	if (egreadPCBready(sc))
		return (1);

	sc->eg_pcb[0] = bus_space_read_1(bst, bsh, EG_COMMAND);
	
	if (egreadPCBready(sc))
		return (1);

	sc->eg_pcb[1] = bus_space_read_1(bst, bsh, EG_COMMAND);

	if (sc->eg_pcb[1] > 62) {
		dprintf(("len %d too large\n", sc->eg_pcb[1]));
		return (1);
	}
	
	for (i = 0; i < sc->eg_pcb[1]; i++) {
		if (egreadPCBready(sc))
			return (1);
		sc->eg_pcb[2+i] = bus_space_read_1(bst, bsh, EG_COMMAND);
	}
	if (egreadPCBready(sc))
		return (1);
	if (egreadPCBstat(sc, EG_PCB_DONE))
		return (1);
	if ((b = bus_space_read_1(bst, bsh, EG_COMMAND)) != sc->eg_pcb[1] + 2)
	    {
		dprintf(("%d != %d\n", b, sc->eg_pcb[1] + 2));
		return (1);
	}

	bus_space_write_1(bst, bsh, EG_CONTROL,
	    (bus_space_read_1(bst, bsh, EG_CONTROL) & ~EG_PCB_STAT) |
	    EG_PCB_ACCEPT);

	return (0);
}	

/*
 * Real stuff
 */

int
egprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct eg_softc *sc = match;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t bst = sc->sc_bst = ia->ia_iot;
	bus_space_handle_t bsh;
	int i;

	if ((ia->ia_iobase & ~0x07f0) != 0) {
		dprintf(("Weird iobase %x\n", ia->ia_iobase));
		return (0);
	}
	
	if (bus_space_map(bst, ia->ia_iobase, EG_IO_PORTS, 0, &bsh)) {
		dprintf(("%s: can't map I/O space\n", sc->sc_dev.dv_xname));
		return (0);
	}
	sc->sc_bsh = bsh;

	/* hard reset card */
	bus_space_write_1(bst, bsh, EG_CONTROL, EG_CTL_RESET); 
	bus_space_write_1(bst, bsh, EG_CONTROL, 0);
	for (i = 0; i < 5000; i++) {
		delay(1000);
		if ((bus_space_read_1(bst, bsh, EG_STATUS) & EG_PCB_STAT) ==
		    EG_PCB_NULL) 
			break;
	}
	if ((bus_space_read_1(bst, bsh, EG_STATUS) & EG_PCB_STAT) !=
	    EG_PCB_NULL) {
		dprintf(("eg: Reset failed\n"));
		goto lose;
	}
	sc->eg_pcb[0] = EG_CMD_GETINFO; /* Get Adapter Info */
	sc->eg_pcb[1] = 0;
	if (egwritePCB(sc) != 0)
		goto lose;
	
	if (egreadPCB(sc) != 0) {
		egprintpcb(sc);
		goto lose;
	}

	if (sc->eg_pcb[0] != EG_RSP_GETINFO || /* Get Adapter Info Response */
	    sc->eg_pcb[1] != 0x0a) {
		egprintpcb(sc);
		goto lose;
	}
	sc->eg_rom_major = sc->eg_pcb[3];
	sc->eg_rom_minor = sc->eg_pcb[2];
	sc->eg_ram = sc->eg_pcb[6] | (sc->eg_pcb[7] << 8);
	
	ia->ia_iosize = 0x08;
	ia->ia_msize = 0;
	bus_space_unmap(bst, bsh, EG_IO_PORTS);
	return (1);

lose:
	sc->sc_bst = sc->sc_bsh = 0;
	bus_space_unmap(bst, bsh, EG_IO_PORTS);
	return (0);
}

void
egattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct eg_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t bst = sc->sc_bst = ia->ia_iot;
	bus_space_handle_t bsh;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	
	if (bus_space_map(bst, ia->ia_iobase, EG_IO_PORTS, 0, &bsh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}
	sc->sc_bsh = bsh;

	egstop(sc);

	sc->eg_pcb[0] = EG_CMD_GETEADDR; /* Get Station address */
	sc->eg_pcb[1] = 0;
	if (egwritePCB(sc) != 0) {
		dprintf(("write error\n"));
		return;
	}	
	if (egreadPCB(sc) != 0) {
		dprintf(("read error\n"));
		egprintpcb(sc);
		return;
	}

	/* check Get station address response */
	if (sc->eg_pcb[0] != EG_RSP_GETEADDR || sc->eg_pcb[1] != 0x06) { 
		dprintf(("parse error\n"));
		egprintpcb(sc);
		return;
	}
	bcopy(&sc->eg_pcb[2], sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);

	printf(": ROM v%d.%02d %dk address %s\n",
	    sc->eg_rom_major, sc->eg_rom_minor, sc->eg_ram,
	    ether_sprintf(sc->sc_arpcom.ac_enaddr));

	sc->eg_pcb[0] = EG_CMD_SETEADDR; /* Set station address */
	if (egwritePCB(sc) != 0) {
		dprintf(("write error2\n"));
		return;
	}
	if (egreadPCB(sc) != 0) {
		dprintf(("read error2\n"));
		egprintpcb(sc);
		return;
	}
	if (sc->eg_pcb[0] != EG_RSP_SETEADDR || sc->eg_pcb[1] != 0x02 ||
	    sc->eg_pcb[2] != 0 || sc->eg_pcb[3] != 0) {
		dprintf(("parse error2\n"));
		egprintpcb(sc);
		return;
	}

	/* Initialize ifnet structure. */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = egstart;
	ifp->if_ioctl = egioctl;
	ifp->if_watchdog = egwatchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	IFQ_SET_READY(&ifp->if_snd);
	
	/* Now we can attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);
	
	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, egintr, sc, sc->sc_dev.dv_xname);
}

void
eginit(sc)
	register struct eg_softc *sc;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	register struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	/* soft reset the board */
	bus_space_write_1(bst, bsh, EG_CONTROL, EG_CTL_FLSH);
	delay(100);
	bus_space_write_1(bst, bsh, EG_CONTROL, EG_CTL_ATTN);
	delay(100);
	bus_space_write_1(bst, bsh, EG_CONTROL, 0);
	delay(200);

	sc->eg_pcb[0] = EG_CMD_CONFIG82586; /* Configure 82586 */
	sc->eg_pcb[1] = 2;
	sc->eg_pcb[2] = 3; /* receive broadcast & multicast */
	sc->eg_pcb[3] = 0;
	if (egwritePCB(sc) != 0)
		dprintf(("write error3\n"));

	if (egreadPCB(sc) != 0) {
		dprintf(("read error\n"));
		egprintpcb(sc);
	} else if (sc->eg_pcb[2] != 0 || sc->eg_pcb[3] != 0)
		printf("%s: configure card command failed\n",
		    sc->sc_dev.dv_xname);

	if (sc->eg_inbuf == 0)
		sc->eg_inbuf = malloc(EG_BUFLEN, M_TEMP, M_NOWAIT);
	sc->eg_incount = 0;

	if (sc->eg_outbuf == 0)
		sc->eg_outbuf = malloc(EG_BUFLEN, M_TEMP, M_NOWAIT);

	bus_space_write_1(bst, bsh, EG_CONTROL, EG_CTL_CMDE);

	sc->eg_incount = 0;
	egrecv(sc);

	/* Interface is now `running', with no output active. */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Attempt to start output, if any. */
	egstart(ifp);
}

void
egrecv(sc)
	struct eg_softc *sc;
{

	while (sc->eg_incount < EG_INLEN) {
		sc->eg_pcb[0] = EG_CMD_RECVPACKET;
		sc->eg_pcb[1] = 0x08;
		sc->eg_pcb[2] = 0; /* address not used.. we send zero */
		sc->eg_pcb[3] = 0;
		sc->eg_pcb[4] = 0;
		sc->eg_pcb[5] = 0;
		sc->eg_pcb[6] = EG_BUFLEN & 0xff; /* our buffer size */
		sc->eg_pcb[7] = (EG_BUFLEN >> 8) & 0xff;
		sc->eg_pcb[8] = 0; /* timeout, 0 == none */
		sc->eg_pcb[9] = 0;
		if (egwritePCB(sc) != 0)
			break;
		sc->eg_incount++;
	}
}

void
egstart(ifp)
	struct ifnet *ifp;
{
	struct eg_softc *sc = ifp->if_softc;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	struct mbuf *m0, *m;
	caddr_t buffer;
	int len;
	u_short *ptr;

	/* Don't transmit if interface is busy or not running */
	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

loop:
	/* Dequeue the next datagram. */
	IFQ_DEQUEUE(&ifp->if_snd, m0);
	if (m0 == 0)
		return;
	
	ifp->if_flags |= IFF_OACTIVE;

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m0->m_flags & M_PKTHDR) == 0)
		panic("egstart: no header mbuf");
	len = max(m0->m_pkthdr.len, ETHER_MIN_LEN);

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m0);
#endif

	sc->eg_pcb[0] = EG_CMD_SENDPACKET;
	sc->eg_pcb[1] = 0x06;
	sc->eg_pcb[2] = 0; /* address not used, we send zero */
	sc->eg_pcb[3] = 0;
	sc->eg_pcb[4] = 0;
	sc->eg_pcb[5] = 0;
	sc->eg_pcb[6] = len; /* length of packet */
	sc->eg_pcb[7] = len >> 8;
	if (egwritePCB(sc) != 0) {
		dprintf(("egwritePCB in egstart failed\n"));
		ifp->if_oerrors++;
		ifp->if_flags &= ~IFF_OACTIVE;
		m_freem(m0);
		goto loop;
	}

	buffer = sc->eg_outbuf;
	for (m = m0; m != 0; m = m->m_next) {
		bcopy(mtod(m, caddr_t), buffer, m->m_len);
		buffer += m->m_len;
	}
	if (len > m0->m_pkthdr.len)
		bzero(buffer, len - m0->m_pkthdr.len);

	/* set direction bit: host -> adapter */
	bus_space_write_1(bst, bsh, EG_CONTROL,
	    bus_space_read_1(bst, bsh, EG_CONTROL) & ~EG_CTL_DIR); 
	
	for (ptr = (u_short *)sc->eg_outbuf; len > 0; len -= 2) {
		bus_space_write_2(bst, bsh, EG_DATA, *ptr++);
		while (!(bus_space_read_1(bst, bsh, EG_STATUS) & EG_STAT_HRDY))
			; /* XXX need timeout here */
	}
	
	m_freem(m0);
}

int
egintr(arg)
	void *arg;
{
	struct eg_softc *sc = arg;
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	int ret = 0;
	int i, len;
	u_short *ptr;

	while (bus_space_read_1(bst, bsh, EG_STATUS) & EG_STAT_ACRF) {
		ret = 1;
		egreadPCB(sc);
		switch (sc->eg_pcb[0]) {
		case EG_RSP_RECVPACKET:
			len = sc->eg_pcb[6] | (sc->eg_pcb[7] << 8);
	
			/* Set direction bit : Adapter -> host */
			bus_space_write_1(bst, bsh, EG_CONTROL,
			    bus_space_read_1(bst, bsh, EG_CONTROL) |
			    EG_CTL_DIR); 

			for (ptr = (u_short *)sc->eg_inbuf; len > 0; len -= 2)
			    {
				while (!(bus_space_read_1(bst, bsh,
				    EG_STATUS) & EG_STAT_HRDY))
					;
				*ptr++ = bus_space_read_2(bst, bsh, EG_DATA);
			}

			len = sc->eg_pcb[8] | (sc->eg_pcb[9] << 8);
			egread(sc, sc->eg_inbuf, len);

			sc->eg_incount--;
			egrecv(sc);
			break;

		case EG_RSP_SENDPACKET:
			if (sc->eg_pcb[6] || sc->eg_pcb[7]) {
				dprintf(("packet dropped\n"));
				sc->sc_arpcom.ac_if.if_oerrors++;
			} else
				sc->sc_arpcom.ac_if.if_opackets++;
			sc->sc_arpcom.ac_if.if_collisions +=
			    sc->eg_pcb[8] & 0xf;
			sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
			egstart(&sc->sc_arpcom.ac_if);
			break;

		case EG_RSP_GETSTATS:
			dprintf(("Card Statistics\n"));
			bcopy(&sc->eg_pcb[2], &i, sizeof(i));
			dprintf(("Receive Packets %d\n", i));
			bcopy(&sc->eg_pcb[6], &i, sizeof(i));
			dprintf(("Transmit Packets %d\n", i));
			dprintf(("CRC errors %d\n", *(short *)&sc->eg_pcb[10]));
			dprintf(("alignment errors %d\n",
			    *(short *)&sc->eg_pcb[12]));
			dprintf(("no resources errors %d\n",
			    *(short *)&sc->eg_pcb[14]));
			dprintf(("overrun errors %d\n",
			    *(short *)&sc->eg_pcb[16]));
			break;
			
		default:
			dprintf(("egintr: Unknown response %x??\n",
			    sc->eg_pcb[0]));
			egprintpcb(sc);
			break;
		}
	}

	return (ret);
}

/*
 * Pass a packet up to the higher levels.
 */
void
egread(sc, buf, len)
	struct eg_softc *sc;
	caddr_t buf;
	int len;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *m;
	
	if (len <= sizeof(struct ether_header) ||
	    len > ETHER_MAX_LEN) {
		printf("%s: invalid packet size %d; dropping\n",
		    sc->sc_dev.dv_xname, len);
		ifp->if_ierrors++;
		return;
	}

	/* Pull packet off interface. */
	m = egget(sc, buf, len);
	if (m == 0) {
		ifp->if_ierrors++;
		return;
	}

	ifp->if_ipackets++;

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If so, hand off the raw packet to BPF.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m);
#endif

	ether_input_mbuf(ifp, m);
}

/*
 * convert buf into mbufs
 */
struct mbuf *
egget(sc, buf, totlen)
	struct eg_softc *sc;
	caddr_t buf;
	int totlen;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *top, **mp, *m;
	int len;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	len = MHLEN;
	top = 0;
	mp = &top;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			len = MLEN;
		}
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		m->m_len = len = min(totlen, len);
		bcopy((caddr_t)buf, mtod(m, caddr_t), len);
		buf += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}

int
egioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct eg_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			eginit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			eginit(sc);
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
			egstop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			eginit(sc);
		} else {
			sc->eg_pcb[0] = EG_CMD_GETSTATS;
			sc->eg_pcb[1] = 0;
			if (egwritePCB(sc) != 0)
				dprintf(("write error\n"));
			/*
			 * XXX deal with flags changes:
			 * IFF_MULTICAST, IFF_PROMISC,
			 * IFF_LINK0, IFF_LINK1,
			 */
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

void
egreset(sc)
	struct eg_softc *sc;
{
	int s;

	dprintf(("egreset()\n"));
	s = splnet();
	egstop(sc);
	eginit(sc);
	splx(s);
}

void
egwatchdog(ifp)
	struct ifnet *ifp;
{
	struct eg_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	sc->sc_arpcom.ac_if.if_oerrors++;

	egreset(sc);
}

void
egstop(sc)
	register struct eg_softc *sc;
{
	bus_space_tag_t bst = sc->sc_bst;
	bus_space_handle_t bsh = sc->sc_bsh;
	
	bus_space_write_1(bst, bsh, EG_CONTROL, 0);
}
