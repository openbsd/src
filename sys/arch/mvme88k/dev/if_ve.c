/*	$OpenBSD: if_ve.c,v 1.11 2001/11/06 19:53:15 miod Exp $ */
/*-
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1982, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* if_ve.c - Motorola MVME376 vme bus ethernet driver.  Based on if_le.c */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h> 
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/bugio.h>
#include <machine/mmu.h>	/* DMA_CACHE_SYNC, etc... */

#include <mvme88k/dev/if_vereg.h>
#include <mvme88k/dev/if_vevar.h>
#include <mvme88k/dev/pcctworeg.h>
#include <mvme88k/dev/vme.h>

#define LEDEBUG 1


#ifdef LEDEBUG
void ve_recv_print __P((struct vam7990_softc *, int));
void ve_xmit_print __P((struct vam7990_softc *, int));
#endif

void ve_rint __P((struct vam7990_softc *));
void ve_tint __P((struct vam7990_softc *));

int ve_put __P((struct vam7990_softc *, int, struct mbuf *));
struct mbuf *ve_get __P((struct vam7990_softc *, int, int));
void ve_read __P((struct vam7990_softc *, int, int)); 

void ve_shutdown __P((void *));

#define	ifp	(&sc->sc_arpcom.ac_if)
#ifndef	ETHER_CMP
#define	ETHER_CMP(a, b) bcmp((a), (b), ETHER_ADDR_LEN)
#endif
#define LANCE_REVC_BUG 1
/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * arpcom.ac_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct	ve_softc {
	struct vam7990_softc	sc_am7990; /* glue to MI code */
	struct intrhand		sc_ih;      /* interrupt vectoring */
	struct vereg1		*sc_r1;     /* LANCE registers */
	u_short			csr;        /* Control/Status reg image */
	u_long			board_addr;
	struct evcnt		sc_intrcnt;
	struct evcnt		sc_errcnt;
	struct vme2reg		*sc_vme2;
	u_char			sc_ipl;
	u_char			sc_vec;
	int			sc_flags;
};

struct cfdriver ve_cd = {
	NULL, "ve", DV_IFNET
};


/* autoconfiguration driver */
void	veattach(struct device *, struct device *, void *);
int	vematch(struct device *, void *, void *);
void	veetheraddr(struct vam7990_softc *sc);

struct cfattach ve_ca = {
	sizeof(struct ve_softc), vematch, veattach
};

void vewrcsr __P((struct vam7990_softc *, u_int16_t, u_int16_t));
u_int16_t verdcsr __P((struct vam7990_softc *, u_int16_t));
void nvram_cmd __P((struct vam7990_softc *, u_char, u_short));
u_int16_t nvram_read __P((struct vam7990_softc *, u_char));
void vereset __P((struct vam7990_softc *));
void ve_ackint __P((struct vam7990_softc *));

/* send command to the nvram controller */
void
nvram_cmd(sc, cmd, addr)
	struct vam7990_softc *sc;
	u_char cmd;
	u_short addr;
{
	int i;
	u_char rcmd = 0;
	struct vereg1 *reg1 = ((struct ve_softc *)sc)->sc_r1;

	rcmd = addr;
	rcmd = rcmd << 3;
	rcmd |= cmd;
	for (i=0;i<8;i++) {
		reg1->ver1_ear=((cmd|(addr<<1))>>i); 
		CDELAY; 
	} 
}

/* read nvram one bit at a time */
u_int16_t
nvram_read(sc, nvram_addr)
	struct vam7990_softc *sc;
	u_char nvram_addr;
{
	u_short val = 0, mask = 0x04000;
	u_int16_t wbit;
	/* these used by macros DO NOT CHANGE!*/
	struct vereg1 *reg1 = ((struct ve_softc *)sc)->sc_r1;
	sc->csr = 0x4f;
	ENABLE_NVRAM;
	nvram_cmd(sc, NVRAM_RCL, 0);
	DISABLE_NVRAM;
	CDELAY;
	ENABLE_NVRAM;
	nvram_cmd(sc, NVRAM_READ, nvram_addr);
	for (wbit=0; wbit<15; wbit++) {
		(reg1->ver1_ear & 0x01) ? 
			(val = (val | mask)) : (val = (val & (~mask)));
		mask = mask>>1;
		CDELAY;
	}
	(reg1->ver1_ear & 0x01) ? 
		(val = (val | 0x8000)) : (val = (val & 0x7FFF));
	CDELAY;
	DISABLE_NVRAM;
	return (val);
}

void
vewrcsr(sc, port, val)
	struct vam7990_softc *sc;
	u_int16_t port, val;
{
	register struct vereg1 *ve1 = ((struct ve_softc *)sc)->sc_r1;

	ve1->ver1_rap = port;
	ve1->ver1_rdp = val;
}

u_int16_t
verdcsr(sc, port)
	struct vam7990_softc *sc;
	u_int16_t port;
{
	register struct vereg1 *ve1 = ((struct ve_softc *)sc)->sc_r1;
	u_int16_t val;

	ve1->ver1_rap = port;
	val = ve1->ver1_rdp;
	return (val);
}

/* reset MVME376, set ipl and vec */
void
vereset(sc)
	struct vam7990_softc *sc;
{
	register struct vereg1 *reg1 = ((struct ve_softc *)sc)->sc_r1;
	u_char vec = ((struct ve_softc *)sc)->sc_vec;
	u_char ipl = ((struct ve_softc *)sc)->sc_ipl;
	sc->csr = 0x4f;
	WRITE_CSR_AND( ~ipl );
	SET_VEC(vec);
	return;
}

/* ack the intrrupt by reenableling interrupts */
void
ve_ackint(sc)
	struct vam7990_softc *sc;
{
	register struct vereg1 *reg1 = ((struct ve_softc *)sc)->sc_r1;
	ENABLE_INTR;
        CLEAR_INTR;
}

int
vematch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{

	struct confargs *ca = args;
	if (!badvaddr((unsigned)ca->ca_vaddr, 1)) {
		return (1);
	} else {
		return (0);
	}           
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
veattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct ve_softc *lesc = (struct ve_softc *)self;
	struct vam7990_softc *sc = &lesc->sc_am7990;
	struct confargs *ca = aux;
	int pri = ca->ca_ipl;
	caddr_t addr;

	addr = ca->ca_vaddr;

	if (addr == 0) {
		printf("\n%s: can't map LANCE registers\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/* Are we the boot device? */
	if (ca->ca_paddr == bootaddr)
		bootdv = self;
	
	lesc->sc_r1 = (struct vereg1 *)ca->ca_vaddr;
	lesc->sc_ipl = ca->ca_ipl;
	lesc->sc_vec = ca->ca_vec;


	/* get the first avaliable etherbuf */
	switch ((int)ca->ca_paddr) {
	case 0xFFFF1200:
		addr = (caddr_t)0xFD6C0000;
		break;
	case 0xFFFF1400:
		addr = (caddr_t)0xFD700000;
		break;
	case 0xFFFF1600:
		addr = (caddr_t)0xFD740000;
		break;
	default:
		panic("ve: invalid address");
	}
	
	sc->sc_mem = (void *)mapiodev(addr, LEMEMSIZE);
	
	if (sc->sc_mem == NULL)	panic("ve: no more memory in external I/O map");
	sc->sc_memsize = LEMEMSIZE;
	sc->sc_conf3 = LE_C3_BSWP;
	sc->sc_addr = kvtop((vm_offset_t)sc->sc_mem);

	/* get ether address via bug call */
	veetheraddr(sc);

	evcnt_attach(&sc->sc_dev, "intr", &lesc->sc_intrcnt);
	evcnt_attach(&sc->sc_dev, "errs", &lesc->sc_errcnt);

	sc->sc_copytodesc = ve_copytobuf_contig;
	sc->sc_copyfromdesc = ve_copyfrombuf_contig;
	sc->sc_copytobuf = ve_copytobuf_contig;
	sc->sc_copyfrombuf = ve_copyfrombuf_contig;
	sc->sc_zerobuf = ve_zerobuf_contig;

	sc->sc_rdcsr = verdcsr;
	sc->sc_wrcsr = vewrcsr;
	sc->sc_hwreset = vereset;
	sc->sc_hwinit = NULL;
	vereset(sc);

	ve_config(sc);

	/* connect the interrupt */
	lesc->sc_ih.ih_fn = ve_intr;
	lesc->sc_ih.ih_arg = sc;
	lesc->sc_ih.ih_wantframe = 0;
	lesc->sc_ih.ih_ipl = pri;
	vmeintr_establish(ca->ca_vec + 0, &lesc->sc_ih);
}

void
veetheraddr(sc)
	struct vam7990_softc *sc;
{
	u_char * cp = sc->sc_arpcom.ac_enaddr;
	u_int16_t ival[3];
	u_char i;

	for (i=0; i<3; i++) {
		ival[i] = nvram_read(sc, i);
	}
	memcpy(cp, &ival[0], 6);
}

void
ve_config(sc)
	struct vam7990_softc *sc;
{
	int mem;

	/* Make sure the chip is stopped. */
	ve_stop(sc);

	/* Initialize ifnet structure. */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = ve_start;
	ifp->if_ioctl = ve_ioctl;
	ifp->if_watchdog = ve_watchdog;
	ifp->if_flags =
	IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
#ifdef LANCE_REVC_BUG
	ifp->if_flags &= ~IFF_MULTICAST;
#endif

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	if (sc->sc_memsize > 262144)
		sc->sc_memsize = 262144;

	switch (sc->sc_memsize) {
	case 8192:
		sc->sc_nrbuf = 4;
		sc->sc_ntbuf = 1;
		break;
	case 16384:
		sc->sc_nrbuf = 8;
		sc->sc_ntbuf = 2;
		break;
	case 32768:
		sc->sc_nrbuf = 16;
		sc->sc_ntbuf = 4;
		break;
	case 65536:
		sc->sc_nrbuf = 32;
		sc->sc_ntbuf = 8;
		break;
	case 131072:
		sc->sc_nrbuf = 64;
		sc->sc_ntbuf = 16;
		break;
	case 262144:
		sc->sc_nrbuf = 128;
		sc->sc_ntbuf = 32;
		break;
	default:
		panic("ve_config: weird memory size %d", sc->sc_memsize);
	}

	printf("\n%s: address %s\n", sc->sc_dev.dv_xname,
	       ether_sprintf(sc->sc_arpcom.ac_enaddr));
	printf("%s: %d receive buffers, %d transmit buffers\n", 
	       sc->sc_dev.dv_xname, sc->sc_nrbuf, sc->sc_ntbuf);

	sc->sc_sh = shutdownhook_establish(ve_shutdown, sc);
	if (sc->sc_sh == NULL)
		panic("ve_config: can't establish shutdownhook");

	mem = 0;
	sc->sc_initaddr = mem;
	mem += sizeof(struct veinit);
	sc->sc_rmdaddr = mem;
	mem += sizeof(struct vermd) * sc->sc_nrbuf;
	sc->sc_tmdaddr = mem;
	mem += sizeof(struct vetmd) * sc->sc_ntbuf;
	sc->sc_rbufaddr = mem;
	mem += LEBLEN * sc->sc_nrbuf;
	sc->sc_tbufaddr = mem;
	mem += LEBLEN * sc->sc_ntbuf;
#ifdef notyet
	if (mem > ...)
		panic(...);
#endif
}

void
ve_reset(sc)
	struct vam7990_softc *sc;
{
	int s;

	s = splimp();
	ve_init(sc);
	splx(s);
}

/*
 * Set up the initialization block and the descriptor rings.
 */
void
ve_meminit(sc)
	register struct vam7990_softc *sc;
{
	u_long a;
	int bix;
	struct veinit init;
	struct vermd rmd;
	struct vetmd tmd;

#if NBPFILTER > 0
	if (ifp->if_flags & IFF_PROMISC)
		init.init_mode = LE_MODE_NORMAL | LE_MODE_PROM;
	else
#endif
		init.init_mode = LE_MODE_NORMAL;
	init.init_padr[0] =
	(sc->sc_arpcom.ac_enaddr[1] << 8) | sc->sc_arpcom.ac_enaddr[0];
	init.init_padr[1] =
	(sc->sc_arpcom.ac_enaddr[3] << 8) | sc->sc_arpcom.ac_enaddr[2];
	init.init_padr[2] =
	(sc->sc_arpcom.ac_enaddr[5] << 8) | sc->sc_arpcom.ac_enaddr[4];
	ve_setladrf(&sc->sc_arpcom, init.init_ladrf);

	sc->sc_last_rd = 0;
	sc->sc_first_td = sc->sc_last_td = sc->sc_no_td = 0;

	a = sc->sc_addr + LE_RMDADDR(sc, 0);
	init.init_rdra = a;
	init.init_rlen = (a >> 16) | ((ffs(sc->sc_nrbuf) - 1) << 13);

	a = sc->sc_addr + LE_TMDADDR(sc, 0);
	init.init_tdra = a;
	init.init_tlen = (a >> 16) | ((ffs(sc->sc_ntbuf) - 1) << 13);

	(*sc->sc_copytodesc)(sc, &init, LE_INITADDR(sc), sizeof(init));

	/*
	 * Set up receive ring descriptors.
	 */
	for (bix = 0; bix < sc->sc_nrbuf; bix++) {
		a = sc->sc_addr + LE_RBUFADDR(sc, bix);
		rmd.rmd0 = a;
		rmd.rmd1_hadr = a >> 16;
		rmd.rmd1_bits = LE_R1_OWN;
		rmd.rmd2 = -LEBLEN | LE_XMD2_ONES;
		rmd.rmd3 = 0;
		(*sc->sc_copytodesc)(sc, &rmd, LE_RMDADDR(sc, bix),
				     sizeof(rmd));
	}

	/*
	 * Set up transmit ring descriptors.
	 */
	for (bix = 0; bix < sc->sc_ntbuf; bix++) {
		a = sc->sc_addr + LE_TBUFADDR(sc, bix);
		tmd.tmd0 = a;
		tmd.tmd1_hadr = a >> 16;
		tmd.tmd1_bits = LE_R1_STP | LE_R1_ENP;
		tmd.tmd2 = -2000 | LE_XMD2_ONES;
		tmd.tmd3 = 0;
		(*sc->sc_copytodesc)(sc, &tmd, LE_TMDADDR(sc, bix),
				     sizeof(tmd));
	}
}

void
ve_stop(sc)
	struct vam7990_softc *sc;
{

	(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_STOP);
}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
void
ve_init(sc)
	register struct vam7990_softc *sc;
{
	register int timo;
	u_long a;

	(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_STOP);
	DELAY(100);

	/* Newer LANCE chips have a reset register */
	if (sc->sc_hwreset)
		(*sc->sc_hwreset)(sc);

	/* Set the correct byte swapping mode, etc. */
	(*sc->sc_wrcsr)(sc, LE_CSR3, sc->sc_conf3);

	/* Set up LANCE init block. */
	ve_meminit(sc);

	/* Give LANCE the physical address of its init block. */
	a = sc->sc_addr + LE_INITADDR(sc);
	(*sc->sc_wrcsr)(sc, LE_CSR1, a);
	(*sc->sc_wrcsr)(sc, LE_CSR2, a >> 16);

	/* Try to initialize the LANCE. */
	DELAY(100);
	(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_INIT);

	/* Wait for initialization to finish. */
	for (timo = 100000; timo; timo--)
		if ((*sc->sc_rdcsr)(sc, LE_CSR0) & LE_C0_IDON)
			break;

	if ((*sc->sc_rdcsr)(sc, LE_CSR0) & LE_C0_IDON) {
		/* Start the LANCE. */
		(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_INEA | LE_C0_STRT |
		    LE_C0_IDON);
		ifp->if_flags |= IFF_RUNNING;
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;
		ve_start(ifp);
	} else
		printf("%s: controller failed to initialize\n", sc->sc_dev.dv_xname);
	if (sc->sc_hwinit)
		(*sc->sc_hwinit)(sc);
}

/*
 * Routine to copy from mbuf chain to transmit buffer in
 * network buffer memory.
 */
int
ve_put(sc, boff, m)
	struct vam7990_softc *sc;
	int boff;
	register struct mbuf *m;
{
	register struct mbuf *n;
	register int len, tlen = 0;

	for (; m; m = n) {
		len = m->m_len;
		if (len == 0) {
			MFREE(m, n);
			continue;
		}
		(*sc->sc_copytobuf)(sc, mtod(m, caddr_t), boff, len);
		boff += len;
		tlen += len;
		MFREE(m, n);
	}
	if (tlen < LEMINSIZE) {
		(*sc->sc_zerobuf)(sc, boff, LEMINSIZE - tlen);
		tlen = LEMINSIZE;
	}
	return (tlen);
}

/*
 * Pull data off an interface.
 * Len is length of data, with local net header stripped.
 * We copy the data into mbufs.  When full cluster sized units are present
 * we copy into clusters.
 */
struct mbuf *
ve_get(sc, boff, totlen)
	struct vam7990_softc *sc;
	int boff, totlen;
{
	register struct mbuf *m;
	struct mbuf *top, **mp;
	int len, pad;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	pad = ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header);
	m->m_data += pad;
	len = MHLEN - pad;
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
		if (top && totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		m->m_len = len = min(totlen, len);
		(*sc->sc_copyfrombuf)(sc, mtod(m, caddr_t), boff, len);
		boff += len;
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}

/*
 * Pass a packet to the higher levels.
 */
void
ve_read(sc, boff, len)
	register struct vam7990_softc *sc;
	int boff, len;
{
	struct mbuf *m;
#ifdef LANCE_REVC_BUG
	struct ether_header *eh;
#endif

	if (len <= sizeof(struct ether_header) ||
	    len > ETHERMTU + sizeof(struct ether_header)) {
#ifdef LEDEBUG
		printf("%s: invalid packet size %d; dropping\n",
		    sc->sc_dev.dv_xname, len);
#endif
		ifp->if_ierrors++;
		return;
	}

	/* Pull packet off interface. */
	m = ve_get(sc, boff, len);
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

#ifdef LANCE_REVC_BUG
	/*
	 * The old LANCE (Rev. C) chips have a bug which causes
	 * garbage to be inserted in front of the received packet.
	 * The work-around is to ignore packets with an invalid
	 * destination address (garbage will usually not match).
	 * Of course, this precludes multicast support...
	 */
	eh = mtod(m, struct ether_header *);
	if (ETHER_CMP(eh->ether_dhost, sc->sc_arpcom.ac_enaddr) &&
	    ETHER_CMP(eh->ether_dhost, etherbroadcastaddr)) {
		m_freem(m);
		return;
	}
#endif

	/* Pass the packet up. */
	ether_input_mbuf(ifp, m);
}

void
ve_rint(sc)
	struct vam7990_softc *sc;
{
	register int bix;
	int rp;
	struct vermd rmd;

	bix = sc->sc_last_rd;

	/* Process all buffers with valid data. */
	for (;;) {
		rp = LE_RMDADDR(sc, bix);
		(*sc->sc_copyfromdesc)(sc, &rmd, rp, sizeof(rmd));

		if (rmd.rmd1_bits & LE_R1_OWN)
			break;

		if (rmd.rmd1_bits & LE_R1_ERR) {
			if (rmd.rmd1_bits & LE_R1_ENP) {
#ifdef LEDEBUG
				if ((rmd.rmd1_bits & LE_R1_OFLO) == 0) {
					if (rmd.rmd1_bits & LE_R1_FRAM)
						printf("%s: framing error\n",
						    sc->sc_dev.dv_xname);
					if (rmd.rmd1_bits & LE_R1_CRC)
						printf("%s: crc mismatch\n",
						    sc->sc_dev.dv_xname);
				}
#endif
			} else {
				if (rmd.rmd1_bits & LE_R1_OFLO)
					printf("%s: overflow\n",
					    sc->sc_dev.dv_xname);
			}
			if (rmd.rmd1_bits & LE_R1_BUFF)
				printf("%s: receive buffer error\n",
				    sc->sc_dev.dv_xname);
			ifp->if_ierrors++;
		} else if ((rmd.rmd1_bits & (LE_R1_STP | LE_R1_ENP)) !=
		    (LE_R1_STP | LE_R1_ENP)) {
			printf("%s: dropping chained buffer\n",
			    sc->sc_dev.dv_xname);
			ifp->if_ierrors++;
		} else {
#ifdef LEDEBUG
			if (sc->sc_debug)
				ve_recv_print(sc, sc->sc_last_rd);
#endif
			ve_read(sc, LE_RBUFADDR(sc, bix),
			    (int)rmd.rmd3 - 4);
		}

		rmd.rmd1_bits = LE_R1_OWN;
		rmd.rmd2 = -LEBLEN | LE_XMD2_ONES;
		rmd.rmd3 = 0;
		(*sc->sc_copytodesc)(sc, &rmd, rp, sizeof(rmd));

#ifdef LEDEBUG
		if (sc->sc_debug)
			printf("sc->sc_last_rd = %x, rmd: "
			       "ladr %04x, hadr %02x, flags %02x, "
			       "bcnt %04x, mcnt %04x\n",
				sc->sc_last_rd,
				rmd.rmd0, rmd.rmd1_hadr, rmd.rmd1_bits,
				rmd.rmd2, rmd.rmd3);
#endif

		if (++bix == sc->sc_nrbuf)
			bix = 0;
	}

	sc->sc_last_rd = bix;
}

void
ve_tint(sc)
	register struct vam7990_softc *sc;
{
	register int bix;
	struct vetmd tmd;

	bix = sc->sc_first_td;

	for (;;) {
		if (sc->sc_no_td <= 0)
			break;

		(*sc->sc_copyfromdesc)(sc, &tmd, LE_TMDADDR(sc, bix),
		    sizeof(tmd));

#ifdef LEDEBUG
		if (sc->sc_debug)
			printf("trans tmd: "
			    "ladr %04x, hadr %02x, flags %02x, "
			    "bcnt %04x, mcnt %04x\n",
			    tmd.tmd0, tmd.tmd1_hadr, tmd.tmd1_bits,
			    tmd.tmd2, tmd.tmd3);
#endif

		if (tmd.tmd1_bits & LE_T1_OWN)
			break;

		ifp->if_flags &= ~IFF_OACTIVE;

		if (tmd.tmd1_bits & LE_T1_ERR) {
			if (tmd.tmd3 & LE_T3_BUFF)
				printf("%s: transmit buffer error\n",
				    sc->sc_dev.dv_xname);
			else if (tmd.tmd3 & LE_T3_UFLO)
				printf("%s: underflow\n", sc->sc_dev.dv_xname);
			if (tmd.tmd3 & (LE_T3_BUFF | LE_T3_UFLO)) {
				ve_reset(sc);
				return;
			}
			if (tmd.tmd3 & LE_T3_LCAR) {
				if (sc->sc_nocarrier)
					(*sc->sc_nocarrier)(sc);
				else
					printf("%s: lost carrier\n",
					    sc->sc_dev.dv_xname);
			}
			if (tmd.tmd3 & LE_T3_LCOL)
				ifp->if_collisions++;
			if (tmd.tmd3 & LE_T3_RTRY) {
				printf("%s: excessive collisions, tdr %d\n",
				    sc->sc_dev.dv_xname,
				    tmd.tmd3 & LE_T3_TDR_MASK);
				ifp->if_collisions += 16;
			}
			ifp->if_oerrors++;
		} else {
			if (tmd.tmd1_bits & LE_T1_ONE)
				ifp->if_collisions++;
			else if (tmd.tmd1_bits & LE_T1_MORE)
				/* Real number is unknown. */
				ifp->if_collisions += 2;
			ifp->if_opackets++;
		}

		if (++bix == sc->sc_ntbuf)
			bix = 0;

		--sc->sc_no_td;
	}

	sc->sc_first_td = bix;

	ve_start(ifp);

	if (sc->sc_no_td == 0)
		ifp->if_timer = 0;
}

/*
 * Controller interrupt.
 */
int
ve_intr(arg)
	register void *arg;
{
	register struct vam7990_softc *sc = arg;
	register u_int16_t isr;

	isr = (*sc->sc_rdcsr)(sc, LE_CSR0);
#ifdef LEDEBUG
	if (sc->sc_debug)
		printf("%s: ve_intr entering with isr=%04x\n",
		    sc->sc_dev.dv_xname, isr);
#endif
	if ((isr & LE_C0_INTR) == 0)
		return (0);

	/* clear the interrupting condition */
	(*sc->sc_wrcsr)(sc, LE_CSR0,
			isr & (LE_C0_INEA | LE_C0_BABL | LE_C0_MISS | 
			       LE_C0_MERR | LE_C0_RINT | LE_C0_TINT | LE_C0_IDON));
	if (isr & LE_C0_ERR) {
		if (isr & LE_C0_BABL) {
#ifdef LEDEBUG
			printf("%s: babble\n", sc->sc_dev.dv_xname);
#endif
			ifp->if_oerrors++;
		}
#if 0
		if (isr & LE_C0_CERR) {
			printf("%s: collision error\n", sc->sc_dev.dv_xname);
			ifp->if_collisions++;
		}
#endif
		if (isr & LE_C0_MISS) {
#ifdef LEDEBUG
			printf("%s: missed packet\n", sc->sc_dev.dv_xname);
#endif
			ifp->if_ierrors++;
		}
		if (isr & LE_C0_MERR) {
			printf("%s: memory error\n", sc->sc_dev.dv_xname);
			ve_reset(sc);
			return (1);
		}
	}

	if ((isr & LE_C0_RXON) == 0) {
		printf("%s: receiver disabled\n", sc->sc_dev.dv_xname);
		ifp->if_ierrors++;
		ve_reset(sc);
		return (1);
	}
	if ((isr & LE_C0_TXON) == 0) {
		printf("%s: transmitter disabled\n", sc->sc_dev.dv_xname);
		ifp->if_oerrors++;
		ve_reset(sc);
		return (1);
	}

	if (isr & LE_C0_RINT)
		ve_rint(sc);
	if (isr & LE_C0_TINT)
		ve_tint(sc);
	ve_ackint(sc);
	return (1);
}

#undef	ifp

void
ve_watchdog(ifp)
	struct ifnet *ifp;
{
	struct vam7990_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++ifp->if_oerrors;

	ve_reset(sc);
}

/*
 * Setup output on interface.
 * Get another datagram to send off of the interface queue, and map it to the
 * interface before starting the output.
 * Called only at splimp or interrupt level.
 */
void
ve_start(ifp)
	register struct ifnet *ifp;
{
	register struct vam7990_softc *sc = ifp->if_softc;
	register int bix;
	register struct mbuf *m;
	struct vetmd tmd;
	int rp;
	int len;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = sc->sc_last_td;

	for (;;) {
		rp = LE_TMDADDR(sc, bix);
		(*sc->sc_copyfromdesc)(sc, &tmd, rp, sizeof(tmd));

		if (tmd.tmd1_bits & LE_T1_OWN) {
			ifp->if_flags |= IFF_OACTIVE;
			printf("missing buffer, no_td = %d, last_td = %d\n",
			    sc->sc_no_td, sc->sc_last_td);
		}

		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;

#if NBPFILTER > 0
		/*
		 * If BPF is listening on this interface, let it see the packet
		 * before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/*
		 * Copy the mbuf chain into the transmit buffer.
		 */
		len = ve_put(sc, LE_TBUFADDR(sc, bix), m);

#ifdef LEDEBUG
		if (len > ETHERMTU + sizeof(struct ether_header))
			printf("packet length %d\n", len);
#endif

		ifp->if_timer = 5;

		/*
		 * Init transmit registers, and set transmit start flag.
		 */
		tmd.tmd1_bits = LE_T1_OWN | LE_T1_STP | LE_T1_ENP;
		tmd.tmd2 = -len | LE_XMD2_ONES;
		tmd.tmd3 = 0;

		(*sc->sc_copytodesc)(sc, &tmd, rp, sizeof(tmd));

#ifdef LEDEBUG
		if (sc->sc_debug)
			ve_xmit_print(sc, sc->sc_last_td);
#endif

		(*sc->sc_wrcsr)(sc, LE_CSR0, LE_C0_INEA | LE_C0_TDMD);

		if (++bix == sc->sc_ntbuf)
			bix = 0;

		if (++sc->sc_no_td == sc->sc_ntbuf) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

	}

	sc->sc_last_td = bix;
}

/*
 * Process an ioctl request.
 */
int
ve_ioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	register struct vam7990_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
		splx(s);
		return error;
	}

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ve_init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			ve_init(sc);
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
			ve_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    	   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			ve_init(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			/*ve_stop(sc);*/
			ve_init(sc);
		}
#ifdef LEDEBUG
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_debug = 1;
		else
			sc->sc_debug = 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			ve_reset(sc);
			error = 0;
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		if (sc->sc_hasifmedia)
			error = ifmedia_ioctl(ifp, ifr, &sc->sc_ifmedia, cmd);
		else
			error = EINVAL;
		break;

	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}

void
ve_shutdown(arg)
	void *arg;
{

	ve_stop((struct vam7990_softc *)arg);
}

#ifdef LEDEBUG
void
ve_recv_print(sc, no)
	struct vam7990_softc *sc;
	int no;
{
	struct vermd rmd;
	u_int16_t len;
	struct ether_header eh;

	(*sc->sc_copyfromdesc)(sc, &rmd, LE_RMDADDR(sc, no), sizeof(rmd));
	len = rmd.rmd3;
	printf("%s: receive buffer %d, len = %d\n", sc->sc_dev.dv_xname, no,
	    len);
	printf("%s: status %04x\n", sc->sc_dev.dv_xname,
	    (*sc->sc_rdcsr)(sc, LE_CSR0));
	printf("%s: ladr %04x, hadr %02x, flags %02x, bcnt %04x, mcnt %04x\n",
	    sc->sc_dev.dv_xname,
	    rmd.rmd0, rmd.rmd1_hadr, rmd.rmd1_bits, rmd.rmd2, rmd.rmd3);
	if (len >= sizeof(eh)) {
		(*sc->sc_copyfrombuf)(sc, &eh, LE_RBUFADDR(sc, no), sizeof(eh));
		printf("%s: dst %s", sc->sc_dev.dv_xname,
			ether_sprintf(eh.ether_dhost));
		printf(" src %s type %04x\n", ether_sprintf(eh.ether_shost),
			ntohs(eh.ether_type));
	}
}

void
ve_xmit_print(sc, no)
	struct vam7990_softc *sc;
	int no;
{
	struct vetmd tmd;
	u_int16_t len;
	struct ether_header eh;

	(*sc->sc_copyfromdesc)(sc, &tmd, LE_TMDADDR(sc, no), sizeof(tmd));
	len = -tmd.tmd2;
	printf("%s: transmit buffer %d, len = %d\n", sc->sc_dev.dv_xname, no,
	    len);
	printf("%s: status %04x\n", sc->sc_dev.dv_xname,
	    (*sc->sc_rdcsr)(sc, LE_CSR0));
	printf("%s: ladr %04x, hadr %02x, flags %02x, bcnt %04x, mcnt %04x\n",
	    sc->sc_dev.dv_xname,
	    tmd.tmd0, tmd.tmd1_hadr, tmd.tmd1_bits, tmd.tmd2, tmd.tmd3);
	if (len >= sizeof(eh)) {
		(*sc->sc_copyfrombuf)(sc, &eh, LE_TBUFADDR(sc, no), sizeof(eh));
		printf("%s: dst %s", sc->sc_dev.dv_xname,
			ether_sprintf(eh.ether_dhost));
		printf(" src %s type %04x\n", ether_sprintf(eh.ether_shost),
		    ntohs(eh.ether_type));
	}
}
#endif /* LEDEBUG */

/*
 * Set up the logical address filter.
 */
void
ve_setladrf(ac, af)
	struct arpcom *ac;
	u_int16_t *af;
{
	struct ifnet *ifp = &ac->ac_if;
	struct ether_multi *enm;
	register u_char *cp, c;
	register u_int32_t crc;
	register int i, len;
	struct ether_multistep step;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	if (ifp->if_flags & IFF_PROMISC)
		goto allmulti;

	af[0] = af[1] = af[2] = af[3] = 0x0000;
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (ETHER_CMP(enm->enm_addrlo, enm->enm_addrhi)) {
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

		cp = enm->enm_addrlo;
		crc = 0xffffffff;
		for (len = sizeof(enm->enm_addrlo); --len >= 0;) {
			c = *cp++;
			for (i = 8; --i >= 0;) {
				if ((crc & 0x01) ^ (c & 0x01)) {
					crc >>= 1;
					crc ^= 0xedb88320;
				} else
					crc >>= 1;
				c >>= 1;
			}
		}
		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Set the corresponding bit in the filter. */
		af[crc >> 4] |= 1 << (crc & 0xf);

		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	return;

allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	af[0] = af[1] = af[2] = af[3] = 0xffff;
}


/*
 * Routines for accessing the transmit and receive buffers.
 * The various CPU and adapter configurations supported by this
 * driver require three different access methods for buffers
 * and descriptors:
 *	(1) contig (contiguous data; no padding),
 *	(2) gap2 (two bytes of data followed by two bytes of padding),
 *	(3) gap16 (16 bytes of data followed by 16 bytes of padding).
 */

/*
 * contig: contiguous data with no padding.
 *
 * Buffers may have any alignment.
 */

void
ve_copytobuf_contig(sc, from, boff, len)
	struct vam7990_softc *sc;
	void *from;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	volatile caddr_t phys = (caddr_t)sc->sc_addr;
	dma_cachectl((vm_offset_t)phys + boff, len, DMA_CACHE_SYNC);
	dma_cachectl((vm_offset_t)buf + boff, len, DMA_CACHE_SYNC);

	/*
	 * Just call bcopy() to do the work.
	 */
	bcopy(from, buf + boff, len);
}

void
ve_copyfrombuf_contig(sc, to, boff, len)
	struct vam7990_softc *sc;
	void *to;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	volatile caddr_t phys = (caddr_t)sc->sc_addr;
	dma_cachectl((vm_offset_t)phys + boff, len, DMA_CACHE_SYNC_INVAL);
	dma_cachectl((vm_offset_t)buf + boff, len, DMA_CACHE_SYNC_INVAL);
	/*
	 * Just call bcopy() to do the work.
	 */
	bcopy(buf + boff, to, len);
}

void
ve_zerobuf_contig(sc, boff, len)
	struct vam7990_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
	volatile caddr_t phys = (caddr_t)sc->sc_addr;
	dma_cachectl((vm_offset_t)phys + boff, len, DMA_CACHE_SYNC);
	dma_cachectl((vm_offset_t)buf + boff, len, DMA_CACHE_SYNC);

	/*
	 * Just let bzero() do the work
	 */
	bzero(buf + boff, len);
}
