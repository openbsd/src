/*	$OpenBSD: qsc.c,v 1.6 2010/04/12 12:57:52 tedu Exp $	*/
/*
 * Copyright (c) 2006 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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
#include <sys/conf.h>

#include <machine/bus.h>
#include <machine/nexus.h>
#include <machine/sid.h>

#include <dev/cons.h>

#include <vax/vxt/vxtbusvar.h>

#include <vax/vxt/qscreg.h>
#include <vax/vxt/qscvar.h>

#ifdef	DDB
#include <machine/cpu.h>
#include <ddb/db_var.h>
#endif

#include "qsckbd.h"
#include "qscms.h"

struct cfdriver qsc_cd = {
	NULL, "qsc", DV_TTY
};

/* console storage */
struct qsc_sv_reg qsccn_sv;

/* prototypes */
cdev_decl(qsc);
cons_decl(qsc);
int	qscintr(void *);
int	qscparam(struct tty *, struct termios *);
void	qscrint(struct qscsoftc *, u_int);
int	qscspeed(int);
void	qscstart(struct tty *);
struct tty *qsctty(dev_t);
void	qscxint(struct qscsoftc *, u_int);

/*
 * Registers are mapped as the least-significant byte of 32-bit
 * addresses. The following macros hide this.
 */

#define	qsc_readp(sc, reg) \
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, 4 * (reg))
#define	qsc_read(sc, line, reg) \
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, \
	    (sc)->sc_regaddr[line][reg])
#define	qsc_writep(sc, reg, val) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, 4 * (reg), (val))
#define	qsc_write(sc, line, reg, val) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, \
	    (sc)->sc_regaddr[line][reg], (val))

#define SC_LINE(dev)	(minor(dev))

/*
 * Attachment glue.
 */

int	qsc_match(struct device *parent, void *self, void *aux);
void	qsc_attach(struct device *parent, struct device *self, void *aux);
int	qsc_print(void *, const char *);

struct cfattach qsc_ca = {
	sizeof(struct qscsoftc), qsc_match, qsc_attach
};

int
qsc_match(struct device *parent, void *cf, void *aux)
{
	struct bp_conf *bp = aux;

	if (strcmp(bp->type, qsc_cd.cd_name) == 0)
		return (1);
	return (0);
}

void
qsc_attach(struct device *parent, struct device *self, void *aux)
{
	extern struct vax_bus_space vax_mem_bus_space;
	struct qscsoftc *sc = (struct qscsoftc *)self;
	bus_space_handle_t ioh;
	u_int line, pair, reg;
#if NQSCKBD > 0 || NQSCMS > 0
	struct qsc_attach_args qa;
#endif

	sc->sc_iot = &vax_mem_bus_space;
	if (bus_space_map(sc->sc_iot, QSCADDR, VAX_NBPG, 0, &ioh) != 0) {
		printf(": can't map registers!\n");
		return;
	}
	sc->sc_ioh = ioh;

	if (cn_tab->cn_putc == qsccnputc) {
		sc->sc_console = 1;
		printf(": console");
	}

	/*
	 * Initialize line-specific data (register addresses)
	 */

	for (line = 0; line < SC_NLINES; line++) {
		sc->sc_regaddr[line][SC_MR] = line * 8 + SC_MRA;
		sc->sc_regaddr[line][SC_CSR] = line * 8 + SC_CSRA;
		sc->sc_regaddr[line][SC_CR] = line * 8 + SC_CRA;
		sc->sc_regaddr[line][SC_TXFIFO] = line * 8 + SC_TXFIFOA;

		sc->sc_regaddr[line][SC_IOPCR] = (line < 2 ? 0 : 0x10) +
		    (line & 1) + SC_IOPCRA;

		sc->sc_regaddr[line][SC_ACR] = (line < 2 ? 0 : 0x10) + SC_ACRAB;
		sc->sc_regaddr[line][SC_IMR] = (line < 2 ? 0 : 0x10) + SC_IMRAB;
		sc->sc_regaddr[line][SC_OPR] = (line < 2 ? 0 : 0x10) + SC_OPRAB;
	}
	for (line = 0; line < SC_NLINES; line++)
		for (reg = 0; reg < SC_LOGICAL; reg++)
			sc->sc_regaddr[line][reg] =
			    0 + 4 * sc->sc_regaddr[line][reg];

	/*
	 * Initialize all lines.
	 */
	sc->sc_sv_reg = sc->sc_console ? &qsccn_sv : &sc->sc_sv_reg_storage;
	for (line = 0; line < SC_NLINES; line++) {
		/* do not reinitialize the console line... */
		if (sc->sc_console && line == QSC_LINE_SERIAL)
			continue;

		sc->sc_sv_reg->sv_mr1[line] =
		    (line == 3 ? ODDPAR | PAREN : PARDIS) | RXRTS | CL8;
		sc->sc_sv_reg->sv_mr2[line] = /* TXCTS | */ SB1;
		sc->sc_sv_reg->sv_csr[line] = line < 2 ? BD9600 : BD4800;
		sc->sc_sv_reg->sv_cr[line]  = TXEN | RXEN;

		pair = line >> 1;

		if (sc->sc_console && pair == (QSC_LINE_SERIAL >> 1))
			continue;

		/* Start out with Tx and RX interrupts disabled */
		sc->sc_sv_reg->sv_imr[pair] = 0;
	}

	for (line = 0; line < SC_NLINES; line++) {
		/* do not reset the console line... */
		if (sc->sc_console && line == QSC_LINE_SERIAL)
			continue;

		qsc_write(sc, line, SC_CR, RXRESET | TXDIS | RXDIS);
		DELAY(1);
		qsc_write(sc, line, SC_CR, TXRESET | TXDIS | RXDIS);
		DELAY(1);
		qsc_write(sc, line, SC_CR, ERRRESET | TXDIS | RXDIS);
		DELAY(1);
		qsc_write(sc, line, SC_CR, BRKINTRESET | TXDIS | RXDIS);
		DELAY(1);
		qsc_write(sc, line, SC_CR, MRZERO | TXDIS | RXDIS);
		DELAY(1);

		qsc_write(sc, line, SC_MR, 0);
		qsc_write(sc, line, SC_MR, sc->sc_sv_reg->sv_mr1[line]);
		qsc_write(sc, line, SC_MR, sc->sc_sv_reg->sv_mr2[line]);
		qsc_write(sc, line, SC_CSR, sc->sc_sv_reg->sv_csr[line]);
		qsc_write(sc, line, SC_CR, sc->sc_sv_reg->sv_cr[line]);
		DELAY(1);
	}

	for (pair = 0; pair < SC_NLINES / 2; pair++)
		qsc_write(sc, pair << 1, SC_IMR,
		    sc->sc_sv_reg->sv_imr[pair]);

	for (line = 0; line < SC_NLINES; line++) {
		sc->sc_tty[line] = NULL;
		sc->sc_swflags[line] = 0;
	}
	if (sc->sc_console)
		sc->sc_swflags[QSC_LINE_SERIAL] |= TIOCFLAG_SOFTCAR;

	printf("\n");

	/*
	 * Configure interrupts. We are bidding in 2681 mode for now.
	 */

	qsc_writep(sc, SC_ICR, 0x00);
	for (line = SC_BIDCRA; line <= SC_BIDCRD; line++)
		qsc_writep(sc, line, 0x00);
	qsc_writep(sc, SC_IVR, VXT_INTRVEC >> 2);

	vxtbus_intr_establish(self->dv_xname, IPL_TTY, qscintr, sc);

	/*
	 * Attach subdevices, and enable RX and TX interrupts on their lines
	 * if successful.
	 */
#if NQSCKBD > 0
	/* keyboard */
	qa.qa_line = QSC_LINE_KEYBOARD;
	qa.qa_console = !sc->sc_console;
	qa.qa_hook = &sc->sc_hook[QSC_LINE_KEYBOARD];
	if (config_found(self, &qa, qsc_print) != NULL)
		sc->sc_sv_reg->sv_imr[QSC_LINE_KEYBOARD >> 1] |= IRXRDYA;
#endif
#if NQSCMS > 0
	/* mouse */
	qa.qa_line = QSC_LINE_MOUSE;
	qa.qa_console = 0;
	qa.qa_hook = &sc->sc_hook[QSC_LINE_MOUSE];
	if (config_found(self, &qa, qsc_print) != NULL)
		sc->sc_sv_reg->sv_imr[QSC_LINE_MOUSE >> 1] |= IRXRDYB;
#endif

	for (pair = 0; pair < SC_NLINES / 2; pair++)
		qsc_write(sc, pair << 1, SC_IMR,
		    sc->sc_sv_reg->sv_imr[pair]);

	sc->sc_rdy = 1;
}

/* speed tables */
const struct qsc_s {
	int kspeed;
	int dspeed;
} qsc_speeds[] = {
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
qscspeed(int speed)
{
	const struct qsc_s *ds;

	for (ds = qsc_speeds; ds->kspeed != -1; ds++)
		if (ds->kspeed == speed)
			return ds->dspeed;

	return NOBAUD;
}

struct tty *
qsctty(dev_t dev)
{
	u_int line;
	struct qscsoftc *sc;

	line = SC_LINE(dev);
	if (qsc_cd.cd_ndevs == 0 || line >= SC_NLINES)
		return (NULL);

	sc = (struct qscsoftc *)qsc_cd.cd_devs[0];
	if (sc == NULL)
		return (NULL);

	return sc->sc_tty[line];
}

void
qscstart(struct tty *tp)
{
	struct qscsoftc *sc;
	dev_t dev;
	int s;
	u_int line;
	int c, tries;

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	dev = tp->t_dev;
	line = SC_LINE(dev);
	sc = (struct qscsoftc *)qsc_cd.cd_devs[0];

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

	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0) {

		/* load transmitter until it is full */
		for (tries = 10000; tries != 0; tries --)
			if (qsc_read(sc, line, SC_SR) & TXRDY)
				break;

		if (tries == 0) {
			timeout_add(&tp->t_rstrt_to, 1);
			tp->t_state |= TS_TIMEOUT;
			break;
		} else {
			c = getc(&tp->t_outq);

			qsc_write(sc, line, SC_TXFIFO, c & 0xff);

			sc->sc_sv_reg->sv_imr[line >> 1] |=
			    line & 1 ? ITXRDYB : ITXRDYA;
			qsc_write(sc, line, SC_IMR,
			    sc->sc_sv_reg->sv_imr[line >> 1]);
		}
	}
	tp->t_state &= ~TS_BUSY;

bail:
	splx(s);
}

int
qscstop(struct tty *tp, int flag)
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

int
qscioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int error;
	u_int line;
	struct tty *tp;
	struct qscsoftc *sc;

	line = SC_LINE(dev);
	sc = (struct qscsoftc *)qsc_cd.cd_devs[0];

	tp = sc->sc_tty[line];
	if (tp == NULL)
		return (ENXIO);

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return(error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return(error);

	switch (cmd) {
	case TIOCGFLAGS:
		*(int *)data = sc->sc_swflags[line];
		break;
	case TIOCSFLAGS:
		error = suser(p, 0);
		if (error != 0)
			return (EPERM);

		sc->sc_swflags[line] = *(int *)data &
		    /* only allow valid flags */
		    (TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

int
qscparam(struct tty *tp, struct termios *t)
{
	int flags;
	u_int line, pair;
	int speeds;
	u_int8_t mr1, mr2;
	struct qscsoftc *sc;
	dev_t dev;

	dev = tp->t_dev;
	line = SC_LINE(dev);
	pair = line >> 1;
	sc = (struct qscsoftc *)qsc_cd.cd_devs[0];

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	flags = tp->t_flags;

	/* disable Tx and Rx */
	if (sc->sc_console == 0 || line != QSC_LINE_SERIAL) {
		if (line & 1)
			sc->sc_sv_reg->sv_imr[pair] &= ~(ITXRDYB | IRXRDYB);
		else
			sc->sc_sv_reg->sv_imr[pair] &= ~(ITXRDYA | IRXRDYA);
		qsc_write(sc, line, SC_IMR, sc->sc_sv_reg->sv_imr[pair]);

		/* set baudrate */
		speeds = qscspeed(tp->t_ispeed);
		if (speeds == NOBAUD)
			speeds = DEFBAUD;
		qsc_write(sc, line, SC_CSR, speeds);
		sc->sc_sv_reg->sv_csr[line] = speeds;

		/* get saved mode registers and clear set up parameters */
		mr1 = sc->sc_sv_reg->sv_mr1[line];
		mr1 &= ~(CLMASK | PARTYPEMASK | PARMODEMASK);

		mr2 = sc->sc_sv_reg->sv_mr2[line];
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

		if (sc->sc_sv_reg->sv_mr1[line] != mr1 ||
		    sc->sc_sv_reg->sv_mr2[line] != mr2) {
			/* write mode registers to duart */
			qsc_write(sc, line, SC_CR, MRONE);
			DELAY(1);
			qsc_write(sc, line, SC_MR, mr1);
			qsc_write(sc, line, SC_MR, mr2);

			/* save changed mode registers */
			sc->sc_sv_reg->sv_mr1[line] = mr1;
			sc->sc_sv_reg->sv_mr2[line] = mr2;
		}
	}

	/* enable transmitter? */
	if (tp->t_state & TS_BUSY) {
		sc->sc_sv_reg->sv_imr[pair] |= line & 1 ? ITXRDYB : ITXRDYA;
	}

	/* re-enable the receiver */
	sc->sc_sv_reg->sv_imr[pair] |= line & 1 ? IRXRDYB : IRXRDYA;
	qsc_write(sc, line, SC_IMR, sc->sc_sv_reg->sv_imr[pair]);

	return (0);
}

int
qscopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int s;
	u_int line;
	struct qscsoftc *sc;
	struct tty *tp;

	line = SC_LINE(dev);
	if (qsc_cd.cd_ndevs == 0 || line >= SC_NLINES)
		return (ENXIO);
	/* Line B is not wired... */
	if (line == QSC_LINE_DEAD)
		return (ENXIO);
	sc = (struct qscsoftc *)qsc_cd.cd_devs[0];
	if (sc == NULL)
		return (ENXIO);

	/* if some other device is using the line, it's not available */
	if (sc->sc_hook[line].fn != NULL)
		return (ENXIO);

	s = spltty();
	if (sc->sc_tty[line] != NULL)
		tp = sc->sc_tty[line];
	else
		tp = sc->sc_tty[line] = ttymalloc();

	tp->t_oproc = qscstart;
	tp->t_param = qscparam;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);

		if (tp->t_ispeed == 0) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = B9600;
			if (sc->sc_console && line == QSC_LINE_SERIAL) {
				/* console is 8N1 */
				tp->t_cflag = (CREAD | CS8 | HUPCL);
			} else {
				tp->t_cflag = TTYDEF_CFLAG;
			}
		}

		if (sc->sc_swflags[line] & TIOCFLAG_CLOCAL)
			tp->t_cflag |= CLOCAL;
		if (sc->sc_swflags[line] & TIOCFLAG_CRTSCTS)
			tp->t_cflag |= CRTSCTS;
		if (sc->sc_swflags[line] & TIOCFLAG_MDMBUF)
			tp->t_cflag |= MDMBUF;

		qscparam(tp, &tp->t_termios);
		ttsetwater(tp);

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
qscclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp;
	struct qscsoftc *sc;
	u_int line;

	line = SC_LINE(dev);
	sc = (struct qscsoftc *)qsc_cd.cd_devs[0];

	tp = sc->sc_tty[line];
	(*linesw[tp->t_line].l_close)(tp, flag, p);
	ttyclose(tp);

	return (0);
}

int
qscread(dev_t dev, struct uio *uio, int flag)
{
	u_int line;
	struct tty *tp;
	struct qscsoftc *sc;

	line = SC_LINE(dev);
	sc = (struct qscsoftc *)qsc_cd.cd_devs[0];

	tp = sc->sc_tty[line];
	if (tp == NULL)
		return (ENXIO);
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
qscwrite(dev_t dev, struct uio *uio, int flag)
{
	u_int line;
	struct tty *tp;
	struct qscsoftc *sc;

	line = SC_LINE(dev);
	sc = (struct qscsoftc *)qsc_cd.cd_devs[0];

	tp = sc->sc_tty[line];
	if (tp == NULL)
		return (ENXIO);
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

void
qscrint(struct qscsoftc *sc, u_int line)
{
	struct tty *tp;
	int data;
	unsigned char sr;
	int overrun = 0;

	tp = sc->sc_tty[line];

	/* read status reg */
	while ((sr = qsc_read(sc, line, SC_SR)) & RXRDY) {
		/* read data and reset receiver */
		data = qsc_read(sc, line, SC_RXFIFO);

		if (sr & RBRK) {
			/* clear break state */
			qsc_write(sc, line, SC_CR, BRKINTRESET);
			DELAY(1);
			qsc_write(sc, line, SC_CR, ERRRESET);
			DELAY(1);
			continue;
		}

		if ((sr & ROVRN) && cold == 0 && overrun == 0) {
			log(LOG_WARNING, "%s line %d: receiver overrun\n",
			    sc->sc_dev.dv_xname, line);
			overrun = 1;
		}

		if (sr & FRERR)
			data |= TTY_FE;
		if (sr & PERR)
			data |= TTY_PE;

		/* clear error state */
		if (sr & (ROVRN | FRERR | PERR)) {
			qsc_write(sc, line, SC_CR, ERRRESET);
			DELAY(1);
		}

		if (sc->sc_hook[line].fn != NULL) {
			if ((data & TTY_ERRORMASK) != 0 ||
			   (*sc->sc_hook[line].fn)(sc->sc_hook[line].arg, data))
			continue;
		}

		if ((tp->t_state & (TS_ISOPEN|TS_WOPEN)) == 0 &&
		    (sc->sc_console == 0 || line != QSC_LINE_SERIAL)) {
			continue;
		}

		/* no errors */
#if defined(DDB)
		if (tp->t_dev == cn_tab->cn_dev) {
			int j = kdbrint(data);

			if (j == 1)
				continue;

			if (j == 2)
				(*linesw[tp->t_line].l_rint)(27, tp);
		}
#endif
		(*linesw[tp->t_line].l_rint)(data, tp);
	}
}

void
qscxint(struct qscsoftc *sc, u_int line)
{
	struct tty *tp;
	u_int pair;

	tp = sc->sc_tty[line];

	if ((tp->t_state & (TS_ISOPEN|TS_WOPEN))==0)
		goto out;

	if (tp->t_state & TS_BUSY) {
		tp->t_state &= ~(TS_BUSY | TS_FLUSH);
		qscstart(tp);
		if (tp->t_state & TS_BUSY) {
			/* do not disable transmitter, yet */
			return;
		}
	}
out:

	/* disable transmitter */
	pair = line >> 1;
	sc->sc_sv_reg->sv_imr[pair] &= line & 1 ? ~ITXRDYB : ~ITXRDYA;
	qsc_write(sc, line, SC_IMR, sc->sc_sv_reg->sv_imr[pair]);
}

int
qscintr(void *arg)
{
	struct qscsoftc *sc = arg;
	u_int8_t isr[SC_NLINES >> 1], curisr;
	u_int pair, line;
	int rc = 0;

	for (pair = 0; pair < SC_NLINES >> 1; pair++) {
		line = pair << 1;

		/* read interrupt status register and mask with imr */
		isr[pair] = curisr = qsc_read(sc, line, SC_ISR);
		curisr &= sc->sc_sv_reg->sv_imr[pair];
		if (curisr == 0)
			continue;

		rc = 1;

		if (curisr & IRXRDYA)
			qscrint(sc, line);
		if (curisr & ITXRDYA)
			qscxint(sc, line);
		if (curisr & IBRKA) {
			qsc_write(sc, line, SC_CR, BRKINTRESET);
			DELAY(1);
		}

		if (curisr & IRXRDYB)
			qscrint(sc, line + 1);
		if (curisr & ITXRDYB)
			qscxint(sc, line + 1);
		if (curisr & IBRKB) {
			qsc_write(sc, line + 1, SC_CR, BRKINTRESET);
			DELAY(1);
		}
	}

	return (rc);
}

/*
 * Console interface routines.
 */

vaddr_t qsc_cnregs;
#define	qsc_cnread(reg) \
	*(volatile u_int8_t *)(qsc_cnregs + 4 * (reg))
#define	qsc_cnwrite(reg, val) \
	*(volatile u_int8_t *)(qsc_cnregs + 4 * (reg)) = (val)

void
qsccnprobe(struct consdev *cp)
{
	int maj;
	extern int getmajor(void *);
	extern vaddr_t iospace;

	if (vax_boardtype != VAX_BTYP_VXT)
		return;

	/* locate the major number */
	if ((maj = getmajor(qscopen)) < 0)
		return;

	qsc_cnregs = iospace;
	ioaccess(iospace, QSCADDR, 1);

	cp->cn_dev = makedev(maj, QSC_LINE_SERIAL);
	cp->cn_pri = vax_confdata & 2 ? CN_LOWPRI : CN_HIGHPRI;
}

void
qsccninit(cp)
	struct consdev *cp;
{
	qsccn_sv.sv_mr1[QSC_LINE_SERIAL] = PARDIS | RXRTS | CL8;
	qsccn_sv.sv_mr2[QSC_LINE_SERIAL] = /* TXCTS | */ SB1;
	qsccn_sv.sv_csr[QSC_LINE_SERIAL] = BD9600;
	qsccn_sv.sv_cr[QSC_LINE_SERIAL]  = TXEN | RXEN;
	qsccn_sv.sv_imr[QSC_LINE_SERIAL] = 0;

	qsc_cnwrite(SC_CRA, RXRESET | TXDIS | RXDIS);
	DELAY(1);
	qsc_cnwrite(SC_CRA, TXRESET | TXDIS | RXDIS);
	DELAY(1);
	qsc_cnwrite(SC_CRA, ERRRESET | TXDIS | RXDIS);
	DELAY(1);
	qsc_cnwrite(SC_CRA, BRKINTRESET | TXDIS | RXDIS);
	DELAY(1);
	qsc_cnwrite(SC_CRA, MRZERO | TXDIS | RXDIS);
	DELAY(1);

	qsc_cnwrite(SC_MRA, 0);
	qsc_cnwrite(SC_MRA, qsccn_sv.sv_mr1[QSC_LINE_SERIAL]);
	qsc_cnwrite(SC_MRA, qsccn_sv.sv_mr2[QSC_LINE_SERIAL]);
	qsc_cnwrite(SC_CSRA, qsccn_sv.sv_csr[QSC_LINE_SERIAL]);
	qsc_cnwrite(SC_CRA, qsccn_sv.sv_cr[QSC_LINE_SERIAL]);
	DELAY(1);

	qsc_cnwrite(SC_IMRAB, qsccn_sv.sv_imr[QSC_LINE_SERIAL]);
	qsc_cnwrite(SC_IMRCD, 0);
}

int
qsccngetc(dev_t dev)
{
	unsigned char sr;	/* status reg of line a/b */
	u_char c;		/* received character */
	int s;

	s = spltty();

	/* disable interrupts for this line and enable receiver */
	qsc_cnwrite(SC_IMRAB, qsccn_sv.sv_imr[QSC_LINE_SERIAL] & ~ITXRDYA);
	qsc_cnwrite(SC_CRA, RXEN);
	DELAY(1);

	for (;;) {
		/* read status reg */
		sr = qsc_cnread(SC_SRA);

		/* receiver interrupt handler*/
		if (sr & RXRDY) {
			/* read character from line */
			c = qsc_cnread(SC_RXFIFOA);

			/* check break condition */
			if (sr & RBRK) {
				/* clear break state */
				qsc_cnwrite(SC_CRA, BRKINTRESET);
				DELAY(1);
				qsc_cnwrite(SC_CRA, ERRRESET);
				DELAY(1);
				break;
			}

			if (sr & (FRERR | PERR | ROVRN)) {
				/* clear error state */
				qsc_cnwrite(SC_CRA, ERRRESET);
				DELAY(1);
			} else {
				break;
			}
		}
	}

	/* restore the previous state */
	qsc_cnwrite(SC_IMRAB, qsccn_sv.sv_imr[QSC_LINE_SERIAL]);
	qsc_cnwrite(SC_CRA, qsccn_sv.sv_cr[QSC_LINE_SERIAL]);

	splx(s);

	return ((int)c);
}

void
qsccnputc(dev_t dev, int c)
{
	int s;

	if (mfpr(PR_MAPEN) == 0)
		return;

	s = spltty();

	/* disable interrupts for this line and enable transmitter */
	qsc_cnwrite(SC_IMRAB, qsccn_sv.sv_imr[QSC_LINE_SERIAL] & ~ITXRDYA);
	qsc_cnwrite(SC_CRA, TXEN);
	DELAY(1);

	while ((qsc_cnread(SC_SRA) & TXRDY) == 0)
		;
	qsc_cnwrite(SC_TXFIFOA, c);

	/* wait for transmitter to empty */
	while ((qsc_cnread(SC_SRA) & TXEMT) == 0)
		;

	/* restore the previous state */
	qsc_cnwrite(SC_IMRAB, qsccn_sv.sv_imr[QSC_LINE_SERIAL]);
	qsc_cnwrite(SC_CRA, qsccn_sv.sv_cr[QSC_LINE_SERIAL]);
	DELAY(1);

	splx(s);
}

void 
qsccnpollc(dev, pollflag)
	dev_t dev;
	int pollflag;
{
}

/*
 * Keyboard and mouse helper routines
 */

#if NQSCKBD > 0 || NQSCMS > 0
int
qsc_print(void *aux, const char *name)
{
	struct qsc_attach_args *qa = aux;

	if (name != NULL)
		printf(qa->qa_line == QSC_LINE_KEYBOARD ?
		    "lkkbd at %s" : "lkms at %s", name);
	else
		printf(" line %d", qa->qa_line);

	return (UNCONF);
}

int
qscgetc(u_int line)
{
	bus_addr_t craddr;
	struct qscsoftc *sc = NULL;
	int s;
	u_int8_t sr, imr, imrmask, cr, c;

	s = spltty();

	craddr = line == QSC_LINE_KEYBOARD ? SC_CRC : SC_CRD;
	imrmask = line & 1 ? ~IRXRDYB : ~IRXRDYA;
	imr = sc != NULL ? sc->sc_sv_reg->sv_imr[line / 2] : 0;
	cr = sc != NULL ? sc->sc_sv_reg->sv_cr[line] : 0;

	/* disable interrupts for this line and enable receiver */
	qsc_cnwrite(SC_IMRCD, imr & imrmask);
	qsc_cnwrite(craddr, RXEN);
	DELAY(1);

	for (;;) {
		/* read status reg */
		sr = qsc_cnread(line == QSC_LINE_KEYBOARD ? SC_SRC : SC_SRD);

		/* receiver interrupt handler*/
		if (sr & RXRDY) {
			/* read character from line */
			c = qsc_cnread(line == QSC_LINE_KEYBOARD ?
			    SC_RXFIFOC : SC_RXFIFOD);

			/* check break condition */
			if (sr & RBRK) {
				/* clear break state */
				qsc_cnwrite(craddr, BRKINTRESET);
				DELAY(1);
				qsc_cnwrite(craddr, ERRRESET);
				DELAY(1);
				break;
			}

			if (sr & (FRERR | PERR | ROVRN)) {
				/* clear error state */
				qsc_cnwrite(craddr, ERRRESET);
				DELAY(1);
			} else {
				break;
			}
		}
	}

	/* restore the previous state */
	qsc_cnwrite(SC_IMRCD, imr);
	qsc_cnwrite(craddr, cr);
	DELAY(1);

	splx(s);

	return ((int)c);
}

void
qscputc(u_int line, int c)
{
	bus_addr_t craddr;
	struct qscsoftc *sc = NULL;
	int s;
	u_int8_t imr, imrmask, cr;

	s = spltty();

	if (qsc_cd.cd_ndevs != 0 &&
	    (sc = (struct qscsoftc *)qsc_cd.cd_devs[0]) != NULL)
		if (sc->sc_rdy == 0)
			sc = NULL;

	craddr = line == QSC_LINE_KEYBOARD ? SC_CRC : SC_CRD;
	imrmask = line & 1 ? ~ITXRDYB : ~ITXRDYA;
	imr = sc != NULL ? sc->sc_sv_reg->sv_imr[line / 2] : 0;
	cr = sc != NULL ? sc->sc_sv_reg->sv_cr[line] : 0;

	/* disable interrupts for this line and enable transmitter */
	qsc_cnwrite(SC_IMRCD, imr & imrmask);
	qsc_cnwrite(craddr, TXEN);
	DELAY(1);

	while ((qsc_cnread(line == QSC_LINE_KEYBOARD ? SC_SRC : SC_SRD) &
	    TXRDY) == 0)
		;
	qsc_cnwrite(line == QSC_LINE_KEYBOARD ? SC_TXFIFOC : SC_TXFIFOD, c);

	/* wait for transmitter to empty */
	while ((qsc_cnread(line == QSC_LINE_KEYBOARD ? SC_SRC : SC_SRD) &
	    TXEMT) == 0)
		;

	/* restore the previous state */
	qsc_cnwrite(SC_IMRCD, imr);
	qsc_cnwrite(craddr, cr);
	DELAY(1);

	splx(s);
}
#endif
