/*	$NetBSD: com.c,v 1.61 1995/07/04 06:47:18 mycroft Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

/*
 * COM driver, based on HP dca driver
 * uses National Semiconductor NS16450/NS16550AF UART
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <machine/pio.h>
#include <pica/pica/pica.h>

#include <dev/isa/comreg.h>
#include <dev/ic/ns16550reg.h>

#define	COM_IBUFSIZE	(2 * 256)
#define	COM_IHIGHWATER	((3 * COM_IBUFSIZE) / 4)

struct com_softc {
	struct device sc_dev;
	struct tty *sc_tty;

	int sc_overflows;
	int sc_floods;
	int sc_errors;

	long	sc_iobase;
	u_char sc_hwflags;
#define	COM_HW_NOIEN	0x01
#define	COM_HW_FIFO	0x02
#define	COM_HW_CONSOLE	0x40
	u_char sc_swflags;
#define	COM_SW_SOFTCAR	0x01
#define	COM_SW_CLOCAL	0x02
#define	COM_SW_CRTSCTS	0x04
#define	COM_SW_MDMBUF	0x08
	u_char sc_msr, sc_mcr;
	u_char sc_dtr;

	u_char *sc_ibuf, *sc_ibufp, *sc_ibufhigh, *sc_ibufend;
	u_char sc_ibufs[2][COM_IBUFSIZE];
};

int commatch __P((struct device *, void *, void *));
void comattach __P((struct device *, struct device *, void *));
int comopen __P((dev_t, int, int, struct proc *));
int comclose __P((dev_t, int, int, struct proc *));
void comdiag __P((void *));
int comintr __P((void *));
void compoll __P((void *));
int comparam __P((struct tty *, struct termios *));
void comstart __P((struct tty *));

struct cfattach com_ca = {
	sizeof(struct com_softc), commatch, comattach
};
struct cfdriver com_cd = {
	NULL, "com", DV_TTY, NULL, 0
};

int	comdefaultrate = TTYDEF_SPEED;
#ifdef COMCONSOLE
int	comconsole = COMCONSOLE;
#else
int	comconsole = -1;
#endif
int	comconsinit;
int	commajor;
int	comsopen = 0;
int	comevents = 0;

#ifdef KGDB
#include <machine/remote-sl.h>
extern int kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif

#define	COMUNIT(x)	(minor(x))

#define	bis(c, b)	do { const register int com_ad = (c); \
			     outb(com_ad, inb(com_ad) | (b)); } while(0)
#define	bic(c, b)	do { const register int com_ad = (c); \
			     outb(com_ad, inb(com_ad) & ~(b)); } while(0)

int
comspeed(speed)
	long speed;
{
#define	divrnd(n, q)	(((n)*2/(q)+1)/2)	/* divide and round off */

	int x, err;

	if (speed == 0)
		return 0;
	if (speed < 0)
		return -1;
	x = divrnd((COM_FREQ / 16), speed);
	if (x <= 0)
		return -1;
	err = divrnd((COM_FREQ / 16) * 1000, speed * x) - 1000;
	if (err < 0)
		err = -err;
	if (err > COM_TOLERANCE)
		return -1;
	return x;

#undef	divrnd(n, q)
}

int
commatch1(iobase)
	long iobase;
{

	/* force access to id reg */
	outb(iobase + com_cfcr, 0);
	outb(iobase + com_iir, 0);
	if (inb(iobase + com_iir) & 0x38)
		return 0;

	return 1;
}

int
commatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata, *aux;
{
	struct confargs *ca = aux;
	long iobase;

	/* Make shure that we're looking for this type of device. */
	if(!BUS_MATCHNAME(ca, "com"))
		return(0);

	iobase = (long)BUS_CVTADDR(ca);
	if (!commatch1(iobase))
		return (0);

	return (1);
}

void
comattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_softc *sc = (void *)self;
	struct confargs *ca = aux;
	struct cfdata *cf = sc->sc_dev.dv_cfdata;
	long iobase;
	struct tty *tp;

	sc->sc_iobase = iobase = (long)BUS_CVTADDR(ca);
	sc->sc_hwflags = cf->cf_flags & COM_HW_NOIEN;
	sc->sc_swflags = 0;

	if (sc->sc_dev.dv_unit == comconsole)
		delay(1000);

	/* look for a NS 16550AF UART with FIFOs */
	outb(iobase + com_fifo,
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_14);
	delay(100);
	if ((inb(iobase + com_iir) & IIR_FIFO_MASK) == IIR_FIFO_MASK)
		if ((inb(iobase + com_fifo) & FIFO_TRIGGER_14) == FIFO_TRIGGER_14) {
			sc->sc_hwflags |= COM_HW_FIFO;
			printf(": ns16550a, working fifo\n");
		} else
			printf(": ns16550, broken fifo\n");
	else
		printf(": ns8250 or ns16450, no fifo\n");
	outb(iobase + com_fifo, 0);

	/* disable interrupts */
	outb(iobase + com_ier, 0);
	outb(iobase + com_mcr, 0);

	BUS_INTR_ESTABLISH(ca, comintr, (void *)(long)sc);

#ifdef KGDB
	if (kgdb_dev == makedev(commajor, unit)) {
		if (comconsole == unit)
			kgdb_dev = -1;	/* can't debug over console port */
		else {
			(void) cominit(unit, kgdb_rate);
			if (kgdb_debug_init) {
				/*
				 * Print prefix of device name,
				 * let kgdb_connect print the rest.
				 */
				printf("%s: ", sc->sc_dev.dv_xname);
				kgdb_connect(1);
			} else
				printf("%s: kgdb enabled\n",
				    sc->sc_dev.dv_xname);
		}
	}
#endif

	if (sc->sc_dev.dv_unit == comconsole) {
		/*
		 * Need to reset baud rate, etc. of next print so reset
		 * comconsinit.  Also make sure console is always "hardwired".
		 */
		comconsinit = 0;
		sc->sc_hwflags |= COM_HW_CONSOLE;
		sc->sc_swflags |= COM_SW_SOFTCAR;
	}
}

int
comopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = COMUNIT(dev);
	struct com_softc *sc;
	long iobase;
	struct tty *tp;
	int s;
	int error = 0;
 
	if (unit >= com_cd.cd_ndevs)
		return ENXIO;
	sc = com_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (!sc->sc_tty)
		tp = sc->sc_tty = ttymalloc();
	else
		tp = sc->sc_tty;

	tp->t_oproc = comstart;
	tp->t_param = comparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		if (sc->sc_swflags & COM_SW_CLOCAL)
			tp->t_cflag |= CLOCAL;
		if (sc->sc_swflags & COM_SW_CRTSCTS)
			tp->t_cflag |= CRTSCTS;
		if (sc->sc_swflags & COM_SW_MDMBUF)
			tp->t_cflag |= MDMBUF;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = comdefaultrate;

		s = spltty();

		comparam(tp, &tp->t_termios);
		ttsetwater(tp);

		if (comsopen++ == 0)
			timeout(compoll, NULL, 1);

		sc->sc_ibufp = sc->sc_ibuf = sc->sc_ibufs[0];
		sc->sc_ibufhigh = sc->sc_ibuf + COM_IHIGHWATER;
		sc->sc_ibufend = sc->sc_ibuf + COM_IBUFSIZE;

		iobase = sc->sc_iobase;
		/* Set the FIFO threshold based on the receive speed. */
		if (sc->sc_hwflags & COM_HW_FIFO)
			outb(iobase + com_fifo,
			    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST |
			    (tp->t_ispeed <= 1200 ? FIFO_TRIGGER_1 : FIFO_TRIGGER_8));
		/* flush any pending I/O */
		while (inb(iobase + com_lsr) & LSR_RXRDY)
			(void) inb(iobase + com_data);
		/* you turn me on, baby */
		sc->sc_mcr = MCR_DTR | MCR_RTS;
		if ((sc->sc_hwflags & COM_HW_NOIEN) == 0)
			sc->sc_mcr |= MCR_IENABLE;
		outb(iobase + com_mcr, sc->sc_mcr);
		outb(iobase + com_ier,
		    IER_ERXRDY | IER_ETXRDY | IER_ERLS | IER_EMSC);

		sc->sc_msr = inb(iobase + com_msr);
		if (sc->sc_swflags & COM_SW_SOFTCAR || sc->sc_msr & MSR_DCD ||
		    tp->t_cflag & MDMBUF)
			tp->t_state |= TS_CARR_ON;
		else
			tp->t_state &= ~TS_CARR_ON;
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		return EBUSY;
	} else
		s = spltty();

	/* wait for carrier if necessary */
	if ((flag & O_NONBLOCK) == 0)
		while ((tp->t_cflag & CLOCAL) == 0 &&
		    (tp->t_state & TS_CARR_ON) == 0) {
			tp->t_state |= TS_WOPEN;
			error = ttysleep(tp, (caddr_t)&tp->t_rawq, 
			    TTIPRI | PCATCH, ttopen, 0);
			if (error) {
				/* XXX should turn off chip if we're the
				   only waiter */
				splx(s);
				return error;
			}
		}
	splx(s);

	return (*linesw[tp->t_line].l_open)(dev, tp);
}
 
int
comclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = COMUNIT(dev);
	struct com_softc *sc = com_cd.cd_devs[unit];
	struct tty *tp = sc->sc_tty;
	long iobase = sc->sc_iobase;
	int s;

	/* XXX This is for cons.c. */
	if ((tp->t_state & TS_ISOPEN) == 0)
		return 0;

	(*linesw[tp->t_line].l_close)(tp, flag);
	s = spltty();
	bic(iobase + com_cfcr, LCR_SBREAK);
	outb(iobase + com_ier, 0);
	if (tp->t_cflag & HUPCL &&
	    (sc->sc_swflags & COM_SW_SOFTCAR) == 0) {
		/* XXX perhaps only clear DTR */
		outb(iobase + com_mcr, 0);
	}
	tp->t_state &= ~(TS_BUSY | TS_FLUSH);
	if (--comsopen == 0)
		untimeout(compoll, NULL);
	splx(s);
	ttyclose(tp);
#ifdef notyet /* XXXX */
	if (unit != comconsole) {
		ttyfree(tp);
		sc->sc_tty = 0;
	}
#endif
	return 0;
}
 
int
comread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct com_softc *sc = com_cd.cd_devs[COMUNIT(dev)];
	struct tty *tp = sc->sc_tty;
 
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
comwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct com_softc *sc = com_cd.cd_devs[COMUNIT(dev)];
	struct tty *tp = sc->sc_tty;
 
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
comtty(dev)
	dev_t dev;
{
	struct com_softc *sc = com_cd.cd_devs[COMUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return (tp);
}
 
static u_char
tiocm_xxx2mcr(data)
	int data;
{
	u_char m = 0;

	if (data & TIOCM_DTR)
		m |= MCR_DTR;
	if (data & TIOCM_RTS)
		m |= MCR_RTS;
	return m;
}

int
comioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = COMUNIT(dev);
	struct com_softc *sc = com_cd.cd_devs[unit];
	struct tty *tp = sc->sc_tty;
	long iobase = sc->sc_iobase;
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	switch (cmd) {
	case TIOCSBRK:
		bis(iobase + com_cfcr, LCR_SBREAK);
		break;
	case TIOCCBRK:
		bic(iobase + com_cfcr, LCR_SBREAK);
		break;
	case TIOCSDTR:
		outb(iobase + com_mcr, sc->sc_mcr |= sc->sc_dtr);
		break;
	case TIOCCDTR:
		outb(iobase + com_mcr, sc->sc_mcr &= ~sc->sc_dtr);
		break;
	case TIOCMSET:
		sc->sc_mcr &= ~(MCR_DTR | MCR_RTS);
	case TIOCMBIS:
		outb(iobase + com_mcr,
		    sc->sc_mcr |= tiocm_xxx2mcr(*(int *)data));
		break;
	case TIOCMBIC:
		outb(iobase + com_mcr,
		    sc->sc_mcr &= ~tiocm_xxx2mcr(*(int *)data));
		break;
	case TIOCMGET: {
		u_char m;
		int bits = 0;

		m = sc->sc_mcr;
		if (m & MCR_DTR)
			bits |= TIOCM_DTR;
		if (m & MCR_RTS)
			bits |= TIOCM_RTS;
		m = sc->sc_msr;
		if (m & MSR_DCD)
			bits |= TIOCM_CD;
		if (m & MSR_CTS)
			bits |= TIOCM_CTS;
		if (m & MSR_DSR)
			bits |= TIOCM_DSR;
		if (m & (MSR_RI | MSR_TERI))
			bits |= TIOCM_RI;
		if (inb(iobase + com_ier))
			bits |= TIOCM_LE;
		*(int *)data = bits;
		break;
	}
	case TIOCGFLAGS: {
		int bits = 0;

		if (sc->sc_swflags & COM_SW_SOFTCAR)
			bits |= TIOCFLAG_SOFTCAR;
		if (sc->sc_swflags & COM_SW_CLOCAL)
			bits |= TIOCFLAG_CLOCAL;
		if (sc->sc_swflags & COM_SW_CRTSCTS)
			bits |= TIOCFLAG_CRTSCTS;
		if (sc->sc_swflags & COM_SW_MDMBUF)
			bits |= TIOCFLAG_MDMBUF;

		*(int *)data = bits;
		break;
	}
	case TIOCSFLAGS: {
		int userbits, driverbits = 0;

		error = suser(p->p_ucred, &p->p_acflag); 
		if (error != 0)
			return(EPERM); 

		userbits = *(int *)data;
		if ((userbits & TIOCFLAG_SOFTCAR) ||
		    (sc->sc_hwflags & COM_HW_CONSOLE))
			driverbits |= COM_SW_SOFTCAR;
		if (userbits & TIOCFLAG_CLOCAL)
			driverbits |= COM_SW_CLOCAL;
		if (userbits & TIOCFLAG_CRTSCTS)
			driverbits |= COM_SW_CRTSCTS;
		if (userbits & TIOCFLAG_MDMBUF)
			driverbits |= COM_SW_MDMBUF;

		sc->sc_swflags = driverbits;
		break;
	}
	default:
		return ENOTTY;
	}

	return 0;
}

int
comparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct com_softc *sc = com_cd.cd_devs[COMUNIT(tp->t_dev)];
	long iobase = sc->sc_iobase;
	int ospeed = comspeed(t->c_ospeed);
	u_char cfcr;
	tcflag_t oldcflag;
	int s;

	/* check requested parameters */
	if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return EINVAL;

	switch (t->c_cflag & CSIZE) {
	case CS5:
		cfcr = LCR_5BITS;
		break;
	case CS6:
		cfcr = LCR_6BITS;
		break;
	case CS7:
		cfcr = LCR_7BITS;
		break;
	case CS8:
		cfcr = LCR_8BITS;
		break;
	}
	if (t->c_cflag & PARENB) {
		cfcr |= LCR_PENAB;
		if ((t->c_cflag & PARODD) == 0)
			cfcr |= LCR_PEVEN;
	}
	if (t->c_cflag & CSTOPB)
		cfcr |= LCR_STOPB;

	s = spltty();

	if (ospeed == 0)
		outb(iobase + com_mcr, sc->sc_mcr &= ~MCR_DTR);

	/*
	 * Set the FIFO threshold based on the receive speed, if we are
	 * changing it.
	 */
	if (tp->t_ispeed != t->c_ispeed) {
		if (sc->sc_hwflags & COM_HW_FIFO)
			outb(iobase + com_fifo,
			    FIFO_ENABLE |
			    (t->c_ispeed <= 1200 ? FIFO_TRIGGER_1 : FIFO_TRIGGER_8));
	}

	if (ospeed != 0) {
		outb(iobase + com_cfcr, cfcr | LCR_DLAB);
		outb(iobase + com_dlbl, ospeed);
		outb(iobase + com_dlbh, ospeed >> 8);
		outb(iobase + com_cfcr, cfcr);
		outb(iobase + com_mcr, sc->sc_mcr |= MCR_DTR);
	} else
		outb(iobase + com_cfcr, cfcr);

	/* When not using CRTSCTS, RTS follows DTR. */
	if ((t->c_cflag & CRTSCTS) == 0) {
		if (sc->sc_mcr & MCR_DTR) {
			if ((sc->sc_mcr & MCR_RTS) == 0)
				outb(iobase + com_mcr, sc->sc_mcr |= MCR_RTS);
		} else {
			if (sc->sc_mcr & MCR_RTS)
				outb(iobase + com_mcr, sc->sc_mcr &= ~MCR_RTS);
		}
		sc->sc_dtr = MCR_DTR | MCR_RTS;
	} else
		sc->sc_dtr = MCR_DTR;

	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	oldcflag = tp->t_cflag;
	tp->t_cflag = t->c_cflag;

	/*
	 * If DCD is off and MDMBUF is changed, ask the tty layer if we should
	 * stop the device.
	 */
	if ((sc->sc_msr & MSR_DCD) == 0 &&
	    (sc->sc_swflags & COM_SW_SOFTCAR) == 0 &&
	    (oldcflag & MDMBUF) != (tp->t_cflag & MDMBUF) &&
	    (*linesw[tp->t_line].l_modem)(tp, 0) == 0) {
		outb(iobase + com_mcr, sc->sc_mcr &= ~sc->sc_dtr);
	}

	splx(s);
	return 0;
}

void
comstart(tp)
	struct tty *tp;
{
	struct com_softc *sc = com_cd.cd_devs[COMUNIT(tp->t_dev)];
	long iobase = sc->sc_iobase;
	int s;

	s = spltty();
	if (tp->t_state & (TS_TTSTOP | TS_BUSY))
		goto out;
	if ((tp->t_cflag & CRTSCTS) != 0 &&
	    (sc->sc_msr & MSR_CTS) == 0)
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		if (tp->t_outq.c_cc == 0)
			goto out;
		selwakeup(&tp->t_wsel);
	}
	tp->t_state |= TS_BUSY;
	if (sc->sc_hwflags & COM_HW_FIFO) {
		u_char buffer[16], *cp = buffer;
		int n = q_to_b(&tp->t_outq, cp, sizeof buffer);
		do {
			outb(iobase + com_data, *cp++);
		} while (--n);
	} else
		outb(iobase + com_data, getc(&tp->t_outq));
out:
	splx(s);
}

/*
 * Stop output on a line.
 */
void
comstop(tp, flag)
	struct tty *tp;
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
}

void
comdiag(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	int overflows, floods;
	int s;

	s = spltty();
	sc->sc_errors = 0;
	overflows = sc->sc_overflows;
	sc->sc_overflows = 0;
	floods = sc->sc_floods;
	sc->sc_floods = 0;
	splx(s);

	log(LOG_WARNING, "%s: %d silo overflow%s, %d ibuf overflow%s\n",
	    sc->sc_dev.dv_xname,
	    overflows, overflows == 1 ? "" : "s",
	    floods, floods == 1 ? "" : "s");
}

void
compoll(arg)
	void *arg;
{
	int unit;
	struct com_softc *sc;
	struct tty *tp;
	register u_char *ibufp;
	u_char *ibufend;
	register int c;
	int s;
	static int lsrmap[8] = {
		0,      TTY_PE,
		TTY_FE, TTY_PE|TTY_FE,
		TTY_FE, TTY_PE|TTY_FE,
		TTY_FE, TTY_PE|TTY_FE
	};

	s = spltty();
	if (comevents == 0) {
		splx(s);
		goto out;
	}
	comevents = 0;
	splx(s);

	for (unit = 0; unit < com_cd.cd_ndevs; unit++) {
		sc = com_cd.cd_devs[unit];
		if (sc == 0 || sc->sc_ibufp == sc->sc_ibuf)
			continue;

		tp = sc->sc_tty;

		s = spltty();

		ibufp = sc->sc_ibuf;
		ibufend = sc->sc_ibufp;

		if (ibufp == ibufend) {
			splx(s);
			continue;
		}

		sc->sc_ibufp = sc->sc_ibuf = (ibufp == sc->sc_ibufs[0]) ?
					     sc->sc_ibufs[1] : sc->sc_ibufs[0];
		sc->sc_ibufhigh = sc->sc_ibuf + COM_IHIGHWATER;
		sc->sc_ibufend = sc->sc_ibuf + COM_IBUFSIZE;

		if (tp == 0 || (tp->t_state & TS_ISOPEN) == 0) {
			splx(s);
			continue;
		}

		if ((tp->t_cflag & CRTSCTS) != 0 &&
		    (sc->sc_mcr & MCR_RTS) == 0)
			outb(sc->sc_iobase + com_mcr, sc->sc_mcr |= MCR_RTS);

		splx(s);

		while (ibufp < ibufend) {
			c = *ibufp++;
			if (*ibufp & LSR_OE) {
				sc->sc_overflows++;
				if (sc->sc_errors++ == 0)
					timeout(comdiag, sc, 60 * hz);
			}
			/* This is ugly, but fast. */
			c |= lsrmap[(*ibufp++ & (LSR_BI|LSR_FE|LSR_PE)) >> 2];
			(*linesw[tp->t_line].l_rint)(c, tp);
		}
	}

out:
	timeout(compoll, NULL, 1);
}

int
comintr(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	long iobase = sc->sc_iobase;
	struct tty *tp;
	u_char lsr, data, msr, delta;

	if (inb(iobase + com_iir) & IIR_NOPEND)
		return (0);

	tp = sc->sc_tty;

	for (;;) {
		lsr = inb(iobase + com_lsr);

		if (lsr & LSR_RCV_MASK) {
			register u_char *p = sc->sc_ibufp;

			comevents = 1;
			do {
				if ((lsr & LSR_BI) != 0) {
#ifdef DDB
					if (sc->sc_dev.dv_unit == comconsole) {
						Debugger();
						goto next;
					}
#endif
					data = '\0';
				} else
					data = inb(iobase + com_data);
				if (p >= sc->sc_ibufend) {
					sc->sc_floods++;
					if (sc->sc_errors++ == 0)
						timeout(comdiag, sc, 60 * hz);
				} else {
					*p++ = data;
					*p++ = lsr;
					if (p == sc->sc_ibufhigh &&
					    (tp->t_cflag & CRTSCTS) != 0)
						outb(iobase + com_mcr,
						     sc->sc_mcr &= ~MCR_RTS);
				}
			next:
				lsr = inb(iobase + com_lsr);
			} while (lsr & LSR_RCV_MASK);

			sc->sc_ibufp = p;
		}

		if (lsr & LSR_TXRDY && (tp->t_state & TS_BUSY) != 0) {
			tp->t_state &= ~TS_BUSY;
			if (tp->t_state & TS_FLUSH)
				tp->t_state &= ~TS_FLUSH;
			else
				(*linesw[tp->t_line].l_start)(tp);
		}

		msr = inb(iobase + com_msr);

		if (msr != sc->sc_msr) {
			delta = msr ^ sc->sc_msr;
			sc->sc_msr = msr;
			if ((delta & MSR_DCD) != 0 &&
			    (sc->sc_swflags & COM_SW_SOFTCAR) == 0 &&
			    (*linesw[tp->t_line].l_modem)(tp, (msr & MSR_DCD) != 0) == 0) {
				outb(iobase + com_mcr, sc->sc_mcr &= ~sc->sc_dtr);
			}
			if ((delta & msr & MSR_CTS) != 0 &&
			    (tp->t_cflag & CRTSCTS) != 0) {
				/* the line is up and we want to do rts/cts flow control */
				(*linesw[tp->t_line].l_start)(tp);
			}
		}

		if (inb(iobase + com_iir) & IIR_NOPEND)
			return (1);
	}
}

/*
 * Following are all routines needed for COM to act as console
 */
#include <dev/cons.h>

#undef  CONADDR		/* This is stupid but using devs before config .. */
#define CONADDR PICA_SYS_COM1

void
comcnprobe(cp)
	struct consdev *cp;
{

	if (!commatch1(CONADDR)) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	/* locate the major number */
	for (commajor = 0; commajor < nchrdev; commajor++)
		if (cdevsw[commajor].d_open == comopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(commajor, CONUNIT);
#ifdef	COMCONSOLE
	cp->cn_pri = CN_REMOTE;		/* Force a serial port console */
#else
	cp->cn_pri = CN_NORMAL;
#endif
}

void
comcninit(cp)
	struct consdev *cp;
{

	cominit(CONUNIT, comdefaultrate);
	comconsole = CONUNIT;
	comconsinit = 0;
}

cominit(unit, rate)
	int unit, rate;
{
	int s = splhigh();
	long iobase = CONADDR;
	u_char stat;

	outb(iobase + com_cfcr, LCR_DLAB);
	rate = comspeed(comdefaultrate);
	outb(iobase + com_dlbl, rate);
	outb(iobase + com_dlbh, rate >> 8);
	outb(iobase + com_cfcr, LCR_8BITS);
	outb(iobase + com_ier, IER_ERXRDY | IER_ETXRDY);
	outb(iobase + com_fifo, FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_4);
	stat = inb(iobase + com_iir);
	splx(s);
}

comcngetc(dev)
	dev_t dev;
{
	int s = splhigh();
	long iobase = CONADDR;
	u_char stat, c;

	while (((stat = inb(iobase + com_lsr)) & LSR_RXRDY) == 0)
		;
	c = inb(iobase + com_data);
	stat = inb(iobase + com_iir);
	splx(s);
	return c;
}

/*
 * Console kernel output character routine.
 */
void
comcnputc(dev, c)
	dev_t dev;
	int c;
{
	int s = splhigh();
	long iobase = CONADDR;
	u_char stat;
	register int timo;

#ifdef KGDB
	if (dev != kgdb_dev)
#endif
	if (comconsinit == 0) {
		(void) cominit(COMUNIT(dev), comdefaultrate);
		comconsinit = 1;
	}
	/* wait for any pending transmission to finish */
	timo = 50000;
	while (((stat = inb(iobase + com_lsr)) & LSR_TXRDY) == 0 && --timo)
		;
	outb(iobase + com_data, c);
	/* wait for this transmission to complete */
	timo = 1500000;
	while (((stat = inb(iobase + com_lsr)) & LSR_TXRDY) == 0 && --timo)
		;
	/* clear any interrupts generated by this transmission */
	stat = inb(iobase + com_iir);
	splx(s);
}

void
comcnpollc(dev, on)
	dev_t dev;
	int on;
{

}
