/*    $OpenBSD: if_hp.c,v 1.15 2005/11/21 18:16:40 millert Exp $       */
/*    $NetBSD: if_hp.c,v 1.21 1995/12/24 02:31:31 mycroft Exp $       */

/* XXX THIS DRIVER IS BROKEN.  IT WILL NOT EVEN COMPILE. */

/*-
 * Copyright (c) 1990, 1991 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

/*
 * HP LAN Ethernet driver
 *
 * Parts inspired from Tim Tucker's if_wd driver for the wd8003,
 * insight on the ne2000 gained from Robert Clements PC/FTP driver.
 *
 * receive bottom end totally rewritten by Curt Mayer, Dec 1992.
 * no longer loses back to back packets.
 * note to driver writers: RTFM!
 *
 * hooks for packet filter added by Charles Hannum, 29DEC1992.
 *
 * Mostly rewritten for HP-labelled EISA controllers by Charles Hannum,
 * 18JAN1993.
 */

#include "hp.h"
#if NHP > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>

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

#include "bpfilter.h"
#if NBPFILTER > 0
#include <sys/selinfo.h>
#include <net/bpf.h>
#endif

#include <machine/cpu.h>
#include <machine/pio.h>

#include <i386/isa/isa_device.h>	/* XXX BROKEN */
#include <dev/isa/if_nereg.h>

int     hpprobe(), hpattach(), hpintr();
int     hpstart(), hpinit(), hpioctl();

struct isa_driver hpdriver =
{
	hpprobe, hpattach, "hp",
};

struct mbuf *hpget();

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * ns_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct hp_softc {
	struct arpcom ns_ac;	/* Ethernet common part */
#define	ns_if		ns_ac.ac_if	/* network-visible interface */
#define	ns_addrp	ns_ac.ac_enaddr	/* hardware Ethernet address */
	int     ns_flags;
#define	DSF_LOCK	1	/* block re-entering enstart */
	int     ns_oactive;
	int     ns_mask;
	struct prhdr ns_ph;	/* hardware header of incoming packet */
	u_char  ns_pb[2048];
	u_char  ns_txstart;	/* transmitter buffer start */
	u_char  ns_rxstart;	/* receiver buffer start */
	u_char  ns_rxend;	/* receiver buffer end */
	u_char  hp_type;	/* HP board type */
	u_char  hp_irq;		/* interrupt vector */
	short   ns_port;	/* i/o port base */
	short   ns_mode;	/* word/byte mode */
	short   ns_rcr;
#if NBPFILTER > 0
	caddr_t ns_bpf;
#endif
}
        hp_softc[NHP];
#define	ENBUFSIZE	(sizeof(struct ether_header) + ETHERMTU + 2 + ETHER_MIN_LEN)

#define	PAT(n)	(0xa55a + 37*(n))

u_short boarddata[16];

#define hp_option (-8)
#define hp_data (-4)
#define HP_RUN (0x01)
#define HP_DATA (0x10)

hpprobe(dvp)
	struct isa_device *dvp;
{
	int     val, i, s, sum, pat;
	register struct hp_softc *ns = &hp_softc[0];
	register hpc;

#ifdef lint
	hpintr(0);
#endif

	hpc = (ns->ns_port = dvp->id_iobase + 0x10);
	s = splnet();

	ns->hp_irq = ffs(dvp->id_irq) - 1;

	/* Extract board address */
	for (i = 0; i < 6; i++)
		ns->ns_addrp[i] = inb(hpc - 0x10 + i);
	ns->hp_type = inb(hpc - 0x10 + 7);

	if (ns->ns_addrp[0] != 0x08 ||
	    ns->ns_addrp[1] != 0x00 ||
	    ns->ns_addrp[2] != 0x09) {
		splx(s);
		return 0;
	}
	/* Word Transfers, Burst Mode Select, Fifo at 8 bytes */
	/* On this board, WTS means 32-bit transfers, which is still
	 * experimental.  - mycroft, 18JAN93 */
#ifdef HP_32BIT
	ns->ns_mode = DSDC_WTS | DSDC_BMS | DSDC_FT1;
#else
	ns->ns_mode = DSDC_BMS | DSDC_FT1;
#endif
	ns->ns_txstart = 0 * 1024 / DS_PGSIZE;
	ns->ns_rxend = 32 * 1024 / DS_PGSIZE;

	ns->ns_rxstart = ns->ns_txstart + (PKTSZ / DS_PGSIZE);

	outb(hpc + hp_option, HP_RUN);

#if 0
	outb(hpc + ds0_isr, 0xff);
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG0 | DSCM_STOP);
	delay(1000);

	/* Check cmd reg and fail if not right */
	if ((i = inb(hpc + ds_cmd)) != (DSCM_NODMA | DSCM_PG0 | DSCM_STOP)) {
		splx(s);
		return (0);
	}
#endif

	outb(hpc + hp_option, 0);

	splx(s);
	return (32);
}
/*
 * Fetch from onboard ROM/RAM
 */
hpfetch(ns, up, ad, len)
	struct hp_softc *ns;
	caddr_t up;
{
	u_char  cmd;
	register hpc = ns->ns_port;
	int     counter = 100000;

	outb(hpc + hp_option, inb(hpc + hp_option) | HP_DATA);

	cmd = inb(hpc + ds_cmd);
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG0 | DSCM_START);

	/* Setup remote dma */
	outb(hpc + ds0_isr, DSIS_RDC);

	if (ns->ns_mode & DSDC_WTS)
		len = (len + 3) & ~3;
	else
		len = (len + 1) & ~1;

	outb(hpc + ds0_rbcr0, len);
	outb(hpc + ds0_rbcr1, len >> 8);
	outb(hpc + ds0_rsar0, ad);
	outb(hpc + ds0_rsar1, ad >> 8);

#ifdef HP_DEBUG
	printf("hpfetch: len=%d ioaddr=0x%03x addr=0x%04x option=0x%02x %d-bit\n",
	    len, hpc + hp_data, ad, inb(hpc + hp_option),
	    ns->ns_mode & DSDC_WTS ? 32 : 16);
	printf("hpfetch: cmd=0x%02x isr=0x%02x ",
	    inb(hpc + ds_cmd), inb(hpc + ds0_isr));
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG2 | DSCM_START);
	printf("imr=0x%02x rcr=0x%02x tcr=0x%02x dcr=0x%02x\n",
	    inb(hpc + ds0_imr), inb(hpc + ds0_rcr), inb(hpc + ds0_tcr),
	    inb(hpc + ds0_dcr));
#endif

	/* Execute & extract from card */
	outb(hpc + ds_cmd, DSCM_RREAD | DSCM_PG0 | DSCM_START);

#ifdef HP_32BIT
	if (ns->ns_mode & DSDC_WTS)
		len = (caddr_t) insd(hpc + hp_data, up, len >> 2) - up;
	else
#endif
		len = (caddr_t) insw(hpc + hp_data, up, len >> 1) - up;

#ifdef HP_DEBUG
	printf("hpfetch: done len=%d\n", len);
#endif

	/* Wait till done, then shutdown feature */
	while ((inb(hpc + ds0_isr) & DSIS_RDC) == 0 && counter-- > 0);
	outb(hpc + ds0_isr, DSIS_RDC);
	outb(hpc + ds_cmd, cmd);

	outb(hpc + hp_option, inb(hpc + hp_option) & ~HP_DATA);
}
/*
 * Put to onboard RAM
 */
hpput(ns, up, ad, len)
	struct hp_softc *ns;
	caddr_t up;
{
	u_char  cmd;
	register hpc = ns->ns_port;
	int     counter = 100000;

	outb(hpc + hp_option, inb(hpc + hp_option) | HP_DATA);

	cmd = inb(hpc + ds_cmd);
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG0 | DSCM_START);

	/* Setup for remote dma */
	outb(hpc + ds0_isr, DSIS_RDC);

	if (ns->ns_mode & DSDC_WTS)
		len = (len + 3) & ~3;
	else
		len = (len + 1) & ~1;

#ifdef HP_DEBUG
	printf("hpput: len=%d ioaddr=0x%03x addr=0x%04x option=0x%02x %d-bit\n",
	    len, hpc + hp_data, ad, inb(hpc + hp_option),
	    ns->ns_mode & DSDC_WTS ? 32 : 16);
	printf("hpput: cmd=0x%02x isr=0x%02x ",
	    inb(hpc + ds_cmd), inb(hpc + ds0_isr));
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG2 | DSCM_START);
	printf("imr=0x%02x rcr=0x%02x tcr=0x%02x dcr=0x%02x\n",
	    inb(hpc + ds0_imr), inb(hpc + ds0_rcr), inb(hpc + ds0_tcr),
	    inb(hpc + ds0_dcr));
	{
		unsigned char *p = (unsigned char *) up;
		int     n = len;
		printf("hpput:");
		while (n--)
			printf(" %02x", *(p++));
		printf("\n");
	}
#endif

	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG0 | DSCM_START);
	outb(hpc + ds0_rbcr0, 0xff);
	outb(hpc + ds_cmd, DSCM_RREAD | DSCM_PG0 | DSCM_START);

	outb(hpc + ds0_rbcr0, len);
	outb(hpc + ds0_rbcr1, len >> 8);
	outb(hpc + ds0_rsar0, ad);
	outb(hpc + ds0_rsar1, ad >> 8);

	/* Execute & stuff to card */
	outb(hpc + ds_cmd, DSCM_RWRITE | DSCM_PG0 | DSCM_START);

#ifdef HP_32BIT
	if (ns->ns_mode & DSDC_WTS)
		len = (caddr_t) outsd(hpc + hp_data, up, len >> 2) - up;
	else
#endif
		len = (caddr_t) outsw(hpc + hp_data, up, len >> 1) - up;

#ifdef HP_DEBUG
	printf("hpput: done len=%d\n", len);
#endif

	/* Wait till done, then shutdown feature */
	while ((inb(hpc + ds0_isr) & DSIS_RDC) == 0 && counter-- > 0);
	outb(hpc + ds0_isr, DSIS_RDC);
	outb(hpc + ds_cmd, cmd);

	outb(hpc + hp_option, inb(hpc + hp_option) & ~HP_DATA);
}
/*
 * Reset of interface.
 */
hpreset(unit, uban)
	int     unit, uban;
{
	register struct hp_softc *ns = &hp_softc[unit];
	register hpc = ns->ns_port;
	if (unit >= NHP)
		return;
	printf("hp%d: reset\n", unit);
	outb(hpc + hp_option, 0);
	ns->ns_flags &= ~DSF_LOCK;
	hpinit(unit);
}

static char *
hp_id(type)
	u_char  type;
{
	static struct {
		u_char  type;
		char   *name;
	} boards[] = {
		{
			0x00, "hp27240"
		}, {
			0x10, "hp24240"
		}, {
			0x01, "hp27245"
		}, {
			0x02, "hp27250"
		}, {
			0x81, "hp27247"
		}, {
			0x91, "hp27247r1"
		}
	};
	int     n = sizeof(boards) / sizeof(boards[0]);

	while (n)
		if (boards[--n].type == type)
			return boards[n].name;

	return "UNKNOWN";
}
/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.  We get the ethernet address here.
 */
hpattach(dvp)
	struct isa_device *dvp;
{
	int     unit = dvp->id_unit;
	register struct hp_softc *ns = &hp_softc[unit];
	register struct ifnet *ifp = &ns->ns_if;

	ifp->if_unit = unit;
	ifp->if_name = hpdriver.name;
	printf("hp%d: %s %d-bit ethernet address %s\n", unit,
	    hp_id(ns->hp_type), ns->ns_mode & DSDC_WTS ? 32 : 16,
	    ether_sprintf(ns->ns_addrp));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	ifp->if_start = hpstart;
	ifp->if_ioctl = hpioctl;
	ifp->if_reset = hpreset;
	ifp->if_watchdog = 0;
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	if_alloc_sadl(ifp);
}
/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
hpinit(unit)
	int     unit;
{
	register struct hp_softc *ns = &hp_softc[unit];
	struct ifnet *ifp = &ns->ns_if;
	int     s;
	int     i;
	char   *cp;
	register hpc = ns->ns_port;

	if (ifp->if_addrlist == (struct ifaddr *) 0)
		return;
	if (ifp->if_flags & IFF_RUNNING)
		return;

	s = splnet();

#ifdef HP_DEBUG
	printf("hpinit: hp%d at 0x%x irq %d\n", unit, hpc, (int) ns->hp_irq);
	printf("hpinit: promiscuous mode %s\n",
	    ns->ns_if.if_flags & IFF_PROMISC ? "on" : "off");
#endif

	ns->ns_rcr = (ns->ns_if.if_flags & IFF_BROADCAST ? DSRC_AB : 0) |
	    (ns->ns_if.if_flags & IFF_PROMISC ? DSRC_PRO : 0);
#ifdef HP_LOG_ERRORS
	ns->ns_rcr |= DSRC_SEP;
#endif

	/* set irq and turn on board */
	outb(hpc + hp_option, HP_RUN | (ns->hp_irq << 1));

	/* init regs */
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG0 | DSCM_STOP);
	outb(hpc + ds0_dcr, 0);
	outb(hpc + ds0_rbcr0, 0);
	outb(hpc + ds0_rbcr1, 0);
	outb(hpc + ds0_rcr, DSRC_MON);
	outb(hpc + ds0_tpsr, ns->ns_txstart);
	outb(hpc + ds0_imr, 0);
	outb(hpc + ds0_tcr, DSTC_LB0);
	outb(hpc + ds0_pstart, ns->ns_rxstart);
	outb(hpc + ds0_bnry, ns->ns_rxend - 1);
	outb(hpc + ds0_pstop, ns->ns_rxend);
	outb(hpc + ds0_isr, 0xff);
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG1 | DSCM_STOP);
	outb(hpc + ds1_curr, ns->ns_rxstart);

	/* set physical address on ethernet */
	for (i = 0; i < 6; i++)
		outb(hpc + ds1_par0 + i, ns->ns_addrp[i]);

	/* clr logical address hash filter for now */
	for (i = 0; i < 8; i++)
		outb(hpc + ds1_mar0 + i, 0xff);

	/* fire it up */
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG0 | DSCM_START);
	outb(hpc + ds0_dcr, ns->ns_mode);
	outb(hpc + ds0_rcr, ns->ns_rcr);
	outb(hpc + ds0_tcr, 0);
	outb(hpc + ds0_imr, 0xff);

	ns->ns_if.if_flags |= IFF_RUNNING;
	ns->ns_flags &= ~DSF_LOCK;
	ns->ns_oactive = 0;
	ns->ns_mask = ~0;
	hpstart(ifp);

#ifdef HP_DEBUG
	printf("hpinit: done\n", unit, hpc);
#endif

	splx(s);
}
/*
 * Setup output on interface.
 * Get another datagram to send off of the interface queue,
 * and map it to the interface before starting the output.
 * called only at splnet or interrupt level.
 */
hpstart(ifp)
	struct ifnet *ifp;
{
	register struct hp_softc *ns = &hp_softc[ifp->if_unit];
	struct mbuf *m0, *m;
	int     buffer;
	int     len, i, total;
	register hpc = ns->ns_port;

	/*
	       * The DS8390 has only one transmit buffer, if it is busy we
	       * must wait until the transmit interrupt completes.
	       */
	if (ns->ns_flags & DSF_LOCK)
		return;

	if (inb(hpc + ds_cmd) & DSCM_TRANS)
		return;

	if ((ns->ns_if.if_flags & IFF_RUNNING) == 0)
		return;

	IFQ_DEQUEUE(&ns->ns_if.if_snd, m);

	if (m == 0)
		return;

	/*
	       * Copy the mbuf chain into the transmit buffer
	       */

	ns->ns_flags |= DSF_LOCK;	/* prevent entering hpstart */
	buffer = ns->ns_txstart * DS_PGSIZE;
	i = 0;
	total = len = m->m_pkthdr.len;

#ifdef HP_DEBUG
	printf("hpstart: len=%d\n", len);
#endif

#if NBPFILTER > 0
	if (ns->ns_bpf)
		bpf_mtap(ns->ns_bpf, m);
#endif

	for (m0 = m; m != 0;) {
		if (m->m_len & 1 && t > m->m_len) {
			m->m_len -= 1;
			hpput(ns, mtod(m, caddr_t), buffer, m->m_len);
			t -= m->m_len;
			buffer += m->m_len;
			m->m_data += m->m_len;
			m->m_len = 1;
			m = m_pullup(m, 2);
		} else {
			hpput(ns, mtod(m, caddr_t), buffer, m->m_len);
			t -= m->m_len;
			buffer += m->m_len;
			MFREE(m, m0);
			m = m0;
		}
	}

	/*
	       * Init transmit length registers, and set transmit start flag.
	       */
	len = total;
	if (len < ETHER_MIN_LEN)
		len = ETHER_MIN_LEN;
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG0 | DSCM_START);
	outb(hpc + ds0_tbcr0, len & 0xff);
	outb(hpc + ds0_tbcr1, (len >> 8) & 0xff);
	outb(hpc + ds0_tpsr, ns->ns_txstart);
	outb(hpc + ds_cmd, DSCM_TRANS | DSCM_NODMA | DSCM_PG0 | DSCM_START);

#ifdef HP_DEBUG
	printf("hpstart: done\n", hpc);
#endif
}
/*
 * Controller interrupt.
 */
hpintr(unit)
{
	register struct hp_softc *ns = &hp_softc[unit];
	u_char  cmd, isr;
	register hpc = ns->ns_port;
	u_char  err;

	/* Save cmd, clear interrupt */
	cmd = inb(hpc + ds_cmd);
loop:
	isr = inb(hpc + ds0_isr);
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG0 | DSCM_START);
	outb(hpc + ds0_isr, isr);

	/* Receiver error */
	if (isr & DSIS_RXE) {
		/* need to read these registers to clear status */
		err = inb(hpc + ds0_rsr);
		(void) inb(hpc + 0xD);
		(void) inb(hpc + 0xE);
		(void) inb(hpc + 0xF);
		ns->ns_if.if_ierrors++;
#ifdef HP_LOG_ERRORS
		isr |= DSIS_RX;
#endif
	}
	/* We received something */
	if (isr & DSIS_RX) {
		u_char  bnry;
		u_char  curr;
		u_short addr;
		int     len;
		int     i;
		unsigned char c;

		while (1) {
			outb(hpc + ds_cmd, DSCM_START | DSCM_NODMA | DSCM_PG0);
			bnry = inb(hpc + ds0_bnry);
			outb(hpc + ds_cmd, DSCM_START | DSCM_NODMA | DSCM_PG1);
			curr = inb(hpc + ds1_curr);

#ifdef HP_DEBUG
			printf("hpintr: receive isr=0x%02x bnry=0x%02x curr=0x%02x\n",
			    isr, bnry, curr);
#endif

			if (++bnry >= ns->ns_rxend)
				bnry = ns->ns_rxstart;

			/* if ring empty, done! */
			if (bnry == curr)
				break;

			addr = bnry * DS_PGSIZE;

			outb(hpc + hp_option, inb(hpc + hp_option) | HP_DATA);

#if 0
			/* send packet with auto packet release */
			outb(hpc + ds_cmd, DSCM_START | DSCM_NODMA | DSCM_PG0);
			outb(hpc + ds0_rbcr1, 0x0f);
			outb(hpc + ds0_dcr, ns->ns_mode | DSDC_AR);
			outb(hpc + ds_cmd, DSCM_SENDP | DSCM_PG0 | DSCM_START);
#endif

			/* get length */
			hpfetch(ns, (caddr_t) & ns->ns_ph, addr, sizeof ns->ns_ph);
			addr += sizeof ns->ns_ph;

#ifdef HP_DEBUG
			printf("hpintr: sendp packet hdr: %x %x %x %x\n",
			    ns->ns_ph.pr_status,
			    ns->ns_ph.pr_nxtpg,
			    ns->ns_ph.pr_sz0,
			    ns->ns_ph.pr_sz1);
#endif

#ifdef HP_LOG_ERRORS
			if (ns->ns_ph.pr_status & (DSRS_CRC | DSRS_FO | DSRS_DFR)) {
				/* Get packet header */
				if (len > 14)
					len = 14;
				hpfetch(ns, (caddr_t) (ns->ns_pb), addr, len);

				/* move boundary up */
				bnry = ns->ns_ph.pr_nxtpg;
				if (--bnry < ns->ns_rxstart)
					bnry = ns->ns_rxend - 1;
				outb(hpc + ds_cmd, DSCM_START | DSCM_NODMA | DSCM_PG0);
				outb(hpc + ds0_bnry, bnry);

				printf("hp%d: receive error status=0x%02x\n", unit,
				    ns->ns_ph.pr_status);
				printf("hp%d: packet header:", unit);
				{
					int     n;
					for (n = 0; n < len; n++)
						printf(" %02x", ns->ns_pb[n]);
				}
				printf("\n");

				continue;
			}
#endif

			ns->ns_if.if_ipackets++;
			len = ns->ns_ph.pr_sz0 + (ns->ns_ph.pr_sz1 << 8);
			if (len < ETHER_MIN_LEN || len > ETHER_MAX_DIX_LEN) {
				printf("hpintr: bnry %x curr %x\n", bnry, curr);
				printf("hpintr: packet hdr: %x %x %x %x\n",
				    ns->ns_ph.pr_status,
				    ns->ns_ph.pr_nxtpg,
				    ns->ns_ph.pr_sz0,
				    ns->ns_ph.pr_sz1);
				printf("isr = 0x%x reg_isr=0x%x\n",
				    isr, inb(hpc + ds0_isr));
				outb(hpc + ds_cmd, DSCM_START | DSCM_NODMA | DSCM_PG0);
				bnry = inb(hpc + ds0_bnry);
				outb(hpc + ds_cmd, DSCM_START | DSCM_NODMA | DSCM_PG1);
				curr = inb(hpc + ds1_curr);
				printf("hpintr: new bnry %x curr %x\n", bnry, curr);
				printf("hpintr: bad len %d\n-hanging-\n",
				    len);
				while (1);
			}
			/* read packet */
			hpfetch(ns, (caddr_t) (ns->ns_pb), addr, len);

			/* move boundary up */
			bnry = ns->ns_ph.pr_nxtpg;
			if (--bnry < ns->ns_rxstart)
				bnry = ns->ns_rxend - 1;
			outb(hpc + ds_cmd, DSCM_START | DSCM_NODMA | DSCM_PG0);
			outb(hpc + ds0_bnry, bnry);

#ifdef HP_DEBUG
			printf("hpintr: receive done bnry=0x%02x\n", bnry);
#endif

			outb(hpc + hp_option, inb(hpc + hp_option) & ~HP_DATA);

			/* adjust for ether header and checksum */
			len -= sizeof(struct ether_header) + sizeof(long);

			/* process packet */
			hpread(ns, (caddr_t) (ns->ns_pb), len);
		}
	}
	/* Transmit error */
	if (isr & DSIS_TXE) {
		ns->ns_flags &= ~DSF_LOCK;
		/* Need to read these registers to clear status */
		ns->ns_if.if_collisions += inb(hpc + ds0_tbcr0);
		ns->ns_if.if_oerrors++;
	}
	/* Packet Transmitted */
	if (isr & DSIS_TX) {
		ns->ns_flags &= ~DSF_LOCK;
		++ns->ns_if.if_opackets;
		ns->ns_if.if_collisions += inb(hpc + ds0_tbcr0);
	}
	/* Receiver ovverun? */
	if (isr & DSIS_ROVRN) {
		log(LOG_ERR, "hp%d: error: isr %x\n", ns - hp_softc, isr
		     /* , DSIS_BITS */ );
		outb(hpc + ds0_rbcr0, 0);
		outb(hpc + ds0_rbcr1, 0);
		outb(hpc + ds0_tcr, DSTC_LB0);
		outb(hpc + ds0_rcr, DSRC_MON);
		outb(hpc + ds_cmd, DSCM_START | DSCM_NODMA);
		outb(hpc + ds0_rcr, ns->ns_rcr);
		outb(hpc + ds0_tcr, 0);
	}
	/* Any more to send? */
	outb(hpc + ds_cmd, DSCM_NODMA | DSCM_PG0 | DSCM_START);
	hpstart(&ns->ns_if);
	outb(hpc + ds_cmd, cmd);
	outb(hpc + ds0_imr, 0xff);

	/* Still more to do? */
	isr = inb(hpc + ds0_isr);
	if (isr)
		goto loop;
}
/*
 * Pass a packet to the higher levels.
 * We deal with the trailer protocol here.
 */
hpread(ns, buf, len)
	register struct hp_softc *ns;
	char   *buf;
	int     len;
{
	register struct ether_header *eh;
	struct mbuf *m;
	int     off, resid;
	register struct ifqueue *inq;
	u_short etype;

	/*
	 * Deal with trailer protocol: if type is trailer type
	 * get true type from first 16-bit word past data.
	 * Remember that type was trailer by setting off.
	 */
	eh = (struct ether_header *) buf;
	etype = ntohs((u_short) eh->ether_type);
#define	hpdataaddr(eh, off, type)	((type)(((caddr_t)((eh)+1)+(off))))
	if (etype >= ETHERTYPE_TRAIL &&
	    etype < ETHERTYPE_TRAIL + ETHERTYPE_NTRAILER) {
		off = (etype - ETHERTYPE_TRAIL) * 512;
		if (off >= ETHERMTU)
			return;	/* sanity */
		eh->ether_type = *hpdataaddr(eh, off, u_short *);
		resid = ntohs(*(hpdataaddr(eh, off + 2, u_short *)));
		if (off + resid > len)
			return;	/* sanity */
		len = off + resid;
	} else
		off = 0;

	if (len == 0)
		return;

#if NBPFILTER > 0
	if (ns->ns_bpf)
		bpf_tap(ns->ns_bpf, buf, len + sizeof(struct ether_header));
#endif
	/*
	       * Pull packet off interface.  Off is nonzero if packet
	       * has trailing header; hpget will then force this header
	       * information to be at the front, but we still have to drop
	       * the type and length which are at the front of any trailer data.
	       */
	m = hpget(buf, len, off, &ns->ns_if);
	if (m == 0)
		return;

	ether_input(&ns->ns_if, eh, m);
}
/*
 * Supporting routines
 */

/*
 * Pull read data off a interface.
 * Len is length of data, with local net header stripped.
 * Off is non-zero if a trailer protocol was used, and
 * gives the offset of the trailer information.
 * We copy the trailer information and then all the normal
 * data into mbufs.  When full cluster sized units are present
 * we copy into clusters.
 */
struct mbuf *
hpget(buf, totlen, off0, ifp)
	caddr_t buf;
	int     totlen, off0;
	struct ifnet *ifp;
{
	struct mbuf *top, **mp, *m, *p;
	int     off = off0, len;
	register caddr_t cp = buf;
	char   *epkt;

	buf += sizeof(struct ether_header);
	cp = buf;
	epkt = cp + totlen;


	if (off) {
		cp += off + 2 * sizeof(u_short);
		totlen -= 2 * sizeof(u_short);
	}
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	m->m_len = MHLEN;

	top = 0;
	mp = &top;
	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			m->m_len = MLEN;
		}
		len = min(totlen, epkt - cp);
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
		totlen -= len;
		if (cp == epkt)
			cp = buf;
	}
	return (top);
}
/*
 * Process an ioctl request.
 */
hpioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long	cmd;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *) data;
	struct hp_softc *ns = &hp_softc[ifp->if_unit];
	struct ifreq *ifr = (struct ifreq *) data;
	int     s = splnet(), error = 0;

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
			hpinit(ifp->if_unit);	/* before arpwhohas */
			((struct arpcom *) ifp)->ac_ipaddr =
			    IA_SIN(ifa)->sin_addr;
			arpwhohas((struct arpcom *) ifp, &IA_SIN(ifa)->sin_addr);
			break;
#endif
		default:
			hpinit(ifp->if_unit);
			break;
		}
		break;

	case SIOCSIFFLAGS:
#ifdef HP_DEBUG
		printf("hp: setting flags, up: %s, running: %s\n",
		    ifp->if_flags & IFF_UP ? "yes" : "no",
		    ifp->if_flags & IFF_RUNNING ? "yes" : "no");
#endif
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    ifp->if_flags & IFF_RUNNING) {
			ifp->if_flags &= ~IFF_RUNNING;
			outb(ns->ns_port + ds_cmd, DSCM_STOP | DSCM_NODMA);
		} else
			if (ifp->if_flags & IFF_UP &&
			    (ifp->if_flags & IFF_RUNNING) == 0)
				hpinit(ifp->if_unit);
		break;

#ifdef notdef
	case SIOCGHWADDR:
		bcopy((caddr_t) ns->ns_addrp, (caddr_t) & ifr->ifr_data,
		    sizeof(ns->ns_addrp));
		break;
#endif

	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}
#endif
