/*	$OpenBSD: dca.c,v 1.39 2010/04/12 12:57:51 tedu Exp $	*/
/*	$NetBSD: dca.c,v 1.35 1997/05/05 20:58:18 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997 Jason R. Thorpe.  All rights reserved.
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
 *	@(#)dca.c	8.2 (Berkeley) 1/12/94
 */

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
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/cons.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>
#include <hp300/dev/diodevs.h>
#include <hp300/dev/dcareg.h>

#ifdef DDB
#include <ddb/db_var.h>
#endif

struct	dca_softc {
	struct device		sc_dev;		/* generic device glue */
	struct isr		sc_isr;
	struct dcadevice	*sc_dca;	/* pointer to hardware */
	struct tty		*sc_tty;	/* our tty instance */
	int			sc_oflows;	/* overflow counter */
	short			sc_flags;	/* state flags */
	u_char			sc_cua;		/* callout mode */

	/*
	 * Bits for sc_flags.
	 */
#define	DCA_ACTIVE	0x0001	/* indicates live unit */
#define	DCA_SOFTCAR	0x0002	/* indicates soft-carrier */
#define	DCA_HASFIFO	0x0004	/* indicates unit has FIFO */
#define DCA_ISCONSOLE	0x0008	/* indicates unit is console */

};

int	dcamatch(struct device *, void *, void *);
void	dcaattach(struct device *, struct device *, void *);

struct cfattach dca_ca = {
	sizeof(struct dca_softc), dcamatch, dcaattach
};

struct cfdriver dca_cd = {
	NULL, "dca", DV_TTY
};

int	dcadefaultrate = TTYDEF_SPEED;
int	dcamajor;

cdev_decl(dca);

int	dcaintr(void *);
void	dcaeint(struct dca_softc *, int);
void	dcamint(struct dca_softc *);

int	dcaparam(struct tty *, struct termios *);
void	dcastart(struct tty *);
int	dcastop(struct tty *, int);
int	dcamctl(struct dca_softc *, int, int);
void	dcainit(struct dcadevice *, int);

int	dca_console_scan(int, caddr_t, void *);
cons_decl(dca);

/*
 * Stuff for DCA console support.
 */
static	struct dcadevice *dca_cn = NULL;	/* pointer to hardware */
static	int dcaconsinit;			/* has been initialized */

const struct speedtab dcaspeedtab[] = {
	{	0,	0		},
	{	50,	DCABRD(50)	},
	{	75,	DCABRD(75)	},
	{	110,	DCABRD(110)	},
	{	134,	DCABRD(134)	},
	{	150,	DCABRD(150)	},
	{	200,	DCABRD(200)	},
	{	300,	DCABRD(300)	},
	{	600,	DCABRD(600)	},
	{	1200,	DCABRD(1200)	},
	{	1800,	DCABRD(1800)	},
	{	2400,	DCABRD(2400)	},
	{	4800,	DCABRD(4800)	},
	{	9600,	DCABRD(9600)	},
	{	19200,	DCABRD(19200)	},
	{	38400,	DCABRD(38400)	},
	{	-1,	-1		},
};

#ifdef KGDB
#include <machine/remote-sl.h>

extern dev_t kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif

#define	DCAUNIT(x)		(minor(x) & 0x7f)
#define DCACUA(x)		(minor(x) & 0x80)

#ifdef DEBUG
long	fifoin[17];
long	fifoout[17];
long	dcaintrcount[16];
long	dcamintcount[16];
#endif

void	dcainit(struct dcadevice *, int);

int
dcamatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct dio_attach_args *da = aux;

	switch (da->da_id) {
	case DIO_DEVICE_ID_DCA0:
	case DIO_DEVICE_ID_DCA0REM:
	case DIO_DEVICE_ID_DCA1:
	case DIO_DEVICE_ID_DCA1REM:
		return (1);
	}

	return (0);
}

void
dcaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct dca_softc *sc = (struct dca_softc *)self;
	struct dio_attach_args *da = aux;
	struct dcadevice *dca;
	int unit = self->dv_unit;
	int scode = da->da_scode;
	int ipl;

	if (scode == conscode) {
		dca = (struct dcadevice *)conaddr;
		sc->sc_flags |= DCA_ISCONSOLE;
		DELAY(100000);

		/*
		 * We didn't know which unit this would be during
		 * the console probe, so we have to fixup cn_dev here.
		 */
		cn_tab->cn_dev = makedev(dcamajor, unit);
	} else {
		dca = (struct dcadevice *)iomap(dio_scodetopa(da->da_scode),
		    da->da_size);
		if (dca == NULL) {
			printf("\n%s: can't map registers\n",
			    sc->sc_dev.dv_xname);
			return;
		}
	}

	sc->sc_dca = dca;

	ipl = DIO_IPL(dca);
	printf(" ipl %d", ipl);

	dca->dca_reset = 0xFF;
	DELAY(100);

	/* look for a NS 16550AF UART with FIFOs */
	dca->dca_fifo = FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_14;
	DELAY(100);
	if ((dca->dca_iir & IIR_FIFO_MASK) == IIR_FIFO_MASK)
		sc->sc_flags |= DCA_HASFIFO;

	/* Establish interrupt handler. */
	sc->sc_isr.isr_func = dcaintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = ipl;
	sc->sc_isr.isr_priority = IPL_TTY;
	dio_intr_establish(&sc->sc_isr, self->dv_xname);

	sc->sc_flags |= DCA_ACTIVE;
	if (self->dv_cfdata->cf_flags)
		sc->sc_flags |= DCA_SOFTCAR;

	/* Enable interrupts. */
	dca->dca_ic = IC_IE;

	/*
	 * Need to reset baud rate, etc. of next print so reset dcaconsinit.
	 * Also make sure console is always "hardwired."
	 */
	if (sc->sc_flags & DCA_ISCONSOLE) {
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
		if (sc->sc_flags & DCA_ISCONSOLE)
			kgdb_dev = NODEV; /* can't debug over console port */
		else {
			dcainit(dca, kgdb_rate);
			dcaconsinit = 1;	/* don't re-init in dcaputc */
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

	if (unit >= dca_cd.cd_ndevs ||
	    (sc = dca_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	if ((sc->sc_flags & DCA_ACTIVE) == 0)
		return (ENXIO);

	dca = sc->sc_dca;

	s = spltty();
	if (sc->sc_tty == NULL) {
		tp = sc->sc_tty = ttymalloc();
	} else
		tp = sc->sc_tty;
	splx(s);

	tp->t_oproc = dcastart;
	tp->t_param = dcaparam;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		/*
		 * Sanity clause: reset the card on first open.
		 * The card might be left in an inconsistent state
		 * if card memory is read inadvertently.
		 */
		dcainit(dca, dcadefaultrate);

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

	} else if (tp->t_state&TS_XCLUDE && suser(p, 0) != 0)
		return (EBUSY);
	else
		s = spltty();

	/* Set modem control state. */
	(void) dcamctl(sc, MCR_DTR | MCR_RTS, DMSET);

	/* Set soft-carrier if so configured. */
	if ((sc->sc_flags & DCA_SOFTCAR) || DCACUA(dev) ||
	    (dcamctl(sc, 0, DMGET) & MSR_DCD))
		tp->t_state |= TS_CARR_ON;

	if (DCACUA(dev)) {
		if (tp->t_state & TS_ISOPEN) {
			/* Ah, but someone already is dialed in... */
			splx(s);
			return (EBUSY);
		}
		sc->sc_cua = 1;		/* We go into CUA mode */
	}

	/* Wait for carrier if necessary. */
	if (flag & O_NONBLOCK) {
		if (!DCACUA(dev) && sc->sc_cua) {
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
			if (!DCACUA(dev) && sc->sc_cua && error == EINTR)
				continue;
			if (error) {
				if (DCACUA(dev))
					sc->sc_cua = 0;
				splx(s);
				return (error);
			}
			if (!DCACUA(dev) && sc->sc_cua)
				continue;
		}
	}
	splx(s);

	if (error == 0)
		error = (*linesw[tp->t_line].l_open)(dev, tp, p);

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
	struct tty *tp;
	struct dcadevice *dca;
	int unit;
	int s;

	unit = DCAUNIT(dev);

	sc = dca_cd.cd_devs[unit];

	dca = sc->sc_dca;
	tp = sc->sc_tty;
	(*linesw[tp->t_line].l_close)(tp, flag, p);

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
dcaread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit = DCAUNIT(dev);
	struct dca_softc *sc;
	struct tty *tp;
	int error, of;

	sc = dca_cd.cd_devs[unit];

	tp = sc->sc_tty;
	of = sc->sc_oflows;
	error = (*linesw[tp->t_line].l_read)(tp, uio, flag);
	/*
	 * XXX hardly a reasonable thing to do, but reporting overflows
	 * at interrupt time just exacerbates the problem.
	 */
	if (sc->sc_oflows != of)
		log(LOG_WARNING, "%s: silo overflow\n", sc->sc_dev.dv_xname);
	return (error);
}

int
dcawrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct dca_softc *sc = dca_cd.cd_devs[DCAUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
dcatty(dev)
	dev_t dev;
{
	struct dca_softc *sc = dca_cd.cd_devs[DCAUNIT(dev)];

	return (sc->sc_tty);
}

int
dcaintr(arg)
	void *arg;
{
	struct dca_softc *sc = arg;
#ifdef KGDB
	int unit = sc->sc_dev.dv_unit;
#endif
	struct dcadevice *dca = sc->sc_dca;
	struct tty *tp = sc->sc_tty;
	u_char code;
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
			if (tp != NULL) { \
				if ((tp->t_state & TS_ISOPEN) == 0) { \
					if (code == FRAME_END && \
					    kgdb_dev == makedev(dcamajor, unit)) \
						kgdb_connect(0); /* trap into kgdb */ \
				} else \
					(*linesw[tp->t_line].l_rint)(code, tp) \
			}
#else
#define	RCVBYTE() \
			code = dca->dca_data; \
			if (tp != NULL && (tp->t_state & TS_ISOPEN) != 0) \
				(*linesw[tp->t_line].l_rint)(code, tp)
#endif
			RCVBYTE();
			if (sc->sc_flags & DCA_HASFIFO) {
#ifdef DEBUG
				int fifocnt = 1;
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
			if (iflowdone == 0 && tp != NULL &&
			    (tp->t_cflag & CRTS_IFLOW) &&
			    tp->t_rawq.c_cc > (TTYHOG / 2)) {
				dca->dca_mcr &= ~MCR_RTS;
				iflowdone = 1;
			}
			break;
		case IIR_TXRDY:
			if (tp != NULL) {
				tp->t_state &=~ (TS_BUSY|TS_FLUSH);
				if (tp->t_line)
					(*linesw[tp->t_line].l_start)(tp);
				else
					dcastart(tp);
			}
			break;
		case IIR_RLS:
			dcaeint(sc, dca->dca_lsr);
			break;
		default:
			if (code & IIR_NOPEND)
				return (1);
			log(LOG_WARNING, "%s: weird interrupt: 0x%x\n",
			    sc->sc_dev.dv_xname, code);
			/* FALLTHROUGH */
		case IIR_MLSC:
			dcamint(sc);
			break;
		}
	}
}

void
dcaeint(sc, stat)
	struct dca_softc *sc;
	int stat;
{
	struct tty *tp = sc->sc_tty;
	struct dcadevice *dca = sc->sc_dca;
	int c;

	c = dca->dca_data;

#if defined(DDB) && !defined(KGDB)
	if ((sc->sc_flags & DCA_ISCONSOLE) && db_console && (stat & LSR_BI)) {
		Debugger();
		return;
	}
#endif

	if (tp == NULL)
		return;

	if ((tp->t_state & TS_ISOPEN) == 0) {
#ifdef KGDB
		/* we don't care about parity errors */
		if (((stat & (LSR_BI|LSR_FE|LSR_PE)) == LSR_PE) &&
		    kgdb_dev == makedev(dcamajor, sc->sc_hd->hp_unit) &&
		    c == FRAME_END)
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

void
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

	if (tp == NULL)
		return;

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
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = DCAUNIT(dev);
	struct dca_softc *sc = dca_cd.cd_devs[unit];
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

		error = suser(p, 0);
		if (error)
			return (EPERM);

		userbits = *(int *)data;

		if ((userbits & TIOCFLAG_SOFTCAR) ||
		    (sc->sc_flags & DCA_ISCONSOLE))
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
	struct tty *tp;
	struct termios *t;
{
	int unit = DCAUNIT(tp->t_dev);
	struct dca_softc *sc = dca_cd.cd_devs[unit];
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
	default:	/* XXX gcc whines about cfcr being unitialized... */
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
	 * Set the FIFO threshold based on the receive speed, if we
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
	struct tty *tp;
{
	int s, c, unit = DCAUNIT(tp->t_dev);
	struct dca_softc *sc = dca_cd.cd_devs[unit];
	struct dcadevice *dca = sc->sc_dca;

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
	splx(s);
	return (bits);
}

void
dcainit(dca, rate)
	struct dcadevice *dca;
	int rate;
{
	int s;
	short stat;

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
	dca->dca_fifo =
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_1;
	dca->dca_mcr = MCR_DTR | MCR_RTS;
	DELAY(100);
	stat = dca->dca_iir;
	splx(s);
}

/*
 * Following are all routines needed for DCA to act as console
 */

int
dca_console_scan(scode, va, arg)
	int scode;
	caddr_t va;
	void *arg;
{
	struct dcadevice *dca = (struct dcadevice *)va;
	struct consdev *cp = arg;
	u_int pri;

	switch (dca->dca_id) {
	case DCAID0:
	case DCAID1:
		pri = CN_LOWPRI;
		break;

	case DCAID0 | DCACON:
	case DCAID1 | DCACON:
		pri = CN_HIGHPRI;
		break;

	default:
		return (0);
	}

#ifdef CONSCODE
	/*
	 * Raise our priority, if appropriate.
	 */
	if (scode == CONSCODE)
		pri = CN_FORCED;
#endif

	/* Only raise priority. */
	if (pri > cp->cn_pri)
		cp->cn_pri = pri;

	/*
	 * If our priority is higher than the currently-remembered
	 * console, stash our priority, for the benefit of dcacninit().
	 */
	if (cn_tab == NULL || cp->cn_pri > cn_tab->cn_pri) {
		cn_tab = cp;
		conscode = scode;
		return (DIO_SIZE(scode, va));
	}
	return (0);
}

void
dcacnprobe(cp)
	struct consdev *cp;
{

	/* locate the major number */
	for (dcamajor = 0; dcamajor < nchrdev; dcamajor++)
		if (cdevsw[dcamajor].d_open == dcaopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(dcamajor, 0);	/* XXX */

	console_scan(dca_console_scan, cp);

#ifdef KGDB
	/* XXX this needs to be fixed. */
	if (major(kgdb_dev) == 1)			/* XXX */
		kgdb_dev = makedev(dcamajor, minor(kgdb_dev));
#endif
}

/* ARGSUSED */
void
dcacninit(cp)
	struct consdev *cp;
{

	/*
	 * We are not interested by the second console pass.
	 */
	if (consolepass != 0)
		return;

	dca_cn = (struct dcadevice *)conaddr;
	dcainit(dca_cn, dcadefaultrate);
	dcaconsinit = 1;
}

/* ARGSUSED */
int
dcacngetc(dev)
	dev_t dev;
{
	u_char stat;
	int c, s;

#ifdef lint
	stat = dev; if (stat) return (0);
#endif

	s = splhigh();
	while (((stat = dca_cn->dca_lsr) & LSR_RXRDY) == 0)
		;
	c = dca_cn->dca_data;
	stat = dca_cn->dca_iir;
	splx(s);
	return (c);
}

/*
 * Console kernel output character routine.
 */
/* ARGSUSED */
void
dcacnputc(dev, c)
	dev_t dev;
	int c;
{
	int timo;
	u_char stat;
	int s = splhigh();

#ifdef lint
	stat = dev; if (stat) return;
#endif

	if (dcaconsinit == 0) {
		dcainit(dca_cn, dcadefaultrate);
		dcaconsinit = 1;
	}
	/* wait for any pending transmission to finish */
	timo = 50000;
	while (((stat = dca_cn->dca_lsr) & LSR_TXRDY) == 0 && --timo)
		;
	dca_cn->dca_data = c;
	/* wait for this transmission to complete */
	timo = 1500000;
	while (((stat = dca_cn->dca_lsr) & LSR_TXRDY) == 0 && --timo)
		;
	/* clear any interrupts generated by this transmission */
	stat = dca_cn->dca_iir;
	splx(s);
}
