/*	$OpenBSD: dart.c,v 1.11 2010/07/02 17:27:01 nicm Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/syslog.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>

#include <dev/cons.h>

#include <machine/avcommon.h>
#include <aviion/dev/dartreg.h>
#define	SPKRDIS	0x10	/* disable speaker on OP3 */
#include <aviion/dev/dartvar.h>

#ifdef	DDB
#include <ddb/db_var.h>
#endif

struct cfdriver dart_cd = {
	NULL, "dart", DV_TTY
};

/* console is on the first port */
#define	CONS_PORT	A_PORT
#ifdef	USE_PROM_CONSOLE
#define	dartcn_sv	sc->sc_sv_reg_storage
#else
struct dart_sv_reg dartcn_sv;
#endif

/* prototypes */
cons_decl(dart);
int	dart_speed(int);
struct tty *darttty(dev_t);
void	dartstart(struct tty *);
int	dartmctl(struct dartsoftc *, int, int, int);
int	dartparam(struct tty *, struct termios *);
void	dartmodemtrans(struct dartsoftc *, unsigned int, unsigned int);
void	dartrint(struct dartsoftc *, int);
void	dartxint(struct dartsoftc *, int);

/*
 * DUART registers are mapped as the least-significant byte of 32-bit
 * addresses. The following macros hide this.
 */

#define	dart_read(sc, reg) \
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, 3 + ((reg) << 2))
#define	dart_write(sc, reg, val) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, 3 + ((reg) << 2), (val))

#define	DART_CHIP(dev)	(minor(dev) >> 1)
#define DART_PORT(dev)	(minor(dev) & 1)

void
dart_common_attach(struct dartsoftc *sc)
{
	if (sc->sc_console) {
		sc->sc_sv_reg = &dartcn_sv;

		if (A_PORT != CONS_PORT) {
			sc->sc_sv_reg->sv_mr1[A_PORT] = PARDIS | RXRTS | CL8;
			sc->sc_sv_reg->sv_mr2[A_PORT] = /* TXCTS | */ SB1;
			sc->sc_sv_reg->sv_csr[A_PORT] = BD9600;
			sc->sc_sv_reg->sv_cr[A_PORT]  = TXEN | RXEN;
			sc->sc_sv_reg->sv_opr |= OPDTRA | OPRTSA;
		} else {
			sc->sc_sv_reg->sv_mr1[B_PORT] = PARDIS | RXRTS | CL8;
			sc->sc_sv_reg->sv_mr2[B_PORT] = /* TXCTS | */ SB1;
			sc->sc_sv_reg->sv_csr[B_PORT] = BD9600;
			sc->sc_sv_reg->sv_cr[B_PORT]  = TXEN | RXEN;
			sc->sc_sv_reg->sv_opr |= OPDTRB | OPRTSB;
		}
	} else {
		sc->sc_sv_reg = &sc->sc_sv_reg_storage;

		sc->sc_sv_reg->sv_mr1[A_PORT] = PARDIS | RXRTS | CL8;
		sc->sc_sv_reg->sv_mr2[A_PORT] = /* TXCTS | */ SB1;
		sc->sc_sv_reg->sv_csr[A_PORT] = BD9600;
		sc->sc_sv_reg->sv_cr[A_PORT]  = TXEN | RXEN;

		sc->sc_sv_reg->sv_mr1[B_PORT] = PARDIS | RXRTS | CL8;
		sc->sc_sv_reg->sv_mr2[B_PORT] = /* TXCTS | */ SB1;
		sc->sc_sv_reg->sv_csr[B_PORT] = BD9600;
		sc->sc_sv_reg->sv_cr[B_PORT]  = TXEN | RXEN;

		sc->sc_sv_reg->sv_opr = OPDTRA | OPRTSA | OPDTRB | OPRTSB;

		/* Start out with Tx and RX interrupts disabled */
		/* Enable input port change interrupt */
		sc->sc_sv_reg->sv_imr  = IIPCHG;
	}

	/* reset port a */
	if (sc->sc_console == 0 || CONS_PORT != A_PORT) {
		dart_write(sc, DART_CRA, RXRESET | TXDIS | RXDIS);
		DELAY_CR;
		dart_write(sc, DART_CRA, TXRESET | TXDIS | RXDIS);
		DELAY_CR;
		dart_write(sc, DART_CRA, ERRRESET | TXDIS | RXDIS);
		DELAY_CR;
		dart_write(sc, DART_CRA, BRKINTRESET | TXDIS | RXDIS);
		DELAY_CR;
		dart_write(sc, DART_CRA, MRRESET | TXDIS | RXDIS);
#if 0
		DELAY_CR;
#endif

		dart_write(sc, DART_MR1A, sc->sc_sv_reg->sv_mr1[A_PORT]);
		dart_write(sc, DART_MR2A, sc->sc_sv_reg->sv_mr2[A_PORT]);
		dart_write(sc, DART_CSRA, sc->sc_sv_reg->sv_csr[A_PORT]);
		dart_write(sc, DART_CRA, sc->sc_sv_reg->sv_cr[A_PORT]);
	}

	/* reset port b */
	if (sc->sc_console == 0 || CONS_PORT != B_PORT) {
		dart_write(sc, DART_CRB, RXRESET | TXDIS | RXDIS);
		DELAY_CR;
		dart_write(sc, DART_CRB, TXRESET | TXDIS | RXDIS);
		DELAY_CR;
		dart_write(sc, DART_CRB, ERRRESET | TXDIS | RXDIS);
		DELAY_CR;
		dart_write(sc, DART_CRB, BRKINTRESET | TXDIS | RXDIS);
		DELAY_CR;
		dart_write(sc, DART_CRB, MRRESET | TXDIS | RXDIS);
#if 0
		DELAY_CR;
#endif

		dart_write(sc, DART_MR1B, sc->sc_sv_reg->sv_mr1[B_PORT]);
		dart_write(sc, DART_MR2B, sc->sc_sv_reg->sv_mr2[B_PORT]);
		dart_write(sc, DART_CSRB, sc->sc_sv_reg->sv_csr[B_PORT]);
		dart_write(sc, DART_CRB, sc->sc_sv_reg->sv_cr[B_PORT]);
	}

	/* initialize common register of a DUART */
	dart_write(sc, DART_OPRS, sc->sc_sv_reg->sv_opr);

#if 0
	dart_write(sc, DART_CTUR, SLCTIM >> 8);
	dart_write(sc, DART_CTLR, SLCTIM & 0xff);
	dart_write(sc, DART_ACR, BDSET2 | CCLK16 | IPDCDIB | IPDCDIA);
#endif
	dart_write(sc, DART_IMR, sc->sc_sv_reg->sv_imr);
	dart_write(sc, DART_OPCR, OPSET | SPKRDIS);

	sc->sc_dart[A_PORT].tty = sc->sc_dart[B_PORT].tty = NULL;
	sc->sc_dart[A_PORT].dart_swflags = sc->sc_dart[B_PORT].dart_swflags = 0;
	if (sc->sc_console)
		sc->sc_dart[CONS_PORT].dart_swflags |= TIOCFLAG_SOFTCAR;

	printf("\n");
}

/* speed tables */
const struct dart_s {
	int kspeed;
	int dspeed;
} dart_speeds[] = {
	{ B0,		0	},	/* 0 baud, special HUP condition */
	{ B50,		NOBAUD	},	/* 50 baud, not implemented */
	{ B75,		BD75	},	/* 75 baud */
	{ B110,		BD110	},	/* 110 baud */
	{ B134,		BD134	},	/* 134.5 baud */
	{ B150,		BD150	},	/* 150 baud */
	{ B200,		NOBAUD	},	/* 200 baud, not implemented */
	{ B300,		BD300	},	/* 300 baud */
	{ B600,		BD600	},	/* 600 baud */
	{ B1200,	BD1200	},	/* 1200 baud */
	{ B1800,	BD1800	},	/* 1800 baud */
	{ B2400,	BD2400	},	/* 2400 baud */
	{ B4800,	BD4800	},	/* 4800 baud */
	{ B9600,	BD9600	},	/* 9600 baud */
	{ B19200,	BD19200	},	/* 19200 baud */
	{ -1,		NOBAUD	},	/* anything more is uncivilized */
};

int
dart_speed(int speed)
{
	const struct dart_s *ds;

	for (ds = dart_speeds; ds->kspeed != -1; ds++)
		if (ds->kspeed == speed)
			return ds->dspeed;

	return NOBAUD;
}

struct tty *
darttty(dev_t dev)
{
	u_int port, chip;
	struct dartsoftc *sc;

	chip = DART_CHIP(dev);
	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs <= chip || port >= NDARTPORTS)
		return (NULL);

	sc = (struct dartsoftc *)dart_cd.cd_devs[chip];
	if (sc == NULL)
		return (NULL);

	return sc->sc_dart[port].tty;
}

void
dartstart(struct tty *tp)
{
	struct dartsoftc *sc;
	dev_t dev;
	int s;
	u_int port, chip;
	int c, tries;
	bus_addr_t ptaddr;

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	dev = tp->t_dev;
	chip = DART_CHIP(dev);
	port = DART_PORT(dev);
	sc = (struct dartsoftc *)dart_cd.cd_devs[chip];
	ptaddr = port == A_PORT ? DART_A_BASE : DART_B_BASE;

	s = spltty();

	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		goto bail;

	ttwakeupwr(tp);
	if (tp->t_outq.c_cc == 0)
		goto bail;

	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0) {

		/* load transmitter until it is full */
		for (tries = 10000; tries != 0; tries --)
			if (dart_read(sc, ptaddr + DART_SRA) & TXRDY)
				break;

		if (tries == 0) {
			timeout_add(&tp->t_rstrt_to, 1);
			tp->t_state |= TS_TIMEOUT;
			break;
		} else {
			c = getc(&tp->t_outq);

			dart_write(sc, ptaddr + DART_TBA, c & 0xff);

			sc->sc_sv_reg->sv_imr |=
			    port == A_PORT ? ITXRDYA : ITXRDYB;
			dart_write(sc, DART_IMR, sc->sc_sv_reg->sv_imr);
		}
	}
	tp->t_state &= ~TS_BUSY;

bail:
	splx(s);
}

int
dartstop(struct tty *tp, int flag)
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);

	return 0;
}

/*
 * To be called at spltty - tty already locked.
 * Returns status of carrier.
 */
int
dartmctl(struct dartsoftc *sc, int port, int flags, int how)
{
	int newflags, flagsmask;
	struct dart_info *dart;
	int s;

	dart = &sc->sc_dart[port];

	s = spltty();

	flagsmask = port == A_PORT ? (OPDTRA | OPRTSA) : (OPDTRB | OPRTSB);
	newflags = (flags & TIOCM_DTR ? (OPDTRA | OPDTRB) : 0) |
	    (flags & TIOCM_RTS ? (OPRTSA | OPRTSB) : 0);
	newflags &= flagsmask;	/* restrict to the port we are acting on */

	switch (how) {
	case DMSET:
		dart_write(sc, DART_OPRS, newflags);
		dart_write(sc, DART_OPRR, ~newflags);
		/* only replace the sv_opr bits for the port we are acting on */
		sc->sc_sv_reg->sv_opr &= ~flagsmask;
		sc->sc_sv_reg->sv_opr |= newflags;
		break;
	case DMBIS:
		dart_write(sc, DART_OPRS, newflags);
		sc->sc_sv_reg->sv_opr |= newflags;
		break;
	case DMBIC:
		dart_write(sc, DART_OPRR, newflags);
		sc->sc_sv_reg->sv_opr &= ~newflags;
		break;
	case DMGET:
		flags = 0;
		if (port == A_PORT) {
			if (sc->sc_sv_reg->sv_opr & OPDTRA)
				flags |= TIOCM_DTR;
			if (sc->sc_sv_reg->sv_opr & OPRTSA)
				flags |= TIOCM_RTS;
		} else {
			if (sc->sc_sv_reg->sv_opr & OPDTRB)
				flags |= TIOCM_DTR;
			if (sc->sc_sv_reg->sv_opr & OPRTSB)
				flags |= TIOCM_RTS;
		}
		break;
	}

	splx(s);
	return (flags);
}

int
dartioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int error;
	u_int port, chip;
	struct tty *tp;
	struct dart_info *dart;
	struct dartsoftc *sc;

	chip = DART_CHIP(dev);
	port = DART_PORT(dev);
	sc = (struct dartsoftc *)dart_cd.cd_devs[chip];
	dart = &sc->sc_dart[port];

	tp = dart->tty;
	if (tp == NULL)
		return (ENXIO);

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return(error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return(error);

	switch (cmd) {
	case TIOCSBRK:
	case TIOCCBRK:
		break;
	case TIOCSDTR:
		(void)dartmctl(sc, port, TIOCM_DTR | TIOCM_RTS, DMBIS);
		break;
	case TIOCCDTR:
		(void)dartmctl(sc, port, TIOCM_DTR | TIOCM_RTS, DMBIC);
		break;
	case TIOCMSET:
		(void)dartmctl(sc, port, *(int *) data, DMSET);
		break;
	case TIOCMBIS:
		(void)dartmctl(sc, port, *(int *) data, DMBIS);
		break;
	case TIOCMBIC:
		(void)dartmctl(sc, port, *(int *) data, DMBIC);
		break;
	case TIOCMGET:
		*(int *)data = dartmctl(sc, port, 0, DMGET);
		break;
	case TIOCGFLAGS:
		*(int *)data = dart->dart_swflags;
		break;
	case TIOCSFLAGS:
		error = suser(p, 0);
		if (error != 0)
			return (EPERM);

		dart->dart_swflags = *(int *)data;
		dart->dart_swflags &= /* only allow valid flags */
			(TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

int
dartparam(struct tty *tp, struct termios *t)
{
	int flags;
	u_int port, chip;
	int speeds;
	unsigned char mr1, mr2;
	struct dart_info *dart;
	struct dartsoftc *sc;
	dev_t dev;
	bus_addr_t ptaddr;

	dev = tp->t_dev;
	chip = DART_CHIP(dev);
	port = DART_PORT(dev);
	sc = (struct dartsoftc *)dart_cd.cd_devs[chip];
	dart = &sc->sc_dart[port];
	ptaddr = port == A_PORT ? DART_A_BASE : DART_B_BASE;

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	flags = tp->t_flags;

	/* Reset to make global changes*/
	/* disable Tx and Rx */

	if (sc->sc_console == 0 || CONS_PORT != port) {
		if (port == A_PORT)
			sc->sc_sv_reg->sv_imr &= ~(ITXRDYA | IRXRDYA);
		else
			sc->sc_sv_reg->sv_imr &= ~(ITXRDYB | IRXRDYB);
		dart_write(sc, DART_IMR, sc->sc_sv_reg->sv_imr);

		/* hang up on zero baud rate */
		if (tp->t_ispeed == 0) {
			dartmctl(sc, port, HUPCL, DMSET);
			return (0);
		} else {
			/* set baudrate */
			speeds = dart_speed(tp->t_ispeed);
			if (speeds == NOBAUD)
				speeds = sc->sc_sv_reg->sv_csr[port];
			dart_write(sc, ptaddr + DART_CSRA, speeds);
			sc->sc_sv_reg->sv_csr[port] = speeds;
		}

		/* get saved mode registers and clear set up parameters */
		mr1 = sc->sc_sv_reg->sv_mr1[port];
		mr1 &= ~(CLMASK | PARTYPEMASK | PARMODEMASK);

		mr2 = sc->sc_sv_reg->sv_mr2[port];
		mr2 &= ~SBMASK;

		/* set up character size */
		switch (t->c_cflag & CSIZE) {
		case CL8:
			mr1 |= CL8;
			break;
		case CL7:
			mr1 |= CL7;
			break;
		case CL6:
			mr1 |= CL6;
			break;
		case CL5:
			mr1 |= CL5;
			break;
		}

		/* set up stop bits */
		if (tp->t_ospeed == B110)
			mr2 |= SB2;
		else
			mr2 |= SB1;

		/* set up parity */
		if (t->c_cflag & PARENB) {
			mr1 |= PAREN;
			if (t->c_cflag & PARODD)
				mr1 |= ODDPAR;
			else
				mr1 |= EVENPAR;
		} else
			mr1 |= PARDIS;

		if (sc->sc_sv_reg->sv_mr1[port] != mr1 ||
		    sc->sc_sv_reg->sv_mr2[port] != mr2) {
			/* write mode registers to duart */
			dart_write(sc, ptaddr + DART_CRA, MRRESET);
			dart_write(sc, ptaddr + DART_MR1A, mr1);
			dart_write(sc, ptaddr + DART_MR2A, mr2);

			/* save changed mode registers */
			sc->sc_sv_reg->sv_mr1[port] = mr1;
			sc->sc_sv_reg->sv_mr2[port] = mr2;
		}
	}

	/* enable transmitter? */
	if (tp->t_state & TS_BUSY) {
		sc->sc_sv_reg->sv_imr |= port == A_PORT ? ITXRDYA : ITXRDYB;
		dart_write(sc, DART_IMR, sc->sc_sv_reg->sv_imr);
	}

	/* re-enable the receiver */
#if 0
	DELAY_CR;
#endif
	sc->sc_sv_reg->sv_imr |= port == A_PORT ? IRXRDYA : IRXRDYB;
	dart_write(sc, DART_IMR, sc->sc_sv_reg->sv_imr);

	return (0);
}

void
dartmodemtrans(struct dartsoftc *sc, unsigned int ip, unsigned int ipcr)
{
	unsigned int dcdstate;
	struct tty *tp;
	int port;
	struct dart_info *dart;

	/* input is inverted at port!!! */
	if (ipcr & IPCRDCDA) {
		port = A_PORT;
		dcdstate = !(ip & IPDCDA);
	} else if (ipcr & IPCRDCDB) {
		port = B_PORT;
		dcdstate = !(ip & IPDCDB);
	} else {
#ifdef DIAGNOSTIC
		printf("dartmodemtrans: unknown transition ip=0x%x ipcr=0x%x\n",
		       ip, ipcr);
#endif
		return;
	}

	dart = &sc->sc_dart[port];
	tp = dart->tty;
	if (tp != NULL)
		ttymodem(tp, dcdstate);
}

int
dartopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int s;
	u_int port, chip;
	struct dart_info *dart;
	struct dartsoftc *sc;
	struct tty *tp;

	chip = DART_CHIP(dev);
	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs <= chip || port >= NDARTPORTS)
		return (ENODEV);
	sc = (struct dartsoftc *)dart_cd.cd_devs[chip];
	if (sc == NULL)
		return (ENODEV);
	dart = &sc->sc_dart[port];

	s = spltty();
	if (dart->tty != NULL)
		tp = dart->tty;
	else
		tp = dart->tty = ttymalloc(0);

	tp->t_oproc = dartstart;
	tp->t_param = dartparam;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);

		if (tp->t_ispeed == 0) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = B9600;
			if (sc->sc_console && port == CONS_PORT) {
				/* console is 8N1 */
				tp->t_cflag = (CREAD | CS8 | HUPCL);
			} else {
				tp->t_cflag = TTYDEF_CFLAG;
			}
		}

		if (dart->dart_swflags & TIOCFLAG_CLOCAL)
			tp->t_cflag |= CLOCAL;
		if (dart->dart_swflags & TIOCFLAG_CRTSCTS)
			tp->t_cflag |= CRTSCTS;
		if (dart->dart_swflags & TIOCFLAG_MDMBUF)
			tp->t_cflag |= MDMBUF;

		dartparam(tp, &tp->t_termios);
		ttsetwater(tp);

		(void)dartmctl(sc, port, TIOCM_DTR | TIOCM_RTS, DMSET);
		tp->t_state |= TS_CARR_ON;
	} else if (tp->t_state & TS_XCLUDE && suser(p, 0) != 0) {
		splx(s);
		return (EBUSY);
	}

	/*
	 * Reset the tty pointer, as there could have been a dialout
	 * use of the tty with a dialin open waiting.
	 */
	tp->t_dev = dev;
	splx(s);
	return ((*linesw[tp->t_line].l_open)(dev, tp, p));
}

int
dartclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp;
	struct dart_info *dart;
	struct dartsoftc *sc;
	u_int port, chip;

	chip = DART_CHIP(dev);
	port = DART_PORT(dev);
	sc = (struct dartsoftc *)dart_cd.cd_devs[chip];
	dart = &sc->sc_dart[port];

	tp = dart->tty;
	(*linesw[tp->t_line].l_close)(tp, flag, p);
	ttyclose(tp);

	return (0);
}

int
dartread(dev_t dev, struct uio *uio, int flag)
{
	u_int port, chip;
	struct tty *tp;
	struct dart_info *dart;
	struct dartsoftc *sc;

	chip = DART_CHIP(dev);
	port = DART_PORT(dev);
	sc = (struct dartsoftc *)dart_cd.cd_devs[chip];
	dart = &sc->sc_dart[port];

	tp = dart->tty;
	if (tp == NULL)
		return (ENXIO);
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
dartwrite(dev_t dev, struct uio *uio, int flag)
{
	u_int port, chip;
	struct tty *tp;
	struct dart_info *dart;
	struct dartsoftc *sc;

	chip = DART_CHIP(dev);
	port = DART_PORT(dev);
	sc = (struct dartsoftc *)dart_cd.cd_devs[chip];
	dart = &sc->sc_dart[port];

	tp = dart->tty;
	if (tp == NULL)
		return (ENXIO);
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

void
dartrint(struct dartsoftc *sc, int port)
{
	struct tty *tp;
	unsigned char data, sr;
	struct dart_info *dart;
	bus_addr_t ptaddr;

	dart = &sc->sc_dart[port];
	ptaddr = port == A_PORT ? DART_A_BASE : DART_B_BASE;
	tp = dart->tty;

	/* read status reg */
	while ((sr = dart_read(sc, ptaddr + DART_SRA)) & RXRDY) {
		/* read data and reset receiver */
		data = dart_read(sc, ptaddr + DART_RBA);

		if ((tp->t_state & (TS_ISOPEN|TS_WOPEN)) == 0 &&
		    (sc->sc_console == 0 || CONS_PORT != port)) {
			return;
		}

		if (sr & RBRK) {
			/* clear break state */
			dart_write(sc, ptaddr + DART_CRA, BRKINTRESET);
			DELAY_CR;
			dart_write(sc, ptaddr + DART_CRA, ERRRESET);

#if defined(DDB)
			if (db_console != 0 &&
			    sc->sc_console && port == CONS_PORT)
				Debugger();
#endif
		} else {
			if (sr & (FRERR | PERR | ROVRN)) { /* errors */
				if (sr & ROVRN)
					log(LOG_WARNING, "%s port %c: "
					    "receiver overrun\n",
					    sc->sc_dev.dv_xname, 'A' + port);
				if (sr & FRERR)
					log(LOG_WARNING, "%s port %c: "
					    "framing error\n",
					    sc->sc_dev.dv_xname, 'A' + port);
				if (sr & PERR)
					log(LOG_WARNING, "%s port %c: "
					    "parity error\n",
					    sc->sc_dev.dv_xname, 'A' + port);
				/* clear error state */
				dart_write(sc, ptaddr + DART_CRA, ERRRESET);
			} else {
				/* no errors */
				(*linesw[tp->t_line].l_rint)(data,tp);
			}
		}
	}
}

void
dartxint(struct dartsoftc *sc, int port)
{
	struct tty *tp;
	struct dart_info *dart;

	dart = &sc->sc_dart[port];
	tp = dart->tty;

	if ((tp->t_state & (TS_ISOPEN|TS_WOPEN))==0)
		goto out;

	if (tp->t_state & TS_BUSY) {
		tp->t_state &= ~(TS_BUSY | TS_FLUSH);
		dartstart(tp);
		if (tp->t_state & TS_BUSY) {
			/* do not disable transmitter, yet */
			return;
		}
	}
out:

	/* disable transmitter */
	sc->sc_sv_reg->sv_imr &= port == A_PORT ? ~ITXRDYA : ~ITXRDYB;
	dart_write(sc, DART_IMR, sc->sc_sv_reg->sv_imr);
}

int
dartintr(void *arg)
{
	struct dartsoftc *sc = arg;
	unsigned char isr, imr;
	int port;

	/* read interrupt status register and mask with imr */
	isr = dart_read(sc, DART_ISR);
	imr = sc->sc_sv_reg->sv_imr;

	if ((isr & imr) == 0) {
		/*
		 * We got an interrupt on a disabled condition (such as TX
		 * ready change on a disabled port). This should not happen,
		 * but we have to claim the interrupt anyway.
		 */
#if defined(DIAGNOSTIC) && !defined(MULTIPROCESSOR)
		printf("%s: spurious interrupt, isr %x imr %x\n",
		    sc->sc_dev.dv_xname, isr, imr);
#endif
		return (1);
	}
	isr &= imr;

	if (isr & IIPCHG) {
		unsigned int ip, ipcr;

		ip = dart_read(sc, DART_IP);
		ipcr = dart_read(sc, DART_IPCR);
		dartmodemtrans(sc, ip, ipcr);
		return (1);
	}

	if (isr & (IRXRDYA | ITXRDYA))
		port = 0;
#ifdef DIAGNOSTIC
	else if ((isr & (IRXRDYB | ITXRDYB)) == 0) {
		printf("%s: spurious interrupt, isr %x\n",
		    sc->sc_dev.dv_xname, isr);
		return (1);	/* claim it anyway */
	}
#endif
	else
		port = 1;

	if (isr & (IRXRDYA | IRXRDYB))
		dartrint(sc, port);
	if (isr & (ITXRDYA | ITXRDYB))
		dartxint(sc, port);
	if (isr & (port == A_PORT ? IBRKA : IBRKB))
		dart_write(sc, port == A_PORT ? DART_CRA : DART_CRB,
		    BRKINTRESET);

	return (1);
}

/*
 * Console interface routines.
#ifdef USE_PROM_CONSOLE
 * Since we select the actual console after all devices are attached,
 * we can safely pick the appropriate softc and use its information.
#endif
 */

#ifdef USE_PROM_CONSOLE
#define	dart_cnread(reg)	dart_read(sc, (reg))
#define	dart_cnwrite(reg, val)	dart_write(sc, (reg), (val))
#else
#define	dart_cnread(reg) \
	*(volatile u_int8_t *)(CONSOLE_DART_BASE + 3 + ((reg) << 2))
#define	dart_cnwrite(reg, val) \
	*(volatile u_int8_t *)(CONSOLE_DART_BASE + 3 + ((reg) << 2)) = (val)
#endif

void
dartcnprobe(struct consdev *cp)
{
	int maj;

	if (badaddr(CONSOLE_DART_BASE, 4) != 0)
		return;

#ifdef USE_PROM_CONSOLE
	/* do not attach as console if dart has been disabled */
	if (dart_cd.cd_ndevs == 0 || dart_cd.cd_devs[0] == NULL)
		return;
#endif

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == dartopen)
			break;
	if (maj == nchrdev)
		return;

	cp->cn_dev = makedev(maj, CONS_PORT);
	cp->cn_pri = CN_LOWPRI;
}

void
dartcninit(cp)
	struct consdev *cp;
{
#ifndef USE_PROM_CONSOLE
	dartcn_sv.sv_mr1[CONS_PORT] = PARDIS | RXRTS | CL8;
	dartcn_sv.sv_mr2[CONS_PORT] = /* TXCTS | */ SB1;
	dartcn_sv.sv_csr[CONS_PORT] = BD9600;
	dartcn_sv.sv_cr[CONS_PORT]  = TXEN | RXEN;
	dartcn_sv.sv_opr = CONS_PORT == A_PORT ? (OPDTRA | OPRTSA) :
	     (OPDTRB | OPRTSB);
	dartcn_sv.sv_imr = IIPCHG;

	dart_cnwrite(DART_CRA, RXRESET | TXDIS | RXDIS);
	DELAY_CR;
	dart_cnwrite(DART_CRA, TXRESET | TXDIS | RXDIS);
	DELAY_CR;
	dart_cnwrite(DART_CRA, ERRRESET | TXDIS | RXDIS);
	DELAY_CR;
	dart_cnwrite(DART_CRA, BRKINTRESET | TXDIS | RXDIS);
	DELAY_CR;
	dart_cnwrite(DART_CRA, MRRESET | TXDIS | RXDIS);
	DELAY_CR;

	dart_cnwrite(DART_MR1A, dartcn_sv.sv_mr1[CONS_PORT]);
	dart_cnwrite(DART_MR2A, dartcn_sv.sv_mr2[CONS_PORT]);
	dart_cnwrite(DART_CSRA, dartcn_sv.sv_csr[CONS_PORT]);
	dart_cnwrite(DART_CRA, dartcn_sv.sv_cr[CONS_PORT]);

	dart_cnwrite(DART_OPRS, dartcn_sv.sv_opr);

	dart_cnwrite(DART_IMR, dartcn_sv.sv_imr);
#endif
}

void
dartcnputc(dev_t dev, int c)
{
#ifdef USE_PROM_CONSOLE
	struct dartsoftc *sc;
#endif
	int s;
	u_int port;
	bus_addr_t ptaddr;

#ifdef USE_PROM_CONSOLE
	sc = (struct dartsoftc *)dart_cd.cd_devs[0];
	port = DART_PORT(dev);
#else
	port = CONS_PORT;
#endif
	ptaddr = port == A_PORT ? DART_A_BASE : DART_B_BASE;

	s = spltty();

	/* inhibit interrupts on the chip */
	dart_cnwrite(DART_IMR, dartcn_sv.sv_imr &
	    (CONS_PORT == A_PORT ? ~ITXRDYA : ~ITXRDYB));
	/* make sure transmitter is enabled */
#if 0
	DELAY_CR;
#endif
	dart_cnwrite(ptaddr + DART_CRA, TXEN);

	while ((dart_cnread(ptaddr + DART_SRA) & TXRDY) == 0)
		;
	dart_cnwrite(ptaddr + DART_TBA, c);

	/* wait for transmitter to empty */
	while ((dart_cnread(ptaddr + DART_SRA) & TXEMT) == 0)
		;

	/* restore the previous state */
	dart_cnwrite(DART_IMR, dartcn_sv.sv_imr);
#if 0
	DELAY_CR;
#endif
	dart_cnwrite(ptaddr + DART_CRA, dartcn_sv.sv_cr[0]);

	splx(s);
}

int
dartcngetc(dev_t dev)
{
#ifdef USE_PROM_CONSOLE
	struct dartsoftc *sc;
#endif
	unsigned char sr;	/* status reg of port a/b */
	u_char c;		/* received character */
	int s;
	u_int port;
	bus_addr_t ptaddr;

#ifdef USE_PROM_CONSOLE
	sc = (struct dartsoftc *)dart_cd.cd_devs[0];
	port = DART_PORT(dev);
#else
	port = CONS_PORT;
#endif
	ptaddr = port == A_PORT ? DART_A_BASE : DART_B_BASE;

	s = spltty();

	/* enable receiver */
	dart_cnwrite(ptaddr + DART_CRA, RXEN);

	for (;;) {
		/* read status reg */
		sr = dart_cnread(ptaddr + DART_SRA);

		/* receiver interrupt handler*/
		if (sr & RXRDY) {
			/* read character from port */
			c = dart_cnread(ptaddr + DART_RBA);

			/* check break condition */
			if (sr & RBRK) {
				/* clear break state */
				dart_cnwrite(ptaddr + DART_CRA, BRKINTRESET);
				DELAY_CR;
				dart_cnwrite(ptaddr + DART_CRA, ERRRESET);
				break;
			}

			if (sr & (FRERR | PERR | ROVRN)) {
				/* clear error state */
				dart_cnwrite(ptaddr + DART_CRA, ERRRESET);
			} else {
				break;
			}
		}
	}
	splx(s);

	return ((int)c);
}
