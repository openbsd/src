/*	$NetBSD: lpt.c,v 1.6.2.1 1995/10/19 01:33:06 phil Exp $	*/

/*
 * Copyright (c) 1994 Matthias Pfaller.
 * Copyright (c) 1994 Poul-Henning Kamp
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
 */

/*
 * Device Driver for Matthias's parallel printer port.
 * This driver is based on the i386 lpt driver and
 * some IP code from Poul-Henning Kamp.
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
#include <sys/malloc.h>
#include <machine/cpu.h>

#include "lpt.h"
#include "lptreg.h"

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
#endif

#define	LPT_INVERT	(LPC_NBUSY|LPC_NERROR|LPC_NACK|LPC_ONLINE)
#define	LPT_MASK	(LPC_NBUSY|LPC_NERROR|LPC_NACK|LPC_NOPAPER|LPC_ONLINE)

#define	TIMEOUT		hz*16	/* wait up to 16 seconds for a ready */
#define	STEP		hz/4

#define	LPTPRI		(PZERO+8)
#define	LPT_BSIZE	1024

#if defined(INET) && defined(PLIP)
#ifndef PLIPMTU			/* MTU for the plip# interfaces */
#if defined(COMPAT_PLIP10)
#define	PLIPMTU		1600				/* Linux 1.0.x */
#elif defined(COMPAT_PLIP11)
#define	PLIPMTU		(ETHERMTU - ifp->if_hdrlen)	/* Linux 1.1.x */
#else
#define PLIPMTU		ETHERMTU			/* Linux 1.3.x */
#endif
#endif

#ifndef PLIPMXSPIN1		/* DELAY factor for the plip# interfaces */
#define	PLIPMXSPIN1	2000	/* Spinning for remote intr to happen */
#endif

#ifndef PLIPMXSPIN2		/* DELAY factor for the plip# interfaces */
#define	PLIPMXSPIN2	6000	/* Spinning for remote handshake to happen */
#endif

#ifndef PLIPMXERRS		/* Max errors before !RUNNING */
#define	PLIPMXERRS	20
#endif
#ifndef PLIPMXRETRY
#define PLIPMXRETRY	20	/* Max number of retransmits */
#endif
#ifndef PLIPRETRY
#define PLIPRETRY	hz/50	/* Time between retransmits */
#endif
#endif

struct lpt_softc {
	struct device sc_dev;
	size_t sc_count;
	u_char *sc_inbuf;
	u_char *sc_cp;
	volatile struct i8255 *sc_i8255;
	int sc_irq;
	u_char sc_state;
#define	LPT_OPEN	0x01	/* device is open */
#define	LPT_INIT	0x02	/* waiting to initialize for open */

	u_char sc_status;
	u_char sc_flags;
#define	LPT_AUTOLF	0x20	/* automatic LF on CR */
#define	LPT_NOPRIME	0x40	/* don't prime on open */

#if defined(INET) && defined(PLIP)
	struct	arpcom	sc_arpcom;
	u_char		*sc_ifbuf;
	int		sc_ifierrs;	/* consecutive input errors */
	int		sc_ifoerrs;	/* consecutive output errors */
	int		sc_ifsoftint;	/* i/o software interrupt */
	volatile int	sc_pending;	/* interrputs pending */
#define PLIP_IPENDING	1
#define PLIP_OPENDING	2

#if defined(COMPAT_PLIP10)
	u_char		sc_adrcksum;
#endif
#endif
};

#define	LPTUNIT(s)	(minor(s) & 0x1f)
#define	LPTFLAGS(s)	(minor(s) & 0xe0)

static int lptmatch(struct device *, void *, void *aux);
static void lptattach(struct device *, struct device *, void *);
static void lptintr(struct lpt_softc *);
static int notready(u_char, struct lpt_softc *);
static void lptout(void *arg);
static int pushbytes(struct lpt_softc *);

#if defined(INET) && defined(PLIP)
/* Functions for the plip# interface */
static void plipattach(struct lpt_softc *,int);
static int plipioctl(struct ifnet *, u_long, caddr_t);
static void plipsoftint(struct lpt_softc *);
static void plipinput(struct lpt_softc *);
static void plipstart(struct ifnet *);
static void plipoutput(struct lpt_softc *);
#endif

struct cfdriver lptcd = {
	NULL,
	"lpt",
	lptmatch,
	lptattach,
	DV_TTY,
	sizeof(struct lpt_softc),
	NULL,
	0
};

lptmatch(struct device *parent, void *cf, void *aux)
{
	volatile struct i8255 *i8255 =
		(volatile struct i8255 *)((struct cfdata *)cf)->cf_loc[0];
	int unit = ((struct cfdata *)cf)->cf_unit;

	if (unit >= LPT_MAX)
		return(0);

	if ((int) i8255 == -1)
		i8255 = LPT_ADR(unit);

	i8255->port_control = LPT_PROBE_MODE;

	i8255->port_control = LPT_PROBE_CLR;
	if ((i8255->port_c & LPT_PROBE_MASK) != 0)
		return 0;
	
	i8255->port_control = LPT_PROBE_SET;
	if ((i8255->port_c & LPT_PROBE_MASK) == 0)
		return 0;

	i8255->port_control = LPT_PROBE_CLR;
	if ((i8255->port_c & LPT_PROBE_MASK) != 0)
		return 0;

	i8255->port_control = LPT_MODE;
	i8255->port_a = LPA_ACTIVE | LPA_NPRIME;
	
	return 1;
}

void
lptattach(struct device *parent, struct device *self, void *aux)
{
	struct lpt_softc *sc = (struct lpt_softc *) self;
	volatile struct i8255 *i8255 =
			(volatile struct i8255 *)self->dv_cfdata->cf_loc[0];

	if ((sc->sc_irq = self->dv_cfdata->cf_loc[1]) == -1)
		sc->sc_irq = LPT_IRQ(self->dv_unit);

	if ((int)i8255 == -1)
		i8255 = LPT_ADR(self->dv_unit);
	i8255->port_control = LPT_MODE;
	i8255->port_a = LPA_ACTIVE | LPA_NPRIME;
	i8255->port_control = LPT_IRQDISABLE;

	sc->sc_state = 0;
	sc->sc_i8255 = i8255;

#if defined(INET) && defined(PLIP)
	plipattach(sc, self->dv_unit);
#endif
	intr_establish(sc->sc_irq, lptintr, sc, sc->sc_dev.dv_xname,
			IPL_NONE, FALLING_EDGE);
	printf(" addr 0x%x, irq %d\n", (int) i8255, sc->sc_irq);
}

/*
 * Reset the printer, then wait until it's selected and not busy.
 */
int
lptopen(dev_t dev, int flag)
{
	struct lpt_softc *sc = (struct lpt_softc *) lptcd.cd_devs[LPTUNIT(dev)];
	volatile struct i8255 *i8255 = sc->sc_i8255;
	u_char flags = LPTFLAGS(dev);
	int error;
	int spin;

	if (LPTUNIT(dev) >= NLPT || !sc)
		return ENXIO;

	if (sc->sc_state)
		return EBUSY;

#if defined(INET) && defined(PLIP)
	if (sc->sc_arpcom.ac_if.if_flags & IFF_UP)
		return EBUSY;
#endif

	sc->sc_state = LPT_INIT;
	sc->sc_flags = flags;

	if ((flags & LPT_NOPRIME) == 0) {
		/* assert INIT for 100 usec to start up printer */
		i8255->port_a &= ~LPA_NPRIME;
		DELAY(100);
	}

	if (flags & LPT_AUTOLF)
		i8255->port_a |= LPA_ALF | LPA_SELECT | LPA_NPRIME;
	else
		i8255->port_a = (i8255->port_a & ~LPA_ALF)
				| LPA_SELECT | LPA_NPRIME;

	/* wait till ready (printer running diagnostics) */
	for (spin = 0; notready(i8255->port_c, sc); spin += STEP) {
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

	sc->sc_inbuf  = malloc(LPT_BSIZE, M_DEVBUF, M_WAITOK);
	sc->sc_status =
	sc->sc_count  = 0;
	sc->sc_state  = LPT_OPEN;

	return 0;
}

int
notready(u_char status, struct lpt_softc *sc)
{
	status ^= LPT_INVERT;

	if (status != sc->sc_status) {
		if (status & LPC_NOPAPER)
			log(LOG_NOTICE, "%s: out of paper\n", sc->sc_dev.dv_xname);
		if (status & LPC_ONLINE)
			log(LOG_NOTICE, "%s: offline\n", sc->sc_dev.dv_xname);
		if (status & LPC_NERROR)
			log(LOG_NOTICE, "%s: output error\n", sc->sc_dev.dv_xname);
		if (status & LPC_NACK)
			log(LOG_NOTICE, "%s: NACK low\n", sc->sc_dev.dv_xname);
		if (status & LPC_NBUSY)
			log(LOG_NOTICE, "%s: NBUSY low\n", sc->sc_dev.dv_xname);
		sc->sc_status = status;
	}
	return status & LPT_MASK;
}

void
lptout(void *arg)
{
	struct lpt_softc *sc = (struct lpt_softc *) arg;
	if (sc->sc_count > 0)
		sc->sc_i8255->port_control = LPT_IRQENABLE;
}

/*
 * Close the device, and free the local line buffer.
 */
lptclose(dev_t dev, int flag)
{
	struct lpt_softc *sc = (struct lpt_softc *) lptcd.cd_devs[LPTUNIT(dev)];

	if (sc->sc_count)
		(void) pushbytes(sc);

	sc->sc_i8255->port_control = LPT_IRQDISABLE;
	sc->sc_state = 0;
	free(sc->sc_inbuf, M_DEVBUF);

	return 0;
}

int
pushbytes(struct lpt_softc *sc)
{
	volatile struct i8255 *i8255 = sc->sc_i8255;
	int error;

	while (sc->sc_count > 0) {
		i8255->port_control = LPT_IRQENABLE;
		if (error = tsleep((caddr_t)sc, LPTPRI | PCATCH,
		    "lptwrite", 0))
			return error;
	}
	return 0;
}

/* 
 * Copy a line from user space to a local buffer, then call pushbytes to
 * get the chars moved to the output queue.
 */
lptwrite(dev_t dev, struct uio *uio)
{
	struct lpt_softc *sc = (struct lpt_softc *) lptcd.cd_devs[LPTUNIT(dev)];
	size_t n;
	int error = 0;

	if (sc->sc_count) return EBUSY;
	while (n = min(LPT_BSIZE, uio->uio_resid)) {
		uiomove(sc->sc_cp = sc->sc_inbuf, n, uio);
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
void
lptintr(struct lpt_softc *sc)
{
	volatile struct i8255 *i8255 = sc->sc_i8255;

#if defined(INET) && defined(PLIP)
	if(sc->sc_arpcom.ac_if.if_flags & IFF_UP) {
		i8255->port_a &= ~LPA_ACKENABLE;
		sc->sc_pending |= PLIP_IPENDING;
		softintr(sc->sc_ifsoftint);
		return;
	}
#endif

	if ((sc->sc_state & LPT_OPEN) == 0) {
		i8255->port_control = LPT_IRQDISABLE;
		return;
	}

	if (sc->sc_count) {
		/* is printer online and ready for output? */
		if (notready(i8255->port_c, sc)) {
			i8255->port_control = LPT_IRQDISABLE;
			timeout(lptout, sc, STEP);
			return;
		}
		/* send char */
		i8255->port_a &= ~LPA_ACTIVE;
		i8255->port_b = *sc->sc_cp++;
		i8255->port_a |= LPA_ACTIVE;
		sc->sc_count--;
	}

	if (sc->sc_count == 0) {
		/* none, wake up the top half to get more */
		i8255->port_control = LPT_IRQDISABLE;
		wakeup((caddr_t)sc);
	}
}

int
lptioctl(dev_t dev, int cmd, caddr_t data, int flag)
{
	int error = 0;

	switch (cmd) {
	default:
		error = EINVAL;
	}

	return error;
}

#if defined(INET) && defined(PLIP)

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
	sc->sc_ifsoftint = intr_establish(SOFTINT, plipsoftint, sc,
					sc->sc_dev.dv_xname, IPL_NET, 0);

	if_attach(ifp);
}

/*
 * Process an ioctl request.
 */
static int
plipioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct proc *p = curproc;
	struct lpt_softc *sc = (struct lpt_softc *) lptcd.cd_devs[ifp->if_unit];
	volatile struct i8255 *i8255 = sc->sc_i8255;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data; 
	int s;
	int error = 0;

	switch (cmd) {

	case SIOCSIFFLAGS:
		if (((ifp->if_flags & IFF_UP) == 0) &&
		    (ifp->if_flags & IFF_RUNNING)) {
		        ifp->if_flags &= ~IFF_RUNNING;
			sc->sc_i8255->port_control = LPT_MODE;
			i8255->port_a = LPA_ACTIVE | LPA_NPRIME;
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
			if (!sc->sc_ifbuf)
				sc->sc_ifbuf =
					malloc(ifp->if_mtu + ifp->if_hdrlen,
					       M_DEVBUF, M_WAITOK);
		        ifp->if_flags |= IFF_RUNNING;
			sc->sc_i8255->port_control = LPT_IRQDISABLE;
			sc->sc_i8255->port_b = 0;
			sc->sc_i8255->port_a |= LPA_ACKENABLE;
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
			sc->sc_i8255->port_control = LPT_IRQDISABLE;
			sc->sc_i8255->port_b = 0;
			sc->sc_i8255->port_a |= LPA_ACKENABLE;
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

static void
plipsoftint(struct lpt_softc *sc)
{
	int pending = sc->sc_pending;

	while (sc->sc_pending & PLIP_IPENDING) {
		pending |= sc->sc_pending;
		sc->sc_pending = 0;
		plipinput(sc);
	}

	if (pending & PLIP_OPENDING)
		plipoutput(sc);
}

static int
plipreceive(volatile struct i8255 *i8255, u_char *buf, int len)
{
	int i;
	u_char cksum = 0, c;

	while (len--) {
		i = PLIPMXSPIN2;
		while ((i8255->port_c & LPC_NBUSY) != 0)
			if (i-- < 0) return -1;
		c = i8255->port_c >> 4;
		i8255->port_b = 0x11;
		while ((i8255->port_c & LPC_NBUSY) == 0)
			if (i-- < 0) return -1;
		c |= i8255->port_c & 0xf0;
		i8255->port_b = 0x01;
		cksum += (*buf++ = c);
	}
	return(cksum);
}

static void
plipinput(struct lpt_softc *sc)
{
	extern struct mbuf *m_devget(char *, int, int, struct ifnet *, void (*)());
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	volatile struct i8255 *i8255 = sc->sc_i8255;
	struct mbuf *m;
	struct ether_header *eh;
	u_char *p = sc->sc_ifbuf, minibuf[4];
	int c, i = 0, s, len, cksum;

	if (!(i8255->port_c & LPC_NACK)) {
		i8255->port_a |= LPA_ACKENABLE;
		ifp->if_collisions++;
		return;
	}
	i8255->port_b = 0x01;
	i8255->port_a &= ~(LPA_ACKENABLE | LPA_ACTIVE);

#if defined(COMPAT_PLIP10)
	if (ifp->if_flags & IFF_LINK0) {
		if (plipreceive(i8255, minibuf, 3) < 0) goto err;
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
			if ((cksum = plipreceive(i8255, p, 1)) < 0) goto err;
			p += 6;
			if ((c = plipreceive(i8255, p, len - 11)) < 0) goto err;
			cksum += c + sc->sc_adrcksum;
			c = p[1]; p[1] = p[2]; p[2] = c;
			cksum &= 0xff;
			break;
		case 0xfd:
			if ((cksum = plipreceive(i8255, p, len)) < 0) goto err;
			break;
		default:
			goto err;
		}
	} else
#endif
	{
		if (plipreceive(i8255, minibuf, 2) < 0) goto err;
		len = (minibuf[1] << 8) | minibuf[0];
		if (len > (ifp->if_mtu + ifp->if_hdrlen)) {
			log(LOG_NOTICE, "plip%d: packet > MTU\n", ifp->if_unit);
			goto err;
		}
		if ((cksum = plipreceive(i8255, p, len)) < 0) goto err;
	}

	if (plipreceive(i8255, minibuf, 1) < 0) goto err;
	if (cksum != minibuf[0]) {
		log(LOG_NOTICE, "plip%d: checksum error\n", ifp->if_unit);
		goto err;
	}
	i8255->port_b = 0x00;

	s = splimp();
	if (m = m_devget(sc->sc_ifbuf, len, 0, ifp, NULL)) {
		/* We assume that the header fit entirely in one mbuf. */
		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.len -= sizeof(*eh);
		m->m_len -= sizeof(*eh);
		m->m_data += sizeof(*eh);
		ether_input(ifp, eh, m);
	}
	splx(s);
	sc->sc_ifierrs = 0;
	ifp->if_ipackets++;
	i8255->port_a |= LPA_ACKENABLE | LPA_ACTIVE;
	return;

err:
	i8255->port_b = 0x00;

	if (sc->sc_ifierrs < PLIPMXERRS) {
		i8255->port_a |= LPA_ACKENABLE | LPA_ACTIVE;
	} else {
		/* We are not able to send receive anything for now,
		 * so stop wasting our time and leave the interrupt
		 * disabled.
		 */
		if (sc->sc_ifierrs == PLIPMXERRS)
			log(LOG_NOTICE, "plip%d: rx hard error\n", ifp->if_unit);
		i8255->port_a |= LPA_ACTIVE;
	}
	ifp->if_ierrors++;
	sc->sc_ifierrs++;
	return;
}

static int
pliptransmit(volatile struct i8255 *i8255, u_char *buf, int len)
{
	int i;
	u_char cksum = 0, c;

	while (len--) {
		i = PLIPMXSPIN2;
		cksum += (c = *buf++);
		while ((i8255->port_c & LPC_NBUSY) == 0)
			if (i-- < 0) return -1;
		i8255->port_b = c & 0x0f;
		i8255->port_b = c & 0x0f | 0x10;
		c >>= 4;
		while ((i8255->port_c & LPC_NBUSY) != 0)
			if (i-- < 0) return -1;
		i8255->port_b = c | 0x10;
		i8255->port_b = c;
	}
	return(cksum);
}

/*
 * Setup output on interface.
 */
static void
plipstart(struct ifnet *ifp)
{
	struct lpt_softc *sc = (struct lpt_softc *) lptcd.cd_devs[ifp->if_unit];
	sc->sc_pending |= PLIP_OPENDING;
	softintr(sc->sc_ifsoftint);
}

static void
plipoutput(struct lpt_softc *sc)
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	volatile struct i8255 *i8255 = sc->sc_i8255;
	struct mbuf *m0, *m;
	u_char minibuf[4], cksum;
	int len, i, s;

	if (ifp->if_flags & IFF_OACTIVE)
		return;
	ifp->if_flags |= IFF_OACTIVE;

	if (sc->sc_ifoerrs)
		untimeout((void (*)(void *))plipoutput, sc);

	for (;;) {
		s = splnet();
		IF_DEQUEUE(&ifp->if_snd, m0);
		splx(s);
		if (!m0)
			break;

		for (len = 0, m = m0; m; m = m->m_next)
			len += m->m_len;
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

		/* Trigger remote interrupt */
		i = PLIPMXSPIN1;
		do {
			if (sc->sc_pending & PLIP_IPENDING) {
				i8255->port_b = 0x00;
				sc->sc_pending = 0;
				plipinput(sc);
				i = PLIPMXSPIN1;
			} else if (i-- < 0)
				goto retry;
			/* Retrigger remote interrupt */
			i8255->port_b = 0x08;
		} while ((i8255->port_c & LPC_NERROR) == 0);
		i8255->port_a &= ~(LPA_ACKENABLE | LPA_ACTIVE);

		if (pliptransmit(i8255, minibuf + 1, minibuf[0]) < 0) goto retry;
		for (cksum = 0, m = m0; m; m = m->m_next) {
			i = pliptransmit(i8255, mtod(m, u_char *), m->m_len);
			if (i < 0) goto retry;
			cksum += i;
		}
		if (pliptransmit(i8255, &cksum, 1) < 0) goto retry;
		i = PLIPMXSPIN2;
		while ((i8255->port_c & LPC_NBUSY) == 0)
			if (i-- < 0) goto retry;
		i8255->port_b = 0x00;

		ifp->if_opackets++;
		ifp->if_obytes += len + 4;
		sc->sc_ifoerrs = 0;
		s = splimp();
		m_freem(m0);
		splx(s);
		i8255->port_a |= LPA_ACKENABLE;
	}
	i8255->port_a |= LPA_ACTIVE;
	ifp->if_flags &= ~IFF_OACTIVE;
	return;

retry:
	if (i8255->port_c & LPC_NACK)
		ifp->if_collisions++;
	else
		ifp->if_oerrors++;

	ifp->if_flags &= ~IFF_OACTIVE;
	i8255->port_b = 0x00;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_UP)) == (IFF_RUNNING | IFF_UP)
	    && sc->sc_ifoerrs < PLIPMXRETRY) {
		s = splnet();
		IF_PREPEND(&ifp->if_snd, m0);
		splx(s);
		i8255->port_a |= LPA_ACKENABLE | LPA_ACTIVE;
		timeout((void (*)(void *))plipoutput, sc, PLIPRETRY);
	} else {
		if (sc->sc_ifoerrs == PLIPMXRETRY) {
			log(LOG_NOTICE, "plip%d: tx hard error\n", ifp->if_unit);
		}
		s = splimp();
		m_freem(m0);
		splx(s);
		i8255->port_a |= LPA_ACTIVE;
	}
	sc->sc_ifoerrs++;
}

#endif
