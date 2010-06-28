/*	$OpenBSD: apci.c,v 1.39 2010/06/28 14:13:27 deraadt Exp $	*/
/*	$NetBSD: apci.c,v 1.9 2000/11/02 00:35:05 eeh Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * Copyright (c) 1997 Michael Smith.  All rights reserved.
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
 *
 *      @(#)dca.c       8.2 (Berkeley) 1/12/94
 */

/*
 * Device driver for the APCI 8250-like UARTs found on the Apollo
 * Utility Chip on HP 9000/400-series workstations.
 *
 * There are 4 APCI UARTs on the Frodo ASIC.  The first one
 * is used to communicate with the Domain keyboard.  The second
 * one is the serial console port when the firmware is in Domain/OS
 * mode, and is mapped to select code 9 by the HP-UX firmware (except
 * on 425e models).
 *
 * We don't bother attaching a tty to the first UART; it lacks modem/flow
 * control, and is directly connected to the keyboard connector anyhow.
 */

/*
 * XXX This driver is very similar to the dca driver, and much
 * XXX more code could be shared.  (Currently, no code is shared.)
 * XXX FIXME!
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
#include <sys/device.h>
#include <sys/timeout.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/hp300spu.h>

#include <dev/cons.h>

#include <hp300/dev/dioreg.h>		/* to check for dca at 9 */
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>

#include <hp300/dev/frodoreg.h>
#include <hp300/dev/frodovar.h>
#include <hp300/dev/apcireg.h>
#include <hp300/dev/apcivar.h>
#include <hp300/dev/dcareg.h>		/* register bit definitions */

#ifdef DDB
#include <ddb/db_var.h>
#endif

struct apci_softc {
	struct	device sc_dev;		/* generic device glue */
	struct	isr sc_isr;
	struct	apciregs *sc_apci;	/* device registers */
	struct	tty *sc_tty;		/* tty glue */
	struct	timeout sc_timeout;	/* timeout */
	int	sc_ferr,
		sc_perr,
		sc_oflow,
		sc_toterr;		/* stats */
	int	sc_flags;
	u_char	sc_cua;			/* callout mode */
};

/* sc_flags */
#define	APCI_HASFIFO	0x01		/* unit has a fifo */
#define	APCI_ISCONSOLE	0x02		/* unit is console */
#define	APCI_SOFTCAR	0x04		/* soft carrier */

int	apcimatch(struct device *, void *, void *);
void	apciattach(struct device *, struct device *, void *);

struct cfattach apci_ca = {
	sizeof(struct apci_softc), apcimatch, apciattach
};

struct cfdriver apci_cd = {
	NULL, "apci", DV_TTY
};

int	apciintr(void *);
void	apcieint(struct apci_softc *, int);
void	apcimint(struct apci_softc *, u_char);
int	apciparam(struct tty *, struct termios *);
void	apcistart(struct tty *);
int	apcimctl(struct apci_softc *, int, int);
void	apcitimeout(void *);

cdev_decl(apci);

#define	APCIUNIT(x)	(minor(x) & 0x7f)
#define APCICUA(x)	(minor(x) & 0x80)

int	apcidefaultrate = TTYDEF_SPEED;

/*
 * Console support.
 */
struct apciregs *apci_cn = NULL;	/* console hardware */
int	apciconsinit;			/* has been initialized */
int	apcimajor;			/* our major number */

cons_decl(apci);

int
apcimatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct frodo_attach_args *fa = aux;

	/* Looking for an apci? */
	if (strcmp(fa->fa_name, apci_cd.cd_name) != 0)
		return (0);

	/* Are we checking a valid APCI offset? */
	switch (fa->fa_offset) {
	case FRODO_APCI_OFFSET(1):
	case FRODO_APCI_OFFSET(2):
	case FRODO_APCI_OFFSET(3):
		/* Yup, we exist! */
		return (1);
	}

	return (0);
}

void
apciattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct apci_softc *sc = (struct apci_softc *)self;
	struct apciregs *apci;
	struct frodo_attach_args *fa = aux;

	sc->sc_apci = apci =
	    (struct apciregs *)IIOV(FRODO_BASE + fa->fa_offset);
	sc->sc_flags = 0;

	/* Initialize timeout structure */
	timeout_set(&sc->sc_timeout, apcitimeout, sc);

	/* Are we the console? */
	if (apci == apci_cn) {
		sc->sc_flags |= APCI_ISCONSOLE;
		delay(100000);

		/*
		 * We didn't know which unit this would be during
		 * the console probe, so we have to fixup cn_dev here.
		 */
		cn_tab->cn_dev = makedev(apcimajor, self->dv_unit);
	}

	/* Look for a FIFO. */
	apci->ap_fifo = FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_14;
	delay(100);
	if ((apci->ap_iir & IIR_FIFO_MASK) == IIR_FIFO_MASK)
		sc->sc_flags |= APCI_HASFIFO;

	/* Establish our interrupt handler. */
	sc->sc_isr.isr_func = apciintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_priority = IPL_TTY;
	frodo_intr_establish(parent, fa->fa_line, &sc->sc_isr, self->dv_xname);

	/* Set soft carrier if requested by operator. */
	if (self->dv_cfdata->cf_flags)
		sc->sc_flags |= APCI_SOFTCAR;

	/*
	 * Need to reset baud rate, etc. of next print, so reset apciconsinit.
	 * Also make sure console is always "hardwired".
	 */
	if (sc->sc_flags & APCI_ISCONSOLE) {
		apciconsinit = 0;
		sc->sc_flags |= APCI_SOFTCAR;
		printf(": console, ");
	} else
		printf(": ");

	if (sc->sc_flags & APCI_HASFIFO)
		printf("working fifo\n");
	else
		printf("no fifo\n");
}

/* ARGSUSED */
int
apciopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = APCIUNIT(dev);
	struct apci_softc *sc;
	struct tty *tp;
	struct apciregs *apci;
	u_char code;
	int s, error = 0;

	if (unit >= apci_cd.cd_ndevs ||
	    (sc = apci_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	apci = sc->sc_apci;

	s = spltty();
	if (sc->sc_tty == NULL) {
		tp = sc->sc_tty = ttymalloc(0);
	} else
		tp = sc->sc_tty;
	splx(s);

	tp->t_oproc = apcistart;
	tp->t_param = apciparam;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/*
		 * Sanity clause: reset the chip on first open.
		 * The chip might be left in an inconsistent state
		 * if it is read inadventently.
		 */
		apciinit(apci, apcidefaultrate, CFCR_8BITS);

		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = apcidefaultrate;

		s = spltty();

		apciparam(tp, &tp->t_termios);
		ttsetwater(tp);

		/* Set the FIFO threshold based on the receive speed. */
		if (sc->sc_flags & APCI_HASFIFO)
			apci->ap_fifo = FIFO_ENABLE | FIFO_RCV_RST |
			    FIFO_XMT_RST |
			    (tp->t_ispeed <= 1200 ? FIFO_TRIGGER_1 :
			    FIFO_TRIGGER_14);

		/* Flush any pending I/O. */
		while ((apci->ap_iir & IIR_IMASK) == IIR_RXRDY)
			code = apci->ap_data;
	} else if (tp->t_state & TS_XCLUDE && suser(p, 0) != 0)
		return (EBUSY);
	else
		s = spltty();

	/* Set the modem control state. */
	(void) apcimctl(sc, MCR_DTR | MCR_RTS, DMSET);

	/* Set soft-carrier if so configured. */
	if ((sc->sc_flags & APCI_SOFTCAR) || APCICUA(dev) ||
	    (apcimctl(sc, 0, DMGET) & MSR_DCD))
		tp->t_state |= TS_CARR_ON;

	if (APCICUA(dev)) {
		if (tp->t_state & TS_ISOPEN) {
			/* Ah, but someone already is dialed in... */
			splx(s);
			return (EBUSY);
		}
		sc->sc_cua = 1;		/* We go into CUA mode */
	}

	/* Wait for carrier if necessary. */
	if (flag & O_NONBLOCK) {
		if (!APCICUA(dev) && sc->sc_cua) {
			/* Opening TTY non-blocking... but the CUA is busy */
			splx(s);
			return (EBUSY);
		}
	} else {
		while (sc->sc_cua ||
		    ((tp->t_cflag & CLOCAL) == 0 &&
		    (tp->t_state & TS_CARR_ON) == 0)) {
			tp->t_state |= TS_WOPEN;
			error = ttysleep(tp, (caddr_t)&tp->t_rawq,
			    TTIPRI | PCATCH, ttopen, 0);
			if (!APCICUA(dev) && sc->sc_cua && error == EINTR)
				continue;
			if (error) {
				if (APCICUA(dev))
					sc->sc_cua = 0;
				splx(s);
				return (error);
			}
			if (!APCICUA(dev) && sc->sc_cua)
				continue;
		}
	}

	splx(s);

	if (error == 0)
		error = (*linesw[tp->t_line].l_open)(dev, tp, p);

	if (error == 0) {
		/* clear errors, start timeout */
		sc->sc_ferr = sc->sc_perr = sc->sc_oflow = sc->sc_toterr = 0;
		timeout_add_sec(&sc->sc_timeout, 1);
	}

	return (error);
}

/* ARGSUSED */
int
apciclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct apci_softc *sc;
	struct tty *tp;
	struct apciregs *apci;
	int unit = APCIUNIT(dev);
	int s;

	sc = apci_cd.cd_devs[unit];
	apci = sc->sc_apci;
	tp = sc->sc_tty;

	(*linesw[tp->t_line].l_close)(tp, flag, p);

	s = spltty();

	apci->ap_cfcr &= ~CFCR_SBREAK;
	apci->ap_ier = 0;
	if (tp->t_cflag & HUPCL && (sc->sc_flags & APCI_SOFTCAR) == 0) {
		/* XXX perhaps only clear DTR */
		(void) apcimctl(sc, 0, DMSET);
	}
	tp->t_state &= ~(TS_BUSY | TS_FLUSH);
	sc->sc_cua = 0;
	splx(s);
	ttyclose(tp);
#if 0
	ttyfree(tp);
	sc->sc_tty = NULL;
#endif
	return (0);
}

int
apciread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct apci_softc *sc = apci_cd.cd_devs[APCIUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
apciwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct apci_softc *sc = apci_cd.cd_devs[APCIUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
apcitty(dev)
	dev_t dev;
{
	struct apci_softc *sc = apci_cd.cd_devs[APCIUNIT(dev)];

	return (sc->sc_tty);
}

int
apciintr(arg)
	void *arg;
{
	struct apci_softc *sc = arg;
	struct apciregs *apci = sc->sc_apci;
	struct tty *tp = sc->sc_tty;
	u_char iir, lsr, c;
	int iflowdone = 0, claimed = 0;

#define	RCVBYTE() \
	c = apci->ap_data; \
	if (tp != NULL && (tp->t_state & TS_ISOPEN) != 0) \
		(*linesw[tp->t_line].l_rint)(c, tp)

	for (;;) {
		iir = apci->ap_iir;	/* get UART status */

		switch (iir & IIR_IMASK) {
		case IIR_RLS:
			apcieint(sc, apci->ap_lsr);
			break;

		case IIR_RXRDY:
		case IIR_RXTOUT:
			RCVBYTE();
			if (sc->sc_flags & APCI_HASFIFO) {
				while ((lsr = apci->ap_lsr) & LSR_RCV_MASK) {
					if (lsr == LSR_RXRDY) {
						RCVBYTE();
					} else
						apcieint(sc, lsr);
				}
			}
			if (iflowdone == 0 && tp != NULL &&
			    (tp->t_cflag & CRTS_IFLOW) &&
			    tp->t_rawq.c_cc > (TTYHOG / 2)) {
				apci->ap_mcr &= ~MCR_RTS;
				iflowdone = 1;
			}
			break;

		case IIR_TXRDY:
			if (tp != NULL) {
				tp->t_state &=~ (TS_BUSY|TS_FLUSH);
				if (tp->t_line)
					(*linesw[tp->t_line].l_start)(tp);
				else
					apcistart(tp);
			}
			break;

		default:
			if (iir & IIR_NOPEND)
				return (claimed);
			log(LOG_WARNING, "%s: weird interrupt: 0x%x\n",
			    sc->sc_dev.dv_xname, iir);
			/* FALLTHROUGH */

		case IIR_MLSC:
			apcimint(sc, apci->ap_msr);
			break;
		}

		claimed = 1;
	}
}

void
apcieint(sc, stat)
	struct apci_softc *sc;
	int stat;
{
	struct tty *tp = sc->sc_tty;
	struct apciregs *apci = sc->sc_apci;
	int c;

	c = apci->ap_data;

#ifdef DDB
	if ((sc->sc_flags & APCI_ISCONSOLE) && db_console && (stat & LSR_BI)) {
		Debugger();
		return;
	}
#endif

	if (tp == NULL || (tp->t_state & TS_ISOPEN) == 0)
		return;

	if (stat & (LSR_BI | LSR_FE)) {
		c |= TTY_FE;
		sc->sc_ferr++;
	} else if (stat & LSR_PE) {
		c |= TTY_PE;
		sc->sc_perr++;
	} else if (stat & LSR_OE)
		sc->sc_oflow++;
	(*linesw[tp->t_line].l_rint)(c, tp);
}

void
apcimint(sc, stat)
	struct apci_softc *sc;
	u_char stat;
{
	struct tty *tp = sc->sc_tty;
	struct apciregs *apci = sc->sc_apci;

	if (tp == NULL)
		return;

	if ((stat & MSR_DDCD) &&
	    (sc->sc_flags & APCI_SOFTCAR) == 0) {
		if (stat & MSR_DCD)
			(void)(*linesw[tp->t_line].l_modem)(tp, 1);
		else if ((*linesw[tp->t_line].l_modem)(tp, 0) == 0)
			apci->ap_mcr &= ~(MCR_DTR | MCR_RTS);
	}

	/*
	 * CTS change.
	 * If doing HW output flow control, start/stop output as appropriate.
	 */
	if ((stat & MSR_DCTS) &&
	    (tp->t_state & TS_ISOPEN) && (tp->t_cflag & CCTS_OFLOW)) {
		if (stat & MSR_CTS) {
			tp->t_state &=~ TS_TTSTOP;
			apcistart(tp);
		} else
			tp->t_state |= TS_TTSTOP;
	}
}

int
apciioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct apci_softc *sc = apci_cd.cd_devs[APCIUNIT(dev)];
	struct tty *tp = sc->sc_tty;
	struct apciregs *apci = sc->sc_apci;
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {
	case TIOCSBRK:
		apci->ap_cfcr |= CFCR_SBREAK;
		break;

	case TIOCCBRK:
		apci->ap_cfcr &= ~CFCR_SBREAK;
		break;

	case TIOCSDTR:
		(void) apcimctl(sc, MCR_DTR | MCR_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) apcimctl(sc, MCR_DTR | MCR_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) apcimctl(sc, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		(void) apcimctl(sc, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		(void) apcimctl(sc, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = apcimctl(sc, 0, DMGET);
		break;

	case TIOCGFLAGS: {
		int bits = 0;

		if (sc->sc_flags & APCI_SOFTCAR)
			bits |= TIOCFLAG_SOFTCAR;

		if (tp->t_cflag & CLOCAL)
			bits |= TIOCFLAG_CLOCAL;

		*(int *)data = bits;
		break;
	}

	case TIOCSFLAGS: {
		int userbits;

		error = suser(p, 0);
		if (error)
			return (EPERM);

		userbits = *(int *)data;

		if ((userbits & TIOCFLAG_SOFTCAR) ||
		    (sc->sc_flags & APCI_ISCONSOLE))
			sc->sc_flags |= APCI_SOFTCAR;

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
apciparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct apci_softc *sc = apci_cd.cd_devs[APCIUNIT(tp->t_dev)];
	struct apciregs *apci = sc->sc_apci;
	int cfcr, cflag = t->c_cflag;
	int ospeed = ttspeedtab(t->c_ospeed, apcispeedtab);
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
	default:	/* XXX gcc whines about cfcr being uninitialized... */
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
		(void) apcimctl(sc, 0, DMSET);	/* hang up line */

	/*
	 * Set the FIFO threshold based on the receive speed, if we
	 * are changing it.
	 */
	if (tp->t_ispeed != t->c_ispeed) {
		if (sc->sc_flags & APCI_HASFIFO)
			apci->ap_fifo = FIFO_ENABLE |
			    (t->c_ispeed <= 1200 ? FIFO_TRIGGER_1 :
			    FIFO_TRIGGER_14);
	}

	if (ospeed != 0) {
		apci->ap_cfcr |= CFCR_DLAB;
		apci->ap_data = ospeed & 0xff;
		apci->ap_ier = (ospeed >> 8) & 0xff;
		apci->ap_cfcr = cfcr;
	} else
		apci->ap_cfcr = cfcr;

	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;

	apci->ap_ier = IER_ERXRDY | IER_ETXRDY | IER_ERLS | IER_EMSC;

	splx(s);
	return (0);
}

void
apcistart(tp)
	struct tty *tp;
{
	struct apci_softc *sc = apci_cd.cd_devs[APCIUNIT(tp->t_dev)];
	struct apciregs *apci = sc->sc_apci;
	int s, c;

	s = spltty();

	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP))
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
		if (tp->t_outq.c_cc == 0)
			goto out;
	}
	if (apci->ap_lsr & LSR_TXRDY) {
		tp->t_state |= TS_BUSY;
		if (sc->sc_flags & APCI_HASFIFO) {
			for (c = 0; c < 16 && tp->t_outq.c_cc; ++c)
				apci->ap_data = getc(&tp->t_outq);
		} else
			apci->ap_data = getc(&tp->t_outq);
	}

 out:
	splx(s);
}

/*
 * Stop output on a line.
 */
/* ARGSUSED */
int
apcistop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY)
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	splx(s);
	return (0);
}

int
apcimctl(sc, bits, how)
	struct apci_softc *sc;
	int bits, how;
{
	struct apciregs *apci = sc->sc_apci;
	int s;

	s = spltty();

	switch (how) {
	case DMSET:
		apci->ap_mcr = bits;
		break;

	case DMBIS:
		apci->ap_mcr |= bits;
		break;

	case DMBIC:
		apci->ap_mcr &= ~bits;
		break;

	case DMGET:
		bits = apci->ap_msr;
		break;
	}

	splx(s);
	return (bits);
}

void
apcitimeout(arg)
	void *arg;
{
	struct apci_softc *sc = arg;
	int ferr, perr, oflow, s;

	if (sc->sc_tty == NULL ||
	    (sc->sc_tty->t_state & TS_ISOPEN) == 0)
		return;

	/* Log any errors. */
	if (sc->sc_ferr || sc->sc_perr || sc->sc_oflow) {
		s = spltty();	/* XXX necessary? */
		ferr = sc->sc_ferr;
		perr = sc->sc_perr;
		oflow = sc->sc_oflow;
		sc->sc_ferr = sc->sc_perr = sc->sc_oflow = 0;
		splx(s);
		sc->sc_toterr += ferr + perr + oflow;
		log(LOG_WARNING,
		    "%s: %d frame, %d parity, %d overflow, %d total errors\n",
		    sc->sc_dev.dv_xname, ferr, perr, oflow, sc->sc_toterr);
	}

	timeout_add_sec(&sc->sc_timeout, 1);
}

/*
 * The following routines are required for the APCI to act as the console.
 */

void
apcicnprobe(cp)
	struct consdev *cp;
{
	volatile u_int8_t *frodoregs;

	/* locate the major number */
	for (apcimajor = 0; apcimajor < nchrdev; apcimajor++)
		if (cdevsw[apcimajor].d_open == apciopen)
			break;

	/* initialize the required fields */
	cp->cn_dev = makedev(apcimajor, 0);	/* XXX */

	/*
	 * The APCI can only be a console on a 425e; on other 4xx
	 * models, the "first" serial port is mapped to the DCA
	 * at select code 9.  See frodo.c for the autoconfiguration
	 * version of this check.
	 */
	if (machineid != HP_425 || mmuid != MMUID_425_E)
		return;

	/*
	 * Check the service switch. On the 425e, this is a physical
	 * switch, unlike other frodo-based machines, so we can use it
	 * as a serial vs internal video selector, since the PROM can not
	 * be configured for serial console.
	 */
	frodoregs = (volatile u_int8_t *)IIOV(FRODO_BASE);
	if (badaddr((caddr_t)frodoregs) == 0 &&
	    !ISSET(frodoregs[FRODO_IISR], FRODO_IISR_SERVICE))
		cp->cn_pri = CN_HIGHPRI;
	else
		cp->cn_pri = CN_LOWPRI;

	/*
	 * If our priority is higher than the currently-remembered
	 * console, install ourselves.
	 */
	if (cn_tab == NULL || cp->cn_pri > cn_tab->cn_pri) {
		cn_tab = cp;
		conscode = CONSCODE_INVALID;
	}
}

/* ARGSUSED */
void
apcicninit(cp)
	struct consdev *cp;
{

	/*
	 * We are not interested by the second console pass.
	 */
	if (consolepass != 0)
		return;

	apci_cn = (struct apciregs *)IIOV(FRODO_BASE + FRODO_APCI_OFFSET(1));
	apciinit(apci_cn, apcidefaultrate, CFCR_8BITS);
	apciconsinit = 1;
}

/* ARGSUSED */
int
apcicngetc(dev)
	dev_t dev;
{
	u_char stat;
	int c, s;

	s = splhigh();
	while (((stat = apci_cn->ap_lsr) & LSR_RXRDY) == 0)
		;
	c = apci_cn->ap_data;

	/* clear any interrupts generated by this transmission */
	stat = apci_cn->ap_iir;
	splx(s);
	return (c);
}

/* ARGSUSED */
void
apcicnputc(dev, c)
	dev_t dev;
	int c;
{
	int timo;
	u_char stat;
	int s;

	s = splhigh();

	if (apciconsinit == 0) {
		apciinit(apci_cn, apcidefaultrate, CFCR_8BITS);
		apciconsinit = 1;
	}

	/* wait for any pending transmission to finish */
	timo = 50000;
	while (((stat = apci_cn->ap_lsr) & LSR_TXRDY) == 0 && --timo)
		;

	apci_cn->ap_data = c & 0xff;

	/* wait for this transmission to complete */
	timo = 1500000;
	while (((stat = apci_cn->ap_lsr) & LSR_TXRDY) == 0 && --timo)
		;

	/* clear any interrupts generated by this transmission */
	stat = apci_cn->ap_iir;
	splx(s);
}
