/*	$NetBSD: dca.c,v 1.18 1995/12/02 18:15:50 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)dca.c	8.2 (Berkeley) 1/12/94
 */

#include "dca.h"
#if NDCA > 0

/*
 *  Driver for the 98626/98644/internal serial interface on hp300/hp400,
 *  based on the National Semiconductor INS8250/NS16550AF/WD16C552 UARTs.
 *
 *  N.B. On the hp700 and some hp300s, there is a "secret bit" with
 *  undocumented behavior.  The third bit of the Modem Control Register
 *  (MCR_IEN == 0x08) must be set to enable interrupts.  Failure to do
 *  so can result in deadlock on those machines, whereas the don't seem to
 *  be any harmful side-effects from setting this bit on non-affected
 *  machines.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <hp300/dev/device.h>
#include <hp300/dev/dcareg.h>

#include <machine/cpu.h>
#include <hp300/hp300/isr.h>

int	dcamatch();
void	dcaattach();
struct	driver dcadriver = {
	dcamatch, dcaattach, "dca",
};

struct	dca_softc {
	struct hp_device	*sc_hd;		/* device info */
	struct dcadevice	*sc_dca;	/* pointer to hardware */
	struct tty		*sc_tty;	/* our tty instance */
	struct isr		sc_isr;		/* interrupt handler */
	int			sc_oflows;	/* overflow counter */
	short			sc_flags;	/* state flags */

	/*
	 * Bits for sc_flags.
	 */
#define	DCA_ACTIVE	0x0001	/* indicates live unit */
#define	DCA_SOFTCAR	0x0002	/* indicates soft-carrier */
#define	DCA_HASFIFO	0x0004	/* indicates unit has FIFO */

} dca_softc[NDCA];

void	dcastart();
int	dcaparam(), dcaintr();
int	ndca = NDCA;
#ifdef DCACONSOLE
int	dcaconsole = DCACONSOLE;
#else
int	dcaconsole = -1;
#endif
int	dcaconsinit;
int	dcadefaultrate = TTYDEF_SPEED;
int	dcamajor;
int	dcafastservice;

struct speedtab dcaspeedtab[] = {
	0,	0,
	50,	DCABRD(50),
	75,	DCABRD(75),
	110,	DCABRD(110),
	134,	DCABRD(134),
	150,	DCABRD(150),
	200,	DCABRD(200),
	300,	DCABRD(300),
	600,	DCABRD(600),
	1200,	DCABRD(1200),
	1800,	DCABRD(1800),
	2400,	DCABRD(2400),
	4800,	DCABRD(4800),
	9600,	DCABRD(9600),
	19200,	DCABRD(19200),
	38400,	DCABRD(38400),
	-1,	-1
};

#ifdef KGDB
#include <machine/remote-sl.h>

extern dev_t kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif

#define	DCAUNIT(x)		minor(x)

#ifdef DEBUG
long	fifoin[17];
long	fifoout[17];
long	dcaintrcount[16];
long	dcamintcount[16];
#endif

int
dcamatch(hd)
	register struct hp_device *hd;
{
	struct dcadevice *dca = (struct dcadevice *)hd->hp_addr;
	struct dca_softc *sc = &dca_softc[hd->hp_unit];

	if (dca->dca_id != DCAID0 &&
	    dca->dca_id != DCAREMID0 &&
	    dca->dca_id != DCAID1 &&
	    dca->dca_id != DCAREMID1)
		return (0);

	hd->hp_ipl = DCAIPL(dca->dca_ic);
	sc->sc_hd = hd;

	return (1);
}

void
dcaattach(hd)
	register struct hp_device *hd;
{
	int unit = hd->hp_unit;
	struct dcadevice *dca = (struct dcadevice *)hd->hp_addr;
	struct dca_softc *sc = &dca_softc[unit];

	if (unit == dcaconsole)
		DELAY(100000);

	dca->dca_reset = 0xFF;
	DELAY(100);

	/* look for a NS 16550AF UART with FIFOs */
	dca->dca_fifo = FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_14;
	DELAY(100);
	if ((dca->dca_iir & IIR_FIFO_MASK) == IIR_FIFO_MASK)
		sc->sc_flags |= DCA_HASFIFO;

	sc->sc_dca = dca;

	/* Establish interrupt handler. */
	sc->sc_isr.isr_ipl = hd->hp_ipl;
	sc->sc_isr.isr_arg = unit;
	sc->sc_isr.isr_intr = dcaintr;
	isrlink(&sc->sc_isr);

	sc->sc_flags |= DCA_ACTIVE;
	if (hd->hp_flags)
		sc->sc_flags |= DCA_SOFTCAR;

	/* Enable interrupts. */
	dca->dca_ic = IC_IE;

	/*
	 * Need to reset baud rate, etc. of next print so reset dcaconsinit.
	 * Also make sure console is always "hardwired."
	 */
	if (unit == dcaconsole) {
		dcaconsinit = 0;
		sc->sc_flags |= DCA_SOFTCAR;
		printf(": console, ");
	} else
		printf(": ");

	if (sc->sc_flags & DCA_HASFIFO)
		printf("working fifo\n");
	else
		printf("no fifo\n");

#ifdef KGDB
	if (kgdb_dev == makedev(dcamajor, unit)) {
		if (dcaconsole == unit)
			kgdb_dev = NODEV; /* can't debug over console port */
		else {
			(void) dcainit(sc, kgdb_rate);
			dcaconsinit = 1;	/* don't re-init in dcaputc */
			if (kgdb_debug_init) {
				/*
				 * Print prefix of device name,
				 * let kgdb_connect print the rest.
				 */
				printf("%s: ", sc->sc_hd->hp_xname);
				kgdb_connect(1);
			} else
				printf("%s: kgdb enabled\n",
				    sc->sc_hd->hp_xname);
		}
	}
#endif
}

/* ARGSUSED */
int
dcaopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = DCAUNIT(dev);
	struct dca_softc *sc;
	struct tty *tp;
	struct dcadevice *dca;
	u_char code;
	int s, error = 0;
 
	if (unit >= NDCA)
		return (ENXIO);

	sc = &dca_softc[unit];
	if ((sc->sc_flags & DCA_ACTIVE) == 0)
		return (ENXIO);

	dca = sc->sc_dca;

	if (sc->sc_tty == NULL)
		tp = sc->sc_tty = ttymalloc();
	else
		tp = sc->sc_tty;
	tp->t_oproc = dcastart;
	tp->t_param = dcaparam;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/*
		 * Sanity clause: reset the card on first open.
		 * The card might be left in an inconsistent state
		 * if card memory is read inadvertently.
		 */
		dcainit(sc, dcadefaultrate);

		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = dcadefaultrate;

		s = spltty();

		dcaparam(tp, &tp->t_termios);
		ttsetwater(tp);

		/* Set the FIFO threshold based on the receive speed. */
		if (sc->sc_flags & DCA_HASFIFO)
                        dca->dca_fifo = FIFO_ENABLE | FIFO_RCV_RST |
                            FIFO_XMT_RST |
			    (tp->t_ispeed <= 1200 ? FIFO_TRIGGER_1 :
			    FIFO_TRIGGER_14);   

		/* Flush any pending I/O */
		while ((dca->dca_iir & IIR_IMASK) == IIR_RXRDY)
			code = dca->dca_data;

	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	else
		s = spltty();

	/* Set modem control state. */
	(void) dcamctl(sc, MCR_DTR | MCR_RTS, DMSET);

	/* Set soft-carrier if so configured. */
	if ((sc->sc_flags & DCA_SOFTCAR) || (dcamctl(sc, 0, DMGET) & MSR_DCD))
		tp->t_state |= TS_CARR_ON;

	/* Wait for carrier if necessary. */
	if ((flag & O_NONBLOCK) == 0)
		while ((tp->t_cflag & CLOCAL) == 0 &&
		    (tp->t_state & TS_CARR_ON) == 0) {
			tp->t_state |= TS_WOPEN; 
			error = ttysleep(tp, (caddr_t)&tp->t_rawq,
			    TTIPRI | PCATCH, ttopen, 0);
			if (error) {
				splx(s);
				return (error);
			}
		}
	splx(s);

	if (error == 0)
		error = (*linesw[tp->t_line].l_open)(dev, tp);
	/*
	 * XXX hack to speed up unbuffered builtin port.
	 * If dca_fastservice is set, a level 5 interrupt
	 * will be directed to dcaintr first.
	 */
	if (error == 0 && unit == 0 && (sc->sc_flags & DCA_HASFIFO) == 0)
		dcafastservice = 1;

	return (error);
}
 
/*ARGSUSED*/
int
dcaclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct dca_softc *sc;
	register struct tty *tp;
	register struct dcadevice *dca;
	register int unit;
	int s;
 
	unit = DCAUNIT(dev);

	if (unit == 0)
		dcafastservice = 0;

	sc = &dca_softc[unit];
	dca = sc->sc_dca;
	tp = sc->sc_tty;
	(*linesw[tp->t_line].l_close)(tp, flag);

	s = spltty();

	dca->dca_cfcr &= ~CFCR_SBREAK;
#ifdef KGDB
	/* do not disable interrupts if debugging */
	if (dev != kgdb_dev)
#endif
	dca->dca_ier = 0;
	if (tp->t_cflag & HUPCL && (sc->sc_flags & DCA_SOFTCAR) == 0) {
		/* XXX perhaps only clear DTR */
		(void) dcamctl(sc, 0, DMSET);
	}
	tp->t_state &= ~(TS_BUSY | TS_FLUSH);
	splx(s);
	ttyclose(tp);
#if 0
	ttyfree(tp);
	sc->sc_tty = NULL;
#endif
	return (0);
}
 
int
dcaread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit = DCAUNIT(dev);
	struct dca_softc *sc = &dca_softc[unit];
	struct tty *tp = sc->sc_tty;
	int error, of;
 
	of = sc->sc_oflows;
	error = (*linesw[tp->t_line].l_read)(tp, uio, flag);
	/*
	 * XXX hardly a reasonable thing to do, but reporting overflows
	 * at interrupt time just exacerbates the problem.
	 */
	if (sc->sc_oflows != of)
		log(LOG_WARNING, "%s: silo overflow\n", sc->sc_hd->hp_xname);
	return (error);
}
 
int
dcawrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct tty *tp = dca_softc[DCAUNIT(dev)].sc_tty;
 
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
dcatty(dev)
	dev_t dev;
{

	return (dca_softc[DCAUNIT(dev)].sc_tty);
}
 
int
dcaintr(unit)
	register int unit;
{
	struct dca_softc *sc = &dca_softc[unit];
	register struct dcadevice *dca = sc->sc_dca;
	register struct tty *tp = sc->sc_tty;
	register u_char code;
	int iflowdone = 0;

	/*
	 * If interrupts aren't enabled, then the interrupt can't
	 * be for us.
	 */
	if ((dca->dca_ic & (IC_IR|IC_IE)) != (IC_IR|IC_IE))
		return (0);

	for (;;) {
		code = dca->dca_iir;
#ifdef DEBUG
		dcaintrcount[code & IIR_IMASK]++;
#endif
		switch (code & IIR_IMASK) {
		case IIR_NOPEND:
			return (1);
		case IIR_RXTOUT:
		case IIR_RXRDY:
			/* do time-critical read in-line */
/*
 * Process a received byte.  Inline for speed...
 */
#ifdef KGDB
#define	RCVBYTE() \
			code = dca->dca_data; \
			if ((tp->t_state & TS_ISOPEN) == 0) { \
				if (code == FRAME_END && \
				    kgdb_dev == makedev(dcamajor, unit)) \
					kgdb_connect(0); /* trap into kgdb */ \
			} else \
				(*linesw[tp->t_line].l_rint)(code, tp)
#else
#define	RCVBYTE() \
			code = dca->dca_data; \
			if ((tp->t_state & TS_ISOPEN) != 0) \
				(*linesw[tp->t_line].l_rint)(code, tp)
#endif
			RCVBYTE();
			if (sc->sc_flags & DCA_HASFIFO) {
#ifdef DEBUG
				register int fifocnt = 1;
#endif
				while ((code = dca->dca_lsr) & LSR_RCV_MASK) {
					if (code == LSR_RXRDY) {
						RCVBYTE();
					} else
						dcaeint(sc, code);
#ifdef DEBUG
					fifocnt++;
#endif
				}
#ifdef DEBUG
				if (fifocnt > 16)
					fifoin[0]++;
				else
					fifoin[fifocnt]++;
#endif
			}
			if (!iflowdone && (tp->t_cflag&CRTS_IFLOW) &&
			    tp->t_rawq.c_cc > TTYHOG/2) {
				dca->dca_mcr &= ~MCR_RTS;
				iflowdone = 1;
			}
			break;
		case IIR_TXRDY:
			tp->t_state &=~ (TS_BUSY|TS_FLUSH);
			if (tp->t_line)
				(*linesw[tp->t_line].l_start)(tp);
			else
				dcastart(tp);
			break;
		case IIR_RLS:
			dcaeint(sc, dca->dca_lsr);
			break;
		default:
			if (code & IIR_NOPEND)
				return (1);
			log(LOG_WARNING, "%s: weird interrupt: 0x%x\n",
			    sc->sc_hd->hp_xname, code);
			/* fall through */
		case IIR_MLSC:
			dcamint(sc);
			break;
		}
	}
}

dcaeint(sc, stat)
	struct dca_softc *sc;
	int stat;
{
	struct tty *tp = sc->sc_tty;
	struct dcadevice *dca = sc->sc_dca;
	int c;

	c = dca->dca_data;
	if ((tp->t_state & TS_ISOPEN) == 0) {
#ifdef KGDB
		/* we don't care about parity errors */
		if (((stat & (LSR_BI|LSR_FE|LSR_PE)) == LSR_PE) &&
		    kgdb_dev == makedev(dcamajor, sc->sc_hd->hp_unit)
		    && c == FRAME_END)
			kgdb_connect(0); /* trap into kgdb */
#endif
		return;
	}
	if (stat & (LSR_BI | LSR_FE))
		c |= TTY_FE;
	else if (stat & LSR_PE)
		c |= TTY_PE;
	else if (stat & LSR_OE)
		sc->sc_oflows++;
	(*linesw[tp->t_line].l_rint)(c, tp);
}

dcamint(sc)
	struct dca_softc *sc;
{
	struct tty *tp = sc->sc_tty;
	struct dcadevice *dca = sc->sc_dca;
	u_char stat;

	stat = dca->dca_msr;
#ifdef DEBUG
	dcamintcount[stat & 0xf]++;
#endif
	if ((stat & MSR_DDCD) &&
	    (sc->sc_flags & DCA_SOFTCAR) == 0) {
		if (stat & MSR_DCD)
			(void)(*linesw[tp->t_line].l_modem)(tp, 1);
		else if ((*linesw[tp->t_line].l_modem)(tp, 0) == 0)
			dca->dca_mcr &= ~(MCR_DTR | MCR_RTS);
	}
	/*
	 * CTS change.
	 * If doing HW output flow control start/stop output as appropriate.
	 */
	if ((stat & MSR_DCTS) &&
	    (tp->t_state & TS_ISOPEN) && (tp->t_cflag & CCTS_OFLOW)) {
		if (stat & MSR_CTS) {
			tp->t_state &=~ TS_TTSTOP;
			dcastart(tp);
		} else {
			tp->t_state |= TS_TTSTOP;
		}
	}
}

int
dcaioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = DCAUNIT(dev);
	struct dca_softc *sc = &dca_softc[unit];
	struct tty *tp = sc->sc_tty;
	struct dcadevice *dca = sc->sc_dca;
	int error;
 
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {
	case TIOCSBRK:
		dca->dca_cfcr |= CFCR_SBREAK;
		break;

	case TIOCCBRK:
		dca->dca_cfcr &= ~CFCR_SBREAK;
		break;

	case TIOCSDTR:
		(void) dcamctl(sc, MCR_DTR | MCR_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) dcamctl(sc, MCR_DTR | MCR_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) dcamctl(sc, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		(void) dcamctl(sc, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		(void) dcamctl(sc, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = dcamctl(sc, 0, DMGET);
		break;

	case TIOCGFLAGS: {
		int bits = 0;

		if (sc->sc_flags & DCA_SOFTCAR)
			bits |= TIOCFLAG_SOFTCAR;

		if (tp->t_cflag & CLOCAL)
			bits |= TIOCFLAG_CLOCAL;

		*(int *)data = bits;
		break;
	}

	case TIOCSFLAGS: {
		int userbits;

		error = suser(p->p_ucred, &p->p_acflag);
		if (error)
			return (EPERM);

		userbits = *(int *)data;

		if ((userbits & TIOCFLAG_SOFTCAR) || (unit == dcaconsole))
			sc->sc_flags |= DCA_SOFTCAR;

		if (userbits & TIOCFLAG_CLOCAL)
			tp->t_cflag |= CLOCAL;

		break;
	}

	default:
		return (ENOTTY);
	}
	return (0);
}

int
dcaparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	int unit = DCAUNIT(tp->t_dev);
	struct dca_softc *sc = &dca_softc[unit];
	struct dcadevice *dca = sc->sc_dca;
	int cfcr, cflag = t->c_cflag;
	int ospeed = ttspeedtab(t->c_ospeed, dcaspeedtab);
	int s;
 
	/* check requested parameters */
        if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
                return (EINVAL);

	switch (cflag & CSIZE) {
	case CS5:
		cfcr = CFCR_5BITS;
		break;

	case CS6:
		cfcr = CFCR_6BITS;
		break;

	case CS7:
		cfcr = CFCR_7BITS;
		break;

	case CS8:
		cfcr = CFCR_8BITS;
		break;
	}
	if (cflag & PARENB) {
		cfcr |= CFCR_PENAB;
		if ((cflag & PARODD) == 0)
			cfcr |= CFCR_PEVEN;
	}
	if (cflag & CSTOPB)
		cfcr |= CFCR_STOPB;

	s = spltty();

	if (ospeed == 0)
		(void) dcamctl(sc, 0, DMSET);	/* hang up line */

	/*
	 * Set the FIFO threshold based on the recieve speed, if we
	 * are changing it.
	 */
	if (tp->t_ispeed != t->c_ispeed) {
		if (sc->sc_flags & DCA_HASFIFO)
			dca->dca_fifo = FIFO_ENABLE |
			    (t->c_ispeed <= 1200 ? FIFO_TRIGGER_1 :
			    FIFO_TRIGGER_14);
	}

	if (ospeed != 0) {
		dca->dca_cfcr |= CFCR_DLAB;
		dca->dca_data = ospeed & 0xFF;
		dca->dca_ier = ospeed >> 8;
		dca->dca_cfcr = cfcr;
	} else
		dca->dca_cfcr = cfcr;

	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;

	dca->dca_ier = IER_ERXRDY | IER_ETXRDY | IER_ERLS | IER_EMSC;
	dca->dca_mcr |= MCR_IEN;

	splx(s);
	return (0);
}
 
void
dcastart(tp)
	register struct tty *tp;
{
	int s, c, unit = DCAUNIT(tp->t_dev);
	struct dca_softc *sc = &dca_softc[unit];
	struct dcadevice *dca = sc->sc_dca;
 
	s = spltty();

	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP))
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
	if (dca->dca_lsr & LSR_TXRDY) {
		tp->t_state |= TS_BUSY;
		if (sc->sc_flags & DCA_HASFIFO) {
			for (c = 0; c < 16 && tp->t_outq.c_cc; ++c)
				dca->dca_data = getc(&tp->t_outq);
#ifdef DEBUG
			if (c > 16)
				fifoout[0]++;
			else
				fifoout[c]++;
#endif
		} else
			dca->dca_data = getc(&tp->t_outq); 
	}

out:
	splx(s);
}

/*
 * Stop output on a line.
 */
/*ARGSUSED*/
int
dcastop(tp, flag)
	register struct tty *tp;
	int flag;
{
	register int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
}
 
dcamctl(sc, bits, how)
	struct dca_softc *sc;
	int bits, how;
{
	struct dcadevice *dca = sc->sc_dca;
	int s;

	/*
	 * Always make sure MCR_IEN is set (unless setting to 0)
	 */
#ifdef KGDB
	if (how == DMSET && kgdb_dev == makedev(dcamajor, sc->sc_hd->hp_unit))
		bits |= MCR_IEN;
	else
#endif
	if (how == DMBIS || (how == DMSET && bits))
		bits |= MCR_IEN;
	else if (how == DMBIC)
		bits &= ~MCR_IEN;
	s = spltty();

	switch (how) {
	case DMSET:
		dca->dca_mcr = bits;
		break;

	case DMBIS:
		dca->dca_mcr |= bits;
		break;

	case DMBIC:
		dca->dca_mcr &= ~bits;
		break;

	case DMGET:
		bits = dca->dca_msr;
		break;
	}
	(void) splx(s);
	return (bits);
}

/*
 * Following are all routines needed for DCA to act as console
 */
#include <dev/cons.h>

void
dcacnprobe(cp)
	struct consdev *cp;
{
	struct dca_softc *sc;
	int unit;

	/* locate the major number */
	for (dcamajor = 0; dcamajor < nchrdev; dcamajor++)
		if (cdevsw[dcamajor].d_open == dcaopen)
			break;

	/* XXX: ick */
	unit = CONUNIT;
	sc = &dca_softc[unit];

	sc->sc_dca = (struct dcadevice *) sctova(CONSCODE);

	/* make sure hardware exists */
	if (badaddr((short *)sc->sc_dca)) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	/* initialize required fields */
	cp->cn_dev = makedev(dcamajor, unit);

	switch (sc->sc_dca->dca_id) {
	case DCAID0:
	case DCAID1:
		cp->cn_pri = CN_NORMAL;
		break;
	case DCAREMID0:
	case DCAREMID1:
		cp->cn_pri = CN_REMOTE;
		break;
	default:
		cp->cn_pri = CN_DEAD;
		break;
	}

	/*
	 * If dcaconsole is initialized, raise our priority.
	 */
	if (dcaconsole == unit)
		cp->cn_pri = CN_REMOTE;
#ifdef KGDB
	if (major(kgdb_dev) == 1)			/* XXX */
		kgdb_dev = makedev(dcamajor, minor(kgdb_dev));
#endif
}

void
dcacninit(cp)
	struct consdev *cp;
{
	int unit = DCAUNIT(cp->cn_dev);
	struct dca_softc *sc = &dca_softc[unit];

	dcainit(sc, dcadefaultrate);
	dcaconsole = unit;
	dcaconsinit = 1;
}

dcainit(sc, rate)
	struct dca_softc *sc;
	int rate;
{
	struct dcadevice *dca = sc->sc_dca;
	int s;
	short stat;

#ifdef lint
	stat = sc->sc_hd->hp_unit; if (stat) return;
#endif

	s = splhigh();

	dca->dca_reset = 0xFF;
	DELAY(100);
	dca->dca_ic = IC_IE;

	dca->dca_cfcr = CFCR_DLAB;
	rate = ttspeedtab(rate, dcaspeedtab);
	dca->dca_data = rate & 0xFF;
	dca->dca_ier = rate >> 8;
	dca->dca_cfcr = CFCR_8BITS;
	dca->dca_ier = IER_ERXRDY | IER_ETXRDY;
	dca->dca_fifo = FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_14;
	dca->dca_mcr |= MCR_IEN;
	DELAY(100);
	stat = dca->dca_iir;
	splx(s);
}

int
dcacngetc(dev)
	dev_t dev;
{
	struct dca_softc *sc = &dca_softc[DCAUNIT(dev)];
	struct dcadevice *dca = sc->sc_dca;
	u_char stat;
	int c, s;

#ifdef lint
	stat = dev; if (stat) return (0);
#endif
	s = splhigh();
	while (((stat = dca->dca_lsr) & LSR_RXRDY) == 0)
		;
	c = dca->dca_data;
	stat = dca->dca_iir;
	splx(s);
	return (c);
}

/*
 * Console kernel output character routine.
 */
void
dcacnputc(dev, c)
	dev_t dev;
	register int c;
{
	struct dca_softc *sc = &dca_softc[DCAUNIT(dev)];
	struct dcadevice *dca = sc->sc_dca;
	int timo;
	u_char stat;
	int s = splhigh();

#ifdef lint
	stat = dev; if (stat) return;
#endif
	if (dcaconsinit == 0) {
		(void) dcainit(sc, dcadefaultrate);
		dcaconsinit = 1;
	}
	/* wait for any pending transmission to finish */
	timo = 50000;
	while (((stat = dca->dca_lsr) & LSR_TXRDY) == 0 && --timo)
		;
	dca->dca_data = c;
	/* wait for this transmission to complete */
	timo = 1500000;
	while (((stat = dca->dca_lsr) & LSR_TXRDY) == 0 && --timo)
		;
	/*
	 * If the "normal" interface was busy transfering a character
	 * we must let our interrupt through to keep things moving.
	 * Otherwise, we clear the interrupt that we have caused.
	 */
	if ((sc->sc_tty->t_state & TS_BUSY) == 0)
		stat = dca->dca_iir;
	splx(s);
}
#endif
