/*	$OpenBSD: if_cnw.c,v 1.21 2010/08/30 20:33:18 deraadt Exp $	*/
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Eriksson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is a driver for the Xircom CreditCard Netwave (also known as
 * the Netwave Airsurfer) wireless LAN PCMCIA adapter.
 *
 * When this driver was developed, the Linux Netwave driver was used
 * as a hardware manual. That driver is Copyright (c) 1997 University
 * of Tromsø, Norway. It is part of the Linux pcmcia-cs package that
 * can be found at http://pcmcia-cs.sourceforge.net/. The most
 * recent version of the pcmcia-cs package when this driver was
 * written was 3.0.6.
 *
 * Unfortunately, a lot of explicit numeric constants were used in the
 * Linux driver. I have tried to use symbolic names whenever possible,
 * but since I don't have any real hardware documentation, there's
 * still one or two "magic numbers" :-(.
 *
 * Driver limitations: This driver doesn't do multicasting or receiver
 * promiscuity, because of missing hardware documentation. I couldn't
 * get receiver promiscuity to work, and I haven't even tried
 * multicast. Volunteers are welcome, of course :-).
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>

#include <dev/pcmcia/if_cnwreg.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <net/if.h>
#include <net/if_dl.h>

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


/*
 * Let these be patchable variables, initialized from macros that can
 * be set in the kernel config file. Someone with lots of spare time
 * could probably write a nice Netwave configuration program to do
 * this a little bit more elegantly :-).
 */
#ifndef CNW_DOMAIN
#define CNW_DOMAIN	0x100
#endif
int cnw_domain = CNW_DOMAIN;		/* Domain */
#ifndef CNW_SCRAMBLEKEY
#define CNW_SCRAMBLEKEY 0
#endif
int cnw_skey = CNW_SCRAMBLEKEY;		/* Scramble key */


int	cnw_match(struct device *, void *, void *);
void	cnw_attach(struct device *, struct device *, void *);
int	cnw_detach(struct device *, int);
int	cnw_activate(struct device *, int);

struct cnw_softc {
	struct device sc_dev;		    /* Device glue (must be first) */
	struct arpcom sc_arpcom;	    /* Ethernet common part */
	int sc_domain;			    /* Netwave domain */
	int sc_skey;			    /* Netwave scramble key */

	/* PCMCIA-specific stuff */
	struct pcmcia_function *sc_pf;	    /* PCMCIA function */
	struct pcmcia_io_handle sc_pcioh;   /* PCMCIA I/O space handle */
	int sc_iowin;			    /*   ...window */
	bus_space_tag_t sc_iot;		    /*   ...bus_space tag */
	bus_space_handle_t sc_ioh;	    /*   ...bus_space handle */
	struct pcmcia_mem_handle sc_pcmemh; /* PCMCIA memory handle */
	bus_addr_t sc_memoff;		    /*   ...offset */
	int sc_memwin;			    /*   ...window */
	bus_space_tag_t sc_memt;	    /*   ...bus_space tag */
	bus_space_handle_t sc_memh;	    /*   ...bus_space handle */
	void *sc_ih;			    /* Interrupt cookie */
};

struct cfattach cnw_ca = {
	sizeof(struct cnw_softc), cnw_match, cnw_attach,
	cnw_detach, cnw_activate 
};

struct cfdriver cnw_cd = {
	NULL, "cnw", DV_IFNET
};

void cnw_reset(struct cnw_softc *);
void cnw_init(struct cnw_softc *);
int cnw_enable(struct cnw_softc *sc);
void cnw_disable(struct cnw_softc *sc);
void cnw_config(struct cnw_softc *sc, u_int8_t *);
void cnw_start(struct ifnet *);
void cnw_transmit(struct cnw_softc *, struct mbuf *);
struct mbuf *cnw_read(struct cnw_softc *);
void cnw_recv(struct cnw_softc *);
int cnw_intr(void *arg);
int cnw_ioctl(struct ifnet *, u_long, caddr_t);
void cnw_watchdog(struct ifnet *);

/* ---------------------------------------------------------------- */

/* Help routines */
static int wait_WOC(struct cnw_softc *, int);
static int read16(struct cnw_softc *, int);
static int cnw_cmd(struct cnw_softc *, int, int, int, int);

/* 
 * Wait until the WOC (Write Operation Complete) bit in the 
 * ASR (Adapter Status Register) is asserted. 
 */
static int
wait_WOC(sc, line)
	struct cnw_softc *sc;
	int line;
{
	int i, asr;

	for (i = 0; i < 5000; i++) {
		asr = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CNW_REG_ASR);
		if (asr & CNW_ASR_WOC)
			return (0);
		DELAY(100);
	}
	if (line > 0)
		printf("%s: wedged at line %d\n", sc->sc_dev.dv_xname, line);
	return (1);
}
#define WAIT_WOC(sc) wait_WOC(sc, __LINE__)


/*
 * Read a 16 bit value from the card. 
 */
static int
read16(sc, offset)
	struct cnw_softc *sc;
	int offset;
{
	int hi, lo;

	/* This could presumably be done more efficient with
	 * bus_space_read_2(), but I don't know anything about the
	 * byte sex guarantees... Besides, this is pretty cheap as
	 * well :-)
	 */
	lo = bus_space_read_1(sc->sc_memt, sc->sc_memh,
			      sc->sc_memoff + offset);
	hi = bus_space_read_1(sc->sc_memt, sc->sc_memh,
			      sc->sc_memoff + offset + 1);
	return ((hi << 8) | lo);
}


/*
 * Send a command to the card by writing it to the command buffer.
 */
int
cnw_cmd(sc, cmd, count, arg1, arg2)
	struct cnw_softc *sc;
	int cmd, count, arg1, arg2;
{
	int ptr = sc->sc_memoff + CNW_EREG_CB;

	if (wait_WOC(sc, 0)) {
		printf("%s: wedged when issuing cmd 0x%x\n",
		       sc->sc_dev.dv_xname, cmd);
		/*
		 * We'll continue anyway, as that's probably the best
		 * thing we can do; at least the user knows there's a
		 * problem, and can reset the interface with ifconfig
		 * down/up.
		 */
	}

	bus_space_write_1(sc->sc_memt, sc->sc_memh, ptr, cmd);
	if (count > 0) {
		bus_space_write_1(sc->sc_memt, sc->sc_memh, ptr + 1, arg1);
		if (count > 1)
			bus_space_write_1(sc->sc_memt, sc->sc_memh,
					  ptr + 2, arg2);
	}
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
			  ptr + count + 1, CNW_CMD_EOC);
	return (0);
}
#define CNW_CMD0(sc, cmd) \
		do { cnw_cmd(sc, cmd, 0, 0, 0); } while (0)
#define CNW_CMD1(sc, cmd, arg1)	\
		do { cnw_cmd(sc, cmd, 1, arg1 , 0); } while (0)
#define CNW_CMD2(sc, cmd, arg1, arg2) \
		do { cnw_cmd(sc, cmd, 2, arg1, arg2); } while (0)

/* ---------------------------------------------------------------- */

/*
 * Reset the hardware.
 */
void
cnw_reset(sc)
	struct cnw_softc *sc;
{
#ifdef CNW_DEBUG
	if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("%s: resetting\n", sc->sc_dev.dv_xname);
#endif
	wait_WOC(sc, 0);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CNW_REG_PMR, CNW_PMR_RESET);
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
			  sc->sc_memoff + CNW_EREG_ASCC, CNW_ASR_WOC);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CNW_REG_PMR, 0);
}


/*
 * Initialize the card.
 */
void
cnw_init(sc)
	struct cnw_softc *sc;
{
	/* Reset the card */
	cnw_reset(sc);

	/* Issue a NOP to check the card */
	CNW_CMD0(sc, CNW_CMD_NOP);

	/* Set up receive configuration */
	CNW_CMD1(sc, CNW_CMD_SRC, CNW_RXCONF_RXENA | CNW_RXCONF_BCAST);

	/* Set up transmit configuration */
	CNW_CMD1(sc, CNW_CMD_STC, CNW_TXCONF_TXENA);

	/* Set domain */
	CNW_CMD2(sc, CNW_CMD_SMD, sc->sc_domain, sc->sc_domain >> 8);

	/* Set scramble key */
	CNW_CMD2(sc, CNW_CMD_SSK, sc->sc_skey, sc->sc_skey >> 8);

	/* Enable interrupts */
	WAIT_WOC(sc);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
			  CNW_REG_IMR, CNW_IMR_IENA | CNW_IMR_RFU1);

	/* Enable receiver */
	CNW_CMD0(sc, CNW_CMD_ER);

	/* "Set the IENA bit in COR" */
	WAIT_WOC(sc);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CNW_REG_COR,
			  CNW_COR_IENA | CNW_COR_LVLREQ);
}


/*
 * Enable and initialize the card.
 */
int
cnw_enable(sc)
	struct cnw_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_NET,
	    cnw_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt handler\n",
		    sc->sc_dev.dv_xname);
		return (EIO);
	}
	if (pcmcia_function_enable(sc->sc_pf) != 0) {
		printf("%s: couldn't enable card\n", sc->sc_dev.dv_xname);
		return (EIO);
	}
	cnw_init(sc);
	ifp->if_flags |= IFF_RUNNING;
	return (0);
}


/*
 * Stop and disable the card.
 */
void
cnw_disable(sc)
	struct cnw_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
	pcmcia_function_disable(sc->sc_pf);
	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_timer = 0;
}


/*
 * Match the hardware we handle.
 */
int
cnw_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer == PCMCIA_VENDOR_XIRCOM &&
	    pa->product == PCMCIA_PRODUCT_XIRCOM_XIR_CNW_801)
		return (1);
	if (pa->manufacturer == PCMCIA_VENDOR_XIRCOM &&
	    pa->product == PCMCIA_PRODUCT_XIRCOM_XIR_CNW_802)
		return (1);
	return (0);
}


/*
 * Attach the card.
 */
void
cnw_attach(parent, self, aux)
	struct device  *parent, *self;
	void           *aux;
{
	struct cnw_softc *sc = (void *) self;
	struct pcmcia_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int i;

	/* Enable the card */
	sc->sc_pf = pa->pf;
	pcmcia_function_init(sc->sc_pf, SIMPLEQ_FIRST(&sc->sc_pf->cfe_head));
	if (pcmcia_function_enable(sc->sc_pf)) {
		printf(": function enable failed\n");
		return;
	}

	/* Map I/O register and "memory" */
	if (pcmcia_io_alloc(sc->sc_pf, 0, CNW_IO_SIZE, CNW_IO_SIZE,
			    &sc->sc_pcioh) != 0) {
		printf(": can't allocate i/o space\n");
		return;
	}
	if (pcmcia_io_map(sc->sc_pf, PCMCIA_WIDTH_IO16, 0,
			  CNW_IO_SIZE, &sc->sc_pcioh, &sc->sc_iowin) != 0) {
		printf(": can't map i/o space\n");
		return;
	}
	sc->sc_iot = sc->sc_pcioh.iot;
	sc->sc_ioh = sc->sc_pcioh.ioh;
	if (pcmcia_mem_alloc(sc->sc_pf, CNW_MEM_SIZE, &sc->sc_pcmemh) != 0) {
		printf(": can't allocate memory\n");
		return;
	}
	if (pcmcia_mem_map(sc->sc_pf, PCMCIA_MEM_COMMON, CNW_MEM_ADDR,
			   CNW_MEM_SIZE, &sc->sc_pcmemh, &sc->sc_memoff,
			   &sc->sc_memwin) != 0) {
		printf(": can't map mem space\n");
		return;
	}
	sc->sc_memt = sc->sc_pcmemh.memt;
	sc->sc_memh = sc->sc_pcmemh.memh;

	/* Finish setup of softc */
	sc->sc_domain = cnw_domain;
	sc->sc_skey = cnw_skey;

	/* Get MAC address */
	cnw_reset(sc);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->sc_arpcom.ac_enaddr[i] = bus_space_read_1(sc->sc_memt,
		    sc->sc_memh, sc->sc_memoff + CNW_EREG_PA + i);
	printf("%s: address %s\n", sc->sc_dev.dv_xname,
	       ether_sprintf(sc->sc_arpcom.ac_enaddr));

	/* Set up ifnet structure */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = cnw_start;
	ifp->if_ioctl = cnw_ioctl;
	ifp->if_watchdog = cnw_watchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	IFQ_SET_READY(&ifp->if_snd);

	/* Attach the interface */
	if_attach(ifp);
	ether_ifattach(ifp);

	/* Disable the card now, and turn it on when the interface goes up */
	pcmcia_function_disable(sc->sc_pf);
}

/*
 * Start outputting on the interface.
 */
void
cnw_start(ifp)
	struct ifnet *ifp;
{
	struct cnw_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	int asr;

#ifdef CNW_DEBUG
	if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("%s: cnw_start\n", ifp->if_xname);
#endif

	for (;;) {
		/* Is there any buffer space available on the card? */
		WAIT_WOC(sc);
		asr = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CNW_REG_ASR);
		if (!(asr & CNW_ASR_TXBA)) {
#ifdef CNW_DEBUG
			if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
				printf("%s: no buffer space\n", ifp->if_xname);
#endif
			return;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == 0)
			return;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif
		
		cnw_transmit(sc, m0);
		++ifp->if_opackets;
		ifp->if_timer = 3; /* start watchdog timer */
	}
}


/*
 * Transmit a packet.
 */
void
cnw_transmit(sc, m0)
	struct cnw_softc *sc;
	struct mbuf *m0;
{
	int buffer, bufsize, bufoffset, bufptr, bufspace, len, mbytes, n;
	struct mbuf *m;
	u_int8_t *mptr;

	/* Get buffer info from card */
	buffer = read16(sc, CNW_EREG_TDP);
	bufsize = read16(sc, CNW_EREG_TDP + 2);
	bufoffset = read16(sc, CNW_EREG_TDP + 4);
#ifdef CNW_DEBUG
	if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("%s: cnw_transmit b=0x%x s=%d o=0x%x\n",
		       sc->sc_dev.dv_xname, buffer, bufsize, bufoffset);
#endif

	/* Copy data from mbuf chain to card buffers */
	bufptr = sc->sc_memoff + buffer + bufoffset;
	bufspace = bufsize;
	len = 0;
	for (m = m0; m; ) {
		mptr = mtod(m, u_int8_t *);
		mbytes = m->m_len;
		len += mbytes;
		while (mbytes > 0) {
			if (bufspace == 0) {
				buffer = read16(sc, buffer);
				bufptr = sc->sc_memoff + buffer + bufoffset;
				bufspace = bufsize;
#ifdef CNW_DEBUG
				if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
					printf("%s:   next buffer @0x%x\n",
					       sc->sc_dev.dv_xname, buffer);
#endif
			}
			n = mbytes <= bufspace ? mbytes : bufspace;
			bus_space_write_region_1(sc->sc_memt, sc->sc_memh,
						 bufptr, mptr, n);
			bufptr += n;
			bufspace -= n;
			mptr += n;
			mbytes -= n;
		}
		MFREE(m, m0);
		m = m0;
	}

	/* Issue transmit command */
	CNW_CMD2(sc, CNW_CMD_TL, len, len >> 8);
}


/*
 * Pull a packet from the card into an mbuf chain.
 */
struct mbuf *
cnw_read(sc)
	struct cnw_softc *sc;
{
	struct mbuf *m, *top, **mp;
	int totbytes, buffer, bufbytes, bufptr, mbytes, n;
	u_int8_t *mptr;

	WAIT_WOC(sc);
	totbytes = read16(sc, CNW_EREG_RDP);
#ifdef CNW_DEBUG
	if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
		printf("%s: recv %d bytes\n", sc->sc_dev.dv_xname, totbytes);
#endif
	buffer = CNW_EREG_RDP + 2;
	bufbytes = 0;
	bufptr = 0; /* XXX make gcc happy */

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = &sc->sc_arpcom.ac_if;
	m->m_pkthdr.len = totbytes;
	mbytes = MHLEN;
	top = 0;
	mp = &top;

	while (totbytes > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			mbytes = MLEN;
		}
		if (totbytes >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				m_freem(top);
				return (0);
			}
			mbytes = MCLBYTES;
		}
		if (!top) {
			int pad =
			    ALIGN(sizeof(struct ether_header)) -
			        sizeof(struct ether_header);
			m->m_data += pad;
			mbytes -= pad;
		}
		mptr = mtod(m, u_int8_t *);
		mbytes = m->m_len = min(totbytes, mbytes);
		totbytes -= mbytes;
		while (mbytes > 0) {
			if (bufbytes == 0) {
				buffer = read16(sc, buffer);
				bufbytes = read16(sc, buffer + 2);
				bufptr = sc->sc_memoff + buffer +
					read16(sc, buffer + 4);
#ifdef CNW_DEBUG
				if (sc->sc_arpcom.ac_if.if_flags & IFF_DEBUG)
					printf("%s:   %d bytes @0x%x+0x%x\n",
					       sc->sc_dev.dv_xname, bufbytes,
					       buffer, bufptr - buffer -
					       sc->sc_memoff);
#endif
			}
			n = mbytes <= bufbytes ? mbytes : bufbytes;
			bus_space_read_region_1(sc->sc_memt, sc->sc_memh,
						bufptr, mptr, n);
			bufbytes -= n;
			bufptr += n;
			mbytes -= n;
			mptr += n;
		}
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}


/*
 * Handle received packets.
 */
void
cnw_recv(sc)
	struct cnw_softc *sc;
{
	int rser;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *m;
	struct ether_header *eh;

	for (;;) {
		WAIT_WOC(sc);
		rser = bus_space_read_1(sc->sc_memt, sc->sc_memh,
					sc->sc_memoff + CNW_EREG_RSER);
		if (!(rser & CNW_RSER_RXAVAIL))
			return;

		/* Pull packet off card */
		m = cnw_read(sc);

		/* Acknowledge packet */
		CNW_CMD0(sc, CNW_CMD_SRP);

		/* Did we manage to get the packet from the interface? */
		if (m == 0) {
			++ifp->if_ierrors;
			return;
		}
		++ifp->if_ipackets;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

		/*
		 * Check that the packet is for us or {multi,broad}cast. Maybe
		 * there's a fool-poof hardware check for this, but I don't
		 * really know...
		 */
		eh = mtod(m, struct ether_header *);
		if ((eh->ether_dhost[0] & 1) == 0 && /* !mcast and !bcast */
		    bcmp(sc->sc_arpcom.ac_enaddr, eh->ether_dhost,
			sizeof(eh->ether_dhost)) != 0) {
			m_freem(m);
			continue;
		}

		ether_input_mbuf(ifp, m);
	}
}


/*
 * Interrupt handler.
 */
int
cnw_intr(arg)
	void *arg;
{
	struct cnw_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int ret, status, rser, tser;

	if (!(sc->sc_arpcom.ac_if.if_flags & IFF_RUNNING))
		return (0);
	ifp->if_timer = 0;	/* stop watchdog timer */

	ret = 0;
	for (;;) {
		WAIT_WOC(sc);
		if (!(bus_space_read_1(sc->sc_iot, sc->sc_ioh,
				       CNW_REG_CCSR) & 0x02)) {
			if (ret == 0)
				printf("%s: spurious interrupt\n",
				       sc->sc_dev.dv_xname);
			return (ret);
		}
		ret = 1;
		status = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CNW_REG_ASR);

		/* Anything to receive? */
		if (status & CNW_ASR_RXRDY)
			cnw_recv(sc);

		/* Receive error */
		if (status & CNW_ASR_RXERR) {
			/*
			 * I get a *lot* of spurious receive errors
			 * (many per second), even when the interface
			 * is quiescent, so we don't increment
			 * if_ierrors here.
			 */
			rser = bus_space_read_1(sc->sc_memt, sc->sc_memh,
						sc->sc_memoff + CNW_EREG_RSER);
			/* Clear error bits in RSER */
			WAIT_WOC(sc);
			bus_space_write_1(sc->sc_memt, sc->sc_memh,
				sc->sc_memoff + CNW_EREG_RSERW,
				CNW_RSER_RXERR |
				(rser & (CNW_RSER_RXCRC | CNW_RSER_RXBIG)));
			/* Clear RXERR in ASR */
			WAIT_WOC(sc);
			bus_space_write_1(sc->sc_memt, sc->sc_memh,
				sc->sc_memoff + CNW_EREG_ASCC, CNW_ASR_RXERR);
		}

		/* Transmit done */
		if (status & CNW_ASR_TXDN) {
			tser = bus_space_read_1(sc->sc_memt, sc->sc_memh,
						CNW_EREG_TSER);
			if (tser & CNW_TSER_TXOK) {
				WAIT_WOC(sc);
				bus_space_write_1(sc->sc_memt, sc->sc_memh,
					sc->sc_memoff + CNW_EREG_TSERW,
					CNW_TSER_TXOK | CNW_TSER_RTRY);
			}
			if (tser & CNW_TSER_ERROR) {
				++ifp->if_oerrors;
				WAIT_WOC(sc);
				bus_space_write_1(sc->sc_memt, sc->sc_memh,
					sc->sc_memoff + CNW_EREG_TSERW,
					(tser & CNW_TSER_ERROR) |
					CNW_TSER_RTRY);
			}
			/* Continue to send packets from the queue */
			cnw_start(&sc->sc_arpcom.ac_if);
		}
				
	}
}


/*
 * Handle device ioctls.
 */
int
cnw_ioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct cnw_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		if (!(ifp->if_flags & IFF_RUNNING) &&
		    (error = cnw_enable(sc)) != 0)
			break;
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_RUNNING) {
			/*
			 * The interface is marked down and it is running, so
			 * stop it.
			 */
			cnw_disable(sc);
		} else if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_UP){
			/*
			 * The interface is marked up and it is stopped, so
			 * start it.
			 */
			error = cnw_enable(sc);
		}
		break;

	default:
		error = ENOTTY;
		break;
	}

	splx(s);
	return (error);
}


/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 * generate an interrupt after a transmit has been started on it.
 */
void
cnw_watchdog(ifp)
	struct ifnet *ifp;
{
	struct cnw_softc *sc = ifp->if_softc;

	printf("%s: device timeout; card reset\n", sc->sc_dev.dv_xname);
	++ifp->if_oerrors;
	cnw_init(sc);
}


int
cnw_detach(dev, flags)
	struct device *dev;
	int flags;
{
	struct cnw_softc *sc = (struct cnw_softc *)dev;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int rv = 0;

	pcmcia_io_unmap(sc->sc_pf, sc->sc_iowin);
	pcmcia_io_free(sc->sc_pf, &sc->sc_pcioh);
	pcmcia_mem_unmap(sc->sc_pf, sc->sc_memwin);
	pcmcia_mem_free(sc->sc_pf, &sc->sc_pcmemh);

	ether_ifdetach(ifp);
	if_detach(ifp);

	return (rv);
}

int
cnw_activate(dev, act)
	struct device *dev;
	int act;
{
	struct cnw_softc *sc = (struct cnw_softc *)dev;
        struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	switch (act) {
	case DVACT_ACTIVATE:
		pcmcia_function_enable(sc->sc_pf);
		sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_NET,
		    cnw_intr, sc, sc->sc_dev.dv_xname);
		cnw_init(sc);
		break;
	case DVACT_DEACTIVATE:
		ifp->if_timer = 0;
		ifp->if_flags &= ~IFF_RUNNING; /* XXX no cnw_stop() ? */
		if (sc->sc_ih)
			pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = NULL;
		pcmcia_function_disable(sc->sc_pf);
		break;
	}
	return (0);
}
