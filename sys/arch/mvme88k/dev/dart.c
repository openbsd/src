/*	$OpenBSD: dart.c,v 1.45 2006/01/01 11:59:39 miod Exp $	*/

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
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/device.h>
#include <sys/simplelock.h>
#include <sys/syslog.h>

#include <machine/asm_macro.h>   /* enable/disable interrupts */
#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/locore.h>
#include <machine/psl.h>

#include <dev/cons.h>

#include <machine/mvme188.h>
#include <mvme88k/dev/sysconreg.h>
#include <mvme88k/dev/dartreg.h>

#define spldart()	spltty()

#ifdef	DDB
#include <ddb/db_var.h>
#endif

#ifdef DEBUG
int dart_debug = 1;
#define dprintf(stuff) if (dart_debug) printf stuff
#else
#define dprintf(stuff)
#endif

#define	NDARTPORTS	2	/* Number of ports */

struct dart_info {
	struct tty		*tty;
	u_char			dart_swflags;
	struct simplelock	t_lock;
};

struct dartsoftc {
	struct device		sc_dev;
	union  dartreg		*dart_reg;
        union  dart_pt_io	*port_reg[NDARTPORTS];
	struct dart_info	sc_dart[NDARTPORTS];
	struct intrhand		sc_ih;
};

int	dartmatch(struct device *parent, void *self, void *aux);
void	dartattach(struct device *parent, struct device *self, void *aux);

struct cfattach dart_ca = {
	sizeof(struct dartsoftc), dartmatch, dartattach
};

struct cfdriver dart_cd = {
	NULL, "dart", DV_TTY
};

/* console is on the first port */
#define	CONS_PORT	A_PORT

/* prototypes */
cons_decl(dart);
int dart_speed(int);
struct tty* darttty(dev_t);
void dartstart(struct tty *);
int dartmctl(dev_t, int, int);
int dartparam(struct tty *, struct termios *);
void dartmodemtrans(struct dartsoftc *, unsigned int, unsigned int);
void dartrint(struct dartsoftc *, int);
void dartxint(struct dartsoftc *, int);

int dartintr(void *);
#if 0
void dartbreak(dev_t dev, int state);
#endif

/*
 * Lock strategy in that driver:
 * Use the tp->t_lock used by chario stuff as a lock
 * when modifying the chip's registers.
 *
 * Should be changed if driver crashes when powering
 * two lines.
 */

#define DART_PORT(dev) minor(dev)

struct dart_sv_reg dart_sv_reg;

/* speed tables */
const struct dart_s {
	int kspeed;
	int dspeed;
} dart_speeds[] = {
	{B0,		0	},	/* 0 baud, special HUP condition */
        {B50,		NOBAUD	},	/* 50 baud, not implemented */
	{B75,		BD75	},	/* 75 baud */
	{B110,		BD110	},	/* 110 baud */
	{B134,		BD134	},	/* 134.5 baud */
	{B150,		BD150	},	/* 150 baud */
	{B200,		NOBAUD	},	/* 200 baud, not implemented */
	{B300,		BD300	},	/* 300 baud */
	{B600,		BD600	},	/* 600 baud */
	{B1200,		BD1200	},	/* 1200 baud */
	{B1800,		BD1800	},	/* 1800 baud */
	{B2400,		BD2400	},	/* 2400 baud */
	{B4800,		BD4800	},	/* 4800 baud */
	{B9600,		BD9600	},	/* 9600 baud */
	{B19200,	BD19200	},	/* 19200 baud */
	{-1,		NOBAUD	},	/* anything more is uncivilized */
};

int
dart_speed(speed)
	int speed;
{
	const struct dart_s *ds;

	for (ds = dart_speeds; ds->kspeed != -1; ds++)
		if (ds->kspeed == speed)
			return ds->dspeed;

	return NOBAUD;
}

struct tty*
darttty(dev)
	dev_t dev;
{
	unsigned int port;
	struct dartsoftc *sc;

	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs == 0 || port >= NDARTPORTS)
		return (NULL);

	sc = (struct dartsoftc *) dart_cd.cd_devs[0];
	return sc->sc_dart[port].tty;
}

int
dartmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	union dartreg *addr;

	/* Don't match if wrong cpu */
	if (brdtyp != BRD_188)
		return (0);

	addr = (union dartreg *)ca->ca_paddr;

	if (badvaddr((vaddr_t)addr, 2)) {
#ifdef DEBUG
		printf("==> dart: failed address check.\n");
#endif
		return (0);
	}

	return (1);
}

void
dartattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct dartsoftc *sc = (struct dartsoftc *)self;
	struct confargs *ca = aux;
	union dartreg *addr; /* pointer to DUART regs */
	union dart_pt_io *ptaddr; /* pointer to port regs */
	int port;	/* port index */

	if (ca->ca_ipl < 0)
		ca->ca_ipl = IPL_TTY;

	/* set up dual port memory and registers and init*/
	sc->dart_reg = (union dartreg *)ca->ca_paddr;
        ptaddr = (union  dart_pt_io *)ca->ca_paddr;
	sc->port_reg[A_PORT] = ptaddr;
	ptaddr++;
	sc->port_reg[B_PORT] = ptaddr;
	sc->sc_dart[A_PORT].tty = NULL;
	sc->sc_dart[B_PORT].tty = NULL;

	addr = sc->dart_reg;

	/* save standard initialization */
	dart_sv_reg.sv_mr1[A_PORT] = PARDIS | RXRTS | CL8;
	dart_sv_reg.sv_mr2[A_PORT] = /* TXCTS | */ SB1;
	dart_sv_reg.sv_csr[A_PORT] = BD9600;
	dart_sv_reg.sv_cr[A_PORT]  = TXEN | RXEN;

	dart_sv_reg.sv_mr1[B_PORT] = PARDIS | RXRTS | CL8;
	dart_sv_reg.sv_mr2[B_PORT] = /* TXCTS | */ SB1;
	dart_sv_reg.sv_csr[B_PORT] = BD9600;
	dart_sv_reg.sv_cr[B_PORT]  = TXEN | RXEN;

	dart_sv_reg.sv_acr  = BDSET2 | CCLK16 | IPDCDIB | IPDCDIA;

	/* Start out with Tx and RX interrupts disabled */
	/* Enable input port change interrupt */
	dart_sv_reg.sv_imr  = IIPCHG;

	/*
	 * Although we are still running using the BUG routines,
	 * this device will be elected as the console after
	 * autoconf.
	 * We do not even test since we know we are an MVME188 and
	 * console is always on the first port.
	 */
	printf(": console");

	dprintf(("\ndartattach: resetting port A\n"));

	/* reset port a */
	addr->write.wr_cra  = RXRESET     | TXDIS | RXDIS;
	DELAY_CR;
	addr->write.wr_cra  = TXRESET     | TXDIS | RXDIS;
	DELAY_CR;
	addr->write.wr_cra  = ERRRESET    | TXDIS | RXDIS;
	DELAY_CR;
	addr->write.wr_cra  = BRKINTRESET | TXDIS | RXDIS;
	DELAY_CR;
	addr->write.wr_cra  = MRRESET     | TXDIS | RXDIS;

	dprintf(("dartattach: resetting port B\n"));

	/* reset port b */
	addr->write.wr_crb  = RXRESET     | TXDIS | RXDIS;
	DELAY_CR;
	addr->write.wr_crb  = TXRESET     | TXDIS | RXDIS;
	DELAY_CR;
	addr->write.wr_crb  = ERRRESET    | TXDIS | RXDIS;
	DELAY_CR;
	addr->write.wr_crb  = BRKINTRESET | TXDIS | RXDIS;
	DELAY_CR;
	addr->write.wr_crb  = MRRESET     | TXDIS | RXDIS;
	DELAY_CR;

	/* initialize ports */
	for (port = 0, ptaddr = (union dart_pt_io *)addr;
	    port < MAXPORTS;
	    port++, ptaddr++) {
		dprintf(("dartattach: init port %c\n", 'A' + port));
		ptaddr->write.wr_mr  = dart_sv_reg.sv_mr1[port];
		ptaddr->write.wr_mr  = dart_sv_reg.sv_mr2[port];
		ptaddr->write.wr_csr = dart_sv_reg.sv_csr[port];
		ptaddr->write.wr_cr  = dart_sv_reg.sv_cr [port];
	}

	dprintf(("dartattach: init common regs\n"));

	/* initialize common register of a DUART */
	addr->write.wr_oprset = OPDTRA | OPRTSA | OPDTRB | OPRTSB;

	addr->write.wr_ctur  = SLCTIM>>8;
	addr->write.wr_ctlr  = SLCTIM & 0xFF;
	addr->write.wr_acr  = dart_sv_reg.sv_acr;
	addr->write.wr_imr  = dart_sv_reg.sv_imr;
	addr->write.wr_opcr = OPSET;
	addr->write.wr_ivr = SYSCON_VECT + SYSCV_SCC;	/* hard coded vector */

	/* enable interrupts */
	sc->sc_ih.ih_fn = dartintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_wantframe = 0;
	sc->sc_ih.ih_ipl = ca->ca_ipl;

	sysconintr_establish(SYSCV_SCC, &sc->sc_ih, self->dv_xname);
	printf("\n");
}

/*
 * To be called at spltty - tty already locked.
 */
void
dartstart(tp)
	struct tty *tp;
{
	dev_t dev;
	struct dartsoftc *sc;
	int s;
	union dart_pt_io *ptaddr;
	union dartreg *addr;
	int port, tries;
	int c;

	dev = tp->t_dev;
	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs == 0 || port >= NDARTPORTS)
		return;

	sc = (struct dartsoftc *) dart_cd.cd_devs[0];
	addr = sc->dart_reg;
	ptaddr = sc->port_reg[port];

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	s = spltty();

	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		goto bail;

	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
		if (tp->t_outq.c_cc == 0)
			goto bail;
	}

	dprintf(("dartstart: dev(%d, %d)\n", major(dev), minor(dev)));

	if (port != CONS_PORT)
		dprintf(("dartstart: ptaddr = 0x%08x from uart at 0x%08x\n",
			 ptaddr, addr));

	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0) {

		/* load transmitter until it is full */
		for (tries = 10000; tries != 0; tries --)
			if (ptaddr->read.rd_sr & TXRDY)
				break;

		if (tries == 0) {
			timeout_add(&tp->t_rstrt_to, 1);
			tp->t_state |= TS_TIMEOUT;
			break;
		} else {
			c = getc(&tp->t_outq);

			if (port != CONS_PORT)
				dprintf(("dartstart: writing char \"%c\" (0x%02x) to port %d\n",
					 c & 0xff, c & 0xff, port));
			ptaddr->write.wr_tb = c & 0xff;

			if (port != CONS_PORT)
				dprintf(("dartstart: enabling Tx int\n"));
			if (port == A_PORT)
				dart_sv_reg.sv_imr |= ITXRDYA;
			else
				dart_sv_reg.sv_imr |= ITXRDYB;
			addr->write.wr_imr = dart_sv_reg.sv_imr;
		}
	}
	tp->t_state &= ~TS_BUSY;

bail:
	splx(s);
}

/*
 * To be called at spltty - tty already locked.
 */
int
dartstop(tp, flag)
	struct tty *tp;
	int flag;
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

#define HANDLE_FLAG(_FLAG_, _PORT_, _AFLAG_, _BFLAG_) \
   if (flags & (_FLAG_)) \
      { newflags |= ((_PORT_) == A_PORT) ? (_AFLAG_) : (_BFLAG_); \
        flags &= ~(_FLAG_); }

#define HOW2STR(_OP_) \
	((_OP_) == DMGET) ? "GET" : \
	 (((_OP_) == DMSET) ? "FORCE" : \
	  ((((_OP_) == DMBIS) ? "SET" : \
	   (((((_OP_) == DMBIC) ? "CLR" : "???"))))))

#define FLAGSTRING \
     "\20\1LE\2DTR\3RTS\4ST\5SR\6CTS\7CAR\10RNG\11DSR\12BRK"

/*
 * To be called at spltty - tty already locked.
 * Returns status of carrier.
 */

int
dartmctl (dev, flags, how)
	dev_t dev;
	int flags;
	int how;
{
	union dartreg *addr;
	int port;
	unsigned int dcdstate;
	int newflags = 0;
	struct dart_info *dart;
	struct dartsoftc *sc;
	int s;

	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs == 0 || port >= NDARTPORTS)
		return (ENODEV);

	sc = (struct dartsoftc *) dart_cd.cd_devs[0];
	dart = &sc->sc_dart[port];
	addr = sc->dart_reg;

	/* special case: set or clear break */
#if 0
	if (flags & TIOCSBRK) {
		dartbreak(port, 1);
		flags &= ~TIOCSBRK;
	}
	if (flags & TIOCCBRK) {
		dartbreak(port, 0);
		flags &= ~TIOCCBRK;
	}
#endif
	s = spltty();

	HANDLE_FLAG(TIOCM_DTR, port, OPDTRA, OPDTRB);
	HANDLE_FLAG(TIOCM_RTS, port, OPRTSA, OPRTSB);

#if 0
	if (flags) {
		printf("dartmctl: currently only BRK, DTR and RTS supported\n");
		printf("dartmctl: op=%s flags left = 0x%b\n",
		       HOW2STR(how), flags, FLAGSTRING);
		panic("dartmctl");
	}
#endif
	dprintf(("dartmctl: action=%s flags=0x%x\n",
		 HOW2STR(how), newflags));

	switch (how) {
	case DMSET:
		addr->write.wr_oprset = newflags;
		addr->write.wr_oprreset = ~newflags;
		break;
	case DMBIS:
		addr->write.wr_oprset = newflags;
		break;
	case DMBIC:
		addr->write.wr_oprreset = newflags;
		break;
	case DMGET:
		panic("dartmctl: DMGET not supported (yet)");
		break;
	}

	/* read DCD input */
	/* input is inverted at port */
	dcdstate = !(addr->read.rd_ip & ((port == A_PORT) ? IPDCDA : IPDCDB));

	dprintf(("dartmctl: DCD is %s\n", dcdstate ? "up" : "down"));
	splx(s);
	return dcdstate;
}

#if 0
/*
 * To be called at spltty - tty already locked.
 */
void
dartbreak(dev, state)
	dev_t dev;
	int state;
{
	union dartreg *addr;
	union dart_pt_io *ptaddr;
	int port;
	struct dart_info *dart;
	struct dartsoftc *sc;

	dprintf(("dartbreak: break %s\n", (state == 1) ? "on" : "off"));

	port = DART_PORT(dev);
	sc = (struct dartsoftc *) dart_cd.cd_devs[0];
	dart = &sc->sc_dart[port];
	addr = sc->dart_reg;

	ptaddr = sc->port_reg[port];

	if (state == 1) {
		/* the duart must be enabled with a dummy byte,
		to prevent the transmitter empty interrupt */
		ptaddr->write.wr_cr = BRKSTART|TXEN;
		ptaddr->write.wr_tb = 0;
	} else {
		ptaddr->write.wr_cr = BRKSTOP;	 /* stop a break*/
	}
}
#endif

int
dartioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error;
	int port;
	struct tty *tp;
	struct dart_info *dart;
	struct dartsoftc *sc;

	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs == 0 || port >= NDARTPORTS)
		return (ENODEV);

	sc = (struct dartsoftc *) dart_cd.cd_devs[0];
	dart = &sc->sc_dart[port];
	tp = dart->tty;
	if (!tp)
		return ENXIO;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return(error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return(error);

	switch (cmd) {
	case TIOCSBRK:
		/* */
		break;

	case TIOCCBRK:
		/* */
		break;

	case TIOCSDTR:
		(void) dartmctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) dartmctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) dartmctl(dev, *(int *) data, DMSET);
		break;

	case TIOCMBIS:
		(void) dartmctl(dev, *(int *) data, DMBIS);
		break;

	case TIOCMBIC:
		(void) dartmctl(dev, *(int *) data, DMBIC);
		break;

	case TIOCMGET:
/*		*(int *)data = dartmctl(dev, 0, DMGET);*/
		break;
	case TIOCGFLAGS:
		if (CONS_PORT == port)
			dart->dart_swflags |= TIOCFLAG_SOFTCAR;
		*(int *)data = dart->dart_swflags;
		break;
	case TIOCSFLAGS:
		error = suser(p, 0);
		if (error != 0)
			return(EPERM);

		dart->dart_swflags = *(int *)data;
		if (CONS_PORT == port)
			dart->dart_swflags |= TIOCFLAG_SOFTCAR;
		dart->dart_swflags &= /* only allow valid flags */
			(TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
		break;
	default:
		return(ENOTTY);
	}

	return 0;
}

/*
 * To be called at spltty - tty already locked.
 */
int
dartparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	union dartreg *addr;
	union dart_pt_io *ptaddr;
	int flags;
	int port;
	int speeds;
	unsigned char mr1, mr2;
	struct dart_info *dart;
	struct dartsoftc *sc;
	dev_t dev;

	dev = tp->t_dev;
	dprintf(("dartparam: setting param for dev(%d, %d)\n", major(dev), minor(dev)));
	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs == 0 || port >= NDARTPORTS)
		return (ENODEV);

	sc = (struct dartsoftc *) dart_cd.cd_devs[0];
	dart = &sc->sc_dart[port];
	addr = sc->dart_reg;
	ptaddr = sc->port_reg[port];

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	flags = tp->t_flags;

	/* Reset to make global changes*/
	/* disable Tx and Rx */
	dprintf(("dartparam: disabling Tx and Rx int\n"));

	if (CONS_PORT == port) {
		dprintf(("dartparam: skipping console init\n"));
	} else {
		if (port == A_PORT)
			dart_sv_reg.sv_imr = dart_sv_reg.sv_imr & ~(ITXRDYA | IRXRDYA);
		else
			dart_sv_reg.sv_imr = dart_sv_reg.sv_imr & ~(ITXRDYB | IRXRDYB);
		addr->write.wr_imr = dart_sv_reg.sv_imr;

		/* hang up on zero baud rate */
		if (tp->t_ispeed == 0) {
			dprintf(("dartparam: ispeed == 0 -> HUP\n"));
			dartmctl(dev, HUPCL, DMSET);
			return 0;
		} else {
			/* set baudrate */
			speeds = dart_speed(tp->t_ispeed);
			dprintf(("dartparam: speed 0x%x, baudrate %d\n", speeds, tp->t_ispeed));
			if (speeds == NOBAUD)
				speeds = dart_sv_reg.sv_csr[port];
			ptaddr->write.wr_csr = speeds;
			dart_sv_reg.sv_csr[port] = speeds;
			dprintf(("dartparam: baudrate set param = %d\n", speeds));
		}

		/* get saved mode registers and clear set up parameters */
		mr1 = dart_sv_reg.sv_mr1[port];
		mr1 &= ~(CLMASK | PARTYPEMASK | PARMODEMASK);

		mr2 = dart_sv_reg.sv_mr2[port];
		mr2 &= ~SBMASK;

		/* set up character size */
		if (flags & CS8) {
			mr1 |= CL8;
			dprintf(("dartparam: PASS8\n"));
		} else if (tp->t_ispeed == B134) {
			mr1 |= CL6;
			dprintf(("dartparam: CS6\n"));
		} else {
			mr1 |= CL7;
			dprintf(("dartparam: CS7\n"));
		}

		/* set up stop bits */
		if (tp->t_ospeed == B110) {
			mr2 |= SB2;
			dprintf(("dartparam: two stop bits\n"));
		} else {
			mr2 |= SB1;
			dprintf(("dartparam: one stop bit\n"));
		}

		/* set up parity */
		if (((flags & PARENB) != PARENB) &&
		    (flags & PARENB)) {
			mr1 |= PAREN;
			if (flags & PARODD) {
				mr1 |= ODDPAR;
				dprintf(("dartparam: odd parity\n"));
			} else {
				mr1 |= EVENPAR;
				dprintf(("dartparam: even parity\n"));
			}
		} else {
			mr1 |= PARDIS;
			dprintf(("dartparam: no parity\n"));
		}

		if ((dart_sv_reg.sv_mr1[port] != mr1)
		    || (dart_sv_reg.sv_mr2[port] != mr2)) {
			/* write mode registers to duart */
			ptaddr->write.wr_cr = MRRESET;
			ptaddr->write.wr_mr = mr1;
			ptaddr->write.wr_mr = mr2;

			/* save changed mode registers */
			dart_sv_reg.sv_mr1[port] = mr1;
			dart_sv_reg.sv_mr2[port] = mr2;
		}
	}

	/* enable transmitter? */
	if (tp->t_state & TS_BUSY) {
		dprintf(("dartparam: reenabling Tx int\n"));

		if (port == A_PORT)
			dart_sv_reg.sv_imr = dart_sv_reg.sv_imr | ITXRDYA;
		else
			dart_sv_reg.sv_imr = dart_sv_reg.sv_imr | ITXRDYB;
		addr->write.wr_imr = dart_sv_reg.sv_imr;
	} else {
		dprintf(("dartparam: not enabling Tx\n"));
	}

	/* re-enable the receiver */
	dprintf(("dartparam: reenabling Rx int\n"));

	DELAY_CR;
	if (port == A_PORT)
		dart_sv_reg.sv_imr = dart_sv_reg.sv_imr | IRXRDYA;
	else
		dart_sv_reg.sv_imr = dart_sv_reg.sv_imr | IRXRDYB;
	addr->write.wr_imr = dart_sv_reg.sv_imr;

	return 0;
}

void
dartmodemtrans(sc, ip, ipcr)
	struct dartsoftc *sc;
	unsigned int ip;
	unsigned int ipcr;
{
	unsigned int dcdstate;
	struct tty *tp;
	int port;
	struct dart_info *dart;

	dprintf(("dartmodemtrans: ip=0x%x ipcr=0x%x\n",
		 ip, ipcr));

	/* input is inverted at port!!! */
	if (ipcr & IPCRDCDA) {
		port = A_PORT;
		dcdstate = !(ip & IPDCDA);
	} else if (ipcr & IPCRDCDB) {
		port = B_PORT;
		dcdstate = !(ip & IPDCDB);
	} else {
		printf("dartmodemtrans: unknown transition ip=0x%x ipcr=0x%x\n",
		       ip, ipcr);
		return;
	}

	dart = &sc->sc_dart[port];
	tp = dart->tty;
	if (tp != NULL) {
		dprintf(("dartmodemtrans: tp=0x%x new DCD state: %s\n",
		    tp, dcdstate ? "UP" : "DOWN"));
		ttymodem(tp, dcdstate);
	}
}

int
dartopen (dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int s, port;
	struct dart_info *dart;
	struct dartsoftc *sc;
	struct tty *tp;

	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs == 0 || port >= NDARTPORTS)
		return (ENODEV);

	sc = (struct dartsoftc *) dart_cd.cd_devs[0]; /* the only one */
	dart = &sc->sc_dart[port];
	s = spltty();

	if (dart->tty) {
		tp = dart->tty;
	} else {
		tp = dart->tty = ttymalloc();
		simple_lock_init(&dart->t_lock);
	}

	simple_lock(&dart->t_lock);
	tp->t_oproc = dartstart;
	tp->t_param = dartparam;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = B9600;
		dartparam(tp, &tp->t_termios);
		if (port == CONS_PORT) {
			/* console is 8N1 */
			tp->t_cflag = (CREAD | CS8 | HUPCL);
		} else {
			tp->t_cflag = TTYDEF_CFLAG;
		}
		ttsetwater(tp);
		(void)dartmctl(dev, TIOCM_DTR | TIOCM_RTS, DMSET);
		tp->t_state |= TS_CARR_ON;
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		simple_unlock(&dart->t_lock);
		splx(s);
		return (EBUSY);
	}
	/*
	 * Reset the tty pointer, as there could have been a dialout
	 * use of the tty with a dialin open waiting.
	 */
	tp->t_dev = dev;
	simple_unlock(&dart->t_lock);
	splx(s);
	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
dartclose (dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct tty *tp;
	struct dart_info *dart;
	struct dartsoftc *sc;
	int port;

	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs == 0 || port >= NDARTPORTS)
		return (ENODEV);

	sc = (struct dartsoftc *) dart_cd.cd_devs[0];
	dart = &sc->sc_dart[port];
	tp = dart->tty;
	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);

	return 0;
}

int
dartread (dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int port;
	struct tty *tp;
	struct dart_info *dart;
	struct dartsoftc *sc;

	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs == 0 || port >= NDARTPORTS)
		return (ENODEV);

	sc = (struct dartsoftc *) dart_cd.cd_devs[0];
	dart = &sc->sc_dart[port];
	tp = dart->tty;

	if (!tp)
		return ENXIO;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
dartwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int port;
	struct tty *tp;
	struct dart_info *dart;
	struct dartsoftc *sc;

	port = DART_PORT(dev);
	if (dart_cd.cd_ndevs == 0 || port >= NDARTPORTS)
		return (ENODEV);

	sc = (struct dartsoftc *)dart_cd.cd_devs[0];
	dart = &sc->sc_dart[port];

	tp = dart->tty;
	if (!tp)
		return ENXIO;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

void
dartrint(sc, port)
	struct dartsoftc *sc;
	int port;
{
	union dartreg *addr;
	union dart_pt_io *ptaddr;
	struct tty *tp;
	unsigned char data, sr;
	struct dart_info *dart;

	dart = &sc->sc_dart[port];
	addr = sc->dart_reg;

	/* read status reg */
	ptaddr = sc->port_reg[port];

	dprintf(("dartrint: Rx int port %d\n", port));

	tp = dart->tty;

	dprintf(("dartrint: ptaddr = 0x%08x from uart at 0x%08x\n",
		 ptaddr, addr));

	while ((sr = ptaddr->read.rd_sr) & RXRDY) {
		dprintf(("dartrint: sr = 0x%08x\n", sr));

		data = ptaddr->read.rd_rb; /* read data and reset receiver */

		dprintf(("dartrint: read char \"%c\" (0x%02x) tp = 0x%x\n",
			 data, data, tp));

		if ((tp->t_state & (TS_ISOPEN|TS_WOPEN)) == 0 && CONS_PORT != port) {
			return;
		}

		if (sr & RBRK) {
			dprintf(("dartrint: BREAK detected\n"));
			/*
			data = tp->t_breakc;
			ttyinput(data, tp);
			*/
			/* clear break state */
			ptaddr->write.wr_cr = BRKINTRESET;
			DELAY_CR;
			ptaddr->write.wr_cr = ERRRESET;

#if defined(DDB)
			if (db_console != 0) {
				Debugger();
			}
#endif
		} else {
			if (sr & (FRERR|PERR|ROVRN)) { /* errors */
				if (sr & ROVRN)
					printf("dart0: receiver overrun port %c\n", 'A' + port);
				if (sr & FRERR)
					printf("dart0: framing error port %c\n", 'A' + port);
				if (sr & PERR)
					printf("dart0: parity error port %c\n", 'A' + port);
				dprintf(("dartrint: error received\n"));
				/* clear error state */
				ptaddr->write.wr_cr = ERRRESET;
			} else {
				/* no errors */
				(*linesw[tp->t_line].l_rint)(data,tp);
#if 0
				{
					if (tp->t_ispeed == B134) /* CS6 */
						data &= 077;
					else if (tp->t_flags & CS8)
						;
					else
						data &= 0177; /* CS7 */
					ttyinput(data, tp);
				}
#endif
			}
		}
	}
	dprintf(("dartrint: ready\n"));
}

void
dartxint(sc, port)
	struct dartsoftc *sc;
	int port;
{
	struct tty *tp;
	struct dart_info *dart;
	union dartreg *addr;

	dart = &sc->sc_dart[port];
	addr = sc->dart_reg;

	tp = dart->tty;

	simple_lock(&dart->t_lock);

	if ((tp->t_state & (TS_ISOPEN|TS_WOPEN))==0)
		goto out;

	if (tp->t_state & TS_FLUSH)
		tp->t_state &= ~TS_FLUSH;

	if (tp->t_state & TS_BUSY) {
		tp->t_state &= ~TS_BUSY;
		dprintf(("dartxint: starting output\n"));
		dartstart(tp);
		if (tp->t_state & TS_BUSY) {
			dprintf(("dartxint: ready - Tx left enabled\n"));
			simple_unlock(&dart->t_lock);
			return;
		}
	}
out:

	/* disable transmitter */
	if (port == 0)
		dart_sv_reg.sv_imr = dart_sv_reg.sv_imr & ~ITXRDYA;
	else
		dart_sv_reg.sv_imr = dart_sv_reg.sv_imr & ~ITXRDYB;

	addr->write.wr_imr = dart_sv_reg.sv_imr;

	simple_unlock(&dart->t_lock);

	dprintf(("dartxint: ready - Tx disabled\n"));
}

int
dartintr(arg)
	void *arg;
{
	struct dartsoftc *sc = arg;

	unsigned char isr, imr;
	int port;
	union dartreg *addr;

	/* read interrupt status register and mask with imr */
	addr = sc->dart_reg;
	isr = addr->read.rd_isr;
	imr = dart_sv_reg.sv_imr;

	if ((isr & imr) == 0) {
		/*
		 * We got an interrupt on a disabled condition (such as TX
		 * ready change on a disabled port). This should not happen,
		 * but we have to claim the interrupt anyway.
		 */
#ifdef DIAGNOSTIC
		printf("dartintr: spurious interrupt, isr %x imr %x\n",
		    isr, imr);
#endif
		return 1;
	}
	isr &= imr;

	if (isr & IIPCHG) {
		unsigned int ip = addr->read.rd_ip;
		unsigned int ipcr = addr->read.rd_ipcr;
		dartmodemtrans(sc, ip, ipcr);
		return 1;
	}

	if (isr & (IRXRDYA | ITXRDYA))
		port = 0;
	else if (isr & (IRXRDYB | ITXRDYB))
		port = 1;
	else {
		printf("dartintr: spurious interrupt, isr 0x%08x\n", isr);
		return 1;	/* claim it anyway */
	}

	dprintf(("dartintr: interrupt from port %d, isr 0x%08x\n",
		 port, isr));

	if (isr & (IRXRDYA | IRXRDYB)) {
		dprintf(("dartintr: Rx interrupt\n"));
		dartrint(sc, port);
	}
	if (isr & (ITXRDYA | ITXRDYB)) {
		dprintf(("dartintr: Tx interrupt\n"));
		dartxint(sc, port);
	}
	if ((port == A_PORT && (isr & IBRKA)) ||
	    (port == B_PORT && (isr & IBRKB))) {
		union dart_pt_io *ptaddr = (union dart_pt_io *)addr + port;

		dprintf(("dartintr: clearing end of BREAK state\n"));
		ptaddr->write.wr_cr = BRKINTRESET;
	}

	dprintf(("dartintr: ready\n"));
	return 1;
}

/*
 * Console interface routines. Currently only dev 0 or 1
 * supported.
 */

void
dartcnprobe(cp)
	struct consdev *cp;
{
	int maj;

	if (brdtyp != BRD_188 || badaddr(DART_BASE, 2) != 0)
		return;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == dartopen)
			break;
	if (maj == nchrdev)
		return;

	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_NORMAL;
}

void
dartcninit(cp)
	struct consdev *cp;
{
}

void
dartcnputc(dev, c)
	dev_t dev;
	int c;
{
	union dartreg *addr;
	union dart_pt_io *ptaddr;
	int s;
	int port;

	port = DART_PORT(dev);

	addr = (union dartreg *) MVME188_DUART;

#if 1
	ptaddr = (union dart_pt_io *) addr + (port * 0x20);
#else
	ptaddr = (union dart_pt_io *) addr + ((dev & 1) ? 1 : 0);
#endif

	s = spltty();

	/* Assume first port initialized if we get here. */
	/* Assume the bug initializes the port */

	/* inhibit interrupts on the chip */
	addr->write.wr_imr = dart_sv_reg.sv_imr & ~ITXRDYA;
	/* make sure transmitter is enabled */
	DELAY_CR;
	ptaddr->write.wr_cr = TXEN;

	while (!(ptaddr->read.rd_sr & TXRDY))
		;
	ptaddr->write.wr_tb = c;

	/* wait for transmitter to empty */
	while (!(ptaddr->read.rd_sr & TXEMT))
		;

	/* restore the previous state */
	addr->write.wr_imr = dart_sv_reg.sv_imr;
	DELAY_CR;
	ptaddr->write.wr_cr = dart_sv_reg.sv_cr[0];

	splx(s);
}

int
dartcngetc(dev)
	dev_t dev;
{
	union dartreg  *addr;	   /* pointer to DUART regs */
	union dart_pt_io  *ptaddr; /* pointer to port regs */
	unsigned char   sr;	       /* status reg of port a/b */
	u_char c;		/* received character */
	int s;
	int port;

	port = DART_PORT(dev);
	s = spltty();
	addr = (union dartreg *) DART_BASE;
#if 1
	ptaddr = (union dart_pt_io *) addr + (port * 0x20);
#else
	ptaddr = (union dart_pt_io *) addr + ((dev & 1) ? 1 : 0);
#endif
	/* enable receiver */
	ptaddr->write.wr_cr = RXEN;

	for (;;) {
		/* read status reg */
		sr = ptaddr->read.rd_sr;

		/* receiver interrupt handler*/
		if (sr & RXRDY) {
			/* read character from port */
			c = ptaddr->read.rd_rb;

			/* check break condition */
			if (sr & RBRK) {
				/* clear break state */
				ptaddr->write.wr_cr = BRKINTRESET;
				DELAY_CR;
				ptaddr->write.wr_cr = ERRRESET;
				break;
			}

			if (sr & (FRERR | PERR | ROVRN)) {
				/* clear error state */
				ptaddr->write.wr_cr = ERRRESET;
				DELAY_CR;
				ptaddr->write.wr_cr = BRKINTRESET;
			} else {
				break;
			}
		}
	}
	splx(s);
	return (int)c;
}
