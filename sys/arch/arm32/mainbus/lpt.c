/* $NetBSD: lpt.c,v 1.6 1996/03/28 21:52:47 mark Exp $ */

/*
 * Copyright (c) 1995 Mark Brinicombe
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1990 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from:$NetBSD: lpt.c,v 1.6 1996/03/28 21:52:47 mark Exp $
 */

/*
 * Device Driver for AT parallel printer port
 */
/*
 * PLIP driver code added by Mark Brinicombe
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/device.h>
#include <sys/syslog.h>

#if defined(INET) && defined(PLIP)
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include "bpfilter.h"
#if NBPFILTER > 0
#include <sys/time.h>
#include <net/bpf.h>
#endif
#endif

#include <machine/cpu.h>
#include <machine/katelib.h>
#include <machine/irqhandler.h>
#include <machine/io.h>
#include <arm32/mainbus/lptreg.h>
#include <arm32/mainbus/mainbus.h>

#define	TIMEOUT		hz*16	/* wait up to 16 seconds for a ready */
#define	STEP		hz/4

#define	LPTPRI		(PZERO+8)
#define	LPT_BSIZE	1024

#if defined(INET) && defined(PLIP)
#ifndef PLIPMTU                 /* MTU for the plip# interfaces */
#if defined(COMPAT_PLIP10)
#define PLIPMTU         1600
#else
#define PLIPMTU         (ETHERMTU - ifp->if_hdrlen)
#endif
#endif

#ifndef PLIPMXSPIN1             /* DELAY factor for the plip# interfaces */
#define PLIPMXSPIN1     2000    /* Spinning for remote intr to happen */
#endif

#ifndef PLIPMXSPIN2             /* DELAY factor for the plip# interfaces */
#define PLIPMXSPIN2     6000    /* Spinning for remote handshake to happen */
#endif

#ifndef PLIPMXERRS              /* Max errors before !RUNNING */
#define PLIPMXERRS      100
#endif
#ifndef PLIPMXRETRY
#define PLIPMXRETRY     10      /* Max number of retransmits */
#endif
#ifndef PLIPRETRY
#define PLIPRETRY       hz/10   /* Time between retransmits */
#endif
#endif

#if !defined(DEBUG) || !defined(notdef)
#define lprintf
#else
#define lprintf		if (lptdebug) printf
int lptdebug = 1;
#endif

struct lpt_softc {
	struct device sc_dev;
	irqhandler_t sc_ih;

	size_t sc_count;
	struct buf *sc_inbuf;
	u_char *sc_cp;
	int sc_spinmax;
	int sc_iobase;
	int sc_irq;
	u_char sc_state;
#define	LPT_OPEN	0x01	/* device is open */
#define	LPT_OBUSY	0x02	/* printer is busy doing output */
#define	LPT_INIT	0x04	/* waiting to initialize for open */
#define LPT_PLIP	0x08	/* busy with PLIP */
	u_char sc_flags;
#define	LPT_AUTOLF	0x20	/* automatic LF on CR */
#define	LPT_NOPRIME	0x40	/* don't prime on open */
#define	LPT_NOINTR	0x80	/* do not use interrupt */
	u_char sc_control;
	u_char sc_laststatus;
#if defined(INET) && defined(PLIP)
	struct arpcom	sc_arpcom;
	u_char		*sc_ifbuf;
	int		sc_iferrs;
	int		sc_ifretry;
#if defined(COMPAT_PLIP10)
	u_char		sc_adrcksum;
#endif
#endif
};

int lptprobe __P((struct device *, void *, void *));
void lptattach __P((struct device *, struct device *, void *));
int lptintr __P((void *));

#if defined(INET) && defined(PLIP)
/* Functions for the plip# interface */
static void plipattach(struct lpt_softc *,int);
static int plipioctl(struct ifnet *, u_long, caddr_t);
static void plipstart(struct ifnet *);
static int plipintr(struct lpt_softc *);
#endif

struct cfattach lpt_ca = {
	sizeof(struct lpt_softc), lptprobe, lptattach
};

struct cfdriver lpt_cd = {
	NULL, "lpt", DV_TTY
};

#define	LPTUNIT(s)	(minor(s) & 0x1f)
#define	LPTFLAGS(s)	(minor(s) & 0xe0)

#define	LPS_INVERT	(LPS_SELECT|LPS_NERR|LPS_NBSY|LPS_NACK)
#define	LPS_MASK	(LPS_SELECT|LPS_NERR|LPS_NBSY|LPS_NACK|LPS_NOPAPER)
#define	NOT_READY()	((inb(iobase + lpt_status) ^ LPS_INVERT) & LPS_MASK)
#define	NOT_READY_ERR()	not_ready(inb(iobase + lpt_status), sc)
static int not_ready __P((u_char, struct lpt_softc *));

static void lptwakeup __P((void *arg));
static int pushbytes __P((struct lpt_softc *));

/*
 * Internal routine to lptprobe to do port tests of one byte value.
 */
int
lpt_port_test(port, data, mask)
	int port;
	u_char data, mask;
{
	int timeout;
	u_char temp;

	data &= mask;
	outb(port, data);
	timeout = 1000;
	do {
		delay(10);
		temp = inb(port) & mask;
	} while (temp != data && --timeout);
	lprintf("lpt: port=0x%x out=0x%x in=0x%x timeout=%d\n", port, data,
	    temp, timeout);
	return (temp == data);
}

/*
 * Logic:
 *	1) You should be able to write to and read back the same value
 *	   to the data port.  Do an alternating zeros, alternating ones,
 *	   walking zero, and walking one test to check for stuck bits.
 *
 *	2) You should be able to write to and read back the same value
 *	   to the control port lower 5 bits, the upper 3 bits are reserved
 *	   per the IBM PC technical reference manauls and different boards
 *	   do different things with them.  Do an alternating zeros, alternating
 *	   ones, walking zero, and walking one test to check for stuck bits.
 *
 *	   Some printers drag the strobe line down when the are powered off
 * 	   so this bit has been masked out of the control port test.
 *
 *	   XXX Some printers may not like a fast pulse on init or strobe, I
 *	   don't know at this point, if that becomes a problem these bits
 *	   should be turned off in the mask byte for the control port test.
 *
 *	3) Set the data and control ports to a value of 0
 */
int
lptprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct mainbus_attach_args *mb = aux;
	int iobase = mb->mb_iobase;
	int port;
	u_char mask, data;
	int i;

#ifdef DEBUG
#define	ABORT	do {printf("lptprobe: mask %x data %x failed\n", mask, data); \
		    return 0;} while (0)
#else
#define	ABORT	return 0
#endif

	port = iobase + lpt_data;
	mask = 0xff;

	data = 0x55;				/* Alternating zeros */
	if (!lpt_port_test(port, data, mask))
		ABORT;

	data = 0xaa;				/* Alternating ones */
	if (!lpt_port_test(port, data, mask))
		ABORT;

	for (i = 0; i < CHAR_BIT; i++) {	/* Walking zero */
		data = ~(1 << i);
		if (!lpt_port_test(port, data, mask))
			ABORT;
	}

	for (i = 0; i < CHAR_BIT; i++) {	/* Walking one */
		data = (1 << i);
		if (!lpt_port_test(port, data, mask))
			ABORT;
	}

	outb(iobase + lpt_data, 0);
	outb(iobase + lpt_control, 0);

	mb->mb_iosize = LPT_NPORTS;
	return 1;
}

void
lptattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lpt_softc *sc = (void *)self;
	struct mainbus_attach_args *mb = aux;
	int iobase = mb->mb_iobase;

	if (mb->mb_irq != IRQUNK)
		printf("\n");
	else
		printf(": polled\n");

	sc->sc_iobase = iobase;
	sc->sc_irq = mb->mb_irq;
	sc->sc_state = 0;
	outb(iobase + lpt_control, LPC_NINIT);

	if (mb->mb_irq != IRQUNK) {
		sc->sc_ih.ih_func = lptintr;
		sc->sc_ih.ih_arg = sc;
#if defined(INET) && defined(PLIP)
		sc->sc_ih.ih_level = IPL_NET;
		sc->sc_ih.ih_name = "lpt/plip";
        	plipattach(sc, self->dv_unit);
#else
		sc->sc_ih.ih_level = IPL_NONE;
		sc->sc_ih.ih_name = "lpt";
#endif
		if (irq_claim(mb->mb_irq, &sc->sc_ih))
			panic("Cannot claim IRQ %d for lpt%d", mb->mb_irq, sc->sc_dev.dv_unit);

	}
#if defined(INET) && defined(PLIP)
	else {
		printf("Warning PLIP device needs IRQ driven lpt driver\n");
	}
#endif
}

/*
 * Reset the printer, then wait until it's selected and not busy.
 */
int
lptopen(dev, flag)
	dev_t dev;
	int flag;
{
	int unit = LPTUNIT(dev);
	u_char flags = LPTFLAGS(dev);
	struct lpt_softc *sc;
	int iobase;
	u_char control;
	int error;
	int spin;

	if (unit >= lpt_cd.cd_ndevs)
		return ENXIO;
	sc = lpt_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (sc->sc_irq == IRQUNK && (flags & LPT_NOINTR) == 0)
		return ENXIO;

#ifdef DIAGNOSTIC
	if (sc->sc_state)
		printf("%s: stat=0x%x not zero\n", sc->sc_dev.dv_xname,
		    sc->sc_state);
#endif

	if (sc->sc_state)
		return EBUSY;

	sc->sc_state = LPT_INIT;
	sc->sc_flags = flags;
	lprintf("%s: open: flags=0x%x\n", sc->sc_dev.dv_xname, flags);
	iobase = sc->sc_iobase;

	if ((flags & LPT_NOPRIME) == 0) {
		/* assert INIT for 100 usec to start up printer */
		outb(iobase + lpt_control, LPC_SELECT);
		delay(100);
	}

	control = LPC_SELECT | LPC_NINIT;
	outb(iobase + lpt_control, control);

	/* wait till ready (printer running diagnostics) */
	for (spin = 0; NOT_READY_ERR(); spin += STEP) {
		if (spin >= TIMEOUT) {
			sc->sc_state = 0;
			return EBUSY;
		}

		/* wait 1/4 second, give up if we get a signal */
		if (error = tsleep((caddr_t)sc, LPTPRI | PCATCH, "lptopen",
		    STEP) != EWOULDBLOCK) {
			sc->sc_state = 0;
			return error;
		}
	}

	if ((flags & LPT_NOINTR) == 0)
		control |= LPC_IENABLE;
	if (flags & LPT_AUTOLF)
		control |= LPC_AUTOLF;
	sc->sc_control = control;
	outb(iobase + lpt_control, control);

	sc->sc_inbuf = geteblk(LPT_BSIZE);
	sc->sc_count = 0;
	sc->sc_state = LPT_OPEN;

	if ((sc->sc_flags & LPT_NOINTR) == 0)
		lptwakeup(sc);

	lprintf("%s: opened\n", sc->sc_dev.dv_xname);
	return 0;
}

int
not_ready(status, sc)
	u_char status;
	struct lpt_softc *sc;
{
	u_char new;

	status = (status ^ LPS_INVERT) & LPS_MASK;
	new = status & ~sc->sc_laststatus;
	sc->sc_laststatus = status;

	if (new & LPS_SELECT)
		log(LOG_NOTICE, "%s: offline\n", sc->sc_dev.dv_xname);
	else if (new & LPS_NOPAPER)
		log(LOG_NOTICE, "%s: out of paper\n", sc->sc_dev.dv_xname);
	else if (new & LPS_NERR)
		log(LOG_NOTICE, "%s: output error\n", sc->sc_dev.dv_xname);

	return status;
}


void
lptwakeup(arg)
	void *arg;
{
	struct lpt_softc *sc = arg;
	int s;

	s = spltty();
	lptintr(sc);
	splx(s);

	timeout(lptwakeup, sc, STEP);
}

/*
 * Close the device, and free the local line buffer.
 */
lptclose(dev, flag)
	dev_t dev;
	int flag;
{
	int unit = LPTUNIT(dev);
	struct lpt_softc *sc = lpt_cd.cd_devs[unit];
	int iobase = sc->sc_iobase;

	if (sc->sc_count)
		(void) pushbytes(sc);

	if ((sc->sc_flags & LPT_NOINTR) == 0)
		untimeout(lptwakeup, sc);

	outb(iobase + lpt_control, LPC_NINIT);
	sc->sc_state = 0;
	outb(iobase + lpt_control, LPC_NINIT);
	brelse(sc->sc_inbuf);

	lprintf("%s: closed\n", sc->sc_dev.dv_xname);
	return 0;
}

int
pushbytes(sc)
	struct lpt_softc *sc;
{
	int iobase = sc->sc_iobase;
	int error;

	if (sc->sc_flags & LPT_NOINTR) {
		int spin, tic;
		u_char control = sc->sc_control;

		while (sc->sc_count > 0) {
			spin = 0;
			while (NOT_READY()) {
				if (++spin < sc->sc_spinmax)
					continue;
				tic = 0;
				/* adapt busy-wait algorithm */
				sc->sc_spinmax++;
				while (NOT_READY_ERR()) {
					/* exponential backoff */
					tic = tic + tic + 1;
					if (tic > TIMEOUT)
						tic = TIMEOUT;
					error = tsleep((caddr_t)sc,
					    LPTPRI | PCATCH, "lptpsh", tic);
					if (error != EWOULDBLOCK)
						return error;
				}
				break;
			}

			outb(iobase + lpt_data, *sc->sc_cp++);
			outb(iobase + lpt_control, control | LPC_STROBE);
			sc->sc_count--;
			outb(iobase + lpt_control, control);

			/* adapt busy-wait algorithm */
			if (spin*2 + 16 < sc->sc_spinmax)
				sc->sc_spinmax--;
		}
	} else {
		int s;

		while (sc->sc_count > 0) {
			/* if the printer is ready for a char, give it one */
			if ((sc->sc_state & LPT_OBUSY) == 0) {
				lprintf("%s: write %d\n", sc->sc_dev.dv_xname,
				    sc->sc_count);
				s = spltty();
				(void) lptintr(sc);
				splx(s);
			}
			if (error = tsleep((caddr_t)sc, LPTPRI | PCATCH,
			    "lptwrite2", 0))
				return error;
		}
	}
	return 0;
}

/* 
 * Copy a line from user space to a local buffer, then call putc to get the
 * chars moved to the output queue.
 */
lptwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	struct lpt_softc *sc = lpt_cd.cd_devs[LPTUNIT(dev)];
	size_t n;
	int error = 0;

	while (n = min(LPT_BSIZE, uio->uio_resid)) {
		uiomove(sc->sc_cp = sc->sc_inbuf->b_data, n, uio);
		sc->sc_count = n;
		error = pushbytes(sc);
		if (error) {
			/*
			 * Return accurate residual if interrupted or timed
			 * out.
			 */
			uio->uio_resid += sc->sc_count;
			sc->sc_count = 0;
			return error;
		}
	}
	return 0;
}

/*
 * Handle printer interrupts which occur when the printer is ready to accept
 * another char.
 */
int
lptintr(arg)
	void *arg;
{
	struct lpt_softc *sc = arg;
	int iobase = sc->sc_iobase;

/*printf("lptintr:\n");*/

#if defined(INET) && defined(PLIP)
	if (sc->sc_arpcom.ac_if.if_flags & IFF_UP) {
		return(plipintr(sc));
	}
#endif

#if 0
	if ((sc->sc_state & LPT_OPEN) == 0)
		return 0;
#endif

	/* is printer online and ready for output */
	if (NOT_READY() && NOT_READY_ERR())
		return 0;

	if (sc->sc_count) {
		u_char control = sc->sc_control;
		/* send char */
		outb(iobase + lpt_data, *sc->sc_cp++);
		outb(iobase + lpt_control, control | LPC_STROBE);
		sc->sc_count--;
		outb(iobase + lpt_control, control);
		sc->sc_state |= LPT_OBUSY;
	} else
		sc->sc_state &= ~LPT_OBUSY;

	if (sc->sc_count == 0) {
		/* none, wake up the top half to get more */
		wakeup((caddr_t)sc);
	}

	return(1);
}

int
lptioctl(dev, cmd, data, flag)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
{
	int error = 0;

	switch (cmd) {
	default:
		error = ENODEV;
	}

	return error;
}


#if defined(INET) && defined(PLIP)

#define PLIP_INTR_ENABLE	(LPC_NINIT | LPC_SELECT | LPC_IENABLE)
#define PLIP_INTR_DISABLE	(LPC_NINIT | LPC_SELECT)
#define PLIP_DATA		(iobase + lpt_data)
#define PLIP_STATUS		(iobase + lpt_status)
#define PLIP_CONTROL		(iobase + lpt_control)
#define PLIP_REMOTE_TRIGGER	0x08
#define PLIP_DELAY_UNIT		50
#if PLIP_DELAY_UNIT > 0	
#define PLIP_DELAY		DELAY(PLIP_DELAY_UNIT)
#else
#define PLIP_DELAY
#endif
#define PLIP_DEBUG_RX	0x01
#define PLIP_DEBUG_TX	0x02
#define PLIP_DEBUG_IF	0x04
#define PLIP_DEBUG	0x07
#if PLIP_DEBUG != 0
static int plip_debug = PLIP_DEBUG;
#endif

static void
plipattach(struct lpt_softc *sc, int unit)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	sc->sc_ifbuf = NULL;
	ifp->if_unit = unit;
	ifp->if_name = "plip";
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS;
	ifp->if_output = ether_output;
	ifp->if_start = plipstart;
	ifp->if_ioctl = plipioctl;
	ifp->if_watchdog = 0;

	ifp->if_type = IFT_ETHER;
	ifp->if_addrlen = 6;
	ifp->if_hdrlen = 14;
	ifp->if_mtu = PLIPMTU;

	if_attach(ifp);
	ether_ifattach(ifp);

	printf("plip%d at lpt%d: mtu=%d,%d,%d", unit, unit, (int) ifp->if_mtu,
	    ifp->if_hdrlen, ifp->if_addrlen);
	if (sizeof(struct ether_header) != 14)
		printf(" ethhdr super kludge mode enabled\n");
	else
		printf("\n");

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif                 
}

/*
 * Process an ioctl request.
 */
static int
plipioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct proc *p = curproc;
	struct lpt_softc *sc = (struct lpt_softc *) lpt_cd.cd_devs[ifp->if_unit];
	unsigned int iobase = sc->sc_iobase;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data; 
	int s;
	int error = 0;

#if PLIP_DEBUG > 0
	printf("plipioctl: cmd=%08x ifp=%08x data=%08x\n", cmd, ifp, data);
	printf("plipioctl: ifp->flags=%08x\n", ifp->if_flags);
#endif

	switch (cmd) {

	case SIOCSIFFLAGS:
		if (((ifp->if_flags & IFF_UP) == 0) &&
		    (ifp->if_flags & IFF_RUNNING)) {
		        ifp->if_flags &= ~IFF_RUNNING;
/* Deactive the parallel port */
#if PLIP_DEBUG != 0
			if (plip_debug & PLIP_DEBUG_IF)
				printf("plip: Disabling lpt irqs\n");
#endif
			outb(PLIP_DATA, 0x00);
			outb(PLIP_CONTROL, PLIP_INTR_DISABLE);
			sc->sc_state = 0;
			         
			if (sc->sc_ifbuf)
				free(sc->sc_ifbuf, M_DEVBUF);
				
			sc->sc_ifbuf = NULL;
		}
		if (((ifp->if_flags & IFF_UP)) &&
		    ((ifp->if_flags & IFF_RUNNING) == 0)) {
			if (sc->sc_state) {
				error = EBUSY;
				break;
			}
/*			if (!(ifp->if_flags & IFF_DEBUG))
				plip_debug = PLIP_DEBUG;
			else
				plip_debug = 0;*/
			sc->sc_state = LPT_OPEN | LPT_PLIP;
			if (!sc->sc_ifbuf)
				sc->sc_ifbuf =
					malloc(ifp->if_mtu + ifp->if_hdrlen,
					       M_DEVBUF, M_WAITOK);
		        ifp->if_flags |= IFF_RUNNING;
/* This starts it running */
/* Enable lpt interrupts */

#if PLIP_DEBUG != 0
			if (plip_debug & PLIP_DEBUG_IF)
				printf("plip: Enabling lpt irqs\n");
#endif
			outb(PLIP_CONTROL, PLIP_INTR_ENABLE);
			outb(PLIP_DATA, 0x00);
		}
		break;

	case SIOCSIFADDR:
		if (ifa->ifa_addr->sa_family == AF_INET) {
			if (!sc->sc_ifbuf)
				sc->sc_ifbuf =
					malloc(PLIPMTU + ifp->if_hdrlen,
					       M_DEVBUF, M_WAITOK);

			sc->sc_arpcom.ac_enaddr[0] = 0xfc;
			sc->sc_arpcom.ac_enaddr[1] = 0xfc;
			bcopy((caddr_t)&IA_SIN(ifa)->sin_addr,
			      (caddr_t)&sc->sc_arpcom.ac_enaddr[2], 4);
			sc->sc_arpcom.ac_ipaddr = IA_SIN(ifa)->sin_addr;
#if defined(COMPAT_PLIP10)
			if (ifp->if_flags & IFF_LINK0) {
				int i;
				sc->sc_arpcom.ac_enaddr[0] = 0xfd;
				sc->sc_arpcom.ac_enaddr[1] = 0xfd;
				for (i = sc->sc_adrcksum = 0; i < 5; i++)
					sc->sc_adrcksum += sc->sc_arpcom.ac_enaddr[i];
				sc->sc_adrcksum *= 2;
			}
#endif
			ifp->if_flags |= IFF_RUNNING | IFF_UP;
#if 0
			for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
				struct sockaddr_dl *sdl;
				if ((sdl = (struct sockaddr_dl *)ifa->ifa_addr) &&
				    sdl->sdl_family == AF_LINK) {
					sdl->sdl_type = IFT_ETHER;
					sdl->sdl_alen = ifp->if_addrlen;
					bcopy((caddr_t)((struct arpcom *)ifp)->ac_enaddr,
					      LLADDR(sdl), ifp->if_addrlen);
					break;
				    }
			}
#endif
/* Looks the same as the start condition above */
/* Enable lpt interrupts */

#if PLIP_DEBUG != 0
			if (plip_debug & PLIP_DEBUG_IF)
				printf("plip: Enabling lpt irqs\n");
#endif
			outb(PLIP_CONTROL, PLIP_INTR_ENABLE);
			outb(PLIP_DATA, 0x00);

			arp_ifinit(&sc->sc_arpcom, ifa);
		} else
			error = EAFNOSUPPORT;
		break;

	case SIOCAIFADDR:
	case SIOCDIFADDR:
	case SIOCSIFDSTADDR:
		if (ifa->ifa_addr->sa_family != AF_INET)
			error = EAFNOSUPPORT;
		break;

	case SIOCSIFMTU:
        	if ((error = suser(p->p_ucred, &p->p_acflag)))
            		return(error);
		if (ifp->if_mtu != ifr->ifr_metric) {
		        ifp->if_mtu = ifr->ifr_metric;
			if (sc->sc_ifbuf) {
				s = splimp();

				free(sc->sc_ifbuf, M_DEVBUF);
				sc->sc_ifbuf =
					malloc(ifp->if_mtu + ifp->if_hdrlen,
					       M_DEVBUF, M_WAITOK);
				splx(s);
			}
		}
		break;

        case SIOCGIFMTU:
	        ifr->ifr_metric = ifp->if_mtu;
		break;

	default:
		error = EINVAL;
	}
	return (error);
}

static int
plipreceive(unsigned int iobase, u_char *buf, int len)
{
	int i;
	u_char cksum = 0, c;
	u_char c0, c1;

#if PLIP_DEBUG != 0
	if (plip_debug & PLIP_DEBUG_RX)
		printf("Rx: ");
#endif

	while (len--) {
		i = PLIPMXSPIN2;
/* Receive a byte */

/* Wait for a steady handshake */
		while (1) {
			c0 = inb(PLIP_STATUS);
			if ((c0 & LPS_NBSY) == 0) {
				c1 = inb(PLIP_STATUS);
				if (c0 == c1) break;
#if PLIP_DEBUG != 0
				if (plip_debug & PLIP_DEBUG_RX)
					printf("rx: %02x-%02x ", c0, c1);
#endif
			}
			--i;
			if (i < 0) {
#if PLIP_DEBUG > 0
				printf("timeout rx lsn %02x\n", c0);
#endif
				return(-1);
			}
			PLIP_DELAY;
		}
		c = (c0 >> 3) & 0x0f;

/* Acknowledge */
		outb(PLIP_DATA, 0x10);

/* Another handshake */		
		i = PLIPMXSPIN2;
		while (1) {
			c0 = inb(PLIP_STATUS);
			if (c0 & LPS_NBSY) {
				c1 = inb(PLIP_STATUS);
				if (c0 == c1) break;
#if PLIP_DEBUG != 0
				if (plip_debug & PLIP_DEBUG_RX)
					printf("rx: %02x-%02x ", c0, c1);
#endif
			}
			--i;
			if (i < 0) {
#if PLIP_DEBUG > 0
				printf("timeout rx msn %02x\n", c0);
#endif
				return(-1);
			}
			PLIP_DELAY;
		}
		c = c | ((c0 << 1) & 0xf0);
/* Acknowledge */
		outb(PLIP_DATA, 0x00);
#if PLIP_DEBUG != 0
		if (plip_debug & PLIP_DEBUG_RX)
			printf("%02x ", c);
#endif

		cksum += (*buf++ = c);
	}
#if PLIP_DEBUG != 0
	if (plip_debug & PLIP_DEBUG_RX)
		printf("\n");
#endif
	return(cksum);
}

static int
plipintr(struct lpt_softc *sc)
{
	extern struct mbuf *m_devget(char *, int, int, struct ifnet *, void (*)());
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	unsigned int iobase = sc->sc_iobase;
	struct mbuf *m;
	struct ether_header *eh;
	u_char *p = sc->sc_ifbuf, minibuf[4];
	int c, s, len, cksum;
	u_char c0;

printf("plipintr:\n");

/* Get the status */

	c0 = inb(PLIP_STATUS);
#if PLIP_DEBUG > 0
	if ((c0 & 0xf8) != 0xc0) {
		printf("st5=%02x ", c0);
	}
#endif

/* Don't want ints while receiving */

	outb(PLIP_CONTROL, PLIP_INTR_DISABLE);

	outb(PLIP_DATA, 0x01);   /* send ACK */ /* via NERR */
                                              
#if defined(COMPAT_PLIP10)
	if (ifp->if_flags & IFF_LINK0) {
		if (plipreceive(iobase, minibuf, 3) < 0) goto err;
		len = (minibuf[1] << 8) | minibuf[2];
		if (len > (ifp->if_mtu + ifp->if_hdrlen)) goto err;

		switch (minibuf[0]) {
		case 0xfc:
			p[0] = p[ 6] = ifp->ac_enaddr[0];
			p[1] = p[ 7] = ifp->ac_enaddr[1];
			p[2] = p[ 8] = ifp->ac_enaddr[2];
			p[3] = p[ 9] = ifp->ac_enaddr[3];
			p[4] = p[10] = ifp->ac_enaddr[4];
			p += 5;
			if ((cksum = plipreceive(iobase, p, 1)) < 0) goto err;
			p += 6;
			if ((c = plipreceive(iobase, p, len - 11)) < 0) goto err;
			cksum += c + sc->sc_adrcksum;
			c = p[1]; p[1] = p[2]; p[2] = c;
			cksum &= 0xff;
			break;
		case 0xfd:
			if ((cksum = plipreceive(iobase, p, len)) < 0) goto err;
			break;
		default:
			goto err;
		}
	} else
#endif
	{
		if (plipreceive(iobase, minibuf, 2) < 0) goto err;
		len = (minibuf[1] << 8) | minibuf[0];
		if (len > (ifp->if_mtu + ifp->if_hdrlen)) {
			log(LOG_NOTICE, "plip%d: packet > MTU\n", ifp->if_unit);
			goto err;
		}
#if PLIP_DEBUG != 0
		if (plip_debug & PLIP_DEBUG_RX)
			printf("len=%d ", len);
#endif
       		if (sizeof(struct ether_header) != 14) {
         		if ((cksum = plipreceive(iobase, p, 14)) < 0) goto err;
         		if ((c = plipreceive(iobase, p+16, len-14)) < 0) goto err;
         		cksum += c;
			len += 2;
		}
		else
         		if ((cksum = plipreceive(iobase, p, len)) < 0) goto err;
	}
	
	if (plipreceive(iobase, minibuf, 1) < 0) goto err;
	if ((cksum & 0xff) != minibuf[0]) {
		printf("cksum=%d, %d, %d\n", cksum, c, minibuf[0]);
		log(LOG_NOTICE, "plip%d: checksum error\n", ifp->if_unit);
		goto err;
	} 

	outb(PLIP_DATA, 0x00);   /* clear ACK */ /* via NERR */
#if PLIP_DEBUG != 0
	if (plip_debug & PLIP_DEBUG_RX)
		printf("done\n");
#endif
	s = splimp();
	
	eh = (struct ether_header *)sc->sc_ifbuf;

	if ((m = m_devget(sc->sc_ifbuf + sizeof(struct ether_header), len - sizeof(struct ether_header), 0, ifp, NULL))) {
		/* We assume that the header fit entirely in one mbuf. */
/*		eh = mtod(m, struct ether_header *);*/
/*		m->m_pkthdr.len -= sizeof(*eh);*/
/*		m->m_len -= sizeof(*eh);*/
/*		m->m_data += sizeof(*eh);*/
/*		printf("m->m_data=%08x ifbuf=%08x eh=%08x\n", m->m_data, sc->sc_ifbuf, eh);*/

#if NBPFILTER > 0
/*
 * Check if there's a BPF listener on this interface.
 * If so, hand off the raw packet to bpf.
 */
		if (sc->sc_arpcom.ac_if.if_bpf) {
			bpf_mtap(sc->sc_arpcom.ac_if.if_bpf, m);
		}
#endif
		ether_input(ifp, eh, m);
	}
	splx(s);
	sc->sc_iferrs = 0;
	ifp->if_ipackets++;

/* Allow ints again */

	outb(PLIP_CONTROL, PLIP_INTR_ENABLE);
	return(1);

err:
	outb(PLIP_DATA, 0x00);   /* clear ACK */ /* via NERR */

	ifp->if_ierrors++;
	sc->sc_iferrs++;
	if (sc->sc_iferrs > PLIPMXERRS
	    || (sc->sc_iferrs > 5 && (inb(iobase + lpt_status) & LPS_NBSY))) {
		/* We are not able to send receive anything for now,
		 * so stop wasting our time and leave the interrupt
		 * disabled.
		 */
		if (sc->sc_iferrs == PLIPMXERRS + 1)
			log(LOG_NOTICE, "plip%d: rx hard error\n", ifp->if_unit);
/*	xxx	i8255->port_a |= LPA_ACTIVE;*/
	} else
;
/*	xxx	i8255->port_a |= LPA_ACKENABLE | LPA_ACTIVE;*/

/* Allow ints again */

	outb(PLIP_CONTROL, PLIP_INTR_ENABLE);
	return(1);
}

static int
pliptransmit(unsigned int iobase, u_char *buf, int len)
{
	int i;
	u_char cksum = 0, c;
	u_char c0;
#if PLIP_DEBUG != 0
	if (plip_debug & PLIP_DEBUG_TX)
		printf("tx: len=%d ", len);
#endif

	while (len--) {
		i = PLIPMXSPIN2;
		cksum += (c = *buf++);
#if PLIP_DEBUG != 0
		if (plip_debug & PLIP_DEBUG_TX)
			printf("%02x ", c);
#endif
/*	xxx	while ((i8255->port_c & LPC_NBUSY) == 0)
			if (i-- < 0) return -1;
		i8255->port_b = c & 0x0f;
		i8255->port_b = c & 0x0f | 0x10;
		c >>= 4;
		while ((i8255->port_c & LPC_NBUSY) != 0)
			if (i-- < 0) return -1;
		i8255->port_b = c | 0x10;
		i8255->port_b = c;
*/

/* Send the nibble + handshake */

		outb(PLIP_DATA, 0x00 | (c & 0x0f));
		outb(PLIP_DATA, 0x10 | (c & 0x0f));

		while (1) {
			c0 = inb(PLIP_STATUS);
			if ((c0 & LPS_NBSY) == 0)
				break;
			if (--i == 0) { /* time out */
#if PLIP_DEBUG > 0
				printf("timeout tx lsn %02x ", c0);
#endif
				return(-1);
			}
			PLIP_DELAY;
		}

		outb(PLIP_DATA, 0x10 | (c >> 4));
		outb(PLIP_DATA, 0x00 | (c >> 4));
		i = PLIPMXSPIN2;
		while (1) {
			c0 = inb(PLIP_STATUS);
			if ((c0 & LPS_NBSY) != 0)
				break;
			if (--i == 0) { /* time out */
#if PLIP_DEBUG > 0
				printf("timeout tx msn %02x ", c0);
#endif
				return(-1);
			}
			PLIP_DELAY;
		}
	}
#if PLIP_DEBUG != 0
	if (plip_debug & PLIP_DEBUG_TX)
		printf("done\n");
#endif
	return(cksum);
}

/*
 * Setup output on interface.
 */
static void
plipstart(struct ifnet *ifp)
{
	struct lpt_softc *sc = (struct lpt_softc *) lpt_cd.cd_devs[ifp->if_unit];
	unsigned int iobase = sc->sc_iobase;
	struct mbuf *m0, *m;
	u_char minibuf[4], cksum;
	int len, i, s;
	u_char *p;

#if PLIP_DEBUG != 0
	if (plip_debug & PLIP_DEBUG_TX)
		printf("plipstart: ");
#endif
	if (ifp->if_flags & IFF_OACTIVE)
		return;
	ifp->if_flags |= IFF_OACTIVE;

	if (sc->sc_ifretry)
		untimeout((void (*)(void *))plipstart, ifp);

	for (;;) {
		s = splimp();
		IF_DEQUEUE(&ifp->if_snd, m0);
		splx(s);
		if (!m0)
			break;

		for (len = 0, m = m0; m; m = m->m_next) {
#if PLIP_DEBUG > 0
			if (plip_debug & PLIP_DEBUG_TX)
				printf("len=%d %d\n", m->m_len, len);
#endif
			len += m->m_len;
		}
#if NBPFILTER > 0
		p = sc->sc_ifbuf;
		for (m = m0; m; m = m->m_next) {
			if (m->m_len == 0)
				continue;
			bcopy(mtod(m, u_char *), p, m->m_len);
			p += m->m_len;
		}
		if (sc->sc_arpcom.ac_if.if_bpf)
			bpf_tap(sc->sc_arpcom.ac_if.if_bpf, sc->sc_ifbuf, len);
#endif
       		if (sizeof(struct ether_header) != 14)
			len -= 2;
#if defined(COMPAT_PLIP10)
		if (ifp->if_flags & IFF_LINK0) {
			minibuf[0] = 3;
			minibuf[1] = 0xfd;
			minibuf[2] = len >> 8;
			minibuf[3] = len;
		} else
#endif
		{
			minibuf[0] = 2;
			minibuf[1] = len;
			minibuf[2] = len >> 8;
		}

/*yyy		for (i = PLIPMXSPIN1; (inb(PLIP_STATUS) & LPS_NERR) != 0; i--)
			if (i < 0) goto retry;*/

		/* Trigger remote interrupt */

#if PLIP_DEBUG > 0
		if (plip_debug & PLIP_DEBUG_TX)
			printf("st=%02x ", inb(PLIP_STATUS));
#endif
		if (inb(PLIP_STATUS) & LPS_NERR) {
			for (i = PLIPMXSPIN1; (inb(PLIP_STATUS) & LPS_NERR) != 0; i--)
				PLIP_DELAY;
#if PLIP_DEBUG > 0
			if (plip_debug & PLIP_DEBUG_TX)
				printf("st1=%02x ", inb(PLIP_STATUS));
#endif
		}

		outb(PLIP_DATA, PLIP_REMOTE_TRIGGER);
		for (i = PLIPMXSPIN1; (inb(PLIP_STATUS) & LPS_NERR) == 0; i--) {
			if (i < 0 || (i > PLIPMXSPIN1/3
			    && inb(PLIP_STATUS) & LPS_NACK)) {
#if PLIP_DEBUG > 0
			    printf("trigger ack timeout\n");
#endif
				goto retry;
			}
			PLIP_DELAY;
		}
#if PLIP_DEBUG > 0
		if (plip_debug & PLIP_DEBUG_TX)
			printf("st3=%02x ", inb(PLIP_STATUS));
#endif

/* Don't want ints while transmitting */

		outb(PLIP_CONTROL, PLIP_INTR_DISABLE);

		if (pliptransmit(iobase, minibuf + 1, minibuf[0]) < 0) goto retry;
		for (cksum = 0, m = m0; m; m = m->m_next) {
			if (sizeof(struct ether_header) != 14 && m == m0) {
				i = pliptransmit(iobase, mtod(m, u_char *), 14);
				if (i < 0) goto retry;
				cksum += i;
				i = pliptransmit(iobase, mtod(m, u_char *)+16, m->m_len-16);
				if (i < 0) goto retry;
			}
			else			         
				i = pliptransmit(iobase, mtod(m, u_char *), m->m_len);
			if (i < 0) goto retry;
			cksum += i;
		}
		if (pliptransmit(iobase, &cksum, 1) < 0) goto retry;
		i = PLIPMXSPIN2;
		while ((inb(PLIP_STATUS) & LPS_NBSY) == 0)
			if (i-- < 0) goto retry;

		outb(iobase + lpt_data, 0x00);
/* Re-enable ints */

		outb(PLIP_CONTROL, PLIP_INTR_ENABLE);

		ifp->if_opackets++;
		ifp->if_obytes += len + 4;
		sc->sc_ifretry = 0;
		s = splimp();
		m_freem(m0);
		splx(s);
	}
	ifp->if_flags &= ~IFF_OACTIVE;
	return;

retry:
#if PLIP_DEBUG > 0
	if (plip_debug & PLIP_DEBUG_TX)
		printf("retry: %02x", inb(iobase + lpt_status));
#endif
	if (inb(PLIP_STATUS & LPS_NACK))
		ifp->if_collisions++;
	else
		ifp->if_oerrors++;
	if ((ifp->if_flags & (IFF_RUNNING | IFF_UP)) == (IFF_RUNNING | IFF_UP)
	    && sc->sc_ifretry < PLIPMXRETRY) {
		sc->sc_ifretry++;
		s = splimp();
		IF_PREPEND(&ifp->if_snd, m0);
		splx(s);
		timeout((void (*)(void *))plipstart, ifp, PLIPRETRY);
	} else {
		if (sc->sc_ifretry == PLIPMXRETRY) {
			sc->sc_ifretry++;
			log(LOG_NOTICE, "plip%d: tx hard error\n", ifp->if_unit);
		}
		s = splimp();
		m_freem(m0);
		splx(s);
	}
	ifp->if_flags &= ~IFF_OACTIVE;
		outb(PLIP_DATA, 0x00);

/* Re-enable ints */

		outb(PLIP_CONTROL, PLIP_INTR_ENABLE);
/*xxx	if (sc->sc_iferrs > PLIPMXERRS)
		i8255->port_a |= LPA_ACTIVE;
	else
		i8255->port_a |= LPA_ACKENABLE | LPA_ACTIVE;*/
	return;
}

#endif
